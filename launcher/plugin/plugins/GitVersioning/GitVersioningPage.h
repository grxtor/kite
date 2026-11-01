/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * GitVersioningPageController: ABI 5 declarative-UI controller for the per-instance "Version History" page.
 *
 * No QWidget is involved: the host renders the "mmco-ui/1" document this class maintains with
 * PluginNode.qml. This object owns the MMCO_UI_ANCHOR_INSTANCE_PAGE surface
 * handle, the cached commit list and the current row selection, and pushes updates through
 * ui_surface_set / ui_surface_set_rows in response to MMCOUiEventCallback events.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "GitRepo.h"

class GitVersioningPageController
{
  public:
	GitVersioningPageController(MMCOContext* ctx, QString instanceId,
								QString instanceRoot);

	/* Builds the initial document from the current git state and registers the INSTANCE_PAGE
	 * surface. Call once, before its page is first requested (see the UI_MAIN_READY /
	 * INSTANCE_CREATED handlers in GitVersioningPlugin.cpp). */
	void createSurface();

	/* Tears the surface down early when the instance is removed while the plugin stays loaded; safe
	 * to call more than once. Not needed on plugin unload: the host tears down a module's surfaces
	 * before mmco_unload(), and calling this afterwards would touch a freed handle. */
	void destroySurface();

	/* Re-reads git state and pushes a fresh status line and row set to the surface. Called from the
	 * Refresh click and after a snapshot, restore or tag. */
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

	/* Destructive-action confirmation through the host's ui_confirm_dialog. */
	bool confirm(const QString& title, const QString& message) const;
	/* Info/warning toast through ui_show_message. type: 0=info, 1=warning. */
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
