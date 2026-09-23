/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitVersioningPlugin — MMCO entry point.
 *
 * ABI 5 — declarative UI surfaces (S33). Every widget this plugin shows
 * is now described as an "mmco-ui/1" JSON document handed to
 * ui_surface_create; the host renders and owns the actual widget tree.
 * There is no QWidget/BasePage/allWidgets()/findChild anywhere in this
 * plugin any more.
 *
 * Hooks:
 *   APP_INITIALIZED       — log whether system git was found (the
 *                           availability check itself already ran, in
 *                           mmco_init(), so the global-settings surface
 *                           reflects it from the very first paint).
 *   UI_MAIN_READY         — instances loaded by the host before this
 *                           plugin's mmco_init() ran (i.e. every
 *                           instance that already existed at launch)
 *                           get their per-instance surfaces created
 *                           here, once the instance list is known to be
 *                           populated (see SystemTray's identical
 *                           reasoning for using this hook instead of
 *                           mmco_init()).
 *   INSTANCE_CREATED      — create the two per-instance surfaces
 *                           (INSTANCE_SETTINGS override group +
 *                           INSTANCE_PAGE "Version History") for a
 *                           newly added instance.
 *   INSTANCE_REMOVED      — destroy both, freeing the plugin-side
 *                           GitVersioningPageController + GitRepo.
 *   UI_INSTANCE_PAGES     — fires every time an instance window's page
 *                           list is (re)built, *before*
 *                           PluginManager::createInstancePages() reads
 *                           our surface's document. We don't append
 *                           anything to page_list_handle any more (that
 *                           was the pre-ABI-5 mechanism) — we just reuse
 *                           this moment to refresh the git history, so
 *                           the page is as current as the old
 *                           reconstruct-a-fresh-BasePage-per-open model
 *                           used to be.
 *   INSTANCE_PRE_LAUNCH   — auto-snapshot the instance if the effective
 *                           (per-instance / global) setting is on.
 *                           Unchanged.
 *
 * Settings model (ABI 3 / S24, unchanged by ABI 5):
 *   - SETTING_AUTO_SNAPSHOT is a normal app setting AND an
 *     instance-overridable setting, gated by SETTING_INSTANCE_OVERRIDE.
 *   - At launch we read the *effective* per-instance value, which the
 *     host resolves from the override gate + global fallback.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "GitRepo.h"
#include "GitVersioningPage.h"
#include <QHash>
#include <cstring>
#include <memory>

MMCO_DEFINE_MODULE("GitVersioning", "1.0.0", "Project Tick",
				   "Track instance changes as Git commits — snapshot, restore, "
				   "per-file history, tags.",
				   "Apache-2.0");

static MMCOContext* g_ctx = nullptr;
static constexpr const char SETTING_AUTO_SNAPSHOT[] =
	"plugin.git_versioning.AutoSnapshotBeforeLaunch";
static constexpr const char SETTING_INSTANCE_OVERRIDE[] =
	"plugin.git_versioning.override";
static bool g_gitAvailable = false;
static void* g_globalSettingsSurface = nullptr;

/* Everything this plugin shows for one instance: the INSTANCE_SETTINGS
 * override-group surface (owned directly, it's just two toggles) and the
 * INSTANCE_PAGE "Version History" surface (owned via the controller,
 * which also carries the GitRepo + cached commit list). */
struct InstanceUi {
	QString instanceId;
	QString instanceRoot;
	void* settingsSurface = nullptr;
	std::unique_ptr<GitVersioningPageController> page;
};
/* QHash is implicitly shared (copy-on-write), so its value type must stay
 * copyable even with a single owner — a unique_ptr value breaks that.
 * Store raw owning pointers instead, same as the pre-ABI-5 code's
 * QHash<QWidget*, InstancePageWidgets*> did. */
static QHash<QString, InstanceUi*> g_instances;

static bool parseBool(const char* v)
{
	if (!v)
		return false;
	QString s = QString::fromUtf8(v).trimmed().toLower();
	return s == QLatin1String("1") || s == QLatin1String("true") ||
		   s == QLatin1String("yes") || s == QLatin1String("on");
}

/* Global auto-snapshot setting (used as the fallback when an instance
 * has no override). */
static bool autoSnapshotEnabled()
{
	if (!g_ctx)
		return false;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle,
									 SETTING_AUTO_SNAPSHOT))
		return false;
	return parseBool(
		g_ctx->app_setting_get(g_ctx->module_handle, SETTING_AUTO_SNAPSHOT));
}

static void ensureSettingRegistered()
{
	if (!g_ctx)
		return;
	if (!g_ctx->app_setting_contains(g_ctx->module_handle,
									 SETTING_AUTO_SNAPSHOT))
		g_ctx->app_setting_register(g_ctx->module_handle, SETTING_AUTO_SNAPSHOT,
									"0");
}

/* Register the per-instance override gate and bind the auto-snapshot
 * setting to it. Idempotent; safe to call repeatedly. */
static void ensureInstanceSettingsRegistered(const char* instanceId)
{
	if (!g_ctx || !instanceId)
		return;

	ensureSettingRegistered();

	if (!g_ctx->instance_setting_contains(g_ctx->module_handle, instanceId,
										  SETTING_INSTANCE_OVERRIDE)) {
		g_ctx->instance_setting_register(g_ctx->module_handle, instanceId,
										 SETTING_INSTANCE_OVERRIDE, "0");
	}
	g_ctx->instance_setting_register_override(g_ctx->module_handle, instanceId,
											  SETTING_AUTO_SNAPSHOT,
											  SETTING_INSTANCE_OVERRIDE);
}

static bool overrideEnabledFor(const char* instanceId)
{
	if (!g_ctx || !instanceId)
		return false;
	return g_ctx->instance_setting_contains(g_ctx->module_handle, instanceId,
											SETTING_INSTANCE_OVERRIDE) &&
		   parseBool(g_ctx->instance_setting_get(g_ctx->module_handle, instanceId,
												 SETTING_INSTANCE_OVERRIDE));
}

/* Effective auto-snapshot value for a given instance: the host resolves
 * the override gate + global fallback for us via instance_setting_get. */
static bool autoSnapshotEnabledForInstance(const char* instanceId)
{
	if (!g_ctx || !instanceId)
		return autoSnapshotEnabled();
	ensureInstanceSettingsRegistered(instanceId);
	if (!g_ctx->instance_setting_contains(g_ctx->module_handle, instanceId,
										  SETTING_AUTO_SNAPSHOT))
		return autoSnapshotEnabled();
	return parseBool(g_ctx->instance_setting_get(g_ctx->module_handle, instanceId,
												 SETTING_AUTO_SNAPSHOT));
}

/* ---- Global settings surface (replaces injectCheckboxIntoMeshMCPage) -- */

static QByteArray buildGlobalSettingsDoc()
{
	QJsonArray children;
	children.append(QJsonObject{
		{"type", "toggle"},
		{"id", "auto_snapshot"},
		{"props",
		 QJsonObject{
			 {"label", "Auto-snapshot instance state before every launch"},
			 {"value", autoSnapshotEnabled()},
			 {"enabled", g_gitAvailable}}}});
	if (!g_gitAvailable) {
		children.append(QJsonObject{
			{"type", "text"},
			{"id", "git_warning"},
			{"props",
			 QJsonObject{
				 {"text", "git is not installed — version history disabled."}}}});
	}
	const QJsonObject doc{
		{"type", "mmco-ui/1"},
		{"root",
		 QJsonObject{{"type", "column"}, {"id", "root"}, {"children", children}}}};
	return QJsonDocument(doc).toJson(QJsonDocument::Compact);
}

static void on_global_settings_event(void*, const char*, const char* node_id,
									 const char* event, const char* value_json)
{
	if (!g_ctx || !node_id || !event)
		return;
	if (std::strcmp(node_id, "auto_snapshot") != 0)
		return;
	if (std::strcmp(event, "change") != 0)
		return;
	const bool checked = value_json && std::strcmp(value_json, "true") == 0;
	g_ctx->app_setting_set(g_ctx->module_handle, SETTING_AUTO_SNAPSHOT,
						   checked ? "1" : "0");
}

static void createGlobalSettingsSurface()
{
	if (!g_ctx)
		return;
	const QByteArray json = buildGlobalSettingsDoc();
	g_globalSettingsSurface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_GLOBAL_SETTINGS, nullptr,
		"Git Versioning", "git-scm", json.constData(), on_global_settings_event,
		nullptr);
}

/* ---- Per-instance settings override surface (replaces
 * injectGroupIntoInstanceSettingsPage) -------------------------------- */

static QByteArray buildInstanceSettingsDoc(const char* instanceId)
{
	const bool overrideEnabled = overrideEnabledFor(instanceId);
	const bool autoSnap = autoSnapshotEnabledForInstance(instanceId);

	QJsonArray children;
	children.append(QJsonObject{
		{"type", "toggle"},
		{"id", "override_enabled"},
		{"props",
		 QJsonObject{{"label", "Override global Git Versioning settings"},
					{"value", overrideEnabled},
					{"enabled", g_gitAvailable}}}});
	children.append(QJsonObject{
		{"type", "toggle"},
		{"id", "auto_snapshot"},
		{"props",
		 QJsonObject{
			 {"label", "Auto-snapshot instance state before every launch"},
			 {"value", autoSnap},
			 {"enabled", g_gitAvailable && overrideEnabled}}}});
	if (!g_gitAvailable) {
		children.append(QJsonObject{
			{"type", "text"},
			{"id", "git_warning"},
			{"props",
			 QJsonObject{
				 {"text", "git is not installed — version history disabled."}}}});
	}
	const QJsonObject doc{
		{"type", "mmco-ui/1"},
		{"root",
		 QJsonObject{{"type", "column"}, {"id", "root"}, {"children", children}}}};
	return QJsonDocument(doc).toJson(QJsonDocument::Compact);
}

static void on_instance_settings_event(void* user_data, const char*,
									   const char* node_id, const char* event,
									   const char* value_json)
{
	auto* rec = static_cast<InstanceUi*>(user_data);
	if (!g_ctx || !rec || !node_id || !event)
		return;
	if (std::strcmp(event, "change") != 0)
		return;

	const QByteArray idUtf8 = rec->instanceId.toUtf8();
	const bool checked = value_json && std::strcmp(value_json, "true") == 0;

	if (std::strcmp(node_id, "override_enabled") == 0) {
		g_ctx->instance_setting_set(g_ctx->module_handle, idUtf8.constData(),
									SETTING_INSTANCE_OVERRIDE,
									checked ? "1" : "0");
		if (!checked) {
			g_ctx->instance_setting_reset(g_ctx->module_handle, idUtf8.constData(),
										  SETTING_AUTO_SNAPSHOT);
		}
		/* Keep the dependent toggle's enabled/value props in sync so the
		 * canonical document (what the next page-open renders from) never
		 * drifts from what's on screen right now. */
		const QJsonObject patch{
			{"enabled", g_gitAvailable && checked},
			{"value", autoSnapshotEnabledForInstance(idUtf8.constData())}};
		const QByteArray patchJson =
			QJsonDocument(patch).toJson(QJsonDocument::Compact);
		g_ctx->ui_surface_set(g_ctx->module_handle, rec->settingsSurface,
							  "auto_snapshot", patchJson.constData());
	} else if (std::strcmp(node_id, "auto_snapshot") == 0) {
		if (!overrideEnabledFor(idUtf8.constData()))
			return; /* toggle should be disabled in this state; ignore stray events */
		g_ctx->instance_setting_set(g_ctx->module_handle, idUtf8.constData(),
									SETTING_AUTO_SNAPSHOT, checked ? "1" : "0");
	}
}

/* ---- Per-instance lifecycle ----------------------------------------- */

static void createInstanceUi(const QString& instanceId, const QString& instanceRoot)
{
	if (!g_ctx || instanceId.isEmpty() || g_instances.contains(instanceId))
		return;

	const QByteArray idUtf8 = instanceId.toUtf8();
	ensureInstanceSettingsRegistered(idUtf8.constData());

	auto* rec = new InstanceUi();
	rec->instanceId = instanceId;
	rec->instanceRoot = instanceRoot;

	const QByteArray doc = buildInstanceSettingsDoc(idUtf8.constData());
	rec->settingsSurface = g_ctx->ui_surface_create(
		g_ctx->module_handle, MMCO_UI_ANCHOR_INSTANCE_SETTINGS,
		idUtf8.constData(), "Git Versioning", nullptr, doc.constData(),
		on_instance_settings_event, rec);

	rec->page = std::make_unique<GitVersioningPageController>(g_ctx, instanceId,
															   instanceRoot);
	rec->page->createSurface();

	g_instances.insert(instanceId, rec);
}

static void destroyInstanceUi(const QString& instanceId)
{
	auto it = g_instances.find(instanceId);
	if (it == g_instances.end())
		return;
	InstanceUi* rec = it.value();
	if (g_ctx && rec->settingsSurface)
		g_ctx->ui_surface_destroy(g_ctx->module_handle, rec->settingsSurface);
	if (rec->page)
		rec->page->destroySurface();
	g_instances.erase(it);
	delete rec;
}

/* ---- Hooks ------------------------------------------------------------ */

static int on_app_initialized(void*, uint32_t, void*, void*)
{
	if (!g_ctx)
		return 0;
	if (g_gitAvailable) {
		QByteArray msg = "git detected: " + GitRepo::gitVersion().toUtf8();
		MMCO_LOG(g_ctx, msg.constData());
	} else {
		MMCO_WARN(g_ctx, "system git not found — GitVersioning will run in "
						 "read-only mode (instance page still visible but "
						 "every operation will fail gracefully).");
	}
	return 0;
}

/* Instances that already existed when this module loaded aren't known
 * until the instance list has actually been populated — mirrors
 * SystemTray's identical reasoning for building its tray menu here
 * instead of in mmco_init(). */
static int on_ui_main_ready(void*, uint32_t, void*, void*)
{
	if (!g_ctx)
		return 0;
	const int total = g_ctx->instance_count(g_ctx->module_handle);
	for (int i = 0; i < total; ++i) {
		/* Copy before the next call: the host returns strings in one
		 * per-module buffer, which instance_get_path() overwrites. */
		const char* rawId = g_ctx->instance_get_id(g_ctx->module_handle, i);
		if (!rawId)
			continue;
		const QByteArray id(rawId);
		const char* path =
			g_ctx->instance_get_path(g_ctx->module_handle, id.constData());
		createInstanceUi(QString::fromUtf8(id),
						 path ? QString::fromUtf8(path) : QString());
	}
	return 0;
}

static int on_instance_created(void*, uint32_t, void* payload, void*)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info || !info->instance_id)
		return 0;
	createInstanceUi(QString::fromUtf8(info->instance_id),
					 info->instance_path ? QString::fromUtf8(info->instance_path)
										  : QString());
	return 0;
}

static int on_instance_removed(void*, uint32_t, void* payload, void*)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info || !info->instance_id)
		return 0;
	destroyInstanceUi(QString::fromUtf8(info->instance_id));
	return 0;
}

/* ABI 5: no BasePage is appended here any more — the INSTANCE_PAGE
 * surface itself is what PluginManager::createInstancePages() renders,
 * reading whatever document our controller last pushed. We reuse this
 * "page list about to be (re)built" moment (fired once per instance
 * window open, *before* createInstancePages() runs — see
 * InstancePageProvider::getPages()) to refresh the git history one more
 * time, so the page is as current as the old per-open BasePage
 * reconstruction used to be. */
static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
	auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
	if (!evt || !evt->instance_id)
		return 0;
	auto it = g_instances.find(QString::fromUtf8(evt->instance_id));
	if (it != g_instances.end() && it.value()->page)
		it.value()->page->reloadHistory();
	return 0;
}

static int on_pre_launch(void*, uint32_t, void* payload, void*)
{
	if (!g_ctx || !payload || !g_gitAvailable)
		return 0;

	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info->instance_id || !info->instance_path)
		return 0;

	/* Honour the per-instance override (with global fallback). */
	if (!autoSnapshotEnabledForInstance(info->instance_id))
		return 0;

	GitRepo repo(QString::fromUtf8(info->instance_id),
				 QString::fromUtf8(info->instance_path));
	if (!repo.isInitialized()) {
		QString err;
		if (!repo.initialize(&err)) {
			QByteArray msg = "GitVersioning: init failed: " + err.toUtf8();
			MMCO_WARN(g_ctx, msg.constData());
			return 0;
		}
	}

	QString err;
	QString sha = repo.commit(
		QStringLiteral("Pre-launch snapshot — %1")
			.arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
		/*isPreLaunch=*/true, &err);
	if (sha.isEmpty() && !err.isEmpty()) {
		QByteArray msg =
			"GitVersioning: pre-launch commit failed: " + err.toUtf8();
		MMCO_WARN(g_ctx, msg.constData());
	} else if (!sha.isEmpty()) {
		QByteArray msg = "GitVersioning: snapshot " + sha.toUtf8();
		MMCO_LOG(g_ctx, msg.constData());
	}
	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "GitVersioning initialising…");

	ensureSettingRegistered();
	g_gitAvailable = GitRepo::gitAvailable();
	createGlobalSettingsSurface();

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_MAIN_READY,
					   on_ui_main_ready, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_CREATED,
					   on_instance_created, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
					   on_instance_pages, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_pre_launch, nullptr);

	MMCO_LOG(ctx, "GitVersioning ready.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "GitVersioning unloading.");
	/* Every surface this module still owns (global settings + every
	 * per-instance settings/page surface) was already torn down by
	 * PluginManager::releaseSurfacesForModule() before this call — see
	 * GitVersioningPageController::destroySurface()'s comment. We only
	 * need to free our own heap state here, never touch a surface
	 * handle again. */
	qDeleteAll(g_instances);
	g_instances.clear();
	g_ctx = nullptr;
	g_globalSettingsSurface = nullptr;
}

} /* extern "C" */
