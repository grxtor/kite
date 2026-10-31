/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "RuleEngine.h"

namespace
{
	Severity severityFromString(const QString& s)
	{
		QString l = s.toLower();
		if (l == QStringLiteral("high") || l == QStringLiteral("critical"))
			return Severity::High;
		if (l == QStringLiteral("low") || l == QStringLiteral("info"))
			return Severity::Low;
		return Severity::Medium;
	}

	int severityRank(Severity s)
	{
		switch (s) {
			case Severity::High:
				return 2;
			case Severity::Medium:
				return 1;
			case Severity::Low:
			default:
				return 0;
		}
	}
} // namespace

bool RuleEngine::loadDirectory(const QString& dir, QString* errorMsg)
{
	QDir d(dir);
	if (!d.exists()) {
		if (errorMsg)
			*errorMsg = QStringLiteral("Rule directory missing: %1").arg(dir);
		return false;
	}
	const auto files = d.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
	int loaded = 0;
	for (const auto& fi : files) {
		QFile f(fi.absoluteFilePath());
		if (!f.open(QIODevice::ReadOnly))
			continue;
		QString err;
		if (loadFromBytes(f.readAll(), fi.completeBaseName(), &err))
			++loaded;
		else if (errorMsg)
			*errorMsg = err;
	}
	return loaded > 0;
}

bool RuleEngine::loadFromBytes(const QByteArray& bytes, const QString& packName,
							   QString* errorMsg)
{
	QJsonParseError je;
	auto doc = QJsonDocument::fromJson(bytes, &je);
	if (je.error != QJsonParseError::NoError) {
		if (errorMsg)
			*errorMsg = je.errorString();
		return false;
	}
	if (!doc.isObject()) {
		if (errorMsg)
			*errorMsg = "rule pack root is not an object";
		return false;
	}
	auto root = doc.object();
	if (root.value("schema").toInt(1) != 1) {
		if (errorMsg)
			*errorMsg = "unsupported schema version";
		return false;
	}

	auto rulesArr = root.value("rules").toArray();
	for (auto v : rulesArr) {
		auto obj = v.toObject();
		Rule r;
		r.packName = packName;
		r.id = obj.value("id").toString();
		r.title = obj.value("title").toString();
		r.severity = severityFromString(obj.value("severity").toString());
		r.advice = obj.value("advice").toString();
		r.actions = obj.value("actions").toArray();
		for (auto t : obj.value("tags").toArray())
			r.tags.append(t.toString());
		for (auto p : obj.value("patterns").toArray()) {
			r.patternStrings.append(p.toString());
			QRegularExpression re(p.toString(),
								  QRegularExpression::CaseInsensitiveOption);
			if (re.isValid())
				r.patterns.append(re);
		}
		if (r.id.isEmpty() || r.patterns.isEmpty())
			continue;
		m_rules.append(r);
	}
	return true;
}

QList<Match> RuleEngine::analyse(const QString& text) const
{
	QList<Match> out;
	const auto lines = text.split('\n');
	for (const auto& r : m_rules) {
		// Try whole-text match first (fast path)
		bool matched = false;
		QString sample;
		int lineNo = -1;
		for (const auto& re : r.patterns) {
			auto m = re.match(text);
			if (!m.hasMatch())
				continue;
			matched = true;
			// Find the line containing the match
			int pos = m.capturedStart();
			int lineStart = text.lastIndexOf('\n', qMax(0, pos - 1)) + 1;
			int lineEnd = text.indexOf('\n', pos);
			if (lineEnd < 0)
				lineEnd = text.size();
			sample = text.mid(lineStart, lineEnd - lineStart).trimmed();
			lineNo = int(text.left(pos).count('\n')) + 1;
			Q_UNUSED(lines);
			break;
		}
		if (!matched)
			continue;
		Match mt;
		mt.ruleId = r.id;
		mt.ruleTitle = r.title;
		mt.severity = r.severity;
		mt.advice = r.advice;
		mt.matchedLine = sample;
		mt.line = lineNo;
		out.append(mt);
	}

	// Sort by severity desc, then score desc (score is populated later from LearningStore).
	std::sort(out.begin(), out.end(), [](const Match& a, const Match& b) {
		int ra = severityRank(a.severity);
		int rb = severityRank(b.severity);
		if (ra != rb)
			return ra > rb;
		return a.score > b.score;
	});
	return out;
}

int RuleEngine::findRuleIndex(const QString& id) const
{
	for (int i = 0; i < m_rules.size(); ++i)
		if (m_rules[i].id == id)
			return i;
	return -1;
}
