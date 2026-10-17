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

#include "models/InstanceExportController.h"

#include <QAbstractFileIconProvider>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QIcon>
#include <QPixmap>
#include <QUrl>

#include "DesktopServices.h"
#include "FileIgnoreProxy.h"
#include "FileSystem.h"
#include "archive/ExportToZipTask.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "modplatform/flame/FlamePackExportTask.h"
#include "modplatform/modrinth/ModrinthPackExportTask.h"
#include "settings/SettingsObject.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* CurseForge warns about packs asking for more than this, so the
	 * figure suggested stops here however much the instance itself is
	 * allowed to use. The same cap as ExportPackDialog's. */
	constexpr int MAX_SUGGESTED_MEMORY_MIB = 1024 * 12;

	/* No icons at all. The platform's own lookup is a call per file, and
	 * this model is pointed at an entire instance (see
	 * ui/FastFileIconProvider); the QML tree draws its folder and file
	 * glyphs from the "isDir" role instead. */
	class NoFileIcons : public QAbstractFileIconProvider
	{
	  public:
		QIcon icon(IconType) const override
		{
			return QIcon();
		}
		QIcon icon(const QFileInfo&) const override
		{
			return QIcon();
		}
	};
} // namespace

/* FileIgnoreProxy the way a QML TreeView wants it: the name column only,
 * and the check state and the folder flag as named roles. */
class ExportFileTree : public FileIgnoreProxy
{
  public:
	enum Roles { IsDirRole = Qt::UserRole + 1 };

	explicit ExportFileTree(const QString& root) : FileIgnoreProxy(root, nullptr)
	{
	}

	QHash<int, QByteArray> roleNames() const override
	{
		auto roles = FileIgnoreProxy::roleNames();
		roles.insert(Qt::CheckStateRole, "checkState");
		roles.insert(IsDirRole, "isDir");
		return roles;
	}

	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override
	{
		if (role != IsDirRole) {
			return FileIgnoreProxy::data(index, role);
		}
		auto* files = qobject_cast<QFileSystemModel*>(sourceModel());
		if (!files || !index.isValid()) {
			return QVariant();
		}
		return files->isDir(mapToSource(index));
	}

	/* After the ignore lists changed. */
	void refilter()
	{
		invalidateRowsFilter();
	}

  protected:
	bool filterAcceptsColumn(int column, const QModelIndex&) const override
	{
		return column == 0;
	}
};

InstanceExportController::InstanceExportController(InstancePtr instance,
												   QObject* parent)
	: QObject(parent), m_instance(std::move(instance)),
	  m_minecraft(dynamic_cast<MinecraftInstance*>(m_instance.get())),
	  m_icons(std::make_unique<NoFileIcons>()),
	  m_fileSystem(std::make_unique<QFileSystemModel>()),
	  m_tree(std::make_unique<ExportFileTree>(m_instance->instanceRoot()))
{
	m_fileSystem->setIconProvider(m_icons.get());

	/* Rooted at the instance folder in every format, even though a pack's
	 * tree starts at the game folder, so the paths it records line up
	 * with the zip export's: all three share one `.packignore`, and a
	 * file unchecked in one should stay unchecked in the others. */
	m_tree->setSourceModel(m_fileSystem.get());
	applyIgnoreLists();
	m_tree->loadBlockedPathsFromFile(ignoreFileName());
	m_tree->sort(0, Qt::AscendingOrder);

	m_fileSystem->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot |
							QDir::AllDirs | QDir::Hidden);
	m_fileSystem->setRootPath(m_instance->instanceRoot());

	connect(m_instance.get(), &BaseInstance::runningStatusChanged, this,
			&InstanceExportController::runningChanged);

	loadPackFields();
}

InstanceExportController::~InstanceExportController()
{
	/* A worker thread may still be inside a running export. Stop it, and
	 * keep the task alive until it has wound down rather than pulling it
	 * out from under that thread; its filter is a snapshot (see
	 * snapshotFilter()), so it needs nothing else of ours. */
	if (m_task && m_task->isRunning()) {
		Task::Ptr task = m_task;
		connect(task.get(), &Task::finished, task.get(),
				[task]() mutable { task.reset(); });
		task->abort();
	}
}

QString InstanceExportController::instanceId() const
{
	return m_instance->id();
}

QString InstanceExportController::instanceName() const
{
	return m_instance->name();
}

bool InstanceExportController::packFormats() const
{
	return m_minecraft != nullptr;
}

bool InstanceExportController::running() const
{
	return m_instance->isRunning();
}

QObject* InstanceExportController::files() const
{
	return m_tree.get();
}

QModelIndex InstanceExportController::filesRoot() const
{
	const QString root = m_format == Format::Zip ? m_instance->instanceRoot()
												 : m_instance->gameRoot();
	QModelIndex index = m_fileSystem->index(root);
	/* A game folder that is not there yet (an instance never started)
	 * shows the instance folder instead: an invalid root would make the
	 * tree start at the top of the disk. */
	if (!index.isValid()) {
		index = m_fileSystem->index(m_instance->instanceRoot());
	}
	return m_tree->mapFromSource(index);
}

QString InstanceExportController::format() const
{
	switch (m_format) {
		case Format::ModrinthPack:
			return QStringLiteral("modrinth");
		case Format::CurseForgePack:
			return QStringLiteral("curseforge");
		case Format::Zip:
			break;
	}
	return QStringLiteral("zip");
}

void InstanceExportController::setFormat(const QString& format)
{
	Format next;
	if (format == QLatin1String("zip")) {
		next = Format::Zip;
	} else if (format == QLatin1String("modrinth")) {
		next = Format::ModrinthPack;
	} else if (format == QLatin1String("curseforge")) {
		next = Format::CurseForgePack;
	} else {
		return;
	}
	if ((next != Format::Zip && !m_minecraft) || next == m_format) {
		return;
	}

	const bool rootMoves = (next == Format::Zip) != (m_format == Format::Zip);
	m_format = next;
	applyIgnoreLists();
	emit choicesChanged();
	if (rootMoves) {
		emit filesRootChanged();
	}
}

void InstanceExportController::setPackName(const QString& name)
{
	if (m_packName == name) {
		return;
	}
	m_packName = name;
	emit choicesChanged();
}

void InstanceExportController::setPackVersion(const QString& version)
{
	if (m_packVersion == version) {
		return;
	}
	m_packVersion = version;
	emit choicesChanged();
}

void InstanceExportController::setPackAuthor(const QString& author)
{
	if (m_packAuthor == author) {
		return;
	}
	m_packAuthor = author;
	emit choicesChanged();
}

void InstanceExportController::setPackSummary(const QString& summary)
{
	if (m_packSummary == summary) {
		return;
	}
	m_packSummary = summary;
	emit choicesChanged();
}

void InstanceExportController::setIncludeOptional(bool include)
{
	if (m_includeOptional == include) {
		return;
	}
	m_includeOptional = include;
	emit choicesChanged();
}

void InstanceExportController::setRecommendMemory(bool recommend)
{
	if (m_recommendMemory == recommend) {
		return;
	}
	m_recommendMemory = recommend;
	emit choicesChanged();
}

void InstanceExportController::setRecommendedMemory(int mib)
{
	if (m_recommendedMemory == mib) {
		return;
	}
	m_recommendedMemory = mib;
	emit choicesChanged();
}

bool InstanceExportController::ready() const
{
	/* Only Modrinth insists: `versionId` is required in an mrpack index,
	 * where a CurseForge manifest is content with an empty version. */
	return m_format != Format::ModrinthPack || !m_packVersion.trimmed().isEmpty();
}

QString InstanceExportController::fileExtension() const
{
	return m_format == Format::ModrinthPack ? QStringLiteral(".mrpack")
											: QStringLiteral(".zip");
}

QString InstanceExportController::suggestedFileUrl() const
{
	const QString name = m_format != Format::Zip && !m_packName.isEmpty()
							 ? m_packName
							 : m_instance->name();
	return QUrl::fromLocalFile(
			   FS::PathCombine(QDir::homePath(),
							   FS::RemoveInvalidFilenameChars(name) +
								   fileExtension()))
		.toString();
}

QObject* InstanceExportController::exportTo(const QString& urlOrPath)
{
	if (m_task && m_task->isRunning()) {
		setError(tr("This instance is already being exported."));
		return nullptr;
	}
	if (!ready()) {
		setError(tr("A Modrinth pack needs a version."));
		return nullptr;
	}

	const QUrl url(urlOrPath);
	QString output = url.isLocalFile() ? url.toLocalFile() : urlOrPath;
	if (output.isEmpty()) {
		setError(tr("Choose where to save the export."));
		return nullptr;
	}
	/* `.zip` is left alone on a Modrinth pack: an mrpack is a zip, and a
	 * user who typed that extension meant it. Anything else gains the
	 * real one, so the file is not one the catalogue refuses to read. */
	const bool named =
		output.endsWith(fileExtension(), Qt::CaseInsensitive) ||
		(m_format == Format::ModrinthPack &&
		 output.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive));
	if (!named) {
		output.append(fileExtension());
	}

	setError(QString());
	saveChoices();

	Task* task = buildTask(output);
	if (!task) {
		return nullptr;
	}
	m_task = Task::Ptr(task);

	auto* watcher = new TaskWatcher(m_task, this);
	watcher->setTitle(tr("Export"));
	m_watcher = watcher;
	connect(watcher, &TaskWatcher::finished, this, [this, output](bool ok) {
		if (!ok) {
			return;
		}
		m_lastOutput = output;
		emit lastOutputChanged();
	});
	task->start();
	return watcher;
}

void InstanceExportController::cancel()
{
	if (m_task && m_task->isRunning()) {
		m_task->abort();
	}
}

void InstanceExportController::saveChoices()
{
	m_tree->saveBlockedPathsToFile(ignoreFileName());
	if (!m_minecraft) {
		return;
	}

	auto settings = m_minecraft->settings();
	settings->set("ExportName", m_packName);
	settings->set("ExportVersion", m_packVersion);
	settings->set("ExportOptionalFiles", m_includeOptional);
	settings->set("ExportSummary", m_packSummary);
	settings->set("ExportAuthor", m_packAuthor);
	if (m_recommendMemory) {
		settings->set("ExportRecommendedRAM", m_recommendedMemory);
	} else {
		/* Reset rather than set to zero: the setting's own default is "do
		 * not state a requirement", and storing a zero on top of it would
		 * keep whatever figure the field happened to show. */
		settings->reset("ExportRecommendedRAM");
	}
}

void InstanceExportController::showInFolder() const
{
	if (m_lastOutput.isEmpty()) {
		return;
	}
	DesktopServices::openDirectory(QFileInfo(m_lastOutput).absolutePath());
}

void InstanceExportController::applyIgnoreLists()
{
	const QDir instanceDir(m_instance->instanceRoot());
	const QString prefix = instanceDir.relativeFilePath(m_instance->gameRoot());

	/* Things that are not part of an instance so much as debris it
	 * produces. Hidden rather than merely unchecked: an export carrying
	 * somebody's crash reports and caches is bigger and no more useful,
	 * and there is no version of "yes, please include my logs" worth
	 * offering a checkbox for. */
	auto& paths = m_tree->ignoreFilesWithPath();
	paths.clear();
	for (const char* path :
		 {"logs", "crash-reports", ".cache", ".fabric", ".quilt"}) {
		paths.insert(FS::PathCombine(prefix, QString::fromLatin1(path)));
	}
	/* The instance's own backups are zip files already; shipping them
	 * inside another zip doubles its size for nothing. They live beside
	 * the game folder, so the path is the instance root's, not @c prefix. */
	paths.insert(QStringLiteral(".backups"));
	m_tree->ignoreFilesWithName() = {QStringLiteral(".DS_Store"),
									 QStringLiteral("thumbs.db"),
									 QStringLiteral("Thumbs.db")};
	m_tree->ignoreFilesWithSuffix().clear();

	if (m_format != Format::Zip && m_minecraft) {
		/* The sidecars that record where each managed file came from.
		 * They are this launcher's bookkeeping, and in a pack they would
		 * be wrong: the manifest is what tells an installer where the
		 * files come from. Spelled out rather than asked of
		 * ModMetadataIndex, whose accessor creates the directory it
		 * names. */
		m_tree->ignoreFilesWithSuffix().append(QStringLiteral(".pw.toml"));
		for (const QString& folder :
			 {m_minecraft->modsRoot(), m_minecraft->coreModsDir(),
			  m_minecraft->resourcePacksDir(), m_minecraft->texturePacksDir(),
			  m_minecraft->shaderPacksDir()}) {
			if (folder.isEmpty()) {
				continue;
			}
			paths.insert(instanceDir.relativeFilePath(
				FS::PathCombine(folder, QStringLiteral(".index"))));
		}
	}
	m_tree->refilter();
}

MMCZip::FilterFileFunction InstanceExportController::snapshotFilter() const
{
	/* The tree stays live while the export runs, and a pack export asks
	 * its filter from a worker thread. A copy answers the same question
	 * from data nobody else touches. */
	auto copy = std::make_shared<FileIgnoreProxy>(m_instance->instanceRoot(),
												  nullptr);
	copy->blockedPaths() = m_tree->blockedPaths();
	copy->ignoreFilesWithName() = m_tree->ignoreFilesWithName();
	copy->ignoreFilesWithSuffix() = m_tree->ignoreFilesWithSuffix();
	copy->ignoreFilesWithPath() = m_tree->ignoreFilesWithPath();
	return [copy](const QFileInfo& file) { return copy->filterFile(file); };
}

Task* InstanceExportController::buildTask(const QString& output)
{
	const MMCZip::FilterFileFunction filter = snapshotFilter();
	const QString packName =
		m_packName.isEmpty() ? m_instance->name() : m_packName;

	switch (m_format) {
		case Format::Zip: {
			saveInstanceIcon();
			QFileInfoList files;
			if (!MMCZip::collectFileListRecursively(
					m_instance->instanceRoot(), QString(), &files, filter)) {
				setError(tr("The instance's files could not be read."));
				return nullptr;
			}
			return new MMCZip::ExportToZipTask(
				output, m_instance->instanceRoot(), files, QString(), true);
		}
		case Format::ModrinthPack:
			return new ModrinthPackExportTask(packName, m_packVersion,
											  m_packSummary, m_includeOptional,
											  m_minecraft, output, filter);
		case Format::CurseForgePack: {
			FlamePackExportOptions options;
			options.name = packName;
			options.version = m_packVersion;
			options.author = m_packAuthor;
			options.optionalFiles = m_includeOptional;
			options.instance = m_minecraft;
			options.output = output;
			options.filter = filter;
			options.recommendedRAM =
				m_recommendMemory ? m_recommendedMemory : 0;
			return new FlamePackExportTask(std::move(options));
		}
	}
	return nullptr;
}

void InstanceExportController::saveInstanceIcon() const
{
	/* Without a launcher (a unit test) there is no icon list to ask. */
	if (!LAUNCHER) {
		return;
	}
	const QString key = m_instance->iconKey();
	const MMCIcon* icon = LAUNCHER->icons()->icon(key);
	/* A built-in icon needs nothing: the importing launcher has it too. */
	if (!icon || icon->isBuiltIn()) {
		return;
	}

	const QString path = icon->getFilePath();
	if (!path.isNull()) {
		FS::copy(path, FS::PathCombine(m_instance->instanceRoot(),
									   QFileInfo(path).fileName()))();
		return;
	}

	const QIcon& image = icon->m_images[icon->type()].icon;
	const QList<QSize> sizes = image.availableSizes();
	if (sizes.isEmpty()) {
		return;
	}
	QSize largest = sizes.first();
	for (const QSize& size : sizes) {
		if (size.width() * size.height() >
			largest.width() * largest.height()) {
			largest = size;
		}
	}
	image.pixmap(largest).save(
		FS::PathCombine(m_instance->instanceRoot(), key + ".png"));
}

void InstanceExportController::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

void InstanceExportController::loadPackFields()
{
	if (!m_minecraft) {
		return;
	}
	auto settings = m_minecraft->settings();
	m_packName = settings->get("ExportName").toString();
	m_packVersion = settings->get("ExportVersion").toString();
	m_packAuthor = settings->get("ExportAuthor").toString();
	m_packSummary = settings->get("ExportSummary").toString();
	m_includeOptional = settings->get("ExportOptionalFiles").toBool();

	const int recommended = settings->get("ExportRecommendedRAM").toInt();
	m_recommendMemory = recommended > 0;
	/* Off, but with a figure already in the field: the instance's own
	 * allocation is the only informed guess there is, and the user should
	 * see what they would be publishing before agreeing to publish it. */
	m_recommendedMemory =
		recommended > 0
			? recommended
			: qMin(settings->get("MaxMemAlloc").toInt(), MAX_SUGGESTED_MEMORY_MIB);
}

QString InstanceExportController::ignoreFileName() const
{
	return FS::PathCombine(m_instance->instanceRoot(),
						   QStringLiteral(".packignore"));
}
