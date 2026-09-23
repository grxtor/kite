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

#include <QIcon>
#include <QString>
#include <memory>

#include "QObjectPtr.h"

class AccountList;
class AuthRequestDecorator;
class HttpMetaCache;
class IconList;
class InstanceList;
class JavaInstallList;
class QNetworkAccessManager;
class SettingsObject;
class TranslationsModel;
class UiHost;

namespace Meta
{
	class Index;
}

/*
 * The launcher services that code outside the user interface is allowed to
 * reach for.
 *
 * Everything here used to be read off `Application`, which derives from
 * QApplication and owns the main window, the settings dialog and the theme
 * manager. That made every file that wanted, say, the network manager depend
 * transitively on QtWidgets and on the entire widget page tree -- which is
 * exactly what stops the core from being reused under a QML user interface.
 *
 * So the dependency is inverted: `Application` implements this interface and
 * registers itself, and the core reaches services through LAUNCHER-> instead
 * of APPLICATION->. Nothing declared here is allowed to mention QtWidgets, and
 * nothing here returns a widget, a window or a dialog. Asking the user a
 * question is not a service -- that inversion belongs to UiHost.
 *
 * QIcon is fine despite appearances: it lives in QtGui, not QtWidgets. QML
 * cannot consume it, so getThemedIcon() is a transitional accessor that the
 * image-provider work will eventually replace with icon names.
 */
class LauncherContext
{
  public:
	virtual ~LauncherContext() = default;

	/* Null before Application's constructor has run and after it has been
	 * destroyed -- notably in the crash handler, which runs while the
	 * application object is being torn down. Callers on those paths must
	 * check. */
	static LauncherContext* instance();

	virtual std::shared_ptr<SettingsObject> settings() const = 0;
	virtual std::shared_ptr<InstanceList> instances() const = 0;
	virtual std::shared_ptr<IconList> icons() const = 0;
	virtual shared_qobject_ptr<AccountList> accounts() const = 0;
	/* Not const: Application::translations() is loaded once during startup
	 * and just returns it, but Application::javalist() builds its
	 * JavaInstallList lazily on first call. Needed by QmlShell (MeshMC_qml,
	 * which links this core and not Application/MeshMC_logic) for the QML
	 * shell's own onboarding -- see QmlShell::languages()/javaInstalls(). */
	virtual std::shared_ptr<TranslationsModel> translations() = 0;
	virtual std::shared_ptr<JavaInstallList> javalist() = 0;

	virtual shared_qobject_ptr<QNetworkAccessManager> network() = 0;
	virtual shared_qobject_ptr<HttpMetaCache> metacache() = 0;
	virtual shared_qobject_ptr<Meta::Index> metadataIndex() = 0;

	virtual QIcon getThemedIcon(const QString& name) = 0;
	virtual QString getJarsPath() = 0;
	virtual QString msaClientId() const = 0;

	/* Applies a proxy configuration to the whole application immediately
	 * (QNetworkProxy::setApplicationProxy() and friends), the same way the
	 * widget ProxyPage's apply button does. @p proxyTypeStr is one of
	 * "None", "Default", "SOCKS5", "HTTP". Needed by QmlShell, which
	 * cannot see Application/QNetworkProxy from MeshMC_qml -- see
	 * QmlShell::applyProxySettings(). */
	virtual void updateProxySettings(QString proxyTypeStr, QString addr,
									 int port, QString user,
									 QString password) = 0;

	/* Null when no plugin host is present -- plugins are an optional build
	 * (MeshMC_PLUGINS, OFF by default). Core code calls through this rather
	 * than reaching PluginManager directly, because PluginManager renders
	 * plugin-supplied user interface and therefore pulls in QtWidgets. */
	virtual AuthRequestDecorator* authRequestDecorator() const = 0;

	/* Never null: a core task that has to ask the user a question cannot
	 * meaningfully carry on without an answer, so the shell installs one
	 * before anything that might ask is allowed to run. */
	virtual UiHost* uiHost() const = 0;

  protected:
	/* Called by the implementation's constructor/destructor. Registering is
	 * not thread safe and is expected to happen once, on the main thread,
	 * before anything else runs. */
	static void setInstance(LauncherContext* context);
};

#define LAUNCHER (LauncherContext::instance())
