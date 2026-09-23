/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "WikiRepoBundle.h"

#include <QUrl>

QString WikiRepoBundle::slugFromFileName(const QString& fileName)
{
	// "Page-Name.md" -> "Page-Name"
	QString base = fileName;
	if (base.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
		base.chop(3);
	return base;
}

QString WikiRepoBundle::titleFromSlug(const QString& slug)
{
	// "Page-Name" -> "Page Name". GitLab/GitHub wikis encode spaces as
	// dashes in the file name; underscores are also seen in older dumps.
	QString t = slug;
	t.replace(QLatin1Char('-'), QLatin1Char(' '));
	t.replace(QLatin1Char('_'), QLatin1Char(' '));
	return t.trimmed();
}

bool WikiRepoBundle::open(const QString& dir)
{
	QDir d(dir);
	if (!d.exists())
		return false;

	const auto files =
		d.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
	for (const auto& fi : files) {
		const QString file = fi.fileName();
		// Skip GitLab/GitHub wiki meta pages from the article list; the
		// sidebar/footer are consumed separately, not shown as articles.
		if (file.compare(QStringLiteral("_Sidebar.md"), Qt::CaseInsensitive) ==
				0 ||
			file.compare(QStringLiteral("_Footer.md"), Qt::CaseInsensitive) == 0)
			continue;

		Article a;
		a.slug = slugFromFileName(file);
		a.title = titleFromSlug(a.slug);
		a.relFile = file;
		if (a.slug.isEmpty())
			continue;
		m_articles.insert(a.slug, a);
		m_order.append(a);
	}

	if (m_articles.isEmpty())
		return false;

	m_root = dir;
	m_name = QFileInfo(dir).fileName();
	if (m_name.isEmpty() || m_name == QStringLiteral("."))
		m_name = QStringLiteral("Wiki");
	m_hasFooter =
		QFileInfo::exists(QDir(m_root).filePath(QStringLiteral("_Footer.md")));
	return true;
}

QList<WikiRepoBundle::Entry> WikiRepoBundle::nav() const
{
	QList<Entry> out;

	// Put Home first if present, then the rest alphabetically.
	auto homeIt = m_articles.constFind(QStringLiteral("Home"));
	if (homeIt != m_articles.constEnd())
		out.append({homeIt->slug, homeIt->title});

	for (const auto& a : m_order) {
		if (a.slug == QStringLiteral("Home"))
			continue;
		out.append({a.slug, a.title});
	}
	return out;
}

QString WikiRepoBundle::rewriteLinks(const QString& markdown) const
{
	QString out = markdown;

	// 1. [[Page Name]]  and  [[text|Page Name]]  ->  [text](wiki:Page-Name)
	//    The wiki stores spaces as dashes in file names, so normalise the
	//    target the same way.
	static const QRegularExpression wikiLink(
		QStringLiteral(R"(\[\[([^\]|]+?)(?:\|([^\]]+?))?\]\])"));
	{
		QString rebuilt;
		int last = 0;
		auto it = wikiLink.globalMatch(out);
		while (it.hasNext()) {
			auto m = it.next();
			rebuilt += out.mid(last, m.capturedStart() - last);
			// Group 1 is either the page (no pipe) or the display text
			// (with pipe); group 2, if present, is the target page.
			QString left = m.captured(1).trimmed();
			QString right = m.captured(2).trimmed();
			QString display = right.isEmpty() ? left : left;
			QString target = right.isEmpty() ? left : right;
			QString slug = target;
			slug.replace(QLatin1Char(' '), QLatin1Char('-'));
			rebuilt += QStringLiteral("[%1](wiki:%2)").arg(display, slug);
			last = m.capturedEnd();
		}
		rebuilt += out.mid(last);
		out = rebuilt;
	}

	// 2. Markdown links to other wiki pages: [text](Target) where Target
	//    is a bare relative page reference (no scheme, no "/", not an
	//    anchor, not an image, doesn't point at an existing file such as
	//    images/foo.png). Rewrite those to the wiki: scheme. Absolute
	//    links (http:, https:, mailto:, #anchor) and real relative files
	//    are left untouched.
	static const QRegularExpression mdLink(
		QStringLiteral(R"(\]\(([^)\s]+)\))"));
	{
		QString rebuilt;
		int last = 0;
		auto it = mdLink.globalMatch(out);
		while (it.hasNext()) {
			auto m = it.next();
			rebuilt += out.mid(last, m.capturedStart() - last);
			QString target = m.captured(1);
			const bool absolute =
				target.contains(QStringLiteral("://")) ||
				target.startsWith(QLatin1Char('#')) ||
				target.startsWith(QStringLiteral("mailto:")) ||
				target.startsWith(QStringLiteral("wiki:")) ||
				target.contains(QLatin1Char('/'));
			// A bare "Page-Name" (optionally with a #fragment) that maps to
			// a known article becomes an internal link.
			QString base = target;
			int hashPos = base.indexOf(QLatin1Char('#'));
			if (hashPos >= 0)
				base = base.left(hashPos);
			if (!absolute && !base.isEmpty() &&
				m_articles.contains(base)) {
				rebuilt += QStringLiteral("](wiki:%1)").arg(target);
			} else {
				rebuilt += m.captured(0);
			}
			last = m.capturedEnd();
		}
		rebuilt += out.mid(last);
		out = rebuilt;
	}

	return out;
}

QString WikiRepoBundle::rewriteImagePaths(const QString& markdown) const
{
	// Resolve relative Markdown image targets (![alt](images/foo.png))
	// against the bundle root so the renderer can load bundled media
	// without knowing the wiki's on-disk location. Absolute (http/https/
	// data:) targets are left alone.
	const QString baseUrl =
		QUrl::fromLocalFile(QDir(m_root).absolutePath() + QLatin1Char('/'))
			.toString();
	static const QRegularExpression imgRef(
		QStringLiteral(R"(!\[([^\]]*)\]\(([^)\s]+)\))"));
	QString out;
	int last = 0;
	auto it = imgRef.globalMatch(markdown);
	while (it.hasNext()) {
		auto m = it.next();
		out += markdown.mid(last, m.capturedStart() - last);
		const QString alt = m.captured(1);
		const QString src = m.captured(2);
		const bool absolute = src.contains(QStringLiteral("://")) ||
							  src.startsWith(QStringLiteral("data:"));
		if (absolute)
			out += m.captured(0);
		else
			out += QStringLiteral("![%1](%2%3)").arg(alt, baseUrl, src);
		last = m.capturedEnd();
	}
	out += markdown.mid(last);
	return out;
}

QString WikiRepoBundle::renderArticleMarkdown(const QString& slug) const
{
	auto it = m_articles.constFind(slug);
	if (it == m_articles.constEnd())
		return {};

	const QString abs = QDir(m_root).filePath(it->relFile);
	QFile f(abs);
	if (!f.open(QIODevice::ReadOnly))
		return {};
	QString body = QString::fromUtf8(f.readAll());

	// Append the shared footer, if the wiki ships one.
	if (m_hasFooter) {
		QFile ff(QDir(m_root).filePath(QStringLiteral("_Footer.md")));
		if (ff.open(QIODevice::ReadOnly)) {
			body += QStringLiteral("\n\n---\n\n");
			body += QString::fromUtf8(ff.readAll());
		}
	}

	body = rewriteLinks(body);
	body = rewriteImagePaths(body);
	return body;
}

QList<WikiRepoBundle::Entry> WikiRepoBundle::searchTitles(const QString& query,
														  int limit) const
{
	QList<Entry> out;
	for (const auto& a : m_order) {
		if (a.title.contains(query, Qt::CaseInsensitive)) {
			out.append({a.slug, a.title});
			if (out.size() >= limit)
				break;
		}
	}
	return out;
}
