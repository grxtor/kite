/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: MIT
 *
 * NVIDIAPrimePlugin — MMCO entry point for the NVIDIA Prime plugin.
 *
 * Forces Minecraft to use the NVIDIA discrete GPU on Optimus laptops.
 *
 * Behaviour:
 *   - Flatpak:      prepends `prime-run` as a wrapper command.
 *   - Non-Flatpak:  injects environment variables:
 *       __NV_PRIME_RENDER_OFFLOAD=1
 *       __VK_LAYER_NV_optimus=NVIDIA_only
 *       __GLX_VENDOR_LIBRARY_NAME=nvidia
 *
 * ABI 5: the toggle is a declarative GLOBAL_SETTINGS surface (one
 * `toggle` node, same shape as SystemTray's settings surface) instead
 * of a QCheckBox injected into the Minecraft settings page via
 * qApp->allWidgets()/findChild(). The setting is stored under the same
 * key as before: plugin.nvidia_prime.enabled.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"

#include <cstring>

MMCO_DEFINE_MODULE("NVIDIA Prime Module", "1.0.0", "Project Tick",
				   "Discrete GPU offload via NVIDIA Prime Render Offload",
				   "MIT");

static MMCOContext* g_ctx = nullptr;
static constexpr const char SETTING_KEY[] = "plugin.nvidia_prime.enabled";
static void* g_settingsSurface = nullptr; /* ABI 5 GLOBAL_SETTINGS surface */

static bool is_flatpak()
{
	return QFile::exists("/.flatpak-info");
}

static bool is_enabled()
{
	if (!g_ctx)
		return false;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle, SETTING_KEY))
		return false;
	const char* v = g_ctx->app_setting_get(g_ctx->module_handle, SETTING_KEY);
	if (!v)
		return false;
	const QString s = QString::fromUtf8(v).trimmed().toLower();
	return s == QLatin1String("1") || s == QLatin1String("true") ||
		   s == QLatin1String("yes") || s == QLatin1String("on");
}

static void ensureSettingRegistered()
{
	if (!g_ctx)
		return;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle, SETTING_KEY))
		g_ctx->app_setting_register(g_ctx->module_handle, SETTING_KEY, "0");
}

/* ── Settings UI: one ABI 5 GLOBAL_SETTINGS surface ───────────────── *
 *
 * Replaces injectCheckboxIntoMinecraftPage()'s allWidgets()/findChild
 * walk against MinecraftPage's "verticalLayout_3": the host renders
 * this document as a titled section inside its own "Plugins" page
 * every time the global Settings dialog opens, from whatever document
 * is currently stored for this surface — so, like SystemTray's
 * settings surface, this only needs to be created ONCE (here, from
 * mmco_init()), not re-injected on every dialog open. */

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
	g_ctx->app_setting_set(g_ctx->module_handle, SETTING_KEY,
						   checked ? "1" : "0");
}

static void create_settings_surface()
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
				  {"label",
				   "Use discrete GPU (NVIDIA Prime Render Offload)"},
				  {"value", is_enabled()},
				  {"enabled", true}}}}}};
	const QByteArray json = QJsonDocument(doc).toJson(QJsonDocument::Compact);
	g_settingsSurface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_GLOBAL_SETTINGS, nullptr,
		"NVIDIA Prime", nullptr, json.constData(), on_settings_surface_event,
		nullptr);
}

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
							  void* /*payload*/, void* /*user_data*/)
{
	char buf[256];
	snprintf(
		buf, sizeof(buf), "NVIDIA Prime Render Offload is %s (Flatpak: %s)",
		is_enabled() ? "ENABLED" : "disabled", is_flatpak() ? "yes" : "no");
	MMCO_LOG(g_ctx, buf);
	return 0;
}

static int on_instance_pre_launch(void* mh, uint32_t /*hook_id*/, void* payload,
								  void* /*user_data*/)
{
	if (!is_enabled())
		return 0;

	auto* info = static_cast<MMCOInstanceInfo*>(payload);

	if (is_flatpak()) {
		g_ctx->launch_prepend_wrapper(mh, "prime-run");

		char buf[512];
		snprintf(buf, sizeof(buf),
				 "Instance '%s': prime-run wrapper will be used (Flatpak)",
				 info->instance_name ? info->instance_name : "?");
		MMCO_LOG(g_ctx, buf);
	} else {
		g_ctx->launch_set_env(mh, "__NV_PRIME_RENDER_OFFLOAD", "1");
		g_ctx->launch_set_env(mh, "__VK_LAYER_NV_optimus", "NVIDIA_only");
		g_ctx->launch_set_env(mh, "__GLX_VENDOR_LIBRARY_NAME", "nvidia");

		char buf[512];
		snprintf(buf, sizeof(buf),
				 "Instance '%s': NVIDIA offload env vars injected",
				 info->instance_name ? info->instance_name : "?");
		MMCO_LOG(g_ctx, buf);
	}

	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "NVIDIA Prime plugin initializing...");

	ensureSettingRegistered();
	create_settings_surface();

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_instance_pre_launch, nullptr);

	MMCO_LOG(ctx, "NVIDIA Prime plugin initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx) {
		MMCO_LOG(g_ctx, "NVIDIA Prime plugin unloading.");
	}
	/* PluginManager tears down every surface this module still owns
	 * when it unloads (see SurfaceRecord teardown in PluginManager.cpp) —
	 * we just drop our raw handle so we never touch it again. */
	g_settingsSurface = nullptr;
	g_ctx = nullptr;
}

} /* extern "C" */
