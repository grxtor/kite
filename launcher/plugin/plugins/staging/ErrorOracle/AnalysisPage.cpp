/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "AnalysisPage.h"
#include "LogIngester.h"
#include "LearningStore.h"

#include <algorithm>

namespace
{
	constexpr int kModalResultBufSize = 4096;

	const char* severityName(Severity s)
	{
		switch (s) {
			case Severity::High:
				return "high";
			case Severity::Medium:
				return "medium";
			case Severity::Low:
			default:
				return "low";
		}
	}

	/* Inverse of PluginUiRenderer's jsonQuoteString: unwraps a bare JSON
	 * scalar (as delivered in MMCOUiEventCallback's value_json for
	 * "select"/"activate" events) back into a plain QString. */
	QString jsonStringValue(const QString& valueJson)
	{
		if (valueJson.isEmpty())
			return QString();
		const QByteArray wrapped = "[" + valueJson.toUtf8() + "]";
		QJsonParseError err{};
		const QJsonDocument jd = QJsonDocument::fromJson(wrapped, &err);
		if (err.error != QJsonParseError::NoError || !jd.isArray() ||
			jd.array().isEmpty())
			return QString();
		return jd.array().first().toString();
	}

	/* The promote-to-rule prompt: three text fields (title, optional
	 * regex pattern, advice) plus a Save/Cancel button row, run
	 * through ui_modal_run. Replaces the QDialog the QWidget-era page
	 * used to build directly — the page has no QWidget of its own to
	 * parent a dialog to any more. The node set has no multi-line text
	 * editor, so the advice field is a single-line text_field (a
	 * behavioural narrowing from the old QTextEdit — long advice text
	 * still works, it just doesn't wrap on screen while typing). */
	QByteArray buildPromoteDoc(const QString& fingerprint, const QString& sampleLine)
	{
		const QJsonObject info{
			{"type", "text"},
			{"id", "info"},
			{"props",
			 QJsonObject{
				 {"format", "markdown"},
				 {"text", QObject::tr("Signature: `%1`\n\nSample: `%2`")
							  .arg(fingerprint, sampleLine)}}}};
		const QJsonObject titleField{
			{"type", "text_field"},
			{"id", "title"},
			{"props",
			 QJsonObject{{"label", QObject::tr("Short title (one sentence)")}}}};
		const QJsonObject patternField{
			{"type", "text_field"},
			{"id", "pattern"},
			{"props",
			 QJsonObject{
				 {"label",
				  QObject::tr(
					  "Regex pattern (leave blank to derive from the sample line)")}}}};
		const QJsonObject adviceField{
			{"type", "text_field"},
			{"id", "advice"},
			{"props",
			 QJsonObject{{"label", QObject::tr("Remediation advice (Markdown)")}}}};
		const QJsonArray buttons{
			QJsonObject{{"type", "button"},
					   {"id", "save"},
					   {"props", QJsonObject{{"label", QObject::tr("Save")}}}},
			QJsonObject{{"type", "button"},
					   {"id", "cancel"},
					   {"props", QJsonObject{{"label", QObject::tr("Cancel")}}}}};
		const QJsonObject buttonRow{
			{"type", "row"}, {"id", "actions"}, {"children", buttons}};
		const QJsonArray children{info, titleField, patternField, adviceField,
								  buttonRow};
		const QJsonObject root{
			{"type", "column"}, {"id", "root"}, {"children", children}};
		const QJsonObject doc{{"type", "mmco-ui/1"}, {"root", root}};
		return QJsonDocument(doc).toJson(QJsonDocument::Compact);
	}
} // namespace

AnalysisPageController::AnalysisPageController(MMCOContext* ctx, QString instanceId,
												QString instanceRoot,
												RuleEngine* engine,
												LearningStore* learning)
	: m_ctx(ctx), m_instanceId(std::move(instanceId)),
	  m_instanceRoot(std::move(instanceRoot)), m_engine(engine),
	  m_learning(learning)
{
}

void AnalysisPageController::notify(int type, const QString& title,
									const QString& message) const
{
	if (!m_ctx || !m_ctx->ui_show_message)
		return;
	m_ctx->ui_show_message(m_ctx->module_handle, type, title.toUtf8().constData(),
						   message.toUtf8().constData());
}

void AnalysisPageController::runAnalysis()
{
	LogIngester ing;
	auto bundle = ing.ingestForInstance(m_instanceRoot);

	m_matches = m_engine->analyse(bundle.combinedText);

	// Score each rule via LearningStore.
	for (auto& m : m_matches) {
		m.score = m_learning->scoreFor(m.ruleId, m_instanceId);
		m_learning->recordSeen(m.ruleId, m_instanceId);
	}
	std::sort(m_matches.begin(), m_matches.end(),
			 [](const Match& a, const Match& b) {
				 if (a.severity != b.severity)
					 return int(a.severity) > int(b.severity);
				 return a.score > b.score;
			 });

	// Compute & remember a fingerprint of the failure for the
	// "promote to rule" affordance.
	m_currentFingerprint = LearningStore::fingerprint(bundle.combinedText);
	m_currentSampleLine.clear();

	// Pull a representative line for the novel promotion UI:
	// first matching `Exception in thread` or first `at <class>.<method>`.
	QRegularExpression re(
		QStringLiteral("(Exception in thread.*|at\\s+[A-Za-z][\\w.$]+)"));
	auto it = re.match(bundle.combinedText);
	if (it.hasMatch())
		m_currentSampleLine = it.captured(1).left(160);

	// Record novel fingerprint if no rule fired but we have a signature.
	if (m_matches.isEmpty() && !m_currentFingerprint.isEmpty()) {
		m_learning->recordNovel(m_currentFingerprint, m_currentSampleLine,
								m_instanceId);
		m_learning->save();
	}

	m_lastSources = bundle.sources;
	m_selectedRuleId.clear();
}

QJsonArray AnalysisPageController::buildRows() const
{
	QJsonArray rows;
	for (const auto& m : m_matches) {
		const QJsonArray cells{
			QString::fromLatin1(severityName(m.severity)), m.ruleTitle,
			m.line >= 0 ? QString::number(m.line) : QStringLiteral("?"),
			QString::number(m.score, 'f', 2)};
		rows.append(QJsonObject{{"id", m.ruleId}, {"cells", cells}});
	}
	return rows;
}

QString AnalysisPageController::buildSummaryText() const
{
	const QString src = m_lastSources.isEmpty()
							? QObject::tr("(no logs found)")
							: m_lastSources.join(QStringLiteral(", "));
	return QObject::tr("Analysed: **%1**. %2 rule match(es). Crash signature: `%3`")
		.arg(src)
		.arg(m_matches.size())
		.arg(m_currentFingerprint.isEmpty() ? QObject::tr("(no stack trace)")
											: m_currentFingerprint);
}

QString AnalysisPageController::buildAdviceText(const Match& m) const
{
	return QStringLiteral("### %1\n\n%2\n\n---\n\n**Matched line:** `%3`\n")
		.arg(m.ruleTitle, m.advice, m.matchedLine);
}

QString AnalysisPageController::noMatchAdviceText() const
{
	return QObject::tr(
		"### No matching rules\n\n"
		"Either the instance ran cleanly or the crash isn't in ErrorOracle's "
		"rule pack yet. If the same signature shows up more than once you can "
		"promote it into a rule using the button below.\n");
}

QJsonObject AnalysisPageController::buildDocument() const
{
	const bool canPromote = m_matches.isEmpty() && !m_currentFingerprint.isEmpty();

	const QJsonObject summaryNode{
		{"type", "text"},
		{"id", "summary"},
		{"props",
		 QJsonObject{{"format", "markdown"}, {"text", buildSummaryText()}}}};

	const QJsonObject listNode{
		{"type", "list"},
		{"id", "matches"},
		{"props",
		 QJsonObject{
			 {"columns", QJsonArray{QObject::tr("Severity"), QObject::tr("Title"),
									QObject::tr("Line"), QObject::tr("Score")}},
			 {"rows", buildRows()}}}};

	const QJsonObject adviceNode{
		{"type", "text"},
		{"id", "advice"},
		{"props",
		 QJsonObject{
			 {"format", "markdown"},
			 {"text", m_matches.isEmpty() ? noMatchAdviceText() : QString()}}}};

	const QJsonArray buttons{
		QJsonObject{{"type", "button"},
				   {"id", "reanalyse"},
				   {"props", QJsonObject{{"label", QObject::tr("Re-analyse")}}}},
		QJsonObject{{"type", "button"},
				   {"id", "helped"},
				   {"props", QJsonObject{{"label", QObject::tr("This fixed it")},
										 {"enabled", false}}}},
		QJsonObject{{"type", "button"},
				   {"id", "didnt_help"},
				   {"props", QJsonObject{{"label", QObject::tr("Didn't help")},
										 {"enabled", false}}}},
		QJsonObject{
			{"type", "button"},
			{"id", "promote"},
			{"props",
			 QJsonObject{{"label", QObject::tr("Promote unknown error to rule…")},
						{"enabled", canPromote}}}}};
	const QJsonObject buttonRow{
		{"type", "row"}, {"id", "actions"}, {"children", buttons}};

	const QJsonArray rootChildren{summaryNode, listNode, adviceNode, buttonRow};
	const QJsonObject root{
		{"type", "column"}, {"id", "root"}, {"children", rootChildren}};
	return QJsonObject{{"type", "mmco-ui/1"}, {"root", root}};
}

void AnalysisPageController::setSummaryText(const QString& text) const
{
	if (!m_ctx || !m_surface)
		return;
	const QJsonObject patch{{"text", text}};
	const QByteArray json = QJsonDocument(patch).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set(m_ctx->module_handle, m_surface, "summary",
						  json.constData());
}

void AnalysisPageController::setAdviceText(const QString& text) const
{
	if (!m_ctx || !m_surface)
		return;
	const QJsonObject patch{{"text", text}};
	const QByteArray json = QJsonDocument(patch).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set(m_ctx->module_handle, m_surface, "advice",
						  json.constData());
}

void AnalysisPageController::pushRows() const
{
	if (!m_ctx || !m_surface)
		return;
	const QByteArray json =
		QJsonDocument(buildRows()).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set_rows(m_ctx->module_handle, m_surface, "matches",
							   json.constData());
}

void AnalysisPageController::setNodeEnabled(const QString& nodeId,
											bool enabled) const
{
	if (!m_ctx || !m_surface)
		return;
	const QJsonObject patch{{"enabled", enabled}};
	const QByteArray json = QJsonDocument(patch).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set(m_ctx->module_handle, m_surface,
						  nodeId.toUtf8().constData(), json.constData());
}

void AnalysisPageController::createSurface()
{
	if (!m_ctx || m_surface)
		return;

	runAnalysis();

	const QByteArray json =
		QJsonDocument(buildDocument()).toJson(QJsonDocument::Compact);
	m_surface = m_ctx->ui_surface_create(
		m_ctx->module_handle, MMCO_UI_ANCHOR_INSTANCE_PAGE,
		m_instanceId.toUtf8().constData(),
		QObject::tr("Error Analysis").toUtf8().constData(), "status-bad",
		json.constData(), &AnalysisPageController::eventTrampoline, this);
}

void AnalysisPageController::destroySurface()
{
	if (!m_ctx || !m_surface)
		return;
	m_ctx->ui_surface_destroy(m_ctx->module_handle, m_surface);
	m_surface = nullptr;
}

void AnalysisPageController::reloadAnalysis()
{
	if (!m_ctx || !m_surface)
		return;

	runAnalysis();

	setSummaryText(buildSummaryText());
	pushRows();
	setAdviceText(m_matches.isEmpty() ? noMatchAdviceText() : QString());
	setNodeEnabled(QStringLiteral("helped"), false);
	setNodeEnabled(QStringLiteral("didnt_help"), false);
	setNodeEnabled(QStringLiteral("promote"),
				  m_matches.isEmpty() && !m_currentFingerprint.isEmpty());
}

Match AnalysisPageController::selectedMatch() const
{
	if (m_selectedRuleId.isEmpty())
		return {};
	for (const auto& m : m_matches)
		if (m.ruleId == m_selectedRuleId)
			return m;
	return {};
}

void AnalysisPageController::onSelectionChanged(const QString& rowId)
{
	m_selectedRuleId = rowId;
	Match m = selectedMatch();
	const bool enable = !m.ruleId.isEmpty();
	setNodeEnabled(QStringLiteral("helped"), enable);
	setNodeEnabled(QStringLiteral("didnt_help"), enable);
	if (enable)
		setAdviceText(buildAdviceText(m));
	else
		setAdviceText(m_matches.isEmpty() ? noMatchAdviceText() : QString());
}

void AnalysisPageController::onReanalyseClicked()
{
	reloadAnalysis();
}

void AnalysisPageController::onHelpedClicked()
{
	Match m = selectedMatch();
	if (m.ruleId.isEmpty() || !m_learning)
		return;
	m_learning->recordHelped(m.ruleId, m_instanceId);
	m_learning->save();
	setSummaryText(
		QObject::tr("Recorded: rule **%1** helped on this instance.").arg(m.ruleTitle));
}

void AnalysisPageController::onDidNotHelpClicked()
{
	Match m = selectedMatch();
	if (m.ruleId.isEmpty() || !m_learning)
		return;
	m_learning->recordDidNotHelp(m.ruleId, m_instanceId);
	m_learning->save();
	setSummaryText(
		QObject::tr("Recorded: rule **%1** did not help.").arg(m.ruleTitle));
}

void AnalysisPageController::onPromoteClicked()
{
	if (!m_ctx || m_currentFingerprint.isEmpty())
		return;

	const QByteArray doc =
		buildPromoteDoc(m_currentFingerprint, m_currentSampleLine);

	char resultBuf[kModalResultBufSize];
	const int rc = m_ctx->ui_modal_run(
		m_ctx->module_handle,
		QObject::tr("Promote crash signature to a user rule").toUtf8().constData(),
		doc.constData(), resultBuf, sizeof(resultBuf));
	if (rc != 0)
		return; /* cancelled / dialog closed */

	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(QByteArray(resultBuf), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return;
	const QJsonObject result = jd.object();
	if (result.value(QStringLiteral("button")).toString() != QLatin1String("save"))
		return;

	const QJsonObject fields = result.value(QStringLiteral("fields")).toObject();
	const QString title = fields.value(QStringLiteral("title")).toString().trimmed();
	QString pattern = fields.value(QStringLiteral("pattern")).toString().trimmed();
	const QString advice =
		fields.value(QStringLiteral("advice")).toString().trimmed();
	if (title.isEmpty() || advice.isEmpty())
		return;

	// Derive a pattern from the sample line if not provided.
	if (pattern.isEmpty() && !m_currentSampleLine.isEmpty()) {
		QString esc = QRegularExpression::escape(m_currentSampleLine);
		pattern = esc.left(120); // pin the prefix
	}
	if (pattern.isEmpty())
		return;

	// Persist into a user rules pack we control.
	QString userRulesDir =
		QString::fromUtf8(/* set by ErrorOraclePlugin.cpp's mmco_init() */
						  qgetenv("MESHMC_USER_RULES_DIR"));
	if (userRulesDir.isEmpty())
		userRulesDir =
			QDir::homePath() + "/.local/share/meshmc/errororacle/userrules";
	QDir().mkpath(userRulesDir);

	QString fileName = "promoted-" + m_currentFingerprint + ".json";
	QFile out(QDir(userRulesDir).filePath(fileName));
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		notify(1, QObject::tr("ErrorOracle"),
			  QObject::tr("Could not write user rule file."));
		return;
	}
	QJsonObject root;
	root["schema"] = 1;
	root["name"] = "User-promoted rules";
	root["version"] = "1.0.0";
	QJsonArray rules;
	QJsonObject rule;
	rule["id"] = "user." + m_currentFingerprint;
	rule["title"] = title;
	rule["severity"] = "medium";
	QJsonArray patterns;
	patterns.append(pattern);
	rule["patterns"] = patterns;
	rule["advice"] = advice;
	QJsonArray tags;
	tags.append("user-promoted");
	rule["tags"] = tags;
	rules.append(rule);
	root["rules"] = rules;
	out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

	if (m_learning) {
		m_learning->forgetNovel(m_currentFingerprint);
		m_learning->save();
	}

	notify(0, QObject::tr("ErrorOracle"),
		  QObject::tr("Saved user rule. Click Re-analyse to load it."));
	setNodeEnabled(QStringLiteral("promote"), false);
}

void AnalysisPageController::eventTrampoline(void* user_data,
											 const char* /*surface_id*/,
											 const char* node_id, const char* event,
											 const char* value_json)
{
	auto* self = static_cast<AnalysisPageController*>(user_data);
	if (!self || !node_id || !event)
		return;
	self->handleEvent(QString::fromUtf8(node_id), QString::fromUtf8(event),
					  value_json ? QString::fromUtf8(value_json) : QString());
}

void AnalysisPageController::handleEvent(const QString& nodeId,
										 const QString& event,
										 const QString& valueJson)
{
	if (event == QLatin1String("click")) {
		if (nodeId == QLatin1String("reanalyse"))
			onReanalyseClicked();
		else if (nodeId == QLatin1String("helped"))
			onHelpedClicked();
		else if (nodeId == QLatin1String("didnt_help"))
			onDidNotHelpClicked();
		else if (nodeId == QLatin1String("promote"))
			onPromoteClicked();
	} else if ((event == QLatin1String("select") ||
			   event == QLatin1String("activate")) &&
			  nodeId == QLatin1String("matches")) {
		onSelectionChanged(jsonStringValue(valueJson));
	}
}
