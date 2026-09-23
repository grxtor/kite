/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "vendor/gamemode_client.h"
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>

#include <cstring>

MMCO_DEFINE_MODULE("Linux Performance Tools", "1.2.0", "Project Tick",
				   "MangoHud FPS overlay and GameMode performance integration "
				   "for Minecraft on Linux",
				   "Apache-2.0");

static constexpr const char SETTING_MANGOHUD[] =
	"plugin.linuxperf.mangohud.enabled";
static constexpr const char SETTING_GAMEMODE[] =
	"plugin.linuxperf.gamemode.enabled";
static constexpr const char SETTING_INSTANCE_OVERRIDE[] =
	"plugin.linuxperf.override";

static MMCOContext* g_ctx = nullptr;
static void* g_globalSurface = nullptr; /* ABI 5 GLOBAL_SETTINGS surface */

static bool is_flatpak()
{
	return QFile::exists(QStringLiteral("/.flatpak-info"));
}

static QString flatpak_mangohud_executable()
{
	const QStringList candidates = {
		QStringLiteral("/usr/lib/extensions/vulkan/MangoHud/bin/mangohud"),
		QStringLiteral(
			"/usr/lib/extensions/vulkan/"
			"org.freedesktop.Platform.VulkanLayer.MangoHud/bin/mangohud")};

	for (const auto& candidate : candidates) {
		QFileInfo info(candidate);
		if (info.exists() && info.isExecutable())
			return candidate;
	}

	return {};
}

static QString mangohud_executable()
{
	if (is_flatpak()) {
		auto flatpakExecutable = flatpak_mangohud_executable();
		if (!flatpakExecutable.isEmpty())
			return flatpakExecutable;
	}

	return QStandardPaths::findExecutable(QStringLiteral("mangohud"));
}

static QString gamemoderun_executable()
{
	return QStandardPaths::findExecutable(QStringLiteral("gamemoderun"));
}

static bool mangohud_available()
{
	return !mangohud_executable().isEmpty();
}

static bool gamemoderun_available()
{
	return !gamemoderun_executable().isEmpty();
}

static QString gamemode_status_text()
{
	int status = gamemode_query_status();
	QString gmBin = gamemoderun_executable();
	if (status >= 0) {
		return QObject::
			tr("GameMode daemon: running — will activate at game launch  (%1)")
				.arg(gmBin);
	}

	if (is_flatpak()) {
		return QObject::tr("GameMode daemon: not reachable from sandbox — add "
						   "session bus access to\n"
						   "com.feralinteractive.GameMode and ensure the host "
						   "gamemoded service is running");
	}

	return QObject::tr(
		"GameMode daemon: not responding — run \"systemctl --user enable --now "
		"gamemoded\" or check gamemode install");
}

static bool ctxBoolApp(const char* key)
{
	if (!g_ctx)
		return false;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle, key))
		return false;
	const char* v = g_ctx->app_setting_get(g_ctx->module_handle, key);
	if (!v)
		return false;
	QString s = QString::fromUtf8(v).trimmed().toLower();
	return s == QLatin1String("1") || s == QLatin1String("true") ||
		   s == QLatin1String("yes") || s == QLatin1String("on");
}

static bool ctxBoolInstance(const char* instanceId, const char* key)
{
	if (!g_ctx || !instanceId)
		return false;
	if (!g_ctx->instance_setting_contains(g_ctx->module_handle, instanceId,
										  key))
		return false;
	const char* v =
		g_ctx->instance_setting_get(g_ctx->module_handle, instanceId, key);
	if (!v)
		return false;
	QString s = QString::fromUtf8(v).trimmed().toLower();
	return s == QLatin1String("1") || s == QLatin1String("true") ||
		   s == QLatin1String("yes") || s == QLatin1String("on");
}

static void ensureSettingsRegistered()
{
	if (!g_ctx)
		return;
	auto ensureKey = [&](const char* key) {
		if (!g_ctx->app_setting_contains(g_ctx->module_handle, key))
			g_ctx->app_setting_register(g_ctx->module_handle, key, "0");
	};
	ensureKey(SETTING_MANGOHUD);
	ensureKey(SETTING_GAMEMODE);
}

static bool isMangohudEnabled()
{
	return ctxBoolApp(SETTING_MANGOHUD);
}

static bool isGamemodeEnabled()
{
	return ctxBoolApp(SETTING_GAMEMODE);
}

static void ensureInstanceSettingsRegistered(const char* instanceId)
{
	if (!g_ctx || !instanceId)
		return;

	ensureSettingsRegistered();

	if (!g_ctx->instance_setting_contains(g_ctx->module_handle, instanceId,
										  SETTING_INSTANCE_OVERRIDE)) {
		g_ctx->instance_setting_register(g_ctx->module_handle, instanceId,
										 SETTING_INSTANCE_OVERRIDE, "0");
	}
	g_ctx->instance_setting_register_override(g_ctx->module_handle, instanceId,
											  SETTING_MANGOHUD,
											  SETTING_INSTANCE_OVERRIDE);
	g_ctx->instance_setting_register_override(g_ctx->module_handle, instanceId,
											  SETTING_GAMEMODE,
											  SETTING_INSTANCE_OVERRIDE);
}

static bool isMangohudEnabledForInstance(const char* instanceId)
{
	if (!instanceId)
		return isMangohudEnabled();
	ensureInstanceSettingsRegistered(instanceId);
	return ctxBoolInstance(instanceId, SETTING_MANGOHUD);
}

static bool isGamemodeEnabledForInstance(const char* instanceId)
{
	if (!instanceId)
		return isGamemodeEnabled();
	ensureInstanceSettingsRegistered(instanceId);
	return ctxBoolInstance(instanceId, SETTING_GAMEMODE);
}

/* ── Settings UI: declarative ABI 5 surfaces ──────────────────────── *
 *
 * Replaces injectCheckboxesIntoMinecraftPage()/
 * injectCheckboxesIntoInstanceSettingsPage()'s allWidgets()/findChild
 * walks with two kinds of `ui_surface_create` calls: one GLOBAL_SETTINGS
 * surface (created once, like NVIDIAPrime/SystemTray) and one
 * INSTANCE_SETTINGS surface per instance, created lazily the first time
 * the host signals that instance's settings context
 * (MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED) and refreshed on every
 * subsequent open. There is no separate "apply" step any more — each
 * toggle persists its own setting immediately on its "change" event,
 * the same way SystemTray's/NVIDIAPrime's settings toggles do. */

static QJsonObject buildGlobalDoc()
{
	const bool mangoAvail = mangohud_available();
	const bool gmAvail = gamemoderun_available();

	QJsonArray children{
		QJsonObject{
			{"type", "toggle"},
			{"id", "mangohud"},
			{"props",
			 QJsonObject{
				 {"label",
				  "Enable MangoHud overlay (FPS / GPU / CPU metrics)"},
				 {"value", isMangohudEnabled() && mangoAvail},
				 {"enabled", mangoAvail}}}},
		QJsonObject{
			{"type", "toggle"},
			{"id", "gamemode"},
			{"props",
			 QJsonObject{
				 {"label", "Enable GameMode (CPU / scheduler performance "
						   "optimisations)"},
				 {"value", isGamemodeEnabled() && gmAvail},
				 {"enabled", gmAvail}}}}};

	if (gmAvail) {
		children.append(QJsonObject{
			{"type", "text"},
			{"id", "gamemode_status"},
			{"props", QJsonObject{{"text", gamemode_status_text()}}}});
	}

	return QJsonObject{
		{"type", "mmco-ui/1"},
		{"root", QJsonObject{{"type", "column"},
							 {"id", "root"},
							 {"children", children}}}};
}

static void on_global_surface_event(void* /*ud*/, const char* /*surface_id*/,
									const char* node_id, const char* event,
									const char* value_json)
{
	if (!g_ctx || !node_id || !event || std::strcmp(event, "change") != 0)
		return;

	const bool checked = value_json && std::strcmp(value_json, "true") == 0;
	const QString id = QString::fromUtf8(node_id);
	if (id == QLatin1String("mangohud"))
		g_ctx->app_setting_set(g_ctx->module_handle, SETTING_MANGOHUD,
							   checked ? "1" : "0");
	else if (id == QLatin1String("gamemode"))
		g_ctx->app_setting_set(g_ctx->module_handle, SETTING_GAMEMODE,
							   checked ? "1" : "0");
}

static void create_global_settings_surface()
{
	if (!g_ctx)
		return;
	const QByteArray json =
		QJsonDocument(buildGlobalDoc()).toJson(QJsonDocument::Compact);
	g_globalSurface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_GLOBAL_SETTINGS, nullptr,
		"Linux Performance Tools", nullptr, json.constData(),
		on_global_surface_event, nullptr);
}

/* Per-instance surface bag, keyed by instance id so
 * MMCO_HOOK_INSTANCE_REMOVED can tear the surface down and so the
 * surface's own event callback (which only gets the opaque `surface_id`
 * the host assigned, not the instance id) can recover which instance it
 * belongs to via `user_data`. */
struct InstanceSurfaceCtx {
	QByteArray instanceId;
	void* surface = nullptr;
};
static QHash<QByteArray, InstanceSurfaceCtx*> g_instanceSurfaces;

static QJsonObject buildInstanceDoc(const char* instanceId)
{
	const bool overrideEnabled =
		ctxBoolInstance(instanceId, SETTING_INSTANCE_OVERRIDE);
	const bool mangoAvail = mangohud_available();
	const bool gmAvail = gamemoderun_available();

	QJsonArray children{
		QJsonObject{
			{"type", "toggle"},
			{"id", "override"},
			{"props",
			 QJsonObject{
				 {"label", "Override global Linux performance tools settings"},
				 {"value", overrideEnabled},
				 {"enabled", true}}}},
		QJsonObject{
			{"type", "toggle"},
			{"id", "mangohud"},
			{"props",
			 QJsonObject{
				 {"label",
				  "Enable MangoHud overlay (FPS / GPU / CPU metrics)"},
				 {"value", isMangohudEnabledForInstance(instanceId) &&
							   mangoAvail},
				 {"enabled", overrideEnabled && mangoAvail}}}},
		QJsonObject{
			{"type", "toggle"},
			{"id", "gamemode"},
			{"props",
			 QJsonObject{
				 {"label", "Enable GameMode (CPU / scheduler performance "
						   "optimisations)"},
				 {"value", isGamemodeEnabledForInstance(instanceId) &&
							   gmAvail},
				 {"enabled", overrideEnabled && gmAvail}}}}};

	if (gmAvail) {
		children.append(QJsonObject{
			{"type", "text"},
			{"id", "gamemode_status"},
			{"props", QJsonObject{{"text", gamemode_status_text()}}}});
	}

	return QJsonObject{
		{"type", "mmco-ui/1"},
		{"root", QJsonObject{{"type", "column"},
							 {"id", "root"},
							 {"children", children}}}};
}

static void refreshInstanceSurface(InstanceSurfaceCtx* ctx)
{
	if (!g_ctx || !ctx || !ctx->surface)
		return;
	const QByteArray json =
		QJsonDocument(buildInstanceDoc(ctx->instanceId.constData()))
			.toJson(QJsonDocument::Compact);
	g_ctx->ui_surface_update(g_ctx->module_handle, ctx->surface,
							 json.constData());
}

static void on_instance_surface_event(void* user_data,
									  const char* /*surface_id*/,
									  const char* node_id, const char* event,
									  const char* value_json)
{
	auto* ctx = static_cast<InstanceSurfaceCtx*>(user_data);
	if (!g_ctx || !ctx || !node_id || !event ||
		std::strcmp(event, "change") != 0)
		return;

	const char* iid = ctx->instanceId.constData();
	const QString id = QString::fromUtf8(node_id);
	const bool checked = value_json && std::strcmp(value_json, "true") == 0;

	ensureInstanceSettingsRegistered(iid);

	if (id == QLatin1String("override")) {
		g_ctx->instance_setting_set(g_ctx->module_handle, iid,
									SETTING_INSTANCE_OVERRIDE,
									checked ? "1" : "0");
		if (!checked) {
			g_ctx->instance_setting_reset(g_ctx->module_handle, iid,
										  SETTING_MANGOHUD);
			g_ctx->instance_setting_reset(g_ctx->module_handle, iid,
										  SETTING_GAMEMODE);
		}
		/* The mangohud/gamemode toggles' enabled state (and, when the
		 * override was just turned off, their displayed value) depend
		 * on the override flag — refresh the whole document. */
		refreshInstanceSurface(ctx);
	} else if (id == QLatin1String("mangohud")) {
		g_ctx->instance_setting_set(g_ctx->module_handle, iid,
									SETTING_MANGOHUD, checked ? "1" : "0");
	} else if (id == QLatin1String("gamemode")) {
		g_ctx->instance_setting_set(g_ctx->module_handle, iid,
									SETTING_GAMEMODE, checked ? "1" : "0");
	}
}

static void ensure_instance_surface(const char* instanceId)
{
	if (!g_ctx || !instanceId)
		return;

	const QByteArray idKey(instanceId);
	auto it = g_instanceSurfaces.find(idKey);
	if (it != g_instanceSurfaces.end()) {
		refreshInstanceSurface(it.value());
		return;
	}

	ensureInstanceSettingsRegistered(instanceId);

	auto* ctx = new InstanceSurfaceCtx{idKey, nullptr};
	const QByteArray json =
		QJsonDocument(buildInstanceDoc(instanceId)).toJson(QJsonDocument::Compact);
	ctx->surface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_INSTANCE_SETTINGS, instanceId,
		"Linux Performance Tools", nullptr, json.constData(),
		on_instance_surface_event, ctx);
	g_instanceSurfaces.insert(idKey, ctx);
}

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
							  void* /*payload*/, void* /*user_data*/)
{
	char buf[256];
	snprintf(buf, sizeof(buf),
			 "LinuxPerf: MangoHud available=%s  GameMode available=%s  "
			 "MangoHud enabled=%s  GameMode enabled=%s",
			 mangohud_available() ? "yes" : "no",
			 gamemoderun_available() ? "yes" : "no",
			 isMangohudEnabled() ? "yes" : "no",
			 isGamemodeEnabled() ? "yes" : "no");
	MMCO_LOG(g_ctx, buf);
	return 0;
}

/* MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED — replaces direct
 * Application::instanceSettingsPageCreated + findChild("verticalLayout_8").
 * Fires every time an instance's settings dialog is (re)opened; we
 * create the surface for that instance the first time and just refresh
 * its values/enabled-state on subsequent opens (mirrors what the
 * PAGE_LOADED hook used to do). */
static int on_instance_settings_page_created(void*, uint32_t, void* payload,
											 void*)
{
	auto* evt = static_cast<MMCOInstanceSettingsPageEvent*>(payload);
	if (!evt || !evt->instance_id)
		return 0;
	ensure_instance_surface(evt->instance_id);
	return 0;
}

static int on_instance_removed(void*, uint32_t, void* payload, void*)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!g_ctx || !info || !info->instance_id)
		return 0;
	auto it = g_instanceSurfaces.find(QByteArray(info->instance_id));
	if (it != g_instanceSurfaces.end()) {
		g_ctx->ui_surface_destroy(g_ctx->module_handle, it.value()->surface);
		delete it.value();
		g_instanceSurfaces.erase(it);
	}
	return 0;
}

static int on_instance_pre_launch(void* mh, uint32_t /*hook_id*/, void* payload,
								  void* /*user_data*/)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	const char* iname = info->instance_name ? info->instance_name : "?";
	const char* iid = info ? info->instance_id : nullptr;

	/*
	 * Wrapper command build order (each call *prepends* to the chain):
	 *
	 *   Step 1 - mangohud:   pending = "mangohud"
	 *   Step 2 - gamemoderun: pending = "gamemoderun mangohud"
	 *
	 * Final command: gamemoderun mangohud java [args]
	 *
	 * gamemoderun wraps the entire chain and requests GameMode for the child;
	 * mangohud hooks into the JVM's graphics APIs via LD_PRELOAD.
	 */
	bool mangoEnabled =
		isMangohudEnabledForInstance(iid) && mangohud_available();
	bool gamemodeEnabled =
		isGamemodeEnabledForInstance(iid) && gamemoderun_available();

	if (mangoEnabled) {
		QByteArray mangohudWrapper = mangohud_executable().toUtf8();

		/* Use the wrapper in both native and Flatpak environments. In the
		 * Flatpak build, prefer the mounted extension path directly instead of
		 * relying on PATH propagation. */
		g_ctx->launch_prepend_wrapper(mh, mangohudWrapper.constData());

		/* MANGOHUD=1  — enables the Vulkan implicit layer (all MC ≥ 1.17) */
		g_ctx->launch_set_env(mh, "MANGOHUD", "1");

		/* MANGOHUD_DLSYM=1 — hook dlsym so MangoHud intercepts OpenGL
		 * functions loaded dynamically by LWJGL (critical for Java/OpenGL) */
		g_ctx->launch_set_env(mh, "MANGOHUD_DLSYM", "1");

		/* MANGOHUD_CONFIGFILE — MangoHud auto-detects config by process name.
		 * Since Minecraft runs as "java", it can't find "java.conf" and
		 * may silently use no HUD.  Point it at the standard MangoHud.conf
		 * so the overlay always appears with sensible defaults. */
		QByteArray mangoCfg =
			(QDir::homePath() +
			 QStringLiteral("/.config/MangoHud/MangoHud.conf"))
				.toUtf8();
		g_ctx->launch_set_env(mh, "MANGOHUD_CONFIGFILE", mangoCfg.constData());

		char buf[512];
		snprintf(buf, sizeof(buf),
				 "LinuxPerf: instance '%s': mangohud wrapper '%s' + env vars "
				 "applied%s",
				 iname, mangohudWrapper.constData(),
				 is_flatpak() ? " (Flatpak)" : "");
		MMCO_LOG(g_ctx, buf);
	}

	if (gamemodeEnabled) {
		QByteArray gamemodeWrapper = gamemoderun_executable().toUtf8();

		/* Prepend gamemoderun so it wraps the entire command (including
		 * mangohud if both are enabled), ensuring GameMode activates for the
		 * child PID. */
		g_ctx->launch_prepend_wrapper(mh, gamemodeWrapper.constData());

		char buf[512];
		snprintf(buf, sizeof(buf),
				 "LinuxPerf: instance '%s': gamemoderun wrapper '%s' applied",
				 iname, gamemodeWrapper.constData());
		MMCO_LOG(g_ctx, buf);
	}

	return 0; /* Never cancel the launch */
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "LinuxPerf plugin initializing...");

	ensureSettingsRegistered();
	create_global_settings_surface();

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);
	ctx->hook_register(ctx->module_handle,
					   MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED,
					   on_instance_settings_page_created, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_instance_pre_launch, nullptr);

	MMCO_LOG(ctx, "LinuxPerf plugin initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "LinuxPerf plugin unloading.");
	g_ctx = nullptr;
	g_globalSurface = nullptr;
	/* PluginManager tears down every surface this module still owns
	 * when it unloads — we just free our own bookkeeping. */
	for (auto* ctx : g_instanceSurfaces)
		delete ctx;
	g_instanceSurfaces.clear();
}

} /* extern "C" */
