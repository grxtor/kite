/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QFutureWatcher>
#include <QThread>
#include <QThreadPool>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QTimer>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QUrl>
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QtConcurrentRun>
#include <algorithm>

#include "InstanceList.h"
#include "crashdoctor/CrashDoctorService.h"
#include "BaseInstance.h"
#include "InstanceTask.h"
#include "settings/INISettingsObject.h"
#include "minecraft/legacy/LegacyInstance.h"
#include "NullInstance.h"
#include "minecraft/MinecraftInstance.h"
#include "FileSystem.h"
#include "ExponentialSeries.h"
#include "WatchLock.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "net/HttpMetaCache.h"
#include "screenshots/ScreenshotListModel.h"
#include "modplatform/PlatformCoverFetcher.h"
#include "launch/LaunchProgressTracker.h"
#include "launch/LaunchTask.h"

const static int GROUP_FILE_FORMAT_VERSION = 1;

InstanceList::InstanceList(SettingsObjectPtr settings,
						   const QStringList& instDirs, QObject* parent)
	: QAbstractListModel(parent), m_globalSettings(settings)
{
	connect(this, &InstanceList::instancesChanged, this,
			&InstanceList::providerUpdated);

	m_watcher = new QFileSystemWatcher(this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&InstanceList::instanceDirContentsChanged);

	applyInstanceDirs(resolveInstanceDirs(instDirs));

	const auto crashCauseChanged = [this](const QString& id) {
		const QModelIndex row = getInstanceIndexById(id);
		if (row.isValid()) {
			emit dataChanged(row, row, {CrashCauseRole});
		}
	};
	connect(CrashDoctorService::instance(), &CrashDoctorService::analysed, this,
			crashCauseChanged);
	connect(CrashDoctorService::instance(), &CrashDoctorService::forgotten, this,
			crashCauseChanged);

	m_platformCovers = new PlatformCoverFetcher(this);
	connect(m_platformCovers, &PlatformCoverFetcher::finished, this,
			[this](const QString& id, PlatformCoverFetcher::Outcome outcome,
				   const QString& fileName) {
				InstancePtr inst = getInstanceById(id);
				if (!inst || outcome == PlatformCoverFetcher::Outcome::Failed) {
					return;
				}
				if (outcome == PlatformCoverFetcher::Outcome::Found) {
					inst->setPlatformCover(fileName, 0);
				} else {
					inst->setPlatformCover(
						QString(), QDateTime::currentSecsSinceEpoch());
				}
			});
}

QStringList InstanceList::resolveInstanceDirs(const QStringList& dirs) const
{
	QStringList resolved;
	for (const QString& dir : dirs) {
		if (dir.isEmpty()) {
			continue;
		}
		/* Created before canonicalising: canonicalPath() returns nothing for a
		 * path that does not exist, so the order matters. */
		if (!QDir::current().exists(dir) && !QDir::current().mkpath(dir)) {
			qWarning() << "Ignoring instance folder" << dir
					   << "- it does not exist and could not be created";
			continue;
		}
		const QString canonical = QDir(dir).canonicalPath();
		if (canonical.isEmpty()) {
			qWarning() << "Ignoring instance folder" << dir
					   << "- could not be resolved";
			continue;
		}
		/* Two settings naming one folder, or a path spelled two ways, would
		 * make every instance in it a duplicate of itself and get skipped. */
		if (resolved.contains(canonical)) {
			qDebug() << "Ignoring duplicate instance folder" << dir;
			continue;
		}
		resolved << canonical;
	}
	return resolved;
}

void InstanceList::applyInstanceDirs(const QStringList& resolved)
{
	for (const QString& dir : m_instDirs) {
		m_watcher->removePath(dir);
	}
	m_instDirs = resolved;
	for (const QString& dir : m_instDirs) {
		m_watcher->addPath(dir);
	}
	qDebug() << "Instance folders:" << m_instDirs;
}

QString InstanceList::rootDirOf(const InstanceId& id) const
{
	return m_instanceRootDirMap.value(id, primaryDir());
}

QStringList InstanceList::decodeInstanceDirList(const QVariant& value)
{
	/* A genuine QStringList is accepted as well as the encoded form: a value
	 * set and read back within one session never passes through the INI file,
	 * and a hand-edited config can be either. */
	if (value.userType() == QMetaType::QStringList) {
		return value.toStringList();
	}

	const QString encoded = value.toString().trimmed();
	if (encoded.isEmpty()) {
		return {};
	}

	QJsonParseError error;
	const QJsonDocument doc = QJsonDocument::fromJson(encoded.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !doc.isArray()) {
		qWarning() << "Could not read the additional instance folders setting"
				   << "-" << error.errorString()
				   << "- ignoring it rather than guessing";
		return {};
	}

	QStringList out;
	const QJsonArray array = doc.array();
	for (const QJsonValue& entry : array) {
		const QString dir = entry.toString();
		if (!dir.isEmpty()) {
			out << dir;
		}
	}
	return out;
}

QVariant InstanceList::encodeInstanceDirList(const QStringList& dirs)
{
	QJsonArray array;
	for (const QString& dir : dirs) {
		if (!dir.isEmpty()) {
			array.append(dir);
		}
	}
	return QString::fromUtf8(
		QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QString InstanceList::rootForStaging(const QString& stagingPath) const
{
	if (stagingPath.isEmpty()) {
		return QString();
	}
	const QString absolute = QFileInfo(stagingPath).absoluteFilePath();
	for (const QString& dir : m_instDirs) {
		/* The separator matters: without it "/data/instances" would claim a
		 * staging path under "/data/instances-backup". */
		if (absolute.startsWith(dir + QLatin1Char('/'))) {
			return dir;
		}
	}
	return QString();
}

InstanceList::~InstanceList()
{
	/* Drops the last InstancePtr reference to each instance while every member
	 * is alive: otherwise m_instances outlives m_launchTrackers, and
	 * destroying an instance fires trackLaunchProgress()'s lambda against a
	 * torn-down QHash. Shutdown calls _exit() before destructors (main.cpp),
	 * but a unit test with the list on the stack hits it. */
	m_instances.clear();
}

int InstanceList::rowCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	return m_instances.count();
}

QModelIndex InstanceList::index(int row, int column,
								const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	if (row < 0 || row >= m_instances.size())
		return QModelIndex();
	return createIndex(row, column, (void*)m_instances.at(row).get());
}

namespace
{
// Copies of picked screenshots; a cover set before copies existed names the screenshot itself.
const QString kCustomCoverPrefix = QStringLiteral("kite-cover-custom");

// The picked cover as a full path.
QString coverFilePath(const BaseInstance* inst)
{
	const QString name = QFileInfo(inst->coverImage()).fileName();
	if (name.isEmpty()) {
		return QString();
	}
	return name.startsWith(kCustomCoverPrefix)
			   ? FS::PathCombine(inst->instanceRoot(), name)
			   : FS::PathCombine(inst->gameRoot(), "screenshots", name);
}

// The fetched modpack art as a full path.
QString platformCoverPath(const BaseInstance* inst)
{
	const QString name = QFileInfo(inst->platformCover()).fileName();
	return name.isEmpty() ? QString()
						  : FS::PathCombine(inst->instanceRoot(), name);
}
} // namespace

QVariant InstanceList::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) {
		return QVariant();
	}
	BaseInstance* pdata = static_cast<BaseInstance*>(index.internalPointer());
	switch (role) {
		case InstancePointerRole: {
			QVariant v = QVariant::fromValue((void*)pdata);
			return v;
		}
		case InstanceIDRole: {
			return pdata->id();
		}
		case Qt::EditRole:
		case Qt::DisplayRole: {
			return pdata->name();
		}
		case Qt::AccessibleTextRole: {
			return tr("%1 Instance").arg(pdata->name());
		}
		case Qt::ToolTipRole: {
			return pdata->instanceRoot();
		}
		case Qt::DecorationRole: {
			return pdata->iconKey();
		}
		case GroupRole: {
			return getInstanceGroup(pdata->id());
		}
		// QML-only roles.
		case IsRunningRole: {
			return pdata->isRunning();
		}
		case CanLaunchRole: {
			return pdata->canLaunch();
		}
		case LastLaunchRole: {
			return pdata->lastLaunch();
		}
		case TotalTimePlayedRole: {
			// int64_t is long on LP64 Linux, which QVariant has no
			// constructor for; qint64 (long long) it has.
			return static_cast<qint64>(pdata->totalTimePlayed());
		}
		case GameVersionRole: {
			return pdata->gameVersion();
		}
		case LoaderRole:
			// Empty for vanilla: the UI words that itself, so it can tell
			// "no loader" apart from a loader's name in any language.
			return pdata->modLoaderName();
		case IconTintRole: {
			auto* context = LauncherContext::instance();
			return context ? context->icons()->tint(pdata->iconKey())
						   : QColor();
		}
		case LaunchStatusRole: {
			auto* tracker = m_launchTrackers.value(pdata, nullptr);
			return tracker ? tracker->status() : QString();
		}
		case LaunchProgressRole: {
			auto* tracker = m_launchTrackers.value(pdata, nullptr);
			return tracker ? tracker->progress() : -1.0;
		}
		case CoverImageRole:
		case CoverKindRole: {
			const InstanceId id = pdata->id();
			const QString customPath = coverFilePath(pdata);
			const QString platformPath = platformCoverPath(pdata);
			const QString version = pdata->gameVersion();
			CoverEntry entry;
			auto it = m_coverCache.constFind(id);
			if (it != m_coverCache.constEnd()) {
				entry = it.value();
			}
			if (it == m_coverCache.constEnd() ||
				entry.customPath != customPath ||
				entry.platformPath != platformPath ||
				entry.version != version) {
				/* Missing or made for other values: look up in the background
				 * and answer with the old entry (or nothing) meanwhile; the
				 * row picks up the result with its dataChanged. data() is
				 * const, hence the const_cast: scheduling mutates
				 * m_coverScansPending and connects a QFutureWatcher. */
				const_cast<InstanceList*>(this)->scheduleCoverScan(
					id, customPath, platformPath, version);
			}
			return role == CoverImageRole ? entry.info.url : entry.info.kind;
		}
		case CoverSourceRole: {
			const QString source = pdata->coverSource();
			const QString image = pdata->coverImage();
			return source.isEmpty() && !image.startsWith(kCustomCoverPrefix)
					   ? image
					   : source;
		}
		case HasCrashedRole: {
			return pdata->hasCrashed();
		}
		case CrashCauseRole: {
			return pdata->hasCrashed()
					   ? CrashDoctorService::instance()->topCause(pdata->id())
					   : QString();
		}
		case PackArtRole: {
			if (!pdata->isManagedPack()) {
				return QString();
			}
			auto* context = LauncherContext::instance();
			const QString key = pdata->iconKey();
			if (context && context->icons()->iconFileExists(key)) {
				return QStringLiteral("image://instanceicon/") + key;
			}
			return pdata->settings()->get("PackIconUrl").toString();
		}
		default:
			break;
	}
	return QVariant();
}

QHash<int, QByteArray> InstanceList::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(InstanceIDRole, "instanceId");
	roles.insert(Qt::DisplayRole, "name");
	roles.insert(Qt::DecorationRole, "iconKey");
	roles.insert(Qt::ToolTipRole, "instanceRoot");
	roles.insert(GroupRole, "group");
	roles.insert(IsRunningRole, "isRunning");
	roles.insert(CanLaunchRole, "canLaunch");
	roles.insert(LastLaunchRole, "lastLaunch");
	roles.insert(TotalTimePlayedRole, "totalTimePlayed");
	roles.insert(GameVersionRole, "gameVersion");
	roles.insert(LoaderRole, "loader");
	roles.insert(IconTintRole, "iconTint");
	roles.insert(LaunchStatusRole, "launchStatus");
	roles.insert(LaunchProgressRole, "launchProgress");
	roles.insert(CoverImageRole, "coverImage");
	roles.insert(CoverKindRole, "coverKind");
	roles.insert(CoverSourceRole, "coverSource");
	roles.insert(HasCrashedRole, "hasCrashed");
	roles.insert(CrashCauseRole, "crashCause");
	roles.insert(PackArtRole, "packArt");
	return roles;
}

bool InstanceList::setData(const QModelIndex& index, const QVariant& value,
						   int role)
{
	if (!index.isValid()) {
		return false;
	}
	if (role != Qt::EditRole) {
		return false;
	}
	BaseInstance* pdata = static_cast<BaseInstance*>(index.internalPointer());
	auto newName = value.toString();
	if (pdata->name() == newName) {
		return true;
	}
	pdata->setName(newName);
	return true;
}

Qt::ItemFlags InstanceList::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f;
	if (index.isValid()) {
		f |= (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
	}
	return f;
}

GroupId InstanceList::getInstanceGroup(const InstanceId& id) const
{
	auto inst = getInstanceById(id);
	if (!inst) {
		return GroupId();
	}
	auto iter = m_instanceGroupIndex.find(inst->id());
	if (iter != m_instanceGroupIndex.end()) {
		return *iter;
	}
	return GroupId();
}

void InstanceList::setInstanceGroup(const InstanceId& id, const GroupId& name)
{
	auto inst = getInstanceById(id);
	if (!inst) {
		qDebug() << "Attempt to set a null instance's group";
		return;
	}

	bool changed = false;
	auto iter = m_instanceGroupIndex.find(inst->id());
	if (iter != m_instanceGroupIndex.end()) {
		if (*iter != name) {
			*iter = name;
			changed = true;
		}
	} else {
		changed = true;
		m_instanceGroupIndex[id] = name;
	}

	if (changed) {
		m_groupNameCache.insert(name);
		auto idx = getInstIndex(inst.get());
		emit dataChanged(index(idx), index(idx), {GroupRole});
		saveGroupList();
	}
}

QStringList InstanceList::getGroups()
{
	return m_groupNameCache.values();
}

void InstanceList::deleteGroup(const QString& name)
{
	bool removed = false;
	qDebug() << "Delete group" << name;
	for (auto& instance : m_instances) {
		const auto& instID = instance->id();
		auto instGroupName = getInstanceGroup(instID);
		if (instGroupName == name) {
			m_instanceGroupIndex.remove(instID);
			qDebug() << "Remove" << instID << "from group" << name;
			removed = true;
			auto idx = getInstIndex(instance.get());
			if (idx >= 0) {
				emit dataChanged(index(idx), index(idx), {GroupRole});
			}
		}
	}
	/* Also removed from the name list and the collapsed set, or it would still
	 * be offered as a group to move instances into. */
	m_groupNameCache.remove(name);
	m_collapsedGroups.remove(name);
	if (removed) {
		saveGroupList();
	}
}

void InstanceList::renameGroup(const GroupId& from, const GroupId& to)
{
	if (from.isEmpty() || to.isEmpty() || from == to) {
		return;
	}

	bool changed = false;
	for (auto iter = m_instanceGroupIndex.begin();
		 iter != m_instanceGroupIndex.end(); ++iter) {
		if (*iter != from) {
			continue;
		}
		*iter = to;
		changed = true;
		const int idx = getInstIndex(getInstanceById(iter.key()).get());
		if (idx >= 0) {
			emit dataChanged(index(idx), index(idx), {GroupRole});
		}
	}

	m_groupNameCache.remove(from);
	if (m_collapsedGroups.remove(from)) {
		m_collapsedGroups.insert(to);
	}
	if (changed) {
		m_groupNameCache.insert(to);
		saveGroupList();
	}
}

bool InstanceList::isGroupCollapsed(const QString& group)
{
	return m_collapsedGroups.contains(group);
}

bool InstanceList::deleteInstance(const InstanceId& id)
{
	auto inst = getInstanceById(id);
	if (!inst) {
		qDebug() << "Cannot delete instance" << id
				 << ". No such instance is present (deleted externally?).";
		return false;
	}

	if (m_instanceGroupIndex.remove(id)) {
		saveGroupList();
	}

	/* Read before the folder goes: the list lives in the instance's config
	 * file, and shortcuts() checks the disk for moved files. */
	const QList<ShortcutData> shortcuts = inst->shortcuts();

	qDebug() << "Will delete instance" << id;
	if (!FS::deletePath(inst->instanceRoot())) {
		qWarning() << "Deletion of instance" << id
				   << "has not been completely successful ...";
		return false;
	}

	qDebug() << "Instance" << id << "has been deleted by Kite.";

	/* The shortcuts are dead whatever happens (their folder is gone), so a
	 * failure is only logged. */
	for (const ShortcutData& shortcut : shortcuts) {
		if (FS::deletePath(shortcut.filePath)) {
			qDebug() << "Deleted shortcut" << shortcut.name << "at"
					 << shortcut.filePath;
		} else {
			qWarning() << "Could not delete shortcut" << shortcut.name << "at"
					   << shortcut.filePath << "of deleted instance" << id;
		}
	}
	return true;
}

bool InstanceList::trashInstance(const InstanceId& id)
{
	auto inst = getInstanceById(id);
	if (!inst) {
		qWarning() << "Cannot trash instance" << id
				   << ". No such instance is present (deleted externally?).";
		return false;
	}

	const QString root = inst->instanceRoot();
	const GroupId group = m_instanceGroupIndex.value(id);
	/* Read while the folder is still there: shortcuts() checks the disk. */
	const QList<ShortcutData> shortcuts = inst->shortcuts();

	qDebug() << "Will trash instance" << id;
	QString trashedRoot;
	if (!FS::trash(root, &trashedRoot)) {
		qWarning() << "Could not move instance" << id << "to the trash.";
		return false;
	}
	qDebug() << "Instance" << id << "is in the trash at" << trashedRoot;

	/* Only now: had the move failed, the instance would still be here and
	 * would have lost its group for nothing. */
	if (m_instanceGroupIndex.remove(id)) {
		saveGroupList();
	}

	TrashedInstance record;
	record.id = id;
	record.name = inst->name();
	record.path = root;
	record.trashPath = trashedRoot;
	record.group = group;

	for (const ShortcutData& shortcut : shortcuts) {
		QString trashedShortcut;
		if (FS::trash(shortcut.filePath, &trashedShortcut)) {
			record.shortcuts.append({shortcut, trashedShortcut});
			continue;
		}

		/* A shortcut whose instance is in the trash launches nothing, so one
		 * that refuses to be trashed is deleted instead; losing its undo beats
		 * leaving it on the desktop. */
		qWarning() << "Could not trash shortcut" << shortcut.name << "at"
				   << shortcut.filePath << "-- deleting it instead.";
		if (!FS::deletePath(shortcut.filePath)) {
			qWarning() << "... and could not delete it either.";
		}
	}

	m_trashHistory.append(record);
	return true;
}

bool InstanceList::trashedSomething() const
{
	return !m_trashHistory.isEmpty();
}

QString InstanceList::lastTrashedName() const
{
	if (m_trashHistory.isEmpty()) {
		return {};
	}
	const TrashedInstance& record = m_trashHistory.last();
	return record.name.isEmpty() ? record.id : record.name;
}

bool InstanceList::undoTrashInstance()
{
	if (m_trashHistory.isEmpty()) {
		/* Nothing to restore is not a failed restore. */
		qWarning() << "Nothing to restore from the trash.";
		return true;
	}

	const TrashedInstance record = m_trashHistory.takeLast();

	/* An instance's id is its folder name, so if something appeared at that
	 * path meanwhile it owns the name; step aside to keep both instead of
	 * refusing. */
	InstanceId id = record.id;
	QString path = record.path;
	while (QFileInfo::exists(path)) {
		id += QLatin1Char('1');
		path += QLatin1Char('1');
	}

	if (!QFile::rename(record.trashPath, path)) {
		qWarning() << "Could not move" << record.trashPath << "back to" << path;
		/* The folder is still in the trash, so put the record back: the
		 * obstacle may be temporary (a file open, the volume busy) and a retry
		 * is worth allowing. */
		m_trashHistory.append(record);
		return false;
	}
	qDebug() << "Restored instance" << id << "from" << record.trashPath;

	bool complete = true;
	for (const TrashedShortcut& item : record.shortcuts) {
		/* Not stepped aside like the folder: a shortcut path ends in a suffix
		 * the shell reads (.lnk, .desktop, .app), and appending past it would
		 * produce something inert. */
		if (QFileInfo::exists(item.shortcut.filePath)) {
			qWarning() << "Not restoring shortcut" << item.shortcut.name
					   << "-- something else is at" << item.shortcut.filePath;
			complete = false;
			continue;
		}
		if (!QFile::rename(item.trashPath, item.shortcut.filePath)) {
			qWarning() << "Could not move shortcut" << item.shortcut.name
					   << "back to" << item.shortcut.filePath;
			complete = false;
			continue;
		}
		qDebug() << "Restored shortcut" << item.shortcut.name << "to"
				 << item.shortcut.filePath;
	}

	/* Set the group before the rescan, so the instance comes back in the group
	 * it left. */
	if (!record.group.isEmpty()) {
		m_instanceGroupIndex[id] = record.group;
		m_groupNameCache.insert(record.group);
	}

	// Synchronous: this lands in providerUpdated() -> loadList().
	emit instancesChanged();

	/* Only worth writing once the rescan has put the id in instanceSet;
	 * saveGroupList() skips unknown instances. */
	if (!record.group.isEmpty()) {
		saveGroupList();
	}

	emit instanceSelectRequest(id);
	return complete;
}

static QMap<InstanceId, InstanceLocator>
getIdMapping(const QList<InstancePtr>& list)
{
	QMap<InstanceId, InstanceLocator> out;
	int i = 0;
	for (auto& item : list) {
		auto id = item->id();
		if (out.contains(id)) {
			qWarning() << "Duplicate ID" << id << "in instance list";
		}
		out[id] = std::make_pair(item, i);
		i++;
	}
	return out;
}

QList<InstanceId> InstanceList::discoverInstances()
{
	QList<InstanceId> out;
	m_instanceRootDirMap.clear();

	for (const QString& rootDir : m_instDirs) {
		qDebug() << "Discovering instances in" << rootDir;
		QDirIterator iter(rootDir,
						  QDir::Dirs | QDir::NoDot | QDir::NoDotDot |
							  QDir::Readable | QDir::Hidden,
						  QDirIterator::FollowSymlinks);
		while (iter.hasNext()) {
			QString subDir = iter.next();
			QFileInfo dirInfo(subDir);
			if (!QFileInfo(FS::PathCombine(subDir, "instance.cfg")).exists())
				continue;
			/* A symlink into any configured root is the same instance reached
			 * a second way; checking only the root being walked would let a
			 * link in one folder resurrect an instance from another as a
			 * phantom duplicate. */
			if (dirInfo.isSymLink()) {
				const QString targetCanonical =
					QFileInfo(dirInfo.symLinkTarget()).canonicalFilePath();
				const bool pointsIntoAnyRoot = std::any_of(
					m_instDirs.cbegin(), m_instDirs.cend(),
					[&targetCanonical](const QString& otherRoot) {
						return targetCanonical.startsWith(
							QFileInfo(otherRoot).canonicalFilePath());
					});
				if (pointsIntoAnyRoot) {
					qDebug() << "Ignoring symlink" << subDir
							 << "that leads into a configured instance folder";
					continue;
				}
			}
			auto id = dirInfo.fileName();
			/* Ids are directory names, so two roots can each hold "1.20.1",
			 * but everything else (group index, shortcuts, play time) keys off
			 * the id alone. The first root wins, so the outcome follows the
			 * configured order rather than directory iteration, and the loser
			 * is reported. */
			if (m_instanceRootDirMap.contains(id)) {
				qWarning() << "Duplicate instance ID" << id << "found in"
						   << rootDir << "- already claimed by"
						   << m_instanceRootDirMap.value(id)
						   << ". Skipping the copy in" << rootDir;
				continue;
			}
			m_instanceRootDirMap[id] = rootDir;
			out.append(id);
			qDebug() << "Found instance ID" << id << "in" << rootDir;
		}
	}
	instanceSet = QSet<QString>(out.begin(), out.end());
	m_instancesProbed = true;
	return out;
}

InstanceList::InstListError InstanceList::loadList()
{
	auto existingIds = getIdMapping(m_instances);

	QList<InstancePtr> newList;

	for (auto& id : discoverInstances()) {
		if (existingIds.contains(id)) {
			auto instPair = existingIds[id];
			existingIds.remove(id);
			qDebug() << "Should keep and soft-reload" << id;
		} else {
			InstancePtr instPtr = loadInstance(id);
			if (instPtr) {
				newList.append(instPtr);
			}
		}
	}

	// TODO: looks like a general algorithm with a few specifics inserted. Do
	// something about it.
	if (!existingIds.isEmpty()) {
		// get the list of removed instances and sort it by their original
		// index, from last to first
		auto deadList = existingIds.values();
		auto orderSortPredicate = [](const InstanceLocator& a,
									 const InstanceLocator& b) -> bool {
			return a.second > b.second;
		};
		std::sort(deadList.begin(), deadList.end(), orderSortPredicate);
		// remove the contiguous ranges of rows
		int front_bookmark = -1;
		int back_bookmark = -1;
		int currentItem = -1;
		auto removeNow = [&]() {
			beginRemoveRows(QModelIndex(), front_bookmark, back_bookmark);
			m_instances.erase(m_instances.begin() + front_bookmark,
							  m_instances.begin() + back_bookmark + 1);
			endRemoveRows();
			front_bookmark = -1;
			back_bookmark = currentItem;
		};
		for (auto& removedItem : deadList) {
			auto instPtr = removedItem.first;
			instPtr->invalidate();
			// Otherwise these per-id caches would grow for as long as the
			// launcher runs, bounded only by every instance ever seen.
			const InstanceId removedId = instPtr->id();
			m_coverCache.remove(removedId);
			m_coverScansPending.remove(removedId);
			m_coverGeneration.remove(removedId);
			m_coverPicks.remove(removedId);
			currentItem = removedItem.second;
			if (back_bookmark == -1) {
				back_bookmark = currentItem;
			} else if (currentItem == front_bookmark - 1) {
			} else {
				removeNow();
			}
			front_bookmark = currentItem;
		}
		if (back_bookmark != -1) {
			removeNow();
		}
	}
	if (newList.size()) {
		add(newList);
	}
	return NoError;
}

void InstanceList::saveNow()
{
	for (auto& item : m_instances) {
		item->saveNow();
	}
}

void InstanceList::add(const QList<InstancePtr>& t)
{
	beginInsertRows(QModelIndex(), m_instances.count(),
					m_instances.count() + t.size() - 1);
	m_instances.append(t);
	for (auto& ptr : t) {
		connect(ptr.get(), &BaseInstance::propertiesChanged, this,
				&InstanceList::propertiesChanged);
		trackLaunchProgress(ptr.get());
	}
	endInsertRows();
}

void InstanceList::trackLaunchProgress(BaseInstance* inst)
{
	/* Parented to the instance, so it is destroyed with it. */
	auto* tracker = new LaunchProgressTracker(inst);
	m_launchTrackers.insert(inst, tracker);
	connect(inst, &QObject::destroyed, this,
			[this, inst]() { m_launchTrackers.remove(inst); });

	connect(inst, &BaseInstance::launchTaskChanged, this,
			[tracker](shared_qobject_ptr<LaunchTask> task) {
				tracker->watch(task.get());
				if (task) {
					connect(task.get(), &LaunchTask::readyForLaunch, tracker,
							&LaunchProgressTracker::clear);
				}
			});
	connect(tracker, &LaunchProgressTracker::changed, this,
			[this, inst]() { emitLaunchProgressChanged(inst); });

	connect(inst, &BaseInstance::runningStatusChanged, this,
			[this, inst](bool) { emitIsRunningChanged(inst); });
}

void InstanceList::emitLaunchProgressChanged(BaseInstance* inst)
{
	int i = getInstIndex(inst);
	if (i != -1) {
		emit dataChanged(index(i), index(i),
						 {LaunchStatusRole, LaunchProgressRole});
	}
}

void InstanceList::emitIsRunningChanged(BaseInstance* inst)
{
	int i = getInstIndex(inst);
	if (i == -1) {
		return;
	}
	QList<int> roles{IsRunningRole};
	// The instance just stopped: the game has by now fetched the version
	// metadata and assets that a "no cover" answer was missing, and may have
	// changed the screenshots. Looked up again with the old answers kept until
	// the new ones arrive.
	if (!inst->isRunning()) {
		const InstanceId id = inst->id();
		/* Bumped even if nothing was cached: a lookup in flight for this id
		 * started before the session ended and may have missed them;
		 * scheduleCoverScan()'s completion handler uses this to discard it. */
		++m_coverGeneration[id];
		QList<InstanceId> again;
		for (auto it = m_coverCache.constBegin();
			 it != m_coverCache.constEnd(); ++it) {
			if (it.key() == id || it->info.url.isEmpty()) {
				again.append(it.key());
			}
		}
		for (const InstanceId& other : again) {
			if (InstancePtr found = getInstanceById(other)) {
				scheduleCoverScan(other, coverFilePath(found.get()),
								  platformCoverPath(found.get()),
								  found->gameVersion());
			}
		}
	}
	emit dataChanged(index(i), index(i), roles);
}

InstanceList::CoverDirs InstanceList::activeCoverDirs() const
{
	if (m_coverDirsSet) {
		return m_coverDirs;
	}
	auto* context = LauncherContext::instance();
	if (!context) {
		return {};
	}
	auto metacache = context->metacache();
	if (!metacache) {
		return {};
	}
	return {metacache->getBasePath(QStringLiteral("meta")),
			metacache->getBasePath(QStringLiteral("asset_indexes")),
			metacache->getBasePath(QStringLiteral("asset_objects"))};
}

void InstanceList::setCoverDirs(const CoverDirs& dirs)
{
	m_coverDirs = dirs;
	m_coverDirsSet = true;
	m_coverCache.clear();
	m_panoramaCache.clear();
}

namespace
{
struct CoverScanResult {
	InstanceList::CoverInfo info;
	QString panorama;
};
} // namespace

void InstanceList::scheduleCoverScan(const InstanceId& id,
									 const QString& customPath,
									 const QString& platformPath,
									 const QString& version)
{
	if (m_coverScansPending.contains(id)) {
		return;
	}
	m_coverScansPending.insert(id);
	const int generation = m_coverGeneration.value(id, 0);
	const CoverDirs dirs = activeCoverDirs();
	const int face = panoramaFaceFor(id);
	const QString panoramaKey = QStringLiteral("%1#%2").arg(version).arg(face);
	const QString knownPanorama = m_panoramaCache.value(panoramaKey);

	auto* watcher = new QFutureWatcher<CoverScanResult>(this);
	connect(watcher, &QFutureWatcher<CoverScanResult>::finished, this,
			[this, id, generation, customPath, platformPath, version, panoramaKey,
			 watcher]() {
				const CoverScanResult result = watcher->result();
				watcher->deleteLater();
				m_coverScansPending.remove(id);

				InstancePtr inst = getInstanceById(id);
				if (!inst) {
					// The instance is gone by now; nothing left to update.
					return;
				}
				if (!result.panorama.isEmpty()) {
					m_panoramaCache.insert(panoramaKey, result.panorama);
				}
				if (m_coverGeneration.value(id, 0) != generation) {
					/* The instance stopped while this lookup was in flight, so
					 * it saw the disk from before; a fresh one replaces it. */
					scheduleCoverScan(id, coverFilePath(inst.get()),
									  platformCoverPath(inst.get()),
									  inst->gameVersion());
					return;
				}

				m_coverCache.insert(
					id, {customPath, platformPath, version, result.info});
				const int row = getInstIndex(inst.get());
				if (row != -1) {
					emit dataChanged(index(row), index(row),
									 {CoverImageRole, CoverKindRole});
				}
				// Shown without a picked cover or fetched art: ask the
				// platform for the pack's own picture (once; see the fetcher).
				if (result.info.kind != QLatin1String("custom") &&
					result.info.kind != QLatin1String("platform")) {
					m_platformCovers->request(inst);
				}
			});
	watcher->setFuture(QtConcurrent::run(
		QThreadPool::globalInstance(),
		[dirs, customPath, platformPath, version, face, knownPanorama]() {
			CoverScanResult result;
			result.panorama = knownPanorama;
			if (result.panorama.isEmpty() ||
				!QFileInfo::exists(result.panorama)) {
				result.panorama = panoramaPath(dirs, version, face);
			}
			result.info = pickCover(customPath, platformPath, result.panorama);
			return result;
		}));
}

int InstanceList::panoramaFaceFor(const InstanceId& id)
{
	// qHash(QString) is seeded per run; a checksum is the same every time.
	return qChecksum(id.toUtf8()) % kPanoramaFaces;
}

QString InstanceList::panoramaPath(const CoverDirs& dirs,
								   const QString& mcVersion, int face)
{
	// The version names a file below meta/; keep it from leaving that folder.
	if (mcVersion.isEmpty() || mcVersion.contains(QLatin1Char('/')) ||
		mcVersion.contains(QLatin1Char('\\')) ||
		mcVersion.contains(QLatin1String(".."))) {
		return QString();
	}
	if (dirs.meta.isEmpty() || dirs.assetIndexes.isEmpty() ||
		dirs.assetObjects.isEmpty()) {
		return QString();
	}

	auto readObject = [](const QString& path) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QJsonObject();
		}
		return QJsonDocument::fromJson(file.readAll()).object();
	};

	const QJsonObject meta = readObject(FS::PathCombine(
		dirs.meta, QStringLiteral("net.minecraft"), mcVersion + ".json"));
	const QString indexId =
		meta.value(QStringLiteral("assetIndex")).toObject().value("id").toString();
	if (indexId.isEmpty() || indexId.contains(QLatin1Char('/')) ||
		indexId.contains(QLatin1Char('\\')) ||
		indexId.contains(QLatin1String(".."))) {
		return QString();
	}

	// The client jar carries a 1x1 placeholder under these names; only the
	// asset objects are the real pictures.
	const QJsonObject index =
		readObject(FS::PathCombine(dirs.assetIndexes, indexId + ".json"));
	const QJsonObject objects = index.value(QStringLiteral("objects")).toObject();
	QString hash;
	for (const int candidate : {face, 0}) {
		hash = objects
				   .value(QStringLiteral(
							  "minecraft/textures/gui/title/background/"
							  "panorama_%1.png")
							  .arg(candidate))
				   .toObject()
				   .value(QStringLiteral("hash"))
				   .toString();
		if (hash.size() >= 2) {
			break;
		}
	}
	if (hash.size() < 2) {
		return QString();
	}

	const QString object =
		FS::PathCombine(dirs.assetObjects, hash.left(2), hash);
	return QFileInfo(object).isFile() ? object : QString();
}

InstanceList::CoverInfo InstanceList::pickCover(const QString& customPath,
												const QString& platformPath,
												const QString& panoramaPath)
{
	if (!customPath.isEmpty() && QFileInfo(customPath).isFile()) {
		return {QUrl::fromLocalFile(customPath).toString(),
				QStringLiteral("custom")};
	}
	if (!platformPath.isEmpty() && QFileInfo(platformPath).isFile()) {
		return {QUrl::fromLocalFile(platformPath).toString(),
				QStringLiteral("platform")};
	}
	if (!panoramaPath.isEmpty()) {
		return {QUrl::fromLocalFile(panoramaPath).toString(),
				QStringLiteral("panorama")};
	}
	return {};
}

namespace
{
// The copy's name carries a random part: a reused name would keep showing the old picture from the image cache.
QString copyCoverFile(const QString& source, const QString& root)
{
	const QString name =
		QStringLiteral("%1-%2.%3")
			.arg(kCustomCoverPrefix,
				 QUuid::createUuid().toString(QUuid::Id128).left(8),
				 QFileInfo(source).suffix().toLower());
	const QString target = FS::PathCombine(root, name);
	const QString part = target + QStringLiteral(".part");
	QFile::remove(part);
	if (!QFile::copy(source, part) || !QFile::rename(part, target)) {
		QFile::remove(part);
		return QString();
	}
	return name;
}

// Only our own copies: an older cover named a screenshot, which is not ours to delete.
void removeCoverCopy(const QString& root, const QString& name)
{
	const QString file = QFileInfo(name).fileName();
	if (file != name || !file.startsWith(kCustomCoverPrefix)) {
		return;
	}
	const QString path = FS::PathCombine(root, file);
	QtConcurrent::run(QThreadPool::globalInstance(),
					  [path]() { QFile::remove(path); });
}

// Checks the file is an image of the screenshots folder, then copies it; the copy's name, or empty. Worker only.
QString copyScreenshotAsCover(const QString& gameRoot, const QString& root,
							  const QString& pathOrFileUrl)
{
	const QUrl url(pathOrFileUrl);
	const QFileInfo info(url.isLocalFile() ? url.toLocalFile() : pathOrFileUrl);
	if (!info.isFile() ||
		!ScreenshotListModel::isImageFile(info.fileName())) {
		return QString();
	}
	const QString shots =
		QFileInfo(FS::PathCombine(gameRoot, "screenshots")).canonicalFilePath();
	if (shots.isEmpty() ||
		QFileInfo(info.absolutePath()).canonicalFilePath() != shots) {
		return QString();
	}
	return copyCoverFile(info.absoluteFilePath(), root);
}
} // namespace

bool InstanceList::setCover(const QString& id, const QString& pathOrFileUrl)
{
	InstancePtr inst = getInstanceById(id);
	if (!inst) {
		return false;
	}
	// A later pick or removal makes this one stale, whatever order they finish in.
	const int pick = ++m_coverPicks[id];
	const QString root = inst->instanceRoot();
	const QString gameRoot = inst->gameRoot();
	const QUrl url(pathOrFileUrl);
	const QString sourceName =
		QFileInfo(url.isLocalFile() ? url.toLocalFile() : pathOrFileUrl)
			.fileName();
	auto* watcher = new QFutureWatcher<QString>(this);
	connect(watcher, &QFutureWatcher<QString>::finished, this,
			[this, id, pick, sourceName, root, watcher]() {
				const QString copy = watcher->result();
				watcher->deleteLater();
				if (copy.isEmpty()) {
					return;
				}
				InstancePtr found = getInstanceById(id);
				if (!found || m_coverPicks.value(id) != pick) {
					removeCoverCopy(root, copy);
					return;
				}
				const QString previous = found->coverImage();
				found->setCustomCover(copy, sourceName);
				removeCoverCopy(root, previous);
			});
	watcher->setFuture(QtConcurrent::run(
		QThreadPool::globalInstance(), [gameRoot, root, pathOrFileUrl]() {
			return copyScreenshotAsCover(gameRoot, root, pathOrFileUrl);
		}));
	return true;
}

void InstanceList::clearCover(const QString& id)
{
	if (InstancePtr inst = getInstanceById(id)) {
		++m_coverPicks[id];
		const QString previous = inst->coverImage();
		inst->setCustomCover(QString(), QString());
		removeCoverCopy(inst->instanceRoot(), previous);
	}
}

void InstanceList::renameCoverSource(const QString& id, const QString& fileName)
{
	InstancePtr inst = getInstanceById(id);
	if (inst && !inst->coverImage().isEmpty()) {
		inst->setCustomCover(inst->coverImage(),
							 QFileInfo(fileName).fileName());
	}
}

QColor InstanceList::averageIconColor(const QImage& source)
{
	const QImage image = source.convertToFormat(QImage::Format_ARGB32);
	double red = 0, green = 0, blue = 0, total = 0;
	for (int y = 0; y < image.height(); ++y) {
		const auto* line =
			reinterpret_cast<const QRgb*>(image.constScanLine(y));
		for (int x = 0; x < image.width(); ++x) {
			const QColor pixel = QColor::fromRgba(line[x]);
			const double weight =
				pixel.alphaF() * (0.25 + qMax(0.0f, pixel.hsvSaturationF()));
			red += pixel.redF() * weight;
			green += pixel.greenF() * weight;
			blue += pixel.blueF() * weight;
			total += weight;
		}
	}
	return total > 0 ? QColor::fromRgbF(red / total, green / total, blue / total)
					 : QColor();
}

QColor InstanceList::iconColor(const QString& key) const
{
	auto it = m_iconColorCache.constFind(key);
	if (it != m_iconColorCache.constEnd()) {
		return it.value();
	}
	const_cast<InstanceList*>(this)->scheduleIconColor(key);
	return QColor();
}

void InstanceList::scheduleIconColor(const QString& key)
{
	if (m_iconColorPending.contains(key)) {
		return;
	}
	m_iconColorPending.insert(key);

	auto* context = LauncherContext::instance();
	std::shared_ptr<IconList> icons = context ? context->icons() : nullptr;
	if (icons && !m_iconUpdatesWatched) {
		m_iconUpdatesWatched = true;
		connect(icons.get(), &IconList::iconUpdated, this,
				&InstanceList::onIconUpdated);
	}

	// Queued, not run inside data(): rasterising the QIcon needs the GUI
	// thread, only the averaging moves to the worker.
	QMetaObject::invokeMethod(
		this,
		[this, key, icons]() {
			const int generation = m_iconColorGeneration.value(key, 0);
			QImage image;
			if (icons) {
				image = icons->getIcon(key)
							.pixmap(QSize(32, 32))
							.toImage();
			}
			auto* watcher = new QFutureWatcher<QColor>(this);
			connect(watcher, &QFutureWatcher<QColor>::finished, this,
					[this, key, generation, watcher]() {
						const QColor color = watcher->result();
						watcher->deleteLater();
						m_iconColorPending.remove(key);
						if (m_iconColorGeneration.value(key, 0) != generation) {
							// The icon changed meanwhile; averaged again,
							// the colour shown until then stays.
							scheduleIconColor(key);
							return;
						}
						m_iconColorCache.insert(key, color);
						emit iconColorsChanged();
					});
			watcher->setFuture(QtConcurrent::run(
				QThreadPool::globalInstance(),
				[image]() { return averageIconColor(image); }));
		},
		Qt::QueuedConnection);
}

void InstanceList::onIconUpdated(const QString& key)
{
	++m_iconColorGeneration[key];
	// Only a colour somebody asked for is kept fresh; the old one stays until
	// the new one arrives.
	if (m_iconColorCache.contains(key)) {
		scheduleIconColor(key);
	}
}

void InstanceList::providerUpdated()
{
	loadList();
}

InstancePtr InstanceList::getInstanceById(QString instId) const
{
	if (instId.isEmpty())
		return InstancePtr();
	for (auto& inst : m_instances) {
		if (inst->id() == instId) {
			return inst;
		}
	}
	return InstancePtr();
}

/* Whether two provider names mean the same catalogue: the launcher records
 * CurseForge as "curseforge", but pack formats and other launchers call it
 * "flame". */
static bool sameProvider(const QString& left, const QString& right)
{
	auto canonical = [](const QString& provider) {
		const QString lower = provider.toLower();
		if (lower == QLatin1String("flame")) {
			return QStringLiteral("curseforge");
		}
		return lower;
	};
	return canonical(left) == canonical(right);
}

InstancePtr InstanceList::getInstanceByManagedPack(const QString& provider,
												   const QString& packId) const
{
	/* Both are required: a pack id alone means different things on each
	 * catalogue, and a provider alone would match every pack installed from
	 * it. */
	if (provider.isEmpty() || packId.isEmpty()) {
		return InstancePtr();
	}

	for (auto& inst : m_instances) {
		if (inst->managedPackId() != packId) {
			continue;
		}
		if (sameProvider(inst->managedPackProvider(), provider)) {
			return inst;
		}
	}
	return InstancePtr();
}

QModelIndex InstanceList::getInstanceIndexById(const QString& id) const
{
	return index(getInstIndex(getInstanceById(id).get()));
}

int InstanceList::getInstIndex(BaseInstance* inst) const
{
	int count = m_instances.count();
	for (int i = 0; i < count; i++) {
		if (inst == m_instances[i].get()) {
			return i;
		}
	}
	return -1;
}

void InstanceList::propertiesChanged(BaseInstance* inst)
{
	int i = getInstIndex(inst);
	if (i != -1) {
		emit dataChanged(index(i), index(i));
	}
}

InstancePtr InstanceList::loadInstance(const InstanceId& id)
{
	if (!m_groupsLoaded) {
		loadGroupList();
	}

	auto instanceRoot = FS::PathCombine(rootDirOf(id), id);
	auto instanceSettings = std::make_shared<INISettingsObject>(
		FS::PathCombine(instanceRoot, "instance.cfg"));
	InstancePtr inst;

	instanceSettings->registerSetting("InstanceType", "Legacy");

	QString inst_type = instanceSettings->get("InstanceType").toString();

	if (inst_type == "OneSix" || inst_type == "Nostalgia") {
		inst.reset(new MinecraftInstance(m_globalSettings, instanceSettings,
										 instanceRoot));
	} else if (inst_type == "Legacy") {
		inst.reset(new LegacyInstance(m_globalSettings, instanceSettings,
									  instanceRoot));
	} else {
		inst.reset(
			new NullInstance(m_globalSettings, instanceSettings, instanceRoot));
	}
	qDebug() << "Loaded instance " << inst->name() << " from "
			 << inst->instanceRoot();
	return inst;
}

void InstanceList::saveGroupList()
{
	qDebug() << "Will save group list now.";
	if (!m_instancesProbed) {
		qDebug() << "Group saving prevented because we don't know the full "
					"list of instances yet.";
		return;
	}
	/* The group file lives in the data folder, not in an instance folder:
	 * grouping is not per-folder (a secondary-folder instance can share a
	 * group with a primary one), and a file in the primary folder would be
	 * lost when the user repoints it. No WatchLock: the path is outside the
	 * watched instance folders, so writing it cannot trigger a rescan. */
	QString groupFileName = QDir::current().filePath("instgroups.json");
	QMap<QString, QSet<QString>> reverseGroupMap;
	for (auto iter = m_instanceGroupIndex.begin();
		 iter != m_instanceGroupIndex.end(); iter++) {
		QString id = iter.key();
		QString group = iter.value();
		if (group.isEmpty())
			continue;
		if (!instanceSet.contains(id)) {
			qDebug() << "Skipping saving missing instance" << id
					 << "to groups list.";
			continue;
		}

		if (!reverseGroupMap.count(group)) {
			QSet<QString> set;
			set.insert(id);
			reverseGroupMap[group] = set;
		} else {
			QSet<QString>& set = reverseGroupMap[group];
			set.insert(id);
		}
	}
	QJsonObject toplevel;
	toplevel.insert("formatVersion", QJsonValue(QString("1")));
	QJsonObject groupsArr;
	for (auto iter = reverseGroupMap.begin(); iter != reverseGroupMap.end();
		 iter++) {
		auto list = iter.value();
		auto name = iter.key();
		QJsonObject groupObj;
		QJsonArray instanceArr;
		groupObj.insert("hidden", QJsonValue(m_collapsedGroups.contains(name)));
		for (auto item : list) {
			instanceArr.append(QJsonValue(item));
		}
		groupObj.insert("instances", instanceArr);
		groupsArr.insert(name, groupObj);
	}
	toplevel.insert("groups", groupsArr);
	QJsonDocument doc(toplevel);
	try {
		FS::write(groupFileName, doc.toJson());
		qDebug() << "Group list saved.";
	} catch (const FS::FileSystemException& e) {
		qCritical() << "Failed to write instance group file :" << e.cause();
	}
}

void InstanceList::loadGroupList()
{
	qDebug() << "Will load group list now.";

	QString groupFileName = QDir::current().filePath("instgroups.json");

	/* Start from nothing: this runs again when the folder settings change, and
	 * without clearing, groups of instances that no longer exist would
	 * accumulate. The file is the whole truth about grouping. */
	m_instanceGroupIndex.clear();
	m_groupNameCache.clear();
	m_collapsedGroups.clear();

	bool migratingLegacyGroups = false;

	/* No file in the data folder? Look where it used to live, the primary
	 * instance folder. Reading it once and writing the result to the new place
	 * at the end of this function keeps an upgrade from looking like every
	 * group was deleted. The old file is left alone as the only copy if
	 * anything goes wrong. */
	if (!QFileInfo::exists(groupFileName)) {
		QString legacyGroupFileName =
			FS::PathCombine(primaryDir(), "instgroups.json");
		if (!QFileInfo::exists(legacyGroupFileName)) {
			/* No file at all is a loaded, empty list. Left unmarked, every
			 * instance loaded later would run this function again, and its
			 * clear() above would throw away any group set since (such as the
			 * one a copy was just committed into). */
			m_groupsLoaded = true;
			return;
		}
		qInfo() << "Migrating instance groups from legacy location"
				<< legacyGroupFileName;
		groupFileName = legacyGroupFileName;
		migratingLegacyGroups = true;
	}

	QByteArray jsonData;
	try {
		jsonData = FS::read(groupFileName);
	} catch (const FS::FileSystemException& e) {
		qCritical() << "Failed to read instance group file :" << e.cause();
		return;
	}

	QJsonParseError error;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &error);

	if (error.error != QJsonParseError::NoError) {
		qCritical()
			<< QString("Failed to parse instance group file: %1 at offset %2")
				   .arg(error.errorString(), QString::number(error.offset))
				   .toUtf8();
		return;
	}

	if (!jsonDoc.isObject()) {
		qWarning() << "Invalid group file. Root entry should be an object.";
		return;
	}

	QJsonObject rootObj = jsonDoc.object();

	if (rootObj.value("formatVersion").toVariant().toInt() !=
		GROUP_FILE_FORMAT_VERSION)
		return;

	if (!rootObj.value("groups").isObject()) {
		qWarning() << "Invalid group list JSON: 'groups' should be an object.";
		return;
	}

	QSet<QString> groupSet;
	m_instanceGroupIndex.clear();

	QJsonObject groupMapping = rootObj.value("groups").toObject();
	for (QJsonObject::iterator iter = groupMapping.begin();
		 iter != groupMapping.end(); iter++) {
		QString groupName = iter.key();

		if (!iter.value().isObject()) {
			qWarning()
				<< QString("Group '%1' in the group list should be an object.")
					   .arg(groupName)
					   .toUtf8();
			continue;
		}

		QJsonObject groupObj = iter.value().toObject();
		if (!groupObj.value("instances").isArray()) {
			qWarning() << QString("Group '%1' in the group list is invalid. It "
								  "should contain an array called 'instances'.")
							  .arg(groupName)
							  .toUtf8();
			continue;
		}

		groupSet.insert(groupName);

		auto hidden = groupObj.value("hidden").toBool(false);
		if (hidden) {
			m_collapsedGroups.insert(groupName);
		}

		QJsonArray instancesArray = groupObj.value("instances").toArray();

		for (QJsonArray::iterator iter2 = instancesArray.begin();
			 iter2 != instancesArray.end(); iter2++) {
			m_instanceGroupIndex[(*iter2).toString()] = groupName;
		}
	}
	m_groupsLoaded = true;
	m_groupNameCache.unite(groupSet);
	qDebug() << "Group list loaded.";

	/* Having read the old file, write the new one now so the migration
	 * completes on the launch that noticed it; waiting for the next save could
	 * leave the groups only where nothing reads them after a crash.
	 * m_groupsLoaded is already true, so this cannot recurse. */
	if (migratingLegacyGroups) {
		saveGroupList();
	}
}

void InstanceList::instanceDirContentsChanged(const QString& path)
{
	Q_UNUSED(path);
	emit instancesChanged();
}

void InstanceList::on_InstFolderChanged(const Setting& setting, QVariant value)
{
	(void)setting;
	(void)value;
	/* Both folder settings are re-read instead of applying the changed one:
	 * this slot is connected to "InstanceDir" and "AdditionalInstanceDirs",
	 * and the roots are an ordered set built from the pair (the primary
	 * decides where new instances and the group file go). */
	QStringList dirs;
	dirs << m_globalSettings->get("InstanceDir").toString();
	dirs << decodeInstanceDirList(
		m_globalSettings->get("AdditionalInstanceDirs"));

	const QStringList resolved = resolveInstanceDirs(dirs);
	if (resolved == m_instDirs) {
		return;
	}

	/* Flush the groups before the switch; they are reloaded from the file
	 * afterwards. */
	if (m_groupsLoaded) {
		saveGroupList();
	}

	applyInstanceDirs(resolved);
	m_groupsLoaded = false;
	emit instancesChanged();
}

void InstanceList::on_GroupStateChanged(const QString& group, bool collapsed)
{
	qDebug() << "Group" << group << (collapsed ? "collapsed" : "expanded");
	if (collapsed) {
		m_collapsedGroups.insert(group);
	} else {
		m_collapsedGroups.remove(group);
	}
	saveGroupList();
}

class InstanceStaging : public Task
{
	Q_OBJECT
	const unsigned minBackoff = 1;
	const unsigned maxBackoff = 16;

  public:
	InstanceStaging(InstanceList* parent, InstanceTask* child,
					const QString& stagingPath, const QString& groupName)
		: backoff(minBackoff, maxBackoff)
	{
		m_parent = parent;
		m_instanceTask = child;
		m_child.reset(child);
		connect(child, &Task::succeeded, this, &InstanceStaging::childSucceded);
		connect(child, &Task::failed, this, &InstanceStaging::childFailed);
		connect(child, &Task::status, this, &InstanceStaging::setStatus);
		connect(child, &Task::details, this, &InstanceStaging::setDetails);
		connect(child, &Task::progress, this, &InstanceStaging::setProgress);
		propagateStepsFrom(child);
		m_groupName = groupName;
		m_stagingPath = stagingPath;
		m_backoffTimer.setSingleShot(true);
		connect(&m_backoffTimer, &QTimer::timeout, this,
				&InstanceStaging::childSucceded);
	}

	virtual ~InstanceStaging() {};

	// FIXME/TODO: add ability to abort during instance commit retries
	bool abort() override
	{
		if (m_child && m_child->canAbort()) {
			return m_child->abort();
		}
		return false;
	}
	bool canAbort() const override
	{
		if (m_child && m_child->canAbort()) {
			return true;
		}
		return false;
	}

  protected:
	virtual void executeTask() override
	{
		m_child->start();
	}
	QStringList warnings() const override
	{
		/* What the task ran into while building the instance plus what went
		 * wrong putting it in place; the second only exists here, since
		 * committing happens after the task has finished. */
		return m_child->warnings() + m_commitWarnings;
	}
	bool isMultiStep() const override
	{
		return m_child && m_child->isMultiStep();
	}
	TaskStepProgressList getStepProgress() const override
	{
		if (!m_child) {
			return {};
		}
		return m_child->getStepProgress();
	}

  private slots:
	void childSucceded()
	{
		unsigned sleepTime = backoff();

		/* Asked now, not remembered from construction: a task may not know
		 * when wrapped whether it replaces an instance (that can depend on
		 * what it finds once running), and its name can change while it runs. */
		const QString instanceName = m_instanceTask->name();
		const QString overrideInstanceId = m_instanceTask->overrideInstanceId();
		const QStringList filesToRemove =
			m_instanceTask->filesToRemoveAfterCommit();

		/* Cleared first: this slot runs again on every backoff retry, and a
		 * warning from a retried attempt should not be reported twice. */
		m_commitWarnings.clear();

		if (m_parent->commitStagedInstance(m_stagingPath, instanceName,
										   m_groupName, overrideInstanceId,
										   filesToRemove, &m_commitWarnings)) {
			emitSucceeded();
			return;
		}
		if (sleepTime == maxBackoff) {
			emitFailed(tr("Failed to commit instance, even after multiple "
						  "retries. It is being blocked by something."));
			return;
		}
		qDebug() << "Failed to commit instance" << instanceName
				 << "Initiating backoff:" << sleepTime;
		m_backoffTimer.start(sleepTime * 500);
	}
	void childFailed(const QString& reason)
	{
		m_parent->destroyStagingPath(m_stagingPath);
		emitFailed(reason);
	}

  private:
	/* Exponential backoff because of Windows antivirus: it interferes while
	 * instances are extracted or created, and NTFS locks files that are open. */
	ExponentialSeries backoff;
	QString m_stagingPath;
	InstanceList* m_parent;
	unique_qobject_ptr<Task> m_child;
	QString m_groupName;
	/* Same object as m_child at its real type, for the instance-level
	 * questions Task does not answer (name, whether it replaces an instance).
	 * Ownership stays with m_child. */
	InstanceTask* m_instanceTask = nullptr;
	QTimer m_backoffTimer;
	QStringList m_commitWarnings;
};

Task* InstanceList::wrapInstanceTask(InstanceTask* task)
{
	QString targetDir = task->targetDir();
	if (targetDir.isEmpty() && task->shouldOverride()) {
		/* An update is staged in the folder its instance lives in, not the
		 * primary one: committing an override merges the staged tree over the
		 * instance, and staging elsewhere would make each pack update in a
		 * secondary folder a cross-filesystem copy. Worked out here so the
		 * caller need not know which disk it is on. */
		targetDir = rootDirOf(task->overrideInstanceId());
	}
	auto stagingPath = getStagedInstancePath(targetDir);
	task->setStagingPath(stagingPath);
	task->setParentSettings(m_globalSettings);
	/* The group is settled before wrapping; the name and the override target
	 * are read at commit time, because the task may still decide them while
	 * running. */
	return new InstanceStaging(this, task, stagingPath, task->group());
}

QString InstanceList::getStagedInstancePath(const QString& targetDir)
{
	QString root = primaryDir();
	if (!targetDir.isEmpty()) {
		/* Refuse rather than fall back to the primary folder: an unconfigured
		 * target means a caller bug or a folder removed from the settings
		 * while this task was queued, and installing elsewhere silently would
		 * surface much later. */
		if (!m_instDirs.contains(targetDir)) {
			qCritical() << "Requested instance folder" << targetDir
						<< "is not one of the configured folders";
			return QString();
		}
		if (!QDir(targetDir).exists()) {
			qCritical() << "Requested instance folder" << targetDir
						<< "is configured but not accessible on disk";
			return QString();
		}
		root = targetDir;
	}
	if (root.isEmpty()) {
		qCritical() << "No usable instance folder to stage into";
		return QString();
	}

	QString key = QUuid::createUuid().toString();
	QString relPath = FS::PathCombine("_KITE_TEMP/", key);
	QDir rootPath(root);
	auto path = FS::PathCombine(root, relPath);
	if (!rootPath.mkpath(relPath)) {
		return QString();
	}
	return path;
}

bool InstanceList::commitStagedInstance(const QString& path,
										const QString& instanceName,
										const QString& groupName,
										const QString& overrideInstanceId,
										const QStringList& filesToRemove,
										QStringList* removalWarnings)
{
	const bool isOverride = !overrideInstanceId.isEmpty();

	/* Which folder this instance lands in: for an override, wherever it
	 * already is; for a new instance, the folder the staging directory is
	 * inside, read off the path. A commit is a rename, which stays one only
	 * while both ends are on one filesystem. */
	const QString root = isOverride ? rootDirOf(overrideInstanceId)
									: rootForStaging(path);
	if (root.isEmpty()) {
		qWarning() << "Cannot commit" << path
				   << "- no configured instance folder contains it";
		return false;
	}

	QDir dir;
	QString instID = isOverride ? overrideInstanceId
								: FS::DirNameFromString(instanceName, root);
	{
		WatchLock lock(m_watcher, root);
		QString destination = FS::PathCombine(root, instID);

		if (isOverride) {
			/* Merge over the existing instance, so anything the pack does not
			 * ship (worlds, screenshots, the user's config edits) stays. */
			if (!FS::overrideFolder(destination, path)) {
				qWarning() << "Failed to merge" << path << "over"
						   << destination;
				return false;
			}

			/* No group or id bookkeeping: the instance has both and an
			 * override does not change them. Its name lives in the staged
			 * instance.cfg we just moved into place. */

			/* Re-read instance.cfg into the instance we hold: loadList() keeps
			 * the existing object for a known id, so the pack page would show
			 * the replaced version, and INISettingsObject's whole-file rewrite
			 * on the first unrelated set() (even a launch) would silently undo
			 * part of the update. */
			if (auto existing = getInstanceById(instID)) {
				if (!existing->settings()->reload()) {
					qWarning() << "Could not re-read settings for" << instID
							   << "after update";
				}
			}

			/* Now that the new version is in place, remove what it no longer
			 * ships (not earlier: a failed update would be left missing mods).
			 * A failed delete is not fatal but is told to the user, since the
			 * game would still load that mod. */
			for (const QString& path : filesToRemove) {
				if (!QFileInfo::exists(path)) {
					continue;
				}
				if (!FS::deletePath(path)) {
					qWarning() << "Could not remove obsolete file" << path;
					if (removalWarnings) {
						removalWarnings->append(
							tr("Could not remove a file the updated pack no "
							   "longer includes: %1")
								.arg(QDir::toNativeSeparators(path)));
					}
				} else {
					qDebug() << "Removed obsolete file" << path;
				}
			}
		} else {
			if (!dir.rename(path, destination)) {
				qWarning() << "Failed to move" << path << "to" << destination;
				return false;
			}
			m_instanceGroupIndex[instID] = groupName;
			m_groupNameCache.insert(groupName);
		}

		instanceSet.insert(instID);
		/* Record the folder now, not at the next discovery pass: loading the
		 * instance for selection asks rootDirOf() for its path and would get
		 * the primary folder. */
		m_instanceRootDirMap[instID] = root;
		emit instancesChanged();
		emit instanceSelectRequest(instID);
	}
	saveGroupList();
	return true;
}

bool InstanceList::destroyStagingPath(const QString& keyPath)
{
	return FS::deletePath(keyPath);
}

#include "InstanceList.moc"
