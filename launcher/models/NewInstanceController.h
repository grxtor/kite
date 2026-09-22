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

#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

#include <memory>

/*
 * Base for the two Meta version-list proxies below: both need the same
 * loading/error tracking and the same "newest first" fallback ordering, and
 * differ only in which rows they keep. QtCore only, same rule as the other
 * QML-facing models here (InstanceFilterModel, SettingsAdapter, ...): no
 * QtWidgets, no ui/.
 *
 * Roles are looked up by NAME from the source model's roleNames() rather
 * than assumed to be BaseVersionList's own enum values, the same reasoning
 * InstanceFilterModel.h gives for doing that with InstanceList: this then
 * works against a fake version list numbering its roles differently (see
 * NewInstanceController_test.cpp), not only against the real one.
 */
class VersionListLoadingProxy : public QSortFilterProxyModel
{
	Q_OBJECT

	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/* The versionId of the first row after filtering and sorting - "the
	 * newest build that fits the current filter". Used to default a
	 * selection without trusting the source list's own idea of
	 * "recommended", which knows nothing about this proxy's filter (see
	 * NewInstanceController::onLoaderListFirstVersionIdChanged()). Empty
	 * when nothing currently matches. */
	Q_PROPERTY(QString firstVersionId READ firstVersionId NOTIFY
				   firstVersionIdChanged)

  public:
	explicit VersionListLoadingProxy(QObject* parent = nullptr);

	bool loading() const
	{
		return m_loading;
	}
	QString error() const
	{
		return m_error;
	}
	int count() const
	{
		return rowCount();
	}
	QString firstVersionId() const;

	void setSourceModel(QAbstractItemModel* sourceModel) override;

  signals:
	void loadingChanged();
	void errorChanged();
	void countChanged();
	void firstVersionIdChanged();

  protected:
	/* Hook for a subclass to resolve whatever extra role it filters rows
	 * on, called after the roles this base class needs are resolved and
	 * before the base class model is set. @p sourceModel may be null. */
	virtual void resolveRoles(QAbstractItemModel* sourceModel)
	{
		Q_UNUSED(sourceModel);
	}

	int versionIdRole() const
	{
		return m_versionIdRole;
	}

	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

  private:
	void setLoading(bool loading);
	void setError(const QString& error);
	/// Recomputes count/firstVersionId, emitting only what actually moved.
	void refreshDerived();
	/* Starts the source list's load task if it has one and it is not
	 * loaded already - the same thing
	 * VersionSelectWidget::loadList() does for the widget's own version
	 * pickers (ui/widgets/VersionSelectWidget.cpp), via the same
	 * BaseVersionList::getLoadTask() / Meta::BaseEntity::load() API. */
	void startLoadIfNeeded();

	int m_versionIdRole = -1;
	int m_sortRole = -1;
	bool m_loading = false;
	QString m_error;
	QString m_firstVersionId;
};

/*
 * Meta::VersionList for "net.minecraft" (BaseVersionList; see
 * meta/VersionList.h), filtered to release builds by default and sorted
 * newest first - the same default VanillaPage's checkboxes start on
 * (ui/pages/modplatform/VanillaPage.cpp), minus its separate "experiments"
 * toggle, which nothing here exposes; an experiment build is always hidden.
 */
class MinecraftVersionListProxy : public VersionListLoadingProxy
{
	Q_OBJECT

	Q_PROPERTY(bool showSnapshots READ showSnapshots WRITE setShowSnapshots
				   NOTIFY showSnapshotsChanged)
	/// Alpha, beta and old-snapshot builds - VanillaPage's other three
	/// checkboxes, collapsed into one: nothing here needs to tell them apart.
	Q_PROPERTY(bool showOldVersions READ showOldVersions WRITE
				   setShowOldVersions NOTIFY showOldVersionsChanged)

  public:
	explicit MinecraftVersionListProxy(QObject* parent = nullptr);

	bool showSnapshots() const
	{
		return m_showSnapshots;
	}
	void setShowSnapshots(bool show);

	bool showOldVersions() const
	{
		return m_showOldVersions;
	}
	void setShowOldVersions(bool show);

  signals:
	void showSnapshotsChanged();
	void showOldVersionsChanged();

  protected:
	void resolveRoles(QAbstractItemModel* sourceModel) override;
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;

  private:
	int m_typeRole = -1;
	bool m_showSnapshots = false;
	bool m_showOldVersions = false;
};

/*
 * One loader's Meta::VersionList - whichever
 * NewInstanceController::loader currently names - filtered to the builds
 * that apply to one Minecraft version: "exact if present", the same rule
 * LoaderVersionPage applies via VersionSelectWidget::setExactIfPresentFilter()
 * (ui/dialogs/InstallLoaderDialog.cpp). A build that names no Minecraft
 * version (Fabric Loader, Quilt Loader) is always kept; one that does must
 * match minecraftVersion exactly.
 */
class LoaderVersionListProxy : public VersionListLoadingProxy
{
	Q_OBJECT

  public:
	explicit LoaderVersionListProxy(QObject* parent = nullptr);

	QString minecraftVersion() const
	{
		return m_minecraftVersion;
	}
	/// Re-filters in place; does not reload, since it is the loader (not the
	/// Minecraft version) that decides which list is the source model.
	void setMinecraftVersion(const QString& version);

  protected:
	void resolveRoles(QAbstractItemModel* sourceModel) override;
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;

  private:
	int m_parentVersionRole = -1;
	QString m_minecraftVersion;
};

/* The Minecraft version (+ loader brand, if one is given) the way
 * VanillaPage::suggestCurrent() suggests a name for the widget's vanilla-only
 * flow (ui/pages/modplatform/VanillaPage.cpp), extended with the loader
 * since this flow can add one at creation. Empty if @p minecraftVersion is.
 *
 * Free-standing so it can be unit-tested without a LauncherContext: it
 * touches nothing but the loader table in minecraft/Component.h. @p loader
 * is "", "fabric", "quilt", "forge" or "neoforge" - see
 * NewInstanceController::loader(). */
QString composeSuggestedInstanceName(const QString& minecraftVersion,
									 const QString& loader);

/*
 * QML-facing "New instance" flow: picks a Minecraft version and, optionally,
 * a mod loader and its version, and builds the same task the widget's
 * NewInstanceDialog + VanillaPage build for a vanilla instance
 * (ui/dialogs/NewInstanceDialog.cpp, ui/pages/modplatform/VanillaPage.cpp)
 * plus the loader component if one was picked - see create().
 */
class NewInstanceController : public QObject
{
	Q_OBJECT

	/// "net.minecraft"'s version list, release-only/newest-first by default.
	Q_PROPERTY(QObject* minecraftVersions READ minecraftVersions CONSTANT)

	/// "", "fabric", "quilt", "forge" or "neoforge".
	Q_PROPERTY(
		QString loader READ loader WRITE setLoader NOTIFY loaderChanged)
	/// The chosen loader's version list, filtered to selectedMinecraftVersion.
	Q_PROPERTY(QObject* loaderVersions READ loaderVersions CONSTANT)
	/// Convenience mirror of loaderVersions.loading, for QML that does not
	/// need the rest of the loader version list's own state.
	Q_PROPERTY(bool loaderLoading READ loaderLoading NOTIFY
				   loaderLoadingChanged)

	Q_PROPERTY(QString selectedMinecraftVersion READ selectedMinecraftVersion
				   NOTIFY selectedMinecraftVersionChanged)
	/// Defaults to the newest build that fits once loaderVersions settles -
	/// see onLoaderListFirstVersionIdChanged() - until selectLoaderVersion()
	/// is called explicitly.
	Q_PROPERTY(QString selectedLoaderVersion READ selectedLoaderVersion NOTIFY
				   selectedLoaderVersionChanged)

	/// Existing instance groups, sorted and deduplicated the way
	/// NewInstanceDialog's constructor prepares its own combo box; "" first,
	/// for "no group" (ui/dialogs/NewInstanceDialog.cpp).
	Q_PROPERTY(QStringList groups READ groups CONSTANT)

  public:
	explicit NewInstanceController(QObject* parent = nullptr);

	QObject* minecraftVersions() const;

	QString loader() const
	{
		return m_loader;
	}
	void setLoader(const QString& loader);

	QObject* loaderVersions() const;
	bool loaderLoading() const;

	QString selectedMinecraftVersion() const
	{
		return m_selectedMinecraftVersion;
	}
	QString selectedLoaderVersion() const
	{
		return m_selectedLoaderVersion;
	}

	QStringList groups() const;

	Q_INVOKABLE void selectMinecraftVersion(const QString& version);
	Q_INVOKABLE void selectLoaderVersion(const QString& version);
	/// See composeSuggestedInstanceName() above.
	Q_INVOKABLE QString suggestedName() const;
	/* Builds and starts the instance-creation task, wrapped the same way
	 * MainWindow::createInstanceFromDialog() wraps NewInstanceDialog's
	 * (ui/MainWindow.cpp), and returns a TaskWatcher for it (parented to
	 * this controller, so QML gets it as CppOwnership without needing its
	 * own expose() call - see QmlShell::expose()). Null if no Minecraft
	 * version is selected yet, or if selectedMinecraftVersion names no
	 * version the metadata index actually has. */
	Q_INVOKABLE QObject* create(const QString& name, const QString& group,
								const QString& iconKey);

  signals:
	void loaderChanged();
	void loaderLoadingChanged();
	void selectedMinecraftVersionChanged();
	void selectedLoaderVersionChanged();

  private:
	/// Points loaderVersions at m_loader's list (or clears it) and re-applies
	/// the current selectedMinecraftVersion filter.
	void refreshLoaderSource();
	/// Defaults selectedLoaderVersion once loaderVersions settles, unless
	/// selectLoaderVersion() was already called for this loader/MC version.
	void onLoaderListFirstVersionIdChanged();

	std::unique_ptr<MinecraftVersionListProxy> m_minecraftVersions;
	std::unique_ptr<LoaderVersionListProxy> m_loaderVersions;

	QString m_loader;
	QString m_selectedMinecraftVersion;
	QString m_selectedLoaderVersion;
};
