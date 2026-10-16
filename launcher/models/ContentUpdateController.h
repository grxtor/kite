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
#include "modplatform/ModDownloadTypes.h"
#include "modplatform/ModUpdateCheckTask.h"

class MinecraftInstance;
class ModFolderModel;
class InstanceContentController;
class ContentProviderModel;

/*
 * The updates a check found, as a list to tick: the QML-facing
 * replacement for ModUpdateDialog's tree. Every row starts ticked.
 */
class ContentUpdateList : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		/// "Modrinth" / "CurseForge".
		ProviderRole,
		/// What is installed now and what it becomes, as a person reads
		/// them; a file name when nothing better was recorded.
		CurrentVersionRole,
		NewVersionRole,
		/// The file being replaced.
		FileNameRole,
		/// Where to read what changed, or "".
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
	/// The ticked rows' downloads.
	QList<ModPlatform::DownloadItem> checkedItems() const;

	Q_INVOKABLE void setChecked(int row, bool checked);
	Q_INVOKABLE void setAllChecked(bool checked);

	/// The page that says what is new in the version an update offers:
	/// the version's own page on Modrinth, the project's page on
	/// CurseForge (which has no stable per-file address to link).
	static QString changelogUrl(const ModUpdateCheckTask::UpdateInfo& update);
	/// @p number when there is one, else @p fileName.
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
 * Keeping installed content current, per instance: the rest of what
 * ModFolderPage did with a file's recorded origin (ModMetadataIndex) -
 *   - "Check for updates": ModUpdateCheckTask over a folder's index, the
 *     results in `updates` to tick, applyUpdates() to download them
 *     (ContentDownloadTask replaces each file in place);
 *   - "Change version…": the project's versions from its provider,
 *     listed exactly as Add content lists them, and reviewVersion() to
 *     install one through a ContentInstallReview (dependencies first;
 *     the conflict analyzer turns it into a replacement);
 *   - "Verify dependencies": DependencyResolver over the installed mods,
 *     what is missing as a ContentInstallReview to tick and install;
 *   - "Reset metadata": forget where files came from.
 * Every download comes back as a TaskWatcher parented to this controller,
 * and the folder is re-read once it ends, success or not.
 *
 * Created by InstanceContentController and parented to it, which is where
 * the folders come from. Kinds as there; coremods and texturepacks have no
 * provider to ask, so supportsUpdates() is false for them.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class ContentUpdateController : public QObject
{
	Q_OBJECT

	/// ContentUpdateList of the last check.
	Q_PROPERTY(QObject* updates READ updates CONSTANT)
	/// The kind the last check was for.
	Q_PROPERTY(QString updatesKind READ updatesKind NOTIFY checkingChanged)
	Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
	/// Why the last call refused to start or found nothing to do;
	/// cleared by the next call that starts.
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
	QString updatesKind() const
	{
		return m_updatesKind;
	}
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

	/// Whether files of @p kind can come from a provider at all.
	Q_INVOKABLE bool supportsUpdates(const QString& kind) const;
	/// Checks the files of @p kind with @p keys (all of them when empty)
	/// for newer compatible versions. A TaskWatcher, or null with
	/// lastError set.
	Q_INVOKABLE QObject* checkForUpdates(const QString& kind,
										 const QStringList& keys);
	/// Gives up on a running check; what was found so far is kept.
	Q_INVOKABLE void cancelCheck();
	/// Downloads the ticked updates. A TaskWatcher, or null with
	/// lastError set.
	Q_INVOKABLE QObject* applyUpdates();

	/// Starts listing the versions of the file with @p key; false (with
	/// lastError) when it has no recorded origin to ask about.
	Q_INVOKABLE bool loadVersions(const QString& kind, const QString& key);
	/// A ContentInstallReview (parented to this controller, already
	/// started) for version @p versionId of the project loadVersions()
	/// listed, or null.
	Q_INVOKABLE QObject* reviewVersion(const QString& versionId);

	/// What the installed mods need and do not have, as a
	/// ContentInstallReview (started), or null with lastError set.
	Q_INVOKABLE QObject* verifyDependencies();

	/// Forgets where the files with @p keys came from (their sidecars);
	/// the files stay. Returns how many records went.
	Q_INVOKABLE int resetMetadata(const QString& kind, const QStringList& keys);

	/// "mods" -> ContentType::Mod, ...; false for kinds no provider has.
	static bool contentTypeFor(const QString& kind,
							   ModPlatform::ContentType& type);
	/// Only the updates for files with @p keys (all when empty).
	static QList<ModUpdateCheckTask::UpdateInfo>
	onlyFor(const QList<ModUpdateCheckTask::UpdateInfo>& updates,
			const QStringList& keys);
	/// The recorded files the dependency resolver can ask about - those
	/// with a platform origin and a version id or hash - the selection
	/// ModFolderPage::verifyDependencies() builds.
	static QList<ModPlatform::SelectedMod>
	resolvableEntries(const QList<ModMetadataIndex::Entry>& entries,
					  const QString& mcVersion);
	/// Gives each update with no recorded current version the version its
	/// installed file declares itself ("0.14.3"), so the review reads
	/// "0.14.3 → 0.15.3" rather than starting from a file name.
	static void fillCurrentVersions(QList<ModUpdateCheckTask::UpdateInfo>& updates,
									const ModFolderModel& folder);
	/// Removes the sidecars of the files with @p keys from @p folder's
	/// index. Static so it can be tested on a folder alone.
	static int forgetOrigins(ModFolderModel& folder, const QStringList& keys);

  signals:
	void checkingChanged();
	void lastErrorChanged();
	void versionsChanged();

  private:
	void setLastError(const QString& error);
	QString mcVersion() const;
	QString loader() const;
	/// A TaskWatcher over @p task, parented here, that re-reads @p folder
	/// when it ends.
	QObject* watchDownload(Task* task, const QString& title,
						   std::shared_ptr<ModFolderModel> folder);
	void onVersionsSearchState();
	void onVersionsEntryUpdated(int row);

  private:
	MinecraftInstance* m_instance = nullptr;
	InstanceContentController* m_lists = nullptr;

	ContentUpdateList* m_updates = nullptr;
	QString m_updatesKind;
	std::shared_ptr<ModFolderModel> m_updatesFolder;
	QPointer<ModUpdateCheckTask> m_check;
	QString m_lastError;

	/// The single-project lookup behind "Change version…"; rebuilt for
	/// each loadVersions(), owned via parentage.
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
