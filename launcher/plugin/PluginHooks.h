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

#include <cstdint>

/* Hook points plugins can register callbacks for. The void* payload type depends on the hook ID. */

enum MMCOHookId : uint32_t {
	/* Application lifecycle */
	MMCO_HOOK_APP_INITIALIZED = 0x0100, /* payload: nullptr */
	MMCO_HOOK_APP_SHUTDOWN = 0x0101,	/* payload: nullptr */

	/* Instance lifecycle */
	MMCO_HOOK_INSTANCE_PRE_LAUNCH = 0x0200,	 /* payload: MMCOInstanceInfo*; just before the game spawns */
	MMCO_HOOK_INSTANCE_POST_LAUNCH = 0x0201, /* payload: MMCOInstanceInfo*; after the game has EXITED */
	MMCO_HOOK_INSTANCE_CREATED = 0x0202,	 /* payload: MMCOInstanceInfo*; only after UI_MAIN_READY, not for instances already present */
	MMCO_HOOK_INSTANCE_REMOVED = 0x0203,	 /* payload: MMCOInstanceInfo*; after UI_MAIN_READY, fired once the instance has left the list */

	/* Settings */
	MMCO_HOOK_SETTINGS_CHANGED = 0x0300, /* payload: MMCOSettingChange* */

	/* Content / mod management */
	MMCO_HOOK_CONTENT_PRE_DOWNLOAD = 0x0400,  /* payload: MMCOContentEvent* */
	MMCO_HOOK_CONTENT_POST_DOWNLOAD = 0x0401, /* payload: MMCOContentEvent* */

	/* Network */
	MMCO_HOOK_NETWORK_PRE_REQUEST = 0x0500,	 /* payload: MMCONetworkEvent* */
	MMCO_HOOK_NETWORK_POST_REQUEST = 0x0501, /* payload: MMCONetworkEvent* */

	/* UI extension points */
	MMCO_HOOK_UI_MAIN_READY = 0x0600, /* payload: MMCOUiMainReadyPayload*; fires once, when the
										 main window is first on screen */

	/* 0x0601-0x0607 are reserved: nothing dispatches them since the widget UI was removed, so a
	 * callback registered for one is never called. The declarations stay for ABI compatibility.
	 * Use ui_surface_create for pages and settings sections instead. */
	MMCO_HOOK_UI_CONTEXT_MENU = 0x0601,	  /* reserved, never fired; payload: MMCOMenuEvent* */
	MMCO_HOOK_UI_INSTANCE_PAGES = 0x0602, /* reserved, never fired; payload: MMCOInstancePagesEvent* */
	MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES =
		0x0603, /* reserved, never fired; payload: MMCOGlobalSettingsPagesEvent* */

	/* ABI 3+. Reserved, never fired (it was the global settings dialog about to be built). */
	MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN = 0x0604,

	/* ABI 3+. Reserved, never fired. Was: the instance settings page assembled. Payload:
	 * MMCOInstanceSettingsPageEvent*. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED = 0x0605,

	/* ABI 3+. Reserved, never fired. Was: the instance settings page loaded its values. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED = 0x0606,

	/* ABI 3+. Reserved, never fired. Was: the instance settings page about to write its values
	 * back. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING = 0x0607,

	/* News */
	MMCO_HOOK_NEWS_UPDATED =
		0x0700, /* payload: nullptr — fires after feeds reload */

	/* Authentication */

	/* MMCO_HOOK_AUTH_REQUEST: fires from AuthRequest::setup() just before the request is sent.
	 * Payload: MMCOAuthRequestEvent*. Callbacks can read url/method, set redirect_url to rewrite
	 * the URL, and add headers with add_header(). Returning non-zero cancels the request
	 * (AuthRequest emits ProtocolUnknownError and the AuthStep fails). */
	MMCO_HOOK_AUTH_REQUEST = 0x0800,

	/* MMCO_HOOK_SESSION_FILL: fires from LaunchController::login() after
	 * MinecraftAccount::fillSession() populated the AuthSession and before the launch task is
	 * built. Payload: MMCOSessionFillEvent*. The return value is ignored; mutations always apply. */
	MMCO_HOOK_SESSION_FILL = 0x0801,

	/* Friends */

	/* MMCO_HOOK_FRIENDS_NOTICE: something about a friend worth a desktop notification, once per
	 * event (a friend came online, a friend request arrived, while the launcher's
	 * FriendsNotifications setting is on; a chat message that is not on screen, while
	 * FriendsMessageNotifications is). Payload: MMCOFriendsNotice*. The return value is
	 * ignored. */
	MMCO_HOOK_FRIENDS_NOTICE = 0x0900,
};

/* Hook callback signature.
 *   module_handle  opaque handle identifying the calling module
 *   hook_id        which hook fired
 *   payload        hook-specific data (may be nullptr)
 *   user_data      pointer the plugin passed at registration
 * Return 0 to continue the chain, non-zero to cancel (only effective for "pre" hooks). */
typedef int (*MMCOHookCallback)(void* module_handle, uint32_t hook_id,
								void* payload, void* user_data);

/* Flags for MMCOContext::hook_register_ex. 0 behaves like hook_register: the callback runs inline.
 *
 * MMCO_HOOK_FLAG_BACKGROUND runs the callback on a worker thread, behind a single "Waiting for plugins"
 * busy indication (there is no per-plugin progress row). It must not touch ui_*, tray_* or main_window_* functions or Qt widgets, and may assume only
 * that it is not on the GUI thread. It may use log_*, fs_*, zip_*, process_run and progress_report.
 * Settings and instance/account/mod/world accessors read GUI-thread state: read what you need in a
 * second, inline callback for the same hook (a dispatch runs all inline callbacks before any
 * background one) and hand it over. Dispatch waits for every background callback before returning. */
#define MMCO_HOOK_FLAG_NONE 0u
#define MMCO_HOOK_FLAG_BACKGROUND 1u

struct MMCOInstanceInfo {
	const char* instance_id;
	const char* instance_name;
	const char* instance_path;
	const char* minecraft_version;
};

struct MMCOSettingChange {
	const char* key;
	const char* old_value;
	const char* new_value;
};

struct MMCOContentEvent {
	const char* instance_id;
	const char* file_name;
	const char* url;
	const char* target_path;
};

struct MMCONetworkEvent {
	const char* url;
	const char* method; /* "GET", "POST", etc. */
	int status_code;	/* 0 for pre-request */
};

struct MMCOMenuEvent {
	const char* context; /* "main", "instance", etc. */
	void* menu_handle;	 /* Opaque handle for mmco_ui_add_menu_item() */
};

/* Payload for MMCO_HOOK_UI_INSTANCE_PAGES (reserved, never fired). */
struct MMCOInstancePagesEvent {
	const char* instance_id;
	const char* instance_name;
	const char* instance_path;
	void* page_list_handle; /* Opaque: QList<BasePage*>* */
	void* instance_handle;	/* Opaque: InstancePtr raw pointer */
};

/* Payload for MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED / _LOADED / _APPLYING (ABI 3+); the hooks are
 * reserved and never fired, the struct stays for ABI compatibility.
 *   instance_id      internal id of the instance the page belongs to
 *   page_handle      the page widget (QWidget*); qobject_cast<QWidget*> is fine, but do not cast it to
 *                    InstanceSettingsPage* (that needs a launcher header)
 *   instance_handle  the BaseInstance being edited; same casting rules
 * Callback return values are ignored: these are notifications, not veto points. */
struct MMCOInstanceSettingsPageEvent {
	const char* instance_id;
	void* page_handle;
	void* instance_handle;
};

/* Running-state callback for MMCOContext::instance_running_register (ABI 3+). The host severs
 * registrations on mmco_unload.
 *   user_data    the cookie passed at registration
 *   instance_id  the instance whose state changed
 *   running      1 if it just started running, 0 if it just stopped */
typedef void (*MMCOInstanceRunningCallback)(void* user_data,
											const char* instance_id,
											int running);

/* Payload for MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES (reserved, never fired). */
struct MMCOGlobalSettingsPagesEvent {
	void* page_list_handle; /* Opaque: QList<BasePage*>* */
};

/* Payload for MMCO_HOOK_UI_MAIN_READY: fires once, the first time the root window is on screen. The
 * fields were the widget main window's handles and are always null now; they stay for ABI
 * compatibility. Use main_window_show/hide/is_visible and main_window_install_close_filter. */
struct MMCOUiMainReadyPayload {
	void* main_window;		 /* always null */
	void* news_toolbar;		 /* always null */
	void* more_news_action;	 /* always null */
	void* news_label_button; /* always null */
};

/* Payload for MMCO_HOOK_AUTH_REQUEST, delivered just before AuthRequest dispatches a network call.
 * String pointers are owned by the host and valid only during the callback; copy what you keep. */
struct MMCOAuthRequestEvent {
	/* Read-only request snapshot */
	const char* url;	/* Effective URL */
	const char* method; /* "GET", "POST", "DELETE", "PUT", ... */
	const char* body;	/* POST body, may be raw bytes (see body_size); NULL for GETs */
	int body_size;		/* 0 for GETs */

	/* Mutable response slots */

	/* Non-null rewrites the request URL before it is sent; NULL means no redirect. The host copies
	 * the value. */
	const char* redirect_url;

	/* Opaque handle to pass to add_header(). */
	void* request_handle;

	/* Adds an HTTP header (UTF-8, NUL-terminated key and value; the host copies both). Returns 0 on
	 * success. */
	int (*add_header)(void* request_handle, const char* key, const char* value);
};

/* Payload for MMCO_HOOK_SESSION_FILL. A non-null overwrite_* string replaces the matching session
 * field; NULL keeps the host default. The host copies every string it consumes; the plugin keeps
 * ownership. account_id and account_is_msa are read-only, so a plugin can decide whether to act. */
struct MMCOSessionFillEvent {
	/* Read-only context */
	const char* account_id; /* internal id of the resolved account   */
	int account_is_msa;		/* 1 if MSA, 0 if offline                */
	int wants_online;		/* 1 if user requested online launch     */

	/* Read-only current session view (host defaults) */
	const char* current_player_name;
	const char* current_uuid;
	const char* current_user_type;

	/* Mutable overwrites (all optional; NULL = no change) */
	const char* overwrite_access_token;
	const char* overwrite_session; /* "token:<at>:<uuid>" format    */
	const char* overwrite_player_name;
	const char* overwrite_uuid;
	const char* overwrite_user_type; /* "mojang" / "legacy" / custom  */
	const char* overwrite_client_token;

	/* Comma-separated key=value pairs or a JSON object literal, appended verbatim to
	 * AuthSession::user_properties. NULL adds none. */
	const char* extra_user_properties;
};

/* Payload for MMCO_HOOK_FRIENDS_NOTICE. Strings are UTF-8, owned by the host and valid only during
 * the callback. title and body are worded by Kite in the launcher's language, ready to show. */
struct MMCOFriendsNotice {
	const char* kind;  /* "online" (came online or started playing) | "request" (sent a request) |
						* "message" (sent a chat message that is not on screen; the
						* FriendsMessageNotifications setting) */
	const char* uuid;  /* the friend's Minecraft UUID */
	const char* name;  /* the friend's Minecraft name */
	const char* title; /* "Alex is online"; the sender's name for a message */
	const char* body;  /* "On mc.hypixel.net", or the message's text; never empty */
};
