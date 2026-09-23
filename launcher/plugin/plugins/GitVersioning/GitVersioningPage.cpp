/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "GitVersioningPage.h"

namespace
{
	constexpr int kModalResultBufSize = 4096;

	QString humanSize(qint64 bytes)
	{
		if (bytes < 1024)
			return QObject::tr("%1 B").arg(bytes);
		double v = bytes / 1024.0;
		if (v < 1024.0)
			return QObject::tr("%1 KiB").arg(QString::number(v, 'f', 1));
		v /= 1024.0;
		if (v < 1024.0)
			return QObject::tr("%1 MiB").arg(QString::number(v, 'f', 1));
		v /= 1024.0;
		return QObject::tr("%1 GiB").arg(QString::number(v, 'f', 2));
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

	/* Shared by the Snapshot-message and Tag-name prompts: a single
	 * text_field plus an OK/Cancel button row, run through ui_modal_run.
	 * Replaces the raw QInputDialog::getText calls the QWidget-era page
	 * used to make directly — the page has no QWidget of its own to
	 * parent a dialog to any more. */
	QByteArray buildTextPromptDoc(const QString& fieldId, const QString& label,
								 const QString& defaultValue,
								 const QString& okId, const QString& okLabel)
	{
		const QJsonObject field{
			{"type", "text_field"},
			{"id", fieldId},
			{"props", QJsonObject{{"label", label}, {"value", defaultValue}}}};
		const QJsonArray buttons{
			QJsonObject{{"type", "button"},
					   {"id", okId},
					   {"props", QJsonObject{{"label", okLabel}}}},
			QJsonObject{{"type", "button"},
					   {"id", "cancel"},
					   {"props", QJsonObject{{"label", QObject::tr("Cancel")}}}}};
		const QJsonObject buttonRow{
			{"type", "row"}, {"id", "actions"}, {"children", buttons}};
		const QJsonArray children{field, buttonRow};
		const QJsonObject root{
			{"type", "column"}, {"id", "root"}, {"children", children}};
		const QJsonObject doc{{"type", "mmco-ui/1"}, {"root", root}};
		return QJsonDocument(doc).toJson(QJsonDocument::Compact);
	}
} // namespace

GitVersioningPageController::GitVersioningPageController(MMCOContext* ctx,
														 QString instanceId,
														 QString instanceRoot)
	: m_ctx(ctx), m_instanceId(std::move(instanceId)),
	  m_instanceRoot(std::move(instanceRoot)),
	  m_repo(m_instanceId, m_instanceRoot)
{
}

bool GitVersioningPageController::confirm(const QString& title,
										  const QString& message) const
{
	if (!m_ctx || !m_ctx->ui_confirm_dialog)
		return false;
	return m_ctx->ui_confirm_dialog(m_ctx->module_handle, title.toUtf8().constData(),
									message.toUtf8().constData()) != 0;
}

void GitVersioningPageController::notify(int type, const QString& title,
										 const QString& message) const
{
	if (!m_ctx || !m_ctx->ui_show_message)
		return;
	m_ctx->ui_show_message(m_ctx->module_handle, type, title.toUtf8().constData(),
						   message.toUtf8().constData());
}

QString GitVersioningPageController::buildStatusText() const
{
	if (!GitRepo::gitAvailable()) {
		return QObject::tr(
			"**git** is not installed on your system. Install Git and reopen "
			"this page.");
	}

	auto st = m_repo.status();
	if (!st.initialized) {
		return QObject::tr(
			"No version history yet. Snapshot now to start tracking changes "
			"to this instance.");
	}

	QStringList parts;
	parts << QObject::tr("HEAD: **%1**").arg(st.head);
	if (st.dirty) {
		parts << QObject::tr("Modified: %1, Deleted: %2, Untracked: %3")
					 .arg(st.modifiedCount)
					 .arg(st.deletedCount)
					 .arg(st.untrackedCount);
	} else {
		parts << QObject::tr("Clean working tree");
	}
	parts << QObject::tr("git %1").arg(GitRepo::gitVersion());
	return parts.join(QStringLiteral(" — "));
}

QJsonArray GitVersioningPageController::buildRows() const
{
	QJsonArray rows;
	// Match the BackupSystem plugin's fixed timestamp format — sortable
	// and unambiguous across locales. Qt6 removed
	// Qt::DefaultLocaleShortDate so an explicit format string is the
	// most portable choice anyway.
	for (const auto& c : m_commits) {
		QString subject = c.subject;
		if (c.isPreLaunch)
			subject = QStringLiteral("⚡ ") + subject;
		const QJsonArray cells{
			c.when.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), c.sha,
			subject, QString::number(c.filesChanged), humanSize(c.sizeAdded),
			humanSize(c.sizeRemoved)};
		rows.append(QJsonObject{{"id", c.fullSha}, {"cells", cells}});
	}
	return rows;
}

QJsonObject GitVersioningPageController::buildDocument() const
{
	const bool gitOk = GitRepo::gitAvailable();

	const QJsonObject statusNode{
		{"type", "text"},
		{"id", "status"},
		{"props",
		 QJsonObject{{"format", "markdown"}, {"text", buildStatusText()}}}};

	const QJsonObject listNode{
		{"type", "list"},
		{"id", "commits"},
		{"props",
		 QJsonObject{
			 {"columns", QJsonArray{QObject::tr("When"), QObject::tr("Commit"),
									QObject::tr("Subject"), QObject::tr("Files"),
									QObject::tr("+"), QObject::tr("-")}},
			 {"rows", gitOk ? buildRows() : QJsonArray{}}}}};

	const QJsonArray buttons{
		QJsonObject{{"type", "button"},
				   {"id", "snapshot"},
				   {"props", QJsonObject{{"label", QObject::tr("Snapshot now")},
										 {"enabled", gitOk}}}},
		QJsonObject{{"type", "button"},
				   {"id", "restore"},
				   {"props", QJsonObject{{"label", QObject::tr("Restore selected")},
										 {"enabled", false}}}},
		QJsonObject{{"type", "button"},
				   {"id", "tag"},
				   {"props", QJsonObject{{"label", QObject::tr("Tag…")},
										 {"enabled", false}}}},
		QJsonObject{{"type", "button"},
				   {"id", "drop"},
				   {"props", QJsonObject{{"label", QObject::tr("Drop last commit")},
										 {"enabled", gitOk}}}},
		QJsonObject{{"type", "button"},
				   {"id", "refresh"},
				   {"props", QJsonObject{{"label", QObject::tr("Refresh")}}}}};
	const QJsonObject buttonRow{
		{"type", "row"}, {"id", "buttons"}, {"children", buttons}};

	const QJsonArray rootChildren{statusNode, listNode, buttonRow};
	const QJsonObject root{
		{"type", "column"}, {"id", "root"}, {"children", rootChildren}};
	return QJsonObject{{"type", "mmco-ui/1"}, {"root", root}};
}

void GitVersioningPageController::setStatusText(const QString& text) const
{
	if (!m_ctx || !m_surface)
		return;
	const QJsonObject patch{{"text", text}};
	const QByteArray json = QJsonDocument(patch).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set(m_ctx->module_handle, m_surface, "status",
						  json.constData());
}

void GitVersioningPageController::pushRows() const
{
	if (!m_ctx || !m_surface)
		return;
	const QByteArray json =
		QJsonDocument(buildRows()).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set_rows(m_ctx->module_handle, m_surface, "commits",
							   json.constData());
}

void GitVersioningPageController::setNodeEnabled(const QString& nodeId,
												 bool enabled) const
{
	if (!m_ctx || !m_surface)
		return;
	const QJsonObject patch{{"enabled", enabled}};
	const QByteArray json = QJsonDocument(patch).toJson(QJsonDocument::Compact);
	m_ctx->ui_surface_set(m_ctx->module_handle, m_surface,
						  nodeId.toUtf8().constData(), json.constData());
}

void GitVersioningPageController::createSurface()
{
	if (!m_ctx || m_surface)
		return;

	if (GitRepo::gitAvailable()) {
		m_commits = m_repo.log();
		for (auto& c : m_commits)
			m_repo.fillCommitStats(c);
	} else {
		m_commits.clear();
	}

	const QByteArray json =
		QJsonDocument(buildDocument()).toJson(QJsonDocument::Compact);
	m_surface = m_ctx->ui_surface_create(
		m_ctx->module_handle, MMCO_UI_ANCHOR_INSTANCE_PAGE,
		m_instanceId.toUtf8().constData(),
		QObject::tr("Version History").toUtf8().constData(), "git-scm",
		json.constData(), &GitVersioningPageController::eventTrampoline, this);
}

void GitVersioningPageController::destroySurface()
{
	if (!m_ctx || !m_surface)
		return;
	m_ctx->ui_surface_destroy(m_ctx->module_handle, m_surface);
	m_surface = nullptr;
}

void GitVersioningPageController::reloadHistory()
{
	if (!m_ctx || !m_surface)
		return;

	setStatusText(buildStatusText());

	const bool gitOk = GitRepo::gitAvailable();
	if (!gitOk) {
		setNodeEnabled(QStringLiteral("snapshot"), false);
		setNodeEnabled(QStringLiteral("restore"), false);
		setNodeEnabled(QStringLiteral("tag"), false);
		setNodeEnabled(QStringLiteral("drop"), false);
		return;
	}

	m_commits = m_repo.log();
	for (auto& c : m_commits)
		m_repo.fillCommitStats(c);
	pushRows();

	/* The row set was just replaced wholesale, so — same as the old
	 * QTreeWidget::clear() used to do — nothing is selected any more. */
	m_selectedSha.clear();
	setNodeEnabled(QStringLiteral("snapshot"), true);
	setNodeEnabled(QStringLiteral("restore"), false);
	setNodeEnabled(QStringLiteral("tag"), false);
	setNodeEnabled(QStringLiteral("drop"), true);
}

GitCommit GitVersioningPageController::selectedCommit() const
{
	if (m_selectedSha.isEmpty())
		return {};
	for (const auto& c : m_commits)
		if (c.fullSha == m_selectedSha)
			return c;
	return {};
}

void GitVersioningPageController::onSelectionChanged(const QString& rowId)
{
	m_selectedSha = rowId;
	const bool has = !rowId.isEmpty();
	setNodeEnabled(QStringLiteral("restore"), has);
	setNodeEnabled(QStringLiteral("tag"), has);
}

void GitVersioningPageController::onSnapshotClicked()
{
	if (!m_ctx)
		return;

	const QString defaultMsg =
		QObject::tr("Manual snapshot %1")
			.arg(QDateTime::currentDateTime().toString(
				QStringLiteral("yyyy-MM-dd HH:mm")));
	const QByteArray doc = buildTextPromptDoc(
		QStringLiteral("message"),
		QObject::tr("Describe the changes you're snapshotting:"), defaultMsg,
		QStringLiteral("snapshot"), QObject::tr("Snapshot"));

	char resultBuf[kModalResultBufSize];
	const int rc = m_ctx->ui_modal_run(
		m_ctx->module_handle, QObject::tr("Snapshot").toUtf8().constData(),
		doc.constData(), resultBuf, sizeof(resultBuf));
	if (rc != 0)
		return; /* cancelled / dialog closed */

	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(QByteArray(resultBuf), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return;
	const QJsonObject result = jd.object();
	if (result.value(QStringLiteral("button")).toString() !=
		QLatin1String("snapshot"))
		return;
	const QString message = result.value(QStringLiteral("fields"))
								 .toObject()
								 .value(QStringLiteral("message"))
								 .toString();

	QString errMsg;
	QString sha = m_repo.commit(message, /*isPreLaunch=*/false, &errMsg);
	if (sha.isEmpty() && !errMsg.isEmpty()) {
		notify(1, QObject::tr("Snapshot failed"), errMsg);
	} else if (sha.isEmpty()) {
		notify(0, QObject::tr("Snapshot"),
			  QObject::tr("Nothing to commit — the working tree was clean."));
	}
	reloadHistory();
}

void GitVersioningPageController::onRestoreClicked()
{
	GitCommit c = selectedCommit();
	if (c.fullSha.isEmpty())
		return;
	if (!confirm(QObject::tr("Restore?"),
				QObject::tr("Restore the instance to commit %1 (%2)?\n\n"
							"An auto-snapshot of the current state is taken "
							"first, so this can be undone by restoring the "
							"previous snapshot.")
					.arg(c.sha, c.subject)))
		return;
	QString errMsg;
	if (!m_repo.restore(c.fullSha, &errMsg))
		notify(1, QObject::tr("Restore failed"), errMsg);
	reloadHistory();
}

void GitVersioningPageController::onTagClicked()
{
	if (!m_ctx)
		return;
	GitCommit c = selectedCommit();
	if (c.fullSha.isEmpty())
		return;

	const QString defaultName = QStringLiteral("milestone-%1")
									.arg(c.when.toString(QStringLiteral("yyyyMMdd")));
	const QByteArray doc = buildTextPromptDoc(
		QStringLiteral("name"), QObject::tr("Tag name:"), defaultName,
		QStringLiteral("tag"), QObject::tr("Tag"));

	char resultBuf[kModalResultBufSize];
	const int rc = m_ctx->ui_modal_run(
		m_ctx->module_handle, QObject::tr("Tag commit").toUtf8().constData(),
		doc.constData(), resultBuf, sizeof(resultBuf));
	if (rc != 0)
		return;

	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(QByteArray(resultBuf), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject())
		return;
	const QJsonObject result = jd.object();
	if (result.value(QStringLiteral("button")).toString() != QLatin1String("tag"))
		return;
	const QString name = result.value(QStringLiteral("fields"))
							 .toObject()
							 .value(QStringLiteral("name"))
							 .toString();
	if (name.isEmpty())
		return;

	QString errMsg;
	if (!m_repo.tag(name, c.fullSha, &errMsg))
		notify(1, QObject::tr("Tag failed"), errMsg);
}

void GitVersioningPageController::onDropClicked()
{
	if (!confirm(QObject::tr("Drop last commit?"),
				QObject::tr("This hard-resets HEAD by one commit. The working "
							"tree is reset to the parent commit too. Continue?")))
		return;
	QString errMsg;
	if (!m_repo.dropHead(&errMsg))
		notify(1, QObject::tr("Drop failed"), errMsg);
	reloadHistory();
}

void GitVersioningPageController::eventTrampoline(void* user_data,
												  const char* /*surface_id*/,
												  const char* node_id,
												  const char* event,
												  const char* value_json)
{
	auto* self = static_cast<GitVersioningPageController*>(user_data);
	if (!self || !node_id || !event)
		return;
	self->handleEvent(QString::fromUtf8(node_id), QString::fromUtf8(event),
					  value_json ? QString::fromUtf8(value_json) : QString());
}

void GitVersioningPageController::handleEvent(const QString& nodeId,
											  const QString& event,
											  const QString& valueJson)
{
	if (event == QLatin1String("click")) {
		if (nodeId == QLatin1String("snapshot"))
			onSnapshotClicked();
		else if (nodeId == QLatin1String("restore"))
			onRestoreClicked();
		else if (nodeId == QLatin1String("tag"))
			onTagClicked();
		else if (nodeId == QLatin1String("drop"))
			onDropClicked();
		else if (nodeId == QLatin1String("refresh"))
			reloadHistory();
	} else if ((event == QLatin1String("select") ||
			   event == QLatin1String("activate")) &&
			  nodeId == QLatin1String("commits")) {
		onSelectionChanged(jsonStringValue(valueJson));
	}
}
