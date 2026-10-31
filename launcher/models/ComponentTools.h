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
#include <QStringList>
#include <QUrl>
#include <memory>

class MinecraftInstance;
class PackProfile;

/*
 * The Version tab's less common actions for QML: add an empty component, add
 * jar mods / replace the main jar, download all again, edit a component's JSON,
 * open folders. Created lazily by InstanceDetails::componentTools(). Whether the
 * game is running is the caller's to check; nothing here guards beyond what
 * PackProfile refuses.
 */
class ComponentTools : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit ComponentTools(std::shared_ptr<MinecraftInstance> instance,
							QObject* parent = nullptr);
	~ComponentTools() override;

	QString error() const
	{
		return m_error;
	}

	/// "My Mod!" -> "io.github.grxtor.custom.mymod": offered when the uid field is
	/// empty; empty if @p name has no letters.
	Q_INVOKABLE static QString suggestUid(const QString& name);

	/// @p uid may be empty (suggestUid() is used). Empty string on success, else what is wrong.
	Q_INVOKABLE QString addEmpty(const QString& name, const QString& uid);

	/// The first problem with a new component, or empty; public so the dialog can check as the user types.
	Q_INVOKABLE static QString validateNewComponent(
		const QString& name, const QString& uid, const QStringList& existingUids);

	Q_INVOKABLE QStringList componentUids() const;

	Q_INVOKABLE bool addJarMods(const QList<QUrl>& files);

	Q_INVOKABLE bool replaceMainJar(const QUrl& file);

	/// TaskWatcher, or null with error() set (no valid account, nothing to update).
	Q_INVOKABLE QObject* downloadAll();

	Q_INVOKABLE bool hasJsonFile(int row) const;

	Q_INVOKABLE bool editComponentJson(int row);

	Q_INVOKABLE void openLibrariesFolder() const;
	Q_INVOKABLE void openGameFolder() const;

	static QStringList existingFiles(const QList<QUrl>& urls);

  signals:
	void errorChanged();

  private:
	void setError(const QString& error);
	PackProfile* profile() const;

	std::shared_ptr<MinecraftInstance> m_instance;
	QString m_error;
};
