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
 * The install plan for one project picked in ContentBrowserView, shown
 * before anything is downloaded: the QML-facing replacement for
 * DownloadSummaryDialog.
 *
 * start() resolves the project's required dependencies (DependencyResolver,
 * mods only) and lists them under the project itself, one row each. The
 * project's own row is always installed; every dependency row can be
 * unticked, and one whose project is already on disk at another version
 * starts unticked - DownloadSummaryDialog's own default. Dependencies no
 * provider had a compatible version of are listed in `unresolved`.
 *
 * install() downloads the ticked rows the way ContentBrowser::install()
 * always has (conflicts settled by ModInstallConflictAnalyzer, then one
 * ContentDownloadTask) and hands back a TaskWatcher parented to this
 * review's own parent, the ContentBrowser, so the download outlives the
 * review. install() and cancel() both end the review (deleteLater()).
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class ContentInstallReview : public QAbstractListModel
{
	Q_OBJECT

	/// The picked project's name.
	Q_PROPERTY(QString title READ title CONSTANT)
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
		/// project itself.
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

	/// @p loader: empty unless the content type uses one. Nothing runs
	/// until start().
	ContentInstallReview(const ModPlatform::SelectedMod& mod,
						 const QString& mcVersion, const QString& loader,
						 std::shared_ptr<ModFolderModel> folder,
						 bool resolveDependencies, QObject* parent = nullptr);
	~ContentInstallReview() override;

	QString title() const
	{
		return m_mod.name;
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
	QStringList unresolved() const;
	int count() const
	{
		return int(m_rows.size());
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// Resolves dependencies, or settles at once when there are none to
	/// look for. Called by ContentBrowser::review().
	void start();
	/// Lists @p dependencies under the project and marks the review
	/// ready - what the resolver's answer goes through. Public so the
	/// review can be tested without the network.
	void applyResolution(const QList<ModPlatform::DependencyInfo>& dependencies,
						 const QList<ModPlatform::UnresolvedDep>& unresolved);
	/// The ticked rows, as download items.
	QList<ModPlatform::DownloadItem> downloadItems() const;

	/// No effect on the project's own row.
	Q_INVOKABLE void setChecked(int row, bool checked);
	Q_INVOKABLE void setAllDependenciesChecked(bool checked);
	/// Starts the download and ends this review; null (and the review
	/// stays open) until `ready`.
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

	void onResolved();

	ModPlatform::SelectedMod m_mod;
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
