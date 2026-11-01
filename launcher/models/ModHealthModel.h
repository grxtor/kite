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
 * The mods tab's health check for QML: ModHealth's issues as readable rows plus
 * per-file lookups for the badge on each mods-list row. Holds no folder or
 * instance: InstanceContentController runs ModHealth::check() and hands the
 * result to setIssues() whenever either changes.
 *
 * Each row's `action`:
 *   "enable"  turn `actionKey` on (a switched-off dependency);
 *   "disable" turn `actionKey` off (a clash, a second copy, a file for another
 *             loader or Minecraft version);
 *   "install" download the mod with id `dependencyId` (a missing dependency;
 *             at the wrong version it replaces the file `replacesKey`);
 *   ""        nothing the tab can do (the loader's own version).
 *
 * ModHealth reports a Duplicate once per copy; only the first becomes a row,
 * problemsFor() still answers for both files.
 */
class ModHealthModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ count NOTIFY changed)
	Q_PROPERTY(int blockingCount READ blockingCount NOTIFY changed)
	/// Goes up by one on every setIssues(), so bindings calling problemsFor() re-evaluate.
	Q_PROPERTY(int revision READ revision NOTIFY changed)

  public:
	enum Roles {
		KeyRole = Qt::UserRole + 1,
		NameRole,
		BlockingRole,
		/// One line: "Sodium needs Fabric API".
		TitleRole,
		DetailRole,
		ActionRole,
		ActionKeyRole,
		ActionLabelRole,
		DependencyIdRole,
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

	void setIssues(const QList<ModHealth::Issue>& issues);

	/// Titles of every issue about the file with @p key, blocking first.
	Q_INVOKABLE QStringList problemsFor(const QString& key) const;
	Q_INVOKABLE bool blockingFor(const QString& key) const;
	/// Every "install" row as {key, dependencyId, replacesKey}, in row order.
	Q_INVOKABLE QVariantList installRows() const;

	static QList<ModHealth::Entry> entriesFor(const QList<Mod>& mods);
	/// Minecraft version, loaders and the ids they answer to
	/// (ModHealth::Environment::builtIns): "minecraft"; Fabric "fabricloader"; Quilt
	/// "quilt_loader" and a versionless "fabricloader"; Forge "forge"; NeoForge
	/// "neoforge", plus "forge" on 1.20.1. @p profile may be null.
	static ModHealth::Environment environmentFor(PackProfile* profile);
	/// "fabricloader" -> "Fabric Loader"; anything unknown is unchanged.
	static QString builtInName(const QString& id);
	static QString loaderName(const QString& platformId);

	static QString titleFor(const ModHealth::Issue& issue);
	static QString detailFor(const ModHealth::Issue& issue,
							 const ModHealth::Environment& environment);
	static QString actionFor(const ModHealth::Issue& issue, QString& actionKey,
							 QString& actionLabel, QString& replacesKey);

	/// The environment the next setIssues() was worked out against, for detailFor().
	void setEnvironment(const ModHealth::Environment& environment)
	{
		m_environment = environment;
	}
	/// Loaders each file is built for (by key), for WrongLoader's title. Set before setIssues().
	void setFileLoaders(const QHash<QString, QStringList>& loaders)
	{
		m_fileLoaders = loaders;
	}

  signals:
	void changed();

  private:
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
	/// Every issue, the second Duplicate copy included, for problemsFor()/blockingFor().
	QList<ModHealth::Issue> m_all;
	ModHealth::Environment m_environment;
	QHash<QString, QStringList> m_fileLoaders;
	int m_revision = 0;
};
