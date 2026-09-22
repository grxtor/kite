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

#include <QObject>
#include <QString>
#include <memory>

#include "BaseInstance.h"
#include "QObjectPtr.h"

class MinecraftInstance;
class ModFolderModel;
class WorldList;
class SettingsAdapter;
class ScreenshotListModel;
class InstanceLogBridge;
class LaunchTask;
class LogModel;

/*
 * QML-facing bridge for one instance's detail page: notes, mods, worlds,
 * the live log, installed components (loader + Minecraft version) and
 * per-instance settings overrides, all in one object so QmlShell only has
 * to hand QML a single thing per open instance.
 *
 * Holds the InstancePtr itself (a shared_ptr): that is what keeps this
 * bridge from outliving the instance in a way that crashes, since the
 * instance simply cannot be destroyed while this bridge is still holding a
 * reference to it. Everything this bridge exposes that it did not create
 * itself (the mod/world/component models) is owned by the instance and
 * only borrowed here - the destructor stops watching them, but never
 * deletes them.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class InstanceDetails : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
	Q_PROPERTY(QString name READ name NOTIFY nameChanged)
	Q_PROPERTY(QString gameRoot READ gameRoot CONSTANT)
	Q_PROPERTY(QString instanceRoot READ instanceRoot CONSTANT)
	/// SettingsAdapter over this instance's own SettingsObject.
	Q_PROPERTY(QObject* settings READ settings CONSTANT)
	Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY notesChanged)

	/// The loader mods ModFolderModel (same one ModFolderPage's
	/// loaderModList() tab shows), or null if this instance is not
	/// Minecraft-backed.
	Q_PROPERTY(QObject* mods READ mods CONSTANT)
	Q_PROPERTY(QString modsDir READ modsDir CONSTANT)
	/// False while the instance is running - same rule ModFolderPage
	/// enforces for the Mods tab specifically (contentChangesAllowed()).
	Q_PROPERTY(bool contentChangesAllowed READ contentChangesAllowed NOTIFY
				   contentChangesAllowedChanged)

	Q_PROPERTY(QObject* worlds READ worlds CONSTANT)
	Q_PROPERTY(QString worldsDir READ worldsDir CONSTANT)

	/// ScreenshotListModel over the game's screenshots folder.
	Q_PROPERTY(QObject* screenshots READ screenshots CONSTANT)
	Q_PROPERTY(QString screenshotsDir READ screenshotsDir CONSTANT)

	/// InstanceLogBridge for the current (or most recent) launch.
	Q_PROPERTY(QObject* log READ log CONSTANT)
	/// This instance's PackProfile, read-only version list, or null.
	Q_PROPERTY(QObject* components READ components CONSTANT)
	Q_PROPERTY(bool isMinecraft READ isMinecraft CONSTANT)

  public:
	explicit InstanceDetails(InstancePtr instance, QObject* parent = nullptr);
	~InstanceDetails() override;

	QString instanceId() const;
	QString name() const;
	QString gameRoot() const;
	QString instanceRoot() const;
	QObject* settings() const;

	QString notes() const;
	void setNotes(const QString& notes);

	QObject* mods() const;
	QString modsDir() const;
	Q_INVOKABLE void setModEnabled(int row, bool enabled);
	Q_INVOKABLE void deleteMod(int row);
	/// @p fileUrlOrPath: a file:// URL (as a QML FileDialog hands out) or
	/// a plain local path.
	Q_INVOKABLE bool installMod(const QString& fileUrlOrPath);
	bool contentChangesAllowed() const;

	QObject* worlds() const;
	QString worldsDir() const;
	Q_INVOKABLE void deleteWorld(int row);

	QObject* screenshots() const;
	QString screenshotsDir() const;

	QObject* log() const;
	QObject* components() const;
	bool isMinecraft() const;

  signals:
	void nameChanged();
	void notesChanged();
	void contentChangesAllowedChanged();

  private slots:
	void onRunningStatusChanged(bool running);

  private:
	InstancePtr m_instance;
	/// Non-owning; valid for as long as m_instance is (which is for the
	/// lifetime of this object). Null when the instance is not Minecraft.
	MinecraftInstance* m_mc = nullptr;

	/// Borrowed from the instance, not created here - see the class
	/// comment.
	std::shared_ptr<ModFolderModel> m_mods;
	std::shared_ptr<WorldList> m_worlds;

	std::unique_ptr<SettingsAdapter> m_settings;
	std::unique_ptr<ScreenshotListModel> m_screenshots;
	/// Owned via QObject parentage (parent is `this`).
	InstanceLogBridge* m_log = nullptr;
};

/*
 * QML-facing view of "the current LaunchTask's LogModel", if any.
 *
 * Follows LogPage::setInstanceLaunchTaskChanged: a LaunchTask (and its
 * LogModel) only exists while a launch is in flight, so model() is null
 * between launches and re-points at a fresh LogModel every time
 * BaseInstance starts a new one.
 */
class InstanceLogBridge : public QObject
{
	Q_OBJECT

	/// The current LaunchTask's LogModel, or null when the instance is
	/// not currently launching/running.
	Q_PROPERTY(QObject* model READ model NOTIFY modelChanged)
	Q_PROPERTY(bool hasLog READ hasLog NOTIFY modelChanged)

  public:
	explicit InstanceLogBridge(InstancePtr instance, QObject* parent = nullptr);
	~InstanceLogBridge() override;

	QObject* model() const;
	bool hasLog() const;

	Q_INVOKABLE void clear();
	/// The whole log as plain text, e.g. for a QML copy action.
	Q_INVOKABLE QString text();
	Q_INVOKABLE void setSuspended(bool suspended);

  signals:
	void modelChanged();

  private slots:
	void onLaunchTaskChanged(shared_qobject_ptr<LaunchTask> task);

  private:
	InstancePtr m_instance;
	shared_qobject_ptr<LaunchTask> m_task;
	shared_qobject_ptr<LogModel> m_model;
};
