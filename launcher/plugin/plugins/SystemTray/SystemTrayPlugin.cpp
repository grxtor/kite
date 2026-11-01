/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 *  SystemTray: Kite MMCO plugin
 *
 *  Adds a persistent QSystemTrayIcon with Show / Hide / Quit actions, a "Launch instance" submenu
 *  (up to MAX_INSTANCE_ENTRIES instances) and an optional minimize-to-tray close filter.
 *
 *  Settings (booleans, in the plugin's namespace):
 *    enabled             master switch                              (default 1)
 *    minimize_to_tray    swallow window close, hide instead         (default 1)
 *    show_notifications  show a transient hint on first hide        (default 1)
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"

/* qApp comes from whichever Q*Application header is included; the SDK does not pull in
 * <QApplication>, so this includes QCoreApplication (QMetaObject::invokeMethod needs only a
 * QObject*). */
#include <QCoreApplication>

#include <cstring>

/* Depends on DesktopNotifier >= 1.0.0: SystemTray only provides the tray icon and menu,
 * desktop-notification dispatch lives in DesktopNotifier. A hard dependency (optional=0) loads
 * DesktopNotifier first and makes SystemTray refuse to load if it is missing or disabled;
 * optional=1 keeps only the load order. */
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

/* The launcher-wide setting that mirrors our plugin-local "enabled" (visible in Settings → Kite).
 * The plugin-namespaced "enabled" stays the runtime source of truth; the global key drives the
 * checkbox and is mirrored back whenever the user flips it. */
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

/* Tray menu: the declarative "mmco-tray-menu/1" doc. The whole menu, including the "Launch
 * instance" submenu, is rebuilt and handed to tray_set_menu() again on every
 * INSTANCE_CREATED/REMOVED. Per-instance node ids are "launch:<instance_id>", so the event callback
 * recovers the instance id from node_id. */

static QByteArray build_tray_menu_json()
{
	QJsonObject openItem{{"type", "button"},
						 {"id", "open"},
						 {"props", QJsonObject{{"label", "Open Kite"}}}};
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
						 {"props", QJsonObject{{"label", "Quit Kite"}}}};

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
		/* quit() unwinds the event loop and Application shutdown, so PluginManager can
		 * mmco_unload() us. */
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
	/* QSystemTrayIcon::ActivationReason is { Unknown, Context=1 (right click), DoubleClick=2,
	 * Trigger=3 (left click), MiddleClick=4 }, not in "Trigger==1" order. Context raises the menu
	 * and must never toggle the window. */
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
	/* One-time hint so the user knows the launcher is still running in the tray. */
	if (!g_hintShown && settingBool("show_notifications", true)) {
		g_hintShown = true;
		g_ctx->tray_show_message(g_ctx->module_handle, g_tray,
								 "Kite is still running",
								 "Use the tray icon to bring the window back "
								 "or quit the launcher.",
								 1 /* Info */, 6000);
	}
	return 1; /* swallow → host will hide() the main window */
}

/* Settings UI: one ABI 5 GLOBAL_SETTINGS surface. The host renders the stored document as a titled
 * section in its "Plugins" page each time Settings opens, so it is created once, from mmco_init(). */

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
				  {"label", "Show Kite system tray icon (Restart required.)"},
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

	/* Installed unconditionally; the callback short-circuits when the setting is off, so toggling
	 * needs no re-registration. */
	g_ctx->main_window_install_close_filter(g_ctx->module_handle,
											on_main_window_close, nullptr);

	/* Refresh the tray menu now that the instance list is ready. */
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

	/* Lifetime anchor for our Qt connections; never deleted, Qt may still have events queued for it
	 * at shutdown. */
	g_guard = new QObject();

	/* Mirror the plugin-local "enabled" onto a launcher-wide setting so the user can toggle it in
	 * Settings → Kite. The global setting wins: every plugin-local read delegates here first. */
	if (!ctx->app_setting_contains(ctx->module_handle,
								   SETTING_GLOBAL_ENABLED)) {
		/* First run: seed from the plugin-local value, else default ON. */
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
	/* Re-sync the plugin-local copy so existing call sites see the canonical answer. */
	settingSetBool("enabled", globalEnabled);

	/* Offered even while the tray is disabled: it is the only way to turn it back on. */
	create_settings_surface(globalEnabled);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);

	if (!globalEnabled) {
		MMCO_LOG(ctx, "SystemTray: disabled via global setting; idle "
					  "(re-enable from Settings → Kite).");
		return 0;
	}

	if (!ctx->tray_is_available(ctx->module_handle)) {
		MMCO_WARN(ctx, "SystemTray: host has no system tray available; idle.");
		return 0;
	}

	/* Defaults, set only when missing so a user choice survives re-init. */
	if (!ctx->setting_get(ctx->module_handle, "enabled"))
		settingSetBool("enabled", true);
	/* Default ON: users who install SystemTray almost always want close-to-tray, else the plugin is
	 * just a quit button. Switch it off in the config file for a plain close == quit. */
	if (!ctx->setting_get(ctx->module_handle, "minimize_to_tray"))
		settingSetBool("minimize_to_tray", true);
	if (!ctx->setting_get(ctx->module_handle, "show_notifications"))
		settingSetBool("show_notifications", true);

	/* Resolve the launcher logo from Kite's resource bundle, with fallbacks for stripped or older
	 * builds. */
	const char* iconCandidates[] = {
		":/io.github.grxtor.Kite.svg",			 /* primary — Kite logo  */
		":/multimc/scalable/instances/logo.svg", /* instance default */
		"kite",								 /* themed name (XDG)      */
		"applications-games",					 /* last-ditch fallback    */
	};
	g_tray = nullptr;
	for (const char* name : iconCandidates) {
		g_tray = ctx->tray_create(ctx->module_handle, name, "Kite");
		if (g_tray)
			break;
	}
	if (!g_tray) {
		MMCO_ERR(ctx, "SystemTray: tray_create() failed.");
		return 0;
	}

	/* Menu layout: Open Kite (Show or Hide), Hide window, separator, Launch instance submenu,
	 * separator, Quit Kite. Built as a JSON doc and re-issued via tray_set_menu() on
	 * INSTANCE_CREATED/REMOVED so the submenu never goes stale. */
	rebuild_tray_menu();
	ctx->tray_set_activation_cb(ctx->module_handle, g_tray, on_tray_activated,
								nullptr);
	ctx->tray_set_visible(ctx->module_handle, g_tray, 1);

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

	/* PluginManager releases the tray, menu, surface and close filter itself
	 * (releaseTrayResourcesForModule()); just drop the raw handles. */
	g_tray = nullptr;
	g_settingsSurface = nullptr;
	g_ctx = nullptr;
}

} /* extern "C" */
