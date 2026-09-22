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
#include <QVariantMap>

#include <map>
#include <memory>

class IdSelectionModel;
class InstanceFilterModel;
class SettingsAdapter;
class QQmlApplicationEngine;
class QQuickWindow;

/*
 * The QML user interface: owns the engine, hands the core's models to it and
 * loads the root window.
 *
 * Everything the QML side sees is passed in as a required property of the
 * root window rather than set as a context property. That keeps each object's
 * type visible to the QML tooling, makes a missing one a load error instead of
 * a silent undefined, and leaves exactly one place -- expose() -- where
 * ownership is decided.
 */
class QmlShell : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
	Q_PROPERTY(QString accountKind READ accountKind NOTIFY accountChanged)
	/// Image url of the default account's skin face; empty without one.
	Q_PROPERTY(QString accountFace READ accountFace NOTIFY accountChanged)
	Q_PROPERTY(int accountCount READ accountCount NOTIFY accountChanged)
	/// The launcher's settings, as a SettingsAdapter.
	Q_PROPERTY(QObject* settings READ settings CONSTANT)
	/// Installed memory in MiB: the ceiling for the memory settings.
	Q_PROPERTY(int systemMemoryMiB READ systemMemoryMiB CONSTANT)
	/// Every instance, most recently played first; never-played ones left out.
	Q_PROPERTY(QObject* recentModel READ recentModel CONSTANT)
	/// At most one row: the instance whose id QML writes into instanceId.
	Q_PROPERTY(QObject* heroModel READ heroModel CONSTANT)

  public:
	explicit QmlShell(QObject* parent = nullptr);
	~QmlShell() override;

	/* Loads the root window on first call; raises it on later ones. Returns
	 * false if the QML failed to load, in which case the reasons have already
	 * been logged. */
	bool show(bool minimized = false);

	/* Hands a C++-owned object to QML. The engine takes ownership of any
	 * QObject without a parent that crosses into JavaScript, and would delete
	 * core models out from under the rest of the launcher; this pins them. */
	static QObject* expose(QObject* object);

	/* Sidebar account summary, read from LAUNCHER->accounts() - QmlShell has
	 * no reason to go through Application for this, and reaching it via the
	 * core keeps QmlShell usable without one. */
	QString accountName() const;
	/// "Microsoft", "Offline", or empty when there is no default account.
	QString accountKind() const;
	int accountCount() const;
	QString accountFace() const;

	QObject* settings() const;
	int systemMemoryMiB() const;
	QObject* recentModel() const;
	QObject* heroModel() const;
	/* The instances of one group (empty = ungrouped) that pass the search,
	 * for one section of the library. Created on first use and kept, so
	 * QML asking again from a rebuilt delegate gets the same model back. */
	Q_INVOKABLE QObject* sectionModel(const QString& group);

	/* Everything below just emits the matching *Requested() signal: QmlShell
	 * sits in MeshMC_qml, which cannot see the widget code that actually
	 * launches an instance, opens a dialog or shows a folder. Application
	 * connects these to the real actions. */
	Q_INVOKABLE void launchInstance(const QString& id);
	Q_INVOKABLE void killInstance(const QString& id);
	Q_INVOKABLE void editInstance(const QString& id);
	Q_INVOKABLE void openInstanceFolder(const QString& id);
	Q_INVOKABLE void createInstance();
	/// @p page: a classic settings page id ("accounts", "proxy-settings",
	/// ...) to open on, or empty for the first one.
	Q_INVOKABLE void openSettings(const QString& page = QString());
	/// Opens a folder in the file manager; relative paths are resolved
	/// against the data folder, which is the working directory.
	Q_INVOKABLE void openPath(const QString& path);
	Q_INVOKABLE void manageAccounts();

  signals:
	/* Emitted when the user closes the root window. */
	void closed();

	/// accountName()/accountKind()/accountCount() moved.
	void accountChanged();

	void launchRequested(const QString& id);
	void killRequested(const QString& id);
	void editRequested(const QString& id);
	void folderRequested(const QString& id);
	void createInstanceRequested();
	void settingsRequested(const QString& page);
	void accountsRequested();

  private:
	QVariantMap rootProperties();
	void scheduleSnapshotIfRequested();

	/* Declared before the engine so they are destroyed after it: QML holds
	 * pointers to both until the engine is gone. */
	std::unique_ptr<InstanceFilterModel> m_instances;
	std::unique_ptr<InstanceFilterModel> m_recent;
	std::unique_ptr<InstanceFilterModel> m_hero;
	// Declared after m_instances, their source, so they are destroyed first.
	std::map<QString, std::unique_ptr<InstanceFilterModel>> m_sections;
	std::unique_ptr<IdSelectionModel> m_selection;
	std::unique_ptr<SettingsAdapter> m_settings;

	std::unique_ptr<QQmlApplicationEngine> m_engine;
	QQuickWindow* m_window = nullptr;
	int m_accountRevision = 0;
};
