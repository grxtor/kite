/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  DesktopNotifier: Kite MMCO plugin
 *
 *  Turns selected Kite lifecycle events into native desktop notifications via the MMCO S19 tray API.
 *  It owns one long-lived hidden tray icon so notifications survive Linux desktops that throttle
 *  freshly shown tray icons. Kite has no settings page for plugin-namespaced keys, so the toggles are
 *  edited in the config file; the defaults are sensible out of the box.
 *
 *  Settings (booleans, plus an int):
 *    notify_launch    instance starting (PRE_LAUNCH)    (default 1)
 *    notify_exit      instance stopped (POST_LAUNCH)    (default 1)
 *    notify_created   fire on INSTANCE_CREATED          (default 0)
 *    notify_removed   fire on INSTANCE_REMOVED          (default 0)
 *    notify_news      fire on NEWS_UPDATED              (default 0)
 *    notify_startup   say hello on APP_INITIALIZED      (default 0)
 *    timeout_ms       notification timeout              (default 5000)
 *
 *  Friends notices (FRIENDS_NOTICE) have no key here: the launcher's own setting
 *  (Settings > Friends) decides whether they are sent, already worded. The same for
 *  CRASH_FIX_FOUND, once per crash newer crash rules know.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include <cstdlib>

MMCO_DEFINE_MODULE("DesktopNotifier", "1.0.0", "Project Tick",
				   "Native desktop notifications for Kite events "
				   "(launch, instance lifecycle, news, friends)",
				   "BSD-3-Clause");

static MMCOContext* g_ctx = nullptr;
static void* g_tray = nullptr; /* hidden tray, only for showMessage() */

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

static int settingInt(const char* key, int fallback)
{
	if (!g_ctx)
		return fallback;
	const char* v = g_ctx->setting_get(g_ctx->module_handle, key);
	if (!v || !*v)
		return fallback;
	bool ok = false;
	int x = QString::fromUtf8(v).toInt(&ok);
	return ok ? x : fallback;
}

static void settingDefault(const char* key, const char* val)
{
	if (g_ctx && !g_ctx->setting_get(g_ctx->module_handle, key))
		g_ctx->setting_set(g_ctx->module_handle, key, val);
}

/* ── notify helper ────────────────────────────────────────────────── */

static void notify(const QString& title, const QString& msg, int iconType)
{
	if (!g_ctx)
		return;
	int timeout = settingInt("timeout_ms", 5000);
	g_ctx->tray_show_message(g_ctx->module_handle, g_tray,
							 title.toUtf8().constData(),
							 msg.toUtf8().constData(), iconType, timeout);
}

/* ── hooks ────────────────────────────────────────────────────────── */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
							  void* /*payload*/, void* /*ud*/)
{
	if (!settingBool("notify_startup", false))
		return 0;
	const char* name = g_ctx->get_app_name(g_ctx->module_handle);
	const char* ver = g_ctx->get_app_version(g_ctx->module_handle);
	notify(QStringLiteral("%1 ready")
			   .arg(QString::fromUtf8(name ? name : "Kite")),
		   QStringLiteral("Version %1 is running.")
			   .arg(QString::fromUtf8(ver ? ver : "?")),
		   1);
	return 0;
}

/* MMCO ABI 2 names its launch hooks confusingly: PRE_LAUNCH fires when the game is about to start,
 * POST_LAUNCH after it has exited. */
static int on_instance_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
								  void* payload, void* /*ud*/)
{
	if (!settingBool("notify_launch", true))
		return 0;
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info)
		return 0;
	QString name = info->instance_name ? QString::fromUtf8(info->instance_name)
									   : QStringLiteral("(unnamed)");
	QString ver = info->minecraft_version
					  ? QString::fromUtf8(info->minecraft_version)
					  : QString();
	QString body =
		ver.isEmpty()
			? QStringLiteral("Launching %1…").arg(name)
			: QStringLiteral("Launching %1 (Minecraft %2)…").arg(name, ver);
	notify(QStringLiteral("Kite instance starting"), body, 1);
	return 0;
}

/* POST_LAUNCH: the game exited. */
static int on_instance_post_launch(void* /*mh*/, uint32_t /*hook_id*/,
								   void* payload, void* /*ud*/)
{
	if (!settingBool("notify_exit", true))
		return 0;
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info)
		return 0;
	QString name = info->instance_name ? QString::fromUtf8(info->instance_name)
									   : QStringLiteral("(unnamed)");
	notify(QStringLiteral("Kite instance stopped"),
		   QStringLiteral("%1 has exited.").arg(name), 1);
	return 0;
}

static int on_instance_created(void* /*mh*/, uint32_t /*hook_id*/,
							   void* payload, void* /*ud*/)
{
	if (!settingBool("notify_created", false))
		return 0;
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	QString name = (info && info->instance_name)
					   ? QString::fromUtf8(info->instance_name)
					   : QStringLiteral("(unnamed)");
	notify(QStringLiteral("Instance created"),
		   QStringLiteral("%1 was added to the instance list.").arg(name), 1);
	return 0;
}

static int on_instance_removed(void* /*mh*/, uint32_t /*hook_id*/,
							   void* payload, void* /*ud*/)
{
	if (!settingBool("notify_removed", false))
		return 0;
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	QString name = (info && info->instance_name)
					   ? QString::fromUtf8(info->instance_name)
					   : QStringLiteral("(unnamed)");
	notify(QStringLiteral("Instance removed"),
		   QStringLiteral("%1 has been deleted.").arg(name), 2);
	return 0;
}

static int on_news_updated(void* /*mh*/, uint32_t /*hook_id*/,
						   void* /*payload*/, void* /*ud*/)
{
	if (!settingBool("notify_news", false))
		return 0;
	int count = g_ctx->news_get_entry_count(g_ctx->module_handle);
	if (count <= 0)
		return 0;
	const char* title = g_ctx->news_get_entry_title(g_ctx->module_handle, 0);
	notify(QStringLiteral("News updated"),
		   title ? QString::fromUtf8(title)
				 : QStringLiteral("%1 new news entries.").arg(count),
		   1);
	return 0;
}

/* FRIENDS_NOTICE: a friend came online, sent a request or a message; Kite words both lines. */
static int on_friends_notice(void* /*mh*/, uint32_t /*hook_id*/,
							 void* payload, void* /*ud*/)
{
	auto* notice = static_cast<MMCOFriendsNotice*>(payload);
	if (!notice || !notice->title || !notice->body)
		return 0;
	notify(QString::fromUtf8(notice->title), QString::fromUtf8(notice->body),
		   1);
	return 0;
}

/* CRASH_FIX_FOUND: newer crash rules know a crash of an instance; Kite words both lines. */
static int on_crash_fix_found(void* /*mh*/, uint32_t /*hook_id*/, void* payload,
							  void* /*ud*/)
{
	auto* notice = static_cast<MMCOCrashFixNotice*>(payload);
	if (!notice || !notice->title || !notice->body)
		return 0;
	notify(QString::fromUtf8(notice->title), QString::fromUtf8(notice->body),
		   1);
	return 0;
}

/* ── lifecycle ────────────────────────────────────────────────────── */

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "DesktopNotifier initializing...");

	/* Seed defaults exactly once. */
	settingDefault("notify_launch", "1"); /* PRE_LAUNCH: game starting   */
	settingDefault("notify_exit", "1");	  /* POST_LAUNCH: game exited    */
	settingDefault("notify_created", "0");
	settingDefault("notify_removed", "0");
	settingDefault("notify_news", "0");
	settingDefault("notify_startup", "0");
	settingDefault("timeout_ms", "5000");

	if (!ctx->tray_is_available(ctx->module_handle)) {
		MMCO_WARN(ctx,
				  "DesktopNotifier: no system tray available; notifications "
				  "will silently no-op.");
	} else {
		/* Long-lived hidden tray icon: a stable source identity for notifications on KDE / GNOME /
		 * Windows. Never shown; the host tears it down at unload. */
		g_tray = ctx->tray_create(ctx->module_handle, "dialog-information",
								  "Kite");
	}

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_instance_pre_launch, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
					   on_instance_post_launch, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_CREATED,
					   on_instance_created, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_NEWS_UPDATED,
					   on_news_updated, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_FRIENDS_NOTICE,
					   on_friends_notice, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_CRASH_FIX_FOUND,
					   on_crash_fix_found, nullptr);

	MMCO_LOG(ctx, "DesktopNotifier initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "DesktopNotifier unloading.");
	g_tray = nullptr; /* host-owned, freed by releaseTrayResourcesForModule */
	g_ctx = nullptr;
}

} /* extern "C" */
