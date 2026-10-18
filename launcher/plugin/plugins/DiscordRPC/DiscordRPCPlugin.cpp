/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: MS-PL
 *
 *  DiscordRPC  —  Kite MMCO plugin
 *
 *  Publishes a Discord Rich Presence activity reflecting what the user
 *  is doing in Kite:
 *
 *    In launcher                 → "Browsing instances"
 *    Instance launching          → "Playing <Instance>"
 *    Activity is cleared on instance exit / app shutdown.
 *
 *  Communication uses the local IPC socket through the vendored
 *  discord_ipc client (vendor/discord_ipc/). No outbound network
 *  traffic; if Discord is not running, the plugin retries silently.
 *
 *  The Discord application ID is hard-coded — change DISCORD_APP_ID
 *  if you fork this plugin for your own community.
 *
 *  Settings (all booleans / strings, stored under the plugin's
 *  namespace):
 *    enabled            — master switch                (default 1)
 *    show_instance_name — include the instance name    (default 1)
 *    show_mc_version    — include the MC version       (default 1)
 *    custom_details     — override the "details" line  (default "")
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "discord_ipc/discord_ipc.h"

#include <QDateTime>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

MMCO_DEFINE_MODULE("DiscordRPC", "1.0.0", "Project Tick",
				   "Discord Rich Presence integration — shows the active "
				   "Kite instance in your Discord status",
				   "MS-PL");

/* TODO(fork): our own application from the Discord Developer Portal.
 * Empty keeps the plugin idle. */
static constexpr const char* DISCORD_APP_ID = "";
static constexpr const char* LARGE_IMAGE_KEY = "kite";

/* ── module state ─────────────────────────────────────────────────── */

static MMCOContext* g_ctx = nullptr;
static QPointer<DiscordIpc> g_ipc;
static qint64 g_launchEpoch = 0; /* unix sec; 0 = idle  (game session start) */
static qint64 g_idleEpoch = 0; /* unix sec;          (launcher session start) */
static QString g_activeInstance;
static QString g_activeMcVersion;
static QString g_activeInstanceId; /* stable id used to disconnect later  */

/*
 * Running-state callback registrations are now owned by PluginManager
 * via the S23 instance_running_register API.  PluginManager severs
 * every registration automatically on mmco_unload() so the plugin's
 * callback can never fire into freed memory — no per-plugin guard
 * QObject is required for this side any more.
 */

/* ── settings ─────────────────────────────────────────────────────── */

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

static QString settingStr(const char* key, const QString& fallback)
{
	if (!g_ctx)
		return fallback;
	const char* v = g_ctx->setting_get(g_ctx->module_handle, key);
	return v ? QString::fromUtf8(v) : fallback;
}

static void settingDefault(const char* key, const char* val)
{
	if (g_ctx && !g_ctx->setting_get(g_ctx->module_handle, key))
		g_ctx->setting_set(g_ctx->module_handle, key, val);
}

/* ── presence helpers ─────────────────────────────────────────────── */

static void publish_idle_presence()
{
	if (!g_ipc)
		return;
	/* Set the launcher-session timestamp exactly once so Discord's
	 * "elapsed" counter doesn't reset every time we bounce back to
	 * idle after a game ends. */
	if (g_idleEpoch == 0)
		g_idleEpoch = QDateTime::currentSecsSinceEpoch();
	DiscordActivity a;
	a.details = QStringLiteral("In launcher");
	a.state = QStringLiteral("Browsing instances");
	a.largeImageKey = QString::fromLatin1(LARGE_IMAGE_KEY);
	a.largeImageText = QStringLiteral("Kite");
	a.startTimestamp = g_idleEpoch;
	g_ipc->setActivity(a);
}

static void publish_playing_presence()
{
	if (!g_ipc)
		return;

	const QString custom = settingStr("custom_details", QString()).trimmed();
	const bool showName = settingBool("show_instance_name", true);
	const bool showVer = settingBool("show_mc_version", true);

	DiscordActivity a;
	if (!custom.isEmpty()) {
		a.details = custom;
	} else if (showName && !g_activeInstance.isEmpty()) {
		a.details = QStringLiteral("Playing %1").arg(g_activeInstance);
	} else {
		a.details = QStringLiteral("Playing Minecraft");
	}

	if (showVer && !g_activeMcVersion.isEmpty()) {
		a.state = QStringLiteral("Minecraft %1").arg(g_activeMcVersion);
	}

	a.largeImageKey = QString::fromLatin1(LARGE_IMAGE_KEY);
	a.largeImageText = QStringLiteral("Kite");
	a.startTimestamp =
		g_launchEpoch > 0 ? g_launchEpoch : QDateTime::currentSecsSinceEpoch();
	g_ipc->setActivity(a);
}

/* ── hooks ────────────────────────────────────────────────────────── */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
							  void* /*payload*/, void* /*ud*/)
{
	if (g_ipc)
		publish_idle_presence();
	return 0;
}

/*
 * NOTE ON MMCO HOOK SEMANTICS (ABI 2)
 *
 *   MMCO_HOOK_INSTANCE_PRE_LAUNCH   — fires *before* LaunchTask::start(),
 *                                     i.e. just before the game spawns.
 *
 *   MMCO_HOOK_INSTANCE_POST_LAUNCH  — fires from LaunchController::onSucceeded
 *                                     which is wired to LaunchTask::succeeded,
 *                                     which is emitted by LaunchTask::
 *                                     emitSucceeded() *after* the JVM exits
 *                                     and m_instance->setRunning(false) has
 *                                     been called. In other words:
 *                                       POST_LAUNCH == GAME EXITED.
 *                                     The name is unfortunate; do not be
 *                                     fooled by it.
 *
 * So our presence flow is:
 *
 *   PRE_LAUNCH  → "Playing X (MC Y.Z)" + connect runningStatusChanged
 *   POST_LAUNCH → "In launcher / Browsing instances"   ← back to idle
 *   running=true/false signal is a defensive fallback for both edges.
 */

/* C-ABI callback installed via instance_running_register.  Mirrors the
 * old QObject::connect(&BaseInstance::runningStatusChanged) handler:
 * if the running edge fires for the instance we currently track in
 * Discord, refresh or clear the presence accordingly. */
static void on_instance_running(void* /*ud*/, const char* instance_id,
								int running)
{
	if (!g_ipc || !instance_id)
		return;
	const QString id = QString::fromUtf8(instance_id);
	if (running) {
		/* Game actually started — refresh the playing presence in
		 * case the MC version wasn't known yet at PRE_LAUNCH time
		 * (it might have been late-resolved by the launcher). */
		if (id == g_activeInstanceId) {
			if (g_ctx) {
				const char* mc = g_ctx->instance_get_mc_version(
					g_ctx->module_handle, instance_id);
				if (mc && *mc)
					g_activeMcVersion = QString::fromUtf8(mc);
			}
			publish_playing_presence();
		}
	} else {
		/* Game exited — fall back to idle, but only if it was *this*
		 * instance we were tracking. */
		if (id == g_activeInstanceId) {
			g_activeInstance.clear();
			g_activeMcVersion.clear();
			g_activeInstanceId.clear();
			g_launchEpoch = 0;
			publish_idle_presence();
		}
	}
}

/* Replaces the old hook_instance_running_signal() / direct
 * APPLICATION->instances() + BaseInstance signal connection.  Asks
 * PluginManager (S23) to deliver runningStatusChanged transitions for
 * the named instance to on_instance_running.  Re-registration replaces
 * any prior callback for the same id, matching the old QObject::
 * disconnect() before connect() dedupe behaviour. */
static void hook_instance_running_signal(const QString& instanceId)
{
	if (!g_ctx)
		return;

	/* Capture the MC version from the C-ABI — the value is the same
	 * one MinecraftInstance::getPackProfile()->getComponentVersion(
	 * "net.minecraft") used to return. */
	const char* mc = g_ctx->instance_get_mc_version(
		g_ctx->module_handle, instanceId.toUtf8().constData());
	if (mc && *mc)
		g_activeMcVersion = QString::fromUtf8(mc);

	g_ctx->instance_running_register(g_ctx->module_handle,
									 instanceId.toUtf8().constData(),
									 &on_instance_running, nullptr);
}

/*
 * PRE_LAUNCH  ==  game is about to start.
 * This is where we *start* showing "Playing X" in Discord.
 */
static int on_instance_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
								  void* payload, void* /*ud*/)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info)
		return 0;

	g_launchEpoch = QDateTime::currentSecsSinceEpoch();
	g_activeInstance = info->instance_name
						   ? QString::fromUtf8(info->instance_name)
						   : QString();
	g_activeMcVersion = info->minecraft_version
							? QString::fromUtf8(info->minecraft_version)
							: QString();
	g_activeInstanceId =
		info->instance_id ? QString::fromUtf8(info->instance_id) : QString();

	if (!g_activeInstanceId.isEmpty())
		hook_instance_running_signal(g_activeInstanceId);

	publish_playing_presence();
	return 0;
}

/*
 * POST_LAUNCH  ==  game has exited (see comment above).
 * Reset the presence to the idle "In launcher" line. The
 * runningStatusChanged(false) handler installed in PRE_LAUNCH does
 * essentially the same thing, but POST_LAUNCH gives us a guaranteed
 * deterministic moment to clear state without waiting for Qt's
 * signal queue.
 */
static int on_instance_post_launch(void* /*mh*/, uint32_t /*hook_id*/,
								   void* payload, void* /*ud*/)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	const QString exitedId = (info && info->instance_id)
								 ? QString::fromUtf8(info->instance_id)
								 : QString();
	/* If a *different* instance was launched in the meantime (e.g. the
	 * user launched B while A was still shutting down), don't clobber
	 * B's presence. */
	if (!exitedId.isEmpty() && exitedId != g_activeInstanceId)
		return 0;

	g_activeInstance.clear();
	g_activeMcVersion.clear();
	g_activeInstanceId.clear();
	g_launchEpoch = 0;
	publish_idle_presence();
	return 0;
}

static int on_app_shutdown(void* /*mh*/, uint32_t /*hook_id*/,
						   void* /*payload*/, void* /*ud*/)
{
	if (g_ipc) {
		g_ipc->clearActivity();
		g_ipc->disconnectFromDiscord();
	}
	return 0;
}

/* ── lifecycle ────────────────────────────────────────────────────── */

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "DiscordRPC initializing...");

	settingDefault("enabled", "1");
	settingDefault("show_instance_name", "1");
	settingDefault("show_mc_version", "1");
	settingDefault("custom_details", "");

	if (!settingBool("enabled", true)) {
		MMCO_LOG(ctx, "DiscordRPC: disabled via setting; idle.");
		return 0;
	}

	if (DISCORD_APP_ID[0] == '\0') {
		MMCO_LOG(ctx, "DiscordRPC: no Discord application ID; idle.");
		return 0;
	}

	g_ipc = new DiscordIpc();
	g_ipc->setClientId(QString::fromLatin1(DISCORD_APP_ID));

	/* Track the previous state so we only log meaningful transitions
	 * (Ready→Disconnected) rather than every retry-cycle bounce. */
	static DiscordIpc::State s_lastState = DiscordIpc::State::Disconnected;
	QObject::connect(g_ipc.data(), &DiscordIpc::stateChanged, g_ipc.data(),
					 [](DiscordIpc::State s) {
						 if (!g_ctx)
							 return;
						 if (s == DiscordIpc::State::Ready) {
							 MMCO_LOG(g_ctx, "DiscordRPC: connected.");
							 if (g_launchEpoch > 0)
								 publish_playing_presence();
							 else
								 publish_idle_presence();
						 } else if (s == DiscordIpc::State::Disconnected &&
									s_lastState == DiscordIpc::State::Ready) {
							 /* Only log the moment we *lose* an established
							  * connection, not the silent retry-loop bounces
							  * while Discord is offline. */
							 MMCO_DBG(g_ctx,
									  "DiscordRPC: disconnected; will retry.");
						 }
						 s_lastState = s;
					 });
	QObject::connect(g_ipc.data(), &DiscordIpc::errorOccurred, g_ipc.data(),
					 [](const QString& msg) {
						 if (g_ctx)
							 MMCO_DBG(g_ctx, QStringLiteral("DiscordRPC: %1")
												 .arg(msg)
												 .toUtf8()
												 .constData());
					 });

	g_ipc->connectToDiscord();

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_instance_pre_launch, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
					   on_instance_post_launch, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_SHUTDOWN,
					   on_app_shutdown, nullptr);

	MMCO_LOG(ctx, "DiscordRPC initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "DiscordRPC unloading.");
	if (g_ipc) {
		g_ipc->clearActivity();
		g_ipc->disconnectFromDiscord();
		g_ipc->deleteLater();
		g_ipc.clear();
	}
	/* The S23 instance_running_register callbacks are owned by
	 * PluginManager — it severs every registration for this module
	 * automatically during the unload sweep, so there is nothing for
	 * us to do here. */
	g_activeInstance.clear();
	g_activeMcVersion.clear();
	g_activeInstanceId.clear();
	g_launchEpoch = 0;
	g_idleEpoch = 0;
	g_ctx = nullptr;
}

} /* extern "C" */
