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

#include <QString>
#include <cstdint>
#include <memory>

#include "QObjectPtr.h"

class AccountList;
class AuthRequestDecorator;
class HttpMetaCache;
class IconList;
class InstanceList;
class JavaInstallList;
class ModBridge;
class NewsChecker;
class NotificationChecker;
class QNetworkAccessManager;
class SettingsObject;
class TranslationsModel;
class UiHost;

namespace Meta
{
	class Index;
}

/* The launcher services that code outside the UI may reach for. `Application`
 * implements this and registers itself, so the core uses LAUNCHER-> instead of
 * APPLICATION-> and does not drag in QtWidgets. Nothing here may mention
 * QtWidgets or return a widget, window or dialog; questions to the user go
 * through UiHost. */
class LauncherContext
{
  public:
	virtual ~LauncherContext() = default;

	/* Null before Application's constructor has run and after it was
	 * destroyed, notably in the crash handler during teardown. Callers on
	 * those paths must check. */
	static LauncherContext* instance();

	virtual std::shared_ptr<SettingsObject> settings() const = 0;
	virtual std::shared_ptr<InstanceList> instances() const = 0;
	virtual std::shared_ptr<IconList> icons() const = 0;
	virtual shared_qobject_ptr<AccountList> accounts() const = 0;
	/* Not const: javalist() builds its JavaInstallList lazily on first call.
	 * Needed by QmlShell (Kite_qml links this core, not Application) for
	 * onboarding. */
	virtual std::shared_ptr<TranslationsModel> translations() = 0;
	virtual std::shared_ptr<JavaInstallList> javalist() = 0;

	virtual shared_qobject_ptr<QNetworkAccessManager> network() = 0;
	virtual shared_qobject_ptr<HttpMetaCache> metacache() = 0;
	virtual shared_qobject_ptr<Meta::Index> metadataIndex() = 0;

	virtual QString getJarsPath() = 0;
	virtual QString msaClientId() const = 0;

	/* Applies a proxy configuration to the whole application at once
	 * (QNetworkProxy::setApplicationProxy() and friends). @p proxyTypeStr is one of "None", "Default",
	 * "SOCKS5", "HTTP". Needed by QmlShell, which cannot see Application. */
	virtual void updateProxySettings(QString proxyTypeStr, QString addr,
									 int port, QString user,
									 QString password) = 0;

	/* Null when no plugin host is present (plugins are an optional build,
	 * Kite_PLUGINS). Core code calls through this because PluginManager
	 * renders plugin UI and pulls in QtWidgets. */
	virtual AuthRequestDecorator* authRequestDecorator() const = 0;

	/* Never null: a core task that must ask the user cannot carry on without
	 * an answer, so the shell installs one before anything that might ask
	 * runs. */
	virtual UiHost* uiHost() const = 0;

	/* The launcher's news feeds, owned by the application and shared by the QML
	 * shell and the plugin news API. Null in a test or tool context, which is
	 * why this is not pure. */
	virtual NewsChecker* newsChecker() const
	{
		return nullptr;
	}
	/* The bridge the Kite mod of a running game talks to the launcher through.
	 * Null in a test or tool context. */
	virtual ModBridge* modBridge() const
	{
		return nullptr;
	}
	/* Hands @p payload to the plugins registered for @p hookId (PluginHooks.h)
	 * and says whether one cancelled. Core code calls through this because
	 * PluginManager pulls in QtWidgets. Nothing happens without a plugin host
	 * or in a test or tool context, which is why this is not pure. */
	virtual bool dispatchPluginHook(uint32_t /*hookId*/, void* /*payload*/)
	{
		return false;
	}

  protected:
	/* Called by the implementation's constructor/destructor. Registering is
	 * not thread safe: once, on the main thread, before anything else runs. */
	static void setInstance(LauncherContext* context);
};

#define LAUNCHER (LauncherContext::instance())
