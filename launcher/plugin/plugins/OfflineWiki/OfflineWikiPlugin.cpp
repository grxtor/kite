/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * OfflineWikiPlugin: MMCO entry point. Clones the official Kite wiki into <plugin_data>/ in the
 * background and exposes it as a read-only global settings page; the wiki is the only content source.
 *
 * The page is one declarative "mmco-ui/1" surface (anchor GLOBAL_SETTINGS):
 *
 *   row
 *     column
 *       text_field "search"   filter box
 *       list       "nav"      page titles (nav list, or search hits while the box has text)
 *     text         "article"  format:"markdown", the current page
 *
 * Internal wiki links are Markdown links with a "wiki:" href (WikiRepoBundle::rewriteLinks produces
 * them); clicking one fires the "article" node's click event with that href, which resolves to a
 * showArticle() call.
 */

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "WikiRepoBundle.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

MMCO_DEFINE_MODULE("OfflineWiki", "1.0.0", "Project Tick",
				   "Offline, read-only viewer for the Kite wiki",
				   "Apache-2.0");

namespace
{
	MMCOContext* g_ctx = nullptr;

	/* The single wiki bundle; null until the clone has produced a usable checkout. */
	WikiRepoBundle* g_wiki = nullptr;

	/* The GLOBAL_SETTINGS surface: created once in mmco_init() and patched in place; the host
	 * re-renders it each time Settings opens. */
	void* g_surface = nullptr;

	/* UI state mirrored locally to recompute the nav list and article text after a background
	 * clone/pull. */
	QString g_searchQuery;
	QString g_currentSlug;

	/* Prevents a second background sync while one runs (e.g. Settings reopened mid-clone). */
	bool g_wikiSyncRunning = false;

	/* The official Kite wiki: cloned on first run, fast-forwarded on later starts. */
	constexpr const char kKiteWikiUrl[] =
		"https://github.com/grxtor/kite.wiki.git";
	constexpr const char kKiteWikiDirName[] = "kite-wiki";

	const QString kNoGitMarkdown = QStringLiteral(
		"### Wiki not available\n\n"
		"The Kite wiki is downloaded automatically using **git**, but no "
		"`git` program was found on your system, so the wiki cannot be "
		"fetched.\n\n"
		"Install Git and restart Kite to download the wiki.");

	const QString kDownloadingMarkdown = QStringLiteral(
		"### Downloading the Kite wiki…\n\n"
		"The wiki is fetched in the background the first time you are "
		"online and will appear here automatically once the download "
		"finishes. If you are offline, Kite retries on every launch.");

	QString pluginDataDir()
	{
		if (!g_ctx)
			return {};
		return QString::fromUtf8(
			g_ctx->fs_plugin_data_dir(g_ctx->module_handle));
	}

	QString wikiPath()
	{
		return QDir(pluginDataDir()).filePath(kKiteWikiDirName);
	}

	QString gitExecutable()
	{
		return QStandardPaths::findExecutable(QStringLiteral("git"));
	}

	/* MMCOUiEventCallback's value_json is a JSON-encoded scalar (encoded by
	 * PluginSurfaceModel::valueToJson / quoteJsonString): change/select/activate payloads arrive as a quoted, escaped string literal
	 * such as `"wiki:Page-Name"`. Wrapping it in an array lets QJsonDocument decode it (it refuses
	 * a bare top-level string). */
	QString jsonUnquote(const char* valueJson)
	{
		if (!valueJson || !*valueJson)
			return {};
		const QByteArray wrapped = "[" + QByteArray(valueJson) + "]";
		QJsonParseError err{};
		const QJsonDocument jd = QJsonDocument::fromJson(wrapped, &err);
		if (err.error != QJsonParseError::NoError || !jd.isArray() ||
			jd.array().isEmpty())
			return QString::fromUtf8(valueJson);
		return jd.array().at(0).toString();
	}

	/* Surface content */

	QJsonArray navRowsJson()
	{
		QList<WikiRepoBundle::Entry> entries;
		if (g_wiki && g_wiki->isOpen()) {
			entries = g_searchQuery.trimmed().isEmpty()
						  ? g_wiki->nav()
						  : g_wiki->searchTitles(g_searchQuery, 200);
		}
		QJsonArray rows;
		for (const auto& e : entries) {
			rows.append(QJsonObject{
				{"id", e.slug},
				{"cells", QJsonArray{e.title}}});
		}
		return rows;
	}

	/* Markdown for the "article" node: the selected article, an empty-state explanation while no
	 * wiki is available, or blank when nothing is picked. */
	QString articleTextFor(const QString& slug)
	{
		if (g_wiki && g_wiki->isOpen()) {
			if (slug.isEmpty())
				return QString();
			const QString md = g_wiki->renderArticleMarkdown(slug);
			if (md.isEmpty())
				return QStringLiteral("*Article not found: %1*").arg(slug);
			return md;
		}
		return gitExecutable().isEmpty() ? kNoGitMarkdown : kDownloadingMarkdown;
	}

	void setArticleProps(const QJsonObject& props)
	{
		if (!g_ctx || !g_surface)
			return;
		const QByteArray json = QJsonDocument(props).toJson(QJsonDocument::Compact);
		g_ctx->ui_surface_set(g_ctx->module_handle, g_surface, "article",
							  json.constData());
	}

	void showArticle(const QString& slug)
	{
		g_currentSlug = slug;
		setArticleProps(QJsonObject{{"text", articleTextFor(slug)}});
	}

	void refreshNavRows()
	{
		if (!g_ctx || !g_surface)
			return;
		const QByteArray json =
			QJsonDocument(navRowsJson()).toJson(QJsonDocument::Compact);
		g_ctx->ui_surface_set_rows(g_ctx->module_handle, g_surface, "nav",
								   json.constData());
	}

	/* Called after a background clone/pull changes what is on disk; the nav list and shown article
	 * (or empty state) catch up. */
	void refreshSurfaceFromWiki()
	{
		refreshNavRows();
		setArticleProps(QJsonObject{{"text", articleTextFor(g_currentSlug)}});
	}

	QJsonObject buildDocumentJson()
	{
		const QJsonObject searchField{
			{"type", "text_field"},
			{"id", "search"},
			{"props",
			 QJsonObject{{"placeholder", "Search…"}, {"value", g_searchQuery}}}};
		const QJsonObject navList{
			{"type", "list"},
			{"id", "nav"},
			{"props",
			 QJsonObject{{"columns", QJsonArray{"Title"}},
						 {"rows", navRowsJson()}}}};
		const QJsonObject navColumn{
			{"type", "column"},
			{"id", "nav_col"},
			{"children", QJsonArray{searchField, navList}}};
		const QJsonObject article{
			{"type", "text"},
			{"id", "article"},
			{"props",
			 QJsonObject{{"format", "markdown"},
						 {"text", articleTextFor(g_currentSlug)}}}};
		const QJsonObject root{{"type", "row"},
							   {"id", "root"},
							   {"children", QJsonArray{navColumn, article}}};
		return QJsonObject{{"type", "mmco-ui/1"}, {"root", root}};
	}

	/* Tries to (re)open the on-disk checkout into g_wiki; true when a usable wiki is open
	 * afterwards. */
	bool openWiki()
	{
		const QString path = wikiPath();
		if (!QFileInfo::exists(QDir(path).filePath(QStringLiteral(".git"))))
			return g_wiki != nullptr;

		auto* w = new WikiRepoBundle();
		if (!w->open(path)) {
			delete w;
			return g_wiki != nullptr;
		}
		delete g_wiki;
		g_wiki = w;
		return true;
	}

	/* Kicks off the Kite wiki clone/update in the background so boot never blocks on git or the
	 * network (the event loop already runs when mmco_init is called). A previously cloned copy is
	 * opened synchronously at init, so the wiki works offline; the background pull is best-effort
	 * and keeps the cached copy on failure. With no cached copy and no network the clone fails, and
	 * the plugin retries on the next launch or when Settings is reopened. */
	void startKiteWikiSync()
	{
		if (g_wikiSyncRunning)
			return;

		const QString exe = gitExecutable();
		if (exe.isEmpty()) {
			MMCO_WARN(g_ctx, "OfflineWiki: git not found — the wiki cannot be "
							 "downloaded (install git and restart).");
			return;
		}

		const QString dir = pluginDataDir();
		QDir().mkpath(dir);
		const QString path = wikiPath();
		const bool haveCheckout =
			QFileInfo::exists(QDir(path).filePath(QStringLiteral(".git")));

		QStringList args;
		QString workingDir;
		if (haveCheckout) {
			workingDir = path;
			args << QStringLiteral("pull") << QStringLiteral("--ff-only")
				 << QStringLiteral("--quiet");
		} else {
			workingDir = dir;
			args << QStringLiteral("clone") << QStringLiteral("--depth")
				 << QStringLiteral("1") << QStringLiteral("--quiet")
				 << QString::fromUtf8(kKiteWikiUrl) << path;
		}

		auto* proc = new QProcess();
		proc->setWorkingDirectory(workingDir);
		auto env = QProcessEnvironment::systemEnvironment();
		env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
		env.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
		proc->setProcessEnvironment(env);
		proc->setProgram(exe);
		proc->setArguments(args);

		g_wikiSyncRunning = true;
		MMCO_LOG(g_ctx, haveCheckout
							? "OfflineWiki: updating Kite wiki in background…"
							: "OfflineWiki: cloning Kite wiki in background…");

		QObject::connect(
			proc,
			QOverload<int, QProcess::ExitStatus>::of(qOverload<int, QProcess::ExitStatus>(&QProcess::finished)),
			proc, [proc, haveCheckout](int code, QProcess::ExitStatus) {
				g_wikiSyncRunning = false;
				if (!g_ctx) { // plugin unloaded mid-flight
					proc->deleteLater();
					return;
				}
				if (code != 0) {
					QByteArray err = proc->readAllStandardError();
					QByteArray msg =
						QByteArray(haveCheckout
									   ? "OfflineWiki: wiki update failed "
										 "(using cached copy if present): "
									   : "OfflineWiki: wiki clone failed (will "
										 "retry next start): ") +
						err;
					MMCO_WARN(g_ctx, msg.constData());
				} else {
					MMCO_LOG(g_ctx, haveCheckout
										? "OfflineWiki: Kite wiki updated."
										: "OfflineWiki: Kite wiki cloned.");
					openWiki();
					refreshSurfaceFromWiki();
				}
				proc->deleteLater();
			});

		proc->start();
	}

	/* Surface event handling */

	void onSearchChanged(const QString& text)
	{
		g_searchQuery = text;
		refreshNavRows();
	}

	void onNavPicked(const char* valueJson)
	{
		const QString slug = jsonUnquote(valueJson);
		if (slug.isEmpty())
			return;
		showArticle(slug);
	}

	void onArticleLinkClicked(const QString& href)
	{
		if (href.isEmpty())
			return;

		// Internal wiki link (wiki:Page-Name[#fragment]): render the target article.
		if (href.startsWith(QStringLiteral("wiki:"))) {
			QString slug = href.mid(5);
			const int hash = slug.indexOf(QLatin1Char('#'));
			if (hash >= 0)
				slug = slug.left(hash);
			if (!slug.isEmpty())
				showArticle(slug);
			return;
		}

		// A pure in-page anchor (#section) is a no-op: the markdown "text" node is a plain QLabel
		// with no scrollToAnchor(), so a plugin cannot scroll to a fragment.
		if (!href.contains(QLatin1Char(':')))
			return;

		// Everything else (http/https/mailto/...) opens in the system browser; remote content is
		// never loaded in the offline viewer.
		QDesktopServices::openUrl(QUrl(href));
	}

	void on_wiki_surface_event(void* /*user_data*/, const char* /*surface_id*/,
							   const char* node_id, const char* event,
							   const char* value_json)
	{
		if (!node_id || !event)
			return;
		const QString id = QString::fromUtf8(node_id);
		const QString ev = QString::fromUtf8(event);

		if (id == QLatin1String("search") && ev == QLatin1String("change")) {
			onSearchChanged(jsonUnquote(value_json));
		} else if (id == QLatin1String("nav") &&
				   (ev == QLatin1String("select") ||
					ev == QLatin1String("activate"))) {
			onNavPicked(value_json);
		} else if (id == QLatin1String("article") && ev == QLatin1String("click")) {
			onArticleLinkClicked(jsonUnquote(value_json));
		}
	}
} // namespace

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "OfflineWiki initialising…");

	// Open a previously cloned wiki synchronously (local I/O) so it works offline at once; the
	// network sync runs in the background.
	openWiki();
	MMCO_LOG(ctx, g_wiki ? "OfflineWiki: Kite wiki ready (cached copy)."
						 : "OfflineWiki: no cached wiki yet.");

	const QByteArray doc =
		QJsonDocument(buildDocumentJson()).toJson(QJsonDocument::Compact);
	g_surface = ctx->ui_surface_create(
		ctx->module_handle, MMCO_UI_ANCHOR_GLOBAL_SETTINGS, nullptr, "Wiki",
		"help-browser", doc.constData(), on_wiki_surface_event, nullptr);
	if (!g_surface)
		MMCO_ERR(ctx, "OfflineWiki: ui_surface_create() failed.");

	startKiteWikiSync();

	MMCO_LOG(ctx, "OfflineWiki ready.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx)
		MMCO_LOG(g_ctx, "OfflineWiki unloading.");
	// Clear g_ctx first so an in-flight background git callback becomes a no-op (it checks g_ctx
	// and only deletes its QProcess). The host tears down g_surface on unload.
	g_ctx = nullptr;
	g_surface = nullptr;
	delete g_wiki;
	g_wiki = nullptr;
}

} /* extern "C" */
