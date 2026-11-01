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

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>

class PackProfile;
class LoaderVersionListProxy;

/*
 * QML-facing install of a mod loader (Forge/NeoForge/Fabric/Quilt/LiteLoader)
 * into one instance's PackProfile. selectLoader() points `versions` at a
 * LoaderVersionListProxy filtered to this instance's Minecraft version;
 * nothing downloads until a loader is selected. install() turns an enabled,
 * non-custom conflicting loader off (never removes it); conflictName tells QML
 * which, so the tab can confirm before calling install(). Created lazily by
 * InstanceDetails and parented to it.
 */
class LoaderInstaller : public QObject
{
	Q_OBJECT

	/// {uid, brandName, iconName} per loader, in display order.
	Q_PROPERTY(QVariantList loaders READ loaders CONSTANT)
	Q_PROPERTY(QString selectedUid READ selectedUid NOTIFY selectedUidChanged)
	/// Null source model (lists nothing) before the first selectLoader() or when `supported` is false.
	Q_PROPERTY(QObject* versions READ versions NOTIFY selectedUidChanged)
	/// False when the loader's `earliestMinecraft` is known not to run on this Minecraft version.
	Q_PROPERTY(bool supported READ supported NOTIFY selectedUidChanged)
	Q_PROPERTY(
		QString unsupportedReason READ unsupportedReason NOTIFY selectedUidChanged)
	Q_PROPERTY(
		QString installedVersion READ installedVersion NOTIFY selectedUidChanged)
	/// Display name of an installed, switched-on loader that conflicts with the selected one; empty if none.
	Q_PROPERTY(QString conflictName READ conflictName NOTIFY selectedUidChanged)

  public:
	explicit LoaderInstaller(PackProfile* profile, QObject* parent = nullptr);
	~LoaderInstaller() override;

	QVariantList loaders() const;
	QString selectedUid() const
	{
		return m_selectedUid;
	}
	QObject* versions() const;
	bool supported() const
	{
		return m_supported;
	}
	QString unsupportedReason() const
	{
		return m_unsupportedReason;
	}
	QString installedVersion() const;
	QString conflictName() const;

	/// Loads the version list the first time; re-picking a uid re-filters but does not redownload.
	Q_INVOKABLE void selectLoader(const QString& uid);
	/// Disables one conflicting loader if any, sets the component's version,
	/// re-enables it if it was installed and disabled, and resolves. False (the
	/// profile's lastError has why) if nothing is selected, @p versionId is empty
	/// or the profile is busy.
	Q_INVOKABLE bool install(const QString& versionId);

	enum class InstallStep { DisableConflict, EnableSelected, ChangeVersion };

	/// Order of install()'s side effects: disable every conflict, switch the
	/// selected loader on, then change its version (which triggers resolve()).
	/// Resolving while it is still disabled would drop it from the launch profile,
	/// since Component::applyTo() skips disabled components. Static so the order is
	/// unit-testable.
	static QList<InstallStep> installSequence(int conflictCount);

  signals:
	void selectedUidChanged();

  private:
	PackProfile* m_profile;
	QString m_selectedUid;
	bool m_supported = true;
	QString m_unsupportedReason;
	/// Re-pointed at the selected loader's list on each selectLoader().
	std::unique_ptr<LoaderVersionListProxy> m_versions;
};
