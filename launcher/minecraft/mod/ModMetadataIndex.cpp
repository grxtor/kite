/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ModMetadataIndex.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>

#include "FileSystem.h"
#include "PackwizSidecar.h"

ModMetadataIndex::ModMetadataIndex(const QDir& folder)
	: m_folder(folder), m_indexDir(folder.absoluteFilePath(".index"))
{
}

QString ModMetadataIndex::canonicalFileName(const QString& fileName)
{
	if (fileName.endsWith(QStringLiteral(".disabled"))) {
		return fileName.left(fileName.size() - 9);
	}
	return fileName;
}

QString ModMetadataIndex::normalizeName(const QString& name)
{
	QString n = name.toLower().trimmed();

	/* Only a loader marker comes off, not every bracket.
	 *
	 * This used to drop anything in parentheses, which made "Fabric API"
	 * and "Fabric API (Forge)" - two genuinely different projects, one
	 * per loader - into the same name. Everything that matches by name
	 * then treated one as the other: an installed Forge port counted as
	 * the Fabric mod being present, and the download queue refused the
	 * second of the pair because it believed it already had it.
	 *
	 * Anything else between brackets stays, because it is usually part
	 * of what the mod is actually called. */
	static const QRegularExpression loaderMarker(QStringLiteral(
		"\\s*\\((?:neo\\s*forge|forge|fabric|quilt|liteloader|babric|bta"
		"|legacy\\s*fabric|ornithe|rift)"
		"(?:\\s*[/+,&]\\s*(?:neo\\s*forge|forge|fabric|quilt|liteloader"
		"|babric|bta|legacy\\s*fabric|ornithe|rift))*\\)\\s*"));
	n.remove(loaderMarker);

	n.remove(QRegularExpression(QStringLiteral("[^a-z0-9 ]")));
	n = n.simplified();
	return n;
}

QString ModMetadataIndex::sidecarPath(const QString& sidecarName) const
{
	return m_indexDir.absoluteFilePath(sidecarName);
}

bool ModMetadataIndex::dropSidecarOwnedBy(const QString& sidecarName,
										  const QString& canonicalName) const
{
	if (sidecarName.isEmpty()) {
		return false;
	}
	const QString path = sidecarPath(sidecarName);
	QFile f(path);
	if (!f.exists()) {
		return false;
	}
	if (!f.open(QIODevice::ReadOnly)) {
		qWarning() << "ModMetadataIndex: cannot read sidecar before "
					  "removing it"
				   << path;
		return false;
	}
	const QByteArray bytes = f.readAll();
	f.close();

	Entry owner;
	if (Packwiz::isSidecarFileName(sidecarName)) {
		owner = Packwiz::parse(bytes, Packwiz::slugFromFileName(sidecarName));
	} else {
		owner = parseJson(bytes);
	}

	if (canonicalFileName(owner.fileName) != canonicalName) {
		/* Belongs to another file of the same project - leave it. */
		return false;
	}
	if (!QFile::remove(path)) {
		qWarning() << "ModMetadataIndex: failed to delete sidecar" << path;
		return false;
	}
	return true;
}

ModMetadataIndex::Entry ModMetadataIndex::parseJson(const QByteArray& bytes)
{
	Entry e;
	QJsonParseError perr{};
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
	if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
		return e;
	}
	const QJsonObject o = doc.object();
	e.fileName = o.value(QStringLiteral("fileName")).toString();
	e.platform = o.value(QStringLiteral("platform")).toString();
	e.projectId = o.value(QStringLiteral("projectId")).toString();
	e.versionId = o.value(QStringLiteral("versionId")).toString();
	e.versionNumber = o.value(QStringLiteral("versionNumber")).toString();
	e.name = o.value(QStringLiteral("name")).toString();
	e.slug = o.value(QStringLiteral("slug")).toString();
	e.downloadUrl = o.value(QStringLiteral("downloadUrl")).toString();
	e.sha1 = o.value(QStringLiteral("sha1")).toString().toLower();
	e.fileSize =
		static_cast<qint64>(o.value(QStringLiteral("fileSize")).toDouble(0));
	e.isDependency = o.value(QStringLiteral("isDependency")).toBool(false);
	const QString iso = o.value(QStringLiteral("installedAt")).toString();
	if (!iso.isEmpty()) {
		e.installedAt = QDateTime::fromString(iso, Qt::ISODate);
	}
	return e;
}

void ModMetadataIndex::load()
{
	QMutexLocker lock(&m_mutex);
	m_entries.clear();
	m_sidecars.clear();

	if (!m_indexDir.exists()) {
		/* Nothing to load yet — that is fine for a fresh folder. */
		return;
	}

	const QStringList files =
		m_indexDir.entryList(QDir::Files | QDir::Readable);

	/* Two passes, legacy format first, so that a folder carrying both
	 * kinds of sidecar for one file ends up trusting the packwiz one -
	 * that is the newer of the two by construction, since writing an
	 * entry replaces its JSON sidecar with a TOML one. */
	for (const QString& name : files) {
		if (!name.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
			continue;
		}
		const QString abs = m_indexDir.absoluteFilePath(name);
		QFile f(abs);
		if (!f.open(QIODevice::ReadOnly)) {
			qWarning() << "ModMetadataIndex: cannot read sidecar" << abs;
			continue;
		}
		const QByteArray bytes = f.readAll();
		f.close();

		const Entry e = parseJson(bytes);
		if (!e.isValid()) {
			qWarning() << "ModMetadataIndex: discarding malformed sidecar"
					   << abs;
			QFile::remove(abs);
			continue;
		}

		/* Prune sidecars whose target file has been removed externally. */
		const QString canon = canonicalFileName(e.fileName);
		const bool exists =
			m_folder.exists(canon) ||
			m_folder.exists(canon + QStringLiteral(".disabled"));
		if (!exists) {
			qDebug() << "ModMetadataIndex: pruning orphaned sidecar" << abs;
			QFile::remove(abs);
			continue;
		}

		m_entries.insert(canon, e);
		m_sidecars.insert(canon, name);
	}

	for (const QString& name : files) {
		if (!Packwiz::isSidecarFileName(name)) {
			continue;
		}
		const QString abs = m_indexDir.absoluteFilePath(name);
		QFile f(abs);
		if (!f.open(QIODevice::ReadOnly)) {
			qWarning() << "ModMetadataIndex: cannot read sidecar" << abs;
			continue;
		}
		const QByteArray bytes = f.readAll();
		f.close();

		const Entry e =
			Packwiz::parse(bytes, Packwiz::slugFromFileName(name));
		if (!e.isValid()) {
			/* Not deleted, unlike the legacy format above: this file may
			 * well have been written by another tool, and throwing away
			 * something we merely failed to understand is worse than
			 * ignoring it. */
			qWarning() << "ModMetadataIndex: ignoring unreadable sidecar"
					   << abs;
			continue;
		}

		const QString canon = canonicalFileName(e.fileName);
		const bool exists =
			m_folder.exists(canon) ||
			m_folder.exists(canon + QStringLiteral(".disabled"));
		if (!exists) {
			/* Also left in place: in a packwiz pack the sidecar is the
			 * definition of what belongs here and the file next to it may
			 * simply not have been downloaded yet. Pruning it would throw
			 * the pack away. */
			qDebug() << "ModMetadataIndex: sidecar without a file, skipping"
					 << abs;
			continue;
		}

		m_entries.insert(canon, e);
		m_sidecars.insert(canon, name);
	}
}

ModMetadataIndex::Entry ModMetadataIndex::get(const QString& fileName) const
{
	QMutexLocker lock(&m_mutex);
	return m_entries.value(canonicalFileName(fileName));
}

bool ModMetadataIndex::contains(const QString& fileName) const
{
	QMutexLocker lock(&m_mutex);
	return m_entries.contains(canonicalFileName(fileName));
}

QList<ModMetadataIndex::Entry> ModMetadataIndex::all() const
{
	QMutexLocker lock(&m_mutex);
	return m_entries.values();
}

QStringList ModMetadataIndex::projectIds(const QString& platform) const
{
	QStringList projects;
	for (const Entry& entry : all()) {
		if (entry.platform == platform && !entry.projectId.isEmpty() &&
			!projects.contains(entry.projectId)) {
			projects.append(entry.projectId);
		}
	}
	return projects;
}

ModMetadataIndex::Entry
ModMetadataIndex::findByPlatformProject(const QString& platform,
										const QString& projectId) const
{
	if (platform.isEmpty() || projectId.isEmpty()) {
		return {};
	}
	QMutexLocker lock(&m_mutex);
	for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
		const Entry& e = it.value();
		if (e.platform.compare(platform, Qt::CaseInsensitive) == 0 &&
			e.projectId == projectId) {
			return e;
		}
	}
	return {};
}

ModMetadataIndex::Entry
ModMetadataIndex::findByNormalizedName(const QString& normalizedName) const
{
	if (normalizedName.isEmpty()) {
		return {};
	}
	QMutexLocker lock(&m_mutex);
	for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
		const Entry& e = it.value();
		if (normalizeName(e.name) == normalizedName) {
			return e;
		}
	}
	return {};
}

ModMetadataIndex::Entry ModMetadataIndex::findBySha1(const QString& sha1) const
{
	if (sha1.isEmpty()) {
		return {};
	}
	const QString needle = sha1.toLower();
	QMutexLocker lock(&m_mutex);
	for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
		const Entry& e = it.value();
		if (e.sha1 == needle) {
			return e;
		}
	}
	return {};
}

bool ModMetadataIndex::put(const Entry& entry)
{
	if (!entry.isValid()) {
		qWarning() << "ModMetadataIndex::put refused invalid entry";
		return false;
	}

	const QString canon = canonicalFileName(entry.fileName);

	QString name = Packwiz::sidecarFileName(entry);
	QString previous;
	{
		QMutexLocker lock(&m_mutex);
		previous = m_sidecars.value(canon);
		for (auto it = m_sidecars.constBegin(); it != m_sidecars.constEnd();
			 ++it) {
			if (it.key() == canon) {
				continue;
			}
			if (it.value().compare(name, Qt::CaseInsensitive) != 0) {
				continue;
			}
			/* Another file of this project already answers to the name
			 * we would like. Taking it would erase that file's history,
			 * so this one gets the archive-derived name instead. */
			name = Packwiz::fallbackSidecarFileName(entry);
			break;
		}
	}
	if (name.isEmpty()) {
		qWarning() << "ModMetadataIndex::put cannot name a sidecar for"
				   << entry.fileName;
		return false;
	}

	FS::ensureFolderPathExists(m_indexDir.absolutePath());

	const QByteArray payload = Packwiz::serialize(entry);
	if (payload.isEmpty()) {
		qWarning() << "ModMetadataIndex::put could not serialize entry for"
				   << entry.fileName;
		return false;
	}
	const QString path = sidecarPath(name);

	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qWarning() << "ModMetadataIndex::put cannot open" << path
				   << "for writing";
		return false;
	}
	if (f.write(payload) != payload.size()) {
		f.cancelWriting();
		qWarning() << "ModMetadataIndex::put short write to" << path;
		return false;
	}
	if (!f.commit()) {
		qWarning() << "ModMetadataIndex::put failed to commit" << path;
		return false;
	}

	{
		QMutexLocker lock(&m_mutex);
		m_entries.insert(canon, entry);
		m_sidecars.insert(canon, name);
	}

	/* Replace rather than accumulate: an entry that used to live in a
	 * legacy JSON sidecar, or under a slug we have since learned, must
	 * leave exactly one file behind. This is also what converts a folder
	 * to the packwiz format - one entry at a time, as it is written. */
	if (!previous.isEmpty() &&
		previous.compare(name, Qt::CaseInsensitive) != 0) {
		dropSidecarOwnedBy(previous, canon);
	}
	return true;
}

bool ModMetadataIndex::remove(const QString& fileName)
{
	const QString canon = canonicalFileName(fileName);
	QString name;
	Entry entry;
	{
		QMutexLocker lock(&m_mutex);
		name = m_sidecars.take(canon);
		entry = m_entries.take(canon);
	}

	if (!name.isEmpty()) {
		return dropSidecarOwnedBy(name, canon);
	}

	/* Nothing recorded, so the index was probably never loaded. Try the
	 * names this file's sidecar could have been written under - the
	 * ownership check inside keeps us from deleting a sibling's. */
	QStringList candidates;
	if (entry.isValid()) {
		candidates << Packwiz::sidecarFileName(entry)
				   << Packwiz::fallbackSidecarFileName(entry);
	} else {
		Entry probe;
		probe.fileName = canon;
		candidates << Packwiz::fallbackSidecarFileName(probe);
	}
	candidates << canon + QStringLiteral(".json");

	for (const QString& candidate : candidates) {
		if (dropSidecarOwnedBy(candidate, canon)) {
			return true;
		}
	}
	return false;
}

QString ModMetadataIndex::sidecarPathFor(const QString& fileName) const
{
	const QString canon = canonicalFileName(fileName);
	QMutexLocker lock(&m_mutex);
	const QString name = m_sidecars.value(canon);
	if (name.isEmpty()) {
		return {};
	}
	return sidecarPath(name);
}

void ModMetadataIndex::rename(const QString& oldFileName,
							  const QString& newFileName)
{
	const QString oldCanon = canonicalFileName(oldFileName);
	const QString newCanon = canonicalFileName(newFileName);
	if (oldCanon == newCanon) {
		return; /* Toggling enabled/disabled keeps canonical name identical. */
	}

	Entry e;
	QString oldSidecar;
	{
		QMutexLocker lock(&m_mutex);
		if (!m_entries.contains(oldCanon)) {
			return;
		}
		e = m_entries.take(oldCanon);
		oldSidecar = m_sidecars.take(oldCanon);
	}

	e.fileName = newCanon;
	if (!put(e)) {
		return;
	}

	QString newSidecar;
	{
		QMutexLocker lock(&m_mutex);
		newSidecar = m_sidecars.value(newCanon);
	}

	/* Cleaned up after the new sidecar exists, and only when the name
	 * really changed: a sidecar is named after the project, so it usually
	 * keeps its name across a rename and deleting it here would delete
	 * what put() has just written. The old file still describes the old
	 * name, which is what the ownership check has to be given. */
	if (oldSidecar.compare(newSidecar, Qt::CaseInsensitive) != 0) {
		dropSidecarOwnedBy(oldSidecar, oldCanon);
	}
}
