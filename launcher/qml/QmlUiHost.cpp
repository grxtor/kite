/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include "qml/QmlUiHost.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

#include "BuildConfig.h"
#include "DesktopServices.h"
#include "modplatform/flame/FlameApi.h"

namespace
{
	QString severityToString(UiHost::Severity severity)
	{
		switch (severity) {
			case UiHost::Severity::Information:
				return QStringLiteral("info");
			case UiHost::Severity::Question:
				return QStringLiteral("question");
			case UiHost::Severity::Warning:
				return QStringLiteral("warning");
			case UiHost::Severity::Critical:
				return QStringLiteral("error");
		}
		return QStringLiteral("info");
	}

	/* Same guard QmlShell::expose() applies to everything it hands to
	 * QML: without it, the engine would try to garbage-collect a QObject
	 * that a C++ stack frame -- here, whichever UiHost call is still
	 * running -- still owns. Kept local rather than reusing
	 * QmlShell::expose() so this file does not need to know QmlShell
	 * exists. */
	QObject* exposeToQml(QObject* object)
	{
		if (object) {
			QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
		}
		return object;
	}

	/* Indeterminate and uncancellable, like WidgetUiHost's own
	 * ProgressDialogBusy -- there is nothing here for the caller to
	 * report progress on or to abort. */
	class BusyToken final : public UiHost::BusyIndicator
	{
	  public:
		BusyToken(QmlUiHost* host, int id) : m_host(host), m_id(id)
		{
		}

		~BusyToken() override
		{
			if (m_host) {
				m_host->endBusy(m_id);
			}
		}

	  private:
		QPointer<QmlUiHost> m_host;
		int m_id;
	};
} // namespace

/* ---------------------------------------------------------------------- */
/* QmlUiRequest                                                            */
/* ---------------------------------------------------------------------- */

QmlUiRequest::QmlUiRequest(Kind kind, QString title, QString text,
						   QObject* parent)
	: QObject(parent), m_kind(kind), m_title(std::move(title)),
	  m_text(std::move(text))
{
	if (m_kind == Kind::UntrustedMods) {
		/* Same friction UntrustedModsDialog applies with its checkbox --
		 * see kConfirmDelayMs there. */
		m_confirmDelayMs = 3000;
	}
}

QString QmlUiRequest::kind() const
{
	switch (m_kind) {
		case Kind::Message:
			return QStringLiteral("message");
		case Kind::Confirm:
			return QStringLiteral("confirm");
		case Kind::Choose:
			return QStringLiteral("choose");
		case Kind::Text:
			return QStringLiteral("text");
		case Kind::BlockedMods:
			return QStringLiteral("blockedMods");
		case Kind::UntrustedMods:
			return QStringLiteral("untrustedMods");
		case Kind::Update:
			return QStringLiteral("update");
	}
	return QString();
}

void QmlUiRequest::setSeverity(UiHost::Severity severity)
{
	m_severity = severityToString(severity);
}

void QmlUiRequest::setLabels(QString acceptLabel, QString rejectLabel)
{
	m_acceptLabel = std::move(acceptLabel);
	m_rejectLabel = std::move(rejectLabel);
}

void QmlUiRequest::setActions(QStringList actions)
{
	m_actions = std::move(actions);
}

void QmlUiRequest::setValue(QString value)
{
	m_value = std::move(value);
}

void QmlUiRequest::setBlockedMods(const QList<BlockedMod>& mods)
{
	m_blockedMods = mods;
	emit blockedModsChanged();
}

void QmlUiRequest::setUntrustedModsFiles(QStringList files)
{
	m_untrustedModsFiles = std::move(files);
}

void QmlUiRequest::setUpdateInfo(QString currentVersion,
								 QString availableVersion,
								 QString releaseNotes)
{
	m_updateInfo = QVariantMap{
		{ QStringLiteral("currentVersion"), currentVersion },
		{ QStringLiteral("availableVersion"), availableVersion },
		{ QStringLiteral("releaseNotes"), releaseNotes },
	};
}

QVariantList QmlUiRequest::blockedMods() const
{
	QVariantList list;
	list.reserve(m_blockedMods.size());
	for (const auto& mod : m_blockedMods) {
		list.append(QVariantMap{
			{ QStringLiteral("fileName"), mod.fileName },
			{ QStringLiteral("targetPath"), mod.targetPath },
			{ QStringLiteral("downloadUrl"),
			  FlameApi::browserDownloadUrl(QString::number(mod.projectId),
										   QString::number(mod.fileId)) },
			{ QStringLiteral("found"), mod.found },
		});
	}
	return list;
}

void QmlUiRequest::accept()
{
	if (m_answered) {
		return;
	}
	m_answered = true;
	m_accepted = true;
	emit answered();
}

void QmlUiRequest::accept(const QString& text)
{
	if (m_answered) {
		return;
	}
	m_answered = true;
	m_accepted = true;
	m_answeredText = text;
	emit answered();
}

void QmlUiRequest::reject()
{
	if (m_answered) {
		return;
	}
	m_answered = true;
	m_accepted = false;
	m_chosenIndex = -1;
	m_updateChoice = UiHost::UpdateChoice::Later;
	emit answered();
}

void QmlUiRequest::choose(int index)
{
	if (m_answered) {
		return;
	}
	m_answered = true;
	m_chosenIndex = index;
	m_accepted = index >= 0;
	emit answered();
}

void QmlUiRequest::answerUpdate(const QString& choice)
{
	if (m_answered) {
		return;
	}
	m_answered = true;
	if (choice == QLatin1String("install")) {
		m_updateChoice = UiHost::UpdateChoice::Install;
	} else if (choice == QLatin1String("skip")) {
		m_updateChoice = UiHost::UpdateChoice::Skip;
	} else {
		m_updateChoice = UiHost::UpdateChoice::Later;
	}
	emit answered();
}

void QmlUiRequest::openDownload(int index)
{
	if (index < 0 || index >= m_blockedMods.size()) {
		return;
	}
	const auto& mod = m_blockedMods[index];
	const QString url = FlameApi::browserDownloadUrl(
		QString::number(mod.projectId), QString::number(mod.fileId));
	DesktopServices::openUrl(QUrl(url));
}

void QmlUiRequest::rescanDownloads()
{
	emit rescanRequested();
}

/* ---------------------------------------------------------------------- */
/* QmlUiHost                                                               */
/* ---------------------------------------------------------------------- */

QmlUiHost::QmlUiHost(QObject* parent) : QObject(parent)
{
}

QmlUiHost::~QmlUiHost()
{
	/* Every request still on the stack belongs to a runRequest() call
	 * further down the C++ call stack (see the class comment) -- still
	 * perfectly valid objects, just not owned by this one. Rejecting each
	 * lets those calls unwind instead of waiting forever for an answer
	 * nobody can give once this object is gone; runRequest() notices this
	 * object is gone (QPointer) when it resumes and skips touching it. */
	for (QmlUiRequest* request : std::as_const(m_stack)) {
		request->reject();
	}
}

QObject* QmlUiHost::current() const
{
	return m_stack.isEmpty() ? nullptr : exposeToQml(m_stack.last());
}

bool QmlUiHost::presenterReady() const
{
	return m_presenterReady;
}

void QmlUiHost::setPresenterReady(bool ready)
{
	if (m_presenterReady == ready) {
		return;
	}
	m_presenterReady = ready;
	emit presenterReadyChanged();
}

bool QmlUiHost::busy() const
{
	return !m_busy.isEmpty();
}

QString QmlUiHost::busyText() const
{
	return m_busy.isEmpty() ? QString() : m_busy.last().text;
}

void QmlUiHost::endBusy(int id)
{
	for (int i = 0; i < m_busy.size(); ++i) {
		if (m_busy[i].id == id) {
			m_busy.removeAt(i);
			emit busyChanged();
			return;
		}
	}
}

std::unique_ptr<UiHost::BusyIndicator> QmlUiHost::showBusy(const QString& text)
{
	const int id = m_nextBusyId++;
	m_busy.append({ id, text });
	emit busyChanged();
	return std::make_unique<BusyToken>(this, id);
}

void QmlUiHost::runRequest(QmlUiRequest& request)
{
	/* Diagnostic for exactly the failure PRESENTER READINESS (see the class
	 * comment) guards against: a call reaching this object before anything
	 * routes here only once ready, or a future bug in that gate, hangs
	 * silently otherwise -- this line is what tells a log reader which
	 * call it was. */
	qDebug() << "QmlUiHost: asking" << request.kind() << "-" << request.title();

	QEventLoop loop;
	/* Queued rather than the default direct connection: a QML binding
	 * reacting to currentChanged() below could in principle answer the
	 * request synchronously, before this function reaches loop.exec() --
	 * a direct connection would call loop.quit() before there is a loop
	 * to quit yet. Queuing means the call is delivered once the loop is
	 * actually pumping events, whichever order these end up running in. */
	connect(&request, &QmlUiRequest::answered, &loop, &QEventLoop::quit,
			Qt::QueuedConnection);
	/* A quit is answered the same way reject() answers it, so a task
	 * blocked here cannot keep the application from exiting -- it sees
	 * the same "gave up" result a real "no" would have produced. */
	connect(qApp, &QCoreApplication::aboutToQuit, &loop,
			[&request]() { request.reject(); }, Qt::QueuedConnection);

	m_stack.append(&request);
	emit currentChanged();

	/* Constructed *before* loop.exec(), while this object is definitely
	 * still alive: QPointer only guards safely against a destruction it
	 * was watching for from the start -- building one from `this` after
	 * the fact, once this object might already be gone, would dereference
	 * freed memory instead of reporting null. */
	QPointer<QmlUiHost> self(this);

	loop.exec();

	/* This object may already be gone -- destroyed while `request` was
	 * still pending, which rejected it exactly the way the branch above
	 * does (see the destructor) and is why loop.exec() just returned.
	 * Nothing below may run in that case. */
	if (self) {
		Q_ASSERT(!m_stack.isEmpty() && m_stack.last() == &request);
		m_stack.removeLast();
		emit currentChanged();
	}
}

void QmlUiHost::message(const QString& title, const QString& text,
						Severity severity)
{
	QmlUiRequest request(QmlUiRequest::Kind::Message, title, text);
	request.setSeverity(severity);
	runRequest(request);
}

bool QmlUiHost::confirm(const QString& title, const QString& text,
						Severity severity, const QString& acceptLabel,
						const QString& rejectLabel)
{
	QmlUiRequest request(QmlUiRequest::Kind::Confirm, title, text);
	request.setSeverity(severity);
	request.setLabels(acceptLabel, rejectLabel);
	runRequest(request);
	return request.accepted();
}

int QmlUiHost::choose(const QString& title, const QString& text,
					  Severity severity, const QStringList& actions)
{
	QmlUiRequest request(QmlUiRequest::Kind::Choose, title, text);
	request.setSeverity(severity);
	request.setActions(actions);
	runRequest(request);
	return request.chosenIndex();
}

std::optional<QString> QmlUiHost::askText(const QString& title,
										   const QString& text,
										   const QString& defaultValue)
{
	QmlUiRequest request(QmlUiRequest::Kind::Text, title, text);
	request.setValue(defaultValue);
	runRequest(request);
	if (!request.accepted()) {
		return std::nullopt;
	}
	return request.answeredText();
}

bool QmlUiHost::resolveBlockedMods(const QString& title, const QString& text,
								   QList<BlockedMod>& mods)
{
	QmlUiRequest request(QmlUiRequest::Kind::BlockedMods, title, text);
	request.setBlockedMods(mods);

	/* Same Downloads-folder watch BlockedModsDialog::setupWatch() and
	 * scanDownloadsFolder() run, kept here rather than in QmlUiRequest so
	 * the request stays a plain data-plus-answer object -- this is the
	 * one kind whose data changes while it is pending, not just once. */
	const QString downloadDir =
		QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
	QFileSystemWatcher watcher;
	auto rescan = [&]() {
		if (downloadDir.isEmpty()) {
			return;
		}
		const QStringList files = QDir(downloadDir).entryList(QDir::Files);
		bool changed = false;
		for (auto& mod : mods) {
			if (!mod.found && files.contains(mod.fileName)) {
				mod.found = true;
				changed = true;
			}
		}
		if (changed) {
			request.setBlockedMods(mods);
		}
	};
	if (!downloadDir.isEmpty() && QDir(downloadDir).exists()) {
		watcher.addPath(downloadDir);
		connect(&watcher, &QFileSystemWatcher::directoryChanged, &watcher,
				[&rescan](const QString&) { rescan(); });
	}
	connect(&request, &QmlUiRequest::rescanRequested, &request,
			[&rescan]() { rescan(); });
	rescan(); // same initial scan the widget dialog runs from its constructor

	runRequest(request);
	return request.accepted();
}

bool QmlUiHost::confirmUntrustedMods(const QStringList& suspectPaths)
{
	/* WidgetUiHost's version of this question carries no title/text of its
	 * own -- UntrustedModsDialog.ui hardcodes them -- so this reproduces
	 * that copy verbatim for QML to show the same way it shows any other
	 * request's title/text. */
	QmlUiRequest request(
		QmlUiRequest::Kind::UntrustedMods, tr("Easy There!"),
		tr("This modpack installs code that is not hosted on Modrinth or "
		   "CurseForge - either downloaded from another host, or carried "
		   "inside the pack itself.\n\n"
		   "Malicious mods are often distributed through links sent on "
		   "platforms such as Discord. We strongly recommend only "
		   "importing modpacks from trusted sources."));
	request.setSeverity(Severity::Warning);
	request.setLabels(tr("Install anyway"), QString());
	request.setUntrustedModsFiles(suspectPaths);
	runRequest(request);
	return request.accepted();
}

UiHost::UpdateChoice QmlUiHost::offerUpdate(const QString& currentVersion,
											const QString& availableVersion,
											const QString& releaseNotes)
{
	QmlUiRequest request(
		QmlUiRequest::Kind::Update,
		tr("A new version of %1 is available!")
			.arg(BuildConfig.MESHMC_DISPLAYNAME),
		tr("Version %1 is now available - you have %2 . Would you like to "
		   "download it now?")
			.arg(availableVersion, currentVersion));
	request.setUpdateInfo(currentVersion, availableVersion, releaseNotes);
	runRequest(request);
	return request.updateChoice();
}
