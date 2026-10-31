/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "plugin/PluginManager.h"
#include "plugin/PluginDependencyResolver.h"
#include "plugin/PluginSignature.h"
#include "Application.h"
#include "core/UiHost.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "BaseInstance.h"
#include "MMCZip.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Component.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/WorldList.h"
#include "minecraft/World.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/services/SkinUpload.h"
#include "minecraft/services/SkinDelete.h"
#include "minecraft/services/CapeChange.h"
#include "plugin/PluginHookTask.h"
#include "tasks/SequentialTask.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/pages/instance/InstanceSettingsPage.h"
#include "icons/IconList.h"
#include "icons/MMCIcon.h"
#include "java/JavaInstallList.h"
#include "java/JavaInstall.h"
#include "settings/SettingsObject.h"
#include "Logging.h"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDebug>
#include <QInputDialog>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include "news/NewsChecker.h"
#include "ui/MainWindow.h"
#include <net/NetJob.h>
#include "net/Download.h"
#include <QPushButton>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QHeaderView>
#include <QAction>
#include <QCloseEvent>
#include <QWindow>
#include <QCursor>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QMutexLocker>
#include <QProcess>
#include <QDialog>
#include <QFrame>
#include <QGroupBox>
#include <QJsonDocument>
#include <QScrollArea>
#include "plugin/PluginUiRenderer.h"
#include <cstring>
#include <unordered_map>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

PluginManager::PluginManager(Application* app, QObject* parent)
	: QObject(parent), m_app(app)
{
}

PluginManager::~PluginManager()
{
	shutdownAll();
}

void PluginManager::initializeAll()
{
	qCDebug(pluginsLog) << "Discovering modules...";

	// Keyring path and verification cache feed the signature pre-flight in discoverModules().
	if (m_app && m_app->settings()) {
		const QString key = QStringLiteral("plugin.signing.keyring_path");
		if (!m_app->settings()->contains(key))
			m_app->settings()->registerSetting(key, QString());
		const QString path = m_app->settings()->get(key).toString();
		if (!path.isEmpty())
			PluginSignature::setKeyringPath(path);
	}

	// The cache is keyed by (size, mtime) of each .mmco and skips the GpgME round-trip.
	{
		const QString cacheDir = QStandardPaths::writableLocation(
			QStandardPaths::AppLocalDataLocation);
		if (!cacheDir.isEmpty()) {
			PluginSignature::setCachePath(QDir(cacheDir).filePath(
				QStringLiteral("plugin-signature-cache.json")));
		}
	}

	const QSet<QString> disabled = disabledModuleNames();
	m_modules = m_loader.discoverModules(disabled);
	PluginSignature::flushCache();

	if (m_modules.isEmpty()) {
		qCDebug(pluginsLog) << "No modules found.";
		return;
	}

	const auto resolved = PluginDependencyResolver::resolve(m_modules);

	// Sized to the full module list so indexes match m_modules.
	m_runtimes.resize(static_cast<size_t>(m_modules.size()));
	m_contexts.resize(static_cast<size_t>(m_modules.size()));

	for (int i : resolved.loadOrder) {
		auto& meta = m_modules[i];

		if (meta.disabled) {
			qCDebug(pluginsLog) << "Skipping disabled module:" << meta.name
					 << "-" << meta.disableDetail;
			continue;
		}

		ensurePluginDataDir(meta);

		auto runtime = std::make_unique<ModuleRuntime>();
		runtime->manager = this;
		runtime->moduleIndex = i;
		runtime->dataDir = meta.dataDir.toStdString();
		m_runtimes[i] = std::move(runtime);

		m_contexts[i] = buildContext(meta);
		m_contexts[i].module_handle = m_runtimes[i].get();

		qCDebug(pluginsLog) << "Initializing module:" << meta.name;
		int rc = meta.initFunc(&m_contexts[i]);
		if (rc != 0) {
			qCWarning(pluginsLog) << "Module" << meta.name
					   << "mmco_init() returned" << rc << "- skipping";
			emit moduleError(meta.name,
							 QString("mmco_init returned %1").arg(rc));
			PluginLoader::unloadModule(meta);
			continue;
		}

		meta.initialized = true;
		qCDebug(pluginsLog) << "Module" << meta.name
				 << "initialized successfully";
		emit moduleLoaded(meta.name);
	}

	for (const auto& meta : m_modules) {
		if (meta.disabled) {
			qCInfo(pluginsLog).noquote() << "Module" << meta.name
							  << "not loaded:" << meta.disableDetail;
		}
	}

	// Before APP_INITIALIZED, so hooks registered in mmco_init() see the first dispatch.
	connectAppSignals();

	dispatchHook(MMCO_HOOK_APP_INITIALIZED);
}

bool PluginManager::isModuleDisabled(const QString& moduleName) const
{
	return disabledModuleNames().contains(moduleName.toLower());
}

void PluginManager::setModuleDisabled(const QString& moduleName, bool disabled)
{
	if (!m_app || !m_app->settings() || moduleName.isEmpty())
		return;

	const QString key = QStringLiteral("plugins.disabled");
	if (!m_app->settings()->contains(key))
		m_app->settings()->registerSetting(key, QString());

	QSet<QString> current = disabledModuleNames();
	const QString lname = moduleName.toLower();
	if (disabled)
		current.insert(lname);
	else
		current.remove(lname);

	QStringList list(current.begin(), current.end());
	list.sort();
	m_app->settings()->set(key, list.join(QLatin1Char(',')));
}

QSet<QString> PluginManager::disabledModuleNames() const
{
	QSet<QString> out;
	if (!m_app || !m_app->settings())
		return out;
	const QString key = QStringLiteral("plugins.disabled");

	// contains() only sees registered settings: register the key first, or the saved disable list
	// would be ignored after a restart.
	if (!m_app->settings()->contains(key)) {
		m_app->settings()->registerSetting(key, QString());
	}

	const QString raw = m_app->settings()->get(key).toString();
	for (const QString& part :
		 raw.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
		const QString trimmed = part.trimmed().toLower();
		if (!trimmed.isEmpty())
			out.insert(trimmed);
	}
	return out;
}

void PluginManager::shutdownAll()
{
	if (m_shutdownDone)
		return;
	m_shutdownDone = true;

	dispatchHook(MMCO_HOOK_APP_SHUTDOWN);

	for (int i = m_modules.size() - 1; i >= 0; --i) {
		auto& meta = m_modules[i];
		if (!meta.initialized)
			continue;

		qCDebug(pluginsLog).noquote() << "Unloading module:" << meta.name;
		// Release tray resources before mmco_unload(); the plugin must not touch them afterwards.
		releaseTrayResourcesForModule(m_runtimes[i].get());
		if (meta.unloadFunc) {
			meta.unloadFunc();
		}
		meta.initialized = false;
	}

	// Unhook the close filter if the main window outlives PluginManager.
	if (m_closeFilterInstalled) {
		if (m_filteredMainWindow)
			m_filteredMainWindow->removeEventFilter(this);
		if (m_filteredShellWindow)
			m_filteredShellWindow->removeEventFilter(this);
		m_closeFilterInstalled = false;
	}

	// No dlclose() or clear() here: plugins link Kite_logic statically and are opened with
	// RTLD_NODELETE so its static destructors do not run at exit; unmapping would also break the
	// MOC statics of plugin Q_OBJECT classes. The OS reclaims the memory.
}

bool PluginManager::dispatchHook(uint32_t hook_id, void* payload)
{
	// Snapshot: callbacks may (un)register hooks, which would invalidate iterators into m_hooks.
	QVector<HookRegistration> inlineRegs;
	QVector<HookRegistration> backgroundRegs;
	{
		auto range = m_hooks.equal_range(hook_id);
		for (auto it = range.first; it != range.second; ++it) {
			if (it.value().flags & MMCO_HOOK_FLAG_BACKGROUND) {
				backgroundRegs.append(it.value());
			} else {
				inlineRegs.append(it.value());
			}
		}
	}

	// Inline callbacks first: they are cheap, and a veto skips the background work.
	for (const auto& reg : inlineRegs) {
		int rc =
			reg.callback(reg.module_handle, hook_id, payload, reg.user_data);
		if (rc != 0) {
			return true; // cancelled
		}
	}

	if (backgroundRegs.isEmpty()) {
		return false;
	}

	return runBackgroundHooks(hook_id, payload, backgroundRegs);
}

bool PluginManager::runBackgroundHooks(uint32_t hook_id, void* payload,
									   const QVector<HookRegistration>& regs)
{
	// Off-loading needs an event loop and a window; at startup and shutdown the callbacks run
	// synchronously. activeWindow() is null when the launcher is unfocused, so the main window
	// tells whether a UI exists (resolveMainWindow() returns nullptr under the QML shell).
	QWidget* owner = QApplication::activeWindow();
	if (!owner) {
		owner = resolveMainWindow();
	}
	// The QML shell has no QWidget to parent a ProgressDialog; it waits behind
	// uiHost()->showBusy().
	const bool qmlProgress = !m_shutdownDone && m_app && m_app->qmlShellChosen() &&
							 m_app->usingQmlShell();
	if (!qmlProgress && (!owner || m_shutdownDone)) {
		for (const auto& reg : regs) {
			int rc = reg.callback(reg.module_handle, hook_id, payload,
								  reg.user_data);
			if (rc != 0) {
				return true;
			}
		}
		return false;
	}

	SequentialTask sequence(nullptr, QStringLiteral("PluginHooks"));
	QVector<PluginHookTask*> tasks;
	tasks.reserve(regs.size());

	for (const auto& reg : regs) {
		auto* r = rt(reg.module_handle);
		const QString name = (r && r->moduleIndex >= 0 &&
							  r->moduleIndex < m_modules.size())
								 ? m_modules[r->moduleIndex].name
								 : QStringLiteral("plugin");

		auto task = shared_qobject_ptr<PluginHookTask>(
			new PluginHookTask(name, reg.module_handle, hook_id, reg.callback,
							   payload, reg.user_data));
		tasks.append(task.get());
		sequence.addTask(task);
	}

	if (qmlProgress) {
		auto busy = m_app->uiHost()->showBusy(tr("Waiting for plugins…"));
		QEventLoop loop;
		connect(&sequence, &Task::finished, &loop, &QEventLoop::quit);
		// Queued, so the loop is running before the sequence can finish.
		QMetaObject::invokeMethod(&sequence, &Task::start,
								  Qt::QueuedConnection);
		loop.exec();
	} else {
		ProgressDialog prog(owner);
		prog.execWithTask(&sequence);
	}

	// Escape closes the dialog but a C callback cannot be interrupted; keep waiting, since
	// destroying the tasks while a worker runs would block the GUI thread.
	if (sequence.isRunning()) {
		QEventLoop loop;
		connect(&sequence, &Task::finished, &loop, &QEventLoop::quit);
		loop.exec();
	}

	// A vetoing task fails the sequence; the tasks after it never ran.
	for (auto* task : tasks) {
		if (task->cancelRequested()) {
			return true;
		}
	}
	return false;
}

MMCOContext PluginManager::buildContext(PluginMetadata& meta)
{
	MMCOContext ctx{};
	ctx.struct_size = sizeof(MMCOContext);
	ctx.abi_version = MMCO_ABI_VERSION;
	ctx.module_handle = nullptr; // set by caller

	// S1 — Logging
	ctx.log_info = api_log_info;
	ctx.log_warn = api_log_warn;
	ctx.log_error = api_log_error;
	ctx.log_debug = api_log_debug;

	// S2 — Hooks
	ctx.hook_register = api_hook_register;
	ctx.hook_unregister = api_hook_unregister;

	// S32 — Background hooks + progress reporting
	ctx.hook_register_ex = api_hook_register_ex;
	ctx.progress_report = api_progress_report;

	// S3 — Settings
	ctx.setting_get = api_setting_get;
	ctx.setting_set = api_setting_set;

	// S4 — Instance Management
	ctx.instance_count = api_instance_count;
	ctx.instance_get_id = api_instance_get_id;
	ctx.instance_get_name = api_instance_get_name;
	ctx.instance_set_name = api_instance_set_name;
	ctx.instance_get_path = api_instance_get_path;
	ctx.instance_get_game_root = api_instance_get_game_root;
	ctx.instance_get_mods_root = api_instance_get_mods_root;
	ctx.instance_get_icon_key = api_instance_get_icon_key;
	ctx.instance_set_icon_key = api_instance_set_icon_key;
	ctx.instance_get_type = api_instance_get_type;
	ctx.instance_get_notes = api_instance_get_notes;
	ctx.instance_set_notes = api_instance_set_notes;
	ctx.instance_is_running = api_instance_is_running;
	ctx.instance_can_launch = api_instance_can_launch;
	ctx.instance_has_crashed = api_instance_has_crashed;
	ctx.instance_has_update = api_instance_has_update;
	ctx.instance_set_update_available = api_instance_set_update_available;
	ctx.instance_component_set_version = api_instance_component_set_version;
	ctx.http_get_with_headers = api_http_get_with_headers;
	ctx.instance_get_total_play_time = api_instance_get_total_play_time;
	ctx.instance_get_last_play_time = api_instance_get_last_play_time;
	ctx.instance_get_last_launch = api_instance_get_last_launch;
	ctx.instance_launch = api_instance_launch;
	ctx.instance_kill = api_instance_kill;
	ctx.instance_delete = api_instance_delete;
	ctx.instance_get_group = api_instance_get_group;
	ctx.instance_set_group = api_instance_set_group;
	ctx.instance_group_count = api_instance_group_count;
	ctx.instance_group_at = api_instance_group_at;
	ctx.instance_component_count = api_instance_component_count;
	ctx.instance_component_get_uid = api_instance_component_get_uid;
	ctx.instance_component_get_name = api_instance_component_get_name;
	ctx.instance_component_get_version = api_instance_component_get_version;
	ctx.instance_get_mc_version = api_instance_get_mc_version;
	ctx.instance_get_jar_mods_dir = api_instance_get_jar_mods_dir;
	ctx.instance_get_resource_packs_dir = api_instance_get_resource_packs_dir;
	ctx.instance_get_texture_packs_dir = api_instance_get_texture_packs_dir;
	ctx.instance_get_shader_packs_dir = api_instance_get_shader_packs_dir;
	ctx.instance_get_worlds_dir = api_instance_get_worlds_dir;

	// S5 — Mod Management
	ctx.mod_count = api_mod_count;
	ctx.mod_get_name = api_mod_get_name;
	ctx.mod_get_version = api_mod_get_version;
	ctx.mod_get_filename = api_mod_get_filename;
	ctx.mod_get_description = api_mod_get_description;
	ctx.mod_is_enabled = api_mod_is_enabled;
	ctx.mod_set_enabled = api_mod_set_enabled;
	ctx.mod_remove = api_mod_remove;
	ctx.mod_install = api_mod_install;
	ctx.mod_refresh = api_mod_refresh;

	// S6 — World Management
	ctx.world_count = api_world_count;
	ctx.world_get_name = api_world_get_name;
	ctx.world_get_folder = api_world_get_folder;
	ctx.world_get_seed = api_world_get_seed;
	ctx.world_get_game_type = api_world_get_game_type;
	ctx.world_get_last_played = api_world_get_last_played;
	ctx.world_delete = api_world_delete;
	ctx.world_rename = api_world_rename;
	ctx.world_install = api_world_install;
	ctx.world_refresh = api_world_refresh;

	// S7 — Account Management
	ctx.account_count = api_account_count;
	ctx.account_get_profile_name = api_account_get_profile_name;
	ctx.account_get_profile_id = api_account_get_profile_id;
	ctx.account_get_type = api_account_get_type;
	ctx.account_get_state = api_account_get_state;
	ctx.account_is_active = api_account_is_active;
	ctx.account_get_default_index = api_account_get_default_index;

	// S8 — Java Management
	ctx.java_count = api_java_count;
	ctx.java_get_version = api_java_get_version;
	ctx.java_get_arch = api_java_get_arch;
	ctx.java_get_path = api_java_get_path;
	ctx.java_is_recommended = api_java_is_recommended;
	ctx.instance_get_java_version = api_instance_get_java_version;

	// S9 — Filesystem
	ctx.fs_plugin_data_dir = api_fs_plugin_data_dir;
	ctx.fs_read = api_fs_read;
	ctx.fs_write = api_fs_write;
	ctx.fs_exists = api_fs_exists;
	ctx.fs_mkdir = api_fs_mkdir;
	ctx.fs_exists_abs = api_fs_exists_abs;
	ctx.fs_remove = api_fs_remove;
	ctx.fs_copy_file = api_fs_copy_file;
	ctx.fs_file_size = api_fs_file_size;
	ctx.fs_list_dir = api_fs_list_dir;

	// S10 — Zip
	ctx.zip_compress_dir = api_zip_compress_dir;
	ctx.zip_extract = api_zip_extract;

	// S11 — Network
	ctx.http_get = api_http_get;
	ctx.http_post = api_http_post;

	// S12 — UI Dialogs
	ctx.ui_show_message = api_ui_show_message;
	ctx.ui_add_menu_item = api_ui_add_menu_item;
	ctx.ui_file_open_dialog = api_ui_file_open_dialog;
	ctx.ui_file_save_dialog = api_ui_file_save_dialog;
	ctx.ui_input_dialog = api_ui_input_dialog;
	ctx.ui_confirm_dialog = api_ui_confirm_dialog;

	// S14 — Utility
	ctx.get_app_version = api_get_app_version;
	ctx.get_app_name = api_get_app_name;
	ctx.get_timestamp = api_get_timestamp;

	// S15 — Launch Modifiers
	ctx.launch_set_env = api_launch_set_env;
	ctx.launch_prepend_wrapper = api_launch_prepend_wrapper;

	// S16 — Application Settings
	ctx.app_setting_get = api_app_setting_get;

	// S17 — News API
	ctx.news_get_entry_count = api_news_get_entry_count;
	ctx.news_get_entry_title = api_news_get_entry_title;
	ctx.news_get_entry_link = api_news_get_entry_link;
	ctx.news_get_entry_content = api_news_get_entry_content;
	ctx.news_get_entry_author = api_news_get_entry_author;
	ctx.news_get_entry_date = api_news_get_entry_date;
	ctx.news_get_entry_feed_index = api_news_get_entry_feed_index;
	ctx.news_add_feed_url = api_news_add_feed_url;
	ctx.news_get_feed_count = api_news_get_feed_count;
	ctx.news_get_feed_url = api_news_get_feed_url;
	ctx.news_reload = api_news_reload;

	// S18 — Plugin Icon Set (ABI 2+)
	ctx.ui_plugin_icon = api_ui_plugin_icon;

	// S19 — System Tray
	ctx.tray_create = api_tray_create;
	ctx.tray_destroy = api_tray_destroy;
	ctx.tray_is_available = api_tray_is_available;
	ctx.tray_set_icon = api_tray_set_icon;
	ctx.tray_set_tooltip = api_tray_set_tooltip;
	ctx.tray_set_visible = api_tray_set_visible;
	ctx.tray_show_message = api_tray_show_message;
	ctx.tray_set_menu = api_tray_set_menu;
	ctx.tray_set_activation_cb = api_tray_set_activation_cb;

	// S20 — Main window helpers
	ctx.main_window_install_close_filter = api_main_window_install_close_filter;
	ctx.main_window_show = api_main_window_show;
	ctx.main_window_hide = api_main_window_hide;
	ctx.main_window_is_visible = api_main_window_is_visible;

	// S21 — Application Settings (write side, ABI 3+)
	ctx.app_setting_set = api_app_setting_set;
	ctx.app_setting_register = api_app_setting_register;
	ctx.app_setting_contains = api_app_setting_contains;

	// S22 — Themed icon resolution (ABI 3+)
	ctx.ui_themed_icon = api_ui_themed_icon;

	// S23 — Instance running-state signal bridge (ABI 3+)
	ctx.instance_running_register = api_instance_running_register;
	ctx.instance_running_unregister = api_instance_running_unregister;

	// S24 — Per-instance settings (ABI 3+)
	ctx.instance_setting_get = api_instance_setting_get;
	ctx.instance_setting_set = api_instance_setting_set;
	ctx.instance_setting_register = api_instance_setting_register;
	ctx.instance_setting_register_override =
		api_instance_setting_register_override;
	ctx.instance_setting_reset = api_instance_setting_reset;
	ctx.instance_setting_contains = api_instance_setting_contains;

	// S25 — Account / skin / cape access (ABI 3+)
	ctx.account_get_id_by_index = api_account_get_id_by_index;
	ctx.account_is_msa_by_id = api_account_is_msa_by_id;
	ctx.account_get_access_token = api_account_get_access_token;
	ctx.account_get_current_cape_id = api_account_get_current_cape_id;
	ctx.account_get_skin_variant = api_account_get_skin_variant;
	ctx.account_get_skin_blob = api_account_get_skin_blob;
	ctx.account_cape_count = api_account_cape_count;
	ctx.account_cape_get_id = api_account_cape_get_id;
	ctx.account_cape_get_alias = api_account_cape_get_alias;
	ctx.account_cape_get_blob = api_account_cape_get_blob;
	ctx.account_set_skin_variant = api_account_set_skin_variant;
	ctx.account_set_current_cape = api_account_set_current_cape;
	ctx.account_set_skin_blob = api_account_set_skin_blob;

	// S26 — Synchronous task helpers (ABI 3+)
	ctx.account_skin_upload = api_account_skin_upload;
	ctx.account_skin_reset = api_account_skin_reset;
	ctx.account_cape_set = api_account_cape_set;

	// S27 — Icon list enumeration (ABI 3+)
	ctx.icon_list_count = api_icon_list_count;
	ctx.icon_list_get_key = api_icon_list_get_key;
	ctx.icon_list_get_name = api_icon_list_get_name;
	ctx.icon_list_get_file_path = api_icon_list_get_file_path;
	ctx.icon_list_save_png = api_icon_list_save_png;

	// S31 — Subprocess execution
	ctx.process_run = api_process_run;

	// S33 — Declarative UI surfaces (ABI 5)
	ctx.ui_surface_create = api_ui_surface_create;
	ctx.ui_surface_update = api_ui_surface_update;
	ctx.ui_surface_set = api_ui_surface_set;
	ctx.ui_surface_set_rows = api_ui_surface_set_rows;
	ctx.ui_surface_destroy = api_ui_surface_destroy;
	ctx.ui_modal_run = api_ui_modal_run;

	return ctx;
}

void PluginManager::ensurePluginDataDir(PluginMetadata& meta)
{
	QString baseDir;
#ifdef Q_OS_WIN
	baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
	baseDir = QDir::homePath() + "/.local/share/Kite";
#endif
	meta.dataDir = QDir(baseDir).filePath("plugin-data/" + meta.moduleId());
	QDir().mkpath(meta.dataDir);
}

namespace
{
	// Thread-local backing store for ScratchString, keyed by instance. A function-local static
	// avoids cross-TU init order issues.
	std::unordered_map<const void*, std::string>& scratchStore()
	{
		static thread_local std::unordered_map<const void*, std::string> store;
		return store;
	}
} // namespace

PluginManager::ScratchString::~ScratchString()
{
	// Only the destroying thread's entry is reclaimed; other entries are inert (overwritten before
	// any getter hands them out).
	scratchStore().erase(this);
}

PluginManager::ScratchString&
PluginManager::ScratchString::operator=(std::string value)
{
	scratchStore()[this] = std::move(value);
	return *this;
}

void PluginManager::ScratchString::assign(const char* bytes, size_t size)
{
	scratchStore()[this].assign(bytes, size);
}

const char* PluginManager::ScratchString::c_str() const
{
	return scratchStore()[this].c_str();
}

const char* PluginManager::ScratchString::data() const
{
	return scratchStore()[this].data();
}

PluginManager::ModuleRuntime* PluginManager::rt(void* mh)
{
	return static_cast<ModuleRuntime*>(mh);
}

void PluginManager::api_log_info(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qInfo().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

void PluginManager::api_log_warn(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qWarning().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

void PluginManager::api_log_error(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qCCritical(pluginsLog).noquote() << meta.name << msg;
}

void PluginManager::api_log_debug(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qCDebug(pluginsLog).noquote() << meta.name << msg;
}

int PluginManager::api_hook_register(void* mh, uint32_t hook_id,
									 MMCOHookCallback cb, void* ud)
{
	auto* r = rt(mh);
	if (!cb)
		return -1;

	HookRegistration reg;
	reg.module_handle = mh;
	reg.callback = cb;
	reg.user_data = ud;
	reg.flags = MMCO_HOOK_FLAG_NONE;

	r->manager->m_hooks.insert(hook_id, reg);
	return 0;
}

int PluginManager::api_hook_register_ex(void* mh, uint32_t hook_id,
										MMCOHookCallback cb, void* ud,
										uint32_t flags)
{
	auto* r = rt(mh);
	if (!cb)
		return -1;

	// Reject unknown flags: a module built against a newer SDK must not silently get old behaviour.
	constexpr uint32_t known = MMCO_HOOK_FLAG_BACKGROUND;
	if ((flags & ~known) != 0u) {
		qWarning() << "[PluginManager] Module"
				   << r->manager->m_modules[r->moduleIndex].name
				   << "registered hook" << Qt::hex << hook_id
				   << "with unknown flags" << flags;
		return -1;
	}

	HookRegistration reg;
	reg.module_handle = mh;
	reg.callback = cb;
	reg.user_data = ud;
	reg.flags = flags;

	r->manager->m_hooks.insert(hook_id, reg);
	return 0;
}

int PluginManager::api_progress_report(void* mh, const char* status,
									   const char* details, int64_t current,
									   int64_t total)
{
	auto* task = PluginHookTask::currentOnThisThread();
	if (!task) {
		// No row to write to: inline callback, or a thread the plugin spawned.
		return -1;
	}
	if (mh && task->moduleHandle() != mh) {
		// Cannot report on behalf of another module.
		return -1;
	}

	task->reportProgress(status ? QString::fromUtf8(status) : QString(),
						 details ? QString::fromUtf8(details) : QString(),
						 current, total);
	return 0;
}

int PluginManager::api_hook_unregister(void* mh, uint32_t hook_id,
									   MMCOHookCallback cb)
{
	auto* r = rt(mh);
	auto& hooks = r->manager->m_hooks;

	auto range = hooks.equal_range(hook_id);
	for (auto it = range.first; it != range.second; ++it) {
		if (it.value().module_handle == mh && it.value().callback == cb) {
			hooks.erase(it);
			return 0;
		}
	}
	return -1;
}

const char* PluginManager::api_setting_get(void* mh, const char* key)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return nullptr;

	auto& meta = r->manager->m_modules[r->moduleIndex];
	QString fullKey =
		QString("plugin.%1.%2").arg(meta.moduleId(), QString::fromUtf8(key));
	QVariant val = app->settings()->get(fullKey);
	if (!val.isValid())
		return nullptr;

	r->tempString = val.toString().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_setting_set(void* mh, const char* key, const char* value)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return -1;

	auto& meta = r->manager->m_modules[r->moduleIndex];
	QString fullKey =
		QString("plugin.%1.%2").arg(meta.moduleId(), QString::fromUtf8(key));

	if (!app->settings()->contains(fullKey)) {
		app->settings()->registerSetting(fullKey, QString());
	}

	app->settings()->set(fullKey, QString::fromUtf8(value));
	return 0;
}

int PluginManager::api_instance_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return 0;
	return app->instances()->count();
}

const char* PluginManager::api_instance_get_id(void* mh, int index)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto list = app->instances();
	if (index < 0 || index >= list->count())
		return nullptr;

	auto inst = list->at(index);
	if (!inst)
		return nullptr;

	r->tempString = inst->id().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_name(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return nullptr;

	r->tempString = inst->name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_path(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return nullptr;

	r->tempString = inst->instanceRoot().toStdString();
	return r->tempString.c_str();
}

static BaseInstance* resolveInstance(PluginManager::ModuleRuntime* r,
									 const char* id)
{
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return nullptr;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	return inst.get();
}

static MinecraftInstance* resolveMC(PluginManager::ModuleRuntime* r,
									const char* id)
{
	return dynamic_cast<MinecraftInstance*>(resolveInstance(r, id));
}

int PluginManager::api_instance_set_name(void* mh, const char* id,
										 const char* name)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !name)
		return -1;
	inst->setName(QString::fromUtf8(name));
	return 0;
}

const char* PluginManager::api_instance_get_game_root(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->gameRoot().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_mods_root(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->modsRoot().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_icon_key(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->iconKey().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_icon_key(void* mh, const char* id,
											 const char* key)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !key)
		return -1;
	inst->setIconKey(QString::fromUtf8(key));
	return 0;
}

const char* PluginManager::api_instance_get_type(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->instanceType().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_notes(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->notes().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_notes(void* mh, const char* id,
										  const char* notes)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !notes)
		return -1;
	inst->setNotes(QString::fromUtf8(notes));
	return 0;
}

int PluginManager::api_instance_is_running(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->isRunning() ? 1 : 0) : 0;
}

int PluginManager::api_instance_can_launch(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->canLaunch() ? 1 : 0) : 0;
}

int PluginManager::api_instance_has_crashed(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->hasCrashed() ? 1 : 0) : 0;
}

int PluginManager::api_instance_has_update(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->hasUpdateAvailable() ? 1 : 0) : 0;
}

int PluginManager::api_instance_set_update_available(void* mh, const char* id,
													 int value)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return -1;
	inst->setUpdateAvailable(value != 0);
	return 0;
}

int PluginManager::api_instance_component_set_version(void* mh, const char* id,
													  const char* uid,
													  const char* version)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile() || !uid || !version)
		return -1;
	// important=true marks the version user-pinned, so the resolver does not bump it later.
	const bool ok = mc->getPackProfile()->setComponentVersion(
		QString::fromUtf8(uid), QString::fromUtf8(version),
		/*important=*/true);
	return ok ? 0 : -1;
}

int64_t PluginManager::api_instance_get_total_play_time(void* mh,
														const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->totalTimePlayed() : 0;
}

int64_t PluginManager::api_instance_get_last_play_time(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->lastTimePlayed() : 0;
}

int64_t PluginManager::api_instance_get_last_launch(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->lastLaunch() : 0;
}

int PluginManager::api_instance_launch(void* mh, const char* id, int online)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return -1;
	// The C ABI knows only online/offline; the instance's profiler setting still applies.
	const LaunchMode mode =
		online != 0 ? LaunchMode::Normal : LaunchMode::Offline;
	return app->launch(inst, mode) ? 0 : -1;
}

int PluginManager::api_instance_kill(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return -1;
	return app->kill(inst) ? 0 : -1;
}

int PluginManager::api_instance_delete(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	app->instances()->deleteInstance(QString::fromUtf8(id));
	return 0;
}

const char* PluginManager::api_instance_get_group(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return nullptr;
	r->tempString =
		app->instances()->getInstanceGroup(QString::fromUtf8(id)).toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_group(void* mh, const char* id,
										  const char* group)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	app->instances()->setInstanceGroup(
		QString::fromUtf8(id), group ? QString::fromUtf8(group) : QString());
	return 0;
}

int PluginManager::api_instance_group_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return 0;
	return app->instances()->getGroups().size();
}

const char* PluginManager::api_instance_group_at(void* mh, int index)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;
	auto groups = app->instances()->getGroups();
	if (index < 0 || index >= groups.size())
		return nullptr;
	r->tempString = groups.at(index).toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_component_count(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return 0;
	return mc->getPackProfile()->rowCount(QModelIndex());
}

const char*
PluginManager::api_instance_component_get_uid(void* mh, const char* id, int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getID().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_component_get_name(void* mh,
														   const char* id,
														   int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getName().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_component_get_version(void* mh,
															  const char* id,
															  int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getVersion().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_mc_version(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	r->tempString = mc->getPackProfile()
						->getComponentVersion("net.minecraft")
						.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_jar_mods_dir(void* mh,
														 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->jarModsDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_resource_packs_dir(void* mh,
															   const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->resourcePacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_texture_packs_dir(void* mh,
															  const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->texturePacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_shader_packs_dir(void* mh,
															 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->shaderPacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_worlds_dir(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->worldDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_fs_plugin_data_dir(void* mh)
{
	auto* r = rt(mh);
	return r->dataDir.c_str();
}

static bool validateRelativePath(const char* rel)
{
	if (!rel || rel[0] == '\0')
		return false;
	QString p = QString::fromUtf8(rel);
	// Reject path traversal
	if (p.contains("..") || p.startsWith('/') || p.startsWith('\\'))
		return false;
	return true;
}

int64_t PluginManager::api_fs_read(void* mh, const char* rel, void* buf,
								   size_t sz)
{
	if (!validateRelativePath(rel))
		return -1;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));

	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return -1;

	qint64 bytesRead = f.read(static_cast<char*>(buf), static_cast<qint64>(sz));
	return bytesRead;
}

int PluginManager::api_fs_write(void* mh, const char* rel, const void* data,
								size_t sz)
{
	if (!validateRelativePath(rel))
		return -1;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));

	QDir().mkpath(QFileInfo(path).absolutePath());

	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return -1;

	qint64 written =
		f.write(static_cast<const char*>(data), static_cast<qint64>(sz));
	return (written == static_cast<qint64>(sz)) ? 0 : -1;
}

int PluginManager::api_fs_exists(void* mh, const char* rel)
{
	if (!validateRelativePath(rel))
		return 0;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));
	return QFile::exists(path) ? 1 : 0;
}

int PluginManager::api_http_get(void* mh, const char* url, MMCOHttpCallback cb,
								void* ud)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;

	if (!url || !cb)
		return -1;

	QString qurl = QString::fromUtf8(url);
	if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
		return -1;

	auto nam = app->network();
	if (!nam)
		return -1;

	QNetworkRequest request{QUrl(qurl)};
	request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);

	QNetworkReply* reply = nam->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [reply, cb, ud]() {
		int status =
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray body = reply->readAll();
		cb(ud, status, body.constData(), static_cast<size_t>(body.size()));
		reply->deleteLater();
	});

	return 0;
}

int PluginManager::api_http_get_with_headers(void* mh, const char* url,
											 const char* const* headers,
											 int header_count,
											 MMCOHttpCallback cb, void* ud)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;

	if (!url || !cb)
		return -1;
	if (header_count > 0 && !headers)
		return -1;

	QString qurl = QString::fromUtf8(url);
	if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
		return -1;

	auto nam = app->network();
	if (!nam)
		return -1;

	QNetworkRequest request{QUrl(qurl)};
	// User-Agent is always launcher-controlled.
	request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);

	for (int i = 0; i < header_count; ++i) {
		if (!headers[i])
			continue;
		QByteArray line(headers[i]);
		int sep = line.indexOf(':');
		if (sep <= 0)
			continue;
		QByteArray name = line.left(sep).trimmed();
		QByteArray value = line.mid(sep + 1).trimmed();
		if (name.isEmpty())
			continue;
		// Case-insensitive; a caller-supplied User-Agent is rejected.
		if (name.toLower() == "user-agent")
			continue;
		request.setRawHeader(name, value);
	}

	QNetworkReply* reply = nam->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [reply, cb, ud]() {
		int status =
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray body = reply->readAll();
		cb(ud, status, body.constData(), static_cast<size_t>(body.size()));
		reply->deleteLater();
	});

	return 0;
}

int PluginManager::api_http_post(void* mh, const char* url, const void* body,
								 size_t body_sz, const char* ct,
								 MMCOHttpCallback cb, void* ud)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;

	if (!url || !cb)
		return -1;

	QString qurl = QString::fromUtf8(url);
	if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
		return -1;

	auto nam = app->network();
	if (!nam)
		return -1;

	QNetworkRequest request{QUrl(qurl)};
	request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
	if (ct)
		request.setHeader(QNetworkRequest::ContentTypeHeader,
						  QString::fromUtf8(ct));

	QByteArray postData(static_cast<const char*>(body),
						static_cast<int>(body_sz));
	QNetworkReply* reply = nam->post(request, postData);

	QObject::connect(reply, &QNetworkReply::finished, [reply, cb, ud]() {
		int status =
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray respBody = reply->readAll();
		cb(ud, status, respBody.constData(),
		   static_cast<size_t>(respBody.size()));
		reply->deleteLater();
	});

	return 0;
}

namespace
{
	// Under the QML shell a plugin's question goes to QmlUiHost, never a widget dialog. It cannot
	// wait for the presenter, so it is declined, which every caller handles like a user cancel.
	bool qmlPresenterMissing(Application* app, const char* api)
	{
		if (app->usingQmlShell()) {
			return false;
		}
		qWarning() << "PluginManager:" << api
				   << "declined: the QML shell cannot ask questions yet";
		return true;
	}
} // namespace

namespace
{
	// Runs an account task to its end: busy indication under the QML shell, ProgressDialog
	// otherwise. True when the task succeeded.
	bool runAccountTask(Application* app, Task* task, const QString& busyText,
						const char* api)
	{
		if (app && app->qmlShellChosen()) {
			if (qmlPresenterMissing(app, api))
				return false;
			auto busy = app->uiHost()->showBusy(busyText);
			QEventLoop loop;
			QObject::connect(task, &Task::finished, &loop, &QEventLoop::quit);
			QMetaObject::invokeMethod(task, &Task::start, Qt::QueuedConnection);
			loop.exec();
			return task->wasSuccessful();
		}
		ProgressDialog prog(QApplication::activeWindow());
		return prog.execWithTask(task) == QDialog::Accepted;
	}
} // namespace

void PluginManager::api_ui_show_message(void* mh, int type, const char* title,
										const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];

	QString qtitle =
		QString("[%1] %2").arg(meta.name, QString::fromUtf8(title));
	QString qmsg = QString::fromUtf8(msg);

	// Under the QML shell route through UiHost instead of a QMessageBox; a message sent before the
	// presenter is up waits for it.
	auto* app = r->manager->m_app;
	if (app && app->qmlShellChosen()) {
		UiHost::Severity severity = UiHost::Severity::Information;
		switch (type) {
			case 1:
				severity = UiHost::Severity::Warning;
				break;
			case 2:
				severity = UiHost::Severity::Critical;
				break;
			default:
				break;
		}
		app->runWhenQmlPresenterReady([app, qtitle, qmsg, severity]() {
			app->uiHost()->message(qtitle, qmsg, severity);
		});
		return;
	}

	switch (type) {
		case 1:
			QMessageBox::warning(nullptr, qtitle, qmsg);
			break;
		case 2:
			QMessageBox::critical(nullptr, qtitle, qmsg);
			break;
		default:
			QMessageBox::information(nullptr, qtitle, qmsg);
			break;
	}
}

int PluginManager::api_ui_add_menu_item(void* /* mh */, void* menu_handle,
										const char* label,
										const char* /* icon */,
										MMCOMenuActionCallback cb, void* ud)
{
	if (!menu_handle || !label || !cb)
		return -1;

	auto* menu = static_cast<QMenu*>(menu_handle);
	QString qlabel = QString::fromUtf8(label);

	QAction* action = menu->addAction(qlabel);
	QObject::connect(action, &QAction::triggered, [cb, ud]() { cb(ud); });

	return 0;
}

const char* PluginManager::api_get_app_version(void* mh)
{
	auto* r = rt(mh);
	r->tempString = BuildConfig.VERSION_STR.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_get_app_name(void* mh)
{
	auto* r = rt(mh);
	r->tempString = BuildConfig.KITE_NAME.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_zip_compress_dir(void* mh, const char* zip,
										const char* dir)
{
	if (!zip || !dir)
		return -1;

	// Inside a background hook report a real percentage on the plugin's row; elsewhere skip the
	// counting pass.
	auto* task = PluginHookTask::currentOnThisThread();
	if (mh && task && task->moduleHandle() != mh) {
		task = nullptr;
	}

	MMCZip::ProgressFunction progress = nullptr;
	if (task) {
		// One queued UI update per file would drown the event loop on a big instance.
		auto lastReported = std::make_shared<qint64>(-1);
		progress = [task, lastReported](qint64 current, qint64 total) {
			const qint64 step = qMax(qint64(1), total / 100);
			if (current != total && current / step == *lastReported) {
				return;
			}
			*lastReported = current / step;
			task->reportProgress(
				QString(),
				PluginManager::tr("%1 / %2 files").arg(current).arg(total),
				current, total);
		};
	}

	bool ok = MMCZip::compressDir(QString::fromUtf8(zip),
								  QString::fromUtf8(dir), nullptr, progress);
	return ok ? 0 : -1;
}

int PluginManager::api_zip_extract(void* mh, const char* zip,
								   const char* target)
{
	(void)mh;
	if (!zip || !target)
		return -1;
	auto result =
		MMCZip::extractDir(QString::fromUtf8(zip), QString::fromUtf8(target));
	return result.has_value() ? 0 : -1;
}

int PluginManager::api_fs_list_dir(void* mh, const char* path, int type,
								   MMCODirEntryCallback cb, void* ud)
{
	(void)mh;
	if (!path || !cb)
		return -1;

	QDir dir(QString::fromUtf8(path));
	if (!dir.exists())
		return -1;

	QDir::Filters filters;
	if (type == 1)
		filters = QDir::Files;
	else if (type == 2)
		filters = QDir::Dirs | QDir::NoDotAndDotDot;
	else
		filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

	const auto entries = dir.entryInfoList(filters);
	for (const auto& entry : entries) {
		cb(ud, entry.fileName().toUtf8().constData(), entry.isDir() ? 1 : 0);
	}
	return 0;
}

int PluginManager::api_fs_copy_file(void* mh, const char* src, const char* dst)
{
	(void)mh;
	if (!src || !dst)
		return -1;
	return QFile::copy(QString::fromUtf8(src), QString::fromUtf8(dst)) ? 0 : -1;
}

int PluginManager::api_fs_remove(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;

	QFileInfo fi(QString::fromUtf8(path));
	if (fi.isDir()) {
		return QDir(fi.absoluteFilePath()).removeRecursively() ? 0 : -1;
	}
	return QFile::remove(fi.absoluteFilePath()) ? 0 : -1;
}

int PluginManager::api_fs_mkdir(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;
	return QDir().mkpath(QString::fromUtf8(path)) ? 0 : -1;
}

int PluginManager::api_fs_exists_abs(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return 0;
	return QFileInfo::exists(QString::fromUtf8(path)) ? 1 : 0;
}

int64_t PluginManager::api_fs_file_size(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;
	QFileInfo fi(QString::fromUtf8(path));
	if (!fi.exists())
		return -1;
	return fi.size();
}

int64_t PluginManager::api_get_timestamp(void* mh)
{
	(void)mh;
	return QDateTime::currentSecsSinceEpoch();
}

static std::shared_ptr<ModFolderModel>
resolveModList(PluginManager::ModuleRuntime* r, const char* inst_id,
			   const char* type)
{
	auto* mc = resolveMC(r, inst_id);
	if (!mc || !type)
		return nullptr;

	QString t = QString::fromUtf8(type).toLower();
	if (t == "loader")
		return mc->loaderModList();
	if (t == "core")
		return mc->coreModList();
	if (t == "resourcepack")
		return mc->resourcePackList();
	if (t == "texturepack")
		return mc->texturePackList();
	if (t == "shaderpack")
		return mc->shaderPackList();
	return nullptr;
}

int PluginManager::api_mod_count(void* mh, const char* inst, const char* type)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	return model ? static_cast<int>(model->size()) : 0;
}

const char* PluginManager::api_mod_get_name(void* mh, const char* inst,
											const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_version(void* mh, const char* inst,
											   const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).version().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_filename(void* mh, const char* inst,
												const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).filename().fileName().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_description(void* mh, const char* inst,
												   const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).description().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_mod_is_enabled(void* mh, const char* inst,
									  const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return 0;
	return model->at(idx).enabled() ? 1 : 0;
}

int PluginManager::api_mod_set_enabled(void* mh, const char* inst,
									   const char* type, int idx, int e)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return -1;
	QModelIndexList indices;
	indices.append(model->index(idx, 0));
	return model->setModStatus(indices, e ? ModFolderModel::Enable
										  : ModFolderModel::Disable)
			   ? 0
			   : -1;
}

int PluginManager::api_mod_remove(void* mh, const char* inst, const char* type,
								  int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return -1;
	QModelIndexList indices;
	indices.append(model->index(idx, 0));
	return model->deleteMods(indices) ? 0 : -1;
}

int PluginManager::api_mod_install(void* mh, const char* inst, const char* type,
								   const char* path)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || !path)
		return -1;
	return model->installMod(QString::fromUtf8(path)) ? 0 : -1;
}

int PluginManager::api_mod_refresh(void* mh, const char* inst, const char* type)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model)
		return -1;
	return model->update() ? 0 : -1;
}

static std::shared_ptr<WorldList>
resolveWorldList(PluginManager::ModuleRuntime* r, const char* inst_id)
{
	auto* mc = resolveMC(r, inst_id);
	if (!mc)
		return nullptr;
	return mc->worldList();
}

int PluginManager::api_world_count(void* mh, const char* inst)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	return wl ? static_cast<int>(wl->size()) : 0;
}

const char* PluginManager::api_world_get_name(void* mh, const char* inst,
											  int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return nullptr;
	r->tempString = wl->allWorlds().at(idx).name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_world_get_folder(void* mh, const char* inst,
												int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return nullptr;
	r->tempString = wl->allWorlds().at(idx).folderName().toStdString();
	return r->tempString.c_str();
}

int64_t PluginManager::api_world_get_seed(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return 0;
	return wl->allWorlds().at(idx).seed();
}

int PluginManager::api_world_get_game_type(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return -1;
	return wl->allWorlds().at(idx).gameType().type;
}

int64_t PluginManager::api_world_get_last_played(void* mh, const char* inst,
												 int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return 0;
	return wl->allWorlds().at(idx).lastPlayed().toMSecsSinceEpoch();
}

int PluginManager::api_world_delete(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return -1;
	return wl->deleteWorld(idx) ? 0 : -1;
}

int PluginManager::api_world_rename(void* mh, const char* inst, int idx,
									const char* name)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()) || !name)
		return -1;
	// WorldList doesn't expose rename by index; access the world directly
	auto& worlds = wl->allWorlds();
	World w = worlds.at(idx);
	return w.rename(QString::fromUtf8(name)) ? 0 : -1;
}

int PluginManager::api_world_install(void* mh, const char* inst,
									 const char* path)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || !path)
		return -1;
	wl->installWorld(QFileInfo(QString::fromUtf8(path)));
	return 0;
}

int PluginManager::api_world_refresh(void* mh, const char* inst)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl)
		return -1;
	return wl->update() ? 0 : -1;
}

int PluginManager::api_account_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return 0;
	return app->accounts()->count();
}

const char* PluginManager::api_account_get_profile_name(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return nullptr;
	auto acc = app->accounts()->at(idx);
	if (!acc)
		return nullptr;
	r->tempString = acc->profileName().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_account_get_profile_id(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return nullptr;
	auto acc = app->accounts()->at(idx);
	if (!acc)
		return nullptr;
	r->tempString = acc->profileId().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_account_get_type(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return nullptr;
	auto acc = app->accounts()->at(idx);
	if (!acc)
		return nullptr;
	r->tempString = acc->typeString().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_account_get_state(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return -1;
	auto acc = app->accounts()->at(idx);
	if (!acc)
		return -1;
	return static_cast<int>(acc->accountState());
}

int PluginManager::api_account_is_active(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return 0;
	auto acc = app->accounts()->at(idx);
	return (acc && acc->isActive()) ? 1 : 0;
}

int PluginManager::api_account_get_default_index(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return -1;
	auto def = app->accounts()->defaultAccount();
	if (!def)
		return -1;
	for (int i = 0; i < app->accounts()->count(); ++i) {
		if (app->accounts()->at(i) == def)
			return i;
	}
	return -1;
}

int PluginManager::api_java_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return 0;
	return app->javalist()->count();
}

const char* PluginManager::api_java_get_version(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->id.toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_java_get_arch(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->arch.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_java_get_path(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->path.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_java_is_recommended(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return 0;
	if (idx < 0 || idx >= app->javalist()->count())
		return 0;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	return (ver && ver->recommended) ? 1 : 0;
}

const char* PluginManager::api_instance_get_java_version(void* mh,
														 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->getJavaVersion().toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_file_open_dialog(void* mh, const char* title,
												   const char* filter)
{
	auto* r = rt(mh);
	const QString qtitle = title ? QString::fromUtf8(title) : QString();
	const QString qfilter = filter ? QString::fromUtf8(filter) : QString();

	std::optional<QString> result;
	auto* app = r->manager->m_app;
	if (app && app->qmlShellChosen()) {
		// activeWindow() is null under the QML shell: ask through UiHost (QtQuick.Dialogs
		// FileDialog).
		if (qmlPresenterMissing(app, "ui_file_open_dialog"))
			return nullptr;
		result = app->uiHost()->pickFile(UiHost::FilePickerMode::Open, qtitle,
										 QString(), qfilter);
	} else {
		const QString path = QFileDialog::getOpenFileName(
			QApplication::activeWindow(), qtitle, QString(), qfilter);
		if (!path.isEmpty())
			result = path;
	}
	if (!result)
		return nullptr;
	r->tempString = result->toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_file_save_dialog(void* mh, const char* title,
												   const char* def,
												   const char* filter)
{
	auto* r = rt(mh);
	const QString qtitle = title ? QString::fromUtf8(title) : QString();
	const QString qdef = def ? QString::fromUtf8(def) : QString();
	const QString qfilter = filter ? QString::fromUtf8(filter) : QString();

	std::optional<QString> result;
	auto* app = r->manager->m_app;
	if (app && app->qmlShellChosen()) {
		if (qmlPresenterMissing(app, "ui_file_save_dialog"))
			return nullptr;
		result = app->uiHost()->pickFile(UiHost::FilePickerMode::Save, qtitle,
										 qdef, qfilter);
	} else {
		const QString path = QFileDialog::getSaveFileName(
			QApplication::activeWindow(), qtitle, qdef, qfilter);
		if (!path.isEmpty())
			result = path;
	}
	if (!result)
		return nullptr;
	r->tempString = result->toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_input_dialog(void* mh, const char* title,
											   const char* prompt,
											   const char* def)
{
	auto* r = rt(mh);
	const QString qtitle = title ? QString::fromUtf8(title) : QString();
	const QString qprompt = prompt ? QString::fromUtf8(prompt) : QString();
	const QString qdef = def ? QString::fromUtf8(def) : QString();

	auto* app = r->manager->m_app;
	if (app && app->qmlShellChosen()) {
		if (qmlPresenterMissing(app, "ui_input_dialog"))
			return nullptr;
		const auto answer = app->uiHost()->askText(qtitle, qprompt, qdef);
		if (!answer)
			return nullptr;
		r->tempString = answer->toStdString();
		return r->tempString.c_str();
	}

	bool ok = false;
	QString result = QInputDialog::getText(nullptr, qtitle, qprompt,
										   QLineEdit::Normal, qdef, &ok);
	if (!ok)
		return nullptr;
	r->tempString = result.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_ui_confirm_dialog(void* mh, const char* title,
										 const char* msg)
{
	auto* r = rt(mh);
	const QString qtitle = title ? QString::fromUtf8(title) : QString();
	const QString qmsg = msg ? QString::fromUtf8(msg) : QString();

	auto* app = r->manager->m_app;
	if (app && app->qmlShellChosen()) {
		if (qmlPresenterMissing(app, "ui_confirm_dialog"))
			return 0;
		return app->uiHost()->confirm(qtitle, qmsg, UiHost::Severity::Question)
				  ? 1
				  : 0;
	}

	auto ret = QMessageBox::question(nullptr, qtitle, qmsg,
									 QMessageBox::Yes | QMessageBox::No,
									 QMessageBox::No);
	return ret == QMessageBox::Yes ? 1 : 0;
}

/*
 * ABI 5 declarative UI surfaces. A plugin describes a widget tree as an "mmco-ui/1" JSON
 * document (see PluginUiRenderer.h); the host renders and owns the widgets. SurfaceRecord::doc
 * stays the canonical document whether or not a view is mounted. Anchors are realised in:
 *   INSTANCE_PAGE      -> createInstancePages()
 *   GLOBAL_SETTINGS    -> createGlobalSettingsPluginsPage()
 *   INSTANCE_SETTINGS  -> buildPluginsSectionWidget()
 */

#include "ui/pages/BasePage.h"

namespace
{
	QString surfaceDocToText(const QJsonObject& doc)
	{
		return QString::fromUtf8(QJsonDocument(doc).toJson(QJsonDocument::Compact));
	}

	// Merges `patch` into the "props" of the node with id `nodeId` (creating it if absent); true
	// when found.
	bool patchNodeProps(QJsonObject& node, const QString& nodeId,
						const QJsonObject& patch)
	{
		if (node.value(QStringLiteral("id")).toString() == nodeId) {
			QJsonObject props = node.value(QStringLiteral("props")).toObject();
			for (auto it = patch.constBegin(); it != patch.constEnd(); ++it)
				props.insert(it.key(), it.value());
			node[QStringLiteral("props")] = props;
			return true;
		}
		if (node.contains(QStringLiteral("children"))) {
			QJsonArray children = node.value(QStringLiteral("children")).toArray();
			for (int i = 0; i < children.size(); ++i) {
				QJsonObject child = children.at(i).toObject();
				if (patchNodeProps(child, nodeId, patch)) {
					children[i] = child;
					node[QStringLiteral("children")] = children;
					return true;
				}
			}
		}
		return false;
	}

	bool patchDocumentNodeProps(QJsonObject& doc, const QString& nodeId,
							   const QJsonObject& patch)
	{
		QJsonObject root = doc.value(QStringLiteral("root")).toObject();
		if (root.isEmpty())
			return false;
		if (!patchNodeProps(root, nodeId, patch))
			return false;
		doc[QStringLiteral("root")] = root;
		return true;
	}

	// Ties a RenderedSurface's lifetime to the QWidget it is parented under (stacked sections).
	class RendererOwner : public QObject
	{
	  public:
		RendererOwner(std::unique_ptr<PluginUiRenderer::RenderedSurface> renderer,
					 QObject* parent)
			: QObject(parent), m_renderer(std::move(renderer))
		{
		}

	  private:
		std::unique_ptr<PluginUiRenderer::RenderedSurface> m_renderer;
	};

	// An INSTANCE_PAGE surface wrapped as its own instance-window page; owns its RenderedSurface.
	class PluginSurfacePage : public QWidget, public BasePage
	{
	  public:
		PluginSurfacePage(QString id, QString displayName, QString iconName,
						  QWidget* content,
						  std::unique_ptr<PluginUiRenderer::RenderedSurface> renderer)
			: m_id(std::move(id)), m_displayName(std::move(displayName)),
			  m_iconName(std::move(iconName)), m_renderer(std::move(renderer))
		{
			auto* layout = new QVBoxLayout(this);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->addWidget(content);
		}

		QString id() const override
		{
			return m_id;
		}
		QString displayName() const override
		{
			return m_displayName;
		}
		QIcon icon() const override
		{
			if (m_iconName.startsWith(QLatin1Char(':')))
				return QIcon(m_iconName);
			return QIcon::fromTheme(m_iconName.isEmpty() ? QStringLiteral("plugin")
														 : m_iconName);
		}
		bool shouldDisplay() const override
		{
			return true;
		}

	  private:
		QString m_id;
		QString m_displayName;
		QString m_iconName;
		std::unique_ptr<PluginUiRenderer::RenderedSurface> m_renderer;
	};

	// The one page every GLOBAL_SETTINGS surface is stacked into.
	class PluginsGroupPage : public QWidget, public BasePage
	{
	  public:
		explicit PluginsGroupPage(QWidget* content)
		{
			auto* scroll = new QScrollArea(this);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setWidget(content);
			auto* layout = new QVBoxLayout(this);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->addWidget(scroll);
		}
		QString id() const override
		{
			return QStringLiteral("plugins");
		}
		QString displayName() const override
		{
			return PluginManager::tr("Plugins");
		}
		QIcon icon() const override
		{
			return QIcon::fromTheme(QStringLiteral("plugin"));
		}
		bool shouldDisplay() const override
		{
			return true;
		}
	};

} // anonymous namespace

PluginManager::SurfaceRecord* PluginManager::findSurface(void* module_handle,
														 void* surface)
{
	for (auto& rec : m_surfaces) {
		if (rec.get() == surface && rec->module_handle == module_handle)
			return rec.get();
	}
	return nullptr;
}

PluginUiRenderer::EventSink PluginManager::makeSurfaceSink(SurfaceRecord* rec)
{
	MMCOUiEventCallback cb = rec->cb;
	void* userData = rec->userData;
	QString surfaceId = rec->surfaceId;
	return [cb, userData, surfaceId](const QString& nodeId, const QString& event,
									 const QString& valueJson) {
		if (!cb)
			return;
		const QByteArray sid = surfaceId.toUtf8();
		const QByteArray nid = nodeId.toUtf8();
		const QByteArray ev = event.toUtf8();
		const QByteArray val = valueJson.toUtf8();
		cb(userData, sid.constData(), nid.constData(), ev.constData(),
		  val.constData());
	};
}

void* PluginManager::api_ui_surface_create(void* mh, int anchor,
										   const char* anchor_context,
										   const char* title, const char* icon_name,
										   const char* json_doc,
										   MMCOUiEventCallback cb, void* user_data)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	if (anchor != MMCO_UI_ANCHOR_GLOBAL_SETTINGS &&
		anchor != MMCO_UI_ANCHOR_INSTANCE_PAGE &&
		anchor != MMCO_UI_ANCHOR_INSTANCE_SETTINGS)
		return nullptr;

	QJsonParseError err{};
	const QJsonDocument jd =
		QJsonDocument::fromJson(QByteArray(json_doc ? json_doc : ""), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject()) {
		qWarning().noquote()
			<< "[Plugin:" << r->manager->m_modules[r->moduleIndex].name
			<< "] ui_surface_create: invalid JSON document:" << err.errorString();
		return nullptr;
	}

	auto rec = std::make_unique<SurfaceRecord>();
	rec->module_handle = mh;
	rec->surfaceId = QStringLiteral("sf-%1").arg(++r->manager->m_nextSurfaceSeq);
	rec->anchor = anchor;
	rec->anchorContext =
		anchor_context ? QString::fromUtf8(anchor_context) : QString();
	rec->title = title ? QString::fromUtf8(title) : QString();
	rec->iconName = icon_name ? QString::fromUtf8(icon_name) : QString();
	rec->doc = jd.object();
	rec->cb = cb;
	rec->userData = user_data;

	auto* handle = rec.get();
	qCDebug(pluginsLog).noquote()
		<< "[Plugin:" << r->manager->m_modules[r->moduleIndex].name
		<< "] ui_surface_create: anchor" << anchor << "context"
		<< (anchor_context ? QString::fromUtf8(anchor_context) : QString())
		<< "->" << handle->surfaceId;
	r->manager->m_surfaces.push_back(std::move(rec));
	emit r->manager->surfacesChanged();
	return handle;
}

int PluginManager::api_ui_surface_update(void* mh, void* surface,
										 const char* json_doc)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto* rec = r->manager->findSurface(mh, surface);
	if (!rec)
		return -1;

	QJsonParseError err{};
	const QJsonDocument jd =
		QJsonDocument::fromJson(QByteArray(json_doc ? json_doc : ""), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return -1;

	rec->doc = jd.object();
	if (rec->mountedRoot && rec->mountedRenderer)
		rec->mountedRenderer->setDocument(rec->doc);
	emit r->manager->surfacesChanged();
	return 0;
}

int PluginManager::api_ui_surface_set(void* mh, void* surface, const char* node_id,
									  const char* json_props)
{
	auto* r = rt(mh);
	if (!r || !node_id)
		return -1;
	auto* rec = r->manager->findSurface(mh, surface);
	if (!rec)
		return -1;

	QJsonParseError err{};
	const QJsonDocument jd =
		QJsonDocument::fromJson(QByteArray(json_props ? json_props : "{}"), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return -1;

	const QString nodeId = QString::fromUtf8(node_id);
	const QJsonObject props = jd.object();
	const bool foundInDoc = patchDocumentNodeProps(rec->doc, nodeId, props);
	if (rec->mountedRoot && rec->mountedRenderer)
		rec->mountedRenderer->setNodeProps(nodeId, props);
	// The mounted renderer is patched above; surfacesChanged() tells readers of `doc`
	// (PluginSurfaceModel).
	if (foundInDoc)
		emit r->manager->surfacesChanged();
	return foundInDoc ? 0 : -1;
}

int PluginManager::api_ui_surface_set_rows(void* mh, void* surface,
										   const char* node_id,
										   const char* json_rows)
{
	auto* r = rt(mh);
	if (!r || !node_id)
		return -1;
	auto* rec = r->manager->findSurface(mh, surface);
	if (!rec)
		return -1;

	QJsonParseError err{};
	const QJsonDocument jd =
		QJsonDocument::fromJson(QByteArray(json_rows ? json_rows : "[]"), &err);
	if (err.error != QJsonParseError::NoError || !jd.isArray())
		return -1;

	const QString nodeId = QString::fromUtf8(node_id);
	const QJsonArray rows = jd.array();
	QJsonObject rowsPatch;
	rowsPatch[QStringLiteral("rows")] = rows;
	const bool foundInDoc = patchDocumentNodeProps(rec->doc, nodeId, rowsPatch);
	if (rec->mountedRoot && rec->mountedRenderer)
		rec->mountedRenderer->setRows(nodeId, rows);
	if (foundInDoc)
		emit r->manager->surfacesChanged();
	return foundInDoc ? 0 : -1;
}

int PluginManager::api_ui_surface_destroy(void* mh, void* surface)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto& vec = r->manager->m_surfaces;
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i].get() != surface || vec[i]->module_handle != mh)
			continue;
		if (vec[i]->mountedRoot && !r->manager->m_shutdownDone)
			vec[i]->mountedRoot->deleteLater();
		vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(i));
		emit r->manager->surfacesChanged();
		return 0;
	}
	return -1;
}

int PluginManager::api_ui_modal_run(void* mh, const char* title,
									const char* json_doc, char* out_result_json,
									int out_buf_size)
{
	if (!json_doc)
		return -1;

	auto* r = rt(mh);
	auto* app = r ? r->manager->m_app : nullptr;
	if (!app)
		return -1;

	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(QByteArray(json_doc), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return -1;

	// Drawn through UiHost: PluginNode.qml under the QML shell, PluginUiRenderer in a QDialog
	// otherwise.
	if (app->qmlShellChosen() && qmlPresenterMissing(app, "ui_modal_run"))
		return -1;
	const auto result = app->uiHost()->runPluginModal(
		title ? QString::fromUtf8(title) : QString(), jd.object());
	if (!result)
		return -1;

	QJsonObject out;
	out[QStringLiteral("button")] = result->clicked;
	out[QStringLiteral("fields")] = result->fields;
	const QByteArray bytes = QJsonDocument(out).toJson(QJsonDocument::Compact);

	if (out_result_json && out_buf_size > 0) {
		const int n = qMin(static_cast<int>(bytes.size()), out_buf_size - 1);
		memcpy(out_result_json, bytes.constData(), static_cast<size_t>(n));
		out_result_json[n] = '\0';
	}
	return 0;
}

QList<BasePage*> PluginManager::createInstancePages(const QString& instanceId)
{
	QList<BasePage*> pages;
	for (auto& rec : m_surfaces) {
		if (rec->anchor != MMCO_UI_ANCHOR_INSTANCE_PAGE ||
			rec->anchorContext != instanceId)
			continue;
		SurfaceRecord* recPtr = rec.get();
		auto renderer = PluginUiRenderer::build(surfaceDocToText(recPtr->doc),
												makeSurfaceSink(recPtr));
		QWidget* root = renderer->rootWidget();
		recPtr->mountedRoot = root;
		recPtr->mountedRenderer = renderer.get();
		pages.append(new PluginSurfacePage(recPtr->surfaceId, recPtr->title,
										   recPtr->iconName, root,
										   std::move(renderer)));
	}
	return pages;
}

QWidget* PluginManager::buildPluginsSectionWidget(int anchor,
												  const QString& anchorContext)
{
	QWidget* container = nullptr;
	QVBoxLayout* layout = nullptr;
	for (auto& rec : m_surfaces) {
		if (rec->anchor != anchor || rec->anchorContext != anchorContext)
			continue;
		if (!container) {
			container = new QWidget();
			layout = new QVBoxLayout(container);
			layout->setContentsMargins(0, 0, 0, 0);
		}
		SurfaceRecord* recPtr = rec.get();
		auto renderer = PluginUiRenderer::build(surfaceDocToText(recPtr->doc),
												makeSurfaceSink(recPtr));
		auto* group = new QGroupBox(recPtr->title);
		auto* groupLayout = new QVBoxLayout(group);
		groupLayout->addWidget(renderer->rootWidget());
		recPtr->mountedRoot = renderer->rootWidget();
		recPtr->mountedRenderer = renderer.get();
		// Released with `group` (see RendererOwner).
		new RendererOwner(std::move(renderer), group);
		layout->addWidget(group);
	}
	if (layout)
		layout->addStretch(1);
	return container;
}

BasePage* PluginManager::createGlobalSettingsPluginsPage()
{
	// One shared "Plugins" page stacks every GLOBAL_SETTINGS surface as a titled section.
	QWidget* content =
		buildPluginsSectionWidget(MMCO_UI_ANCHOR_GLOBAL_SETTINGS, QString());
	if (!content)
		return nullptr;
	return new PluginsGroupPage(content);
}

void PluginManager::releaseSurfacesForModule(void* module_handle)
{
	for (int i = static_cast<int>(m_surfaces.size()) - 1; i >= 0; --i) {
		auto& rec = m_surfaces[static_cast<size_t>(i)];
		if (rec->module_handle != module_handle)
			continue;
		if (rec->mountedRoot && !m_shutdownDone)
			rec->mountedRoot->deleteLater();
		m_surfaces.erase(m_surfaces.begin() + i);
	}
}

QList<PluginManager::SurfaceInfo>
PluginManager::surfaces(int anchor, const QString& anchorContext) const
{
	QList<SurfaceInfo> out;
	for (auto& rec : m_surfaces) {
		if (anchor >= 0 && rec->anchor != anchor)
			continue;
		if (!anchorContext.isNull() && rec->anchorContext != anchorContext)
			continue;
		SurfaceInfo info;
		info.handle = rec.get();
		info.surfaceId = rec->surfaceId;
		info.anchor = rec->anchor;
		info.anchorContext = rec->anchorContext;
		info.title = rec->title;
		info.iconName = rec->iconName;
		info.document = surfaceDocToText(rec->doc);
		out.append(info);
	}
	return out;
}

void PluginManager::deliverUiEvent(const QString& surfaceId, const QString& nodeId,
								   const QString& event, const QString& valueJson)
{
	for (auto& rec : m_surfaces) {
		if (rec->surfaceId != surfaceId)
			continue;
		if (rec->cb) {
			const QByteArray sid = surfaceId.toUtf8();
			const QByteArray nid = nodeId.toUtf8();
			const QByteArray ev = event.toUtf8();
			const QByteArray val = valueJson.toUtf8();
			rec->cb(rec->userData, sid.constData(), nid.constData(),
					ev.constData(), val.constData());
		}
		return;
	}
}

/* ── S15 — Launch Modifiers ───────────────────────────────────────── */

int PluginManager::api_launch_set_env(void* mh, const char* key,
									  const char* value)
{
	auto* r = rt(mh);
	if (!key || !value)
		return -1;
	QMutexLocker lock(&r->manager->m_launchModMutex);
	r->manager->m_pendingLaunchEnv.insert(QString::fromUtf8(key),
										  QString::fromUtf8(value));
	return 0;
}

int PluginManager::api_launch_prepend_wrapper(void* mh, const char* wrapper_cmd)
{
	auto* r = rt(mh);
	if (!wrapper_cmd || wrapper_cmd[0] == '\0')
		return -1;
	QString cmd = QString::fromUtf8(wrapper_cmd);
	QMutexLocker lock(&r->manager->m_launchModMutex);
	if (r->manager->m_pendingLaunchWrapper.isEmpty()) {
		r->manager->m_pendingLaunchWrapper = cmd;
	} else {
		r->manager->m_pendingLaunchWrapper =
			cmd + " " + r->manager->m_pendingLaunchWrapper;
	}
	return 0;
}

void PluginManager::clearPendingLaunchMods()
{
	QMutexLocker lock(&m_launchModMutex);
	m_pendingLaunchEnv.clear();
	m_pendingLaunchWrapper.clear();
}

/* ── S16 — Application Settings ───────────────────────────────────── */

const char* PluginManager::api_app_setting_get(void* mh, const char* key)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings() || !key)
		return nullptr;

	QString qKey = QString::fromUtf8(key);
	if (!app->settings()->contains(qKey))
		return nullptr;

	QVariant val = app->settings()->get(qKey);
	if (!val.isValid())
		return nullptr;

	r->tempString = val.toString().toStdString();
	return r->tempString.c_str();
}

QMap<QString, QString> PluginManager::takePendingLaunchEnv()
{
	QMutexLocker lock(&m_launchModMutex);
	QMap<QString, QString> env;
	env.swap(m_pendingLaunchEnv);
	return env;
}

QString PluginManager::takePendingLaunchWrapper()
{
	QMutexLocker lock(&m_launchModMutex);
	QString w;
	w.swap(m_pendingLaunchWrapper);
	return w;
}

/* ── S18 — Plugin Icon Set (ABI 2+) ────────────────────────────────── */

const char* PluginManager::api_ui_plugin_icon(void* mh, const char* name)
{
	auto* r = rt(mh);
	if (!r || !name)
		return nullptr;

	auto& meta = r->manager->m_modules[r->moduleIndex];
	if (meta.iconSetResource.isEmpty())
		return nullptr;

	QString setName = meta.iconSetResource;
	while (setName.startsWith(QLatin1Char(':')) ||
		   setName.startsWith(QLatin1Char('/')))
		setName.remove(0, 1);
	if (setName.startsWith(QLatin1String("plugins/")))
		setName.remove(0, 8);

	// Candidate paths, tried in order; plugins may pass the name with or without an extension.
	const QString rawName = QString::fromUtf8(name);
	QStringList candidates;
	candidates << QStringLiteral(":/plugins/%1/%2").arg(setName, rawName);
	if (!rawName.contains(QLatin1Char('.'))) {
		candidates
			<< QStringLiteral(":/plugins/%1/%2.svg").arg(setName, rawName)
			<< QStringLiteral(":/plugins/%1/%2.png").arg(setName, rawName);
	}

	for (const QString& path : candidates) {
		if (QFile::exists(path)) {
			r->tempString = path.toStdString();
			return r->tempString.c_str();
		}
	}

	qWarning().noquote() << "[Plugin:" << meta.name
						 << "] Icon not found:" << rawName << "(set:" << setName
						 << ")";
	return nullptr;
}

/* ── S17 — News API ────────────────────────────────────────────────── */

// S17 is a read-only view onto Application::newsChecker(), which owns, merges and sorts every feed.
NewsChecker* PluginManager::newsChecker() const
{
	return m_app ? m_app->newsChecker() : nullptr;
}

QList<NewsEntryPtr> PluginManager::newsEntries() const
{
	auto* checker = newsChecker();
	if (!checker)
		return {};
	return checker->getNewsEntries();
}

/* Every entry getter resolves the runtime, bounds-checks the merged list and returns the string
 * through the module's scratch string, valid until its next API call. */
#define MMCO_NEWS_ENTRY_STRING(field)                                          \
	auto* r = rt(mh);                                                          \
	if (!r)                                                                    \
		return nullptr;                                                        \
	const auto entries = r->manager->newsEntries();                            \
	if (index < 0 || index >= entries.size())                                  \
		return nullptr;                                                        \
	r->tempString = (field).toStdString();                                     \
	return r->tempString.c_str();

int PluginManager::api_news_get_entry_count(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	return r->manager->newsEntries().size();
}

const char* PluginManager::api_news_get_entry_title(void* mh, int index)
{
	MMCO_NEWS_ENTRY_STRING(entries[index]->title)
}

const char* PluginManager::api_news_get_entry_link(void* mh, int index)
{
	MMCO_NEWS_ENTRY_STRING(entries[index]->link)
}

const char* PluginManager::api_news_get_entry_content(void* mh, int index)
{
	MMCO_NEWS_ENTRY_STRING(entries[index]->content)
}

const char* PluginManager::api_news_get_entry_author(void* mh, int index)
{
	MMCO_NEWS_ENTRY_STRING(entries[index]->author)
}

const char* PluginManager::api_news_get_entry_date(void* mh, int index)
{
	/* The ABI documents this as ISO 8601. */
	MMCO_NEWS_ENTRY_STRING(entries[index]->pubDate.toString(Qt::ISODate))
}

#undef MMCO_NEWS_ENTRY_STRING

int PluginManager::api_news_get_entry_feed_index(void* mh, int index)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	const auto entries = r->manager->newsEntries();
	if (index < 0 || index >= entries.size())
		return -1;
	return entries[index]->feedIndex;
}

int PluginManager::api_news_add_feed_url(void* mh, const char* url)
{
	auto* r = rt(mh);
	if (!r || !url)
		return -1;

	// Feeds are fixed at build time (Kite_NEWS_EXTRA_FEEDS); adding one mid-session would renumber
	// the indices readers hold.
	qWarning() << "[PluginManager] news_add_feed_url is no longer supported:"
			   << "feeds are configured at build time. Ignoring"
			   << QString::fromUtf8(url);
	return -1;
}

int PluginManager::api_news_get_feed_count(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return 0;
	auto* checker = r->manager->newsChecker();
	return checker ? checker->feedUrls().size() : 0;
}

const char* PluginManager::api_news_get_feed_url(void* mh, int index)
{
	auto* r = rt(mh);
	if (!r || index < 0)
		return nullptr;

	auto* checker = r->manager->newsChecker();
	if (!checker)
		return nullptr;

	const QStringList urls = checker->feedUrls();
	if (index >= urls.size())
		return nullptr;

	r->tempString = urls.at(index).toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_news_reload(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return -1;

	auto* checker = r->manager->newsChecker();
	if (!checker)
		return -1;

	// One reload covers every feed; MMCO_HOOK_NEWS_UPDATED fires from newsLoaded() once all have
	// landed.
	checker->reloadNews();
	return 0;
}

/* S19 System Tray / S20 Main Window helpers. Resources are tracked per module and torn down in
 * releaseTrayResourcesForModule(); plugins only get opaque handles. Icon names resolve via
 * QIcon::fromTheme(), then as a Qt resource path. */

namespace
{
	QIcon mmco_resolve_icon(const char* name)
	{
		if (!name || !*name)
			return QIcon();
		QString s = QString::fromUtf8(name);
		QIcon themed = QIcon::fromTheme(s);
		if (!themed.isNull())
			return themed;
		return QIcon(s);
	}

	QSystemTrayIcon::MessageIcon mmco_message_icon(int icon_type)
	{
		switch (icon_type) {
			case 1:
				return QSystemTrayIcon::Information;
			case 2:
				return QSystemTrayIcon::Warning;
			case 3:
				return QSystemTrayIcon::Critical;
			default:
				return QSystemTrayIcon::NoIcon;
		}
	}
} // namespace

QWidget* PluginManager::resolveMainWindow()
{
	if (m_filteredMainWindow)
		return m_filteredMainWindow.data();

	for (auto* w : qApp->topLevelWidgets()) {
		if (w->objectName() == QStringLiteral("MainWindow")) {
			m_filteredMainWindow = w;
			return w;
		}
	}
	return nullptr;
}

QWindow* PluginManager::resolveShellWindow()
{
	if (m_filteredShellWindow)
		return m_filteredShellWindow.data();
	if (!m_app)
		return nullptr;

	// The QML shell window exists only when the widget MainWindow does not (see useQmlShell()).
	QWindow* window = m_app->qmlShellWindow();
	if (window)
		m_filteredShellWindow = window;
	return window;
}

void PluginManager::ensureCloseFilterInstalled()
{
	if (m_closeFilterInstalled)
		return;
	if (QWidget* mw = resolveMainWindow()) {
		mw->installEventFilter(this);
		m_closeFilterInstalled = true;
		return;
	}
	if (QWindow* window = resolveShellWindow()) {
		window->installEventFilter(this);
		m_closeFilterInstalled = true;
	}
}

bool PluginManager::eventFilter(QObject* watched, QEvent* event)
{
	// Filters close events on the main window (widget MainWindow, or the QML shell's QQuickWindow).
	// QmlShell installs its own filter first, so this later one runs first and can stop a vetoed
	// close.
	const bool isMainWindow = watched && watched == m_filteredMainWindow.data();
	const bool isShellWindow = watched && watched == m_filteredShellWindow.data();
	if (event && event->type() == QEvent::Close &&
		(isMainWindow || isShellWindow) && !m_closeFilters.isEmpty()) {
		bool swallow = false;
		/* Iterate over a copy: callbacks may install/remove filters. */
		const auto filters = m_closeFilters;
		for (const auto& f : filters) {
			if (!f.cb)
				continue;
			int rc = f.cb(f.user_data);
			if (rc != 0)
				swallow = true;
		}
		if (swallow) {
			auto* ce = static_cast<QCloseEvent*>(event);
			ce->ignore();
			// Hide rather than close, as tray-aware apps do.
			if (isMainWindow) {
				if (auto* mw = qobject_cast<QWidget*>(watched))
					mw->hide();
			} else if (auto* window = qobject_cast<QWindow*>(watched)) {
				window->hide();
			}
			return true;
		}
	}
	return QObject::eventFilter(watched, event);
}

void PluginManager::releaseTrayResourcesForModule(void* module_handle)
{
	// Normal unload: deleteLater() the QObjects. Shutdown (m_shutdownDone): only hide() and sever
	// the connections via the per-tray guard; deleteLater() during Application teardown corrupts
	// Qt's signal/slot bookkeeping (see shutdownAll()).

	const bool shuttingDown = m_shutdownDone;

	// Close filters are pure C-struct entries, safe to drop unconditionally.
	for (int i = m_closeFilters.size() - 1; i >= 0; --i) {
		if (m_closeFilters[i].module_handle == module_handle)
			m_closeFilters.removeAt(i);
	}
	if (m_closeFilters.isEmpty() && m_closeFilterInstalled && !shuttingDown) {
		if (m_filteredMainWindow)
			m_filteredMainWindow->removeEventFilter(this);
		if (m_filteredShellWindow)
			m_filteredShellWindow->removeEventFilter(this);
		m_closeFilterInstalled = false;
	}

	// Hide first so the platform plugin lets go of the popup menu before the QMenu is touched. Each
	// tray owns at most one QMenu, torn down with it.
	for (int i = m_trayIcons.size() - 1; i >= 0; --i) {
		if (m_trayIcons[i].module_handle != module_handle)
			continue;
		auto* icon = m_trayIcons[i].icon;
		auto* guard = m_trayIcons[i].guard;
		auto* menu = m_trayIcons[i].menu;
		if (icon) {
			// Detach the context menu before hiding; XCB trays re-enter it during hide.
			icon->setContextMenu(nullptr);
			icon->hide();
			if (!shuttingDown)
				icon->deleteLater();
		}
		if (menu && !shuttingDown)
			menu->deleteLater();
		if (guard && !shuttingDown)
			guard->deleteLater();
		m_trayIcons.removeAt(i);
	}

	releaseSurfacesForModule(module_handle);

	// Deleting a record's guard severs its connection to runningStatusChanged. Shutdown leaves the
	// guard alone and just forgets the record.
	for (int i = m_instanceRunning.size() - 1; i >= 0; --i) {
		if (m_instanceRunning[i].module_handle != module_handle)
			continue;
		QObject* g = m_instanceRunning[i].guard;
		if (g && !shuttingDown)
			g->deleteLater();
		m_instanceRunning.removeAt(i);
	}
}

/* ── ABI 3 — Application signal bridges ──────────────────────────── */

void PluginManager::connectAppSignals()
{
	if (!m_app)
		return;

	/* globalSettingsAboutToOpen -> MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN, so plugins reach it
	 * through the C ABI without linking Application's metaobject. Parented on `this`: severed with
	 * PluginManager. */
	QObject::connect(m_app, &Application::globalSettingsAboutToOpen, this,
					 [this]() {
						 this->dispatchHook(
							 MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN, nullptr);
					 });

	/* instanceSettingsPageCreated -> MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED. Page and instance go
	 * out as void* (no launcher types in the payload); plugins qobject_cast page_handle. The page's
	 * settingsLoaded / settingsAboutToApply are bridged to the ABI 3 hooks; the connection is
	 * anchored on the page. */
	QObject::connect(
		m_app, &Application::instanceSettingsPageCreated, this,
		[this](InstanceSettingsPage* page, BaseInstance* inst) {
			MMCOInstanceSettingsPageEvent ev{};
			QByteArray idBytes;
			if (inst) {
				idBytes = inst->id().toUtf8();
				ev.instance_id = idBytes.constData();
				ev.instance_handle = inst;
			}
			ev.page_handle = page;
			this->dispatchHook(MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED, &ev);

			// INSTANCE_SETTINGS surfaces of this instance are stacked as titled sections in one
			// host "Plugins" group in the Workarounds tab.
			if (page && inst) {
				if (QWidget* section = this->buildPluginsSectionWidget(
						MMCO_UI_ANCHOR_INSTANCE_SETTINGS, inst->id())) {
					if (auto* workaroundsLayout = page->findChild<QVBoxLayout*>(
							QStringLiteral("verticalLayout_8"))) {
						auto* group = new QGroupBox(tr("Plugins"));
						auto* groupLayout = new QVBoxLayout(group);
						groupLayout->addWidget(section);
						const int insertAt = qMax(0, workaroundsLayout->count() - 1);
						workaroundsLayout->insertWidget(insertAt, group);
					} else {
						delete section;
					}
				}
			}

			if (!page)
				return;
			// Capture the raw BaseInstance (owned by InstanceList) and a copy of its id; the
			// connection anchored on `page` stops with the dialog.
			BaseInstance* capturedRaw = inst;
			QByteArray capturedId = idBytes;
			QObject::connect(page, &InstanceSettingsPage::settingsLoaded, page,
							 [this, page, capturedRaw, capturedId]() {
								 MMCOInstanceSettingsPageEvent ev2{};
								 if (capturedRaw) {
									 ev2.instance_id = capturedId.constData();
									 ev2.instance_handle = capturedRaw;
								 }
								 ev2.page_handle = page;
								 this->dispatchHook(
									 MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED,
									 &ev2);
							 });
			QObject::connect(page, &InstanceSettingsPage::settingsAboutToApply,
							 page, [this, page, capturedRaw, capturedId]() {
								 MMCOInstanceSettingsPageEvent ev2{};
								 if (capturedRaw) {
									 ev2.instance_id = capturedId.constData();
									 ev2.instance_handle = capturedRaw;
								 }
								 ev2.page_handle = page;
								 this->dispatchHook(
									 MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING,
									 &ev2);
							 });
		});
}

/* ── S21 — Application Settings (write side, ABI 3+) ─────────────── */

int PluginManager::api_app_setting_set(void* mh, const char* key,
									   const char* value)
{
	auto* r = rt(mh);
	if (!r || !key)
		return -1;
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return -1;
	const QString qKey = QString::fromUtf8(key);
	const QString qVal = value ? QString::fromUtf8(value) : QString();
	if (!app->settings()->contains(qKey))
		return -1;
	app->settings()->set(qKey, qVal);
	return 0;
}

int PluginManager::api_app_setting_register(void* mh, const char* key,
											const char* default_value)
{
	auto* r = rt(mh);
	if (!r || !key)
		return -1;
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return -1;
	const QString qKey = QString::fromUtf8(key);
	if (app->settings()->contains(qKey))
		return 0; /* already registered — treat as success */
	const QString qDef =
		default_value ? QString::fromUtf8(default_value) : QString();
	app->settings()->registerSetting(qKey, qDef);
	return 0;
}

int PluginManager::api_app_setting_contains(void* mh, const char* key)
{
	auto* r = rt(mh);
	if (!r || !key)
		return 0;
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return 0;
	return app->settings()->contains(QString::fromUtf8(key)) ? 1 : 0;
}

/* ── S22 — Themed icon resolution (ABI 3+) ───────────────────────── */

const char* PluginManager::api_ui_themed_icon(void* mh, const char* name)
{
	auto* r = rt(mh);
	if (!r || !name || !*name)
		return nullptr;
	auto* app = r->manager->m_app;
	if (!app)
		return nullptr;

	// A QIcon cannot cross the C ABI: return a name the icon parameters of the UI/tray/menu APIs
	// accept (the launcher icon list's resource path if it owns the name, else the bare name for
	// QIcon::fromTheme()).
	const QString qName = QString::fromUtf8(name);
	if (app->icons()) {
		const QIcon icon = app->icons()->getIcon(qName);
		if (!icon.isNull()) {
			// QIcon does not expose its path; the logical name resolves the same way in the UI
			// APIs.
			r->tempString = qName.toStdString();
			return r->tempString.c_str();
		}
	}
	r->tempString = qName.toStdString();
	return r->tempString.c_str();
}

/* ── S23 — Instance running-state signal bridge (ABI 3+) ─────────── */

int PluginManager::api_instance_running_register(void* mh,
												 const char* instance_id,
												 MMCOInstanceRunningCallback cb,
												 void* ud)
{
	auto* r = rt(mh);
	if (!r || !instance_id || !cb)
		return -1;
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return -1;

	const QString qId = QString::fromUtf8(instance_id);
	auto inst = app->instances()->getInstanceById(qId);
	if (!inst)
		return -1;

	// Replace any registration for this (module, instance) so an edge never fires two callbacks.
	auto& vec = r->manager->m_instanceRunning;
	for (int i = vec.size() - 1; i >= 0; --i) {
		if (vec[i].module_handle == mh && vec[i].instanceId == qId) {
			if (vec[i].guard)
				vec[i].guard->deleteLater();
			vec.removeAt(i);
		}
	}

	auto* guard = new QObject();
	InstanceRunningRecord rec{mh, qId, cb, ud, guard};

	// Captured by value; the id is copied so the callback survives instance rename / re-bind.
	const QByteArray idUtf8 = qId.toUtf8();
	QObject::connect(inst.get(), &BaseInstance::runningStatusChanged, guard,
					 [cb, ud, idUtf8](bool running) {
						 if (cb)
							 cb(ud, idUtf8.constData(), running ? 1 : 0);
					 });

	vec.append(rec);
	return 0;
}

int PluginManager::api_instance_running_unregister(void* mh,
												   const char* instance_id)
{
	auto* r = rt(mh);
	if (!r || !instance_id)
		return -1;
	const QString qId = QString::fromUtf8(instance_id);
	auto& vec = r->manager->m_instanceRunning;
	for (int i = vec.size() - 1; i >= 0; --i) {
		if (vec[i].module_handle == mh && vec[i].instanceId == qId) {
			if (vec[i].guard)
				vec[i].guard->deleteLater();
			vec.removeAt(i);
		}
	}
	return 0; /* idempotent */
}

/* ── S19 trampolines ─────────────────────────────────────────────── */

void* PluginManager::api_tray_create(void* mh, const char* icon_name,
									 const char* tooltip)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return nullptr;

	auto* tray = new QSystemTrayIcon();
	if (icon_name && *icon_name)
		tray->setIcon(mmco_resolve_icon(icon_name));
	if (tooltip)
		tray->setToolTip(QString::fromUtf8(tooltip));

	TrayRecord rec{mh, tray, new QObject()};
	r->manager->m_trayIcons.append(rec);
	return tray;
}

int PluginManager::api_tray_destroy(void* mh, void* tray_handle)
{
	auto* r = rt(mh);
	if (!r || !tray_handle)
		return -1;
	auto& vec = r->manager->m_trayIcons;
	for (int i = 0; i < vec.size(); ++i) {
		if (vec[i].icon == tray_handle && vec[i].module_handle == mh) {
			if (vec[i].icon) {
				vec[i].icon->hide();
				vec[i].icon->deleteLater();
			}
			if (vec[i].guard)
				vec[i].guard->deleteLater();
			vec.removeAt(i);
			return 0;
		}
	}
	return -1;
}

int PluginManager::api_tray_is_available(void* /*mh*/)
{
	return QSystemTrayIcon::isSystemTrayAvailable() ? 1 : 0;
}

int PluginManager::api_tray_set_icon(void* /*mh*/, void* tray_handle,
									 const char* icon_name)
{
	if (!tray_handle)
		return -1;
	static_cast<QSystemTrayIcon*>(tray_handle)
		->setIcon(mmco_resolve_icon(icon_name));
	return 0;
}

int PluginManager::api_tray_set_tooltip(void* /*mh*/, void* tray_handle,
										const char* tooltip)
{
	if (!tray_handle)
		return -1;
	static_cast<QSystemTrayIcon*>(tray_handle)
		->setToolTip(QString::fromUtf8(tooltip ? tooltip : ""));
	return 0;
}

int PluginManager::api_tray_set_visible(void* /*mh*/, void* tray_handle,
										int visible)
{
	if (!tray_handle)
		return -1;
	static_cast<QSystemTrayIcon*>(tray_handle)->setVisible(visible != 0);
	return 0;
}

int PluginManager::api_tray_show_message(void* mh, void* tray_handle,
										 const char* title, const char* message,
										 int icon_type, int msecs)
{
	if (msecs <= 0)
		msecs = 10000;
	const QString qtitle = QString::fromUtf8(title ? title : "");
	const QString qmsg = QString::fromUtf8(message ? message : "");
	const auto micon = mmco_message_icon(icon_type);

	if (tray_handle) {
		auto* tray = static_cast<QSystemTrayIcon*>(tray_handle);
		const bool wasVisible = tray->isVisible();
		if (!wasVisible)
			tray->show();
		tray->showMessage(qtitle, qmsg, micon, msecs);
		if (!wasVisible) {
			// Restore on the next event-loop turn so the notification can be raised.
			QTimer::singleShot(msecs + 200, tray, [tray]() {
				if (tray)
					tray->hide();
			});
		}
		return 0;
	}

	/* Fire-and-forget mode — host owns a transient tray icon. */
	auto* r = rt(mh);
	if (!r)
		return -1;
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return -1;
	auto* tray = new QSystemTrayIcon();
	tray->show();
	tray->showMessage(qtitle, qmsg, micon, msecs);
	QTimer::singleShot(msecs + 500, tray, [tray]() {
		if (tray) {
			tray->hide();
			tray->deleteLater();
		}
	});
	return 0;
}

int PluginManager::api_tray_set_menu(void* mh, void* tray_handle,
									 const char* json_menu_doc,
									 MMCOUiEventCallback cb, void* user_data)
{
	auto* r = rt(mh);
	if (!r || !tray_handle)
		return -1;
	auto* tray = static_cast<QSystemTrayIcon*>(tray_handle);

	TrayRecord* rec = nullptr;
	for (auto& tr : r->manager->m_trayIcons) {
		if (tr.icon == tray_handle && tr.module_handle == mh) {
			rec = &tr;
			break;
		}
	}
	if (!rec)
		return -1;

	if (!json_menu_doc) {
		// A null document detaches the menu.
		tray->setContextMenu(nullptr);
		if (rec->menu) {
			rec->menu->deleteLater();
			rec->menu = nullptr;
		}
		return 0;
	}

	// One host-owned QMenu per tray, rebuilt in place on every call.
	if (!rec->menu)
		rec->menu = new QMenu();

	PluginUiRenderer::EventSink sink;
	if (cb) {
		sink = [cb, user_data](const QString& nodeId, const QString& event,
							   const QString& valueJson) {
			const QByteArray nid = nodeId.toUtf8();
			const QByteArray ev = event.toUtf8();
			const QByteArray val = valueJson.toUtf8();
			cb(user_data, "tray", nid.constData(), ev.constData(),
			  val.constData());
		};
	}
	if (!PluginUiRenderer::buildTrayMenu(rec->menu, QString::fromUtf8(json_menu_doc),
										sink))
		return -1;

	QMenu* menu = rec->menu;
#ifdef Q_OS_WIN
	tray->setContextMenu(nullptr);
	QObject::disconnect(tray, &QSystemTrayIcon::activated, menu, nullptr);
	QObject::connect(
		tray, &QSystemTrayIcon::activated, menu,
		[menu](QSystemTrayIcon::ActivationReason reason) {
			if (reason != QSystemTrayIcon::Context)
				return;
			menu->winId();
			::SetForegroundWindow(
				reinterpret_cast<HWND>(menu->winId()));
			menu->popup(QCursor::pos());
		});
#else
	tray->setContextMenu(menu);
#endif
	return 0;
}

int PluginManager::api_tray_set_activation_cb(void* mh, void* tray_handle,
											  MMCOTrayActivationCallback cb,
											  void* ud)
{
	auto* r = rt(mh);
	if (!r || !tray_handle)
		return -1;
	auto& vec = r->manager->m_trayIcons;
	for (auto& rec : vec) {
		if (rec.icon != tray_handle || rec.module_handle != mh)
			continue;
		// Deleting the per-tray guard severs every previous connection.
		if (rec.guard)
			rec.guard->deleteLater();
		rec.guard = new QObject();
		if (!cb)
			return 0;
		QObject::connect(rec.icon, &QSystemTrayIcon::activated, rec.guard,
						 [cb, ud](QSystemTrayIcon::ActivationReason reason) {
							 cb(ud, static_cast<int>(reason));
						 });
		return 0;
	}
	return -1;
}

/* ── S20 trampolines ─────────────────────────────────────────────── */

int PluginManager::api_main_window_install_close_filter(
	void* mh, MMCOMainWindowCloseCallback cb, void* user_data)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto* self = r->manager;

	if (!cb) {
		for (int i = self->m_closeFilters.size() - 1; i >= 0; --i) {
			if (self->m_closeFilters[i].module_handle == mh)
				self->m_closeFilters.removeAt(i);
		}
		if (self->m_closeFilters.isEmpty() && self->m_closeFilterInstalled) {
			if (self->m_filteredMainWindow)
				self->m_filteredMainWindow->removeEventFilter(self);
			if (self->m_filteredShellWindow)
				self->m_filteredShellWindow->removeEventFilter(self);
			self->m_closeFilterInstalled = false;
		}
		return 0;
	}

	if (!self->resolveMainWindow() && !self->resolveShellWindow())
		return -1;

	self->m_closeFilters.append({mh, cb, user_data});
	self->ensureCloseFilterInstalled();
	return 0;
}

int PluginManager::api_main_window_show(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto* self = r->manager;
	if (QWidget* mw = self->resolveMainWindow()) {
		mw->show();
		mw->raise();
		mw->activateWindow();
		return 0;
	}
	if (QWindow* window = self->resolveShellWindow()) {
		window->show();
		window->raise();
		window->requestActivate();
		return 0;
	}
	return -1;
}

int PluginManager::api_main_window_hide(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto* self = r->manager;
	if (QWidget* mw = self->resolveMainWindow()) {
		mw->hide();
		return 0;
	}
	if (QWindow* window = self->resolveShellWindow()) {
		window->hide();
		return 0;
	}
	return -1;
}

int PluginManager::api_main_window_is_visible(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return 0;
	auto* self = r->manager;
	if (QWidget* mw = self->resolveMainWindow())
		return mw->isVisible() ? 1 : 0;
	if (QWindow* window = self->resolveShellWindow())
		return window->isVisible() ? 1 : 0;
	return 0;
}

/* ── S24 — Per-instance settings (ABI 3+) ────────────────────────── */

namespace
{
	// Resolves an instance by id; nullptr if the id is unknown or the instance list is not ready.
	InstancePtr resolveInstance(Application* app, const char* instance_id)
	{
		if (!app || !app->instances() || !instance_id)
			return {};
		return app->instances()->getInstanceById(
			QString::fromUtf8(instance_id));
	}
} // namespace

const char* PluginManager::api_instance_setting_get(void* mh,
													const char* instance_id,
													const char* key)
{
	auto* r = rt(mh);
	if (!r || !key)
		return nullptr;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	if (!inst || !inst->settings())
		return nullptr;
	const QString qKey = QString::fromUtf8(key);
	if (!inst->settings()->contains(qKey))
		return nullptr;
	r->tempString = inst->settings()->get(qKey).toString().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_setting_set(void* mh, const char* instance_id,
											const char* key, const char* value)
{
	auto* r = rt(mh);
	if (!r || !key)
		return -1;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	if (!inst || !inst->settings())
		return -1;
	const QString qKey = QString::fromUtf8(key);
	const QString qVal = value ? QString::fromUtf8(value) : QString();
	if (!inst->settings()->contains(qKey))
		return -1;
	inst->settings()->set(qKey, qVal);
	return 0;
}

int PluginManager::api_instance_setting_register(void* mh,
												 const char* instance_id,
												 const char* key,
												 const char* default_value)
{
	auto* r = rt(mh);
	if (!r || !key)
		return -1;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	if (!inst || !inst->settings())
		return -1;
	const QString qKey = QString::fromUtf8(key);
	if (inst->settings()->contains(qKey))
		return 0;
	const QString qDef =
		default_value ? QString::fromUtf8(default_value) : QString();
	inst->settings()->registerSetting(qKey, qDef);
	return 0;
}

int PluginManager::api_instance_setting_register_override(
	void* mh, const char* instance_id, const char* key, const char* gate_key)
{
	auto* r = rt(mh);
	if (!r || !key || !gate_key)
		return -1;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	auto* app = r->manager->m_app;
	if (!inst || !inst->settings() || !app || !app->settings())
		return -1;

	const QString qKey = QString::fromUtf8(key);
	const QString qGate = QString::fromUtf8(gate_key);

	// Seed the per-instance gate to false (use the global default) the first time.
	auto gate = inst->settings()->getSetting(qGate);
	if (!gate)
		gate = inst->settings()->registerSetting(qGate, false);
	if (!gate)
		return -1;

	// Skip registerOverride() when the key is already registered.
	if (inst->settings()->contains(qKey))
		return 0;
	auto original = app->settings()->getSetting(qKey);
	if (!original)
		return -1;
	inst->settings()->registerOverride(original, gate);
	return 0;
}

int PluginManager::api_instance_setting_reset(void* mh, const char* instance_id,
											  const char* key)
{
	auto* r = rt(mh);
	if (!r || !key)
		return -1;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	if (!inst || !inst->settings())
		return -1;
	inst->settings()->reset(QString::fromUtf8(key));
	return 0;
}

int PluginManager::api_instance_setting_contains(void* mh,
												 const char* instance_id,
												 const char* key)
{
	auto* r = rt(mh);
	if (!r || !key)
		return 0;
	auto inst = resolveInstance(r->manager->m_app, instance_id);
	if (!inst || !inst->settings())
		return 0;
	return inst->settings()->contains(QString::fromUtf8(key)) ? 1 : 0;
}

/* ── S25 — Account / skin / cape access (ABI 3+) ─────────────────── */

namespace
{
	// Resolves an account by the id account_get_profile_id() returns (linear scan).
	MinecraftAccountPtr resolveAccount(Application* app, const char* account_id)
	{
		if (!app || !account_id)
			return {};
		auto accounts = app->accounts();
		if (!accounts)
			return {};
		const QString qId = QString::fromUtf8(account_id);
		for (int i = 0; i < accounts->count(); ++i) {
			auto a = accounts->at(i);
			if (a && a->profileId() == qId)
				return a;
		}
		return {};
	}
} // namespace

const char* PluginManager::api_account_get_id_by_index(void* mh, int index)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return nullptr;
	if (index < 0 || index >= app->accounts()->count())
		return nullptr;
	auto a = app->accounts()->at(index);
	if (!a)
		return nullptr;
	r->tempString = a->profileId().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_account_is_msa_by_id(void* mh, const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return 0;
	auto a = resolveAccount(r->manager->m_app, account_id);
	return (a && a->isMSA()) ? 1 : 0;
}

const char* PluginManager::api_account_get_access_token(void* mh,
														const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a)
		return nullptr;
	r->tempString = a->accessToken().toStdString();
	return r->tempString.c_str();
}

const char*
PluginManager::api_account_get_current_cape_id(void* mh, const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return nullptr;
	r->tempString =
		a->accountData()->minecraftProfile.currentCape.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_account_get_skin_variant(void* mh,
														const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return nullptr;
	r->tempString =
		a->accountData()->minecraftProfile.skin.variant.toStdString();
	return r->tempString.c_str();
}

/* Skin and cape PNG blobs are returned through ModuleRuntime::tempString (std::string is
 * byte-clean). The caller treats it as void*; valid until the next API call on the same module. */
int64_t PluginManager::api_account_get_skin_blob(void* mh,
												 const char* account_id,
												 const void** out_ptr)
{
	auto* r = rt(mh);
	if (!r || !out_ptr) {
		if (out_ptr)
			*out_ptr = nullptr;
		return -1;
	}
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData()) {
		*out_ptr = nullptr;
		return -1;
	}
	const QByteArray& blob = a->accountData()->minecraftProfile.skin.data;
	r->tempString.assign(blob.constData(), blob.size());
	*out_ptr = r->tempString.data();
	return blob.size();
}

int PluginManager::api_account_cape_count(void* mh, const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return -1;
	return a->accountData()->minecraftProfile.capes.size();
}

namespace
{
	// Index into the profile service's own cape order (MinecraftProfile::capes).
	const Cape* capeAt(MinecraftAccountPtr a, int index)
	{
		if (!a || !a->accountData() || index < 0)
			return nullptr;
		const auto& capes = a->accountData()->minecraftProfile.capes;
		if (index >= capes.size())
			return nullptr;
		return &capes.at(index);
	}
} // namespace

const char* PluginManager::api_account_cape_get_id(void* mh,
												   const char* account_id,
												   int index)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto a = resolveAccount(r->manager->m_app, account_id);
	const Cape* c = capeAt(a, index);
	if (!c)
		return nullptr;
	r->tempString = c->id.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_account_cape_get_alias(void* mh,
													  const char* account_id,
													  int index)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto a = resolveAccount(r->manager->m_app, account_id);
	const Cape* c = capeAt(a, index);
	if (!c)
		return nullptr;
	r->tempString = c->alias.toStdString();
	return r->tempString.c_str();
}

int64_t PluginManager::api_account_cape_get_blob(void* mh,
												 const char* account_id,
												 int index,
												 const void** out_ptr)
{
	auto* r = rt(mh);
	if (!r || !out_ptr) {
		if (out_ptr)
			*out_ptr = nullptr;
		return -1;
	}
	auto a = resolveAccount(r->manager->m_app, account_id);
	const Cape* c = capeAt(a, index);
	if (!c) {
		*out_ptr = nullptr;
		return -1;
	}
	r->tempString.assign(c->data.constData(), c->data.size());
	*out_ptr = r->tempString.data();
	return c->data.size();
}

int PluginManager::api_account_set_skin_variant(void* mh,
												const char* account_id,
												const char* variant)
{
	auto* r = rt(mh);
	if (!r || !variant)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return -1;
	a->accountData()->minecraftProfile.skin.variant =
		QString::fromUtf8(variant);
	return 0;
}

int PluginManager::api_account_set_current_cape(void* mh,
												const char* account_id,
												const char* cape_id)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return -1;
	a->accountData()->minecraftProfile.currentCape =
		cape_id ? QString::fromUtf8(cape_id) : QString();
	return 0;
}

int PluginManager::api_account_set_skin_blob(void* mh, const char* account_id,
											 const void* data, int64_t size)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a || !a->accountData())
		return -1;
	if (data && size > 0)
		a->accountData()->minecraftProfile.skin.data =
			QByteArray(static_cast<const char*>(data), static_cast<int>(size));
	else
		a->accountData()->minecraftProfile.skin.data.clear();
	return 0;
}

/* ── S26 — Synchronous task helpers (ABI 3+) ─────────────────────── */

int PluginManager::api_account_skin_upload(void* mh, const char* account_id,
										   const void* png_bytes, int64_t size,
										   const char* variant)
{
	auto* r = rt(mh);
	if (!r || !png_bytes || size <= 0 || !variant)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a)
		return -1;

	const QByteArray bytes(static_cast<const char*>(png_bytes),
						   static_cast<int>(size));
	const QString variantStr = QString::fromUtf8(variant).trimmed().toUpper();
	const SkinUpload::Model model = variantStr == QLatin1String("SLIM") ||
											variantStr == QLatin1String("ALEX")
										? SkinUpload::ALEX
										: SkinUpload::STEVE;

	auto task = shared_qobject_ptr<SkinUpload>(
		new SkinUpload(nullptr, a->accessToken(), bytes, model));
	if (!runAccountTask(r->manager->m_app, task.get(), tr("Uploading skin…"),
						"account_skin_set"))
		return -1;
	return 0;
}

int PluginManager::api_account_skin_reset(void* mh, const char* account_id)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a)
		return -1;

	auto task = shared_qobject_ptr<SkinDelete>(
		new SkinDelete(nullptr, a->accessToken()));
	if (!runAccountTask(r->manager->m_app, task.get(), tr("Resetting skin…"),
						"account_skin_reset"))
		return -1;
	return 0;
}

int PluginManager::api_account_cape_set(void* mh, const char* account_id,
										const char* cape_id)
{
	auto* r = rt(mh);
	if (!r)
		return -1;
	auto a = resolveAccount(r->manager->m_app, account_id);
	if (!a)
		return -1;

	const QString cape = cape_id ? QString::fromUtf8(cape_id) : QString();
	auto task = shared_qobject_ptr<CapeChange>(
		new CapeChange(nullptr, a->accessToken(), cape));
	if (!runAccountTask(r->manager->m_app, task.get(), tr("Changing cape…"),
						"account_cape_set"))
		return -1;
	return 0;
}

/* ── S27 — Icon list enumeration (ABI 3+) ─────────────────────── */

int PluginManager::api_icon_list_count(void* mh)
{
	auto* r = rt(mh);
	if (!r)
		return 0;
	auto* app = r->manager->m_app;
	if (!app || !app->icons())
		return 0;
	return app->icons()->rowCount();
}

const char* PluginManager::api_icon_list_get_key(void* mh, int index)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto* app = r->manager->m_app;
	if (!app || !app->icons())
		return nullptr;
	auto* model = app->icons().get();
	if (index < 0 || index >= model->rowCount())
		return nullptr;
	QModelIndex idx = model->index(index, 0);
	r->tempString = model->data(idx, Qt::UserRole).toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_icon_list_get_name(void* mh, int index)
{
	auto* r = rt(mh);
	if (!r)
		return nullptr;
	auto* app = r->manager->m_app;
	if (!app || !app->icons())
		return nullptr;
	auto* model = app->icons().get();
	if (index < 0 || index >= model->rowCount())
		return nullptr;
	QModelIndex idx = model->index(index, 0);
	r->tempString = model->data(idx, Qt::DisplayRole).toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_icon_list_get_file_path(void* mh,
													   const char* icon_key)
{
	auto* r = rt(mh);
	if (!r || !icon_key)
		return nullptr;
	auto* app = r->manager->m_app;
	if (!app || !app->icons())
		return nullptr;
	const MMCIcon* ic = app->icons()->icon(QString::fromUtf8(icon_key));
	if (!ic)
		return nullptr;
	r->tempString = ic->getFilePath().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_icon_list_save_png(void* mh, const char* icon_key,
										  const char* dest_path)
{
	auto* r = rt(mh);
	if (!r || !icon_key || !dest_path)
		return -1;
	auto* app = r->manager->m_app;
	if (!app || !app->icons())
		return -1;
	app->icons()->saveIcon(QString::fromUtf8(icon_key),
						   QString::fromUtf8(dest_path), "PNG");
	return QFileInfo::exists(QString::fromUtf8(dest_path)) ? 0 : -1;
}

/* ── Section 31: Subprocess execution ───────────────────────────────── */

int PluginManager::api_process_run(void* mh, const char* program,
								   const char* const* args, int arg_count,
								   const char* working_dir,
								   const char* stdin_data, int stdin_size,
								   char* out_buf, int out_buf_size,
								   int* out_exit_code, int timeout_ms)
{
	auto* r = rt(mh);
	if (!r || !program || program[0] == '\0')
		return -3;
	if (arg_count < 0 || (arg_count > 0 && !args))
		return -3;
	if (out_buf && out_buf_size > 0)
		out_buf[0] = '\0';

	QStringList argList;
	argList.reserve(arg_count);
	for (int i = 0; i < arg_count; ++i) {
		// argv elements are passed verbatim: no shell parsing, so no shell injection.
		argList << QString::fromUtf8(args[i] ? args[i] : "");
	}

	QProcess proc;
	if (working_dir && working_dir[0] != '\0')
		proc.setWorkingDirectory(QString::fromUtf8(working_dir));
	proc.setProgram(QString::fromUtf8(program));
	proc.setArguments(argList);
	// Separate stderr so it never pollutes the stdout the plugin parses.
	proc.setProcessChannelMode(QProcess::SeparateChannels);

	proc.start();
	if (!proc.waitForStarted(5000)) {
		qWarning() << "[PluginManager] process_run: failed to start"
				   << QString::fromUtf8(program) << "-" << proc.errorString();
		return -1;
	}

	if (stdin_data && stdin_size > 0) {
		proc.write(stdin_data, stdin_size);
	}
	proc.closeWriteChannel();

	const int effectiveTimeout = (timeout_ms > 0) ? timeout_ms : 30000;
	if (!proc.waitForFinished(effectiveTimeout)) {
		qWarning() << "[PluginManager] process_run: timed out after"
				   << effectiveTimeout << "ms:"
				   << QString::fromUtf8(program);
		proc.kill();
		proc.waitForFinished(2000);
		return -2;
	}

	const QByteArray outBytes = proc.readAllStandardOutput();
	if (out_buf && out_buf_size > 0) {
		const int copyLen =
			qMin(outBytes.size(), out_buf_size - 1); // leave room for NUL
		if (copyLen > 0)
			memcpy(out_buf, outBytes.constData(),
				   static_cast<size_t>(copyLen));
		out_buf[copyLen] = '\0';
	}

	if (out_exit_code)
		*out_exit_code = (proc.exitStatus() == QProcess::NormalExit)
							 ? proc.exitCode()
							 : -1;
	return 0;
}

/* PluginPage MOC — required because PluginPage has Q_OBJECT */
#include "PluginManager.moc"
