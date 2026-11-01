/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * AnalysisPageController: ABI 5 declarative-UI controller for the per-instance "Error Analysis" page.
 *
 * No QWidget is involved: the host renders the "mmco-ui/1" document this class maintains with
 * PluginNode.qml. This object owns the MMCO_UI_ANCHOR_INSTANCE_PAGE surface
 * handle, the cached rule-engine matches and the current row selection, and pushes updates through
 * ui_surface_set / ui_surface_set_rows in response to MMCOUiEventCallback events.
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

	/* Builds the initial document from a fresh analysis and registers the INSTANCE_PAGE surface.
	 * Call once per instance, before its page list can be requested (see the UI_MAIN_READY /
	 * INSTANCE_CREATED handlers in ErrorOraclePlugin.cpp). */
	void createSurface();

	/* Tears the surface down early when the instance is removed while the plugin stays loaded; safe
	 * to call more than once. Not needed on plugin unload: the host tears down a module's surfaces
	 * before mmco_unload(), and calling this afterwards would touch a freed handle. */
	void destroySurface();

	/* Re-runs the rule engine over the instance's latest log/crash report and pushes a fresh
	 * summary, rows and advice text to the surface. Called from the Re-analyse click. */
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
	/* Ingests the latest log, runs the rule engine, scores and sorts the matches, and records a
	 * novel fingerprint if nothing matched. Pure state update; callers push the result to the
	 * surface. */
	void runAnalysis();

	/* Info/warning toast through the host's ui_show_message. type: 0=info, 1=warning. */
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
