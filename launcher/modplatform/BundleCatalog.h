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

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "net/NetJob.h"

/*
 * The curated catalog of recommended mods: bundles (Performance, Comfort,
 * PvP) of Modrinth project ids, and the presets ("Kite Performance") that
 * pick bundles for a new instance.
 *
 * The catalog built into the launcher (:/kite/bundles.json) is what a
 * fresh install uses; refresh() replaces it with meta.kiteclient.com's
 * copy when there is one, so the lists can change without a release.
 * Strings in it are {"en": ..., "tr": ...}; they are resolved on every
 * call from the selected language, so nothing here caches a translation.
 */
class BundleCatalog : public QObject
{
	Q_OBJECT

	/// id, name, summary, minecraft ("" = any), loader, bundles (ids),
	/// options (the id of the options.txt set).
	Q_PROPERTY(QVariantList presets READ presets NOTIFY changed)

  public:
	explicit BundleCatalog(QObject* parent = nullptr);

	/// Replaces the catalog with @p json when it is a valid format-1
	/// catalog; otherwise leaves it as it was and says why in @p error.
	bool loadFromJson(const QByteArray& json, QString* error = nullptr);
	/// Overrides the selected language, for tests.
	void setLanguage(const QString& language);

	QVariantList presets() const;

	/// The mods of @p bundleIds, in catalog order, that exist for
	/// @p loader (empty = vanilla, nothing): project, name, summary, bundle,
	/// bundleName, category, categoryName, isDefault, serverSafe, icon (a URL,
	/// "" when the catalog has none). A project
	/// in several bundles is listed once, under the first.
	Q_INVOKABLE QVariantList mods(const QStringList& bundleIds,
								  const QString& loader) const;

	/// A started ContentInstallReview of @p projectIds (and what they
	/// need) for @p mcVersion and @p loader, not yet tied to an instance;
	/// null when there is nothing to install. Give it to installInto().
	Q_INVOKABLE QObject* preview(const QStringList& projectIds,
								 const QString& mcVersion,
								 const QString& loader);
	/// Points @p review at the mods folder of instance @p instanceId and
	/// installs it: the download's TaskWatcher, or null.
	Q_INVOKABLE QObject* installInto(QObject* review,
									 const QString& instanceId);

	/// Sets the preset's memory and, if the instance has none yet, its
	/// options.txt, and turns Kite Performance on for a preset that has the
	/// performance bundle. Whether anything was applied.
	Q_INVOKABLE bool applyPresetDefaults(const QString& instanceId,
										 const QString& presetId);

	/// Replaces the catalog with the copy refresh() last downloaded, when
	/// there is one and it is valid; no network. For code that wants the
	/// current lists without showing them (the launch-time setup).
	bool loadCached();

	/// Asks meta.kiteclient.com for a newer catalog; once per run, silent
	/// when there is none.
	Q_INVOKABLE void refresh();

  signals:
	void changed();

  private:
	QString language() const;
	/// The text of a {"en": ..., "tr": ...} value in the current language.
	QString localized(const QJsonValue& value) const;
	QJsonObject presetById(const QString& presetId) const;
	void onRefreshed(const QString& path);

  private:
	QJsonObject m_catalog;
	QString m_language;
	bool m_refreshed = false;
	NetJob::Ptr m_refreshJob;
};
