/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "LearningStore.h"

#include <QCryptographicHash>
#include <QSaveFile>

uint qHash(const LearningStore::Key& k, uint seed) noexcept
{
	return qHash(k.ruleId, seed) ^ (qHash(k.instanceId, seed) << 1);
}

bool LearningStore::open(const QString& filePath)
{
	m_path = filePath;
	QFile f(filePath);
	if (!f.exists())
		return true; // empty store is fine
	if (!f.open(QIODevice::ReadOnly))
		return false;
	auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject())
		return false;
	auto root = doc.object();

	auto outArr = root.value("outcomes").toArray();
	for (auto v : outArr) {
		auto obj = v.toObject();
		Key k;
		k.ruleId = obj.value("rule").toString();
		k.instanceId = obj.value("instance").toString();
		OutcomeStats s;
		s.helped = obj.value("helped").toInt();
		s.didNotHelp = obj.value("didNotHelp").toInt();
		s.seen = obj.value("seen").toInt();
		s.lastSeenSecs = qint64(obj.value("lastSeen").toDouble());
		m_outcomes.insert(k, s);
	}

	auto novArr = root.value("novel").toArray();
	for (auto v : novArr) {
		auto obj = v.toObject();
		NovelFingerprint nf;
		nf.signature = obj.value("signature").toString();
		nf.sampleLine = obj.value("sample").toString();
		nf.sampleInstance = obj.value("instance").toString();
		nf.occurrences = obj.value("count").toInt();
		nf.firstSeenSecs = qint64(obj.value("firstSeen").toDouble());
		nf.lastSeenSecs = qint64(obj.value("lastSeen").toDouble());
		m_novel.insert(nf.signature, nf);
	}
	return true;
}

bool LearningStore::save()
{
	if (m_path.isEmpty())
		return false;

	QJsonObject root;
	QJsonArray outArr;
	for (auto it = m_outcomes.constBegin(); it != m_outcomes.constEnd(); ++it) {
		QJsonObject obj;
		obj["rule"] = it.key().ruleId;
		obj["instance"] = it.key().instanceId;
		obj["helped"] = it.value().helped;
		obj["didNotHelp"] = it.value().didNotHelp;
		obj["seen"] = it.value().seen;
		obj["lastSeen"] = double(it.value().lastSeenSecs);
		outArr.append(obj);
	}
	root["outcomes"] = outArr;

	QJsonArray novArr;
	for (auto it = m_novel.constBegin(); it != m_novel.constEnd(); ++it) {
		QJsonObject obj;
		obj["signature"] = it.value().signature;
		obj["sample"] = it.value().sampleLine;
		obj["instance"] = it.value().sampleInstance;
		obj["count"] = it.value().occurrences;
		obj["firstSeen"] = double(it.value().firstSeenSecs);
		obj["lastSeen"] = double(it.value().lastSeenSecs);
		novArr.append(obj);
	}
	root["novel"] = novArr;

	QSaveFile f(m_path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	return f.commit();
}

double LearningStore::scoreFor(const QString& ruleId,
							   const QString& instanceId) const
{
	Key k{ruleId, instanceId};
	auto it = m_outcomes.constFind(k);
	if (it == m_outcomes.constEnd())
		return 0.0;
	double total = it->helped + it->didNotHelp + 0.001;
	// Wilson-ish bounded confidence: helped fraction times the log-scaled observation count; caps
	// near +1.
	double frac = double(it->helped) / total;
	double obs = std::log10(double(it->seen + 1) + 1.0);
	return (frac * 2.0 - 1.0) * std::min(obs, 1.5);
}

void LearningStore::recordSeen(const QString& ruleId, const QString& instanceId)
{
	Key k{ruleId, instanceId};
	auto& s = m_outcomes[k];
	++s.seen;
	s.lastSeenSecs = QDateTime::currentSecsSinceEpoch();
}

void LearningStore::recordHelped(const QString& ruleId,
								 const QString& instanceId)
{
	Key k{ruleId, instanceId};
	auto& s = m_outcomes[k];
	++s.helped;
	s.lastSeenSecs = QDateTime::currentSecsSinceEpoch();
}

void LearningStore::recordDidNotHelp(const QString& ruleId,
									 const QString& instanceId)
{
	Key k{ruleId, instanceId};
	auto& s = m_outcomes[k];
	++s.didNotHelp;
	s.lastSeenSecs = QDateTime::currentSecsSinceEpoch();
}

QString LearningStore::fingerprint(const QString& logText)
{
	// Pull the first up-to-5 top-of-stack frames out of the log. We
	// normalise:
	//   • drop the line number     (":at Foo.bar(Foo.java:123)")
	//   • drop trailing $hash       ("$1abc")
	//   • lower-case the class name
	QRegularExpression re(QStringLiteral(
		R"(at\s+([A-Za-z_][A-Za-z0-9_$.]+)\.([A-Za-z_<][A-Za-z0-9_>$]*)\()"));
	auto it = re.globalMatch(logText);
	QStringList frames;
	while (it.hasNext() && frames.size() < 5) {
		auto m = it.next();
		QString cls = m.captured(1);
		QString method = m.captured(2);
		cls.remove(QRegularExpression(QStringLiteral("\\$[0-9a-fA-F]+$")));
		frames << (cls.toLower() + "::" + method);
	}
	if (frames.isEmpty())
		return {};

	QByteArray src = frames.join('\n').toUtf8();
	return QString::fromLatin1(
		QCryptographicHash::hash(src, QCryptographicHash::Sha1)
			.toHex()
			.left(16));
}

void LearningStore::recordNovel(const QString& signature,
								const QString& sampleLine,
								const QString& instanceId)
{
	if (signature.isEmpty())
		return;
	auto& n = m_novel[signature];
	if (n.signature.isEmpty()) {
		n.signature = signature;
		n.firstSeenSecs = QDateTime::currentSecsSinceEpoch();
		n.sampleLine = sampleLine;
		n.sampleInstance = instanceId;
	}
	++n.occurrences;
	n.lastSeenSecs = QDateTime::currentSecsSinceEpoch();
}

void LearningStore::forgetNovel(const QString& signature)
{
	m_novel.remove(signature);
}
