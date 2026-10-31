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
 *
 *
 * MMCOContext, the runtime context passed to mmco_init(): the only interface plugins have
 * into Kite. Plugins must not call Qt, Kite internals or any symbol not exposed here.
 *
 * Valid until mmco_unload() returns. Strings returned by getters are valid until the next API
 * call on the same module unless documented otherwise.
 */

#pragma once

#include "plugin/PluginHooks.h"
#include <cstdint>
#include <cstddef>

typedef void (*MMCOHttpCallback)(void* user_data, int status_code,
								 const void* response_body,
								 size_t response_size);
typedef void (*MMCOMenuActionCallback)(void* user_data);
typedef void (*MMCODirEntryCallback)(void* user_data, const char* entry_name,
									 int is_dir);

/* ABI 5. One event callback for every declarative UI surface and for the tray menu.
 *   surface_id  surface/menu the event came from ("tray" for tray-menu events)
 *   node_id     plugin-assigned id of the node that fired
 *   event       "click" (button/link/menu item), "change" (toggle/text_field/number_field/choice;
 *               value_json is the new value) or "select"/"activate" (list row; value_json is the row id)
 *   value_json  event payload, "" if none */
typedef void (*MMCOUiEventCallback)(void* user_data, const char* surface_id,
									const char* node_id, const char* event,
									const char* value_json);

/* ABI 5. Where a declarative UI surface is displayed:
 *   GLOBAL_SETTINGS    titled section in the "Plugins" page of the global settings
 *   INSTANCE_PAGE      its own tab on the instance's page (anchor_context = instance id)
 *   INSTANCE_SETTINGS  titled section in a "Plugins" group on the instance's settings tab
 *                      (anchor_context = instance id) */
enum MMCOUiAnchor {
	MMCO_UI_ANCHOR_GLOBAL_SETTINGS = 0,
	MMCO_UI_ANCHOR_INSTANCE_PAGE = 1,
	MMCO_UI_ANCHOR_INSTANCE_SETTINGS = 2
};

/* Tray activation reason, the value of QSystemTrayIcon::ActivationReason: 0 Unknown, 1 Context
 * (right click), 2 DoubleClick, 3 Trigger (single left click), 4 MiddleClick. Context fires in
 * addition to the popup of a menu attached with tray_set_menu(). */
typedef void (*MMCOTrayActivationCallback)(void* user_data, int reason);

/* Main-window close filter. Return 0 to let the close happen, 1 to swallow it (e.g. hide to tray).
 * The first filter returning non-zero wins; later filters still run but cannot override it. */
typedef int (*MMCOMainWindowCloseCallback)(void* user_data);

struct MMCOContext {
	uint32_t struct_size; /* sizeof(MMCOContext) for forward compat */
	uint32_t abi_version; /* MMCO_ABI_VERSION */
	void* module_handle;  /* Opaque handle to identify this module */

	void (*log_info)(void* mh, const char* msg);
	void (*log_warn)(void* mh, const char* msg);
	void (*log_error)(void* mh, const char* msg);
	void (*log_debug)(void* mh, const char* msg);

	int (*hook_register)(void* mh, uint32_t hook_id, MMCOHookCallback callback,
						 void* user_data);
	int (*hook_unregister)(void* mh, uint32_t hook_id,
						   MMCOHookCallback callback);

	const char* (*setting_get)(void* mh, const char* key);
	int (*setting_set)(void* mh, const char* key, const char* value);

	/* Enumeration */
	int (*instance_count)(void* mh);
	const char* (*instance_get_id)(void* mh, int index);

	/* Properties (by instance ID) */
	const char* (*instance_get_name)(void* mh, const char* id);
	int (*instance_set_name)(void* mh, const char* id, const char* name);
	const char* (*instance_get_path)(void* mh, const char* id);
	const char* (*instance_get_game_root)(void* mh, const char* id);
	const char* (*instance_get_mods_root)(void* mh, const char* id);
	const char* (*instance_get_icon_key)(void* mh, const char* id);
	int (*instance_set_icon_key)(void* mh, const char* id, const char* key);
	const char* (*instance_get_type)(void* mh, const char* id);
	const char* (*instance_get_notes)(void* mh, const char* id);
	int (*instance_set_notes)(void* mh, const char* id, const char* notes);

	/* State queries */
	int (*instance_is_running)(void* mh, const char* id);
	int (*instance_can_launch)(void* mh, const char* id);
	int (*instance_has_crashed)(void* mh, const char* id);
	int (*instance_has_update)(void* mh, const char* id);
	int64_t (*instance_get_total_play_time)(void* mh, const char* id);
	int64_t (*instance_get_last_play_time)(void* mh, const char* id);
	int64_t (*instance_get_last_launch)(void* mh, const char* id);

	/* Actions */
	int (*instance_launch)(void* mh, const char* id, int online);
	int (*instance_kill)(void* mh, const char* id);
	int (*instance_delete)(void* mh, const char* id);

	/* Groups */
	const char* (*instance_get_group)(void* mh, const char* id);
	int (*instance_set_group)(void* mh, const char* id, const char* group);
	int (*instance_group_count)(void* mh);
	const char* (*instance_group_at)(void* mh, int index);

	/* Pack Profile / Components */
	int (*instance_component_count)(void* mh, const char* id);
	const char* (*instance_component_get_uid)(void* mh, const char* id,
											  int index);
	const char* (*instance_component_get_name)(void* mh, const char* id,
											   int index);
	const char* (*instance_component_get_version)(void* mh, const char* id,
												  int index);
	const char* (*instance_get_mc_version)(void* mh, const char* id);

	/* Instance directories */
	const char* (*instance_get_jar_mods_dir)(void* mh, const char* id);
	const char* (*instance_get_resource_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_texture_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_shader_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_worlds_dir)(void* mh, const char* id);

	int (*mod_count)(void* mh, const char* instance_id, const char* type);
	const char* (*mod_get_name)(void* mh, const char* instance_id,
								const char* type, int index);
	const char* (*mod_get_version)(void* mh, const char* instance_id,
								   const char* type, int index);
	const char* (*mod_get_filename)(void* mh, const char* instance_id,
									const char* type, int index);
	const char* (*mod_get_description)(void* mh, const char* instance_id,
									   const char* type, int index);
	int (*mod_is_enabled)(void* mh, const char* instance_id, const char* type,
						  int index);
	int (*mod_set_enabled)(void* mh, const char* instance_id, const char* type,
						   int index, int enabled);
	int (*mod_remove)(void* mh, const char* instance_id, const char* type,
					  int index);
	int (*mod_install)(void* mh, const char* instance_id, const char* type,
					   const char* filepath);
	int (*mod_refresh)(void* mh, const char* instance_id, const char* type);

	int (*world_count)(void* mh, const char* instance_id);
	const char* (*world_get_name)(void* mh, const char* instance_id, int index);
	const char* (*world_get_folder)(void* mh, const char* instance_id,
									int index);
	int64_t (*world_get_seed)(void* mh, const char* instance_id, int index);
	int (*world_get_game_type)(void* mh, const char* instance_id, int index);
	int64_t (*world_get_last_played)(void* mh, const char* instance_id,
									 int index);
	int (*world_delete)(void* mh, const char* instance_id, int index);
	int (*world_rename)(void* mh, const char* instance_id, int index,
						const char* new_name);
	int (*world_install)(void* mh, const char* instance_id,
						 const char* filepath);
	int (*world_refresh)(void* mh, const char* instance_id);

	int (*account_count)(void* mh);
	const char* (*account_get_profile_name)(void* mh, int index);
	const char* (*account_get_profile_id)(void* mh, int index);
	const char* (*account_get_type)(void* mh, int index);
	int (*account_get_state)(void* mh, int index);
	int (*account_is_active)(void* mh, int index);
	int (*account_get_default_index)(void* mh);

	int (*java_count)(void* mh);
	const char* (*java_get_version)(void* mh, int index);
	const char* (*java_get_arch)(void* mh, int index);
	const char* (*java_get_path)(void* mh, int index);
	int (*java_is_recommended)(void* mh, int index);
	const char* (*instance_get_java_version)(void* mh, const char* id);

	/* Sandboxed (relative to plugin data dir) */
	const char* (*fs_plugin_data_dir)(void* mh);
	int64_t (*fs_read)(void* mh, const char* rel_path, void* buf, size_t sz);
	int (*fs_write)(void* mh, const char* rel_path, const void* data,
					size_t sz);
	int (*fs_exists)(void* mh, const char* rel_path);

	/* Absolute path operations */
	int (*fs_mkdir)(void* mh, const char* abs_path);
	int (*fs_exists_abs)(void* mh, const char* abs_path);
	int (*fs_remove)(void* mh, const char* abs_path);
	int (*fs_copy_file)(void* mh, const char* src, const char* dst);
	int64_t (*fs_file_size)(void* mh, const char* abs_path);
	int (*fs_list_dir)(void* mh, const char* abs_path, int type,
					   MMCODirEntryCallback callback, void* user_data);

	int (*zip_compress_dir)(void* mh, const char* zip_path,
							const char* dir_path);
	int (*zip_extract)(void* mh, const char* zip_path, const char* target_dir);

	int (*http_get)(void* mh, const char* url, MMCOHttpCallback callback,
					void* user_data);
	int (*http_post)(void* mh, const char* url, const void* body,
					 size_t body_size, const char* content_type,
					 MMCOHttpCallback callback, void* user_data);

	void (*ui_show_message)(void* mh, int type, const char* title,
							const char* msg);
	/* Dead since the widget UI was removed: no hook hands out a menu_handle any more (see
	 * MMCO_HOOK_UI_CONTEXT_MENU, reserved), so this always returns -1. Kept for ABI. */
	int (*ui_add_menu_item)(void* mh, void* menu_handle, const char* label,
							const char* icon_name,
							MMCOMenuActionCallback callback, void* user_data);

	/* Returns the user-chosen path, or nullptr on cancel */
	const char* (*ui_file_open_dialog)(void* mh, const char* title,
									   const char* filter);
	const char* (*ui_file_save_dialog)(void* mh, const char* title,
									   const char* default_name,
									   const char* filter);
	/* Returns user text, or nullptr on cancel */
	const char* (*ui_input_dialog)(void* mh, const char* title,
								   const char* prompt,
								   const char* default_value);
	/* Returns 1=Yes, 0=No */
	int (*ui_confirm_dialog)(void* mh, const char* title, const char* message);

	const char* (*get_app_version)(void* mh);
	const char* (*get_app_name)(void* mh);
	int64_t (*get_timestamp)(void* mh);

	/* S15 — Launch Modifiers (only valid inside INSTANCE_PRE_LAUNCH hooks) */

	/* Sets an environment variable for the launching instance; removed after the instance stops. */
	int (*launch_set_env)(void* mh, const char* key, const char* value);

	/* Prepends a wrapper command for the launching instance, ahead of any existing wrapper. */
	int (*launch_prepend_wrapper)(void* mh, const char* wrapper_cmd);

	/* S16 — Application Settings (read-only global settings) */

	/* Returns the value, or nullptr if the key does not exist. */
	const char* (*app_setting_get)(void* mh, const char* key);
	/* S17 — News API */

	/* Total number of loaded news entries across all feeds; -1 if the news system is unavailable. */
	int (*news_get_entry_count)(void* mh);

	/* Index-taking news getters return nullptr when the index is out of range. */
	const char* (*news_get_entry_title)(void* mh, int index);

	const char* (*news_get_entry_link)(void* mh, int index);

	const char* (*news_get_entry_content)(void* mh, int index);

	const char* (*news_get_entry_author)(void* mh, int index);

	/* ISO 8601, e.g. "2026-05-08T14:30:00". */
	const char* (*news_get_entry_date)(void* mh, int index);

	/* Index of the feed the entry came from. */
	int (*news_get_entry_feed_index)(void* mh, int index);

	/* Not supported: always returns -1; feeds are fixed at build time. */
	int (*news_add_feed_url)(void* mh, const char* url);

	/* Number of feed URLs, including the default. */
	int (*news_get_feed_count)(void* mh);

	const char* (*news_get_feed_url)(void* mh, int index);

	/* Non-blocking; results arrive via MMCO_HOOK_NEWS_UPDATED. */
	int (*news_reload)(void* mh);

	/* S18 — Plugin icon set (ABI 2+) */

	/* Resolves a logical icon name from the module's bundled icon set to a
	 * ":/plugins/<icon_set>/<name>" resource path usable in any icon-name parameter. Returns nullptr
	 * if the module declared no icon_set_resource or the icon does not exist. */
	const char* (*ui_plugin_icon)(void* mh, const char* name);

	/* S19 System Tray (ABI 2+). Handles are opaque and owned by the host: released by tray_destroy
	 * or when the module unloads. Creation returns nullptr on failure (e.g. no system tray).
	 * Passing nullptr as the tray handle to tray_show_message() uses a transient hidden icon. */

	/* Returns an opaque handle, or nullptr if there is no usable tray (see tray_is_available()). */
	void* (*tray_create)(void* mh, const char* icon_name, const char* tooltip);

	/* Idempotent; nullptr is a no-op. */
	int (*tray_destroy)(void* mh, void* tray_handle);

	int (*tray_is_available)(void* mh);

	/* icon_name: a theme name or a ":/..." resource path. */
	int (*tray_set_icon)(void* mh, void* tray_handle, const char* icon_name);
	int (*tray_set_tooltip)(void* mh, void* tray_handle, const char* tooltip);
	int (*tray_set_visible)(void* mh, void* tray_handle, int visible);

	/* icon_type: 0 None, 1 Info, 2 Warning, 3 Critical. msecs: timeout hint (10000 if 0).
	 * A nullptr tray_handle uses a transient hidden tray (fire-and-forget notify). */
	int (*tray_show_message)(void* mh, void* tray_handle, const char* title,
							 const char* message, int icon_type, int msecs);

	/* Attaches a declarative menu shown on right-click (ABI 5). json_menu_doc is an "mmco-ui/1" tree
	 * whose root's children are `button` (item), `separator` or `section` (submenu). Clicking an item
	 * calls cb with event="click" and node_id = the item's id. nullptr detaches the menu; call again
	 * with a new document to rebuild it. */
	int (*tray_set_menu)(void* mh, void* tray_handle, const char* json_menu_doc,
						 MMCOUiEventCallback cb, void* user_data);

	/* Fires on left/middle/double click. cb=nullptr clears it. One callback per tray. */
	int (*tray_set_activation_cb)(void* mh, void* tray_handle,
								  MMCOTrayActivationCallback cb, void* ud);

	/* S20 Main-window helpers (ABI 2+) */

	/* Installs a filter run on every main-window QCloseEvent: return 0 to let the close proceed, 1 to
	 * swallow it (the host hides the window). A module's filters are removed on mmco_unload();
	 * cb=nullptr clears them earlier. Returns 0 on success, -1 if the main window is not alive yet
	 * (wait for MMCO_HOOK_UI_MAIN_READY). */
	int (*main_window_install_close_filter)(void* mh,
											MMCOMainWindowCloseCallback cb,
											void* user_data);

	/* Shows (and raises) or hides the main window. */
	int (*main_window_show)(void* mh);
	int (*main_window_hide)(void* mh);
	int (*main_window_is_visible)(void* mh);

	/* S21 Application-scope settings, write side (ABI 3+). Keys are namespaced like the launcher's own
	 * settings, with no plugin prefix; use setting_get/setting_set for plugin-private keys.
	 *   app_setting_register  creates the key with a UTF-8 default; no-op returning 0 if it exists
	 *   app_setting_set       overwrites an existing (registered) key
	 *   app_setting_contains  1 if the key is registered, 0 otherwise
	 * register/set return 0 on success, -1 on error. */
	int (*app_setting_set)(void* mh, const char* key, const char* value);
	int (*app_setting_register)(void* mh, const char* key,
								const char* default_value);
	int (*app_setting_contains)(void* mh, const char* key);

	/* S22 Themed icon resolution (ABI 3+). Resolves a logical name to a string accepted by the icon-name
	 * parameters (XDG theme name or ":/..." resource path). Returns nullptr if the name does not resolve. */
	const char* (*ui_themed_icon)(void* mh, const char* name);

	/* S23 Instance running-state bridge (ABI 3+).
	 *   instance_running_register    delivers running-state transitions of the instance to cb(ud, id, running);
	 *                                one callback per (module, instance), re-registering replaces it.
	 *                                Returns -1 if the id does not resolve.
	 *   instance_running_unregister  stops delivery; no-op (returns 0) if not registered.
	 * The host severs a module's connections on mmco_unload(). */
	int (*instance_running_register)(void* mh, const char* instance_id,
									 MMCOInstanceRunningCallback cb, void* ud);
	int (*instance_running_unregister)(void* mh, const char* instance_id);

	/* S24 Per-instance settings (ABI 3+). Values are UTF-8 strings. Return 0 on success, -1 on failure
	 * (unknown instance, missing key, ...). register_override mirrors SettingsObject::registerOverride():
	 * it exposes an application-scope setting on the instance, controlled by the per-instance bool `gate_key`. */
	const char* (*instance_setting_get)(void* mh, const char* instance_id,
										const char* key);
	int (*instance_setting_set)(void* mh, const char* instance_id,
								const char* key, const char* value);
	int (*instance_setting_register)(void* mh, const char* instance_id,
									 const char* key,
									 const char* default_value);
	int (*instance_setting_register_override)(void* mh, const char* instance_id,
											  const char* key,
											  const char* gate_key);
	int (*instance_setting_reset)(void* mh, const char* instance_id,
								  const char* key);
	int (*instance_setting_contains)(void* mh, const char* instance_id,
									 const char* key);

	/* S25 Account / skin / cape access (ABI 3+), by account id. account_get_id_by_index returns the id
	 * matching S7's account_get_profile_id at the same index. Blob getters return the PNG size in bytes
	 * (0 if absent, -1 on error); `out_ptr` is valid until the next API call on the module. Variants are
	 * "CLASSIC" / "SLIM" (case-insensitive). Setters return 0 on success, -1 on failure and change only
	 * the in-memory copy; account_skin_upload pushes a skin to Mojang. */
	const char* (*account_get_id_by_index)(void* mh, int index);
	int (*account_is_msa_by_id)(void* mh, const char* account_id);
	const char* (*account_get_access_token)(void* mh, const char* account_id);
	const char* (*account_get_current_cape_id)(void* mh,
											   const char* account_id);
	const char* (*account_get_skin_variant)(void* mh, const char* account_id);
	int64_t (*account_get_skin_blob)(void* mh, const char* account_id,
									 const void** out_ptr);
	int (*account_cape_count)(void* mh, const char* account_id);
	const char* (*account_cape_get_id)(void* mh, const char* account_id,
									   int index);
	const char* (*account_cape_get_alias)(void* mh, const char* account_id,
										  int index);
	int64_t (*account_cape_get_blob)(void* mh, const char* account_id,
									 int index, const void** out_ptr);
	int (*account_set_skin_variant)(void* mh, const char* account_id,
									const char* variant);
	int (*account_set_current_cape)(void* mh, const char* account_id,
									const char* cape_id);
	int (*account_set_skin_blob)(void* mh, const char* account_id,
								 const void* data, int64_t size);

	/* S26 Synchronous task helpers (ABI 3+). Run launcher-side network tasks behind a busy
	 * indication.
	 *   account_skin_upload  POST a skin PNG (variant "STEVE" or "ALEX")
	 *   account_skin_reset   DELETE the active skin
	 *   account_cape_set     PUT the active cape id ("" = no cape)
	 * Return 0 on success, -1 on failure (task error or user cancel). */
	int (*account_skin_upload)(void* mh, const char* account_id,
							   const void* png_bytes, int64_t size,
							   const char* variant);
	int (*account_skin_reset)(void* mh, const char* account_id);
	int (*account_cape_set)(void* mh, const char* account_id,
							const char* cape_id);

	/* S27 Icon list enumeration (ABI 3+), e.g. for an icon picker.
	 *   icon_list_get_key        stable key (usable with instance_set_icon_key and icon-name parameters)
	 *   icon_list_get_name       human-readable display name
	 *   icon_list_get_file_path  absolute path on disk, "" if the icon is in-memory only
	 *   icon_list_save_png       writes the icon's PNG to dest_path; 0 on success */
	int (*icon_list_count)(void* mh);
	const char* (*icon_list_get_key)(void* mh, int index);
	const char* (*icon_list_get_name)(void* mh, int index);
	const char* (*icon_list_get_file_path)(void* mh, const char* icon_key);
	int (*icon_list_save_png)(void* mh, const char* icon_key,
							  const char* dest_path);

	/* S28 Instance update flag (ABI 3+). Companion setter to instance_has_update: value != 0 marks
	 * the instance as having an update (the `checkupdate` badge), 0 clears it. Returns 0 on success,
	 * -1 if the id is unknown. Thread: GUI thread only. */
	int (*instance_set_update_available)(void* mh, const char* id, int value);

	/* S29 Component version write (ABI 3+). Sets the version of the PackProfile component `uid`
	 * (e.g. "net.fabricmc.fabric-loader", "net.minecraft"), appending the component if the instance
	 * lacks it. An existing component is reverted to its catalogue entry first and pinned as
	 * user-chosen (important=1), so the resolver does not upgrade it. Returns 0 on success, -1 on
	 * failure (unknown instance, no PackProfile, or the component could not be reverted).
	 * Thread: GUI thread only. */
	int (*instance_component_set_version)(void* mh, const char* id,
										  const char* uid, const char* version);

	/* S30 http_get with custom headers (ABI 3+). `headers` holds `header_count` NUL-terminated
	 * "Name: Value" strings. User-Agent is always set by the launcher, whatever is passed. The launcher
	 * passes no keys through this API; the plugin supplies its own (e.g. CurseForge's x-api-key).
	 * Returns 0 if queued, -1 on argument errors. Body lifetime as in http_get. */
	int (*http_get_with_headers)(void* mh, const char* url,
								 const char* const* headers, int header_count,
								 MMCOHttpCallback callback, void* user_data);

	/* S31 Subprocess execution (no ABI bump). Runs a program synchronously through QProcess and
	 * captures its stdout.
	 *   program        executable name (resolved on PATH) or absolute path; not run through a shell,
	 *                  each argv element is passed verbatim
	 *   args           `arg_count` NUL-terminated strings (argv[1..]); may be NULL if arg_count is 0
	 *   working_dir    directory to run in, or NULL for the host's
	 *   stdin_data     bytes for the child's stdin, or NULL; stdin_size is their length
	 *   out_buf        caller-owned buffer for stdout (NUL-terminated, truncated to fit), NULL discards;
	 *                  out_buf_size includes the NUL
	 *   out_exit_code  receives the exit code, or NULL
	 *   timeout_ms     hard timeout, the child is killed; <= 0 means 30 s
	 * Returns 0 if the process ran to completion (see out_exit_code), -1 if it failed to start,
	 * -2 if it timed out and was killed, -3 for invalid arguments.
	 * Thread: any; the host pumps a local event loop, so the GUI thread is fine too. */
	int (*process_run)(void* mh, const char* program, const char* const* args,
					   int arg_count, const char* working_dir,
					   const char* stdin_data, int stdin_size, char* out_buf,
					   int out_buf_size, int* out_exit_code, int timeout_ms);

	/* S32 Background hooks + progress reporting (ABI stays at 3; struct_size tells the host what a
	 * module was built against). hook_register_ex is hook_register plus a flags word (MMCO_HOOK_FLAG_*
	 * in PluginHooks.h); 0 behaves like hook_register.
	 * progress_report is accepted but not displayed (the host shows only a busy indication while
	 * background callbacks run); its arguments are:
	 *   handle           the module_handle the callback was invoked with
	 *   status           row title, NULL keeps the current one
	 *   details          optional second line, NULL keeps the current one
	 *   current / total  work done / total; total <= 0 is indeterminate (busy bar)
	 * Returns 0 if queued, -1 on unusable arguments or outside a background hook callback. The update is
	 * marshalled onto the GUI thread, so calling it in a tight loop is safe. */
	int (*hook_register_ex)(void* mh, uint32_t hook_id,
							MMCOHookCallback callback, void* user_data,
							uint32_t flags);
	int (*progress_report)(void* handle, const char* status,
						   const char* details, int64_t current,
						   int64_t total);

	/* S33 Declarative UI surfaces (ABI 5). A plugin describes a node tree as an "mmco-ui/1" JSON
	 * document, {"type":"mmco-ui/1","root":<node>}; the host renders it (qml/Components/PluginNode.qml)
	 * and owns the result, no widget handle is handed back.
	 * Fourteen node types, each {"type":..., "id":..., "props":{...}} (containers add "children"):
	 *   containers          column, row, section (props.title)
	 *   content leaves      heading and text (props.text; text also props.format "markdown"),
	 *                       separator, progress (props.value 0..100, < 0 or absent = busy)
	 *   interactive leaves  button (label, style "primary", enabled), toggle (label, description,
	 *                       value), text_field (label, placeholder, value), number_field (label, min,
	 *                       max, step, value), choice (label, options, value), list (columns, rows
	 *                       of {id, cells}), link (text, href)
	 * Every node takes props.visible (false hides it). The optional "id" addresses the node in events
	 * and in ui_surface_set / ui_surface_set_rows. No colour, font or geometry prop exists: the host
	 * owns presentation.
	 *   ui_surface_create    builds and shows a surface at the anchor. anchor_context is nullptr for
	 *                        GLOBAL_SETTINGS, else the instance id. title/icon_name label it (page title
	 *                        for INSTANCE_PAGE, section title otherwise). cb receives every click/change/
	 *                        select event (see MMCOUiEventCallback). Returns an opaque handle, or nullptr
	 *                        on failure (bad JSON, unknown anchor).
	 *   ui_surface_update    replaces the whole document
	 *   ui_surface_set       patches one node's `props` without touching the rest of the tree
	 *   ui_surface_set_rows  replaces a `list` node's `rows` only (cheap refresh)
	 *   ui_surface_destroy   tears the surface down early; a module's surfaces also go on unload
	 * The mutators return 0 on success, -1 on failure (unknown surface, malformed JSON, unknown node_id). */
	void* (*ui_surface_create)(void* mh, int anchor, const char* anchor_context,
							   const char* title, const char* icon_name,
							   const char* json_doc, MMCOUiEventCallback cb,
							   void* user_data);
	int (*ui_surface_update)(void* mh, void* surface, const char* json_doc);
	int (*ui_surface_set)(void* mh, void* surface, const char* node_id,
						  const char* json_props);
	int (*ui_surface_set_rows)(void* mh, void* surface, const char* node_id,
							   const char* json_rows);
	int (*ui_surface_destroy)(void* mh, void* surface);

	/* Blocking: shows a small transient doc (at least one `button`) parented to the active window and
	 * returns once a button fires. out_result_json receives {"button":"<id>","fields":{"<id>":"<value>",...}},
	 * the current value of each interactive node, truncated to out_buf_size. Returns 0 on a button click,
	 * -1 on bad arguments / malformed JSON. */
	int (*ui_modal_run)(void* mh, const char* title, const char* json_doc,
						char* out_result_json, int out_buf_size);
};
