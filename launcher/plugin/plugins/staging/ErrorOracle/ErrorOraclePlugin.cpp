/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * ErrorOraclePlugin — MMCO entry point. Loads the built-in rule pack
 * + any user rules at init, exposes a per-instance "Error Analysis"
 * page, and on every INSTANCE_POST_LAUNCH with a crash, automatically
 * pops a system-tray notification telling the user "ErrorOracle found
 * %d suggestion(s) for the last crash."
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "RuleEngine.h"
#include "LearningStore.h"
#include "LogIngester.h"
#include "AnalysisPage.h"

#include <QStandardPaths>

MMCO_DEFINE_MODULE("ErrorOracle", "1.0.0", "Project Tick",
				   "LLM-free crash/log analyser with a learning feedback loop.",
				   "Apache-2.0");

namespace
{
	MMCOContext* g_ctx = nullptr;
	RuleEngine* g_engine = nullptr;
	LearningStore* g_learning = nullptr;

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

static int on_instance_pages(void*, uint32_t, void* payload, void*)
{
	auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
	if (!evt || !evt->page_list_handle || !evt->instance_id)
		return 0;

	auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);

	const QString instId = QString::fromUtf8(evt->instance_id);
	const QString instRoot =
		evt->instance_path ? QString::fromUtf8(evt->instance_path) : QString();

	pages->append(new AnalysisPage(instId, instRoot, g_engine, g_learning));
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
	delete g_engine;
	g_engine = nullptr;
	delete g_learning;
	g_learning = nullptr;
	g_ctx = nullptr;
}

} /* extern "C" */
