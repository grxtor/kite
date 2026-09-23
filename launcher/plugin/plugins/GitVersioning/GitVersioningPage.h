/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitVersioningPageController — ABI 5 declarative-UI controller for the
 * per-instance "Version History" page.
 *
 * This replaces the former GitVersioningPage (a QWidget + BasePage
 * subclass). There is no QWidget here at all: the actual widget tree is
 * built by the host's PluginUiRenderer from the "mmco-ui/1" JSON document
 * this class maintains, and mounted fresh every time the instance window's
 * page list is rebuilt (see PluginManager::createInstancePages). This
 * object just owns the MMCO_UI_ANCHOR_INSTANCE_PAGE surface handle, the
 * cached commit list, and the current row selection, and pushes updates
 * through ui_surface_set / ui_surface_set_rows in response to events
 * delivered through MMCOUiEventCallback.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "GitRepo.h"

class GitVersioningPageController
{
  public:
	GitVersioningPageController(MMCOContext* ctx, QString instanceId,
								QString instanceRoot);

	/* Builds the initial document from the current git state and
	 * registers the MMCO_UI_ANCHOR_INSTANCE_PAGE surface. Must be called
	 * once, before the instance's page list can be requested (i.e.
	 * before the instance window can be opened for this instance) —
	 * see GitVersioningPlugin.cpp's MMCO_HOOK_UI_MAIN_READY /
	 * MMCO_HOOK_INSTANCE_CREATED handlers. */
	void createSurface();

	/* Tears the surface down early — used when the instance itself is
	 * removed while the plugin stays loaded. Safe to call more than
	 * once. NOT needed on plugin unload: the host tears down every
	 * surface a module still owns automatically before mmco_unload()
	 * runs (see PluginManager::releaseSurfacesForModule) — calling this
	 * afterwards would touch an already-freed handle. */
	void destroySurface();

	/* Re-reads git state and pushes a fresh status line + row set to the
	 * surface. Called from the Refresh button's click event and — to
	 * keep the page as up to date as the old per-open BasePage
	 * reconstruction used to be — every time this instance's page list
	 * is about to be rebuilt (see GitVersioningPlugin.cpp's
	 * MMCO_HOOK_UI_INSTANCE_PAGES handler). */
	void reloadHistory();

  private:
	void handleEvent(const QString& nodeId, const QString& event,
					 const QString& valueJson);
	static void eventTrampoline(void* user_data, const char* surface_id,
							   const char* node_id, const char* event,
							   const char* value_json);

	void onSnapshotClicked();
	void onRestoreClicked();
	void onTagClicked();
	void onDropClicked();
	void onSelectionChanged(const QString& rowId);

	GitCommit selectedCommit() const;

	/* Destructive-action confirmation, routed through the host's
	 * ui_confirm_dialog (unchanged since ABI 2 — a single opaque host
	 * dialog, not a persistent widget, so ABI 5 left it as-is). */
	bool confirm(const QString& title, const QString& message) const;
	/* Info/warning toast, routed through the host's ui_show_message
	 * (also unchanged since ABI 2). type: 0=info, 1=warning. */
	void notify(int type, const QString& title, const QString& message) const;

	QJsonObject buildDocument() const;
	QJsonArray buildRows() const;
	QString buildStatusText() const;
	void setStatusText(const QString& text) const;
	void pushRows() const;
	void setNodeEnabled(const QString& nodeId, bool enabled) const;

	MMCOContext* m_ctx = nullptr;
	QString m_instanceId;
	QString m_instanceRoot;
	GitRepo m_repo;
	QList<GitCommit> m_commits;
	QString m_selectedSha; /* full sha of the selected commit row, or empty */
	void* m_surface = nullptr;
};
