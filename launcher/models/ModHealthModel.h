/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "minecraft/mod/ModHealth.h"

class Mod;
class PackProfile;

/*
 * The mods tab's health check as QML sees it: ModHealth's issues as a
 * list of rows a person can read and act on, plus per-file lookups for
 * the badge on each row of the mods list.
 *
 * Holds no folder or instance of its own. InstanceContentController reads
 * the mods folder into entriesFor() and the instance into
 * environmentFor(), runs ModHealth::check() and hands the result to
 * setIssues() whenever either changes - which keeps this testable on
 * made-up issues.
 *
 * Every row says what to do about it through `action`:
 *   "enable"  turn `actionKey` on (a switched-off dependency);
 *   "disable" turn `actionKey` off (a clash, a second copy, a file for
 *             another loader or Minecraft version);
 *   "install" download the mod with id `dependencyId` and install it
 *             (a missing dependency: "Install"; one at the wrong version:
 *             "Update", replacing the file `replacesKey`);
 *   ""        nothing the tab can do (the loader's own version).
 *
 * Duplicate issues come in pairs from ModHealth, one per copy; only the
 * first of each pair becomes a row, so the list names each problem once.
 * problemsFor() still answers for both files.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class ModHealthModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count NOTIFY changed)
	/// Rows the loader would refuse to start over.
	Q_PROPERTY(int blockingCount READ blockingCount NOTIFY changed)
	/// Goes up by one on every setIssues(), so a QML binding that calls
	/// problemsFor() can depend on it and be re-evaluated.
	Q_PROPERTY(int revision READ revision NOTIFY changed)

  public:
	enum Roles {
		/// The file the row is about (ContentFilterModel's key).
		KeyRole = Qt::UserRole + 1,
		NameRole,
		BlockingRole,
		/// One line: "Sodium needs Fabric API".
		TitleRole,
		/// What exactly, and what follows from it: "Version 0.92.0 or
		/// newer is needed, and it is not installed."
		DetailRole,
		/// "enable", "disable", "install" or "".
		ActionRole,
		/// The file "enable"/"disable" acts on.
		ActionKeyRole,
		/// The button's label: "Turn on Fabric API", "Turn off
		/// optifabric-1.14.jar", "Install".
		ActionLabelRole,
		/// The id on the other end of the issue (Issue::otherId): what
		/// "install" looks up.
		DependencyIdRole,
		/// The file "install" replaces - the dependency at the wrong
		/// version - or "".
		ReplacesKeyRole
	};

	explicit ModHealthModel(QObject* parent = nullptr);

	int count() const
	{
		return int(m_rows.size());
	}
	int blockingCount() const;
	int revision() const
	{
		return m_revision;
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// Replaces the issues (as ModHealth::check() returns them).
	void setIssues(const QList<ModHealth::Issue>& issues);

	/// The titles of every issue about the file with @p key, blocking
	/// ones first; empty when it has none.
	Q_INVOKABLE QStringList problemsFor(const QString& key) const;
	/// Whether any issue about the file with @p key is blocking.
	Q_INVOKABLE bool blockingFor(const QString& key) const;
	/// Every "install" row as a map of `key`, `dependencyId` and
	/// `replacesKey`, in row order: what an "Install all" asks for.
	Q_INVOKABLE QVariantList installRows() const;

	/// The files of a mods folder as ModHealth reads them: key, name,
	/// whether it is on, and what it says about itself.
	static QList<ModHealth::Entry> entriesFor(const QList<Mod>& mods);
	/// The instance's Minecraft version, loaders and the ids they answer
	/// to (see ModHealth::Environment::builtIns): "minecraft"; with Fabric
	/// "fabricloader"; with Quilt "quilt_loader" and a versionless
	/// "fabricloader"; with Forge "forge"; with NeoForge "neoforge", and
	/// "forge" as well on Minecraft 1.20.1, where NeoForge still answers
	/// to it. @p profile may be null.
	static ModHealth::Environment environmentFor(PackProfile* profile);
	/// A built-in id as a person knows it: "fabricloader" -> "Fabric
	/// Loader", "minecraft" -> "Minecraft"; anything else unchanged.
	static QString builtInName(const QString& id);
	/// A loader platform id as a person knows it: "neoforge" ->
	/// "NeoForge".
	static QString loaderName(const QString& platformId);

	/// The row texts and action for @p issue, as data() returns them.
	static QString titleFor(const ModHealth::Issue& issue);
	static QString detailFor(const ModHealth::Issue& issue,
							 const ModHealth::Environment& environment);
	static QString actionFor(const ModHealth::Issue& issue, QString& actionKey,
							 QString& actionLabel, QString& replacesKey);

	/// The environment the next setIssues() was worked out against, for
	/// detailFor() ("this instance runs Fabric").
	void setEnvironment(const ModHealth::Environment& environment)
	{
		m_environment = environment;
	}
	/// The loaders each file is built for (details.loaders, by key), for
	/// WrongLoader's title: an Issue does not carry them. Set before the
	/// next setIssues(), like setEnvironment().
	void setFileLoaders(const QHash<QString, QStringList>& loaders)
	{
		m_fileLoaders = loaders;
	}

  signals:
	void changed();

  private:
	/// titleFor(), with the file's loaders in WrongLoader's.
	QString titleOf(const ModHealth::Issue& issue) const;

	struct Row {
		ModHealth::Issue issue;
		QString title;
		QString detail;
		QString action;
		QString actionKey;
		QString actionLabel;
		QString replacesKey;
	};
	QList<Row> m_rows;
	/// Every issue, the second copy of each Duplicate pair included, for
	/// problemsFor() / blockingFor().
	QList<ModHealth::Issue> m_all;
	ModHealth::Environment m_environment;
	QHash<QString, QStringList> m_fileLoaders;
	int m_revision = 0;
};
