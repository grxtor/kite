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

#include "models/InstanceShortcutController.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include "DesktopServices.h"
#include "FileSystem.h"
#include "InstanceShortcuts.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"

namespace
{
	QVariantMap location(const QString& value, const QString& label)
	{
		return {{QStringLiteral("value"), value},
				{QStringLiteral("label"), label}};
	}
} // namespace

InstanceShortcutController::InstanceShortcutController(InstancePtr instance,
													   QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
}

QString InstanceShortcutController::instanceId() const
{
	return m_instance->id();
}

QString InstanceShortcutController::instanceName() const
{
	return m_instance->name();
}

QVariantList InstanceShortcutController::locations() const
{
	QVariantList list;
	/* Inside a Flatpak sandbox neither folder is ours to write to; the
	 * save dialog goes through the portal and is the only way out. */
	if (!DesktopServices::isFlatpak()) {
		if (!InstanceShortcuts::folderFor(ShortcutTarget::Desktop).isEmpty()) {
			list.append(location(QStringLiteral("desktop"), tr("Desktop")));
		}
		if (!InstanceShortcuts::folderFor(ShortcutTarget::Applications)
				 .isEmpty()) {
			list.append(
				location(QStringLiteral("applications"), tr("Applications")));
		}
	}
	list.append(location(QStringLiteral("other"), tr("Other…")));
	return list;
}

QString InstanceShortcutController::fileSuffix() const
{
	return InstanceShortcuts::fileSuffix();
}

QStringList InstanceShortcutController::accounts() const
{
	QStringList names;
	if (!LAUNCHER || !LAUNCHER->accounts()) {
		return names;
	}
	auto list = LAUNCHER->accounts();
	for (int i = 0; i < list->count(); ++i) {
		const QString name = list->at(i)->profileName();
		if (!name.isEmpty()) {
			names.append(name);
		}
	}
	return names;
}

QString InstanceShortcutController::defaultAccount() const
{
	if (!LAUNCHER || !LAUNCHER->accounts()) {
		return QString();
	}
	auto account = LAUNCHER->accounts()->defaultAccount();
	return account ? account->profileName() : QString();
}

QString InstanceShortcutController::suggestedName(const QString& server) const
{
	const QString address = server.trimmed();
	return address.isEmpty() ? m_instance->name()
							  : tr("%1 (%2)").arg(m_instance->name(), address);
}

QString InstanceShortcutController::suggestedFileUrl(const QString& name) const
{
	QString folder = FS::getDesktopDir();
	if (folder.isEmpty()) {
		folder = QDir::homePath();
	}
	return QUrl::fromLocalFile(
			   FS::PathCombine(folder, FS::RemoveInvalidFilenameChars(name) +
										   fileSuffix()))
		.toString();
}

bool InstanceShortcutController::create(const QString& name,
										const QString& location,
										const QString& server,
										const QString& account)
{
	const QString address = server.trimmed();

	InstanceShortcuts::Request request;
	request.instance = m_instance.get();
	request.name = name.trimmed().isEmpty() ? suggestedName(address)
											: name.trimmed();
	request.targetString = address.isEmpty() ? tr("instance") : tr("server");
	if (!address.isEmpty()) {
		request.extraArgs << QStringLiteral("--server") << address;
	}
	if (!account.isEmpty()) {
		request.extraArgs << QStringLiteral("--profile") << account;
	}

	InstanceShortcuts::Result result;
	if (location == QLatin1String("desktop") ||
		location == QLatin1String("applications")) {
		request.target = location == QLatin1String("desktop")
							 ? ShortcutTarget::Desktop
							 : ShortcutTarget::Applications;
		result = InstanceShortcuts::createInFolder(request);
	} else {
		const QUrl url(location);
		const QString path = url.isLocalFile() ? url.toLocalFile() : location;
		if (path.isEmpty() || location == QLatin1String("other")) {
			setError(tr("Choose where to put the shortcut."));
			return false;
		}
		request.target = ShortcutTarget::Other;
		result = InstanceShortcuts::create(
			request, InstanceShortcuts::withoutSuffix(path));
	}

	if (!result.ok) {
		setError(result.error);
		return false;
	}
	setError(QString());
	m_lastPath = result.path;
	emit lastPathChanged();
	return true;
}

void InstanceShortcutController::showInFolder() const
{
	if (m_lastPath.isEmpty()) {
		return;
	}
	DesktopServices::openDirectory(QFileInfo(m_lastPath).absolutePath());
}

void InstanceShortcutController::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}
