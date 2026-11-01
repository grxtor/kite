/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
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

/* Settings UI: ABI 5 surfaces. One GLOBAL_SETTINGS surface created once, and one INSTANCE_SETTINGS
 * surface per instance, created for every instance on MMCO_HOOK_UI_MAIN_READY. Each toggle
 * persists its setting immediately on its "change" event. */

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

/* Per-instance surface bag keyed by instance id, so INSTANCE_REMOVED can tear it down and the
 * surface event callback (which gets only the host's opaque surface_id) can recover its instance
 * through user_data. */
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
		/* The mangohud/gamemode toggles' enabled state (and displayed value once the override is
		 * turned off) depend on the override flag: refresh the whole document. */
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

/* The instance list is populated by the time the main window is up, so every existing instance
 * gets its surface here. */
static int on_ui_main_ready(void*, uint32_t, void*, void*)
{
	if (!g_ctx)
		return 0;
	const int total = g_ctx->instance_count(g_ctx->module_handle);
	for (int i = 0; i < total; ++i) {
		/* Copy before the next call: the host returns strings in one per-module buffer. */
		const char* rawId = g_ctx->instance_get_id(g_ctx->module_handle, i);
		if (!rawId)
			continue;
		const QByteArray id(rawId);
		ensure_instance_surface(id.constData());
	}
	return 0;
}

/* Instances created, imported or copied after the main window is up. */
static int on_instance_created(void*, uint32_t, void* payload, void*)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!g_ctx || !info || !info->instance_id)
		return 0;
	ensure_instance_surface(info->instance_id);
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

	/* Wrapper build order (each call prepends): mangohud first, then gamemoderun, giving
	 * "gamemoderun mangohud java [args]". gamemoderun wraps the chain and requests GameMode for the
	 * child; mangohud hooks the JVM's graphics APIs via LD_PRELOAD. */
	bool mangoEnabled =
		isMangohudEnabledForInstance(iid) && mangohud_available();
	bool gamemodeEnabled =
		isGamemodeEnabledForInstance(iid) && gamemoderun_available();

	if (mangoEnabled) {
		QByteArray mangohudWrapper = mangohud_executable().toUtf8();

		/* Native and Flatpak alike; in the Flatpak build prefer the mounted extension path instead
		 * of relying on PATH propagation. */
		g_ctx->launch_prepend_wrapper(mh, mangohudWrapper.constData());

		/* MANGOHUD=1  — enables the Vulkan implicit layer (all MC ≥ 1.17) */
		g_ctx->launch_set_env(mh, "MANGOHUD", "1");

		/* MANGOHUD_DLSYM=1: MangoHud intercepts OpenGL functions LWJGL loads dynamically (critical
		 * for Java/OpenGL). */
		g_ctx->launch_set_env(mh, "MANGOHUD_DLSYM", "1");

		/* MANGOHUD_CONFIGFILE: MangoHud finds its config by process name, and Minecraft runs as
		 * "java", so it would not find "java.conf" and may show no HUD. Point it at the standard
		 * MangoHud.conf. */
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

		/* Prepend gamemoderun last so it wraps the whole command (mangohud included) and GameMode
		 * activates for the child PID. */
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
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_MAIN_READY,
					   on_ui_main_ready, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_CREATED,
					   on_instance_created, nullptr);
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
	/* PluginManager tears down the surfaces this module owns on unload; only free our own
	 * bookkeeping. */
	for (auto* ctx : g_instanceSurfaces)
		delete ctx;
	g_instanceSurfaces.clear();
}

} /* extern "C" */
