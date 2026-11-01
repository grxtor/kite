/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * WikiRepoBundle: the wiki backend, a cloned Git wiki repository in the flat layout GitLab/GitHub
 * wikis use:
 *
 *   <root>/Page-Name.md   one Markdown file per page; the slug is the base name (dashes for spaces)
 *   <root>/Home.md        landing page (first page alphabetically if absent)
 *   <root>/_Sidebar.md    meta page, not an article
 *   <root>/_Footer.md     optional, appended to every page
 *   <root>/images/...     media referenced with relative paths
 *
 * Pages are found by scanning *.md. Wiki links are rewritten (still Markdown) to an internal "wiki:"
 * scheme so the declarative UI's markdown text node navigates through ordinary link clicks:
 *
 *   [[Page Name]]      -> [Page Name](wiki:Page-Name)
 *   [text](Page-Name)  -> [text](wiki:Page-Name)
 *
 * Absolute links (http(s):, mailto:, #anchors) and links to existing files stay untouched. Relative
 * images (![alt](images/foo.png)) become absolute file:// URLs so they resolve without the renderer
 * knowing the bundle root.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"

class WikiRepoBundle
{
  public:
	/* A single navigable page: its slug ("Page-Name") and display
	 * title ("Page Name"). */
	struct Entry {
		QString slug;
		QString title;
	};

	bool open(const QString& dir);

	QString name() const
	{
		return m_name;
	}
	QString rootPath() const
	{
		return m_root;
	}
	bool isOpen() const
	{
		return !m_root.isEmpty();
	}

	/* Navigation list: Home first (if present), then alphabetical. */
	QList<Entry> nav() const;

	/* Resolves a slug to the article's Markdown source (links rewritten to wiki:, images to
	 * absolute file:// URLs) for a "text" node with format "markdown"; empty if not found. */
	QString renderArticleMarkdown(const QString& slug) const;

	/* Pages whose title contains `query` (case-insensitive substring). */
	QList<Entry> searchTitles(const QString& query, int limit = 200) const;

	static QString slugFromFileName(const QString& fileName);
	static QString titleFromSlug(const QString& slug);

  private:
	struct Article {
		QString slug;	 // "Page-Name"
		QString title;	 // "Page Name"
		QString relFile; // "Page-Name.md"
	};

	QString rewriteLinks(const QString& markdown) const;
	QString rewriteImagePaths(const QString& markdown) const;

	QString m_name;
	QString m_root;
	QHash<QString, Article> m_articles; // slug -> article
	QList<Article> m_order;				// alphabetical
	bool m_hasFooter = false;
};
