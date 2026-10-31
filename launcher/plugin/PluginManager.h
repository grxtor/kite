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

#pragma once

#include "plugin/PluginLoader.h"
#include "plugin/PluginMetadata.h"
#include "plugin/PluginHooks.h"
#include "plugin/PluginAPI.h"
#include "plugin/PluginUiRenderer.h"

#include "news/NewsEntry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QMutex>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QMultiMap>
#include <QSet>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <functional>

class BasePage;
class NewsChecker;
class QAction;
class QEvent;
class QMenu;
class QSystemTrayIcon;
class QWidget;
class QWindow;

/* PluginManager owns the plugin lifecycle and bridges loaded .mmco modules to Kite: discovery via
 * PluginLoader, building each MMCOContext, mmco_init()/mmco_unload(), hook dispatch, and the API
 * functions behind MMCOContext. */

class Application;

class PluginManager : public QObject
{
	Q_OBJECT

  public:
	explicit PluginManager(Application* app, QObject* parent = nullptr);
	~PluginManager() override;

	/* Discovers and initialises all modules; called once at startup, after subsystems are ready. */
	void initializeAll();

	/* Calls mmco_unload() on all modules in reverse load order. */
	void shutdownAll();

	/* Dispatches a hook to all registered listeners. Returns true if any callback cancelled
	 * (non-zero return). */
	bool dispatchHook(uint32_t hook_id, void* payload = nullptr);

	const QVector<PluginMetadata>& modules() const
	{
		return m_modules;
	}
	int moduleCount() const
	{
		return m_modules.size();
	}

	/* The disabled set is persisted in the application settings under "plugins.disabled"
	 * (comma-separated module names, case-insensitive). Toggling loads or unloads nothing at
	 * runtime; it takes effect on the next launcher start (PluginsDialog warns the user). */
	bool isModuleDisabled(const QString& moduleName) const;
	void setModuleDisabled(const QString& moduleName, bool disabled);
	QSet<QString> disabledModuleNames() const;

	/* ABI 5 declarative UI surfaces. Each ui_surface_create() is recorded as a SurfaceInfo (anchor,
	 * optional instance-id context, title/icon, current "mmco-ui/1" JSON), whether or not a widget
	 * view is on screen. A renderer reads the snapshot, watches surfacesChanged() and reports
	 * events through deliverUiEvent(). */
	struct SurfaceInfo {
		void* handle = nullptr; /* opaque; same value ui_surface_create returned */
		QString surfaceId;
		int anchor = 0; /* MMCOUiAnchor */
		QString anchorContext; /* instance id, or empty for GLOBAL_SETTINGS */
		QString title;
		QString iconName;
		QString document; /* current "mmco-ui/1" JSON, as text */
	};

	/* Snapshot of every live surface. `anchor` filters to one MMCOUiAnchor (-1 = any);
	 * `anchorContext` to an exact instance id (a null QString = any; an empty non-null QString
	 * matches only GLOBAL_SETTINGS surfaces). */
	QList<SurfaceInfo> surfaces(int anchor = -1,
							   const QString& anchorContext = QString()) const;

	/* Delivers a UI event to a surface's MMCOUiEventCallback, as a rendered widget would. No-op if
	 * surfaceId names no live surface. */
	void deliverUiEvent(const QString& surfaceId, const QString& nodeId,
						const QString& event, const QString& valueJson);

	/* Host-internal widget builders for InstancePageProvider, the global-settings page provider and
	 * the INSTANCE_SETTINGS_PAGE_CREATED bridge. Each call renders fresh widgets from the surface's
	 * current document; returned pages and widgets are caller-owned. */

	/* One BasePage per INSTANCE_PAGE surface anchored to `instanceId`, shown as tabs in the
	 * instance window. */
	QList<BasePage*> createInstancePages(const QString& instanceId);

	/* A single "Plugins" BasePage stacking every GLOBAL_SETTINGS surface as a titled section, or
	 * nullptr if there are none. */
	BasePage* createGlobalSettingsPluginsPage();

	/* Per-module scratch buffer behind every `const char*` getter: the returned pointer stays valid
	 * until the module's next API call *on the same thread*. The bytes live in thread-local storage
	 * keyed by the instance, so background hook threads never share a buffer. assign(), c_str(),
	 * data() and assignment forward to the calling thread's copy. */
	class ScratchString
	{
	  public:
		ScratchString() = default;
		~ScratchString();

		/* Non-copyable: the identity of the object is the storage key. */
		ScratchString(const ScratchString&) = delete;
		ScratchString& operator=(const ScratchString&) = delete;

		ScratchString& operator=(std::string value);

		/* Binary-safe fill, used for skin/cape PNG blobs. */
		void assign(const char* bytes, size_t size);

		const char* c_str() const;
		const char* data() const;
	};

	/* The opaque object behind module_handle; lets static API callbacks find their way back to the
	 * manager. Public so helpers in the .cpp can use it. */
	struct ModuleRuntime {
		PluginManager* manager;
		int moduleIndex;
		/* Written once in initializeAll(); read-only afterwards, so safe from any thread. */
		std::string dataDir;
		ScratchString tempString;
	};

	Application* m_app;

	/* Installed on the main window when a plugin registers a close callback via
	 * main_window_install_close_filter(); lets the callbacks accept or swallow the QCloseEvent. */
	bool eventFilter(QObject* watched, QEvent* event) override;

  signals:
	void moduleLoaded(const QString& name);
	void moduleUnloaded(const QString& name);
	void moduleError(const QString& name, const QString& error);
	/* Any surface was created, updated or destroyed; surfaces() readers re-read on this. */
	void surfacesChanged();

  private:
	MMCOContext buildContext(PluginMetadata& meta);

	void ensurePluginDataDir(PluginMetadata& meta);

	/* API implementation: static so they can be C function pointers; they resolve the PluginManager
	 * through module_handle, a ModuleRuntime pointer. */

	struct HookRegistration {
		void* module_handle;
		MMCOHookCallback callback;
		void* user_data;
		/* MMCO_HOOK_FLAG_*; 0 for plain hook_register (inline). */
		uint32_t flags;
	};

	/* Runs the background-flagged callbacks of one dispatch as a SequentialTask behind a
	 * ProgressDialog, one row per module. Returns true if one vetoed. */
	bool runBackgroundHooks(uint32_t hook_id, void* payload,
							const QVector<HookRegistration>& regs);

	/* Static API trampolines — Section 1: Logging */
	static void api_log_info(void* mh, const char* msg);
	static void api_log_warn(void* mh, const char* msg);
	static void api_log_error(void* mh, const char* msg);
	static void api_log_debug(void* mh, const char* msg);

	/* Section 2: Hooks */
	static int api_hook_register(void* mh, uint32_t hook_id,
								 MMCOHookCallback cb, void* ud);
	static int api_hook_unregister(void* mh, uint32_t hook_id,
								   MMCOHookCallback cb);

	/* Section 32: Background hooks + progress reporting */
	static int api_hook_register_ex(void* mh, uint32_t hook_id,
									MMCOHookCallback cb, void* ud,
									uint32_t flags);
	static int api_progress_report(void* mh, const char* status,
								   const char* details, int64_t current,
								   int64_t total);

	/* Section 3: Settings */
	static const char* api_setting_get(void* mh, const char* key);
	static int api_setting_set(void* mh, const char* key, const char* value);

	/* Section 4: Instance Management */
	static int api_instance_count(void* mh);
	static const char* api_instance_get_id(void* mh, int index);
	static const char* api_instance_get_name(void* mh, const char* id);
	static int api_instance_set_name(void* mh, const char* id,
									 const char* name);
	static const char* api_instance_get_path(void* mh, const char* id);
	static const char* api_instance_get_game_root(void* mh, const char* id);
	static const char* api_instance_get_mods_root(void* mh, const char* id);
	static const char* api_instance_get_icon_key(void* mh, const char* id);
	static int api_instance_set_icon_key(void* mh, const char* id,
										 const char* key);
	static const char* api_instance_get_type(void* mh, const char* id);
	static const char* api_instance_get_notes(void* mh, const char* id);
	static int api_instance_set_notes(void* mh, const char* id,
									  const char* notes);
	static int api_instance_is_running(void* mh, const char* id);
	static int api_instance_can_launch(void* mh, const char* id);
	static int api_instance_has_crashed(void* mh, const char* id);
	static int api_instance_has_update(void* mh, const char* id);
	static int api_instance_set_update_available(void* mh, const char* id,
												 int value);
	static int api_instance_component_set_version(void* mh, const char* id,
												  const char* uid,
												  const char* version);
	static int api_http_get_with_headers(void* mh, const char* url,
										 const char* const* headers,
										 int header_count,
										 MMCOHttpCallback callback,
										 void* user_data);
	/* Section 31: Subprocess execution */
	static int api_process_run(void* mh, const char* program,
							   const char* const* args, int arg_count,
							   const char* working_dir, const char* stdin_data,
							   int stdin_size, char* out_buf, int out_buf_size,
							   int* out_exit_code, int timeout_ms);
	static int64_t api_instance_get_total_play_time(void* mh, const char* id);
	static int64_t api_instance_get_last_play_time(void* mh, const char* id);
	static int64_t api_instance_get_last_launch(void* mh, const char* id);
	static int api_instance_launch(void* mh, const char* id, int online);
	static int api_instance_kill(void* mh, const char* id);
	static int api_instance_delete(void* mh, const char* id);
	static const char* api_instance_get_group(void* mh, const char* id);
	static int api_instance_set_group(void* mh, const char* id,
									  const char* group);
	static int api_instance_group_count(void* mh);
	static const char* api_instance_group_at(void* mh, int index);
	static int api_instance_component_count(void* mh, const char* id);
	static const char* api_instance_component_get_uid(void* mh, const char* id,
													  int idx);
	static const char* api_instance_component_get_name(void* mh, const char* id,
													   int idx);
	static const char*
	api_instance_component_get_version(void* mh, const char* id, int idx);
	static const char* api_instance_get_mc_version(void* mh, const char* id);
	static const char* api_instance_get_jar_mods_dir(void* mh, const char* id);
	static const char* api_instance_get_resource_packs_dir(void* mh,
														   const char* id);
	static const char* api_instance_get_texture_packs_dir(void* mh,
														  const char* id);
	static const char* api_instance_get_shader_packs_dir(void* mh,
														 const char* id);
	static const char* api_instance_get_worlds_dir(void* mh, const char* id);

	/* Section 5: Mod Management */
	static int api_mod_count(void* mh, const char* inst, const char* type);
	static const char* api_mod_get_name(void* mh, const char* inst,
										const char* type, int idx);
	static const char* api_mod_get_version(void* mh, const char* inst,
										   const char* type, int idx);
	static const char* api_mod_get_filename(void* mh, const char* inst,
											const char* type, int idx);
	static const char* api_mod_get_description(void* mh, const char* inst,
											   const char* type, int idx);
	static int api_mod_is_enabled(void* mh, const char* inst, const char* type,
								  int idx);
	static int api_mod_set_enabled(void* mh, const char* inst, const char* type,
								   int idx, int e);
	static int api_mod_remove(void* mh, const char* inst, const char* type,
							  int idx);
	static int api_mod_install(void* mh, const char* inst, const char* type,
							   const char* path);
	static int api_mod_refresh(void* mh, const char* inst, const char* type);

	/* Section 6: World Management */
	static int api_world_count(void* mh, const char* inst);
	static const char* api_world_get_name(void* mh, const char* inst, int idx);
	static const char* api_world_get_folder(void* mh, const char* inst,
											int idx);
	static int64_t api_world_get_seed(void* mh, const char* inst, int idx);
	static int api_world_get_game_type(void* mh, const char* inst, int idx);
	static int64_t api_world_get_last_played(void* mh, const char* inst,
											 int idx);
	static int api_world_delete(void* mh, const char* inst, int idx);
	static int api_world_rename(void* mh, const char* inst, int idx,
								const char* name);
	static int api_world_install(void* mh, const char* inst, const char* path);
	static int api_world_refresh(void* mh, const char* inst);

	/* Section 7: Account Management */
	static int api_account_count(void* mh);
	static const char* api_account_get_profile_name(void* mh, int idx);
	static const char* api_account_get_profile_id(void* mh, int idx);
	static const char* api_account_get_type(void* mh, int idx);
	static int api_account_get_state(void* mh, int idx);
	static int api_account_is_active(void* mh, int idx);
	static int api_account_get_default_index(void* mh);

	/* Section 8: Java Management */
	static int api_java_count(void* mh);
	static const char* api_java_get_version(void* mh, int idx);
	static const char* api_java_get_arch(void* mh, int idx);
	static const char* api_java_get_path(void* mh, int idx);
	static int api_java_is_recommended(void* mh, int idx);
	static const char* api_instance_get_java_version(void* mh, const char* id);

	/* Section 9: Filesystem */
	static const char* api_fs_plugin_data_dir(void* mh);
	static int64_t api_fs_read(void* mh, const char* rel, void* buf, size_t sz);
	static int api_fs_write(void* mh, const char* rel, const void* data,
							size_t sz);
	static int api_fs_exists(void* mh, const char* rel);
	static int api_fs_mkdir(void* mh, const char* path);
	static int api_fs_exists_abs(void* mh, const char* path);
	static int api_fs_remove(void* mh, const char* path);
	static int api_fs_copy_file(void* mh, const char* src, const char* dst);
	static int64_t api_fs_file_size(void* mh, const char* path);
	static int api_fs_list_dir(void* mh, const char* path, int type,
							   MMCODirEntryCallback cb, void* ud);

	/* Section 10: Zip */
	static int api_zip_compress_dir(void* mh, const char* zip, const char* dir);
	static int api_zip_extract(void* mh, const char* zip, const char* target);

	/* Section 11: Network */
	static int api_http_get(void* mh, const char* url, MMCOHttpCallback cb,
							void* ud);
	static int api_http_post(void* mh, const char* url, const void* body,
							 size_t body_sz, const char* ct,
							 MMCOHttpCallback cb, void* ud);

	/* Section 12: UI Dialogs */
	static void api_ui_show_message(void* mh, int type, const char* title,
									const char* msg);
	static int api_ui_add_menu_item(void* mh, void* menu_handle,
									const char* label, const char* icon,
									MMCOMenuActionCallback cb, void* ud);
	static const char* api_ui_file_open_dialog(void* mh, const char* title,
											   const char* filter);
	static const char* api_ui_file_save_dialog(void* mh, const char* title,
											   const char* def,
											   const char* filter);
	static const char* api_ui_input_dialog(void* mh, const char* title,
										   const char* prompt, const char* def);
	static int api_ui_confirm_dialog(void* mh, const char* title,
									 const char* msg);

	/* Section 33: Declarative UI surfaces (ABI 5) */
	static void* api_ui_surface_create(void* mh, int anchor,
									   const char* anchor_context,
									   const char* title, const char* icon_name,
									   const char* json_doc,
									   MMCOUiEventCallback cb, void* user_data);
	static int api_ui_surface_update(void* mh, void* surface,
									 const char* json_doc);
	static int api_ui_surface_set(void* mh, void* surface, const char* node_id,
								  const char* json_props);
	static int api_ui_surface_set_rows(void* mh, void* surface,
									   const char* node_id,
									   const char* json_rows);
	static int api_ui_surface_destroy(void* mh, void* surface);
	static int api_ui_modal_run(void* mh, const char* title,
								const char* json_doc, char* out_result_json,
								int out_buf_size);

	/* Section 14: Utility */
	static const char* api_get_app_version(void* mh);
	static const char* api_get_app_name(void* mh);
	static int64_t api_get_timestamp(void* mh);

	/* Section 15: Launch Modifiers */
	static int api_launch_set_env(void* mh, const char* key, const char* value);
	static int api_launch_prepend_wrapper(void* mh, const char* wrapper_cmd);

	/* Section 16: Application Settings */
	static const char* api_app_setting_get(void* mh, const char* key);

	/* Section 21: Application Settings — write side (ABI 3+) */
	static int api_app_setting_set(void* mh, const char* key,
								   const char* value);
	static int api_app_setting_register(void* mh, const char* key,
										const char* default_value);
	static int api_app_setting_contains(void* mh, const char* key);

	/* Section 22: Themed icon resolution (ABI 3+) */
	static const char* api_ui_themed_icon(void* mh, const char* name);

	/* Section 23: Instance running-state signal bridge (ABI 3+) */
	static int api_instance_running_register(void* mh, const char* instance_id,
											 MMCOInstanceRunningCallback cb,
											 void* ud);
	static int api_instance_running_unregister(void* mh,
											   const char* instance_id);

	/* Section 24: Per-instance settings (ABI 3+) */
	static const char* api_instance_setting_get(void* mh,
												const char* instance_id,
												const char* key);
	static int api_instance_setting_set(void* mh, const char* instance_id,
										const char* key, const char* value);
	static int api_instance_setting_register(void* mh, const char* instance_id,
											 const char* key,
											 const char* default_value);
	static int api_instance_setting_register_override(void* mh,
													  const char* instance_id,
													  const char* key,
													  const char* gate_key);
	static int api_instance_setting_reset(void* mh, const char* instance_id,
										  const char* key);
	static int api_instance_setting_contains(void* mh, const char* instance_id,
											 const char* key);

	/* Section 25: Account / skin / cape access (ABI 3+) */
	static const char* api_account_get_id_by_index(void* mh, int index);
	static int api_account_is_msa_by_id(void* mh, const char* account_id);
	static const char* api_account_get_access_token(void* mh,
													const char* account_id);
	static const char* api_account_get_current_cape_id(void* mh,
													   const char* account_id);
	static const char* api_account_get_skin_variant(void* mh,
													const char* account_id);
	static int64_t api_account_get_skin_blob(void* mh, const char* account_id,
											 const void** out_ptr);
	static int api_account_cape_count(void* mh, const char* account_id);
	static const char* api_account_cape_get_id(void* mh, const char* account_id,
											   int index);
	static const char*
	api_account_cape_get_alias(void* mh, const char* account_id, int index);
	static int64_t api_account_cape_get_blob(void* mh, const char* account_id,
											 int index, const void** out_ptr);
	static int api_account_set_skin_variant(void* mh, const char* account_id,
											const char* variant);
	static int api_account_set_current_cape(void* mh, const char* account_id,
											const char* cape_id);
	static int api_account_set_skin_blob(void* mh, const char* account_id,
										 const void* data, int64_t size);

	/* Section 26: Synchronous task helpers (ABI 3+) */
	static int api_account_skin_upload(void* mh, const char* account_id,
									   const void* png_bytes, int64_t size,
									   const char* variant);
	static int api_account_skin_reset(void* mh, const char* account_id);
	static int api_account_cape_set(void* mh, const char* account_id,
									const char* cape_id);

	/* Section 27: Icon list enumeration (ABI 3+) */
	static int api_icon_list_count(void* mh);
	static const char* api_icon_list_get_key(void* mh, int index);
	static const char* api_icon_list_get_name(void* mh, int index);
	static const char* api_icon_list_get_file_path(void* mh,
												   const char* icon_key);
	static int api_icon_list_save_png(void* mh, const char* icon_key,
									  const char* dest_path);

	/* Section 18: Plugin Icon Set (ABI 2+) */
	static const char* api_ui_plugin_icon(void* mh, const char* name);

	/* Section 19: System Tray (additive) */
	static void* api_tray_create(void* mh, const char* icon_name,
								 const char* tooltip);
	static int api_tray_destroy(void* mh, void* tray_handle);
	static int api_tray_is_available(void* mh);
	static int api_tray_set_icon(void* mh, void* tray_handle,
								 const char* icon_name);
	static int api_tray_set_tooltip(void* mh, void* tray_handle,
									const char* tooltip);
	static int api_tray_set_visible(void* mh, void* tray_handle, int visible);
	static int api_tray_show_message(void* mh, void* tray_handle,
									 const char* title, const char* message,
									 int icon_type, int msecs);
	static int api_tray_set_menu(void* mh, void* tray_handle,
								 const char* json_menu_doc,
								 MMCOUiEventCallback cb, void* user_data);
	static int api_tray_set_activation_cb(void* mh, void* tray_handle,
										  MMCOTrayActivationCallback cb,
										  void* ud);

	/* Section 20: Main window helpers */
	static int api_main_window_install_close_filter(
		void* mh, MMCOMainWindowCloseCallback cb, void* user_data);
	static int api_main_window_show(void* mh);
	static int api_main_window_hide(void* mh);
	static int api_main_window_is_visible(void* mh);

	/* S17 News API: read-only projection of Application::newsChecker(), which owns and parses every
	 * feed. */
	NewsChecker* newsChecker() const;
	QList<NewsEntryPtr> newsEntries() const;
	static int api_news_get_entry_count(void* mh);
	static const char* api_news_get_entry_title(void* mh, int index);
	static const char* api_news_get_entry_link(void* mh, int index);
	static const char* api_news_get_entry_content(void* mh, int index);
	static const char* api_news_get_entry_author(void* mh, int index);
	static const char* api_news_get_entry_date(void* mh, int index);
	static int api_news_get_entry_feed_index(void* mh, int index);
	static int api_news_add_feed_url(void* mh, const char* url);
	static int api_news_get_feed_count(void* mh);
	static const char* api_news_get_feed_url(void* mh, int index);
	static int api_news_reload(void* mh);

	/* Helpers */
	static ModuleRuntime* rt(void* mh);

	PluginLoader m_loader;
	QVector<PluginMetadata> m_modules;
	std::vector<std::unique_ptr<ModuleRuntime>> m_runtimes;
	std::vector<MMCOContext> m_contexts;

	/* hook_id -> list of registrations */
	QMultiMap<uint32_t, HookRegistration> m_hooks;

  private:
	/* Pending launch modifications set by plugins during PRE_LAUNCH hooks. Those callbacks may run
	 * on a worker thread while LaunchController reads the values on the GUI thread, so every access
	 * takes m_launchModMutex (non-recursive: no method below calls another one that locks). */
	mutable QMutex m_launchModMutex;
	QMap<QString, QString> m_pendingLaunchEnv;
	QString m_pendingLaunchWrapper;

	/* S19 / S20 state. Tray icons and close filters are tracked per owning module so they can be
	 * released together on unload. */
	struct TrayRecord {
		void* module_handle;
		QSystemTrayIcon* icon;
		QObject* guard; /* relay for activation signal */
		/* The one QMenu api_tray_set_menu() renders the menu document into, rebuilt in place on
		 * every call; nullptr until the first tray_set_menu(). */
		QMenu* menu = nullptr;
	};
	struct CloseFilterRecord {
		void* module_handle;
		MMCOMainWindowCloseCallback cb;
		void* user_data;
	};
	QVector<TrayRecord> m_trayIcons;
	QVector<CloseFilterRecord> m_closeFilters;
	bool m_closeFilterInstalled = false;
	QPointer<QWidget> m_filteredMainWindow;
	/* The QML shell's root QWindow, cached like m_filteredMainWindow (see resolveShellWindow()).
	 * Set only when there is no widget MainWindow. */
	QPointer<QWindow> m_filteredShellWindow;

	/* S23 (ABI 3+): per-module, per-instance running-state callbacks. Each record's `guard` QObject
	 * anchors the connection to the instance's runningStatusChanged; deleting it on unload severs
	 * the connection, so the callback never fires into freed memory. One record per (module_handle,
	 * instance_id); re-registering replaces it. */
	struct InstanceRunningRecord {
		void* module_handle;
		QString instanceId;
		MMCOInstanceRunningCallback cb;
		void* user_data;
		QObject* guard;
	};
	QVector<InstanceRunningRecord> m_instanceRunning;

	/* One record per ui_surface_create(). `doc` is the canonical parsed document that surfaces()
	 * and the widget builders read. `mountedRoot`/`mountedRenderer` track the widget currently on
	 * screen for the surface, if any: ui_surface_update/_set/_set_rows always patch `doc` and also
	 * the live widget when mounted. mountedRoot is a QPointer that clears when Qt tears the view
	 * down; mountedRenderer is dereferenced only while mountedRoot is non-null (same lifetime, see
	 * RendererOwner in PluginManager.cpp). */
	struct SurfaceRecord {
		void* module_handle = nullptr;
		QString surfaceId;
		int anchor = 0;
		QString anchorContext;
		QString title;
		QString iconName;
		QJsonObject doc;
		MMCOUiEventCallback cb = nullptr;
		void* userData = nullptr;
		QPointer<QWidget> mountedRoot;
		PluginUiRenderer::RenderedSurface* mountedRenderer = nullptr;
	};
	std::vector<std::unique_ptr<SurfaceRecord>> m_surfaces;
	int m_nextSurfaceSeq = 0;

	/* Finds the SurfaceRecord a `void* surface` handle refers to (its own stable heap address), or
	 * nullptr. */
	SurfaceRecord* findSurface(void* module_handle, void* surface);
	/* Builds the EventSink that forwards PluginUiRenderer callbacks into a surface's
	 * MMCOUiEventCallback. */
	static PluginUiRenderer::EventSink makeSurfaceSink(SurfaceRecord* rec);
	/* Renders every surface at (anchor, anchorContext) into one QWidget stacking a titled QGroupBox
	 * per surface, or nullptr if none. Shared by createGlobalSettingsPluginsPage() and the
	 * INSTANCE_SETTINGS_PAGE_CREATED bridge. */
	QWidget* buildPluginsSectionWidget(int anchor, const QString& anchorContext);
	/* Releases every SurfaceRecord owned by `module_handle`; called from
	 * releaseTrayResourcesForModule(). */
	void releaseSurfacesForModule(void* module_handle);

	/* Resolves the main window (objectName "MainWindow"), cached in a QPointer; nullptr before it
	 * is built or when the QML shell is active. */
	QWidget* resolveMainWindow();
	/* Resolves the QML shell's top-level QWindow via Application, cached; counterpart of
	 * resolveMainWindow() for main_window_* and the close filter. nullptr before the shell is shown
	 * or when the widget MainWindow is active. */
	QWindow* resolveShellWindow();
	/* Installs our eventFilter on the active top-level window (widget MainWindow or QML shell
	 * window), at most once. */
	void ensureCloseFilterInstalled();
	/* Releases all S19/S20/S23 resources of a module; called from shutdownAll() before
	 * mmco_unload(). */
	void releaseTrayResourcesForModule(void* module_handle);

	/* Wires Application::globalSettingsAboutToOpen and instanceSettingsPageCreated to the matching
	 * MMCO hooks. Called once from initializeAll() after modules are up; the connections are owned
	 * by `this`. */
	void connectAppSignals();

	bool m_shutdownDone = false;

  public:
	/* Called by LaunchController before/after dispatching PRE_LAUNCH hook */
	void clearPendingLaunchMods();
	QMap<QString, QString> takePendingLaunchEnv();
	QString takePendingLaunchWrapper();
};
