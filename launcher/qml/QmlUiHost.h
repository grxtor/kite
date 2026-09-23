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

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

#include "core/UiHost.h"

/*
 * One question QmlUiHost is asking, published to QML as `current` while it
 * is unanswered.
 *
 * `kind` decides which of the other properties apply and which of the
 * Q_INVOKABLEs is the right way to answer:
 *   message       -- accept() only (a plain acknowledgement)
 *   confirm       -- accept() / reject()
 *   choose        -- choose(index); actions holds the button labels
 *   text          -- value holds the pre-filled default; accept(QString) /
 *                    reject()
 *   blockedMods   -- blockedMods (live), openDownload(index),
 *                    rescanDownloads(), then accept() / reject()
 *   untrustedMods -- untrustedModsFiles, confirmDelayMs, then
 *                    accept() / reject()
 *   update        -- updateInfo, then answerUpdate("install"|"later"|"skip")
 *   profileSetup  -- profileNameStatus (live), checkProfileName(name),
 *                    submitProfileName(name); accept() fires itself once
 *                    the profile is actually created, reject() cancels
 *   filePicker    -- filePickerMode/filePickerFilter/filePickerDefaultPath,
 *                    then accept(path) / reject() (QML shows a native
 *                    QtQuick.Dialogs file dialog rather than this
 *                    request's own UI for this kind)
 *
 * Ownership is C++'s throughout: QmlUiHost hands this to QML with
 * QQmlEngine::CppOwnership (see exposeToQml() in the .cpp) and deletes it
 * itself once answered, the same guard QmlShell::expose() applies to
 * everything else it publishes.
 */
class QmlUiRequest : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString kind READ kind CONSTANT)
	Q_PROPERTY(QString title READ title CONSTANT)
	Q_PROPERTY(QString text READ text CONSTANT)
	/// "info" | "question" | "warning" | "error"; "info" where UiHost's call
	/// carries no severity of its own (blockedMods, update).
	Q_PROPERTY(QString severity READ severity CONSTANT)
	Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
	Q_PROPERTY(QString rejectLabel READ rejectLabel CONSTANT)
	/// Button labels for kind == "choose"; empty otherwise.
	Q_PROPERTY(QStringList actions READ actions CONSTANT)
	/// kind == "text": the value to pre-fill the field with; empty otherwise.
	Q_PROPERTY(QString value READ value CONSTANT)
	/* kind == "blockedMods": one entry per mod, in the order
	 * resolveBlockedMods() received them -- {fileName, targetPath,
	 * downloadUrl, found}. NOTIFY rather than CONSTANT: QmlUiHost watches
	 * the Downloads folder for as long as this request is pending and
	 * updates `found` live (see QmlUiHost::resolveBlockedMods()). */
	Q_PROPERTY(QVariantList blockedMods READ blockedMods NOTIFY blockedModsChanged)
	/// kind == "untrustedMods": the files in question, instance-relative.
	Q_PROPERTY(QStringList untrustedModsFiles READ untrustedModsFiles CONSTANT)
	/* kind == "untrustedMods": how long, in milliseconds, QML should keep
	 * its accept action out of reach after showing this -- the same
	 * deliberate friction UntrustedModsDialog applies with its checkbox
	 * (see kConfirmDelayMs in UntrustedModsDialog.cpp). Zero for every
	 * other kind. This is a hint for QML to apply; nothing here enforces
	 * it. */
	Q_PROPERTY(int confirmDelayMs READ confirmDelayMs CONSTANT)
	/// kind == "update": currentVersion, availableVersion, releaseNotes
	/// (Markdown, exactly as published -- rendering it is QML's job, the
	/// way HoeDown was the widget dialog's).
	Q_PROPERTY(QVariantMap updateInfo READ updateInfo CONSTANT)
	/* kind == "profileSetup": "unset" | "pending" | "available" | "exists" |
	 * "notAllowed" | "error" -- see checkProfileName()/submitProfileName().
	 * NOTIFY rather than CONSTANT: this is the one other property (besides
	 * blockedMods) that changes live while the request is pending. */
	Q_PROPERTY(QString profileNameStatus READ profileNameStatus NOTIFY
			   profileNameStatusChanged)
	/// kind == "profileSetup": human-readable detail for the status above
	/// ("name too short", "already exists", a server error); empty when
	/// there is nothing to show.
	Q_PROPERTY(QString profileNameError READ profileNameError NOTIFY
			   profileNameStatusChanged)
	/// kind == "profileSetup": true while submitProfileName()'s network
	/// call is in flight -- QML disables its form while this is true.
	Q_PROPERTY(bool profileSubmitting READ profileSubmitting NOTIFY
			   profileSubmittingChanged)
	/// kind == "filePicker": "open" | "save".
	Q_PROPERTY(QString filePickerMode READ filePickerMode CONSTANT)
	/// kind == "filePicker": a Qt filter string, as the plugin gave it.
	Q_PROPERTY(QString filePickerFilter READ filePickerFilter CONSTANT)
	/// kind == "filePicker": FilePickerMode::Save's suggested filename;
	/// empty otherwise.
	Q_PROPERTY(QString filePickerDefaultPath READ filePickerDefaultPath
			   CONSTANT)

  public:
	enum class Kind {
		Message,
		Confirm,
		Choose,
		Text,
		BlockedMods,
		UntrustedMods,
		Update,
		ProfileSetup,
		FilePicker,
	};

	QmlUiRequest(Kind kind, QString title, QString text,
				 QObject* parent = nullptr);

	QString kind() const;
	QString title() const
	{
		return m_title;
	}
	QString text() const
	{
		return m_text;
	}
	QString severity() const
	{
		return m_severity;
	}
	QString acceptLabel() const
	{
		return m_acceptLabel;
	}
	QString rejectLabel() const
	{
		return m_rejectLabel;
	}
	QStringList actions() const
	{
		return m_actions;
	}
	QString value() const
	{
		return m_value;
	}
	QVariantList blockedMods() const;
	QStringList untrustedModsFiles() const
	{
		return m_untrustedModsFiles;
	}
	int confirmDelayMs() const
	{
		return m_confirmDelayMs;
	}
	QVariantMap updateInfo() const
	{
		return m_updateInfo;
	}
	QString profileNameStatus() const
	{
		return m_profileNameStatus;
	}
	QString profileNameError() const
	{
		return m_profileNameError;
	}
	bool profileSubmitting() const
	{
		return m_profileSubmitting;
	}
	QString filePickerMode() const
	{
		return m_filePickerMode;
	}
	QString filePickerFilter() const
	{
		return m_filePickerFilter;
	}
	QString filePickerDefaultPath() const
	{
		return m_filePickerDefaultPath;
	}

	/* Filled in by QmlUiHost before the request is published (from
	 * runRequest()'s caller, never after) -- not reachable from QML, which
	 * only ever sees the getters above. */
	void setSeverity(UiHost::Severity severity);
	void setLabels(QString acceptLabel, QString rejectLabel);
	void setActions(QStringList actions);
	void setValue(QString value);
	void setBlockedMods(const QList<BlockedMod>& mods);
	void setUntrustedModsFiles(QStringList files);
	void setUpdateInfo(QString currentVersion, QString availableVersion,
						QString releaseNotes);
	void setFilePicker(QString mode, QString defaultPath, QString filter);
	/* Read by QmlUiHost's checkNameRequested/submitNameRequested handlers
	 * (which do the actual network work -- see QmlUiHost::setupProfile())
	 * to update what QML sees; not reachable from QML itself, which only
	 * ever sees the getters above. */
	void setProfileNameStatus(QString status, QString error);
	void setProfileSubmitting(bool submitting);
	/* Bumped by checkProfileName() every time it actually starts a network
	 * check (not on the local-validation-only path), before it emits
	 * checkNameRequested(). QmlUiHost::setupProfile()'s connected slot reads
	 * this synchronously -- the connection is direct, so the read happens
	 * inside the same call stack as the increment -- and captures it as the
	 * check's identity; a result that comes back once a newer check has
	 * started is discarded by comparing against this again. Mirrors
	 * ProfileSetupDialog's isChecking/currentCheck guard (see
	 * QmlUiHost::setupProfile()'s comment) without serializing the checks
	 * themselves. */
	int checkSequence() const
	{
		return m_checkSequence;
	}

	/* Read by QmlUiHost once answered() has fired; meaningless before
	 * then. */
	bool accepted() const
	{
		return m_accepted;
	}
	int chosenIndex() const
	{
		return m_chosenIndex;
	}
	UiHost::UpdateChoice updateChoice() const
	{
		return m_updateChoice;
	}
	/// kind == "text": the text accept(QString) was called with; meaningless
	/// unless accepted() is true.
	QString answeredText() const
	{
		return m_answeredText;
	}

	/// kind == "message": the acknowledgement. kind == "confirm" /
	/// "blockedMods" / "untrustedMods": the positive answer.
	Q_INVOKABLE void accept();
	/// kind == "text": commits @p text as the answer -- the counterpart to
	/// accept() above for the one kind that hands back a value rather than
	/// a plain yes.
	Q_INVOKABLE void accept(const QString& text);
	/// The negative answer, or "back out" -- same as closing the widget
	/// dialogs did. Valid for every kind except "choose" and "update",
	/// which have their own invokables below.
	Q_INVOKABLE void reject();
	/// kind == "choose": answers with the index into `actions`, or a
	/// negative index for "backed out" (see UiHost::choose()'s doc
	/// comment).
	Q_INVOKABLE void choose(int index);
	/// kind == "update": one of "install", "later", "skip". Anything else
	/// is treated as "later".
	Q_INVOKABLE void answerUpdate(const QString& choice);
	/// kind == "blockedMods": opens mod @p index's download page in the
	/// system browser, the way BlockedModsDialog's per-row button did.
	/// No-op for an out-of-range index.
	Q_INVOKABLE void openDownload(int index);
	/// kind == "blockedMods": forces an immediate re-check of the
	/// Downloads folder instead of waiting for the next filesystem event.
	Q_INVOKABLE void rescanDownloads();
	/// kind == "profileSetup": checks whether @p name is a valid, available
	/// Minecraft profile name -- validates the shape locally (3-16
	/// letters/digits/underscores, the same rule the widget dialog's field
	/// validator enforces) before asking QmlUiHost to check availability
	/// over the network; updates profileNameStatus either way.
	Q_INVOKABLE void checkProfileName(const QString& name);
	/// kind == "profileSetup": creates the profile with @p name, the way
	/// the widget dialog's OK button does. No-op unless profileNameStatus
	/// is currently "available". Answers the request with accept() on
	/// success; on failure, sets profileNameStatus to "error" and leaves
	/// the request open so the user can try another name.
	Q_INVOKABLE void submitProfileName(const QString& name);

  signals:
	/// One of the answer invokables above ran; QmlUiHost::runRequest() is
	/// waiting on this to leave its event loop.
	void answered();
	void blockedModsChanged();
	/// rescanDownloads() was called; QmlUiHost::resolveBlockedMods()
	/// connects this to the scan it already has running.
	void rescanRequested();
	void profileNameStatusChanged();
	void profileSubmittingChanged();
	/// checkProfileName() passed local validation; QmlUiHost::setupProfile()
	/// connects this to the actual network check.
	void checkNameRequested(const QString& name);
	/// submitProfileName() was called while available; QmlUiHost::setupProfile()
	/// connects this to the actual profile-creation call.
	void submitNameRequested(const QString& name);

  private:
	Kind m_kind;
	QString m_title;
	QString m_text;
	QString m_severity = QStringLiteral("info");
	QString m_acceptLabel;
	QString m_rejectLabel;
	QStringList m_actions;
	QString m_value;
	QString m_answeredText;
	QList<BlockedMod> m_blockedMods;
	QStringList m_untrustedModsFiles;
	int m_confirmDelayMs = 0;
	QVariantMap m_updateInfo;
	QString m_profileNameStatus = QStringLiteral("unset");
	QString m_profileNameError;
	bool m_profileSubmitting = false;
	QString m_filePickerMode;
	QString m_filePickerFilter;
	QString m_filePickerDefaultPath;
	int m_checkSequence = 0;

	bool m_answered = false;
	bool m_accepted = false;
	int m_chosenIndex = -1;
	UiHost::UpdateChoice m_updateChoice = UiHost::UpdateChoice::Later;
};

/*
 * QML implementation of UiHost: a QEventLoop stands in for QDialog::exec(),
 * and QmlUiRequest stands in for the widget dialogs (BlockedModsDialog,
 * UntrustedModsDialog, UpdateAvailableDialog, CustomMessageBox) that
 * previously answered these questions.
 *
 * Each override below builds a QmlUiRequest describing the question,
 * publishes it as `current`, and blocks in runRequest()'s local QEventLoop
 * until QML answers it (a Q_INVOKABLE on the request, which emits
 * answered()) or the request is cancelled -- explicitly, by the application
 * quitting, or by this object itself being destroyed while the request is
 * still outstanding (see runRequest() and the destructor). The request
 * lives on the call's own stack, exactly as long as it is `current`;
 * nothing here ever hands it to QML with any ownership but CppOwnership, so
 * the QML engine never tries to delete it out from under us.
 *
 * RE-ENTRANCY: a call reached while another is already pending -- a
 * background task asking a question while the user is still looking at an
 * earlier one, say -- stacks rather than replacing what is already showing.
 * `current` always names the innermost (most recently asked) request, and
 * QML can only ever answer that one: answering it pops it and its call
 * returns to whichever call it interrupted, which is what makes `current`
 * point back at that older request again. This falls out of every request
 * being an ordinary nested C++ call with its own QEventLoop -- there is no
 * separate queue for requests to get out of order in.
 *
 * BUSY nests the same way: `busy` is true and `busyText` names the most
 * recently started text for as long as any BusyIndicator this object handed
 * out is still alive, in whatever order they end up being destroyed.
 *
 * QUIT / DESTRUCTION: aboutToQuit and the destructor both reject() every
 * request still on the stack, so that a call blocked in runRequest() never
 * outlives the application or this object -- it unwinds and returns the
 * same answer a real "no" would. A request answered this way is read back
 * from its own stack frame regardless of whether this object still exists;
 * runRequest() itself only touches `this` again through a QPointer guard,
 * so resuming after this object is gone updates nothing and crashes
 * nothing.
 *
 * PRESENTER READINESS: this object exists (and is reachable through
 * QmlShell::uiHost()) from the moment QmlShell::show() creates it, which is
 * before the QML engine has even loaded Main.qml, let alone before whatever
 * item there binds to `current` and can actually show a request. A UiHost
 * call reached in that window -- an automatic startup update check finding
 * no updater binary is the one that actually happened -- would otherwise
 * wait forever for an answer nobody could give. So Application::uiHost()
 * (via QmlShell::uiHostInterface()) only routes to this object once
 * `presenterReady` is true, falling back to the widget host until then; the
 * QML item that shows `current` calls setPresenterReady(true) once it is
 * live (e.g. Component.onCompleted), the same way it will one day call
 * setPresenterReady(false) if it is ever torn down first.
 */
class QmlUiHost : public QObject, public UiHost
{
	Q_OBJECT

	/// The innermost pending request, or null. CppOwnership; see the class
	/// comment.
	Q_PROPERTY(QObject* current READ current NOTIFY currentChanged)
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
	/// Text of the most recently started still-active showBusy() call, or
	/// empty when busy is false.
	Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
	/* Whether some QML item is actually watching `current` and able to
	 * answer a request -- see the class comment's PRESENTER READINESS
	 * section. False until that item calls setPresenterReady(true). */
	Q_PROPERTY(bool presenterReady READ presenterReady WRITE setPresenterReady
			   NOTIFY presenterReadyChanged)

  public:
	explicit QmlUiHost(QObject* parent = nullptr);
	~QmlUiHost() override;

	std::unique_ptr<BusyIndicator> showBusy(const QString& text) override;

	void message(const QString& title, const QString& text,
				 Severity severity) override;

	bool confirm(const QString& title, const QString& text,
				 Severity severity, const QString& acceptLabel = QString(),
				 const QString& rejectLabel = QString()) override;

	int choose(const QString& title, const QString& text, Severity severity,
			   const QStringList& actions) override;

	std::optional<QString> askText(
		const QString& title, const QString& text,
		const QString& defaultValue = QString()) override;

	bool resolveBlockedMods(const QString& title, const QString& text,
							QList<BlockedMod>& mods) override;

	bool confirmUntrustedMods(const QStringList& suspectPaths) override;

	UpdateChoice offerUpdate(const QString& currentVersion,
							 const QString& availableVersion,
							 const QString& releaseNotes) override;

	bool setupProfile(MinecraftAccountPtr account) override;

	std::optional<QString> pickFile(FilePickerMode mode, const QString& title,
									const QString& defaultPath,
									const QString& filter) override;

	QObject* current() const;
	bool busy() const;
	QString busyText() const;
	bool presenterReady() const;
	/// Called by the QML item that shows `current` -- see PRESENTER
	/// READINESS in the class comment. Application/QmlShell read this
	/// through uiHostInterface() to decide whether this object is safe to
	/// route UiHost calls to yet.
	Q_INVOKABLE void setPresenterReady(bool ready);

	/* Called by the BusyIndicator showBusy() hands out, from its
	 * destructor (a QPointer guards against this object already being
	 * gone). Not meant for any other caller. */
	void endBusy(int id);

  signals:
	void currentChanged();
	void busyChanged();
	void presenterReadyChanged();

  private:
	/* Publishes @p request as `current`, blocks until it is answered or
	 * cancelled, then un-publishes it. See the class comment for the
	 * re-entrancy and quit/destruction rules this implements. */
	void runRequest(QmlUiRequest& request);

	QList<QmlUiRequest*> m_stack;
	bool m_presenterReady = false;

	struct BusyEntry {
		int id;
		QString text;
	};
	QList<BusyEntry> m_busy;
	int m_nextBusyId = 0;
};
