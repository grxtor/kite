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
#include <QStringList>
#include <QVariantList>

#include "BaseInstance.h"

/*
 * "Create shortcut…" for one instance, for the QML dialog. A shortcut can go on
 * the desktop, into the applications folder or anywhere a save dialog picks; it
 * can join a server on launch and play as a given account. Joining a world is
 * left out: no instance announces the quick-play support it needs yet. The
 * writing is InstanceShortcuts (core).
 */
class InstanceShortcutController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
	Q_PROPERTY(QString instanceName READ instanceName CONSTANT)
	/// [{value, label}]: "desktop" and "applications" where writable, then always "other" (save dialog).
	Q_PROPERTY(QVariantList locations READ locations CONSTANT)
	/// ".desktop", ".lnk"; empty on macOS.
	Q_PROPERTY(QString fileSuffix READ fileSuffix CONSTANT)
	Q_PROPERTY(QStringList accounts READ accounts CONSTANT)
	Q_PROPERTY(QString defaultAccount READ defaultAccount CONSTANT)
	Q_PROPERTY(QString lastPath READ lastPath NOTIFY lastPathChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit InstanceShortcutController(InstancePtr instance,
										QObject* parent = nullptr);

	QString instanceId() const;
	QString instanceName() const;
	QVariantList locations() const;
	QString fileSuffix() const;
	QStringList accounts() const;
	QString defaultAccount() const;
	QString lastPath() const
	{
		return m_lastPath;
	}
	QString error() const
	{
		return m_error;
	}

	/// The name a shortcut gets when the user types none: the instance's,
	/// plus the server's address when it joins one.
	Q_INVOKABLE QString suggestedName(const QString& server) const;
	/// A file:// URL on the desktop called @p name.
	Q_INVOKABLE QString suggestedFileUrl(const QString& name) const;
	/// @p location: "desktop", "applications", or a file:// URL or path. @p name,
	/// @p server (address to join) and @p account (profile to play as) are
	/// optional. False with `error` set.
	Q_INVOKABLE bool create(const QString& name, const QString& location,
							const QString& server, const QString& account);
	Q_INVOKABLE void showInFolder() const;

  signals:
	void lastPathChanged();
	void errorChanged();

  private:
	void setError(const QString& error);

	InstancePtr m_instance;
	QString m_lastPath;
	QString m_error;
};
