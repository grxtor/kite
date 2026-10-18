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

#include "models/FolderSettingsController.h"

#include <QDir>

#include "BuildConfig.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"

namespace
{
	const QStringList kFolderKeys = {
		QStringLiteral("InstanceDir"), QStringLiteral("CentralModsDir"),
		QStringLiteral("IconsDir"), QStringLiteral("SkinsDir"),
		QStringLiteral("JavaDir")};

	// What the file dialog gave us, if it is a folder that exists.
	QString existingDir(const QUrl& url)
	{
		const QString raw = url.toLocalFile();
		return !raw.isEmpty() && QDir(raw).exists() ? raw : QString();
	}
} // namespace

FolderSettingsController::FolderSettingsController(QObject* parent)
	: QObject(parent)
{
	if (LAUNCHER && LAUNCHER->settings()) {
		connect(LAUNCHER->settings().get(), &SettingsObject::SettingChanged,
				this, [this](const Setting& setting, QVariant) {
					if (setting.id() == QLatin1String("AdditionalInstanceDirs")) {
						emit additionalDirsChanged();
					}
				});
	}
}

FolderSettingsController::~FolderSettingsController() = default;

QStringList FolderSettingsController::additionalDirs() const
{
	if (!LAUNCHER || !LAUNCHER->settings()) {
		return {};
	}
	return InstanceList::decodeInstanceDirList(
		LAUNCHER->settings()->get("AdditionalInstanceDirs"));
}

bool FolderSettingsController::isFolderKey(const QString& key)
{
	return kFolderKeys.contains(key);
}

QString FolderSettingsController::instanceDirWarning(const QString& rawDir,
													 const QString& cookedDir,
													 bool flatpak)
{
	if (FS::checkProblemticPathJava(QDir(cookedDir))) {
		return tr("This folder's path contains a \"!\". Java is known to "
				  "have trouble with that, so your instances probably will "
				  "not start.");
	}

	/* A folder handed to us through a Flatpak portal is borrowed, not
	 * granted: the sandbox exposes a one-off pick under /run/user for the
	 * lifetime of the session. It works until the next restart, when the
	 * launcher can no longer see it and the instances in it read as
	 * deleted. */
	if (flatpak && rawDir.startsWith(QLatin1String("/run/user"))) {
		return tr("This folder was shared with %1 only for this session. "
				  "After a restart %1 will no longer be able to open it. "
				  "Granting it access with Flatseal is recommended.")
			.arg(BuildConfig.KITE_DISPLAYNAME);
	}
	return QString();
}

QString FolderSettingsController::duplicateDirMessage(
	const QString& cookedDir, const QString& primaryDir,
	const QStringList& additionalDirs)
{
	if (cookedDir.compare(FS::NormalizePath(primaryDir),
						  Qt::CaseInsensitive) == 0) {
		return tr("This is already your main instance folder.");
	}
	for (const QString& dir : additionalDirs) {
		if (cookedDir.compare(dir, Qt::CaseInsensitive) == 0) {
			return tr("This folder has already been added.");
		}
	}
	return QString();
}

bool FolderSettingsController::setFolder(const QString& key, const QUrl& url)
{
	if (!LAUNCHER || !isFolderKey(key)) {
		return false;
	}
	const QString raw = existingDir(url);
	if (raw.isEmpty()) {
		return false;
	}
	const QString cooked = FS::NormalizePath(raw);

	if (key == QLatin1String("InstanceDir")) {
		const QString warning =
			instanceDirWarning(raw, cooked, DesktopServices::isFlatpak());
		if (!warning.isEmpty() &&
			!LAUNCHER->uiHost()->confirm(
				tr("Use this folder?"), warning + QStringLiteral("\n\n") +
											tr("Do you want to use it anyway?"),
				UiHost::Severity::Warning, tr("Use it anyway"),
				tr("Cancel"))) {
			return false;
		}
	}
	LAUNCHER->settings()->set(key, cooked);
	return true;
}

bool FolderSettingsController::addAdditionalDir(const QUrl& url)
{
	if (!LAUNCHER) {
		return false;
	}
	const QString raw = existingDir(url);
	if (raw.isEmpty()) {
		return false;
	}
	const QString cooked = FS::NormalizePath(raw);

	// Before the path warnings: a folder that is already known is the whole
	// answer, and there is no point warning about a "!" in a path we are
	// about to refuse for another reason.
	const QString duplicate = duplicateDirMessage(
		cooked, LAUNCHER->settings()->get("InstanceDir").toString(),
		additionalDirs());
	if (!duplicate.isEmpty()) {
		LAUNCHER->uiHost()->message(tr("Folder already in use"), duplicate,
									UiHost::Severity::Warning);
		return false;
	}

	const QString warning =
		instanceDirWarning(raw, cooked, DesktopServices::isFlatpak());
	if (!warning.isEmpty() &&
		!LAUNCHER->uiHost()->confirm(
			tr("Use this folder?"),
			warning + QStringLiteral("\n\n") +
				tr("Do you want to use it anyway?"),
			UiHost::Severity::Warning, tr("Use it anyway"), tr("Cancel"))) {
		return false;
	}

	QStringList dirs = additionalDirs();
	dirs << cooked;
	/* Encoded, not handed over as a list: the INI backend stringifies every
	 * value it writes, and a multi-element QStringList stringifies to
	 * nothing at all. InstanceList owns that detail. */
	LAUNCHER->settings()->set("AdditionalInstanceDirs",
							  InstanceList::encodeInstanceDirList(dirs));
	return true;
}

void FolderSettingsController::removeAdditionalDir(const QString& dir)
{
	if (!LAUNCHER) {
		return;
	}
	QStringList dirs = additionalDirs();
	if (dirs.removeAll(dir) == 0) {
		return;
	}
	LAUNCHER->settings()->set("AdditionalInstanceDirs",
							  InstanceList::encodeInstanceDirList(dirs));
}
