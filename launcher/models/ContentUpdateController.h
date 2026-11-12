/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <memory>

#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentType.h"
#include "modplatform/DependencyResolver.h"
#include "modplatform/ModDownloadTypes.h"
#include "modplatform/ModUpdateCheckTask.h"

class MinecraftInstance;
class ModFolderModel;
class InstanceContentController;
class ContentProviderModel;

/* The updates a check found, as a list to tick. Every row starts ticked. */
class ContentUpdateList : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		ProviderRole,
		CurrentVersionRole,
		NewVersionRole,
		FileNameRole,
		ChangelogRole,
		CheckedRole
	};

	explicit ContentUpdateList(QObject* parent = nullptr);

	int count() const
	{
		return int(m_rows.size());
	}
	int checkedCount() const;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	void setUpdates(const QList<ModUpdateCheckTask::UpdateInfo>& updates);
	void clear();
	QList<ModPlatform::DownloadItem> checkedItems() const;

	Q_INVOKABLE void setChecked(int row, bool checked);
	Q_INVOKABLE void setAllChecked(bool checked);

	/// Version page on Modrinth, project page on CurseForge (no stable per-file address).
	static QString changelogUrl(const ModUpdateCheckTask::UpdateInfo& update);
	static QString readableVersion(const QString& number,
								   const QString& fileName);

  signals:
	void countChanged();
	void checkedChanged();

  private:
	struct Row {
		ModUpdateCheckTask::UpdateInfo update;
		bool checked = true;
	};
	QList<Row> m_rows;
};

/*
 * Per-instance content updates: check for updates, change a file's version,
 * verify dependencies, install what the health check found missing, reset
 * metadata. Every download comes back as a TaskWatcher parented to this
 * controller; the folder is re-read once it ends. Created by and parented to
 * InstanceContentController. supportsUpdates() is false for coremods and
 * texturepacks (no provider).
 */
class ContentUpdateController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QObject* updates READ updates CONSTANT)
	Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
	/// Why the last call refused to start or found nothing; cleared by the next call that starts.
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

	/// "Change version…": whose versions are listed, the list (the shape
	/// ContentBrowser::versions has), and which one is installed now.
	Q_PROPERTY(QString versionsTitle READ versionsTitle NOTIFY versionsChanged)
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	Q_PROPERTY(bool versionsLoading READ versionsLoading NOTIFY versionsChanged)
	Q_PROPERTY(QString installedVersionId READ installedVersionId NOTIFY
				   versionsChanged)
	Q_PROPERTY(QString versionsError READ versionsError NOTIFY versionsChanged)

  public:
	ContentUpdateController(MinecraftInstance* instance,
							InstanceContentController* lists,
							QObject* parent = nullptr);
	~ContentUpdateController() override;

	QObject* updates() const;
	bool checking() const
	{
		return !m_check.isNull();
	}
	QString lastError() const
	{
		return m_lastError;
	}
	QString versionsTitle() const
	{
		return m_versionsTitle;
	}
	QVariantList versions() const
	{
		return m_versions;
	}
	bool versionsLoading() const
	{
		return m_versionsLoading;
	}
	QString installedVersionId() const
	{
		return m_installedVersionId;
	}
	QString versionsError() const
	{
		return m_versionsError;
	}

	Q_INVOKABLE bool supportsUpdates(const QString& kind) const;
	/// Checks files of @p kind with @p keys (all when empty). TaskWatcher, or null with lastError set.
	Q_INVOKABLE QObject* checkForUpdates(const QString& kind,
										 const QStringList& keys);
	Q_INVOKABLE void cancelCheck();
	/// TaskWatcher, or null with lastError set.
	Q_INVOKABLE QObject* applyUpdates();

	/// False (with lastError) when the file has no recorded origin.
	Q_INVOKABLE bool loadVersions(const QString& kind, const QString& key);
	/// ContentInstallReview (started, parented here) for @p versionId, or null.
	Q_INVOKABLE QObject* reviewVersion(const QString& versionId);

	/// ContentInstallReview (started) for missing dependencies, or null with lastError set.
	Q_INVOKABLE QObject* verifyDependencies();
	/// Installs the mods with @p ids for the files with @p keys that need them,
	/// replacing the file with key @p replaceKeys[i] for ids[i] when not empty.
	/// ContentInstallReview (started), or null with lastError set.
	Q_INVOKABLE QObject* installMissing(const QStringList& keys,
										const QStringList& ids,
										const QStringList& replaceKeys);

	/// Installs recommended Modrinth @p projectIds not already installed, with their
	/// dependencies. ContentInstallReview (started), or null.
	Q_INVOKABLE QObject* installBundle(const QStringList& projectIds);
	Q_INVOKABLE QStringList installedProjects() const;
	/// Whether a mod named OptiFine is installed; Sodium does not run with it.
	Q_INVOKABLE bool hasOptiFine() const;

	/// Forgets where the files with @p keys came from; the files stay. Returns how many records went.
	Q_INVOKABLE int resetMetadata(const QString& kind, const QStringList& keys);

	/// "mods" -> ContentType::Mod, ...; false for kinds no provider has.
	static bool contentTypeFor(const QString& kind,
							   ModPlatform::ContentType& type);
	static QList<ModUpdateCheckTask::UpdateInfo>
	onlyFor(const QList<ModUpdateCheckTask::UpdateInfo>& updates,
			const QStringList& keys);
	/// Recorded files with a platform origin and a version id or hash.
	static QList<ModPlatform::SelectedMod>
	resolvableEntries(const QList<ModMetadataIndex::Entry>& entries,
					  const QString& mcVersion);
	struct MissingPlan {
		QList<ModPlatform::SelectedMod> selected;
		QList<DependencyResolver::WantedProject> wanted;
		QHash<QString, QString> replacements;
	};
	/// A key no file has is ignored; a file being replaced is not asked about its dependencies.
	static MissingPlan planMissing(const ModFolderModel& folder,
								   const QString& mcVersion,
								   const QStringList& keys,
								   const QStringList& ids,
								   const QStringList& replaceKeys);
	/// Fills a missing current version from the version the installed file declares.
	static void fillCurrentVersions(QList<ModUpdateCheckTask::UpdateInfo>& updates,
									const ModFolderModel& folder);
	static int forgetOrigins(ModFolderModel& folder, const QStringList& keys);

  signals:
	void checkingChanged();
	void lastErrorChanged();
	void versionsChanged();

  private:
	void setLastError(const QString& error);
	QString mcVersion() const;
	QString loader() const;
	/// Re-reads @p folder when the task ends.
	QObject* watchDownload(Task* task, const QString& title,
						   std::shared_ptr<ModFolderModel> folder);
	void onVersionsSearchState();
	void onVersionsEntryUpdated(int row);

  private:
	MinecraftInstance* m_instance = nullptr;
	InstanceContentController* m_lists = nullptr;

	ContentUpdateList* m_updates = nullptr;
	QString m_updatesKind;
	/// The updates listed are those of every file of the kind, not a selection.
	bool m_updatesWhole = false;
	std::shared_ptr<ModFolderModel> m_updatesFolder;
	QPointer<ModUpdateCheckTask> m_check;
	QString m_lastError;

	/// Single-project lookup behind "Change version…"; rebuilt per loadVersions().
	QPointer<ContentProviderModel> m_versionsModel;
	QString m_versionsTitle;
	QVariantList m_versions;
	bool m_versionsLoading = false;
	QString m_installedVersionId;
	QString m_versionsError;
	QString m_versionsKind;
	QString m_versionsPlatform;
	std::shared_ptr<ModFolderModel> m_versionsFolder;
};
