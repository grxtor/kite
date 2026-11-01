/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "SkinLibrary.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>

#include "FileSystem.h"
#include "Json.h"

namespace
{
	// The index lives inside the library directory, so it travels with it.
	const char* const kIndexFileName = "index.json";
	const char* const kIndexSkinsKey = "skins";

	// Finds a file name in `directory` that is not taken, appending a counter
	// before the extension. Bounded: 256 taken names mean something is wrong
	// with the directory, and looping forever would hide it.
	QString availableName(const QDir& directory, const QString& fileName)
	{
		if (!directory.exists(fileName)) {
			return directory.absoluteFilePath(fileName);
		}

		const QFileInfo info(fileName);
		const QString base = info.completeBaseName();
		const QString extension = info.suffix();

		for (int attempt = 1; attempt <= 256; ++attempt) {
			const QString candidate =
				QStringLiteral("%1%2.%3").arg(base).arg(attempt).arg(extension);
			if (!directory.exists(candidate)) {
				return directory.absoluteFilePath(candidate);
			}
		}
		return QString();
	}

	SkinEntry::Arms armsFromVariant(const QString& variant)
	{
		return variant.toUpper() == QLatin1String("SLIM")
				   ? SkinEntry::Arms::Slim
				   : SkinEntry::Arms::Classic;
	}
} // namespace

SkinLibrary::SkinLibrary(QObject* parent, const QString& path,
						 MinecraftAccountPtr account)
	: QAbstractListModel(parent), m_account(account)
{
	m_dir.setPath(path);
	FS::ensureFolderPathExists(m_dir.absolutePath());

	// Files and directories both, so a stray subdirectory does not make the
	// listing look empty; non-PNG entries are filtered during the rescan.
	// Locale-aware sorting matches the user's file manager.
	m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files |
					QDir::Dirs);
	m_dir.setSorting(QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);

	m_watcher = new QFileSystemWatcher(this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&SkinLibrary::onDirectoryChanged);
	connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
			&SkinLibrary::onFileChanged);

	// Arms the watcher and does the first rescan. Import buttons only copy a
	// file into place and rely on the watcher to notice it.
	startWatching();
}

SkinLibrary::~SkinLibrary()
{
	save();
}

void SkinLibrary::startWatching()
{
	if (m_isWatching) {
		return;
	}
	rescan();
	m_isWatching = m_watcher->addPath(m_dir.absolutePath());
	if (m_isWatching) {
		qDebug() << "Watching skin library at" << m_dir.absolutePath();
	} else {
		qWarning() << "Could not watch skin library at"
				   << m_dir.absolutePath()
				   << "- skins added outside the launcher will only appear "
					  "after reopening the dialog";
	}
}

void SkinLibrary::stopWatching()
{
	save();
	if (!m_isWatching) {
		return;
	}
	m_isWatching = !m_watcher->removePath(m_dir.absolutePath());
}

bool SkinLibrary::adoptAccountSkin(QList<SkinEntry>& entries) const
{
	if (!m_account || !m_account->accountData()) {
		return false;
	}

	const Skin& worn = m_account->accountData()->minecraftProfile.skin;

	// Both halves are required: the URL is the identity we match on and the
	// bytes are what gets written. An account with a URL but no data is adopted
	// on a later refresh.
	if (worn.url.isEmpty() || worn.data.isEmpty()) {
		return false;
	}

	const SkinEntry::Arms wornArms = armsFromVariant(worn.variant);
	for (SkinEntry& existing : entries) {
		if (existing.textureUrl() != worn.url) {
			continue;
		}
		// Already in the library. The cape comes from the profile every time
		// (it may have changed elsewhere). The arm width only when the
		// profile's changed since the library last saw it: every index save
		// rescans, and taking it each time would undo a width picked here and
		// not worn yet.
		existing.setCapeId(
			m_account->accountData()->minecraftProfile.currentCape);
		if (existing.wornArms() == wornArms) {
			return false;
		}
		existing.setArms(wornArms);
		existing.setWornArms(wornArms);
		return true;
	}

	// Not in the library yet: save it under the profile name; if an unrelated
	// skin has that name, use the file name from the texture URL.
	QString fileName = m_account->profileName() + QStringLiteral(".png");
	if (m_dir.exists(fileName)) {
		fileName = QUrl(worn.url).fileName() + QStringLiteral(".png");
	}
	const QString path = m_dir.absoluteFilePath(fileName);

	QImage texture;
	if (!texture.loadFromData(worn.data, "PNG")) {
		qWarning() << "The skin the account is wearing could not be decoded, "
					  "so it was not added to the library";
		return false;
	}
	if (!texture.save(path, "PNG")) {
		qWarning() << "Could not write the account's skin to" << path;
		return false;
	}

	SkinEntry adopted(path);
	adopted.setArms(wornArms);
	adopted.setWornArms(wornArms);
	adopted.setCapeId(m_account->accountData()->minecraftProfile.currentCape);
	adopted.setTextureUrl(worn.url);
	entries.append(adopted);
	return true;
}

void SkinLibrary::rescan()
{
	QList<SkinEntry> found;
	m_dir.refresh();

	// Pass 1: the index. It carries the metadata, so everything found later is
	// new.
	const QFileInfo indexInfo(
		m_dir.absoluteFilePath(QLatin1String(kIndexFileName)));
	if (indexInfo.exists()) {
		try {
			const QJsonDocument doc = Json::requireDocument(
				indexInfo.absoluteFilePath(), QStringLiteral("skin index"));
			const QJsonArray records =
				doc.object().value(QLatin1String(kIndexSkinsKey)).toArray();
			for (const QJsonValue& record : records) {
				SkinEntry entry(m_dir, record.toObject());
				// An unusable entry's file is gone or no longer a skin;
				// dropping it keeps the directory the source of truth.
				if (entry.isUsable()) {
					found.append(entry);
				}
			}
		} catch (const Exception& e) {
			qCritical() << "Could not read the skin index:" << e.cause();
		}
	}

	bool needsSave = adoptAccountSkin(found);

	// Pass 2: PNGs the index did not account for (dropped in with a file
	// manager).
	const QFileInfoList contents = m_dir.entryInfoList();
	for (const QFileInfo& candidate : contents) {
		if (!candidate.isFile() ||
			candidate.suffix().compare(QLatin1String("png"),
									   Qt::CaseInsensitive) != 0) {
			continue;
		}

		SkinEntry entry(candidate.absoluteFilePath());
		if (!entry.isUsable()) {
			continue;
		}

		const QString name = entry.name();
		const bool known =
			std::any_of(found.cbegin(), found.cend(),
						[&name](const SkinEntry& e) { return e.name() == name; });
		if (!known) {
			found.append(entry);
			needsSave = true;
		}
	}

	std::sort(found.begin(), found.end(),
			  [](const SkinEntry& a, const SkinEntry& b) {
				  return a.path().localeAwareCompare(b.path()) < 0;
			  });

	beginResetModel();
	m_entries.swap(found);
	endResetModel();

	if (needsSave) {
		save();
	}
}

void SkinLibrary::onDirectoryChanged(const QString& path)
{
	QDir changed(path);
	if (!changed.exists() &&
		!FS::ensureFolderPathExists(changed.absolutePath())) {
		qWarning() << "Skin library directory" << changed.absolutePath()
				   << "does not exist and could not be created";
		return;
	}

	if (m_dir.absolutePath() != changed.absolutePath()) {
		// The library was pointed elsewhere (setting changed): move the watch
		// first.
		m_dir.setPath(path);
		m_dir.refresh();
		if (m_isWatching) {
			stopWatching();
		}
		startWatching();
		return;
	}
	rescan();
}

void SkinLibrary::onFileChanged(const QString& path)
{
	const QFileInfo changed(path);
	if (!changed.exists()) {
		// Deletions arrive as a directory change too, which rescans.
		return;
	}

	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries[row].path() != changed.absoluteFilePath()) {
			continue;
		}
		m_entries[row].reload();
		const QModelIndex changedIndex = index(row);
		emit dataChanged(changedIndex, changedIndex);
		return;
	}
}

int SkinLibrary::indexOfName(const QString& name) const
{
	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries[row].name() == name) {
			return row;
		}
	}
	return -1;
}

int SkinLibrary::indexOfAccountSkin() const
{
	if (!m_account || !m_account->accountData()) {
		return -1;
	}
	const QString wornUrl =
		m_account->accountData()->minecraftProfile.skin.url;
	if (wornUrl.isEmpty()) {
		return -1;
	}
	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries[row].textureUrl() == wornUrl) {
			return row;
		}
	}
	return -1;
}

const SkinEntry* SkinLibrary::entry(const QString& name) const
{
	const int row = indexOfName(name);
	return row == -1 ? nullptr : &m_entries[row];
}

SkinEntry* SkinLibrary::entry(const QString& name)
{
	const int row = indexOfName(name);
	return row == -1 ? nullptr : &m_entries[row];
}

void SkinLibrary::save()
{
	QJsonArray records;
	for (const SkinEntry& entry : m_entries) {
		records.append(entry.toRecord());
	}

	QJsonObject index;
	index[QLatin1String(kIndexSkinsKey)] = records;

	try {
		Json::write(index,
					m_dir.absoluteFilePath(QLatin1String(kIndexFileName)));
	} catch (const Exception& e) {
		qCritical() << "Could not write the skin index:" << e.cause();
	}
}

void SkinLibrary::mergeEntry(const SkinEntry& incoming)
{
	for (int row = 0; row < m_entries.size(); ++row) {
		if (m_entries[row].path() != incoming.path()) {
			continue;
		}
		// Same file: keep the row, take the metadata.
		m_entries[row].setCapeId(incoming.capeId());
		m_entries[row].setArms(incoming.arms());
		m_entries[row].setTextureUrl(incoming.textureUrl());
		const QModelIndex changedIndex = index(row);
		emit dataChanged(changedIndex, changedIndex);
		save();
		return;
	}

	const int row = m_entries.size();
	beginInsertRows(QModelIndex(), row, row);
	m_entries.append(incoming);
	endInsertRows();
	save();
}

QString SkinLibrary::importFile(const QString& sourcePath,
								const QString& preferredName)
{
	if (sourcePath.isEmpty()) {
		return tr("Path is empty.");
	}

	const QFileInfo source(sourcePath);
	if (!source.exists()) {
		return tr("File doesn't exist.");
	}
	if (!source.isFile()) {
		return tr("Not a file.");
	}
	if (!source.isReadable()) {
		return tr("File is not readable.");
	}

	// A ".png" suffix is taken at face value; anything else must decode to the
	// right dimensions, which is expensive, so it runs only without that hint.
	if (source.suffix().compare(QLatin1String("png"), Qt::CaseInsensitive) !=
			0 &&
		!SkinEntry(source.absoluteFilePath()).isUsable()) {
		return tr("Skin images must be 64x64 or 64x32 pixel PNG files.");
	}

	const QString target = availableName(
		m_dir, preferredName.isEmpty() ? source.fileName() : preferredName);
	if (target.isEmpty()) {
		return tr("Unable to find a free file name in the skins folder.");
	}

	if (!QFile::copy(source.absoluteFilePath(), target)) {
		return tr("Unable to copy file");
	}
	return QString();
}

bool SkinLibrary::remove(const QString& name, bool toTrash)
{
	const int row = indexOfName(name);
	if (row == -1) {
		return false;
	}

	const QString path = m_entries[row].path();
	const bool gone = toTrash ? FS::trash(path) : QFile::remove(path);
	if (!gone) {
		return false;
	}

	beginRemoveRows(QModelIndex(), row, row);
	m_entries.removeAt(row);
	endRemoveRows();
	save();
	return true;
}

QVariant SkinLibrary::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) {
		return QVariant();
	}
	const int row = index.row();
	if (row < 0 || row >= m_entries.size()) {
		return QVariant();
	}

	const SkinEntry& entry = m_entries[row];
	switch (role) {
		case Qt::DecorationRole:
		case ImageRole: {
			// The flat sprite is what the list shows; the raw texture is the
			// fallback. QImage is consumable by QML as-is.
			const QImage thumbnail = entry.thumbnail();
			return thumbnail.isNull() ? entry.texture() : thumbnail;
		}
		case Qt::DisplayRole:
		case Qt::EditRole:
		case Qt::UserRole:
		case NameRole:
			return entry.name();
		default:
			return QVariant();
	}
}

bool SkinLibrary::setData(const QModelIndex& index, const QVariant& value,
						  int role)
{
	if (!index.isValid() || role != Qt::EditRole) {
		return false;
	}
	const int row = index.row();
	if (row < 0 || row >= m_entries.size()) {
		return false;
	}

	SkinEntry& entry = m_entries[row];
	const QString newName = value.toString();
	if (entry.name() == newName) {
		// Committing an unchanged name is not a failure.
		return true;
	}
	if (!entry.renameTo(newName)) {
		return false;
	}

	emit dataChanged(index, index);
	save();
	return true;
}

int SkinLibrary::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_entries.size();
}

QHash<int, QByteArray> SkinLibrary::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(NameRole, "name");
	roles.insert(ImageRole, "image");
	return roles;
}

Qt::ItemFlags SkinLibrary::flags(const QModelIndex& index) const
{
	Qt::ItemFlags result = QAbstractListModel::flags(index);
	if (index.isValid()) {
		result |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	}
	return result;
}
