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
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

#include "modplatform/DependencyResolver.h"
#include "modplatform/ModDownloadTypes.h"

class ModFolderModel;

/*
 * An install plan shown before anything is downloaded. Two shapes:
 *   - One picked project: its row comes first and is always installed; start()
 *     resolves its required dependencies into rows under it.
 *   - Missing dependencies (missingDependencies()): the resolver runs over what
 *     is installed and only what is missing is listed; there is no picked row.
 *     The mod health check's "Install" is this shape too
 *     (setWantedProjects(), setReplacements()).
 * Dependency rows can be unticked; one already on disk at another version
 * starts unticked. Dependencies no provider had a compatible version of are in
 * `unresolved`. install() downloads the ticked rows (conflicts settled by
 * ModInstallConflictAnalyzer: a picked version replaces the installed one) and
 * returns a TaskWatcher parented to the review's parent, so the download
 * outlives the review. install() and cancel() both end it (deleteLater()).
 */
class ContentInstallReview : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(QString title READ title CONSTANT)
	/// 1 when the first row is a picked project, 0 for a missing-dependency review.
	Q_PROPERTY(int primaryCount READ primaryCount CONSTANT)
	Q_PROPERTY(bool resolving READ resolving NOTIFY stateChanged)
	Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
	/// False means install() can run unasked.
	Q_PROPERTY(bool needsReview READ needsReview NOTIFY stateChanged)
	Q_PROPERTY(int dependencyCount READ dependencyCount NOTIFY stateChanged)
	Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
	Q_PROPERTY(int checkedDependencyCount READ checkedDependencyCount NOTIFY
				   checkedChanged)
	Q_PROPERTY(QStringList unresolved READ unresolved NOTIFY stateChanged)
	Q_PROPERTY(int count READ count NOTIFY stateChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		FileNameRole,
		ProviderRole,
		/// ", "-joined; empty for the picked project.
		RequiredByRole,
		IsDependencyRole,
		CheckedRole,
		MaybeInstalledRole,
		ReplacesRole,
		/// The author only allows downloads from their own page.
		BrowserOnlyRole,
		VersionTypeRole
	};

	/// @p loader: empty unless the content type uses one. Nothing runs until start().
	ContentInstallReview(const ModPlatform::SelectedMod& mod,
						 const QString& mcVersion, const QString& loader,
						 std::shared_ptr<ModFolderModel> folder,
						 bool resolveDependencies, QObject* parent = nullptr);
	/// What @p installed (the folder's recorded files) need and lack. Nothing runs until start().
	static ContentInstallReview*
	missingDependencies(const QList<ModPlatform::SelectedMod>& installed,
						const QString& title, const QString& mcVersion,
						const QString& loader,
						std::shared_ptr<ModFolderModel> folder,
						QObject* parent = nullptr);
	~ContentInstallReview() override;

	QString title() const
	{
		return m_title;
	}
	int primaryCount() const
	{
		return m_primaryCount;
	}
	bool resolving() const
	{
		return m_resolving;
	}
	bool ready() const
	{
		return m_ready;
	}
	bool needsReview() const;
	int dependencyCount() const;
	int checkedCount() const;
	int checkedDependencyCount() const;
	QStringList unresolved() const;
	/// The wanted ids (setWantedProjects()) Modrinth answered with nothing for
	/// this Minecraft version and loader; a lookup that failed is not among
	/// them. Set once the resolution is over.
	QStringList notAvailable() const
	{
		return m_notAvailable;
	}
	int count() const
	{
		return int(m_rows.size());
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// Mods looked up by id as if the installed ones required them. Before
	/// start(); a review with only these still resolves.
	void setWantedProjects(const QList<DependencyResolver::WantedProject>& wanted);
	/// For the dependency answering wanted id K: the file @p fileByWantedId has
	/// under K, which it replaces. Such a row starts ticked even though a version
	/// is on disk. Before start() or applyResolution().
	void setReplacements(const QHash<QString, QString>& fileByWantedId);
	/// Resolves dependencies, or settles at once when there are none.
	void start();
	/// Lists @p dependencies under the picked project and marks the review ready;
	/// public so tests need no network.
	void applyResolution(const QList<ModPlatform::DependencyInfo>& dependencies,
						 const QList<ModPlatform::UnresolvedDep>& unresolved);
	QList<ModPlatform::DownloadItem> downloadItems() const;

	/// No effect on the picked project's own row.
	Q_INVOKABLE void setChecked(int row, bool checked);
	Q_INVOKABLE void setAllDependenciesChecked(bool checked);
	/// Starts the download and ends this review; null (and the review
	/// stays open) until `ready`, or when nothing is ticked.
	Q_INVOKABLE QObject* install();
	Q_INVOKABLE void cancel();

	static QString providerName(const QString& platform);

  signals:
	void stateChanged();
	void checkedChanged();

  private:
	struct Row {
		ModPlatform::DownloadItem item;
		QString provider;
		QStringList requiredBy;
		QString versionType;
		bool checked = true;
		bool maybeInstalled = false;
	};

	ContentInstallReview(const QList<ModPlatform::SelectedMod>& resolveFor,
						 const QString& title, const QString& mcVersion,
						 const QString& loader,
						 std::shared_ptr<ModFolderModel> folder,
						 QObject* parent);
	void onResolved();

	QList<ModPlatform::SelectedMod> m_resolveFor;
	QString m_title;
	int m_primaryCount = 0;
	QString m_mcVersion;
	QString m_loader;
	std::shared_ptr<ModFolderModel> m_folder;
	bool m_resolveDependencies = false;
	QList<DependencyResolver::WantedProject> m_wanted;
	QHash<QString, QString> m_replacements;

	QList<Row> m_rows;
	QList<ModPlatform::UnresolvedDep> m_unresolved;
	QStringList m_notAvailable;
	bool m_resolving = false;
	bool m_ready = false;
	bool m_finished = false;
	QPointer<DependencyResolver> m_resolver;
};
