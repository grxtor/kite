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

#include "qml/QmlShell.h"

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "InstanceList.h"
#include "models/IdSelectionModel.h"
#include "models/InstanceDetails.h"
#include "models/InstanceFilterModel.h"
#include "models/SettingsAdapter.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "Sys.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"
#include <QDir>
#include "qml/AccountFaceProvider.h"
#include "qml/InstanceIconProvider.h"
#include "qml/ScreenshotThumbnailProvider.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace
{
	/* Spelled out rather than loadFromModule(), which is Qt 6.5+; the floor is
	 * 6.4. The module sets RESOURCE_PREFIX "/qt/qml" so this path is stable. */
	const QUrl kRootUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml"));
} // namespace

QmlShell::QmlShell(QObject* parent) : QObject(parent)
{
	/* Sidebar account summary. Both signals exist on AccountList already;
	 * either one moving the default account or the list itself is reason
	 * enough to re-read all three properties, so both are wired to the
	 * same accountChanged() rather than tracked separately. */
	auto accounts = LAUNCHER->accounts();
	/* Any change may be a new skin under the same account id, and QML
	 * caches images by url: the revision in the face url makes it refetch. */
	const auto bump = [this] {
		++m_accountRevision;
		emit accountChanged();
	};
	connect(accounts.get(), &AccountList::listChanged, this, bump);
	connect(accounts.get(), &AccountList::defaultAccountChanged, this, bump);
}

QmlShell::~QmlShell() = default;

QObject* QmlShell::expose(QObject* object)
{
	if (object) {
		QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
	}
	return object;
}

QString QmlShell::accountName() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	return account ? account->profileName() : QString();
}

QString QmlShell::accountFace() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	if (!account) {
		return QString();
	}
	// Offline accounts have no profile id; the provider accepts either.
	const QString id = account->profileId().isEmpty() ? account->internalId()
													   : account->profileId();
	return QStringLiteral("image://accountface/%1?rev=%2")
		.arg(id)
		.arg(m_accountRevision);
}

QString QmlShell::accountKind() const
{
	auto account = LAUNCHER->accounts()->defaultAccount();
	if (!account) {
		return QString();
	}
	return account->isMSA() ? QStringLiteral("Microsoft")
							: QStringLiteral("Offline");
}

int QmlShell::accountCount() const
{
	return LAUNCHER->accounts()->count();
}

void QmlShell::launchInstance(const QString& id)
{
	emit launchRequested(id);
}

void QmlShell::killInstance(const QString& id)
{
	emit killRequested(id);
}

void QmlShell::editInstance(const QString& id)
{
	emit editRequested(id);
}

void QmlShell::openInstanceFolder(const QString& id)
{
	emit folderRequested(id);
}

void QmlShell::createInstance()
{
	emit createInstanceRequested();
}

void QmlShell::openSettings(const QString& page)
{
	emit settingsRequested(page);
}

void QmlShell::openPath(const QString& path)
{
	DesktopServices::openDirectory(QDir(path).absolutePath(), true);
}

void QmlShell::manageAccounts()
{
	emit accountsRequested();
}

QObject* QmlShell::settings() const
{
	return expose(m_settings.get());
}

int QmlShell::systemMemoryMiB() const
{
	return static_cast<int>(Sys::getSystemRam() / Sys::mebibyte);
}

QObject* QmlShell::modpackModel() const
{
	return expose(m_modpacks.get());
}

QObject* QmlShell::installModpack(const QString& projectId,
								  const QString& versionId,
								  const QString& instanceName,
								  const QString& group)
{
	if (!m_modpacks) {
		return nullptr;
	}
	return expose(
		m_modpacks->install(projectId, versionId, instanceName, group));
}

QObject* QmlShell::recentModel() const
{
	return expose(m_recent.get());
}

QObject* QmlShell::heroModel() const
{
	return expose(m_hero.get());
}

QObject* QmlShell::instancePageModel() const
{
	return expose(m_instancePage.get());
}

QObject* QmlShell::sectionModel(const QString& group)
{
	if (!m_instances) {
		return nullptr;
	}
	auto& section = m_sections[group];
	if (!section) {
		section = std::make_unique<InstanceFilterModel>();
		section->setExactGroup(true);
		section->setGroup(group);
		section->setSourceModel(m_instances.get());
	}
	return expose(section.get());
}

QObject* QmlShell::instanceDetails(const QString& id)
{
	if (m_instanceDetails && m_instanceDetails->instanceId() == id) {
		return expose(m_instanceDetails.get());
	}

	auto instance = LAUNCHER->instances()->getInstanceById(id);
	if (!instance) {
		return nullptr;
	}

	// Replaces (and destroys, via unique_ptr assignment) whichever detail
	// page was open before - only one is kept at a time.
	m_instanceDetails = std::make_unique<InstanceDetails>(instance);

	// Every QObject* the bridge hands to QML needs the same CppOwnership
	// pinning as everything else exposed here, or the engine will try to
	// delete a model the instance still owns.
	expose(m_instanceDetails->settings());
	expose(m_instanceDetails->mods());
	expose(m_instanceDetails->worlds());
	expose(m_instanceDetails->log());
	expose(m_instanceDetails->components());
	expose(m_instanceDetails->screenshots());

	return expose(m_instanceDetails.get());
}

QVariantMap QmlShell::rootProperties()
{
	QVariantMap props;
	props.insert(QStringLiteral("instanceModel"),
				 QVariant::fromValue(expose(m_instances.get())));
	props.insert(QStringLiteral("selection"),
				 QVariant::fromValue(expose(m_selection.get())));
	props.insert(QStringLiteral("shell"), QVariant::fromValue(expose(this)));
	return props;
}

bool QmlShell::show(bool minimized)
{
	if (m_window) {
		m_window->showNormal();
		m_window->raise();
		m_window->requestActivate();
		return true;
	}

	/* The grid shows the core's instance list through a filtering, naturally
	 * sorted proxy -- the same ordering the widget grid used -- and keeps its
	 * selection by instance id, since rows move under the proxy. */
	m_instances = std::make_unique<InstanceFilterModel>();
	m_instances->setSourceModel(LAUNCHER->instances().get());
	m_recent = std::make_unique<InstanceFilterModel>();
	m_recent->setRecentFirst(true);
	m_recent->setSourceModel(LAUNCHER->instances().get());
	m_hero = std::make_unique<InstanceFilterModel>();
	/* Nothing matches until QML names an instance, so there is no flash of
	 * every instance as a hero. Ids are folder names; none contains '/'. */
	m_hero->setInstanceId(QStringLiteral("/"));
	m_hero->setSourceModel(LAUNCHER->instances().get());
	m_instancePage = std::make_unique<InstanceFilterModel>();
	m_instancePage->setInstanceId(QStringLiteral("/"));
	m_instancePage->setSourceModel(LAUNCHER->instances().get());
	m_selection = std::make_unique<IdSelectionModel>();
	m_settings = std::make_unique<SettingsAdapter>(LAUNCHER->settings());
	m_modpacks = std::make_unique<ModrinthModpackModel>();

	/* The sort order reads InstSortMode on every comparison, but a proxy
	 * only compares when told to: re-sort when the setting moves. */
	connect(LAUNCHER->settings().get(), &SettingsObject::SettingChanged, this,
			[this](const Setting& setting, const QVariant&) {
				if (setting.id() != QLatin1String("InstSortMode")) {
					return;
				}
				m_instances->invalidate();
				for (auto& section : m_sections) {
					section.second->invalidate();
				}
			});

	/* Must be chosen before the first engine exists: Qt Quick Controls binds
	 * its style when QtQuick.Controls is first imported, and cannot switch
	 * afterwards. The style falls back to Basic for anything it does not
	 * define itself. */
	QQuickStyle::setStyle(QStringLiteral("MeshMC.Style"));

	m_engine = std::make_unique<QQmlApplicationEngine>();
	m_engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

	/* The engine takes ownership of image providers. Registered before load()
	 * so the first frame already has icons rather than broken images. */
	m_engine->addImageProvider(QStringLiteral("instanceicon"),
							   new InstanceIconProvider(LAUNCHER->icons()));
	m_engine->addImageProvider(QStringLiteral("accountface"),
							   new AccountFaceProvider());
	m_engine->addImageProvider(QStringLiteral("screenshot"),
							   new ScreenshotThumbnailProvider());
	m_engine->setInitialProperties(rootProperties());
	m_engine->load(kRootUrl);

	const auto roots = m_engine->rootObjects();
	m_window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
	if (!m_window) {
		qCritical() << "QML shell: the root object at" << kRootUrl
					<< "failed to load or is not a window";
		m_engine.reset();
		return false;
	}

	connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
		if (!visible)
			emit closed();
	});

	if (minimized)
		m_window->showMinimized();
	else
		m_window->show();

	scheduleSnapshotIfRequested();
	return true;
}

void QmlShell::scheduleSnapshotIfRequested()
{
	/* MESHMC_QML_SNAPSHOT=<file.png> renders the real window, with the real
	 * models, into an image and exits. Combined with QT_QPA_PLATFORM=offscreen
	 * it never touches a display, so it can run on a CI runner and be diffed:
	 * this is what visual regression checks of the QML UI are built on.
	 *
	 * The delay lets layouts settle and asynchronously loaded images arrive;
	 * grabWindow() then forces a render of whatever is current. */
	const QString path = qEnvironmentVariable("MESHMC_QML_SNAPSHOT");
	if (path.isEmpty())
		return;

	/* Long enough for the first frame; MESHMC_QML_SNAPSHOT_DELAY (ms) waits
	 * longer, for pages that show network results. */
	bool delayOk = false;
	const int delay =
		qEnvironmentVariableIntValue("MESHMC_QML_SNAPSHOT_DELAY", &delayOk);
	QTimer::singleShot(delayOk ? delay : 750, this, [this, path]() {
		const QImage image = m_window->grabWindow();
		const bool saved = !image.isNull() && image.save(path);
		if (saved)
			qInfo() << "QML shell: snapshot written to" << path << image.size();
		else
			qCritical() << "QML shell: could not write snapshot to" << path;
		QCoreApplication::exit(saved ? 0 : 1);
	});
}
