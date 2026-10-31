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
 * Kite Plugin SDK: C ABI header for .mmco module development.
 *
 * The canonical, language-neutral definition of the MMCO ABI. Pure C99 with no Qt and no C++ types, so
 * it can be consumed from C, C++, Rust (bindgen), Zig and any other language that speaks the C ABI.
 *
 *   C plugins:    include this header. It covers logging, hooks, instances, mods, worlds, accounts,
 *                 java, fs, zip, http, news, settings, launch modifiers and the declarative UI.
 *   C++ plugins:  include "mmco_cxx_sdk.hpp", which includes this header and adds the Qt includes.
 *
 * Usage:
 *   1. #include "mmco_c_sdk.h"
 *   2. Define the module with MMCO_DEFINE_MODULE(...) and implement mmco_init() and mmco_unload()
 *   3. Build a shared library with the .mmco extension
 *   4. Install it in a search path:
 *        <app_dir>/mmcmodules/                    (Linux + Windows portable)
 *        Kite.app/Contents/PlugIns/mmcmodules/    (macOS app bundle)
 *        ~/.local/lib/mmcmodules/                 (user-local on Linux)
 *        /usr/local/lib/mmcmodules/               (system-wide)
 *        /usr/lib/mmcmodules/                     (distro packages)
 *
 * Plugins must not include Qt or Kite headers directly, and must not fork or exec processes
 * (use process_run).
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MMCO_MAGIC 0x4D4D434F
#define MMCO_VERSION "10.0.0"
#define MMCO_ABI_VERSION 5
#define MMCO_EXTENSION ".mmco"
#define MMCO_FLAG_NONE 0x00000000
#define MMCO_TRAILER_MAGIC 0x53434D4D /* ASCII "MMCS" — see MMCOFormat.h */
#define MMCO_VERNUM                                                            \
	0x0A000000L /* MMNNRRSM: major minor revision status modified */
#define MMCO_VER_MAJOR 10
#define MMCO_VER_MINOR 0
#define MMCO_VER_REVISION 0
#define MMCO_VER_STATUS 0 /* 0=devel, 1-E=beta, F=Release (DEPRECATED) */
#define MMCO_VER_STATUSH                                                       \
	0x0 /* Hex values: 0=devel, 1-9=beta, A-E=Release Candidate, F=Release */
#define MMCO_VER_MODIFIED 0 /* non-zero if modified externally from mmco */

/* Symbol visibility for .mmco shared libraries */
#if defined(_WIN32) || defined(__CYGWIN__)
#define MMCO_EXPORT __declspec(dllexport)
#else
#define MMCO_EXPORT __attribute__((visibility("default")))
#endif

/* Optional dependency on another .mmco module. */
typedef struct MMCODependency {
	const char* name;
	const char* min_version; /* NULL or "" = any version */
	uint32_t optional;		 /* non-zero = optional dep */
} MMCODependency;

typedef struct MMCOModuleInfo {
	uint32_t magic;
	uint32_t abi_version;
	const char* name;
	const char* version;
	const char* author;
	const char* description;
	const char* license;
	uint32_t flags;
	const char* code_link;

	/* Icon set, dependency table, GPG signing key — see MMCOFormat.h */
	const char* icon_set_resource;
	const MMCODependency* dependencies;
	uint32_t dependency_count;
	const char* signing_key_id;
} MMCOModuleInfo;

/* Pinned to 32 bits with a sentinel (`enum : uint32_t` is not portable to C99); hook_id crosses the
 * ABI as uint32_t. */
enum MMCOHookId {
	MMCO_HOOK_APP_INITIALIZED = 0x0100,
	MMCO_HOOK_APP_SHUTDOWN = 0x0101,
	MMCO_HOOK_INSTANCE_PRE_LAUNCH = 0x0200,
	MMCO_HOOK_INSTANCE_POST_LAUNCH = 0x0201,
	MMCO_HOOK_INSTANCE_CREATED = 0x0202,
	MMCO_HOOK_INSTANCE_REMOVED = 0x0203,
	MMCO_HOOK_SETTINGS_CHANGED = 0x0300,
	MMCO_HOOK_CONTENT_PRE_DOWNLOAD = 0x0400,
	MMCO_HOOK_CONTENT_POST_DOWNLOAD = 0x0401,
	MMCO_HOOK_NETWORK_PRE_REQUEST = 0x0500,
	MMCO_HOOK_NETWORK_POST_REQUEST = 0x0501,
	MMCO_HOOK_UI_MAIN_READY = 0x0600,
	MMCO_HOOK_UI_CONTEXT_MENU = 0x0601,
	MMCO_HOOK_UI_INSTANCE_PAGES = 0x0602,
	MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES = 0x0603,

	/* ABI 3+. Global settings dialog about to open. Payload: nullptr. */
	MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN = 0x0604,

	/* ABI 3+. Per-instance settings page constructed. Payload: MMCOInstanceSettingsPageEvent*. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED = 0x0605,
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED = 0x0606,
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING = 0x0607,

	/* News */
	MMCO_HOOK_NEWS_UPDATED = 0x0700, /* payload: nullptr */

	/* Plugin-driven authentication providers (ABI 2+ additive) */
	MMCO_HOOK_AUTH_REQUEST = 0x0800, /* payload: MMCOAuthRequestEvent* */
	MMCO_HOOK_SESSION_FILL = 0x0801, /* payload: MMCOSessionFillEvent* */

	/* Forces a 32-bit underlying type across C and C++ compilers; never a real hook id. */
	MMCO_HOOK_FORCE_UINT32 = 0x7FFFFFFF
};

typedef struct MMCOInstanceInfo {
	const char* instance_id;
	const char* instance_name;
	const char* instance_path;
	const char* minecraft_version;
} MMCOInstanceInfo;

typedef struct MMCOSettingChange {
	const char* key;
	const char* old_value;
	const char* new_value;
} MMCOSettingChange;

typedef struct MMCOContentEvent {
	const char* instance_id;
	const char* file_name;
	const char* url;
	const char* target_path;
} MMCOContentEvent;

typedef struct MMCONetworkEvent {
	const char* url;
	const char* method;
	int status_code;
} MMCONetworkEvent;

typedef struct MMCOMenuEvent {
	const char* context;
	void* menu_handle;
} MMCOMenuEvent;

typedef struct MMCOInstancePagesEvent {
	const char* instance_id;
	const char* instance_name;
	const char* instance_path;
	void* page_list_handle;
	void* instance_handle;
} MMCOInstancePagesEvent;

/* ABI 3+. Payload for MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED. page_handle is an opaque QWidget*
 * (the instance settings page), instance_handle an opaque BaseInstance*.
 * qobject_cast<QWidget*>(page_handle) is fine; do not cast to launcher-private types. */
typedef struct MMCOInstanceSettingsPageEvent {
	const char* instance_id;
	void* page_handle;
	void* instance_handle;
} MMCOInstanceSettingsPageEvent;

/* ABI 3+. Callback for MMCOContext::instance_running_register; `running` is 1 when the instance
 * just started, 0 when it stopped. */
typedef void (*MMCOInstanceRunningCallback)(void* user_data,
											const char* instance_id,
											int running);

/* Payload for MMCO_HOOK_UI_MAIN_READY (ABI 2+): opaque handles to the main window's widgets, valid for
 * its lifetime; do not delete them or take ownership. All fields are null under the QML shell.
 *   main_window        QMainWindow* (MainWindow*)
 *   news_toolbar       QToolBar*
 *   more_news_action   QAction* (the "More News..." action)
 *   news_label_button  QToolButton* (the clickable news headline) */
typedef struct MMCOUiMainReadyPayload {
	void* main_window;
	void* news_toolbar;
	void* more_news_action;
	void* news_label_button;
} MMCOUiMainReadyPayload;

typedef struct MMCOGlobalSettingsPagesEvent {
	void* page_list_handle;
} MMCOGlobalSettingsPagesEvent;

/* Payload for MMCO_HOOK_AUTH_REQUEST (ABI 2+).
 *   url               effective URL of the in-flight request (read-only)
 *   method            "GET" | "POST" | "PUT" | "DELETE" | ... (read-only)
 *   body / body_size  POST/PUT body bytes (read-only); NULL/0 for GETs
 *   redirect_url      non-null rewrites the URL before send; the host copies the string
 *   request_handle    opaque cookie to pass to add_header()
 *   add_header        appends "Key: value" to the request's HTTP headers; returns 0 on success
 * Returning non-zero cancels the request: AuthRequest emits a network error and the AuthStep fails. */
typedef struct MMCOAuthRequestEvent {
	const char* url;
	const char* method;
	const char* body;
	int body_size;

	const char* redirect_url;

	void* request_handle;
	int (*add_header)(void* request_handle, const char* key, const char* value);
} MMCOAuthRequestEvent;

/* Payload for MMCO_HOOK_SESSION_FILL (ABI 2+): lets a plugin overwrite AuthSession fields after the
 * host filled its defaults and before the launch task is built. NULL in an overwrite_* slot keeps the
 * host default; the host copies the strings.
 *   account_id, account_is_msa (1 = MSA, 0 = offline), wants_online (1 = online launch requested)
 *   current_*              read-only snapshot of the host-filled session fields
 *   overwrite_*            non-null replaces the corresponding field
 *   extra_user_properties  appended to AuthSession::user_properties verbatim; NULL adds nothing */
typedef struct MMCOSessionFillEvent {
	const char* account_id;
	int account_is_msa;
	int wants_online;

	const char* current_player_name;
	const char* current_uuid;
	const char* current_user_type;

	const char* overwrite_access_token;
	const char* overwrite_session;
	const char* overwrite_player_name;
	const char* overwrite_uuid;
	const char* overwrite_user_type;
	const char* overwrite_client_token;

	const char* extra_user_properties;
} MMCOSessionFillEvent;

typedef int (*MMCOHookCallback)(void* module_handle, uint32_t hook_id,
								void* payload, void* user_data);

/* Flags for hook_register_ex (S32). 0 runs the callback inline.
 *
 * MMCO_HOOK_FLAG_BACKGROUND runs the callback on a worker thread, shown as its own row in the progress
 * dialog. It must not touch ui_*, tray_* or main_window_* functions or Qt widgets, and may assume only
 * that it is not on the GUI thread. It may use log_*, fs_*, zip_*, process_run and progress_report.
 * Settings and instance/account/mod/world accessors read GUI-thread state: read what you need in a
 * second, inline callback for the same hook (a dispatch runs all inline callbacks before any
 * background one) and hand it over. Dispatch waits for every background callback before returning. */
#define MMCO_HOOK_FLAG_NONE 0u
#define MMCO_HOOK_FLAG_BACKGROUND 1u

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
 *   GLOBAL_SETTINGS    titled section in the host's "Plugins" page of the global Settings dialog
 *   INSTANCE_PAGE      its own page in the instance window (anchor_context = instance id)
 *   INSTANCE_SETTINGS  titled section in a "Plugins" group on the instance's Settings page
 *                      (anchor_context = instance id) */
enum MMCOUiAnchor {
	MMCO_UI_ANCHOR_GLOBAL_SETTINGS = 0,
	MMCO_UI_ANCHOR_INSTANCE_PAGE = 1,
	MMCO_UI_ANCHOR_INSTANCE_SETTINGS = 2
};

/* S19 tray activation reason, the value of QSystemTrayIcon::ActivationReason (Qt 6): 0 Unknown, 1
 * Context (right click), 2 DoubleClick, 3 Trigger (single left click), 4 MiddleClick. A left click
 * is 3, not 1. */
typedef void (*MMCOTrayActivationCallback)(void* user_data, int reason);

/* S20 Main-window close filter. Return 0 to let the close proceed, 1 to swallow it (hide-to-tray). */
typedef int (*MMCOMainWindowCloseCallback)(void* user_data);

typedef struct MMCOContext {
	uint32_t struct_size;
	uint32_t abi_version;
	void* module_handle;

	/* S1 — Logging */
	void (*log_info)(void* mh, const char* msg);
	void (*log_warn)(void* mh, const char* msg);
	void (*log_error)(void* mh, const char* msg);
	void (*log_debug)(void* mh, const char* msg);

	/* S2 — Hooks */
	int (*hook_register)(void* mh, uint32_t hook_id, MMCOHookCallback callback,
						 void* user_data);
	int (*hook_unregister)(void* mh, uint32_t hook_id,
						   MMCOHookCallback callback);

	/* S3 — Settings */
	const char* (*setting_get)(void* mh, const char* key);
	int (*setting_set)(void* mh, const char* key, const char* value);

	/* S4 — Instance Management */
	int (*instance_count)(void* mh);
	const char* (*instance_get_id)(void* mh, int index);
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
	int (*instance_is_running)(void* mh, const char* id);
	int (*instance_can_launch)(void* mh, const char* id);
	int (*instance_has_crashed)(void* mh, const char* id);
	int (*instance_has_update)(void* mh, const char* id);
	int64_t (*instance_get_total_play_time)(void* mh, const char* id);
	int64_t (*instance_get_last_play_time)(void* mh, const char* id);
	int64_t (*instance_get_last_launch)(void* mh, const char* id);
	int (*instance_launch)(void* mh, const char* id, int online);
	int (*instance_kill)(void* mh, const char* id);
	int (*instance_delete)(void* mh, const char* id);
	const char* (*instance_get_group)(void* mh, const char* id);
	int (*instance_set_group)(void* mh, const char* id, const char* group);
	int (*instance_group_count)(void* mh);
	const char* (*instance_group_at)(void* mh, int index);
	int (*instance_component_count)(void* mh, const char* id);
	const char* (*instance_component_get_uid)(void* mh, const char* id,
											  int index);
	const char* (*instance_component_get_name)(void* mh, const char* id,
											   int index);
	const char* (*instance_component_get_version)(void* mh, const char* id,
												  int index);
	const char* (*instance_get_mc_version)(void* mh, const char* id);
	const char* (*instance_get_jar_mods_dir)(void* mh, const char* id);
	const char* (*instance_get_resource_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_texture_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_shader_packs_dir)(void* mh, const char* id);
	const char* (*instance_get_worlds_dir)(void* mh, const char* id);

	/* S5 — Mod Management (type:
	 * "loader","core","resourcepack","texturepack","shaderpack") */
	int (*mod_count)(void* mh, const char* instance_id, const char* type);
	const char* (*mod_get_name)(void* mh, const char* iid, const char* type,
								int index);
	const char* (*mod_get_version)(void* mh, const char* iid, const char* type,
								   int index);
	const char* (*mod_get_filename)(void* mh, const char* iid, const char* type,
									int index);
	const char* (*mod_get_description)(void* mh, const char* iid,
									   const char* type, int index);
	int (*mod_is_enabled)(void* mh, const char* iid, const char* type,
						  int index);
	int (*mod_set_enabled)(void* mh, const char* iid, const char* type,
						   int index, int enabled);
	int (*mod_remove)(void* mh, const char* iid, const char* type, int index);
	int (*mod_install)(void* mh, const char* iid, const char* type,
					   const char* filepath);
	int (*mod_refresh)(void* mh, const char* iid, const char* type);

	/* S6 — World Management */
	int (*world_count)(void* mh, const char* instance_id);
	const char* (*world_get_name)(void* mh, const char* iid, int index);
	const char* (*world_get_folder)(void* mh, const char* iid, int index);
	int64_t (*world_get_seed)(void* mh, const char* iid, int index);
	int (*world_get_game_type)(void* mh, const char* iid, int index);
	int64_t (*world_get_last_played)(void* mh, const char* iid, int index);
	int (*world_delete)(void* mh, const char* iid, int index);
	int (*world_rename)(void* mh, const char* iid, int index, const char* name);
	int (*world_install)(void* mh, const char* iid, const char* filepath);
	int (*world_refresh)(void* mh, const char* iid);

	/* S7 — Account Management (read-only) */
	int (*account_count)(void* mh);
	const char* (*account_get_profile_name)(void* mh, int index);
	const char* (*account_get_profile_id)(void* mh, int index);
	const char* (*account_get_type)(void* mh, int index);
	int (*account_get_state)(void* mh, int index);
	int (*account_is_active)(void* mh, int index);
	int (*account_get_default_index)(void* mh);

	/* S8 — Java Management (read-only) */
	int (*java_count)(void* mh);
	const char* (*java_get_version)(void* mh, int index);
	const char* (*java_get_arch)(void* mh, int index);
	const char* (*java_get_path)(void* mh, int index);
	int (*java_is_recommended)(void* mh, int index);
	const char* (*instance_get_java_version)(void* mh, const char* id);

	/* S9 — Filesystem */
	const char* (*fs_plugin_data_dir)(void* mh);
	int64_t (*fs_read)(void* mh, const char* rel_path, void* buf, size_t sz);
	int (*fs_write)(void* mh, const char* rel_path, const void* data,
					size_t sz);
	int (*fs_exists)(void* mh, const char* rel_path);
	int (*fs_mkdir)(void* mh, const char* abs_path);
	int (*fs_exists_abs)(void* mh, const char* abs_path);
	int (*fs_remove)(void* mh, const char* abs_path);
	int (*fs_copy_file)(void* mh, const char* src, const char* dst);
	int64_t (*fs_file_size)(void* mh, const char* abs_path);
	int (*fs_list_dir)(void* mh, const char* abs_path, int type,
					   MMCODirEntryCallback callback, void* user_data);

	/* S10 — Zip / Archive */
	int (*zip_compress_dir)(void* mh, const char* zip_path,
							const char* dir_path);
	int (*zip_extract)(void* mh, const char* zip_path, const char* target_dir);

	/* S11 — Network */
	int (*http_get)(void* mh, const char* url, MMCOHttpCallback callback,
					void* user_data);
	int (*http_post)(void* mh, const char* url, const void* body,
					 size_t body_size, const char* content_type,
					 MMCOHttpCallback callback, void* user_data);

	/* S12 — UI: Dialogs */
	void (*ui_show_message)(void* mh, int type, const char* title,
							const char* msg);
	int (*ui_add_menu_item)(void* mh, void* menu_handle, const char* label,
							const char* icon_name, MMCOMenuActionCallback cb,
							void* ud);
	const char* (*ui_file_open_dialog)(void* mh, const char* title,
									   const char* filter);
	const char* (*ui_file_save_dialog)(void* mh, const char* title,
									   const char* default_name,
									   const char* filter);
	const char* (*ui_input_dialog)(void* mh, const char* title,
								   const char* prompt,
								   const char* default_value);
	int (*ui_confirm_dialog)(void* mh, const char* title, const char* message);

	/* S14 — Utility */
	const char* (*get_app_version)(void* mh);
	const char* (*get_app_name)(void* mh);
	int64_t (*get_timestamp)(void* mh);

	/* S15 — Launch Modifiers (only valid inside INSTANCE_PRE_LAUNCH hooks) */
	int (*launch_set_env)(void* mh, const char* key, const char* value);
	int (*launch_prepend_wrapper)(void* mh, const char* wrapper_cmd);

	/* S16 — Application Settings (read-only global settings) */
	const char* (*app_setting_get)(void* mh, const char* key);

	/* S17 — News API */
	int (*news_get_entry_count)(void* mh);
	const char* (*news_get_entry_title)(void* mh, int index);
	const char* (*news_get_entry_link)(void* mh, int index);
	const char* (*news_get_entry_content)(void* mh, int index);
	const char* (*news_get_entry_author)(void* mh, int index);
	const char* (*news_get_entry_date)(void* mh, int index);
	int (*news_get_entry_feed_index)(void* mh, int index);
	int (*news_add_feed_url)(void* mh, const char* url);
	int (*news_get_feed_count)(void* mh);
	const char* (*news_get_feed_url)(void* mh, int index);
	int (*news_reload)(void* mh);

	/* S18 — Plugin icon set */

	/* Resolves a logical icon name from the module's bundled icon set to a Qt resource path (e.g.
	 * ":/plugins/MyPlugin/foo"). Returns nullptr if the module declared no icon_set_resource or the
	 * icon does not exist. */
	const char* (*ui_plugin_icon)(void* mh, const char* name);

	/* S19 — System Tray (additive; no ABI bump) */
	void* (*tray_create)(void* mh, const char* icon_name, const char* tooltip);
	int (*tray_destroy)(void* mh, void* tray_handle);
	int (*tray_is_available)(void* mh);
	int (*tray_set_icon)(void* mh, void* tray_handle, const char* icon_name);
	int (*tray_set_tooltip)(void* mh, void* tray_handle, const char* tooltip);
	int (*tray_set_visible)(void* mh, void* tray_handle, int visible);
	/* icon_type: 0=None, 1=Info, 2=Warning, 3=Critical. A nullptr tray_handle uses a transient
	 * hidden tray. */
	int (*tray_show_message)(void* mh, void* tray_handle, const char* title,
							 const char* message, int icon_type, int msecs);
	/* Attaches a declarative menu shown on right-click (ABI 5). json_menu_doc is an "mmco-ui/1" tree
	 * whose root's children are `button` (item), `separator` or `section` (submenu). Clicking an item
	 * calls cb with event="click" and node_id = the item's id. nullptr detaches the menu; call again
	 * with a new document to rebuild it. */
	int (*tray_set_menu)(void* mh, void* tray_handle, const char* json_menu_doc,
						 MMCOUiEventCallback cb, void* user_data);
	int (*tray_set_activation_cb)(void* mh, void* tray_handle,
								  MMCOTrayActivationCallback cb, void* ud);

	/* S20 — Main window helpers (additive) */
	int (*main_window_install_close_filter)(void* mh,
											MMCOMainWindowCloseCallback cb,
											void* user_data);
	int (*main_window_show)(void* mh);
	int (*main_window_hide)(void* mh);
	int (*main_window_is_visible)(void* mh);

	/* S21 Application-scope settings (ABI 3+). Keys are NOT auto-namespaced. */
	int (*app_setting_set)(void* mh, const char* key, const char* value);
	int (*app_setting_register)(void* mh, const char* key,
								const char* default_value);
	int (*app_setting_contains)(void* mh, const char* key);

	/* S22 Themed icon resolution (ABI 3+). Returns a string usable in the icon-name parameters of
	 * the UI/tray/menu APIs. */
	const char* (*ui_themed_icon)(void* mh, const char* name);

	/* S23 Instance running-state bridge (ABI 3+). */
	int (*instance_running_register)(void* mh, const char* instance_id,
									 MMCOInstanceRunningCallback cb, void* ud);
	int (*instance_running_unregister)(void* mh, const char* instance_id);

	/* S24 Per-instance settings (ABI 3+). Values are UTF-8 strings; 0 on success, -1 on failure. */
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

	/* S25 Account / skin / cape access (ABI 3+). */
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

	/* S26 Synchronous task helpers (ABI 3+): modal progress dialogs for skin/cape network
	 * operations. */
	int (*account_skin_upload)(void* mh, const char* account_id,
							   const void* png_bytes, int64_t size,
							   const char* variant);
	int (*account_skin_reset)(void* mh, const char* account_id);
	int (*account_cape_set)(void* mh, const char* account_id,
							const char* cape_id);

	/* S27 Icon list enumeration (ABI 3+). */
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

	/* S29 Component version write (ABI 3+). Sets the version of the PackProfile component `uid`, appending
	 * the component if the instance lacks it. uids: "net.minecraft", "net.minecraftforge",
	 * "net.fabricmc.fabric-loader", "net.neoforged", "org.quiltmc.quilt-loader".
	 * Returns 0 on success, -1 on failure. Thread: GUI thread only. */
	int (*instance_component_set_version)(void* mh, const char* id,
										  const char* uid, const char* version);

	/* S30 http_get with custom headers (ABI 3+). `headers` holds `header_count` NUL-terminated
	 * "Name: Value" strings. User-Agent is always set by the launcher. Returns 0 if queued, -1 on argument
	 * errors. Body lifetime as in http_get. */
	int (*http_get_with_headers)(void* mh, const char* url,
								 const char* const* headers, int header_count,
								 MMCOHttpCallback callback, void* user_data);

	/* S31 Subprocess execution (no ABI bump). Runs a program synchronously through QProcess and
	 * captures its stdout.
	 *   program        executable name (resolved on PATH) or absolute path; not run through a shell,
	 *                  each argv element is passed verbatim
	 *   args           `arg_count` NUL-terminated strings (argv[1..]); may be NULL if arg_count is 0
	 *   working_dir    directory to run in, or NULL
	 *   stdin_data     bytes for the child's stdin, or NULL; stdin_size is their length
	 *   out_buf        caller buffer for stdout (NUL-terminated, truncated to fit), NULL discards;
	 *                  out_buf_size includes the NUL
	 *   out_exit_code  receives the exit code, or NULL
	 *   timeout_ms     hard timeout, the child is killed; <= 0 means 30 s
	 * Returns 0 if the process ran to completion (see out_exit_code), -1 if it failed to start,
	 * -2 on timeout, -3 for invalid arguments. */
	int (*process_run)(void* mh, const char* program, const char* const* args,
					   int arg_count, const char* working_dir,
					   const char* stdin_data, int stdin_size, char* out_buf,
					   int out_buf_size, int* out_exit_code, int timeout_ms);

	/* S32 Background hooks + progress reporting (ABI stays at 3). hook_register_ex is hook_register
	 * plus a flags word (MMCO_HOOK_FLAG_* above); 0 behaves like hook_register.
	 * progress_report publishes the running background callback's progress into the progress dialog:
	 *   handle           the module_handle the callback was invoked with
	 *   status           row title, NULL keeps the current one
	 *   details          second line, NULL keeps the current one
	 *   current / total  work done / total; total <= 0 renders an indeterminate bar
	 * Returns 0 if queued, -1 on bad arguments or outside a background hook callback. The update is
	 * marshalled onto the GUI thread, so a tight loop is fine. */
	int (*hook_register_ex)(void* mh, uint32_t hook_id,
							MMCOHookCallback callback, void* user_data,
							uint32_t flags);
	int (*progress_report)(void* handle, const char* status,
						   const char* details, int64_t current,
						   int64_t total);

	/* S33 Declarative UI surfaces (ABI 5). A plugin describes a widget tree as an "mmco-ui/1" JSON
	 * document; the host renders and owns the widgets, no QWidget* is handed back.
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
} MMCOContext;

/* Linkage helper for the exported module-info / entry-point symbols: C linkage in C++. */
#ifdef __cplusplus
#define MMCO_EXTERN_C extern "C"
#else
#define MMCO_EXTERN_C
#endif

/* NULL-pointer literal usable from both C and C++ inside the
 * MMCO_DEFINE_MODULE expansions. */
#ifdef __cplusplus
#define MMCO_NULL nullptr
#else
#define MMCO_NULL NULL
#endif

/* MMCO_DEFINE_MODULE emits mmco_module_info. Arguments (5 to 7): name, version, author, description,
 * license (SPDX id), code_link (optional, nullptr to skip), icon_set_resource (optional, see
 * MMCOModuleInfo::icon_set_resource). Use MMCO_DEFINE_MODULE_EX for dependencies or a signing key. */

#define MMCO_DEFINE_MODULE_7(mod_name, mod_version, mod_author, mod_desc,      \
							 mod_license, mod_code_link, mod_icon_set)         \
	MMCO_EXTERN_C MMCO_EXPORT MMCOModuleInfo mmco_module_info = {              \
		MMCO_MAGIC,	   MMCO_ABI_VERSION, mod_name,	  mod_version,             \
		mod_author,	   mod_desc,		 mod_license, MMCO_FLAG_NONE,          \
		mod_code_link, mod_icon_set,	 MMCO_NULL,	  0u,                      \
		MMCO_NULL}

#define MMCO_DEFINE_MODULE_6(mod_name, mod_version, mod_author, mod_desc,      \
							 mod_license, mod_code_link)                       \
	MMCO_EXTERN_C MMCO_EXPORT MMCOModuleInfo mmco_module_info = {              \
		MMCO_MAGIC,	   MMCO_ABI_VERSION, mod_name,	  mod_version,             \
		mod_author,	   mod_desc,		 mod_license, MMCO_FLAG_NONE,          \
		mod_code_link, MMCO_NULL,		 MMCO_NULL,	  0u,                      \
		MMCO_NULL}

#define MMCO_DEFINE_MODULE_5(mod_name, mod_version, mod_author, mod_desc,      \
							 mod_license)                                      \
	MMCO_EXTERN_C MMCO_EXPORT MMCOModuleInfo mmco_module_info = {              \
		MMCO_MAGIC, MMCO_ABI_VERSION, mod_name,	   mod_version,                \
		mod_author, mod_desc,		  mod_license, MMCO_FLAG_NONE,             \
		MMCO_NULL,	MMCO_NULL,		  MMCO_NULL,   0u,                         \
		MMCO_NULL}

/* Full variant with an icon set, a dependency table and a signing-key id; pass nullptr / 0 for unused fields.
 *   MMCO_DEFINE_MODULE_EX("MyMod", "1.0", "Me", "desc", "MIT", "https://example.org", "my_icons",
 *                         my_deps_array, 2, "ABCD1234..."); */
#define MMCO_DEFINE_MODULE_EX(mod_name, mod_version, mod_author, mod_desc,     \
							  mod_license, mod_code_link, mod_icon_set,        \
							  mod_deps_ptr, mod_deps_count, mod_signing_key)   \
	MMCO_EXTERN_C MMCO_EXPORT MMCOModuleInfo mmco_module_info = {              \
		MMCO_MAGIC,		MMCO_ABI_VERSION, mod_name,		mod_version,           \
		mod_author,		mod_desc,		  mod_license,	MMCO_FLAG_NONE,        \
		mod_code_link,	mod_icon_set,	  mod_deps_ptr, mod_deps_count,        \
		mod_signing_key}

#define MMCO_EXPAND(x) x
#define MMCO_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define MMCO_DEFINE_MODULE(...)                                                \
	MMCO_EXPAND(MMCO_GET_MACRO(__VA_ARGS__, MMCO_DEFINE_MODULE_7,              \
							   MMCO_DEFINE_MODULE_6,                           \
							   MMCO_DEFINE_MODULE_5)(__VA_ARGS__))

#define MMCO_LOG(ctx, msg) (ctx)->log_info((ctx)->module_handle, (msg))
#define MMCO_WARN(ctx, msg) (ctx)->log_warn((ctx)->module_handle, (msg))
#define MMCO_ERR(ctx, msg) (ctx)->log_error((ctx)->module_handle, (msg))
#define MMCO_DBG(ctx, msg) (ctx)->log_debug((ctx)->module_handle, (msg))

/* Shorthand to call API functions with module handle */
#define MMCO_MH (ctx->module_handle)

#ifdef __cplusplus
} /* extern "C" */
#endif
