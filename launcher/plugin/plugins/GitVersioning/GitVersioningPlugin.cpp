/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitVersioningPlugin — MMCO entry point.
 *
 * Hooks:
 *   APP_INITIALIZED               — register settings + verify git is
 *                                   reachable.
 *   GLOBAL_SETTINGS_ABOUT_TO_OPEN — inject the global auto-snapshot
 *                                   checkbox into the MeshMC page.
 *   INSTANCE_SETTINGS_PAGE_*      — (ABI 3) inject a per-instance
 *                                   override of the auto-snapshot
 *                                   setting, persisted via S24.
 *   UI_INSTANCE_PAGES             — inject a "Version History" page per
 *                                   instance.
 *   INSTANCE_PRE_LAUNCH           — auto-snapshot the instance if the
 *                                   effective (per-instance / global)
 *                                   setting is on.
 *
 * Settings model (ABI 3 / S24):
 *   - SETTING_AUTO_SNAPSHOT is a normal app setting AND an
 *     instance-overridable setting, gated by SETTING_INSTANCE_OVERRIDE.
 *   - At launch we read the *effective* per-instance value, which the
 *     host resolves from the override gate + global fallback.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "GitRepo.h"
#include "GitVersioningPage.h"
#include <QHash>

MMCO_DEFINE_MODULE("GitVersioning", "1.0.0", "Project Tick",
				   "Track instance changes as Git commits — snapshot, restore, "
				   "per-file history, tags.",
				   "Apache-2.0");

static MMCOContext* g_ctx = nullptr;
static constexpr const char SETTING_AUTO_SNAPSHOT[] =
	"plugin.git_versioning.AutoSnapshotBeforeLaunch";
static constexpr const char SETTING_INSTANCE_OVERRIDE[] =
	"plugin.git_versioning.override";
static QObject* g_guard = nullptr;
static QCheckBox* g_checkbox = nullptr;
static bool g_gitAvailable = false;

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
	return parseBool(g_ctx->instance_setting_get(
		g_ctx->module_handle, instanceId, SETTING_AUTO_SNAPSHOT));
}

static void injectCheckboxIntoMeshMCPage()
{
	QWidget* meshMCPage = nullptr;
	for (auto* w : qApp->allWidgets()) {
		if (w->objectName() == QStringLiteral("MeshMCPage")) {
			meshMCPage = w;
			break;
		}
	}
	if (!meshMCPage)
		return;

	auto* layout =
		meshMCPage->findChild<QVBoxLayout*>(QStringLiteral("verticalLayout_9"));
	if (!layout)
		return;

	auto* groupBox = new QGroupBox(QObject::tr("Git Versioning"));
	groupBox->setObjectName(QStringLiteral("gitVersioningGroupBox"));
	auto* gl = new QVBoxLayout(groupBox);

	g_checkbox = new QCheckBox(
		QObject::tr("Auto-snapshot instance state before every launch"),
		groupBox);
	g_checkbox->setObjectName(QStringLiteral("gitAutoSnapshotCheck"));
	g_checkbox->setToolTip(
		QObject::tr("Commit any pending changes to the instance's Git history "
					"right before the JVM starts. The history lives in the "
					"instance's .history/ directory."));
	gl->addWidget(g_checkbox);

	if (!g_gitAvailable) {
		auto* warn = new QLabel(QObject::tr(
			"<i>git is not installed — version history disabled.</i>"));
		warn->setStyleSheet(QStringLiteral("color: #cc6666;"));
		gl->addWidget(warn);
		g_checkbox->setEnabled(false);
	}

	int spacerIdx = layout->count() - 1;
	layout->insertWidget(spacerIdx, groupBox);

	g_checkbox->setChecked(autoSnapshotEnabled());

	QObject::connect(g_checkbox, &QCheckBox::toggled, g_guard,
					 [](bool checked) {
						 if (g_ctx)
							 g_ctx->app_setting_set(g_ctx->module_handle,
													SETTING_AUTO_SNAPSHOT,
													checked ? "1" : "0");
					 });
}

/* ---- ABI 3 per-instance settings page (S24) ----------------------- */

/* Per-page widget bag, keyed by the page QWidget* so the LOADED /
 * APPLYING hooks can find it again. Freed when the page is destroyed. */
struct InstancePageWidgets {
	QGroupBox* groupBox = nullptr;
	QCheckBox* autoSnap = nullptr;
	QByteArray instanceId; /* copied — survives the signal storm */
};
static QHash<QWidget*, InstancePageWidgets*> g_pageWidgets;

static void syncInstanceWidgets(InstancePageWidgets* w)
{
	if (!w || !g_ctx)
		return;
	const char* id = w->instanceId.constData();
	const bool overrideEnabled =
		g_ctx->instance_setting_contains(g_ctx->module_handle, id,
										 SETTING_INSTANCE_OVERRIDE) &&
		parseBool(g_ctx->instance_setting_get(g_ctx->module_handle, id,
											  SETTING_INSTANCE_OVERRIDE));
	w->groupBox->setChecked(overrideEnabled);
	w->autoSnap->setChecked(autoSnapshotEnabledForInstance(id) &&
							w->autoSnap->isEnabled());
}

static void applyInstanceWidgets(InstancePageWidgets* w)
{
	if (!w || !g_ctx)
		return;
	const char* id = w->instanceId.constData();
	ensureInstanceSettingsRegistered(id);
	const bool overrideEnabled = w->groupBox->isChecked();
	g_ctx->instance_setting_set(g_ctx->module_handle, id,
								SETTING_INSTANCE_OVERRIDE,
								overrideEnabled ? "1" : "0");
	if (overrideEnabled) {
		g_ctx->instance_setting_set(g_ctx->module_handle, id,
									SETTING_AUTO_SNAPSHOT,
									w->autoSnap->isChecked() ? "1" : "0");
	} else {
		g_ctx->instance_setting_reset(g_ctx->module_handle, id,
									  SETTING_AUTO_SNAPSHOT);
	}
}

static void injectGroupIntoInstanceSettingsPage(QWidget* page,
												const char* instanceId)
{
	if (!page || !instanceId)
		return;

	ensureInstanceSettingsRegistered(instanceId);

	auto* layout =
		page->findChild<QVBoxLayout*>(QStringLiteral("verticalLayout_8"));
	if (!layout)
		return;

	if (page->findChild<QGroupBox*>(
			QStringLiteral("gitVersioningInstanceGroupBox")))
		return;

	auto* groupBox = new QGroupBox(
		QObject::tr("Override global Git Versioning settings"), page);
	groupBox->setObjectName(QStringLiteral("gitVersioningInstanceGroupBox"));
	groupBox->setCheckable(true);
	auto* gl = new QVBoxLayout(groupBox);

	auto* autoSnap = new QCheckBox(
		QObject::tr("Auto-snapshot instance state before every launch"),
		groupBox);
	autoSnap->setObjectName(QStringLiteral("gitInstanceAutoSnapshotCheck"));
	autoSnap->setToolTip(
		QObject::tr("Commit any pending changes to this instance's Git "
					"history right before the JVM starts. The history lives "
					"in the instance's .history/ directory."));
	autoSnap->setEnabled(g_gitAvailable);
	gl->addWidget(autoSnap);

	if (!g_gitAvailable) {
		auto* warn = new QLabel(
			QObject::tr("git is not installed — version history disabled."),
			groupBox);
		warn->setStyleSheet(QStringLiteral("color: #cc6666;"));
		gl->addWidget(warn);
	}

	int spacerIdx = layout->count() - 1;
	layout->insertWidget(spacerIdx, groupBox);

	auto* bag = new InstancePageWidgets;
	bag->groupBox = groupBox;
	bag->autoSnap = autoSnap;
	bag->instanceId = QByteArray(instanceId);
	g_pageWidgets.insert(page, bag);

	QObject::connect(page, &QObject::destroyed, qApp, [page]() {
		auto it = g_pageWidgets.find(page);
		if (it != g_pageWidgets.end()) {
			delete it.value();
			g_pageWidgets.erase(it);
		}
	});

	syncInstanceWidgets(bag);
}

static int on_app_initialized(void*, uint32_t, void*, void*)
{
	g_gitAvailable = GitRepo::gitAvailable();
	if (g_gitAvailable && g_ctx) {
		QByteArray msg = "git detected: " + GitRepo::gitVersion().toUtf8();
		MMCO_LOG(g_ctx, msg.constData());
	} else if (g_ctx) {
		MMCO_WARN(g_ctx, "system git not found — GitVersioning will run in "
						 "read-only mode (instance page still visible but "
						 "every operation will fail gracefully).");
	}

	g_guard = new QObject();
	return 0;
}

/* MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN handler — replaces the
 * legacy direct connect to Application::globalSettingsAboutToOpen. */
static int on_global_settings_about_to_open(void*, uint32_t, void*, void*)
{
	g_checkbox = nullptr;
	QTimer::singleShot(0, qApp, injectCheckboxIntoMeshMCPage);
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

static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
	auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
	if (!evt || !evt->page_list_handle || !evt->instance_id)
		return 0;

	auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);

	const QString instId = QString::fromUtf8(evt->instance_id);
	const QString instRoot =
		evt->instance_path ? QString::fromUtf8(evt->instance_path) : QString();
	pages->append(new GitVersioningPage(g_ctx, instId, instRoot));
	return 0;
}

/* MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED — inject our override group
 * box into the just-built per-instance settings page. */
static int on_instance_settings_page_created(void*, uint32_t, void* payload,
											 void*)
{
	auto* evt = static_cast<MMCOInstanceSettingsPageEvent*>(payload);
	if (!evt || !evt->page_handle || !evt->instance_id)
		return 0;
	injectGroupIntoInstanceSettingsPage(static_cast<QWidget*>(evt->page_handle),
										evt->instance_id);
	return 0;
}

/* MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED — page refreshed its values
 * from the backing store; mirror them into our widgets. */
static int on_instance_settings_page_loaded(void*, uint32_t, void* payload,
											void*)
{
	auto* evt = static_cast<MMCOInstanceSettingsPageEvent*>(payload);
	if (!evt || !evt->page_handle)
		return 0;
	auto it = g_pageWidgets.find(static_cast<QWidget*>(evt->page_handle));
	if (it != g_pageWidgets.end())
		syncInstanceWidgets(it.value());
	return 0;
}

/* MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING — page is about to commit;
 * push our widgets back into the instance settings via S24. */
static int on_instance_settings_page_applying(void*, uint32_t, void* payload,
											  void*)
{
	auto* evt = static_cast<MMCOInstanceSettingsPageEvent*>(payload);
	if (!evt || !evt->page_handle)
		return 0;
	auto it = g_pageWidgets.find(static_cast<QWidget*>(evt->page_handle));
	if (it != g_pageWidgets.end())
		applyInstanceWidgets(it.value());
	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "GitVersioning initialising…");

	ensureSettingRegistered();

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
					   on_app_initialized, nullptr);
	ctx->hook_register(ctx->module_handle,
					   MMCO_HOOK_GLOBAL_SETTINGS_ABOUT_TO_OPEN,
					   on_global_settings_about_to_open, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_PRE_LAUNCH,
					   on_pre_launch, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
					   on_instance_pages, nullptr);
	ctx->hook_register(ctx->module_handle,
					   MMCO_HOOK_INSTANCE_SETTINGS_PAGE_CREATED,
					   on_instance_settings_page_created, nullptr);
	ctx->hook_register(ctx->module_handle,
					   MMCO_HOOK_INSTANCE_SETTINGS_PAGE_LOADED,
					   on_instance_settings_page_loaded, nullptr);
	ctx->hook_register(ctx->module_handle,
					   MMCO_HOOK_INSTANCE_SETTINGS_PAGE_APPLYING,
					   on_instance_settings_page_applying, nullptr);

	MMCO_LOG(ctx, "GitVersioning ready.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "GitVersioning unloading.");
	qDeleteAll(g_pageWidgets);
	g_pageWidgets.clear();
	g_ctx = nullptr;
	g_checkbox = nullptr;
	g_guard = nullptr;
}

} /* extern "C" */
