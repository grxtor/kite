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

#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

#include <memory>

/*
 * Base for the two Meta version-list proxies: shared loading/error tracking
 * and "newest first" fallback ordering. Roles are looked up by name from the
 * source model, so a list that numbers its roles differently works too.
 */
class VersionListLoadingProxy : public QSortFilterProxyModel
{
	Q_OBJECT

	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/* versionId of the first row after filtering and sorting (the newest build
	 * that fits the filter). Empty when nothing matches. */
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
	/* Resolves extra roles the subclass filters on; called before the base model
	 * is set. @p sourceModel may be null. */
	virtual void resolveRoles(QAbstractItemModel* sourceModel)
	{
		Q_UNUSED(sourceModel);
	}

	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

  private:
	void setLoading(bool loading);
	void setError(const QString& error);
	/// Recomputes count/firstVersionId, emitting only what actually moved.
	void refreshDerived();
	void startLoadIfNeeded();

	int m_versionIdRole = -1;
	int m_sortRole = -1;
	bool m_loading = false;
	QString m_error;
	QString m_firstVersionId;
};

/* Meta version list for "net.minecraft": release builds by default, newest
 * first. Experiments are always hidden. */
class MinecraftVersionListProxy : public VersionListLoadingProxy
{
	Q_OBJECT

	Q_PROPERTY(bool showSnapshots READ showSnapshots WRITE setShowSnapshots
				   NOTIFY showSnapshotsChanged)
	/// Alpha, beta and old-snapshot builds.
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

/* One loader's version list filtered to one Minecraft version ("exact if
 * present"). A build that names no Minecraft version (Fabric Loader, Quilt
 * Loader) is always kept. */
class LoaderVersionListProxy : public VersionListLoadingProxy
{
	Q_OBJECT

  public:
	explicit LoaderVersionListProxy(QObject* parent = nullptr);

	QString minecraftVersion() const
	{
		return m_minecraftVersion;
	}
	/// Re-filters in place; does not reload.
	void setMinecraftVersion(const QString& version);

  protected:
	void resolveRoles(QAbstractItemModel* sourceModel) override;
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;

  private:
	int m_parentVersionRole = -1;
	QString m_minecraftVersion;
};

/* Minecraft version plus loader brand, if given. Empty if @p minecraftVersion
 * is. @p loader is "", "fabric", "quilt", "forge" or "neoforge". */
QString composeSuggestedInstanceName(const QString& minecraftVersion,
									 const QString& loader);

/* Suggested name for an import source (local path, "file://" URL or http(s)
 * link): the file name minus extension, with CurseForge's "?client=y"
 * rewritten. Empty if there is none. */
QString suggestedImportName(const QString& source);

/* True if @p source is a remote http(s)/ftp URL or an existing local file with
 * a modpack-archive extension. Empty or unparsable input is never valid. */
bool importSourceLooksValid(const QString& source);

/*
 * QML-facing "New instance" flow: Minecraft version, optional loader and its
 * version; builds the creation task (see create()).
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
	Q_PROPERTY(bool loaderLoading READ loaderLoading NOTIFY
				   loaderLoadingChanged)

	Q_PROPERTY(QString selectedMinecraftVersion READ selectedMinecraftVersion
				   NOTIFY selectedMinecraftVersionChanged)
	/// Defaults to the newest fitting build until selectLoaderVersion() is called.
	Q_PROPERTY(QString selectedLoaderVersion READ selectedLoaderVersion NOTIFY
				   selectedLoaderVersionChanged)

	/// Existing groups, sorted and deduplicated; "" first for "no group".
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
	Q_INVOKABLE QString suggestedName() const;
	/* Builds and starts the creation task; returns a TaskWatcher parented to this
	 * controller. Null if no Minecraft version is selected or the index lacks it.
	 * @p gameFiles false skips pre-downloading the game's files (they come with
	 * the first launch). */
	Q_INVOKABLE QObject* create(const QString& name, const QString& group,
								const QString& iconKey, bool gameFiles = true);

	Q_INVOKABLE QString suggestedNameForImportSource(const QString& source) const;
	Q_INVOKABLE bool isImportSourceValid(const QString& source) const;
	/* Builds and starts an import of a local archive or modpack URL; returns a
	 * TaskWatcher parented to this controller. Questions the import asks go
	 * through LAUNCHER->uiHost(). Null if @p source is empty or not a usable URL. */
	Q_INVOKABLE QObject* importFrom(const QString& source, const QString& name,
									const QString& group,
									const QString& iconKey);

  signals:
	void loaderChanged();
	void loaderLoadingChanged();
	void selectedMinecraftVersionChanged();
	void selectedLoaderVersionChanged();

  private:
	void refreshLoaderSource();
	/// Defaults selectedLoaderVersion unless selectLoaderVersion() was already called.
	void onLoaderListFirstVersionIdChanged();

	std::unique_ptr<MinecraftVersionListProxy> m_minecraftVersions;
	std::unique_ptr<LoaderVersionListProxy> m_loaderVersions;

	QString m_loader;
	QString m_selectedMinecraftVersion;
	QString m_selectedLoaderVersion;
};
