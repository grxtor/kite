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

#include "models/ComponentTools.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "DesktopServices.h"
#include "core/LauncherContext.h"
#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"
#include "tasks/TaskWatcher.h"

ComponentTools::ComponentTools(std::shared_ptr<MinecraftInstance> instance,
							   QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
}

ComponentTools::~ComponentTools() = default;

void ComponentTools::setError(const QString& error)
{
	if (m_error != error) {
		m_error = error;
		emit errorChanged();
	}
}

PackProfile* ComponentTools::profile() const
{
	return m_instance ? m_instance->getPackProfile().get() : nullptr;
}

QString ComponentTools::suggestUid(const QString& name)
{
	QString letters = name.toLower();
	letters.remove(QRegularExpression(QStringLiteral("[^a-z]")));
	if (letters.isEmpty()) {
		return QString();
	}
	return QStringLiteral("org.projecttick.custom.") + letters;
}

QString ComponentTools::validateNewComponent(const QString& name,
											 const QString& uid,
											 const QStringList& existingUids)
{
	const QString cleanName = name.trimmed();
	if (cleanName.isEmpty()) {
		return tr("Give the component a name.");
	}
	QString cleanUid = uid.trimmed();
	if (cleanUid.isEmpty()) {
		cleanUid = suggestUid(cleanName);
	}
	if (cleanUid.isEmpty()) {
		return tr("Give the component an id, for example "
				  "org.example.mycomponent.");
	}
	if (existingUids.contains(cleanUid)) {
		return tr("This instance already has a component with that id.");
	}
	return QString();
}

QStringList ComponentTools::componentUids() const
{
	QStringList uids;
	if (auto* p = profile()) {
		for (int i = 0; i < p->rowCount(); ++i) {
			uids << p->getComponent(i)->getID();
		}
	}
	return uids;
}

QString ComponentTools::addEmpty(const QString& name, const QString& uid)
{
	auto* p = profile();
	if (!p) {
		return tr("This instance has no components.");
	}
	const QString problem = validateNewComponent(name, uid, componentUids());
	if (!problem.isEmpty()) {
		return problem;
	}
	QString cleanUid = uid.trimmed();
	if (cleanUid.isEmpty()) {
		cleanUid = suggestUid(name.trimmed());
	}
	if (!p->installEmpty(cleanUid, name.trimmed())) {
		return tr("The component could not be created.");
	}
	return QString();
}

QStringList ComponentTools::existingFiles(const QList<QUrl>& urls)
{
	QStringList paths;
	for (const QUrl& url : urls) {
		const QString path = url.toLocalFile();
		if (!path.isEmpty() && QFileInfo(path).isFile()) {
			paths << path;
		}
	}
	return paths;
}

bool ComponentTools::addJarMods(const QList<QUrl>& files)
{
	auto* p = profile();
	const QStringList paths = existingFiles(files);
	if (!p || paths.isEmpty()) {
		setError(tr("Choose at least one .jar or .zip file."));
		return false;
	}
	setError(QString());
	p->installJarMods(paths);
	return true;
}

bool ComponentTools::replaceMainJar(const QUrl& file)
{
	auto* p = profile();
	const QStringList paths = existingFiles({file});
	if (!p || paths.isEmpty()) {
		setError(tr("Choose a .jar file."));
		return false;
	}
	setError(QString());
	p->installCustomJar(paths.first());
	return true;
}

QObject* ComponentTools::downloadAll()
{
	if (!m_instance) {
		return nullptr;
	}
	if (!LAUNCHER || !LAUNCHER->accounts()->anyAccountIsValid()) {
		setError(tr("MeshMC cannot download Minecraft unless you have signed "
					"in with an account. Add one under Accounts."));
		return nullptr;
	}
	Task::Ptr task = m_instance->createUpdateTask(Net::Mode::Online);
	if (!task) {
		setError(tr("There is nothing to download for this instance."));
		return nullptr;
	}
	setError(QString());
	auto* watcher = new TaskWatcher(task, this);
	watcher->setTitle(tr("Downloading game files"));
	task->start();
	return watcher;
}

bool ComponentTools::hasJsonFile(int row) const
{
	auto* p = profile();
	if (!p || row < 0 || row >= p->rowCount()) {
		return false;
	}
	return QFileInfo::exists(p->getComponent(row)->getFilename());
}

bool ComponentTools::editComponentJson(int row)
{
	auto* p = profile();
	if (!p || row < 0 || row >= p->rowCount()) {
		return false;
	}
	const QString file = p->getComponent(row)->getFilename();
	if (!QFileInfo::exists(file)) {
		setError(tr("This component has no file of its own to edit. "
					"Customize it first."));
		return false;
	}
	setError(QString());
	// The editor chosen in Settings > External tools, else whatever the
	// system opens .json files with - the same rule Application uses.
	const QString absolute = QDir::current().absoluteFilePath(file);
	const QString editor = LAUNCHER && LAUNCHER->settings()
							   ? LAUNCHER->settings()->get("JsonEditor").toString()
							   : QString();
	const bool ok = editor.isEmpty()
						? DesktopServices::openUrl(QUrl::fromLocalFile(absolute))
						: DesktopServices::run(editor, {absolute});
	if (!ok) {
		setError(tr("The JSON editor could not be started."));
	}
	return ok;
}

void ComponentTools::openLibrariesFolder() const
{
	if (m_instance) {
		DesktopServices::openDirectory(m_instance->getLocalLibraryPath(), true);
	}
}

void ComponentTools::openGameFolder() const
{
	if (m_instance) {
		DesktopServices::openDirectory(m_instance->gameRoot(), true);
	}
}
