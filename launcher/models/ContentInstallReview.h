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
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

#include "modplatform/ModDownloadTypes.h"

class ModFolderModel;
class DependencyResolver;

/*
 * An install plan, shown before anything is downloaded: the QML-facing
 * replacement for DownloadSummaryDialog.
 *
 * Two shapes:
 *   - One picked project (Add content, or "Change version…" in the
 *     Content tab): its own row comes first and is always installed - it
 *     is what the install is for - and start() resolves its required
 *     dependencies (DependencyResolver, mods only) into rows under it.
 *   - Missing dependencies (missingDependencies(), "Verify dependencies"):
 *     the resolver is run over what is already installed, and only what
 *     it finds missing is listed. There is no picked row.
 *
 * Every dependency row can be unticked; one whose project is already on
 * disk at another version starts unticked - DownloadSummaryDialog's own
 * default. Dependencies no provider had a compatible version of are listed
 * in `unresolved`.
 *
 * install() downloads the ticked rows the way the widget page did
 * (conflicts settled by ModInstallConflictAnalyzer - a picked version of
 * something already installed replaces it in place - then one
 * ContentDownloadTask) and hands back a TaskWatcher parented to this
 * review's own parent, so the download outlives the review. install() and
 * cancel() both end the review (deleteLater()).
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class ContentInstallReview : public QAbstractListModel
{
	Q_OBJECT

	/// The picked project's name, or a heading for a missing-dependency
	/// review.
	Q_PROPERTY(QString title READ title CONSTANT)
	/// 1 when the first row is a picked project (always installed), 0 for
	/// a missing-dependency review.
	Q_PROPERTY(int primaryCount READ primaryCount CONSTANT)
	/// Still asking the providers for dependencies.
	Q_PROPERTY(bool resolving READ resolving NOTIFY stateChanged)
	/// Resolution is over (or was never needed); install() may be called.
	Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
	/// Whether there is anything to review at all: a dependency, or one
	/// that could not be found. False means install() can run unasked.
	Q_PROPERTY(bool needsReview READ needsReview NOTIFY stateChanged)
	Q_PROPERTY(int dependencyCount READ dependencyCount NOTIFY stateChanged)
	/// Rows that will be downloaded.
	Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
	/// Dependency rows that will be downloaded.
	Q_PROPERTY(int checkedDependencyCount READ checkedDependencyCount NOTIFY
				   checkedChanged)
	/// Names of required dependencies nothing compatible was found for.
	Q_PROPERTY(QStringList unresolved READ unresolved NOTIFY stateChanged)
	Q_PROPERTY(int count READ count NOTIFY stateChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		FileNameRole,
		/// "Modrinth" / "CurseForge".
		ProviderRole,
		/// What asked for this dependency, ", "-joined; empty for the
		/// picked project.
		RequiredByRole,
		IsDependencyRole,
		CheckedRole,
		/// A version of this is already installed.
		MaybeInstalledRole,
		/// The author only allows downloads from their own page.
		BrowserOnlyRole,
		/// "release" / "beta" / "alpha" / "".
		VersionTypeRole
	};

	/// One picked project. @p loader: empty unless the content type uses
	/// one. Nothing runs until start().
	ContentInstallReview(const ModPlatform::SelectedMod& mod,
						 const QString& mcVersion, const QString& loader,
						 std::shared_ptr<ModFolderModel> folder,
						 bool resolveDependencies, QObject* parent = nullptr);
	/// What @p installed (the folder's own recorded files) need and do
	/// not have. Nothing runs until start().
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
	int count() const
	{
		return int(m_rows.size());
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// Resolves dependencies, or settles at once when there are none to
	/// look for. Called by whoever created the review.
	void start();
	/// Lists @p dependencies under the picked project (if any) and marks
	/// the review ready - what the resolver's answer goes through. Public
	/// so the review can be tested without the network.
	void applyResolution(const QList<ModPlatform::DependencyInfo>& dependencies,
						 const QList<ModPlatform::UnresolvedDep>& unresolved);
	/// The ticked rows, as download items.
	QList<ModPlatform::DownloadItem> downloadItems() const;

	/// No effect on the picked project's own row.
	Q_INVOKABLE void setChecked(int row, bool checked);
	Q_INVOKABLE void setAllDependenciesChecked(bool checked);
	/// Starts the download and ends this review; null (and the review
	/// stays open) until `ready`, or when nothing is ticked.
	Q_INVOKABLE QObject* install();
	/// Stops resolving and ends this review.
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

	/// The missing-dependency shape; see missingDependencies().
	ContentInstallReview(const QList<ModPlatform::SelectedMod>& resolveFor,
						 const QString& title, const QString& mcVersion,
						 const QString& loader,
						 std::shared_ptr<ModFolderModel> folder,
						 QObject* parent);
	void onResolved();

	/// What the resolver is asked about.
	QList<ModPlatform::SelectedMod> m_resolveFor;
	QString m_title;
	int m_primaryCount = 0;
	QString m_mcVersion;
	QString m_loader;
	std::shared_ptr<ModFolderModel> m_folder;
	bool m_resolveDependencies = false;

	QList<Row> m_rows;
	QList<ModPlatform::UnresolvedDep> m_unresolved;
	bool m_resolving = false;
	bool m_ready = false;
	bool m_finished = false;
	/// Owned via QObject parentage (parent is `this`).
	QPointer<DependencyResolver> m_resolver;
};
