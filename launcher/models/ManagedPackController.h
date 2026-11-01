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
#include <QString>
#include <QUrl>
#include <QVariantList>

#include "modplatform/ManagedPackVersions.h"
#include "net/NetJob.h"

class BaseInstance;

/*
 * QML-facing view of one instance's Modrinth/CurseForge provenance and
 * available updates, for instances with a catalogue id. A pack without one (an
 * old import) cannot be updated from a hand-typed URL here: hasPackId is false
 * so the tab can say so. Created lazily by InstanceDetails::managedPack(), only
 * when isSupported() says yes.
 */
class ManagedPackController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString providerLabel READ providerLabel CONSTANT)
	Q_PROPERTY(QString packName READ packName CONSTANT)
	Q_PROPERTY(QString packUrl READ packUrl CONSTANT)
	Q_PROPERTY(QString installedVersionName READ installedVersionName CONSTANT)
	Q_PROPERTY(QString installedVersionId READ installedVersionId CONSTANT)
	/// False for a pack with a known provider but no catalogue id (an old import);
	/// fetchVersions()/updateToVersion() are no-ops then.
	Q_PROPERTY(bool hasPackId READ hasPackId CONSTANT)
	/// {id, label, name, minecraft, releaseType ("release", "beta", "alpha" or ""),
	/// current, installable, changelog}, newest first; empty until fetchVersions() succeeds.
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	/// Name filter for the "update from a file" dialog, e.g. "Modrinth pack (*.mrpack *.zip)".
	Q_PROPERTY(QString fileFilter READ fileFilter CONSTANT)
	/// An update replaces files the game has open, so updates refuse while running.
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit ManagedPackController(BaseInstance* instance,
								   QObject* parent = nullptr);
	~ManagedPackController() override;

	/// A known provider, and for CurseForge a build with an API key compiled in.
	static bool isSupported(const BaseInstance* instance);

	enum class Provider { Unknown, Modrinth, CurseForge };
	/// Maps the `PackProvider` string onto the enum ("flame" is a CurseForge
	/// synonym). Public for unit tests.
	static Provider providerFromString(const QString& provider);
	static QString fileFilterFor(Provider provider);

	QString providerLabel() const;
	QString fileFilter() const;
	bool running() const;
	QString packName() const;
	QString packUrl() const;
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

	/// No-op once loaded or loading; see reload().
	Q_INVOKABLE void fetchVersions();
	/// Drops what was loaded and fetches again.
	Q_INVOKABLE void reload();
	/// Replaces this instance with version `versions()[index]` in place (trusted:
	/// the URL came from the catalogue). TaskWatcher, or null for a bad index, a
	/// version with no download (CurseForge withholds some) or a running instance.
	Q_INVOKABLE QObject* updateToVersion(int index);
	/// Replaces this instance with the archive at @p file; works for a pack without
	/// a catalogue id. No version is recorded and the source is untrusted
	/// (untrusted mods are asked about). TaskWatcher, or null if @p file does not
	/// exist or the instance is running.
	Q_INVOKABLE QObject* updateFromFile(const QUrl& file);

  signals:
	void versionsChanged();
	void loadingChanged();
	void runningChanged();
	void errorChanged();

  private:
	void setLoading(bool loading);
	void setError(const QString& error);
	void applyVersions(quint64 generation, const QByteArray& bytes);
	void rebuildVariants();
	/// The import both update paths run: replaces this instance in place
	/// with the pack at @p url. Returns the watcher, already started.
	QObject* startUpdate(const QUrl& url, bool trusted,
						 const QString& versionId, const QString& versionName,
						 const QString& title);

	/// Borrowed; valid for this controller's lifetime (both are parented to InstanceDetails).
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
