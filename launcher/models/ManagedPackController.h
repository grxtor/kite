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
#include <QString>
#include <QVariantList>

#include "modplatform/ManagedPackVersions.h"
#include "net/NetJob.h"

class BaseInstance;

/*
 * QML-facing view of one instance's Modrinth/CurseForge provenance and
 * available updates - the widget-free replacement for ManagedPackPage,
 * scoped to instances that have a catalogue id (hasManagedPackId()).
 *
 * The no-pack-id mode ManagedPackPage also offers (update from a hand-typed
 * URL or a local file) is deliberately not reproduced here: that mode
 * exists for instances an older MeshMC, or a drag-and-drop import, recorded
 * without ever storing a catalogue id, and covering it would mean adding a
 * QML file picker for an import path this pass has not had time to verify
 * end to end. hasPackId is false for that case so a QML tab can say so
 * rather than pretend the feature works.
 *
 * Created lazily by InstanceDetails::managedPack() - see that method - and
 * only when isSupported() actually says yes, mirroring ManagedPackPage::
 * isSupported() exactly (a provider MeshMC recognises, and a CurseForge
 * build actually compiled with an API key).
 */
class ManagedPackController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString providerLabel READ providerLabel CONSTANT)
	Q_PROPERTY(QString packName READ packName CONSTANT)
	Q_PROPERTY(QString packUrl READ packUrl CONSTANT)
	Q_PROPERTY(QString packId READ packId CONSTANT)
	Q_PROPERTY(QString installedVersionName READ installedVersionName CONSTANT)
	Q_PROPERTY(QString installedVersionId READ installedVersionId CONSTANT)
	/// False for a pack MeshMC knows the provider of but not the catalogue
	/// id of (an old import) - fetchVersions()/updateToVersion() are both
	/// no-ops while this is false. See the class comment.
	Q_PROPERTY(bool hasPackId READ hasPackId CONSTANT)
	/// QVariantList of {id, label, current, installable, changelog}, newest
	/// first - empty until fetchVersions() succeeds.
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	/// Empty on success, or while nothing has failed yet.
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit ManagedPackController(BaseInstance* instance,
								   QObject* parent = nullptr);
	~ManagedPackController() override;

	/// Whether an instance page should offer this at all - mirrors
	/// ManagedPackPage::isSupported() exactly (see its own comment):
	/// a provider MeshMC recognises, and - for CurseForge - a build that
	/// actually has an API key compiled in.
	static bool isSupported(const BaseInstance* instance);

	enum class Provider { Unknown, Modrinth, CurseForge };
	/// Maps the `PackProvider` string onto the enum - mirrors
	/// ManagedPackPage::providerFromString() exactly (see its own comment
	/// on "flame" as a CurseForge synonym). Public, unlike the rest of this
	/// class's internals, so it can be unit tested without a BaseInstance -
	/// same reasoning as ContentBrowser::isVersionCompatible().
	static Provider providerFromString(const QString& provider);

	QString providerLabel() const;
	QString packName() const;
	QString packUrl() const;
	QString packId() const;
	QString installedVersionName() const;
	QString installedVersionId() const;
	bool hasPackId() const;
	QVariantList versions() const
	{
		return m_versionsVariant;
	}
	bool loading() const
	{
		return m_loading;
	}
	QString error() const
	{
		return m_error;
	}

	/// Fetches the pack's version list. A no-op once already loaded (or
	/// loading) - see reload() to force a refresh.
	Q_INVOKABLE void fetchVersions();
	/// Drops whatever was loaded and fetches again - for a "Reload" action
	/// after a failure.
	Q_INVOKABLE void reload();
	/// Replaces this instance with version `versions()[index]` in place,
	/// the same InstanceImportTask-based update ManagedPackPage::
	/// updatePack() runs for a catalogue-selected version (trusted source,
	/// since the download URL came from the catalogue itself). Returns a
	/// TaskWatcher, or null when `index` is out of range, that version has
	/// no download (CurseForge withholds some), or the instance is
	/// currently running.
	Q_INVOKABLE QObject* updateToVersion(int index);

  signals:
	void versionsChanged();
	void loadingChanged();
	void errorChanged();

  private:
	void setLoading(bool loading);
	void setError(const QString& error);
	void applyVersions(quint64 generation, const QByteArray& bytes);
	void rebuildVariants();

	/// Borrowed - valid for this controller's whole lifetime (parented to
	/// the InstanceDetails that owns both).
	BaseInstance* m_instance;
	Provider m_provider = Provider::Unknown;

	ManagedPack::VersionList m_versions;
	QVariantList m_versionsVariant;
	bool m_loaded = false;
	bool m_loading = false;
	QString m_error;

	NetJob::Ptr m_versionsJob;
	quint64 m_generation = 0;
};
