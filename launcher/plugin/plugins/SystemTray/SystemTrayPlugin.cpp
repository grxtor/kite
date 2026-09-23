/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 *  SystemTray  —  MeshMC MMCO plugin
 *
 *  Adds a persistent QSystemTrayIcon for the launcher with:
 *    • Show / Hide / Quit actions
 *    • A dynamically rebuilt "Launch instance…" submenu
 *      (up to MAX_INSTANCE_ENTRIES recent instances)
 *    • Optional "minimize to tray" close-event filter
 *
 *  Everything is funnelled through the S19/S20/S33 API surfaces exposed
 *  by PluginManager, so this plugin compiles cleanly against the public
 *  SDK and does not poke at MainWindow internals.
 *
 *  Settings (all booleans, stored under the plugin's namespace):
 *    enabled           — master switch                          (default 1)
 *    minimize_to_tray  — swallow window close, hide instead     (default 0)
 *    show_notifications — show a transient hint on first hide   (default 1)
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"

/* qApp (used below to invoke "quit" on the application object) is
 * defined by whichever Q*Application header is included; the SDK
 * header no longer pulls in <QApplication> (ABI 5 dropped Qt::Widgets
 * from MeshMC::SDK), so this plugin includes the QtCore-only
 * QCoreApplication header itself — QMetaObject::invokeMethod only
 * needs a QObject*, which qApp still resolves to either way. */
#include <QCoreApplication>

#include <cstring>

/* ── dependencies ─────────────────────────────────────────────────── *
 *
 * SystemTray depends on DesktopNotifier ≥ 1.0.0.
 *
 * Rationale: SystemTray itself only provides the persistent tray icon
 * and its right-click menu — the actual desktop-notification dispatch
 * (instance launched, news updated, …) lives in DesktopNotifier. By
 * declaring a hard dependency we guarantee:
 *
 *   1. DesktopNotifier is loaded *before* SystemTray (the resolver
 *      runs a Kahn topological sort over the dep graph).
 *   2. If the user removes / disables DesktopNotifier, SystemTray
 *      refuses to load with a clear "Required dependency missing"
 *      reason instead of silently dropping notifications.
 *
 * The dependency is "hard" (optional=0). To make it soft, flip
 * optional to 1 — the resolver will then load SystemTray even when
 * DesktopNotifier is absent, but the load order guarantee still holds
 * whenever both are present. */
static const MMCODependency k_systemTrayDeps[] = {
	{"DesktopNotifier", "1.0.0", 0},
};

MMCO_DEFINE_MODULE_EX(
	"SystemTray", "1.0.0", "Project Tick",
	"System tray icon with quick-launch menu and minimize-to-tray support",
	"Apache-2.0",
	/* code_link        */ nullptr,
	/* icon_set         */ nullptr,
	/* dependencies     */ k_systemTrayDeps,
	/* dependency_count */ 1u,
	/* signing_key_id   */ nullptr);

/* ── module-local state ───────────────────────────────────────────── */

static MMCOContext* g_ctx = nullptr;
static void* g_tray = nullptr;			  /* QSystemTrayIcon*           */
static void* g_settingsSurface = nullptr; /* ABI 5 GLOBAL_SETTINGS surface */
static QObject* g_guard = nullptr; /* anchor for our Qt connections */

/* The launcher-wide setting key we mirror our "enabled" plugin-local
 * setting onto. Lives in APPLICATION->settings() so it's visible in
 * Settings → MeshMC and can be toggled by the user without poking at
 * raw config files. The plugin still treats its own plugin-namespaced
 * "enabled" setting as the runtime source of truth — the global key
 * just drives the checkbox and is mirrored back into the plugin
 * namespace whenever the user flips it. */
static constexpr const char SETTING_GLOBAL_ENABLED[] =
	"plugin.system_tray.Enabled";

static constexpr int MAX_INSTANCE_ENTRIES = 8;

static bool is_flatpak()
{
	return QFile::exists(QStringLiteral("/.flatpak-info"));
}

/* ── settings helpers ─────────────────────────────────────────────── */

static bool settingBool(const char* key, bool fallback)
{
	if (!g_ctx)
		return fallback;
	const char* v = g_ctx->setting_get(g_ctx->module_handle, key);
	if (!v)
		return fallback;
	QString s = QString::fromUtf8(v).trimmed().toLower();
	if (s.isEmpty())
		return fallback;
	return s == "1" || s == "true" || s == "yes" || s == "on";
}

static void settingSetBool(const char* key, bool value)
{
	if (!g_ctx)
		return;
	g_ctx->setting_set(g_ctx->module_handle, key, value ? "1" : "0");
}

/* ── tray menu: build the declarative "mmco-tray-menu/1" doc ─────── *
 *
 * ABI 5 replaces the imperative tray_menu_create/add_action/
 * add_submenu family with one JSON document per tray_set_menu() call.
 * The whole menu — including the "Launch instance" submenu contents —
 * is rebuilt from scratch here and handed to tray_set_menu() again on
 * every INSTANCE_CREATED/REMOVED, exactly like the old
 * rebuild_launch_submenu() did with the imperative API.
 *
 * Each per-instance entry's node id is "launch:<instance_id>", so the
 * single event callback below can recover the instance id directly
 * from node_id — no more separate LaunchUserData/g_launchEntries
 * bookkeeping to keep in sync with the menu's contents. */

static QByteArray build_tray_menu_json()
{
	QJsonObject openItem{{"type", "button"},
						 {"id", "open"},
						 {"props", QJsonObject{{"label", "Open MeshMC"}}}};
	QJsonObject hideItem{{"type", "button"},
						 {"id", "hide"},
						 {"props", QJsonObject{{"label", "Hide window"}}}};
	QJsonObject sep{{"type", "separator"}};

	QJsonArray launchChildren;
	if (g_ctx) {
		const int total = g_ctx->instance_count(g_ctx->module_handle);
		int shown = 0;
		for (int i = 0; i < total && shown < MAX_INSTANCE_ENTRIES; ++i) {
			const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
			if (!id)
				continue;
			const std::string idCopy = id;
			const char* name =
				g_ctx->instance_get_name(g_ctx->module_handle, idCopy.c_str());
			const QString label = name ? QString::fromUtf8(name)
									   : QString::fromStdString(idCopy);
			launchChildren.append(QJsonObject{
				{"type", "button"},
				{"id", QStringLiteral("launch:%1")
						  .arg(QString::fromStdString(idCopy))},
				{"props", QJsonObject{{"label", label}}}});
			++shown;
		}
	}
	if (launchChildren.isEmpty()) {
		launchChildren.append(QJsonObject{
			{"type", "button"},
			{"id", "launch:none"},
			{"props",
			 QJsonObject{{"label", "(no instances)"}, {"enabled", false}}}});
	}
	const QJsonObject launchSection{
		{"type", "section"},
		{"id", "launch"},
		{"props", QJsonObject{{"title", "Launch instance"}}},
		{"children", launchChildren}};

	QJsonObject quitItem{{"type", "button"},
						 {"id", "quit"},
						 {"props", QJsonObject{{"label", "Quit MeshMC"}}}};

	const QJsonArray items{openItem, hideItem, sep, launchSection, sep, quitItem};
	const QJsonObject doc{{"type", "mmco-tray-menu/1"}, {"items", items}};
	return QJsonDocument(doc).toJson(QJsonDocument::Compact);
}

static void rebuild_tray_menu();

/* ── tray menu event callback ─────────────────────────────────────── */

static void on_tray_menu_event(void* /*ud*/, const char* /*surface_id*/,
							   const char* node_id, const char* event,
							   const char* /*value_json*/)
{
	if (!g_ctx || !node_id || !event || std::strcmp(event, "click") != 0)
		return;

	const QString id = QString::fromUtf8(node_id);
	if (id == QLatin1String("open")) {
		g_ctx->main_window_show(g_ctx->module_handle);
	} else if (id == QLatin1String("hide")) {
		g_ctx->main_window_hide(g_ctx->module_handle);
	} else if (id == QLatin1String("quit")) {
		/* QCoreApplication::quit() is the cleanest path — it tears down
		 * the event loop which in turn unwinds MeshMC's Application
		 * shutdown, giving PluginManager a chance to mmco_unload() us
		 * properly. */
		QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
	} else if (id.startsWith(QLatin1String("launch:"))) {
		const QByteArray instId = id.mid(7).toUtf8();
		if (!instId.isEmpty())
			g_ctx->instance_launch(g_ctx->module_handle, instId.constData(),
								   /*online=*/1);
	}
}

static void rebuild_tray_menu()
{
	if (!g_ctx || !g_tray)
		return;
	const QByteArray json = build_tray_menu_json();
	g_ctx->tray_set_menu(g_ctx->module_handle, g_tray, json.constData(),
						 on_tray_menu_event, nullptr);
}

/* ── tray activation: left-click toggles the main window ──────────── */

static void on_tray_activated(void* /*ud*/, int reason)
{
	/* QSystemTrayIcon::ActivationReason values (per Qt 6 docs):
	 *   0 = Unknown
	 *   1 = Context      (right-click / context-menu requested)
	 *   2 = DoubleClick
	 *   3 = Trigger      (single click — left click)
	 *   4 = MiddleClick
	 *
	 * NOTE: these are NOT in numeric "Trigger=1" order — the enum is
	 * { Unknown, Context, DoubleClick, Trigger, MiddleClick }. An earlier
	 * version assumed Trigger==1, so a right-click (Context==1) was
	 * toggling the window. We must only toggle on Trigger (3); the
	 * right-click (Context==1) raises the menu and must never toggle the
	 * window. */
	constexpr int kContext = 1;
	constexpr int kDoubleClick = 2;
	constexpr int kTrigger = 3;

	if (reason == kContext)
		return; /* right-click → menu only, never toggle */

#if defined(_WIN32) || defined(_WIN64)
	/* Windows: single left-click (Trigger) toggles the window. */
	if (reason != kTrigger)
		return;
#else
	/* X11/macOS: single click or double click toggles. */
	if (reason != kTrigger && reason != kDoubleClick)
		return;
#endif
	if (!g_ctx)
		return;
	if (g_ctx->main_window_is_visible(g_ctx->module_handle))
		g_ctx->main_window_hide(g_ctx->module_handle);
	else
		g_ctx->main_window_show(g_ctx->module_handle);
}

/* ── close-filter: swallow QCloseEvent when minimize_to_tray is on ── */

static bool g_hintShown = false;

static int on_main_window_close(void* /*ud*/)
{
	if (!g_ctx)
		return 0;
	if (!settingBool("minimize_to_tray", false))
		return 0;
	/* Show a one-time hint so the user knows the launcher is still
	 * running in the tray. */
	if (!g_hintShown && settingBool("show_notifications", true)) {
		g_hintShown = true;
		g_ctx->tray_show_message(g_ctx->module_handle, g_tray,
								 "MeshMC is still running",
								 "Use the tray icon to bring the window back "
								 "or quit the launcher.",
								 1 /* Info */, 6000);
	}
	return 1; /* swallow → host will hide() the main window */
}

/* ── Settings UI: one ABI 5 GLOBAL_SETTINGS surface ───────────────── *
 *
 * Replaces injectCheckboxIntoMeshMCPage()'s allWidgets()/findChild walk
 * against MeshMCPage's "verticalLayout_9": the host now renders this
 * document as a titled section inside its own "Plugins" page every
 * time the global Settings dialog opens, from whatever document is
 * currently stored for this surface — so, unlike the old pattern,
 * this only needs to be created ONCE (here, from mmco_init()), not
 * re-injected via MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN on every
 * open. */

static void on_settings_surface_event(void* /*ud*/, const char* /*surface_id*/,
									  const char* node_id, const char* event,
									  const char* value_json)
{
	if (!g_ctx || !node_id || !event)
		return;
	if (QString::fromUtf8(node_id) != QLatin1String("enabled"))
		return;
	if (std::strcmp(event, "change") != 0)
		return;

	const bool checked = value_json && std::strcmp(value_json, "true") == 0;
	g_ctx->app_setting_set(g_ctx->module_handle, SETTING_GLOBAL_ENABLED,
						   checked ? "1" : "0");
	settingSetBool("enabled", checked);
}

static void create_settings_surface(bool currentlyEnabled)
{
	if (!g_ctx)
		return;
	const QJsonObject doc{
		{"type", "mmco-ui/1"},
		{"root",
		 QJsonObject{
			 {"type", "toggle"},
			 {"id", "enabled"},
			 {"props",
			  QJsonObject{
				  {"label", "Show MeshMC system tray icon (Restart required.)"},
				  {"value", currentlyEnabled},
				  {"enabled", true}}}}}};
	const QByteArray json = QJsonDocument(doc).toJson(QJsonDocument::Compact);
	g_settingsSurface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_GLOBAL_SETTINGS, nullptr,
		"System Tray", nullptr, json.constData(), on_settings_surface_event,
		nullptr);
}

/* ── hooks ────────────────────────────────────────────────────────── */

static int on_app_initialized(void*, uint32_t, void*, void*)
{
	return 0;
}

static int on_ui_main_ready(void* /*mh*/, uint32_t /*hook_id*/,
							void* /*payload*/, void* /*ud*/)
{
	if (!g_ctx)
		return 0;

	/* Install the close-event filter unconditionally — the callback
	 * itself short-circuits when the setting is off. This way the
	 * setting can be toggled at runtime without re-registering. */
	g_ctx->main_window_install_close_filter(g_ctx->module_handle,
											on_main_window_close, nullptr);

	/* Refresh the tray menu now that the UI is up — instance list is
	 * ready. */
	rebuild_tray_menu();
	return 0;
}

static int on_instance_created(void* /*mh*/, uint32_t /*hook_id*/,
							   void* /*payload*/, void* /*ud*/)
{
	rebuild_tray_menu();
	return 0;
}

static int on_instance_removed(void* /*mh*/, uint32_t /*hook_id*/,
							   void* /*payload*/, void* /*ud*/)
{
	rebuild_tray_menu();
	return 0;
}

/* ── lifecycle ────────────────────────────────────────────────────── */

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "SystemTray initializing...");

	if (is_flatpak()) {
		MMCO_LOG(ctx, "SystemTray: Flatpak sandbox detected; disabled "
					  "(system tray is unreliable inside Flatpak).");
		return 0;
	}

	/* Lifetime anchor for our Qt connections. We intentionally never
	 * delete this; Qt may still have queued events targeting it at
	 * shutdown. */
	g_guard = new QObject();

	/* Mirror the plugin-local "enabled" key onto a launcher-wide
	 * setting so the user can toggle it from Settings → MeshMC. The
	 * global setting wins on conflict — every plugin-local read
	 * delegates here first. */
	if (!ctx->app_setting_contains(ctx->module_handle,
								   SETTING_GLOBAL_ENABLED)) {
		/* First run — seed from the plugin-local value (if any), otherwise
		 * default ON. */
		ctx->app_setting_register(ctx->module_handle, SETTING_GLOBAL_ENABLED,
								  settingBool("enabled", true) ? "1" : "0");
	}
	bool globalEnabled = false;
	{
		const char* v =
			ctx->app_setting_get(ctx->module_handle, SETTING_GLOBAL_ENABLED);
		if (v) {
			QString s = QString::fromUtf8(v).trimmed().toLower();
			globalEnabled =
				s == QLatin1String("1") || s == QLatin1String("true") ||
				s == QLatin1String("yes") || s == QLatin1String("on");
		}
	}
	/* Re-sync the plugin-local copy so existing call-sites see the
	 * canonical answer. */
	settingSetBool("enabled", globalEnabled);

	/* The settings checkbox is offered regardless of whether the tray
	 * itself is currently enabled — it is the only way for the user to
	 * flip it back on. */
	create_settings_surface(globalEnabled);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);

	if (!globalEnabled) {
		MMCO_LOG(ctx, "SystemTray: disabled via global setting; idle "
					  "(re-enable from Settings → MeshMC).");
		return 0;
	}

	if (!ctx->tray_is_available(ctx->module_handle)) {
		MMCO_WARN(ctx, "SystemTray: host has no system tray available; idle.");
		return 0;
	}

	/* Default setting values (only set when missing — so we don't overwrite
	 * a user choice on re-init). */
	if (!ctx->setting_get(ctx->module_handle, "enabled"))
		settingSetBool("enabled", true);
	/* If the user installs SystemTray they almost always want the
	 * close-to-tray behaviour — otherwise the plugin is just a quit
	 * button.  Default to ON; users who want vanilla "close == quit"
	 * can flip the setting in the config file. */
	if (!ctx->setting_get(ctx->module_handle, "minimize_to_tray"))
		settingSetBool("minimize_to_tray", true);
	if (!ctx->setting_get(ctx->module_handle, "show_notifications"))
		settingSetBool("show_notifications", true);

	/* Resolve the launcher's own logo through the SDK — the icon is
	 * baked into MeshMC's resource bundle at ":/org.projecttick.MeshMC.svg"
	 * and is loaded directly. We try a few fallbacks so the tray still
	 * gets a sensible icon on stripped/older builds. */
	const char* iconCandidates[] = {
		":/org.projecttick.MeshMC.svg",			 /* primary — MeshMC logo  */
		":/multimc/scalable/instances/logo.svg", /* instance default */
		"meshmc",								 /* themed name (XDG)      */
		"applications-games",					 /* last-ditch fallback    */
	};
	g_tray = nullptr;
	for (const char* name : iconCandidates) {
		g_tray = ctx->tray_create(ctx->module_handle, name, "MeshMC");
		if (g_tray)
			break;
	}
	if (!g_tray) {
		MMCO_ERR(ctx, "SystemTray: tray_create() failed.");
		return 0;
	}

	/*
	 * Menu layout (top → bottom, the way most launchers do it):
	 *   Open MeshMC               ← primary action, picks Show or Hide
	 *   Hide window
	 *   ─────────────────────
	 *   Launch instance ▸
	 *       <Instance 1>
	 *       <Instance 2>
	 *       …
	 *   ─────────────────────
	 *   Quit MeshMC
	 *
	 * Built once as a JSON doc (build_tray_menu_json()) and re-issued
	 * via tray_set_menu() on every INSTANCE_CREATED/REMOVED so the
	 * "Launch instance" submenu never goes stale. */
	rebuild_tray_menu();
	ctx->tray_set_activation_cb(ctx->module_handle, g_tray, on_tray_activated,
								nullptr);
	ctx->tray_set_visible(ctx->module_handle, g_tray, 1);

	/* Hooks. */
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_MAIN_READY,
					   on_ui_main_ready, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_CREATED,
					   on_instance_created, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);

	MMCO_LOG(ctx, "SystemTray initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "SystemTray unloading.");

	/* PluginManager will sweep up the tray/menu/surface/close-filter on
	 * its own — see releaseTrayResourcesForModule() /
	 * releaseSurfacesForModule(). We just drop our raw handles so we
	 * never touch them again. */
	g_tray = nullptr;
	g_settingsSurface = nullptr;
	g_ctx = nullptr;
}

} /* extern "C" */
