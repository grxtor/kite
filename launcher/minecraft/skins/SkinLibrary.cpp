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

#include "SkinLibrary.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

#include "FileSystem.h"
#include "Json.h"

namespace
{
	/* The index lives inside the library directory, so it travels with it. */
	const char* const kIndexFileName = "index.json";
	const char* const kIndexSkinsKey = "skins";

	/* Find a file name in `directory` that is not taken yet, by appending a
	 * counter before the extension.
	 *
	 * Bounded on purpose: if 256 variations are all taken, something is wrong
	 * with the directory rather than with this name, and looping forever
	 * would only hide it. */
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

	/* Files and directories both, so that a stray subdirectory does not make
	 * the listing look empty; non-PNG entries are filtered out per entry
	 * during the rescan. Locale-aware name sorting is what makes the list
	 * order match what the user sees in their file manager. */
	m_dir.setFilter(QDir::Readable | QDir::NoDotAndDotDot | QDir::Files |
					QDir::Dirs);
	m_dir.setSorting(QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);

	m_watcher = new QFileSystemWatcher(this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&SkinLibrary::onDirectoryChanged);
	connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
			&SkinLibrary::onFileChanged);

	/* Arms the watcher and does the first rescan. Watching from the start is
	 * what makes "Open Folder", drop a PNG in, and see it appear work without
	 * reopening the dialog -- and the import buttons rely on it too, since
	 * they only copy a file into place and let the watcher notice. */
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

	/* Both halves are required: the URL is the identity we match on, and
	 * without the bytes there is nothing to write to disk. An account that
	 * has a URL but no cached data will be adopted on a later refresh. */
	if (worn.url.isEmpty() || worn.data.isEmpty()) {
		return false;
	}

	for (SkinEntry& existing : entries) {
		if (existing.textureUrl() != worn.url) {
			continue;
		}
		/* Already in the library. The file is right, but the cape and arm
		 * width may have been changed elsewhere (in game, or on another
		 * machine), so those come from the profile every time. */
		existing.setCapeId(
			m_account->accountData()->minecraftProfile.currentCape);
		existing.setArms(armsFromVariant(worn.variant));
		return false;
	}

	/* Not in the library yet -- save it under the profile name, which is
	 * what a user would recognise. If that name is taken by an unrelated
	 * skin, fall back to the file name from the texture URL. */
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
	adopted.setArms(armsFromVariant(worn.variant));
	adopted.setCapeId(m_account->accountData()->minecraftProfile.currentCape);
	adopted.setTextureUrl(worn.url);
	entries.append(adopted);
	return true;
}

void SkinLibrary::rescan()
{
	QList<SkinEntry> found;
	m_dir.refresh();

	/* Pass 1: the index. It carries the metadata, so it goes first and
	 * everything found later is treated as new. */
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
				/* An unusable entry means the file behind it is gone or is
				 * no longer a skin. Dropping it here is what lets the
				 * directory stay the source of truth. */
				if (entry.isUsable()) {
					found.append(entry);
				}
			}
		} catch (const Exception& e) {
			qCritical() << "Could not read the skin index:" << e.cause();
		}
	}

	bool needsSave = adoptAccountSkin(found);

	/* Pass 2: PNGs in the directory that the index did not account for --
	 * files the user dropped in with a file manager. */
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
		/* The library was pointed somewhere else (the setting changed).
		 * Move the watch along with it before rescanning. */
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
		/* Deletions arrive as a directory change too, which rescans. */
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
		/* Same file: keep the row, take the metadata. */
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

	/* A ".png" suffix is taken at face value; anything else has to prove it
	 * is a skin by decoding to the right dimensions. Decoding is the
	 * expensive check, so it only runs when the name gives no hint. */
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

void SkinLibrary::importFiles(const QStringList& sourcePaths)
{
	for (const QString& path : sourcePaths) {
		const QString problem = importFile(path);
		if (!problem.isEmpty()) {
			qWarning() << "Skipped" << path << "while importing skins:"
					   << problem;
		}
	}
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
	/* removeAt() rather than remove(): QList only grew an index-taking
	 * remove() in Qt 6, and this builds against Qt 5.15 as well. */
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
			/* The flat sprite is what the list is meant to show; the raw
			 * texture is only a fallback for the case where the sprite could
			 * not be produced, so the row still shows *something*. QImage is
			 * consumable by QML as-is, so ImageRole needs no conversion. */
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
		/* Committing an unchanged name is not a failure. */
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
	/* Drops are accepted on the empty area as well as on rows, so the flag
	 * is unconditional. */
	Qt::ItemFlags result =
		Qt::ItemIsDropEnabled | QAbstractListModel::flags(index);
	if (index.isValid()) {
		result |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	}
	return result;
}

QStringList SkinLibrary::mimeTypes() const
{
	return {QStringLiteral("text/uri-list")};
}

Qt::DropActions SkinLibrary::supportedDropActions() const
{
	/* Copy only: a dropped skin is imported into the library, the original
	 * is left where the user had it. */
	return Qt::CopyAction;
}

bool SkinLibrary::dropMimeData(const QMimeData* data, Qt::DropAction action,
							   int row, int column, const QModelIndex& parent)
{
	Q_UNUSED(row)
	Q_UNUSED(column)
	Q_UNUSED(parent)

	if (action == Qt::IgnoreAction) {
		return true;
	}
	if (!data || !(action & supportedDropActions())) {
		return false;
	}
	if (!data->hasUrls()) {
		return false;
	}

	QStringList files;
	for (const QUrl& url : data->urls()) {
		/* Remote URLs are the "Import URL" button's job; a drop has to be
		 * something already on this machine. */
		if (url.isLocalFile()) {
			files.append(url.toLocalFile());
		}
	}
	if (files.isEmpty()) {
		return false;
	}

	importFiles(files);
	return true;
}
