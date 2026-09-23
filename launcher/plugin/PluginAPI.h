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
 *
 *
 * MMCOContext — the runtime context passed to mmco_init().
 *
 * This is the ONLY interface plugins have into MeshMC. All interaction
 * goes through function pointers in this struct. Plugins MUST NOT call
 * Qt, MeshMC internals, or any symbol not exposed here.
 *
 * The context is valid for the lifetime of the plugin (until mmco_unload()
 * returns). Pointers obtained through the API (e.g. strings from getters)
 * are valid until the next API call on the same module unless documented
 * otherwise.
 */

#pragma once

#include "plugin/PluginHooks.h"
#include <cstdint>
#include <cstddef>

/*

 */

/* Forward-declare callback typedefs used by the context */
typedef void (*MMCOHttpCallback)(void* user_data, int status_code,
								 const void* response_body,
								 size_t response_size);
typedef void (*MMCOMenuActionCallback)(void* user_data);
typedef void (*MMCODirEntryCallback)(void* user_data, const char* entry_name,
									 int is_dir);

/*
 * MMCOUiEventCallback — ABI 5. Single event callback shape for every
 * declarative UI surface (ui_surface_create) and for the declarative
 * tray menu (tray_set_menu).
 *
 *   surface_id — stable string identifying which surface/menu this
 *                event came from (a plugin with several surfaces
 *                sharing one callback distinguishes them by this).
 *                For tray-menu events this is always "tray".
 *   node_id    — the plugin-assigned id of the node that fired.
 *   event      — "click" (button/link/menu item), "change" (toggle/
 *                text_field/number_field/choice — value_json is the
 *                new value), or "select"/"activate" (list row —
 *                value_json is the row id).
 *   value_json — event-specific payload, or "" if not applicable.
 */
typedef void (*MMCOUiEventCallback)(void* user_data, const char* surface_id,
									const char* node_id, const char* event,
									const char* value_json);

/*
 * MMCOUiAnchor — ABI 5. Where a declarative UI surface (ui_surface_create)
 * is displayed:
 *
 *   MMCO_UI_ANCHOR_GLOBAL_SETTINGS   — stacked as a titled section inside
 *     the host's single "Plugins" page in the global Settings dialog.
 *   MMCO_UI_ANCHOR_INSTANCE_PAGE     — its own page in the instance window
 *     (anchor_context = instance id).
 *   MMCO_UI_ANCHOR_INSTANCE_SETTINGS — stacked as a titled section inside
 *     a "Plugins" group on that instance's Settings page
 *     (anchor_context = instance id).
 */
enum MMCOUiAnchor {
	MMCO_UI_ANCHOR_GLOBAL_SETTINGS = 0,
	MMCO_UI_ANCHOR_INSTANCE_PAGE = 1,
	MMCO_UI_ANCHOR_INSTANCE_SETTINGS = 2
};

/*
 * Tray-icon activation reason — passed to MMCOTrayActivationCallback.
 *
 *   0 = unknown
 *   1 = single click (left)
 *   2 = double click (left)
 *   3 = middle click
 *   4 = context menu (right click) — note that if the tray icon has a
 *       menu attached via tray_set_menu(), the menu is shown automatically
 *       and this callback fires *in addition* to the menu popup.
 *
 * Values mirror QSystemTrayIcon::ActivationReason 1..4.
 */
typedef void (*MMCOTrayActivationCallback)(void* user_data, int reason);

/*
 * Main-window close-event filter callback.
 *
 * Returns:
 *   0 = let the close happen normally (default Qt behaviour)
 *   1 = swallow the close event (e.g. hide-to-tray instead of quitting)
 *
 * If multiple filters are installed, the first one to return non-zero
 * wins; later filters in the chain are still invoked so they can run
 * their bookkeeping but cannot override the decision.
 */
typedef int (*MMCOMainWindowCloseCallback)(void* user_data);

struct MMCOContext {
	/* ABI guard */
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

	/* Set an environment variable for the current launching instance.
	 * The variable is injected via qputenv() before launch and removed
	 * via qunsetenv() after the instance stops. */
	int (*launch_set_env)(void* mh, const char* key, const char* value);

	/* Prepend a wrapper command for the current launching instance.
	 * If the instance already has a wrapper, this command is prepended. */
	int (*launch_prepend_wrapper)(void* mh, const char* wrapper_cmd);

	/* S16 — Application Settings (read-only global settings) */

	/* Read a global application setting by key. Returns the value as a
	 * string, or nullptr if the key does not exist. The returned pointer
	 * is valid until the next API call on the same module. */
	const char* (*app_setting_get)(void* mh, const char* key);
	/* S17 — News API */

	/* Returns the total number of loaded news entries across all feeds.
	 * Returns -1 if the news system is not available. */
	int (*news_get_entry_count)(void* mh);

	/* Returns the title of the news entry at the given index.
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_entry_title)(void* mh, int index);

	/* Returns the URL link of the news entry at the given index.
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_entry_link)(void* mh, int index);

	/* Returns the HTML/text content of the news entry at the given index.
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_entry_content)(void* mh, int index);

	/* Returns the author of the news entry at the given index.
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_entry_author)(void* mh, int index);

	/* Returns the publication date of the news entry at the given index
	 * as an ISO 8601 string (e.g. "2026-05-08T14:30:00").
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_entry_date)(void* mh, int index);

	/* Returns the feed URL index for the news entry at the given index.
	 * This identifies which feed the entry came from. */
	int (*news_get_entry_feed_index)(void* mh, int index);

	/* Registers an additional RSS feed URL to be fetched alongside the
	 * default feed. Returns 0 on success, -1 on failure.
	 * The URL is stored for the lifetime of the application. */
	int (*news_add_feed_url)(void* mh, const char* url);

	/* Returns the number of registered feed URLs (including the default). */
	int (*news_get_feed_count)(void* mh);

	/* Returns the feed URL at the given index.
	 * Returns nullptr if index is out of range. */
	const char* (*news_get_feed_url)(void* mh, int index);

	/* Triggers a reload of all news feeds. Non-blocking — results arrive
	 * via the MMCO_HOOK_NEWS_UPDATED hook. Returns 0 on success. */
	int (*news_reload)(void* mh);

	/* S18 — Plugin icon set (ABI 2+) */

	/* Resolve a logical icon name from the calling module's bundled
	 * icon set into a Qt resource path that can be passed to any other
	 * icon-name parameter in this context (ui_surface_create's
	 * icon_name, tray_create/tray_set_icon, a node's "icon" prop) —
	 * they all forward to QIcon::fromTheme() / QIcon::QIcon(QString).
	 *
	 * Returns a string of the form ":/plugins/<icon_set>/<name>" or
	 * nullptr if the module did not declare an icon_set_resource or
	 * the icon does not exist. The pointer is valid until the next
	 * API call on the same module.
	 *
	 * Example:
	 *   const char* iconPath = ctx->ui_plugin_icon(MMCO_MH, "settings");
	 *   ctx->ui_surface_create(MMCO_MH, MMCO_UI_ANCHOR_GLOBAL_SETTINGS,
	 *                          nullptr, "Settings", iconPath, doc, cb, ud);
	 */
	const char* (*ui_plugin_icon)(void* mh, const char* name);

	/* ───────────────────────────────────────────────────────────────
	 * S19 — System Tray (ABI 2+, additive)
	 *
	 * Lets a plugin own one or more QSystemTrayIcon instances and attach
	 * a declarative menu (see tray_set_menu, ABI 5) to them. Memory is
	 * owned by PluginManager — every tray handle handed out here is
	 * released either on tray_destroy, or automatically when the
	 * owning module is unloaded (no leaks at shutdown).
	 *
	 * All handles are opaque pointers — never cast them yourself.
	 * Returns from creation functions: nullptr on failure (e.g. system
	 * tray not available on the host desktop).
	 *
	 * Notify-style usage without a real tray icon: pass nullptr for the
	 * tray handle to tray_show_message() and the host falls back to a
	 * transient hidden icon — useful for DesktopNotifier-style plugins
	 * that do not want a persistent indicator.
	 * ─────────────────────────────────────────────────────────────── */

	/* Create a new system-tray icon. Returns opaque handle or nullptr if
	 * the platform has no usable system tray (call is_available() first
	 * to discriminate from other failures). */
	void* (*tray_create)(void* mh, const char* icon_name, const char* tooltip);

	/* Destroy a tray handle previously returned by tray_create(). Idempotent;
	 * passing nullptr is a no-op. */
	int (*tray_destroy)(void* mh, void* tray_handle);

	/* Returns 1 if QSystemTrayIcon::isSystemTrayAvailable() is true. */
	int (*tray_is_available)(void* mh);

	/* Update icon — accepts the same names as every other icon-name
	 * parameter in this context (theme names + ":/..." Qt resource
	 * paths). */
	int (*tray_set_icon)(void* mh, void* tray_handle, const char* icon_name);
	int (*tray_set_tooltip)(void* mh, void* tray_handle, const char* tooltip);
	int (*tray_set_visible)(void* mh, void* tray_handle, int visible);

	/* Show a balloon / native notification through the tray.
	 *   icon_type: 0=None, 1=Info, 2=Warning, 3=Critical
	 *   msecs:     timeout hint in ms (10000 if 0)
	 * If tray_handle is nullptr, a transient hidden tray is created and
	 * torn down behind the scenes — useful for fire-and-forget notify. */
	int (*tray_show_message)(void* mh, void* tray_handle, const char* title,
							 const char* message, int icon_type, int msecs);

	/* Attach a declarative menu to the tray icon — the menu pops up on
	 * right-click (ABI 5). `json_menu_doc` is a small "mmco-ui/1" tree
	 * whose root's children are `button` (menu item), `separator`, or
	 * `section` (submenu, itself containing more button/separator/
	 * section children) nodes. Clicking an item fires `cb` with
	 * event="click" and node_id = the item's id. Pass json_menu_doc =
	 * nullptr to detach the menu. `cb`/`user_data` replace the previous
	 * per-action MMCOMenuActionCallback plumbing — one callback serves
	 * every item in the doc. Re-call with a freshly built document to
	 * rebuild the menu (e.g. on MMCO_HOOK_INSTANCE_CREATED/REMOVED). */
	int (*tray_set_menu)(void* mh, void* tray_handle, const char* json_menu_doc,
						 MMCOUiEventCallback cb, void* user_data);

	/* Register an activation callback (fires on left/middle/double click).
	 * Pass cb=nullptr to clear. Only one callback per tray. */
	int (*tray_set_activation_cb)(void* mh, void* tray_handle,
								  MMCOTrayActivationCallback cb, void* ud);

	/* ───────────────────────────────────────────────────────────────
	 * S20 — Main-window helpers (ABI 2+, additive)
	 * ─────────────────────────────────────────────────────────────── */

	/* Install a filter that runs whenever the main window receives a
	 * QCloseEvent. The callback decides whether the close proceeds
	 * (return 0) or is swallowed and the window is hidden instead
	 * (return 1 — host calls hide() on the main window).
	 *
	 * Multiple filters can be installed; PluginManager removes all of
	 * a plugin's filters automatically on mmco_unload(). Pass cb=nullptr
	 * to clear all filters previously installed by this module.
	 *
	 * Returns 0 on success, -1 if the main window is not yet alive
	 * (e.g. called too early — wait for MMCO_HOOK_UI_MAIN_READY). */
	int (*main_window_install_close_filter)(void* mh,
											MMCOMainWindowCloseCallback cb,
											void* user_data);

	/* Show / raise the main window (counterpart to the close filter for
	 * "Show MeshMC" tray actions). hide() is also exposed for symmetry. */
	int (*main_window_show)(void* mh);
	int (*main_window_hide)(void* mh);
	int (*main_window_is_visible)(void* mh);

	/* ───────────────────────────────────────────────────────────────
	 * S21 — Application-scope settings (ABI 3+, additive)
	 *
	 * The S16 `app_setting_get` field above is read-only and limited to
	 * settings that have already been registered.  S21 adds the write
	 * side, a registration helper, and an existence probe so plugins
	 * never need to reach into APPLICATION->settings() directly:
	 *
	 *   • app_setting_register: create the key if missing with the
	 *     supplied UTF-8 default. Returns 0 on success, -1 on error,
	 *     and is a no-op (returning 0) when the key already exists.
	 *   • app_setting_set:      overwrite the value of an existing key.
	 *     Returns 0 on success, -1 on error.  The caller is expected
	 *     to have registered the key first (either via this API or by
	 *     the launcher itself).
	 *   • app_setting_contains: 1 if the key is registered, 0 otherwise.
	 *
	 * All three are namespaced exactly the way the launcher's own
	 * settings are — there is no automatic plugin-private prefix on
	 * top.  Callers that want a private namespace should keep using
	 * setting_get / setting_set from S3.
	 * ─────────────────────────────────────────────────────────────── */
	int (*app_setting_set)(void* mh, const char* key, const char* value);
	int (*app_setting_register)(void* mh, const char* key,
								const char* default_value);
	int (*app_setting_contains)(void* mh, const char* key);

	/* ───────────────────────────────────────────────────────────────
	 * S22 — Themed icon resolution (ABI 3+, additive)
	 *
	 * Resolves a logical name to a string suitable for the existing
	 * icon-name parameters in S12/S13/S19 (button/tray/menu APIs all
	 * accept either an XDG theme name or a ":/..." Qt resource path).
	 *
	 * The returned pointer is valid until the next API call on the
	 * same module. Returns nullptr if the name does not resolve.
	 *
	 * Replaces direct calls to APPLICATION->getThemedIcon(name) and
	 * APPLICATION->icons()->getIcon(name) inside plugin code.
	 * ─────────────────────────────────────────────────────────────── */
	const char* (*ui_themed_icon)(void* mh, const char* name);

	/* ───────────────────────────────────────────────────────────────
	 * S23 — Instance running-state signal bridge (ABI 3+, additive)
	 *
	 * Replaces the legacy pattern of resolving an InstancePtr via
	 * APPLICATION->instances()->getInstanceById(id) and connecting to
	 * its `runningStatusChanged(bool)` Qt signal.
	 *
	 *   instance_running_register   — start delivering running-state
	 *     transitions for the given instance id to `cb(ud, id, r)`.
	 *     The host owns the underlying Qt connection through a
	 *     per-module guard QObject; mmco_unload() automatically
	 *     severs every connection for that module.
	 *     Returns 0 on success, -1 if the id does not resolve.
	 *
	 *   instance_running_unregister — stop delivering transitions for
	 *     the given id.  Returns 0 on success.  Calling with an id
	 *     that has no registration is a no-op (returns 0).
	 *
	 * A single module may have at most one callback per instance id.
	 * Re-registering replaces the previous callback for that id.
	 * ─────────────────────────────────────────────────────────────── */
	int (*instance_running_register)(void* mh, const char* instance_id,
									 MMCOInstanceRunningCallback cb, void* ud);
	int (*instance_running_unregister)(void* mh, const char* instance_id);

	/* ───────────────────────────────────────────────────────────────
	 * S24 — Per-instance settings (ABI 3+, additive)
	 *
	 * Replaces inst->settings()->{get,set,registerSetting,
	 * registerOverride,reset,contains}.  All values are exchanged as
	 * UTF-8 strings; plugins convert to bool / int as needed.
	 *
	 * Each call resolves the BaseInstance via APPLICATION->instances()
	 * inside PluginManager and routes the call to the instance's own
	 * SettingsObject. Returns 0 on success, -1 on failure (unknown
	 * instance id, missing key, etc.).  The "register override" call
	 * mirrors SettingsObject::registerOverride(): expose an
	 * application-scope setting on the instance, controlled by a
	 * per-instance bool gate (`gate_key`).
	 * ─────────────────────────────────────────────────────────────── */
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

	/* ───────────────────────────────────────────────────────────────
	 * S25 — MinecraftAccount / skin / cape access (ABI 3+, additive)
	 *
	 * Replaces SkinManager's direct AccountList / MinecraftAccountPtr
	 * / AccountData access. PluginManager looks up the account on the
	 * host's AccountList by id and returns the requested field.
	 *
	 * `account_get_id_by_index` exposes the account id matching S7's
	 * `account_get_profile_id` — same indexing, separate getter so
	 * plugins can scan by index and then drive everything else by id.
	 *
	 * Skin / cape blobs are PNG byte sequences; the pointer returned
	 * via `out_ptr` is valid until the next API call on the same
	 * module. The function returns the size in bytes (0 if absent,
	 * -1 on error).
	 *
	 * Variant strings are "CLASSIC" / "SLIM" (case-insensitive).
	 * Setters return 0 on success, -1 on failure.  account_skin_blob
	 * is the in-memory cache; account_skin_upload below is what
	 * actually pushes a new skin to Mojang's servers.
	 * ─────────────────────────────────────────────────────────────── */
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

	/* S26 — Synchronous task helpers (ABI 3+, additive)
	 *
	 * Run launcher-side network tasks that used to be wrapped in
	 * SequentialTask + ProgressDialog::execWithTask. The C-ABI pumps
	 * a modal progress dialog parented to the active window so the
	 * UX matches what the plugin used to drive manually.
	 *
	 *   account_skin_upload  — POST a new skin PNG (variant == "STEVE"
	 *                          or "ALEX").
	 *   account_skin_reset   — DELETE the active skin.
	 *   account_cape_set     — PUT the active cape id (empty string
	 *                          == "no cape").
	 *
	 * Returns 0 on success, -1 on failure (the task reported an error
	 * or the user cancelled). */
	int (*account_skin_upload)(void* mh, const char* account_id,
							   const void* png_bytes, int64_t size,
							   const char* variant);
	int (*account_skin_reset)(void* mh, const char* account_id);
	int (*account_cape_set)(void* mh, const char* account_id,
							const char* cape_id);

	/* ───────────────────────────────────────────────────────────────
	 * S27 — Icon list enumeration (ABI 3+, additive)
	 *
	 * Exposes the launcher's IconList through the C ABI for plugins
	 * that need to render an icon picker (Filelink's "choose
	 * shortcut icon" dialog is the canonical case). Replaces direct
	 * APPLICATION->icons() / IconList::rowCount() / IconList::data()
	 * access.
	 *
	 *   icon_list_count    — number of icons known to the host.
	 *   icon_list_get_key  — stable key for the icon at the given
	 *                        index (suitable for instance_set_icon_key
	 *                        and tray/menu icon parameters).
	 *   icon_list_get_name — human-readable display name (same
	 *                        column the launcher's icon list shows).
	 *   icon_list_get_file_path — absolute path on disk for the icon
	 *                        (or "" if the icon is in-memory only).
	 *                        Used by Filelink to embed the icon in a
	 *                        platform-native shortcut.
	 *   icon_list_save_png — write the icon's PNG bytes to
	 *                        `dest_path`. Returns 0 on success.
	 * ─────────────────────────────────────────────────────────────── */
	int (*icon_list_count)(void* mh);
	const char* (*icon_list_get_key)(void* mh, int index);
	const char* (*icon_list_get_name)(void* mh, int index);
	const char* (*icon_list_get_file_path)(void* mh, const char* icon_key);
	int (*icon_list_save_png)(void* mh, const char* icon_key,
							  const char* dest_path);

	/* ───────────────────────────────────────────────────────────────
	 * S28 — Instance update-flag write (ABI 3+, additive)
	 *
	 * Companion setter to `instance_has_update`. Lets plugins drive
	 * the per-instance "update available" indicator that the
	 * InstanceView delegate renders as the `checkupdate` badge.
	 *
	 * Until this API landed, `BaseInstance::m_hasUpdate` had a
	 * setter (`setUpdateAvailable`) but no caller anywhere in the
	 * codebase, which meant the badge slot was permanently dark.
	 * The launcher intentionally has no built-in modpack-update
	 * scanner — that surface belongs to a plugin (PackUpdater). This
	 * API is the seam through which such a plugin reports its
	 * findings back to the launcher's UI.
	 *
	 *   value != 0 → mark instance as having an update available
	 *   value == 0 → clear the flag
	 *
	 * Returns 0 on success, -1 if the instance id is unknown.
	 * Thread: main / GUI thread only (touches BaseInstance which
	 * emits propertiesChanged).
	 * ─────────────────────────────────────────────────────────────── */
	int (*instance_set_update_available)(void* mh, const char* id, int value);

	/* ───────────────────────────────────────────────────────────────
	 * S29 — Component version write (ABI 3+, additive)
	 *
	 * Set the version of a `PackProfile` component by uid, or create
	 * the component if the instance doesn't have one yet. Mirrors
	 * `PackProfile::setComponentVersion(uid, version, important)`
	 * which is the same call the in-tree pack importers (Flame,
	 * Modrinth, FTB, ATL, Technic) use to wire loader + Minecraft
	 * versions into a freshly imported instance.
	 *
	 * The complement of the read-only `instance_component_get_version`
	 * accessor: where that one resolves by *index* (after counting
	 * via `instance_component_count`), this one resolves by *uid*,
	 * which is what a plugin actually knows when it wants to set a
	 * specific loader's version (`net.fabricmc.fabric-loader`,
	 * `net.neoforged`, `net.minecraft`, …).
	 *
	 * Behaviour:
	 *   - If a component with that uid exists, its version is reverted
	 *     to the catalogue entry and the requested version is applied
	 *     (`important=1` flags it as user-pinned so the resolver
	 *     doesn't try to upgrade it out from under you).
	 *   - If the component doesn't exist, a new one is appended to
	 *     the profile with the given version.
	 *
	 * Returns 0 on success, -1 on failure (unknown instance, no
	 * PackProfile, or PackProfile::setComponentVersion refused —
	 * typically because the component couldn't be reverted to a clean
	 * state, see PackProfile::setComponentVersion's `revert()` path).
	 *
	 * Thread: main / GUI thread only. The change emits dataChanged
	 * on the PackProfile model, which Qt expects from the GUI thread.
	 * ─────────────────────────────────────────────────────────────── */
	int (*instance_component_set_version)(void* mh, const char* id,
										  const char* uid, const char* version);

	/* ───────────────────────────────────────────────────────────────
	 * S30 — HTTP GET with custom headers (ABI 3+, additive)
	 *
	 * S11's `http_get` only ever sets User-Agent. Plugins that talk
	 * to APIs gated by auth headers (CurseForge's `x-api-key` is
	 * the canonical case) use this variant instead. Keys come from
	 * the plugin's own `#include "BuildConfig.h"` — the launcher
	 * does NOT pass keys through this surface; that's deliberate so
	 * a key doesn't have to traverse a generic plugin API.
	 *
	 * `headers` is an array of NUL-terminated "Name: Value"
	 * strings; `header_count` is the array length. The launcher
	 * always sets/overrides User-Agent regardless of what you pass,
	 * to keep outbound traffic identifiable.
	 *
	 * Returns 0 if the request was queued, -1 on argument errors.
	 * Body-lifetime contract matches `http_get`.
	 * ─────────────────────────────────────────────────────────────── */
	int (*http_get_with_headers)(void* mh, const char* url,
								 const char* const* headers, int header_count,
								 MMCOHttpCallback callback, void* user_data);

	/* ───────────────────────────────────────────────────────────────
	 * S31 — Subprocess execution (additive; no ABI bump)
	 *
	 * Run an external program synchronously and capture its output.
	 * The host runs it through QProcess, so plugins (including plain-C
	 * plugins that have no Qt of their own) can shell out to version
	 * control tools, archivers, or any other CLI without forking or
	 * exec()ing themselves — the host owns process lifetime, timeout,
	 * working directory, and stdin/stdout handling.
	 *
	 * Arguments:
	 *   program        — executable name or absolute path. Resolved
	 *                    against PATH by the host. NOT run through a
	 *                    shell, so there is no shell-injection surface:
	 *                    each argv element is passed verbatim.
	 *   args           — array of `arg_count` NUL-terminated argument
	 *                    strings (argv[1..]). May be NULL if arg_count
	 *                    is 0.
	 *   arg_count      — number of entries in `args`.
	 *   working_dir    — directory to run in, or NULL for the host's
	 *                    current working directory.
	 *   stdin_data     — bytes to feed to the child's stdin, or NULL.
	 *   stdin_size     — length of stdin_data in bytes (0 if none).
	 *   out_buf        — caller-owned buffer that receives the child's
	 *                    captured stdout (NUL-terminated, truncated to
	 *                    fit). May be NULL to discard stdout.
	 *   out_buf_size   — size of out_buf in bytes (including the NUL).
	 *   out_exit_code  — receives the child's exit code, or NULL.
	 *   timeout_ms     — hard timeout; the child is killed if it
	 *                    exceeds it. <= 0 means a default (30s).
	 *
	 * Returns:
	 *    0  — the process ran to completion (check out_exit_code for
	 *         the program's own success/failure).
	 *   -1  — failed to start (program not found, etc.).
	 *   -2  — timed out and was killed.
	 *   -3  — invalid arguments.
	 *
	 * Thread: may be called from any thread; the host pumps a local
	 * event loop internally so it is safe from the GUI thread too.
	 * ─────────────────────────────────────────────────────────────── */
	int (*process_run)(void* mh, const char* program, const char* const* args,
					   int arg_count, const char* working_dir,
					   const char* stdin_data, int stdin_size, char* out_buf,
					   int out_buf_size, int* out_exit_code, int timeout_ms);

	/* ───────────────────────────────────────────────────────────────
	 * S32 — Background hooks + progress reporting (additive; ABI
	 * stays at 3, `struct_size` tells the host what a module was
	 * built against)
	 *
	 * hook_register_ex is hook_register plus a flags word — see
	 * MMCO_HOOK_FLAG_* in PluginHooks.h. Passing 0 behaves exactly
	 * like hook_register, so opting in is a one-line change.
	 *
	 * progress_report publishes the running background callback's
	 * progress into the launcher's progress dialog:
	 *   handle  — the module_handle the callback was invoked with.
	 *   status  — short line shown as the row's title, NULL to keep
	 *             the current one.
	 *   details — optional second line, NULL to keep the current one.
	 *   current — work done so far.
	 *   total   — total work; <= 0 means "indeterminate", which the
	 *             host renders as a busy bar.
	 *
	 * Returns 0 if the update was queued, -1 if the arguments are
	 * unusable or the caller is not currently inside a background hook
	 * callback (there is no row to report into). The host marshals the
	 * update onto the GUI thread, so calling this in a tight loop is
	 * safe and never blocks on the UI.
	 * ─────────────────────────────────────────────────────────────── */
	int (*hook_register_ex)(void* mh, uint32_t hook_id,
							MMCOHookCallback callback, void* user_data,
							uint32_t flags);
	int (*progress_report)(void* handle, const char* status,
						   const char* details, int64_t current,
						   int64_t total);

	/* ───────────────────────────────────────────────────────────────
	 * S33 — Declarative UI surfaces (ABI 5)
	 *
	 * Replaces the S13 imperative widget builder (ui_page_create /
	 * ui_layout_* / ui_button_* / ui_label_* / ui_tree_*, all gone as
	 * of ABI 5) and the allWidgets()/findChild() settings-injection
	 * pattern. A plugin describes a small widget tree as a JSON
	 * document (the "mmco-ui/1" format — see PluginUiRenderer.h) and
	 * the host renders and owns the real QWidget tree; no QWidget* is
	 * ever handed back to a plugin.
	 *
	 *   ui_surface_create — build and display a surface at the given
	 *     anchor. `anchor_context` is nullptr for GLOBAL_SETTINGS, or
	 *     the instance id for INSTANCE_PAGE / INSTANCE_SETTINGS.
	 *     `title`/`icon_name` label the surface (page title for
	 *     INSTANCE_PAGE, section title otherwise). `cb`/`user_data`
	 *     receive every click/change/select event from nodes in the
	 *     doc (see MMCOUiEventCallback). Returns an opaque surface
	 *     handle, or nullptr on failure (bad JSON, unknown anchor).
	 *   ui_surface_update — replace the whole document.
	 *   ui_surface_set    — patch one node's `props` (e.g. a toggle's
	 *     value, a button's enabled state) without touching the rest
	 *     of the tree.
	 *   ui_surface_set_rows — replace a `list` node's `rows` only;
	 *     the cheap refresh path, replacing the old
	 *     ui_tree_clear + ui_tree_add_row loop.
	 *   ui_surface_destroy — tear down a surface early. Every surface
	 *     a module still owns is also torn down automatically when
	 *     the module unloads.
	 *
	 * All four mutators return 0 on success, -1 on failure (unknown
	 * surface handle, malformed JSON, or unknown node_id).
	 * ─────────────────────────────────────────────────────────────── */
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

	/* Blocking: shows a small transient doc (must contain at least one
	 * `button`) parented to the active window, pumps a local event
	 * loop (same pattern as S26's account_skin_upload), and returns
	 * once a button fires. `out_result_json` receives
	 * {"button":"<id>","fields":{"<id>":"<value>", ...}} — one entry
	 * per interactive node's current value at the time the button was
	 * clicked, truncated to fit `out_buf_size`. Returns 0 on a button
	 * click, -1 on bad arguments / malformed JSON. */
	int (*ui_modal_run)(void* mh, const char* title, const char* json_doc,
						char* out_result_json, int out_buf_size);
};
