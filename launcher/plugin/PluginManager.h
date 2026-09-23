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

/*
 * PluginManager — owns the plugin lifecycle and provides the bridge
 * between loaded .mmco modules and MeshMC internals.
 *
 * Responsibilities:
 *   - Discover and load modules via PluginLoader
 *   - Build MMCOContext for each module (populating function pointers)
 *   - Call mmco_init() / mmco_unload()
 *   - Dispatch hooks to registered callbacks
 *   - Implement the API functions that back MMCOContext
 */

class Application;

class PluginManager : public QObject
{
	Q_OBJECT

  public:
	explicit PluginManager(Application* app, QObject* parent = nullptr);
	~PluginManager() override;

	/*
	 * Discover and initialise all modules. Called once during
	 * Application startup, after subsystems are ready.
	 */
	void initializeAll();

	/*
	 * Gracefully shut down all modules (calls mmco_unload() in
	 * reverse load order, then dlclose).
	 */
	void shutdownAll();

	/*
	 * Dispatch a hook to all registered listeners.
	 * Returns true if any callback signalled cancellation (non-zero return).
	 */
	bool dispatchHook(uint32_t hook_id, void* payload = nullptr);

	/*
	 * Query loaded modules.
	 */
	const QVector<PluginMetadata>& modules() const
	{
		return m_modules;
	}
	int moduleCount() const
	{
		return m_modules.size();
	}

	/*
	 * Disable / enable management.
	 *
	 * The disabled-set is persisted in the application settings under
	 * the key "plugins.disabled" as a comma-separated list of module
	 * names (case-insensitive). Toggling a module DOES NOT load or
	 * unload anything at runtime — the change takes effect on the next
	 * launcher start. PluginsDialog calls these to mutate the set; the
	 * dialog warns the user that a restart is required.
	 */
	bool isModuleDisabled(const QString& moduleName) const;
	void setModuleDisabled(const QString& moduleName, bool disabled);
	QSet<QString> disabledModuleNames() const;

	/*
	 * ─── ABI 5 — Declarative UI surfaces ──────────────────────────────
	 *
	 * Every ui_surface_create() call is recorded here as a SurfaceInfo
	 * — anchor, optional instance-id context, title/icon, and the
	 * current "mmco-ui/1" JSON document — independent of whether the
	 * widget renderer currently has anything on screen for it. This is
	 * the seam a future QML-based shell renders from instead of the
	 * QWidget tree PluginUiRenderer builds today: read the snapshot,
	 * watch surfacesChanged() for updates, and call deliverUiEvent()
	 * to report clicks/changes back to the owning plugin exactly the
	 * way a rendered QWidget does internally.
	 */
	struct SurfaceInfo {
		void* handle = nullptr; /* opaque; same value ui_surface_create returned */
		QString surfaceId;
		int anchor = 0; /* MMCOUiAnchor */
		QString anchorContext; /* instance id, or empty for GLOBAL_SETTINGS */
		QString title;
		QString iconName;
		QString document; /* current "mmco-ui/1" JSON, as text */
	};

	/*
	 * Snapshot of every live surface. `anchor` filters to one
	 * MMCOUiAnchor value, or pass -1 for "any". `anchorContext`
	 * filters to an exact instance id, or pass a default-constructed
	 * (null) QString for "any context" — a real-but-empty QString("")
	 * matches only GLOBAL_SETTINGS surfaces, which always have an
	 * empty context.
	 */
	QList<SurfaceInfo> surfaces(int anchor = -1,
							   const QString& anchorContext = QString()) const;

	/*
	 * Deliver a synthetic UI event to a surface's registered
	 * MMCOUiEventCallback — what a future QML renderer calls instead
	 * of relying on PluginUiRenderer's own Qt signal/slot wiring.
	 * No-op if surfaceId names no live surface.
	 */
	void deliverUiEvent(const QString& surfaceId, const QString& nodeId,
						const QString& event, const QString& valueJson);

	/*
	 * Host-internal widget builders — used by InstancePageProvider,
	 * Application's global-settings page provider, and the
	 * INSTANCE_SETTINGS_PAGE_CREATED bridge below to turn the surfaces
	 * above into the current widget UI. Each call renders fresh
	 * QWidgets from the surface's current document; every returned
	 * page/widget is caller-owned.
	 */

	/* One BasePage per MMCO_UI_ANCHOR_INSTANCE_PAGE surface anchored to
	 * `instanceId` — these become their own tabs in the instance
	 * window, alongside GitVersioningPage-style plugin pages. */
	QList<BasePage*> createInstancePages(const QString& instanceId);

	/* A single "Plugins" BasePage stacking every
	 * MMCO_UI_ANCHOR_GLOBAL_SETTINGS surface as a titled section, or
	 * nullptr if there are none — inserted into the global Settings
	 * dialog's page list. */
	BasePage* createGlobalSettingsPluginsPage();

	/*
	 * ScratchString — the per-module scratch buffer that backs every
	 * `const char*` getter in the plugin API.
	 *
	 * The ABI contract is "the returned pointer stays valid until the
	 * next API call by the same module". This used to be a plain
	 * std::string member of ModuleRuntime, which is only correct while
	 * every call happens on the GUI thread: as soon as a hook callback
	 * runs on a worker thread, two threads write the same buffer and a
	 * reader can be handed a pointer that the other thread has already
	 * reallocated.
	 *
	 * The bytes therefore live in thread-local storage keyed by the
	 * ScratchString instance, which turns the contract into "valid
	 * until the next API call by the same module *on the same thread*"
	 * — what callers actually rely on, and safe for background hooks.
	 * Call sites are untouched: assignment, c_str(), data() and
	 * assign() all forward to the calling thread's own copy.
	 */
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

	/*
	 * ModuleRuntime — the opaque object behind module_handle.
	 * Lets static API callbacks find their way back to the manager.
	 * Public so helper functions in the .cpp can use it.
	 */
	struct ModuleRuntime {
		PluginManager* manager;
		int moduleIndex;
		/* Written once during initializeAll(), read-only afterwards —
		 * safe to hand out from any thread. */
		std::string dataDir;
		ScratchString tempString;
	};

	Application* m_app;

	/*
	 * QObject event filter — installed on the main window when at least
	 * one plugin has registered a close-event callback via
	 * main_window_install_close_filter(). Intercepts QCloseEvent and
	 * lets registered callbacks decide whether to accept or swallow it.
	 */
	bool eventFilter(QObject* watched, QEvent* event) override;

  signals:
	void moduleLoaded(const QString& name);
	void moduleUnloaded(const QString& name);
	void moduleError(const QString& name, const QString& error);
	/* Any surface was created/updated/destroyed — a future QML shell
	 * (or anything else watching surfaces()) re-reads on this. */
	void surfacesChanged();

  private:
	/* Build an MMCOContext for a specific module */
	MMCOContext buildContext(PluginMetadata& meta);

	/* Ensure the plugin data directory exists */
	void ensurePluginDataDir(PluginMetadata& meta);

	/* ── API implementation (static callbacks wired into MMCOContext) ── */
	/* These are static so they can be used as C function pointers.
	 * They resolve the PluginManager instance via the module_handle,
	 * which is actually a pointer to a ModuleRuntime struct.
	 */

	struct HookRegistration {
		void* module_handle;
		MMCOHookCallback callback;
		void* user_data;
		/* MMCO_HOOK_FLAG_* — 0 for anything registered through the
		 * plain hook_register, which keeps the inline behaviour. */
		uint32_t flags;
	};

	/* Runs the background-flagged callbacks of one dispatch as a
	 * SequentialTask behind a ProgressDialog, one row per module.
	 * Returns true if one of them vetoed the operation. */
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

	/* S17 — News API.
	 *
	 * Read-only projection of MainWindow's NewsChecker, which owns and
	 * parses every feed. Both helpers cope with there being no main
	 * window yet (startup) or any more (shutdown). */
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
	/* NOTE: the instance toolbar actions a plugin could once register here
	 * are gone along with the API that fed them (ui_register_instance_action
	 * / _cb, deprecated no-ops in ABI 3-4, removed entirely in ABI 5). A
	 * plugin's per-instance UI belongs on an instance-window page instead
	 * — see ui_surface_create's MMCO_UI_ANCHOR_INSTANCE_PAGE in PluginAPI.h. */

	/* Pending launch modifications (set by plugins during PRE_LAUNCH hooks).
	 *
	 * PRE_LAUNCH callbacks may run on a worker thread while
	 * LaunchController reads the collected values on the GUI thread, so
	 * every access goes through m_launchModMutex. The mutex is
	 * recursive-free: no method below calls another one that locks. */
	mutable QMutex m_launchModMutex;
	QMap<QString, QString> m_pendingLaunchEnv;
	QString m_pendingLaunchWrapper;

	/* S17 — News: no state. NewsChecker holds the feeds and the
	 * entries; see newsChecker() above. */

	/* S19 / S20 — system-tray and main-window helpers state.
	 * All tray icons and close filters are tracked per owning module so
	 * PluginManager can release them en masse when a module is
	 * unloaded — preventing leaks and dangling Qt parents. */
	struct TrayRecord {
		void* module_handle;
		QSystemTrayIcon* icon;
		QObject* guard; /* relay for activation signal */
		/* ABI 5 — the one QMenu a tray's declarative menu doc is
		 * rendered into by api_tray_set_menu(); rebuilt in place on
		 * every call instead of the plugin creating/owning it via the
		 * removed tray_menu_* family. nullptr until the first
		 * tray_set_menu() call. */
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
	/* The QML shell's root QWindow, cached the same way
	 * m_filteredMainWindow is -- see resolveShellWindow(). Only ever set
	 * when there is no widget MainWindow (the QML shell is the active
	 * UI); both can't be non-null at once. */
	QPointer<QWindow> m_filteredShellWindow;

	/* S23 (ABI 3+) — per-module per-instance running-state callbacks.
	 *
	 * Each record owns one QObject `guard` that anchors the Qt
	 * QObject::connect to the resolved BaseInstance's
	 * runningStatusChanged signal. mmco_unload() / module teardown
	 * deletes the guard, which severs the connection automatically
	 * — Qt's normal sender/receiver bookkeeping then guarantees the
	 * plugin's callback can never fire into freed memory.
	 *
	 * A single module may have at most one record per (module_handle,
	 * instance_id) pair; re-registering replaces the existing one. */
	struct InstanceRunningRecord {
		void* module_handle;
		QString instanceId;
		MMCOInstanceRunningCallback cb;
		void* user_data;
		QObject* guard;
	};
	QVector<InstanceRunningRecord> m_instanceRunning;

	/* ─── ABI 5 — Declarative UI surfaces ─────────────────────────────
	 *
	 * One record per ui_surface_create() call. `doc` is the canonical,
	 * always-current parsed document — the single source of truth
	 * every accessor (surfaces(), the widget builders below) reads
	 * from. `mountedRoot`/`mountedRenderer` track whichever rendered
	 * widget is *currently on screen* for this surface, if any: since
	 * every page/dialog that displays a surface is rebuilt fresh each
	 * time it is opened (same as the old per-plugin BasePage pattern),
	 * ui_surface_update/_set/_set_rows always patch `doc` and — when a
	 * view happens to be mounted right now — also patch that live
	 * widget immediately, so e.g. GitVersioning's row-selection ->
	 * button-enabled wiring stays instant while the instance page is
	 * open. mountedRoot is a QPointer so it self-clears the moment
	 * Qt tears down that view; mountedRenderer is only ever
	 * dereferenced while mountedRoot is still non-null (they share the
	 * same lifetime — see PluginManager.cpp's RendererOwner). */
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

	/* Find the SurfaceRecord a `void* surface` handle refers to (the
	 * handle is that record's own stable heap address), or nullptr. */
	SurfaceRecord* findSurface(void* module_handle, void* surface);
	/* Build the EventSink that forwards PluginUiRenderer callbacks into
	 * a surface's MMCOUiEventCallback. */
	static PluginUiRenderer::EventSink makeSurfaceSink(SurfaceRecord* rec);
	/* Render every surface at (anchor, anchorContext) into one
	 * QWidget stacking a titled QGroupBox per surface, or nullptr if
	 * there are none. Shared by createGlobalSettingsPluginsPage() and
	 * the INSTANCE_SETTINGS_PAGE_CREATED bridge in connectAppSignals(). */
	QWidget* buildPluginsSectionWidget(int anchor, const QString& anchorContext);
	/* Release every SurfaceRecord owned by `module_handle` — called
	 * from releaseTrayResourcesForModule() so ABI 5 surfaces get the
	 * same per-module teardown as tray icons/menus. */
	void releaseSurfacesForModule(void* module_handle);

	/* Resolve the launcher's main window (objectName == "MainWindow"),
	 * cached for the lifetime of the QPointer. Returns nullptr if the
	 * window has not been built yet, or when the QML shell is the
	 * active UI instead (see resolveShellWindow()). */
	QWidget* resolveMainWindow();
	/* Resolve the QML shell's top-level QWindow via Application, cached
	 * for the lifetime of the QPointer — the generalised counterpart to
	 * resolveMainWindow() for main_window_show/hide/is_visible and the
	 * close filter when the widget MainWindow does not exist. Returns
	 * nullptr before the shell has been shown, or when the widget
	 * MainWindow is the active UI instead. */
	QWindow* resolveShellWindow();
	/* Make sure our QObject::eventFilter is installed on whichever
	 * top-level window is active (widget MainWindow or the QML shell's
	 * window). Safe to call multiple times — installs at most once. */
	void ensureCloseFilterInstalled();
	/* Release all S19/S20/S23 resources owned by the given module
	 * handle.  Called from shutdownAll() right before mmco_unload(). */
	void releaseTrayResourcesForModule(void* module_handle);

	/* Wire the two Application Qt signals we re-publish as hooks:
	 *   • globalSettingsAboutToOpen   ->
	 * MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN • instanceSettingsPageCreated ->
	 * MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED Called once from initializeAll()
	 * after modules are up. The connections are owned by `this` (PluginManager
	 * is a QObject) and severed automatically on destruction. */
	void connectAppSignals();

	bool m_shutdownDone = false;

  public:
	/* Called by LaunchController before/after dispatching PRE_LAUNCH hook */
	void clearPendingLaunchMods();
	QMap<QString, QString> takePendingLaunchEnv();
	QString takePendingLaunchWrapper();
};
