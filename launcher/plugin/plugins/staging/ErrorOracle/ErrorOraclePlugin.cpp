/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * ErrorOraclePlugin — MMCO entry point. Loads the built-in rule pack
 * + any user rules at init, exposes a per-instance "Error Analysis"
 * page, and on every INSTANCE_POST_LAUNCH with a crash, automatically
 * pops a system-tray notification telling the user "ErrorOracle found
 * %d suggestion(s) for the last crash."
 *
 * ABI 5 — declarative UI surfaces (plugin-abi5-spec.md §4 step 3).
 * The "Error Analysis" page is now an mmco-ui/1 document handed to
 * ui_surface_create; the host renders and owns the actual widget
 * tree. There is no QWidget/BasePage/page_list_handle append anywhere
 * in this plugin any more — migrated the same way GitVersioning's
 * instance page was migrated in commit aa2c3e9c (see
 * GitVersioning/GitVersioningPlugin.cpp for the reference shape):
 *   UI_MAIN_READY     — instances loaded by the host before this
 *                       plugin's mmco_init() ran get their surface
 *                       created here.
 *   INSTANCE_CREATED  — create the surface for a newly added instance.
 *   INSTANCE_REMOVED  — destroy it, freeing the AnalysisPageController.
 *   UI_INSTANCE_PAGES — fires every time an instance window's page
 *                       list is (re)built, *before*
 *                       PluginManager::createInstancePages() reads
 *                       our surface's document. We don't append
 *                       anything to page_list_handle any more (that
 *                       was the pre-ABI-5 mechanism) — we just reuse
 *                       this moment to re-run the analysis, so the
 *                       page is as current as the old
 *                       reconstruct-a-fresh-BasePage-per-open model
 *                       used to be.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "RuleEngine.h"
#include "LearningStore.h"
#include "LogIngester.h"
#include "AnalysisPage.h"

/* QCoreApplication::applicationDirPath() (builtInRulesDir(), below) used
 * to come in transitively via the SDK header's <QApplication>; ABI 5
 * dropped that include (see mmco_cxx_sdk.hpp), so this plugin now
 * includes the QtCore-only QCoreApplication header itself. */
#include <QCoreApplication>
#include <QStandardPaths>

MMCO_DEFINE_MODULE("ErrorOracle", "1.0.0", "Project Tick",
				   "LLM-free crash/log analyser with a learning feedback loop.",
				   "Apache-2.0");

namespace
{
	MMCOContext* g_ctx = nullptr;
	RuleEngine* g_engine = nullptr;
	LearningStore* g_learning = nullptr;
	/* Raw owning pointers, not unique_ptr: QHash is implicitly shared
	 * (copy-on-write), so its value type must stay copyable even with
	 * a single owner — same reasoning as GitVersioningPlugin.cpp's
	 * QHash<QString, InstanceUi*>. */
	QHash<QString, AnalysisPageController*> g_instances;

	QString builtInRulesDir()
	{
		// Search order (first directory that exists wins):
		//   1) $MESHMC_ERROR_ORACLE_RULES                (dev override)
		//   2) macOS: Contents/Resources/MMCOPluginData/
		//             ErrorOracle/rules                  (current install)
		//   3) macOS: Contents/PlugIns/mmcmodules/
		//             ErrorOracle/rules                  (legacy fallback)
		//   4) <appDir>/mmcmodules/ErrorOracle/rules     (Linux/Windows
		//   install) 5) <appDir>/ErrorOracle/rules                (build-tree
		//   staging)
		//
		// Rule packs ship as plain JSON. On macOS that means they
		// must live under Contents/Resources/ — Apple's codesign
		// strict pass treats any non-Mach-O file it finds inside
		// Contents/PlugIns/ as an unsigned subcomponent of the
		// main executable and aborts with
		//   "code object is not signed at all
		//    In subcomponent:
		//    .../PlugIns/mmcmodules/ErrorOracle/rules/jvm.json"
		// Files under Resources/ are hashed opaquely via
		// CodeResources, which is what we want.
		// See MMCO_PLUGIN_DATA_DEST_DIR in the top-level
		// CMakeLists.txt for the install-side counterpart.
		QStringList candidates;
#ifdef Q_OS_MAC
		// applicationDirPath() inside an .app is
		//   <bundle>/Contents/MacOS
		// — one cdUp lands us at Contents/, the shared parent of
		// Resources/ and PlugIns/.
		{
			QDir bundleDir(QCoreApplication::applicationDirPath());
			if (bundleDir.cdUp()) { // MacOS -> Contents
				candidates << bundleDir.filePath(
					"Resources/MMCOPluginData/ErrorOracle/rules");
				// Legacy location from a previous macOS layout
				// attempt; kept so installs that predate this
				// commit keep finding their rules.
				candidates << bundleDir.filePath(
					"PlugIns/mmcmodules/ErrorOracle/rules");
			}
		}
#endif
		candidates << QCoreApplication::applicationDirPath() +
						  "/mmcmodules/ErrorOracle/rules";
		candidates << QCoreApplication::applicationDirPath() +
						  "/ErrorOracle/rules";
		if (auto envDir = qEnvironmentVariable("MESHMC_ERROR_ORACLE_RULES");
			!envDir.isEmpty())
			candidates.prepend(envDir);

		for (const auto& d : candidates) {
			if (QDir(d).exists())
				return d;
		}
		return {};
	}

	QString userRulesDir()
	{
		QString data = QString::fromUtf8(
			g_ctx ? g_ctx->fs_plugin_data_dir(g_ctx->module_handle) : "");
		if (data.isEmpty())
			data = QStandardPaths::writableLocation(
					   QStandardPaths::AppDataLocation) +
				   "/error-oracle";
		return QDir(data).filePath("userrules");
	}

	void loadAllRules()
	{
		if (!g_engine)
			return;

		QString builtin = builtInRulesDir();
		if (!builtin.isEmpty()) {
			QString err;
			g_engine->loadDirectory(builtin, &err);
		}

		QString user = userRulesDir();
		QDir().mkpath(user);
		g_engine->loadDirectory(user);
	}
} // namespace

/* ---- Per-instance lifecycle ------------------------------------------ */

static void createInstanceUi(const QString& instanceId, const QString& instanceRoot)
{
	if (!g_ctx || instanceId.isEmpty() || g_instances.contains(instanceId))
		return;
	auto* controller = new AnalysisPageController(g_ctx, instanceId, instanceRoot,
												  g_engine, g_learning);
	controller->createSurface();
	g_instances.insert(instanceId, controller);
}

static void destroyInstanceUi(const QString& instanceId)
{
	auto it = g_instances.find(instanceId);
	if (it == g_instances.end())
		return;
	AnalysisPageController* controller = it.value();
	controller->destroySurface();
	g_instances.erase(it);
	delete controller;
}

/* Instances that already existed when this module loaded aren't known
 * until the instance list has actually been populated — mirrors
 * GitVersioning's identical reasoning for building its per-instance
 * surfaces here instead of in mmco_init(). */
static int on_ui_main_ready(void*, uint32_t, void*, void*)
{
	if (!g_ctx)
		return 0;
	const int total = g_ctx->instance_count(g_ctx->module_handle);
	for (int i = 0; i < total; ++i) {
		const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
		if (!id)
			continue;
		const char* path = g_ctx->instance_get_path(g_ctx->module_handle, id);
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
 * "page list about to be (re)built" moment to refresh the analysis one
 * more time, so the page is as current as the old per-open BasePage
 * reconstruction used to be. */
static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
	auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
	if (!evt || !evt->instance_id)
		return 0;
	auto it = g_instances.find(QString::fromUtf8(evt->instance_id));
	if (it != g_instances.end())
		it.value()->reloadAnalysis();
	return 0;
}

static int on_post_launch(void*, uint32_t, void* payload, void*)
{
	if (!payload || !g_ctx || !g_engine)
		return 0;
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info->instance_id || !info->instance_path)
		return 0;

	if (!g_ctx->instance_has_crashed(g_ctx->module_handle, info->instance_id))
		return 0;

	LogIngester ing;
	auto bundle = ing.ingestForInstance(QString::fromUtf8(info->instance_path));
	auto matches = g_engine->analyse(bundle.combinedText);

	if (matches.isEmpty()) {
		// Record a novel fingerprint so the user can promote it later.
		QString sig = LearningStore::fingerprint(bundle.combinedText);
		QString sample;
		QRegularExpression re(QStringLiteral("Exception in thread.*"));
		auto m = re.match(bundle.combinedText);
		if (m.hasMatch())
			sample = m.captured(0).left(160);
		if (g_learning && !sig.isEmpty()) {
			g_learning->recordNovel(sig, sample,
									QString::fromUtf8(info->instance_id));
			g_learning->save();
		}
	} else if (g_ctx->tray_show_message) {
		QByteArray title = QStringLiteral("ErrorOracle").toUtf8();
		QByteArray body =
			QStringLiteral("Found %1 suggestion(s) for the last crash. "
						   "Open the instance's Error Analysis page for "
						   "details.")
				.arg(matches.size())
				.toUtf8();
		g_ctx->tray_show_message(g_ctx->module_handle, nullptr,
								 title.constData(), body.constData(),
								 /*Warning*/ 2, 8000);
	}
	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "ErrorOracle initialising…");

	g_engine = new RuleEngine();
	loadAllRules();

	g_learning = new LearningStore();
	QString plug =
		QString::fromUtf8(ctx->fs_plugin_data_dir(ctx->module_handle));
	g_learning->open(QDir(plug).filePath("learn.json"));

	qputenv("MESHMC_USER_RULES_DIR", userRulesDir().toUtf8());

	{
		QByteArray msg = "ErrorOracle: loaded " +
						 QByteArray::number(g_engine->ruleCount()) + " rules";
		MMCO_LOG(ctx, msg.constData());
	}

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_MAIN_READY,
					   on_ui_main_ready, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_CREATED,
					   on_instance_created, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
					   on_instance_pages, nullptr);
	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
					   on_post_launch, nullptr);

	MMCO_LOG(ctx, "ErrorOracle ready.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "ErrorOracle unloading.");
	if (g_learning)
		g_learning->save();
	/* Every surface this module still owns was already torn down by
	 * PluginManager::releaseSurfacesForModule() before this call — see
	 * GitVersioningPageController::destroySurface()'s comment. We only
	 * need to free our own heap state here, never touch a surface
	 * handle again. */
	qDeleteAll(g_instances);
	g_instances.clear();
	delete g_engine;
	g_engine = nullptr;
	delete g_learning;
	g_learning = nullptr;
	g_ctx = nullptr;
}

} /* extern "C" */
