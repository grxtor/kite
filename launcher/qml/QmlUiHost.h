/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <functional>
#include <memory>

#include "core/UiHost.h"

/*
 * One question QmlUiHost is asking, published to QML as `current` while it
 * is unanswered. `kind` decides which properties apply and how to answer:
 *   message       -- accept()
 *   confirm       -- accept() / reject()
 *   choose        -- choose(index); actions holds the button labels
 *   text          -- value is the pre-filled default; accept(QString) / reject()
 *   blockedMods   -- blockedMods (live), openDownload(index),
 *                    rescanDownloads(), then accept() / reject()
 *   untrustedMods -- untrustedModsFiles, confirmDelayMs, then accept() / reject()
 *   update        -- updateInfo, then answerUpdate("install"|"later"|"skip")
 *   profileSetup  -- profileNameStatus (live), checkProfileName(name),
 *                    submitProfileName(name); accept() fires itself once the
 *                    profile is created, reject() cancels
 *   filePicker    -- filePickerMode/Filter/DefaultPath, then accept(path) /
 *                    reject(); QML shows a native file dialog for this kind
 *   pickAccount   -- accountOptions, then chooseAccount(index, makeDefault) /
 *                    reject()
 *   optionalMods  -- optionalMods (an AtlOptionalModsModel ticked in place),
 *                    then accept() to install what is ticked / reject() to
 *                    keep the pack's defaults
 *   modal         -- modalDocument, drawn with PluginNode.qml, which reports
 *                    user actions through sendEvent() (this request is its
 *                    `sink`); a "click" answers it, reject() backs out
 *
 * C++ owns it (QQmlEngine::CppOwnership) and deletes it once answered.
 */
class QmlUiRequest : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString kind READ kind CONSTANT)
	Q_PROPERTY(QString title READ title CONSTANT)
	Q_PROPERTY(QString text READ text CONSTANT)
	/// "info" | "question" | "warning" | "error"; "info" where UiHost's call
	/// carries none (blockedMods, update).
	Q_PROPERTY(QString severity READ severity CONSTANT)
	Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
	Q_PROPERTY(QString rejectLabel READ rejectLabel CONSTANT)
	/// kind == "choose": the button labels.
	Q_PROPERTY(QStringList actions READ actions CONSTANT)
	/// kind == "text": the value to pre-fill the field with.
	Q_PROPERTY(QString value READ value CONSTANT)
	/* kind == "blockedMods": {fileName, targetPath, downloadUrl, found} per
	 * mod. NOTIFY because QmlUiHost watches the Downloads folder while the
	 * request is pending and updates `found` live. */
	Q_PROPERTY(QVariantList blockedMods READ blockedMods NOTIFY blockedModsChanged)
	/// kind == "untrustedMods": the files in question, instance-relative.
	Q_PROPERTY(QStringList untrustedModsFiles READ untrustedModsFiles CONSTANT)
	/* kind == "untrustedMods": milliseconds QML should keep its accept action
	 * out of reach; nothing here enforces it. Zero for every other kind. */
	Q_PROPERTY(int confirmDelayMs READ confirmDelayMs CONSTANT)
	/// kind == "update": currentVersion, availableVersion, releaseNotes
	/// (Markdown as published; QML renders it).
	Q_PROPERTY(QVariantMap updateInfo READ updateInfo CONSTANT)
	/// kind == "profileSetup": "unset" | "pending" | "available" | "exists" |
	/// "notAllowed" | "error". Changes live, like blockedMods.
	Q_PROPERTY(QString profileNameStatus READ profileNameStatus NOTIFY
			   profileNameStatusChanged)
	/// kind == "profileSetup": detail for the status above; empty when there is
	/// nothing to show.
	Q_PROPERTY(QString profileNameError READ profileNameError NOTIFY
			   profileNameStatusChanged)
	/// kind == "profileSetup": true while the network call is in flight; QML
	/// disables its form.
	Q_PROPERTY(bool profileSubmitting READ profileSubmitting NOTIFY
			   profileSubmittingChanged)
	/// kind == "filePicker": "open" | "save".
	Q_PROPERTY(QString filePickerMode READ filePickerMode CONSTANT)
	/// kind == "filePicker": a Qt filter string, as the plugin gave it.
	Q_PROPERTY(QString filePickerFilter READ filePickerFilter CONSTANT)
	/// kind == "filePicker": the suggested filename for Save; empty otherwise.
	Q_PROPERTY(QString filePickerDefaultPath READ filePickerDefaultPath
			   CONSTANT)
	/// kind == "pickAccount": {name, kind ("Microsoft" or "Offline"), faceId
	/// (for image://accountface/<faceId>)} per account.
	Q_PROPERTY(QVariantList accountOptions READ accountOptions CONSTANT)
	/// kind == "modal": the plugin's "mmco-ui/1" document, as a map.
	Q_PROPERTY(QVariantMap modalDocument READ modalDocument CONSTANT)
	/// kind == "optionalMods": owned by the blocked caller while the request
	/// is pending.
	Q_PROPERTY(QObject* optionalMods READ optionalMods CONSTANT)

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
		PickAccount,
		Modal,
		OptionalMods,
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
	QVariantList accountOptions() const
	{
		return m_accountOptions;
	}
	QVariantMap modalDocument() const
	{
		return m_modalDocument;
	}
	QObject* optionalMods() const
	{
		return m_optionalMods;
	}

	/* Filled in by QmlUiHost before the request is published, never after.
	 * Not reachable from QML. */
	void setSeverity(UiHost::Severity severity);
	void setLabels(QString acceptLabel, QString rejectLabel);
	void setActions(QStringList actions);
	void setValue(QString value);
	void setBlockedMods(const QList<BlockedMod>& mods);
	void setUntrustedModsFiles(QStringList files);
	void setUpdateInfo(QString currentVersion, QString availableVersion,
						QString releaseNotes);
	void setFilePicker(QString mode, QString defaultPath, QString filter);
	void setAccountOptions(QVariantList options);
	/// The document and the starting value of every input node in it.
	void setModal(QVariantMap document, QVariantMap fields);
	void setOptionalMods(QObject* mods);
	/// Called from QmlUiHost's checkNameRequested/submitNameRequested handlers.
	void setProfileNameStatus(QString status, QString error);
	void setProfileSubmitting(bool submitting);
	/* Bumped by checkProfileName() each time it starts a network check, before
	 * it emits checkNameRequested(). The connection is direct, so
	 * QmlUiHost::setupProfile()'s slot reads it inside the same call stack and
	 * keeps it as that check's identity; a result that returns after a newer
	 * check has started is discarded by comparing against it again. */
	int checkSequence() const
	{
		return m_checkSequence;
	}

	/* Read by QmlUiHost once answered() has fired; meaningless before. */
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
	/// kind == "pickAccount": the "make it the default" choice.
	bool makeDefault() const
	{
		return m_makeDefault;
	}
	/// kind == "modal": the node id whose click answered the request.
	QString modalClicked() const
	{
		return m_modalClicked;
	}
	/// kind == "modal": every input node's value when it was answered.
	QVariantMap modalFields() const
	{
		return m_modalFields;
	}
	/// kind == "text": the text accept(QString) was called with.
	QString answeredText() const
	{
		return m_answeredText;
	}

	/// message: the acknowledgement. confirm / blockedMods / untrustedMods:
	/// the positive answer.
	Q_INVOKABLE void accept();
	/// kind == "text": commits @p text as the answer.
	Q_INVOKABLE void accept(const QString& text);
	/// The negative answer, or "back out". Not for "choose" and "update",
	/// which have their own invokables.
	Q_INVOKABLE void reject();
	/// kind == "choose": the index into `actions`, or negative for backed out.
	Q_INVOKABLE void choose(int index);
	/// kind == "pickAccount": the index into `accountOptions` (negative for
	/// backed out) and whether that account becomes the default.
	Q_INVOKABLE void chooseAccount(int index, bool makeDefault);
	/* kind == "modal": PluginNode.qml's sink. A "change" records @p value as
	 * node @p nodeId's field; a "click" answers the request with that node.
	 * Other events are ignored; @p surfaceId is unused. */
	Q_INVOKABLE void sendEvent(const QString& surfaceId, const QString& nodeId,
							   const QString& event, const QVariant& value);
	/// kind == "update": "install", "later" or "skip"; anything else is "later".
	Q_INVOKABLE void answerUpdate(const QString& choice);
	/// kind == "blockedMods": opens mod @p index's download page in the system
	/// browser. No-op for an out-of-range index.
	Q_INVOKABLE void openDownload(int index);
	/// kind == "blockedMods": re-checks the Downloads folder now.
	Q_INVOKABLE void rescanDownloads();
	/// kind == "profileSetup": validates @p name locally (3-16 letters, digits
	/// or underscores) before asking QmlUiHost to check availability over the
	/// network; updates profileNameStatus either way.
	Q_INVOKABLE void checkProfileName(const QString& name);
	/// kind == "profileSetup": creates the profile. No-op unless
	/// profileNameStatus is "available". Accepts the request on success; on
	/// failure sets the status to "error" and leaves it open for another name.
	Q_INVOKABLE void submitProfileName(const QString& name);

  signals:
	/// One of the answer invokables ran; QmlUiHost::runRequest() waits on this.
	void answered();
	void blockedModsChanged();
	/// rescanDownloads() was called.
	void rescanRequested();
	void profileNameStatusChanged();
	void profileSubmittingChanged();
	/// checkProfileName() passed local validation.
	void checkNameRequested(const QString& name);
	/// submitProfileName() was called while available.
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
	QVariantList m_accountOptions;
	QVariantMap m_modalDocument;
	QObject* m_optionalMods = nullptr;
	QVariantMap m_modalFields;
	QString m_modalClicked;
	int m_checkSequence = 0;

	bool m_answered = false;
	bool m_accepted = false;
	int m_chosenIndex = -1;
	bool m_makeDefault = false;
	UiHost::UpdateChoice m_updateChoice = UiHost::UpdateChoice::Later;
};

/*
 * QML implementation of UiHost: a QEventLoop stands in for QDialog::exec().
 * Each override builds a QmlUiRequest, publishes it as `current` and blocks in
 * runRequest() until QML answers it or it is cancelled.
 *
 * Re-entrancy: a call made while another is pending stacks. `current` is
 * always the innermost request; answering it pops it and returns to the call
 * it interrupted.
 *
 * Busy nests the same way: `busy` is true, with `busyText` and
 * `busySkipLabel` of the most recently started one, while any BusyIndicator
 * handed out is alive.
 *
 * Quit/destruction: aboutToQuit and the destructor reject() every request
 * still on the stack, so a blocked call unwinds with a "no" answer.
 * runRequest() touches `this` afterwards only through a QPointer.
 *
 * Presenter readiness: this object exists from QmlShell::load(), before any
 * QML item is bound to `current`, and a call made then would wait forever.
 * QmlShell::uiHostInterface() therefore returns it only once `presenterReady`
 * is true, and Application uses the widget host until then. The item that shows
 * `current` calls setPresenterReady(true) when it is live.
 */
class QmlUiHost : public QObject, public UiHost
{
	Q_OBJECT

	/// The innermost pending request, or null. CppOwnership.
	Q_PROPERTY(QObject* current READ current NOTIFY currentChanged)
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
	/// Empty when busy is false.
	Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
	/// Empty when that showBusy() call offers no skip.
	Q_PROPERTY(QString busySkipLabel READ busySkipLabel NOTIFY busyChanged)
	/// Whether a QML item can answer a request; false until it calls
	/// setPresenterReady(true).
	Q_PROPERTY(bool presenterReady READ presenterReady WRITE setPresenterReady
			   NOTIFY presenterReadyChanged)

  public:
	explicit QmlUiHost(QObject* parent = nullptr);
	~QmlUiHost() override;

	std::unique_ptr<BusyIndicator> showBusy(
		const QString& text, const QString& skipLabel = QString(),
		std::function<void()> onSkip = {}) override;

	void message(const QString& title, const QString& text,
				 Severity severity) override;

	bool confirm(const QString& title, const QString& text,
				 Severity severity, const QString& acceptLabel = QString(),
				 const QString& rejectLabel = QString()) override;

	int choose(const QString& title, const QString& text, Severity severity,
			   const QStringList& actions) override;

	AccountChoice pickAccount(
		const QString& title, const QString& text,
		const QList<MinecraftAccountPtr>& accounts) override;

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

	std::optional<PluginModalResult> runPluginModal(
		const QString& title, const QJsonObject& document) override;

	bool chooseOptionalMods(const QString& packName,
							AtlOptionalModsModel& mods) override;

	QObject* current() const;
	bool busy() const;
	QString busyText() const;
	QString busySkipLabel() const;
	/// Runs the onSkip of the most recent showBusy(); no-op when it has none.
	Q_INVOKABLE void skipBusy();
	bool presenterReady() const;
	/// Called by the QML item that shows `current`.
	Q_INVOKABLE void setPresenterReady(bool ready);

	/* Called from the destructor of the BusyIndicator showBusy() hands out. */
	void endBusy(int id);

  signals:
	void currentChanged();
	void busyChanged();
	void presenterReadyChanged();

  private:
	/* Publishes @p request as `current`, blocks until it is answered or
	 * cancelled, then un-publishes it. */
	void runRequest(QmlUiRequest& request);

	QList<QmlUiRequest*> m_stack;
	bool m_presenterReady = false;

	struct BusyEntry {
		int id;
		QString text;
		QString skipLabel;
		std::function<void()> onSkip;
	};
	QList<BusyEntry> m_busy;
	int m_nextBusyId = 0;
};
