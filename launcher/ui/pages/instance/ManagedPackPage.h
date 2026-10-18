/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QString>
#include <QUrl>
#include <QWidget>

#include "BaseInstance.h"
#include "modplatform/ManagedPackVersions.h"
#include "net/NetJob.h"
#include "ui/pages/BasePage.h"

namespace Ui
{
	class ManagedPackPage;
}

class InstanceTask;
class InstanceWindow;

/* The "Modpack" tab of an instance that came from Modrinth or
 * CurseForge.
 *
 * Shows what pack and which version of it is installed, lists the other
 * versions the catalogue has, renders the changelog of whichever one is
 * selected, and replaces the instance with that version in place.
 *
 * One class covers both providers rather than a base plus a subclass
 * each. The two differ in exactly three respects - the endpoint that
 * lists versions, how the reply is shaped, and whether a changelog
 * arrives with the list or needs its own request - and all three are
 * narrow enough to name in a switch. Splitting them into subclasses
 * would duplicate the whole of the fetch/select/render cycle to vary
 * those three points, and that duplication is where the two halves
 * would drift apart.
 *
 * There is also a second, quieter mode. An instance can have a provider
 * recorded but no pack id (imported from a local .mrpack, or imported
 * by a Kite old enough not to have recorded one). There is no
 * catalogue to query, so instead of a version list the page offers a URL
 * field and "Update From File" - which is the only route such an
 * instance has to a newer version. */
class ManagedPackPage : public QWidget, public BasePage
{
	Q_OBJECT

  public:
	/* Which catalogue backs this instance. Unknown covers both "no
	 * provider recorded" and "a provider string we do not implement",
	 * which behave the same: there is nothing to show. */
	enum class Provider { Unknown, Modrinth, CurseForge };

	/* Maps the `PackProvider` string onto the enum. Accepts "flame" as
	 * a synonym for "curseforge": that is what the upstream launchers
	 * call it, and an instance.cfg written by one of them - or by an
	 * import tool that copied their vocabulary - should not silently
	 * lose its provider. */
	static Provider providerFromString(const QString& provider);

	/* Whether an instance page should exist for this instance at all.
	 * Asked by InstancePageProvider so it can skip constructing the
	 * page - and therefore the .ui - for the overwhelming majority of
	 * instances, which are not managed packs. */
	static bool isSupported(const BaseInstance* instance);

	explicit ManagedPackPage(BaseInstance* instance,
							 InstanceWindow* instanceWindow = nullptr,
							 QWidget* parent = nullptr);
	~ManagedPackPage() override;

	QString id() const override
	{
		return QStringLiteral("managed_pack");
	}
	QString displayName() const override;
	QIcon icon() const override;
	QString helpPage() const override;
	bool shouldDisplay() const override;
	bool apply() override
	{
		return true;
	}

	void openedImpl() override;

	/* The window this page lives in, when it is an instance window
	 * rather than the settings dialog. Set after construction because
	 * the window builds its page container before it can hand out a
	 * pointer to itself.
	 *
	 * Only used to close the window once an update has landed: the
	 * instance the window is showing has just been replaced on disk,
	 * so leaving it open would show stale everything. */
	void setInstanceWindow(InstanceWindow* window)
	{
		m_instanceWindow = window;
	}

  private slots:
	/* Combo box selection changed: render that version's changelog and
	 * let the update button describe what it would do. */
	void suggestVersion();

	/* Update to the selected version, or - in the no-pack-id mode - to
	 * whatever the user put in the URL field. */
	void update();

	/* Update from an archive the user picks off disk. */
	void updateFromFile();

	/* Retry everything after a failure. */
	void reload();

  private:
	/* Ask the catalogue for the pack's version list. No-op when there
	 * is no pack id, or when the list is already loaded. */
	void fetchVersions();

	/* Hand the parsed list to the combo box.
	 *
	 * The selection is left on entry 0, i.e. the newest version, rather
	 * than moved to the installed one. Opening the tab is nearly always
	 * a question about the newest version, and the installed one is
	 * already spelled out in "Current version:" above - and marked
	 * "(Current)" in the list - so pointing the selection at it would
	 * make the common case an extra click. */
	void applyVersions(quint64 generation, const QByteArray& bytes);

	/* CurseForge only: fetch the changelog for one version, then render
	 * it if that version is still the selected one. */
	void fetchChangelogFor(int versionIndex);

	/* Put `changelog` (markdown or HTML, as the provider sent it) into
	 * the text browser. */
	void renderChangelog(const QString& changelog);

	/* Everything failed: say so in every part of the UI that would
	 * otherwise look like it is still working, and offer a retry. */
	void setFailState();

	/* Lay the page out for an instance with no pack id: no version
	 * list, a URL field instead, and an explanation in place of a
	 * changelog. */
	void showLocalPackMode();

	/* Fill in the three "Pack Information" rows. */
	void showPackInformation();

	/* The pack's page on the provider's website. */
	QString packUrl() const;

	/* Currently selected version, or nullptr when the list is empty or
	 * the selection is out of range. Returning a pointer rather than a
	 * copy keeps the changelog - which can be tens of kilobytes - from
	 * being copied on every combo box change. */
	const ManagedPack::Version* selectedVersion() const;

	/* Kick off an import that overwrites this instance with `url`.
	 *
	 * `versionId` / `versionName` are recorded onto the instance when
	 * the update lands, so the page and the catalogue keep agreeing
	 * about what is installed. Both are empty for a file or URL the
	 * user supplied, where we genuinely do not know the version.
	 *
	 * `trusted` says whether the launcher chose this download itself. A
	 * version picked out of the catalogue list is trusted; a file or a
	 * URL the user supplied is not, and the importer will list what such
	 * an archive wants to install before fetching any of it. */
	void updatePack(const QUrl& url, bool trusted,
					const QString& versionId = QString(),
					const QString& versionName = QString());

	/* Run an instance task behind a progress dialog, reporting failures
	 * and warnings the way the rest of the launcher does. Returns
	 * whether it succeeded. */
	bool runUpdateTask(InstanceTask* task);

	/* Post-update bookkeeping: tell the user, and close the instance
	 * window if we are in one. */
	void onUpdateFinished(bool succeeded, const QString& versionName);

  private:
	Ui::ManagedPackPage* ui;

	BaseInstance* m_instance;
	InstanceWindow* m_instanceWindow = nullptr;

	Provider m_provider = Provider::Unknown;

	ManagedPack::VersionList m_versions;

	/* Whether the version list has been fetched successfully. Reopening
	 * the tab should not re-hit the API, but a failure has to be
	 * retryable - so this is set only on success, and cleared by
	 * reload(). */
	bool m_loaded = false;

	/* In-flight jobs, so that reopening or reloading the page cannot
	 * leave an older reply to land on top of a newer one. Both are
	 * aborted before a new one starts; the generation counter covers
	 * the window where an abort races with a reply already queued. */
	NetJob::Ptr m_versionsJob;
	NetJob::Ptr m_changelogJob;
	quint64 m_generation = 0;
};
