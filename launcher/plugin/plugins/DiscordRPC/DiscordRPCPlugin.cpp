/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: MS-PL
 *
 *  DiscordRPC: Kite MMCO plugin
 *
 *  Publishes a Discord Rich Presence activity: "Browsing instances" in the launcher, "Playing <Instance>"
 *  while a game runs; cleared on instance exit and app shutdown. Talks to the local IPC socket through
 *  the vendored discord_ipc client (no outbound network traffic) and retries silently if Discord is not
 *  running.
 *
 *  Settings (stored under the plugin's namespace):
 *    enabled             master switch                 (default 1)
 *    show_instance_name  include the instance name     (default 1)
 *    show_mc_version     include the MC version        (default 1)
 *    custom_details      override the "details" line   (default "")
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

/* Kite's application in the Discord Developer Portal. Its name is the
 * "Playing …" line; the "kite" art asset is uploaded there. */
static constexpr const char* DISCORD_APP_ID = "1566189846161260634";
static constexpr const char* LARGE_IMAGE_KEY = "kite";

/* ── module state ─────────────────────────────────────────────────── */

static MMCOContext* g_ctx = nullptr;
static QPointer<DiscordIpc> g_ipc;
static qint64 g_launchEpoch = 0; /* unix sec; 0 = idle  (game session start) */
static qint64 g_idleEpoch = 0; /* unix sec;          (launcher session start) */
static QString g_activeInstance;
static QString g_activeMcVersion;
static QString g_activeInstanceId; /* stable id used to disconnect later  */

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
	/* Set the launcher-session timestamp once so Discord's elapsed counter does not reset each time
	 * we return to idle. */
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

/* Hook semantics (ABI 2): INSTANCE_PRE_LAUNCH fires just before the game spawns.
 * INSTANCE_POST_LAUNCH fires after the JVM exited (LaunchTask::succeeded), so despite its name it
 * means the game EXITED. Flow: PRE_LAUNCH shows "Playing X (MC Y.Z)" and registers the
 * running-state callback; POST_LAUNCH returns to "In launcher"; the running true/false callback is
 * a fallback for both edges. */

/* Delivered through instance_running_register: if the running edge is for the instance tracked in
 * Discord, refresh or clear the presence. */
static void on_instance_running(void* /*ud*/, const char* instance_id,
								int running)
{
	if (!g_ipc || !instance_id)
		return;
	const QString id = QString::fromUtf8(instance_id);
	if (running) {
		/* Game started: refresh the playing presence in case the MC version was not resolved yet at
		 * PRE_LAUNCH. */
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
		/* Game exited: go idle, but only if it was this instance we were tracking. */
		if (id == g_activeInstanceId) {
			g_activeInstance.clear();
			g_activeMcVersion.clear();
			g_activeInstanceId.clear();
			g_launchEpoch = 0;
			publish_idle_presence();
		}
	}
}

/* Asks PluginManager (S23) to deliver runningStatusChanged transitions for the instance to
 * on_instance_running; re-registering replaces the previous callback. */
static void hook_instance_running_signal(const QString& instanceId)
{
	if (!g_ctx)
		return;

	/* Capture the MC version (the net.minecraft component version). */
	const char* mc = g_ctx->instance_get_mc_version(
		g_ctx->module_handle, instanceId.toUtf8().constData());
	if (mc && *mc)
		g_activeMcVersion = QString::fromUtf8(mc);

	g_ctx->instance_running_register(g_ctx->module_handle,
									 instanceId.toUtf8().constData(),
									 &on_instance_running, nullptr);
}

/* PRE_LAUNCH: the game is about to start; begin showing "Playing X". */
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

/* POST_LAUNCH: the game has exited. Reset to the idle line; the running=false callback does much
 * the same, but this is a deterministic moment that does not wait for Qt's signal queue. */
static int on_instance_post_launch(void* /*mh*/, uint32_t /*hook_id*/,
								   void* payload, void* /*ud*/)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	const QString exitedId = (info && info->instance_id)
								 ? QString::fromUtf8(info->instance_id)
								 : QString();
	/* A different instance launched in the meantime (B while A was shutting down) keeps its
	 * presence. */
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

	/* Track the previous state so only meaningful transitions (Ready to Disconnected) are logged,
	 * not retry bounces. */
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
							 /* Log only the moment an established connection is lost, not the
							  * retry-loop bounces while Discord is offline. */
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
	/* instance_running_register callbacks are severed by PluginManager's unload sweep; nothing to
	 * do here. */
	g_activeInstance.clear();
	g_activeMcVersion.clear();
	g_activeInstanceId.clear();
	g_launchEpoch = 0;
	g_idleEpoch = 0;
	g_ctx = nullptr;
}

} /* extern "C" */
