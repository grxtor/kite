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

#include <cstdint>

/*
 * Hook points where plugins can intercept or extend MeshMC behaviour.
 *
 * Hooks follow the observer pattern: plugins register callbacks for
 * specific hook IDs, and MeshMC dispatches to all registered callbacks
 * when the corresponding event occurs.
 *
 * Hook callbacks receive a generic void* payload whose concrete type
 * depends on the hook ID (documented below).
 */

enum MMCOHookId : uint32_t {
	/* Application lifecycle */
	MMCO_HOOK_APP_INITIALIZED = 0x0100, /* payload: nullptr */
	MMCO_HOOK_APP_SHUTDOWN = 0x0101,	/* payload: nullptr */

	/* Instance lifecycle */
	MMCO_HOOK_INSTANCE_PRE_LAUNCH = 0x0200,	 /* payload: MMCOInstanceInfo* */
	MMCO_HOOK_INSTANCE_POST_LAUNCH = 0x0201, /* payload: MMCOInstanceInfo* */
	MMCO_HOOK_INSTANCE_CREATED = 0x0202,	 /* payload: MMCOInstanceInfo* */
	MMCO_HOOK_INSTANCE_REMOVED = 0x0203,	 /* payload: MMCOInstanceInfo* */

	/* Settings */
	MMCO_HOOK_SETTINGS_CHANGED = 0x0300, /* payload: MMCOSettingChange* */

	/* Content / mod management */
	MMCO_HOOK_CONTENT_PRE_DOWNLOAD = 0x0400,  /* payload: MMCOContentEvent* */
	MMCO_HOOK_CONTENT_POST_DOWNLOAD = 0x0401, /* payload: MMCOContentEvent* */

	/* Network */
	MMCO_HOOK_NETWORK_PRE_REQUEST = 0x0500,	 /* payload: MMCONetworkEvent* */
	MMCO_HOOK_NETWORK_POST_REQUEST = 0x0501, /* payload: MMCONetworkEvent* */

	/* UI extension points */
	MMCO_HOOK_UI_MAIN_READY = 0x0600,	  /* payload: MMCOUiMainReadyPayload* */
	MMCO_HOOK_UI_CONTEXT_MENU = 0x0601,	  /* payload: MMCOMenuEvent* */
	MMCO_HOOK_UI_INSTANCE_PAGES = 0x0602, /* payload: MMCOInstancePagesEvent* */
	MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES =
		0x0603, /* payload: MMCOGlobalSettingsPagesEvent* */

	/* ABI 3+ — global-settings dialog lifecycle.
	 *
	 * Fires just before the Application's global settings dialog is
	 * built. Plugins that mutate the dialog's pages on every open
	 * (e.g. to inject a checkbox into the MeshMCPage) hook here and
	 * schedule a deferred QTimer::singleShot(0, …) to do the actual
	 * widget walk on the next event-loop turn (after the dialog's
	 * own setupUi has run).  Payload: nullptr. */
	MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN = 0x0604,

	/* ABI 3+ — per-instance settings page just constructed.
	 *
	 * Fires from InstanceSettingsPage's ctor right after its UI is
	 * assembled. Replaces the legacy
	 *   QObject::connect(APPLICATION,
	 *                    &Application::instanceSettingsPageCreated, …)
	 * dance.
	 *
	 * Payload: MMCOInstanceSettingsPageEvent* — see below. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED = 0x0605,

	/* ABI 3+ — per-instance settings page finished loading values from
	 * its backing store. Plugins that mirror values out of the
	 * instance into their own widgets hook here to refresh.
	 * Payload: MMCOInstanceSettingsPageEvent* (same fields as 0x0605). */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED = 0x0606,

	/* ABI 3+ — per-instance settings page about to write back its values.
	 * Plugins that own custom widgets on the page hook here to push
	 * the user's choice back into the instance settings before the
	 * page commits.
	 * Payload: MMCOInstanceSettingsPageEvent*. */
	MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING = 0x0607,

	/* News */
	MMCO_HOOK_NEWS_UPDATED =
		0x0700, /* payload: nullptr — fires after feeds reload */

	/* Authentication — plugin-driven auth-provider extension points.
	 *
	 * These two hooks are the foundation of third-party authentication
	 * support (Drasl, Ely.by, LittleSkin, custom Yggdrasil servers,
	 * anything authlib-injector-compatible).  By intercepting auth
	 * traffic at the request layer AND injecting session fields at the
	 * launch layer, a plugin can fully replace the Mojang flow without
	 * touching launcher code. */

	/* MMCO_HOOK_AUTH_REQUEST
	 *
	 * Fires from AuthRequest::setup() just before the network request
	 * is dispatched.  The payload is an MMCOAuthRequestEvent which
	 * exposes the in-flight QNetworkRequest's URL, HTTP method, and
	 * a mutable redirect slot.
	 *
	 * Callbacks may:
	 *   • Read payload->url and payload->method to identify the call.
	 *   • Set payload->redirect_url to non-null to rewrite the URL
	 *     before the request is sent (e.g. swap api.minecraftservices.com
	 *     for authserver.ely.by).
	 *   • Append HTTP headers via payload->add_header(key, value).
	 *
	 * Returning non-zero cancels the request entirely; the AuthRequest
	 * emits ProtocolUnknownError and the calling AuthStep fails. */
	MMCO_HOOK_AUTH_REQUEST = 0x0800,

	/* MMCO_HOOK_SESSION_FILL
	 *
	 * Fires from LaunchController::login() immediately after
	 * MinecraftAccount::fillSession() has populated the AuthSession
	 * with the host's default account data, and before the launch
	 * task is built.  Plugins can overwrite any field — access_token,
	 * uuid, player_name, user_type, session, or append user_properties
	 * — to inject a non-Mojang session into the JVM launch.
	 *
	 * Payload: MMCOSessionFillEvent*.  Return value is ignored
	 * (mutations always apply). */
	MMCO_HOOK_SESSION_FILL = 0x0801,
};

/*
 * Hook callback signature.
 *   module_handle: opaque handle identifying the calling module
 *   hook_id:       which hook fired
 *   payload:       hook-specific data (may be nullptr)
 *   user_data:     arbitrary pointer the plugin passed at registration
 *
 * Return 0 to allow the chain to continue, non-zero to signal cancellation
 * (only effective for "pre" hooks).
 */
typedef int (*MMCOHookCallback)(void* module_handle, uint32_t hook_id,
								void* payload, void* user_data);

/*
 * Flags for MMCOContext::hook_register_ex.
 *
 * MMCO_HOOK_FLAG_BACKGROUND — run this callback on a worker thread
 * instead of on the thread that fired the hook. The host wraps every
 * background callback of a dispatch into its own task and shows it as
 * a separate row in the launcher's progress dialog, so a slow callback
 * (archiving an instance, shelling out to git, hashing files) no
 * longer freezes the window.
 *
 * A background callback MUST NOT touch any ui_*, tray_* or
 * main_window_* function, must not create or mutate Qt widgets, and
 * must not assume anything about its thread beyond "not the GUI
 * thread". It MAY use log_*, fs_*, zip_*, process_run and
 * progress_report.
 *
 * Everything else — the settings getters and setters, and every
 * instance / account / mod / world accessor — reads launcher state
 * that only the GUI thread owns. Register a second, inline callback
 * for the same hook, read what you need there, and hand it to the
 * background one: the host runs all inline callbacks of a dispatch
 * before any background one.
 *
 * Dispatch still waits for every background callback to finish before
 * it returns, so the ordering guarantees callers rely on (e.g.
 * LaunchController reading pending env right after PRE_LAUNCH) are
 * unchanged.
 *
 * Omitting the flag — or using the plain hook_register — keeps the
 * legacy behaviour: the callback runs inline, blocking the caller.
 */
#define MMCO_HOOK_FLAG_NONE 0u
#define MMCO_HOOK_FLAG_BACKGROUND 1u

/* Payload structures for hooks */

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

/*
 * Payload for MMCO_HOOK_UI_INSTANCE_PAGES.
 * Plugins receive this when instance page dialogs are being built.
 * They can add pages via the page_list_handle (opaque pointer to
 * QList<BasePage*>*).
 */
struct MMCOInstancePagesEvent {
	const char* instance_id;
	const char* instance_name;
	const char* instance_path;
	void* page_list_handle; /* Opaque: QList<BasePage*>* */
	void* instance_handle;	/* Opaque: InstancePtr raw pointer */
};

/*
 * Payload for MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED (ABI 3+).
 *
 * Fires from InstanceSettingsPage's ctor right after its UI is
 * assembled. Lets plugins inject extra widgets into the launcher's
 * built-in per-instance settings page without subclassing.
 *
 *   instance_id     — internal id of the instance the page belongs to.
 *   page_handle     — opaque pointer to the just-built page widget
 *                     (a QWidget*). Plugins may qobject_cast<QWidget*>
 *                     this handle (Qt operation, allowed) but MUST
 *                     NOT cast it to InstanceSettingsPage* — that
 *                     would require including a launcher header and
 *                     break standalone builds.
 *   instance_handle — opaque pointer to the BaseInstance the page is
 *                     editing. Same casting rules apply.
 *
 * Handles are valid for the lifetime of the settings dialog; plugins
 * must NOT delete them. Returning non-zero from the callback has no
 * effect — this is a notification, not a veto point.
 */
struct MMCOInstanceSettingsPageEvent {
	const char* instance_id;
	void* page_handle;
	void* instance_handle;
};

/*
 * MMCOInstanceRunningCallback — ABI 3+ replacement for the legacy
 * pattern of grabbing an InstancePtr via
 *   APPLICATION->instances()->getInstanceById(id)
 * and connecting to BaseInstance::runningStatusChanged.
 *
 * Registered through MMCOContext::instance_running_register; the host
 * tracks every registration per-module and severs them on mmco_unload
 * so a stale callback can never fire into freed plugin memory.
 *
 *   user_data    — the cookie the plugin passed at registration time.
 *   instance_id  — the id of the instance whose state changed.
 *   running      — 1 if the instance just started running, 0 if it
 *                  just stopped.
 */
typedef void (*MMCOInstanceRunningCallback)(void* user_data,
											const char* instance_id,
											int running);

/*
 * Payload for MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES.
 * Plugins receive this when the global settings dialog is being built.
 * They can add pages via the page_list_handle (opaque pointer to
 * QList<BasePage*>*).
 */
struct MMCOGlobalSettingsPagesEvent {
	void* page_list_handle; /* Opaque: QList<BasePage*>* */
};

/*
 * Payload for MMCO_HOOK_UI_MAIN_READY.
 *
 * Fired once after MainWindow has finished assembling its top-level
 * widgets (toolbars, news label, status bar). Plugins that need to hook
 * into the main UI receive direct opaque handles to the long-lived
 * widgets here, so they no longer need to walk `qApp->allWidgets()`.
 *
 * Handles are valid for the lifetime of the main window. Plugins should
 * NOT take ownership of them or delete them. Cast them through Qt's
 * normal qobject_cast<>() to the documented concrete type.
 *
 * Under the QML shell (MESHMC_QML_UI / MeshMC_QML_UI) there is no
 * MainWindow and none of these widgets exist: the hook still fires once,
 * right after the shell's root window is shown, but every field below is
 * null. Use main_window_show/hide/is_visible and
 * main_window_install_close_filter instead, which work under either UI.
 */
struct MMCOUiMainReadyPayload {
	void* main_window;		 /* Opaque: QMainWindow* (MainWindow*); null under the QML shell */
	void* news_toolbar;		 /* Opaque: QToolBar*; null under the QML shell */
	void* more_news_action;	 /* Opaque: QAction*; null under the QML shell */
	void* news_label_button; /* Opaque: QToolButton*; null under the QML shell */
};

/*
 * Payload for MMCO_HOOK_AUTH_REQUEST.
 *
 * Plugins receive this just before AuthRequest dispatches a network
 * call. The fields under "Read-only" describe the request as it stands;
 * the fields under "Mutable" let the plugin redirect or augment it.
 *
 * Header append is mediated through a function pointer rather than a
 * raw vector so the host can do bookkeeping (lifetime, encoding) and
 * the plugin never touches QByteArray internals.  Pass `add_header`
 * the request_handle below and a UTF-8 key/value pair.
 *
 * All string pointers are owned by the host and valid for the duration
 * of the callback only — copy them if you need to keep them.
 */
struct MMCOAuthRequestEvent {
	/* ── Read-only request snapshot ─────────────────────────────── */
	const char* url;	/* Effective URL of the in-flight request    */
	const char* method; /* "GET", "POST", "DELETE", "PUT", …         */
	const char* body;	/* POST body (UTF-8 if textual; may be raw   *
						 * bytes — see body_size). NULL for GETs.    */
	int body_size;		/* Length of body in bytes. 0 for GETs.      */

	/* ── Mutable response slots ─────────────────────────────────── */

	/* Set to a non-null UTF-8 string to rewrite the request URL
	 * before it is sent. NULL means "no redirect". The host copies
	 * the value; you may free or reuse the memory after this call. */
	const char* redirect_url;

	/* ── Header injection helper ────────────────────────────────── */

	/* Opaque handle the plugin passes to add_header(). The host
	 * uses this to identify which in-flight request to mutate. */
	void* request_handle;

	/* Add an HTTP header to the in-flight request.  key and value
	 * must be UTF-8 NUL-terminated. Returns 0 on success. The host
	 * copies both strings internally. */
	int (*add_header)(void* request_handle, const char* key, const char* value);
};

/*
 * Payload for MMCO_HOOK_SESSION_FILL.
 *
 * Plugins receive this after the host has populated an AuthSession
 * with its default account view and before the launch task is built.
 * Any non-null overwrite_* string replaces the corresponding session
 * field. Passing NULL leaves the field as the host's default. The
 * host copies every string it consumes; the plugin retains ownership.
 *
 * Use cases:
 *   • Inject a Yggdrasil-style access token from a custom auth server
 *     (set overwrite_access_token + overwrite_uuid + overwrite_user_type
 *     to "mojang").
 *   • Replace the player_name to match an alias used by the auth
 *     provider.
 *   • Append user_properties (textures, custom claims) for
 *     authlib-injector-style providers.
 *
 * The account_id and account_is_msa fields are read-only and let the
 * plugin decide whether this hook should act (e.g. only override for
 * MSA accounts that were explicitly marked as "auth-redirected" by a
 * companion settings page).
 */
struct MMCOSessionFillEvent {
	/* ── Read-only context ──────────────────────────────────────── */
	const char* account_id; /* internal id of the resolved account   */
	int account_is_msa;		/* 1 if MSA, 0 if offline                */
	int wants_online;		/* 1 if user requested online launch     */

	/* ── Read-only current session view (host defaults) ─────────── */
	const char* current_player_name;
	const char* current_uuid;
	const char* current_user_type;

	/* ── Mutable overwrites (all optional; NULL = no change) ───── */
	const char* overwrite_access_token;
	const char* overwrite_session; /* "token:<at>:<uuid>" format    */
	const char* overwrite_player_name;
	const char* overwrite_uuid;
	const char* overwrite_user_type; /* "mojang" / "legacy" / custom  */
	const char* overwrite_client_token;

	/* ── User-properties append ─────────────────────────────────── *
	 * Comma-separated key=value pairs, or a JSON object literal —
	 * the host appends them to AuthSession::user_properties verbatim.
	 * NULL means "no properties to add". */
	const char* extra_user_properties;
};
