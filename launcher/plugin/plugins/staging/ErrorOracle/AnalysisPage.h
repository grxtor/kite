/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * AnalysisPageController — ABI 5 declarative-UI controller for the
 * per-instance "Error Analysis" page.
 *
 * This replaces the former AnalysisPage (a QWidget + BasePage
 * subclass: match tree, advice text, summary label, action buttons),
 * migrated exactly the way GitVersioningPage was migrated in commit
 * aa2c3e9c — see GitVersioning/GitVersioningPage.h/.cpp for the
 * reference shape this mirrors. There is no QWidget here at all: the
 * actual widget tree is built by the host's PluginUiRenderer from the
 * "mmco-ui/1" JSON document this class maintains, and mounted fresh
 * every time the instance window's page list is rebuilt (see
 * PluginManager::createInstancePages). This object just owns the
 * MMCO_UI_ANCHOR_INSTANCE_PAGE surface handle, the cached rule-engine
 * matches, and the current row selection, and pushes updates through
 * ui_surface_set / ui_surface_set_rows in response to events delivered
 * through MMCOUiEventCallback.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"
#include "RuleEngine.h"

class LearningStore;

class AnalysisPageController
{
  public:
	AnalysisPageController(MMCOContext* ctx, QString instanceId, QString instanceRoot,
						   RuleEngine* engine, LearningStore* learning);

	/* Builds the initial document from a fresh analysis run and
	 * registers the MMCO_UI_ANCHOR_INSTANCE_PAGE surface. Must be
	 * called once per instance, before that instance's page list can
	 * be requested — see ErrorOraclePlugin.cpp's
	 * MMCO_HOOK_UI_MAIN_READY / MMCO_HOOK_INSTANCE_CREATED handlers. */
	void createSurface();

	/* Tears the surface down early — used when the instance itself is
	 * removed while the plugin stays loaded. Safe to call more than
	 * once. NOT needed on plugin unload: the host tears down every
	 * surface a module still owns automatically before mmco_unload()
	 * runs (see PluginManager::releaseSurfacesForModule) — calling
	 * this afterwards would touch an already-freed handle. */
	void destroySurface();

	/* Re-runs the rule engine over the instance's latest log/crash
	 * report and pushes a fresh summary + row set + advice text to
	 * the surface. Called from the Re-analyse button's click event
	 * and — to keep the page as up to date as the old per-open
	 * BasePage reconstruction used to be — every time this instance's
	 * page list is about to be rebuilt (see ErrorOraclePlugin.cpp's
	 * MMCO_HOOK_UI_INSTANCE_PAGES handler). */
	void reloadAnalysis();

  private:
	void handleEvent(const QString& nodeId, const QString& event,
					 const QString& valueJson);
	static void eventTrampoline(void* user_data, const char* surface_id,
							   const char* node_id, const char* event,
							   const char* value_json);

	void onReanalyseClicked();
	void onHelpedClicked();
	void onDidNotHelpClicked();
	void onPromoteClicked();
	void onSelectionChanged(const QString& rowId);

	Match selectedMatch() const;
	/* Ingests the instance's latest log, runs the rule engine, scores
	 * + sorts the matches, and records a novel fingerprint if nothing
	 * matched. Pure state update — callers push the result to the
	 * surface themselves (createSurface() / reloadAnalysis()). */
	void runAnalysis();

	/* Info/warning toast, routed through the host's ui_show_message
	 * (unchanged since ABI 2 — a single opaque host dialog, not a
	 * persistent widget, so ABI 5 left it as-is). type: 0=info,
	 * 1=warning. */
	void notify(int type, const QString& title, const QString& message) const;

	QJsonObject buildDocument() const;
	QJsonArray buildRows() const;
	QString buildSummaryText() const;
	QString buildAdviceText(const Match& m) const;
	QString noMatchAdviceText() const;
	void setSummaryText(const QString& text) const;
	void setAdviceText(const QString& text) const;
	void pushRows() const;
	void setNodeEnabled(const QString& nodeId, bool enabled) const;

	MMCOContext* m_ctx = nullptr;
	QString m_instanceId;
	QString m_instanceRoot;
	RuleEngine* m_engine = nullptr;
	LearningStore* m_learning = nullptr;
	QList<Match> m_matches;
	QStringList m_lastSources; /* log/crash-report paths from the last ingest */
	QString m_currentFingerprint;
	QString m_currentSampleLine;
	QString m_selectedRuleId; /* ruleId of the selected match row, or empty */
	void* m_surface = nullptr;
};
