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
#include "models/InstanceFilterModel.h"
#include "qml/InstanceIconProvider.h"
#include "core/LauncherContext.h"

namespace
{
	/* Spelled out rather than loadFromModule(), which is Qt 6.5+; the floor is
	 * 6.4. The module sets RESOURCE_PREFIX "/qt/qml" so this path is stable. */
	const QUrl kRootUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml"));
} // namespace

QmlShell::QmlShell(QObject* parent) : QObject(parent) {}

QmlShell::~QmlShell() = default;

QObject* QmlShell::expose(QObject* object)
{
	if (object) {
		QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
	}
	return object;
}

QVariantMap QmlShell::rootProperties() const
{
	QVariantMap props;
	props.insert(QStringLiteral("instanceModel"),
				 QVariant::fromValue(expose(m_instances.get())));
	props.insert(QStringLiteral("selection"),
				 QVariant::fromValue(expose(m_selection.get())));
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
	m_selection = std::make_unique<IdSelectionModel>();

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

	QTimer::singleShot(750, this, [this, path]() {
		const QImage image = m_window->grabWindow();
		const bool saved = !image.isNull() && image.save(path);
		if (saved)
			qInfo() << "QML shell: snapshot written to" << path << image.size();
		else
			qCritical() << "QML shell: could not write snapshot to" << path;
		QCoreApplication::exit(saved ? 0 : 1);
	});
}
