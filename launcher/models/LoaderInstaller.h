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

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>

class PackProfile;
class LoaderVersionListProxy;

/*
 * QML-facing bridge to installing a mod loader (Forge/NeoForge/Fabric/
 * Quilt/LiteLoader) into one instance's PackProfile - the widget-free
 * replacement for InstallLoaderDialog + LoaderVersionPage.
 *
 * Picking a loader (selectLoader()) points `versions` at a LoaderVersionListProxy
 * (models/NewInstanceController.h) - the very proxy the "New instance"
 * dialog's own loader picker already uses, filtered to builds this
 * instance's Minecraft version can actually run and carrying its own
 * `loading`/`error`/`count` state (see VersionListLoadingProxy), so this
 * class does not need to track any of that itself. Loading is lazy:
 * nothing downloads until a loader is actually selected (mirrors
 * LoaderVersionPage::openedImpl()'s own "fetching them all would stall the
 * window on lists nobody will scroll" reasoning), and re-selecting an
 * already-loaded loader does not re-download - see
 * VersionListLoadingProxy::startLoadIfNeeded().
 *
 * install() collapses InstallLoaderDialog's per-conflict question chain
 * (settleConflicts()) into one automatic decision, the same simplification
 * ContentBrowser::install() already makes for content installs: an enabled,
 * non-custom conflicting loader is turned off (never removed - the least
 * destructive option the widget dialog itself offers), so a plain "Install"
 * click cannot delete anything. `conflictName` tells QML which loader (if
 * any) that will be, so the tab can confirm through the app's own
 * ConfirmDialog before calling install() - this class does not ask itself,
 * since it has no UI to ask with.
 *
 * Created lazily by InstanceDetails::loaderInstaller() and parented to it,
 * so opening the Version tab never touches the network by itself.
 */
class LoaderInstaller : public QObject
{
	Q_OBJECT

	/// Every loader this launcher knows how to install, in display order -
	/// see knownModLoaders(). Each entry is {uid, brandName, iconName}.
	Q_PROPERTY(QVariantList loaders READ loaders CONSTANT)
	/// The uid last passed to selectLoader(), or empty before the first
	/// call.
	Q_PROPERTY(QString selectedUid READ selectedUid NOTIFY selectedUidChanged)
	/// The selected loader's version list, filtered to this instance's
	/// Minecraft version - see the class comment. Its source model is null
	/// (so it lists nothing) before the first selectLoader() call, or when
	/// `supported` is false.
	Q_PROPERTY(QObject* versions READ versions NOTIFY selectedUidChanged)
	/// False when the selected loader is known ahead of time (from its
	/// stated `earliestMinecraft`) not to run on this instance's Minecraft
	/// version - `versions` is not even worth loading then.
	Q_PROPERTY(bool supported READ supported NOTIFY selectedUidChanged)
	/// User-facing reason for `supported` being false; empty otherwise.
	Q_PROPERTY(
		QString unsupportedReason READ unsupportedReason NOTIFY selectedUidChanged)
	/// The selected loader's currently installed version, or empty if it
	/// is not installed at all.
	Q_PROPERTY(
		QString installedVersion READ installedVersion NOTIFY selectedUidChanged)
	/// The display name of an already-installed, switched-on loader that
	/// conflicts with the selected one, or empty if there is none - see
	/// the class comment.
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

	/// Points this installer at loader `uid` (one of loaders()' uids),
	/// loading its version list the first time it is selected - see the
	/// class comment. Re-picking the same uid re-applies the current
	/// Minecraft version filter but does not redownload.
	Q_INVOKABLE void selectLoader(const QString& uid);
	/// Installs `versionId` of the selected loader into the profile: turns
	/// off one conflicting loader if there is one (see conflictName() and
	/// the class comment), sets the component's version, switches it back
	/// on if it was previously installed and disabled, and resolves.
	/// Returns false (the profile's own lastError has why) if nothing is
	/// selected, `versionId` is empty, or the profile is already busy with
	/// another update.
	Q_INVOKABLE bool install(const QString& versionId);

	/// The three kinds of step install() can perform, in the order a
	/// single call may need them.
	enum class InstallStep { DisableConflict, EnableSelected, ChangeVersion };

	/// The fixed order install() performs its side effects in, given how
	/// many already-enabled, non-custom conflicting loaders it found (see
	/// conflictName()'s own filter): every conflict is disabled first,
	/// then the selected loader is switched on, and only then is its
	/// version changed (which is what triggers PackProfile::resolve()).
	/// That last ordering matters: Component::applyTo() skips disabled
	/// components, so resolving while the selected loader is still
	/// disabled would silently drop it from the launch profile until an
	/// unrelated resolve happened later - see install()'s own comment.
	/// Pulled out as a pure, static function so this sequencing is
	/// unit-testable without a real PackProfile/MinecraftInstance
	/// fixture, the same reason ContentBrowser::isVersionCompatible() is
	/// public and static.
	static QList<InstallStep> installSequence(int conflictCount);

  signals:
	void selectedUidChanged();

  private:
	PackProfile* m_profile;
	QString m_selectedUid;
	bool m_supported = true;
	QString m_unsupportedReason;
	/// Re-pointed at a different loader's Meta::VersionList on each
	/// selectLoader() call - mirrors
	/// NewInstanceController::refreshLoaderSource(). One proxy is enough
	/// (unlike ContentBrowser's per-provider models): only one loader is
	/// ever being looked at at a time.
	std::unique_ptr<LoaderVersionListProxy> m_versions;
};
