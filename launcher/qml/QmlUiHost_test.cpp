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

#include <QTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimer>
#include <memory>

#include "qml/QmlUiHost.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "models/AtlOptionalModsModel.h"

/*
 * Drives QmlUiHost the way QML would, without any QML: confirm()/etc. block
 * in their own QEventLoop, so every test here schedules the "QML" side of
 * the conversation on a zero-delay QTimer before making the blocking call --
 * the timer fires once that call's internal loop starts pumping events, the
 * same trick used to test QDialog::exec().
 */
class QmlUiHostTest : public QObject
{
	Q_OBJECT

  private slots:
	void confirmReturnsTrueWhenQmlAccepts()
	{
		QmlUiHost host;
		QVERIFY(!host.current());

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("confirm"));
			QCOMPARE(request->title(), QStringLiteral("Title"));
			QCOMPARE(request->text(), QStringLiteral("Text"));
			QCOMPARE(request->severity(), QStringLiteral("question"));
			request->accept();
		});

		const bool accepted =
			host.confirm(QStringLiteral("Title"), QStringLiteral("Text"),
						 UiHost::Severity::Question);

		QVERIFY(accepted);
		QVERIFY(!host.current());
	}

	void confirmReturnsFalseWhenQmlRejects()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			request->reject();
		});

		const bool accepted =
			host.confirm(QStringLiteral("Title"), QStringLiteral("Text"),
						 UiHost::Severity::Warning);

		QVERIFY(!accepted);
	}

	void chooseReturnsTheChosenIndex()
	{
		QmlUiHost host;
		const QStringList actions{ QStringLiteral("Keep"),
								   QStringLiteral("Replace") };

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("choose"));
			QCOMPARE(request->actions(), QStringList({ QStringLiteral("Keep"),
													   QStringLiteral("Replace") }));
			request->choose(1);
		});

		const int chosen = host.choose(QStringLiteral("Title"),
									   QStringLiteral("Text"),
									   UiHost::Severity::Question, actions);

		QCOMPARE(chosen, 1);
	}

	void askTextReturnsTheEnteredValueWhenQmlAccepts()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("text"));
			QCOMPARE(request->value(), QStringLiteral("User"));
			request->accept(QStringLiteral("Steve"));
		});

		const auto answer =
			host.askText(QStringLiteral("Title"), QStringLiteral("Text"),
						 QStringLiteral("User"));

		QVERIFY(answer.has_value());
		QCOMPARE(*answer, QStringLiteral("Steve"));
	}

	void askTextReturnsNulloptWhenQmlRejects()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			request->reject();
		});

		const auto answer =
			host.askText(QStringLiteral("Title"), QStringLiteral("Text"));

		QVERIFY(!answer.has_value());
	}

	void confirmIsCancelledWhenTheApplicationQuits()
	{
		QmlUiHost host;
		bool accepted = true;

		/* qApp->quit() only unwinds a QEventLoop while a top-level
		 * QCoreApplication::exec() is actually running somewhere on the
		 * stack -- true of the real launcher (see main.cpp) but not of a
		 * QTest slot, which never calls exec() itself. This test supplies
		 * that outer loop so it exercises the same shape runRequest()
		 * relies on in production, rather than the call it makes to
		 * qApp->quit() silently doing nothing. */
		QTimer::singleShot(0, [&]() {
			QTimer::singleShot(0, [&host]() {
				QVERIFY(host.current());
				qApp->quit();
			});
			accepted = host.confirm(QStringLiteral("Title"),
									QStringLiteral("Text"),
									UiHost::Severity::Question);
		});
		qApp->exec();

		QVERIFY(!accepted);
		QVERIFY(!host.current());
	}

	void confirmIsCancelledWhenTheHostIsDestroyedWhilePending()
	{
		auto host = std::make_unique<QmlUiHost>();

		QTimer::singleShot(0, [&host]() {
			QVERIFY(host->current());
			host.reset(); // destroys the host while confirm() below is blocked
		});

		const bool accepted = host->confirm(QStringLiteral("Title"),
											QStringLiteral("Text"),
											UiHost::Severity::Question);

		QVERIFY(!accepted);
		QVERIFY(!host);
	}

	void presenterReadyDefaultsToFalseAndNotifies()
	{
		QmlUiHost host;
		QVERIFY(!host.presenterReady());

		QSignalSpy spy(&host, &QmlUiHost::presenterReadyChanged);
		host.setPresenterReady(true);
		QVERIFY(host.presenterReady());
		QCOMPARE(spy.count(), 1);

		// No-op: same value, no redundant notification.
		host.setPresenterReady(true);
		QCOMPARE(spy.count(), 1);
	}

	void checkProfileNameRejectsAnInvalidShapeWithoutAskingTheNetwork()
	{
		QmlUiRequest request(QmlUiRequest::Kind::ProfileSetup,
							 QStringLiteral("Title"), QStringLiteral("Text"));
		QSignalSpy checkSpy(&request, &QmlUiRequest::checkNameRequested);

		request.checkProfileName(QStringLiteral("ab")); // too short

		QCOMPARE(request.profileNameStatus(), QStringLiteral("unset"));
		QVERIFY(!request.profileNameError().isEmpty());
		QCOMPARE(checkSpy.count(), 0);
	}

	void checkProfileNameAsksTheNetworkForAValidShape()
	{
		QmlUiRequest request(QmlUiRequest::Kind::ProfileSetup,
							 QStringLiteral("Title"), QStringLiteral("Text"));
		QSignalSpy checkSpy(&request, &QmlUiRequest::checkNameRequested);

		request.checkProfileName(QStringLiteral("Steve"));

		QCOMPARE(request.profileNameStatus(), QStringLiteral("pending"));
		QCOMPARE(checkSpy.count(), 1);
		QCOMPARE(checkSpy.at(0).at(0).toString(), QStringLiteral("Steve"));
	}

	void submitProfileNameDoesNothingUntilTheNameIsAvailable()
	{
		QmlUiRequest request(QmlUiRequest::Kind::ProfileSetup,
							 QStringLiteral("Title"), QStringLiteral("Text"));
		QSignalSpy submitSpy(&request, &QmlUiRequest::submitNameRequested);

		request.submitProfileName(QStringLiteral("Steve")); // still "unset"

		QCOMPARE(submitSpy.count(), 0);
		QVERIFY(!request.profileSubmitting());
	}

	void submitProfileNameAsksTheNetworkOnceAvailable()
	{
		QmlUiRequest request(QmlUiRequest::Kind::ProfileSetup,
							 QStringLiteral("Title"), QStringLiteral("Text"));
		request.setProfileNameStatus(QStringLiteral("available"), QString());
		QSignalSpy submitSpy(&request, &QmlUiRequest::submitNameRequested);

		request.submitProfileName(QStringLiteral("Steve"));

		QCOMPARE(submitSpy.count(), 1);
		QVERIFY(request.profileSubmitting());

		// Re-entrant clicks while a submission is already in flight are
		// ignored rather than starting a second one.
		request.submitProfileName(QStringLiteral("Steve"));
		QCOMPARE(submitSpy.count(), 1);
	}

	void setupProfileReturnsFalseWhenQmlRejects()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("profileSetup"));
			request->reject();
		});

		const bool created = host.setupProfile(nullptr);

		QVERIFY(!created);
		QVERIFY(!host.current());
	}

	void pickFileReturnsNulloptWhenQmlRejects()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("filePicker"));
			request->reject();
		});

		const auto result =
			host.pickFile(UiHost::FilePickerMode::Open, QStringLiteral("Title"),
						  QString(), QStringLiteral("*.txt"));

		QVERIFY(!result.has_value());
	}

	void pickFileUnwrapsAFileUrlToALocalPath()
	{
		QmlUiHost host;

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->filePickerMode(), QStringLiteral("save"));
			request->accept(QStringLiteral("file:///tmp/example.txt"));
		});

		const auto result =
			host.pickFile(UiHost::FilePickerMode::Save, QStringLiteral("Title"),
						  QStringLiteral("example.txt"), QString());

		QVERIFY(result.has_value());
		QCOMPARE(*result, QStringLiteral("/tmp/example.txt"));
	}

	void busyNestsAndReportsTheInnermostText()
	{
		QmlUiHost host;
		QVERIFY(!host.busy());
		QVERIFY(host.busyText().isEmpty());

		auto outer = host.showBusy(QStringLiteral("Loading"));
		QVERIFY(host.busy());
		QCOMPARE(host.busyText(), QStringLiteral("Loading"));

		{
			auto inner = host.showBusy(QStringLiteral("Downloading"));
			QVERIFY(host.busy());
			QCOMPARE(host.busyText(), QStringLiteral("Downloading"));
		}
		// `inner` destroyed above; the outer indicator is still alive.
		QVERIFY(host.busy());
		QCOMPARE(host.busyText(), QStringLiteral("Loading"));

		outer.reset();
		QVERIFY(!host.busy());
		QVERIFY(host.busyText().isEmpty());
	}

	/* "Play offline" while an account refreshes: the skip label is the
	 * innermost wait's own, skipBusy() runs its callback, and a wait
	 * without one offers nothing to skip. */
	void busySkipRunsTheCallbackOfTheInnermostWait()
	{
		QmlUiHost host;
		int skipped = 0;

		auto plain = host.showBusy(QStringLiteral("Loading"));
		QVERIFY(host.busySkipLabel().isEmpty());
		host.skipBusy(); // nothing to skip: no-op
		QCOMPARE(skipped, 0);

		std::unique_ptr<UiHost::BusyIndicator> refresh;
		refresh = host.showBusy(QStringLiteral("Refreshing account"),
								QStringLiteral("Play offline"),
								[&]() {
									++skipped;
									// What a real caller does: stop waiting.
									refresh.reset();
								});
		QCOMPARE(host.busySkipLabel(), QStringLiteral("Play offline"));

		host.skipBusy();
		QCOMPARE(skipped, 1);
		QVERIFY(!refresh);
		// Back to the outer wait, which offers no skip.
		QCOMPARE(host.busyText(), QStringLiteral("Loading"));
		QVERIFY(host.busySkipLabel().isEmpty());
	}

	/* The launch account picker: the accounts as QML sees them, and the
	 * chosen index together with "make it the default". */
	void pickAccountReturnsTheChoiceAndTheDefaultWish()
	{
		QmlUiHost host;
		const QList<MinecraftAccountPtr> accounts{
			MinecraftAccount::createOffline(QStringLiteral("Steve")),
			MinecraftAccount::createOffline(QStringLiteral("Alex")),
		};

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("pickAccount"));
			const auto options = request->accountOptions();
			QCOMPARE(options.size(), 2);
			QCOMPARE(options.at(1).toMap().value(QStringLiteral("name")).toString(),
					 QStringLiteral("Alex"));
			QCOMPARE(options.at(1).toMap().value(QStringLiteral("kind")).toString(),
					 QStringLiteral("Offline"));
			QVERIFY(!options.at(1).toMap().value(QStringLiteral("faceId")).toString().isEmpty());
			request->chooseAccount(1, true);
		});

		const auto choice = host.pickAccount(QStringLiteral("Title"),
											 QStringLiteral("Text"), accounts);
		QCOMPARE(choice.index, 1);
		QVERIFY(choice.makeDefault);
	}

	/* A plugin's one-off prompt: the clicked node's id comes back with
	 * every input's value -- its starting value unless QML changed it. */
	void pluginModalReturnsTheClickAndTheFields()
	{
		QmlUiHost host;
		const QJsonObject document{
			{ "type", "column" },
			{ "children",
			  QJsonArray{
				  QJsonObject{ { "type", "text_field" }, { "id", "name" },
							   { "props", QJsonObject{ { "value", "Steve" } } } },
				  QJsonObject{ { "type", "toggle" }, { "id", "agree" } },
				  QJsonObject{ { "type", "number_field" }, { "id", "count" },
							   { "props", QJsonObject{ { "value", 3 } } } },
				  QJsonObject{ { "type", "button" }, { "id", "ok" } },
			  } },
		};

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("modal"));
			QCOMPARE(request->modalDocument().value("type").toString(),
					 QStringLiteral("column"));
			request->sendEvent(QStringLiteral("modal"), QStringLiteral("name"),
							   QStringLiteral("change"), QStringLiteral("Alex"));
			request->sendEvent(QStringLiteral("modal"), QStringLiteral("ok"),
							   QStringLiteral("click"), QVariant());
		});

		const auto result = host.runPluginModal(QStringLiteral("Title"), document);
		QVERIFY(result.has_value());
		QCOMPARE(result->clicked, QStringLiteral("ok"));
		QCOMPARE(result->fields.value("name").toString(), QStringLiteral("Alex"));
		QCOMPARE(result->fields.value("agree").toBool(true), false);
		QCOMPARE(result->fields.value("count").toDouble(), 3.0);
	}

	void pluginModalBackingOutAnswersNothing()
	{
		QmlUiHost host;
		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			request->reject();
		});
		const auto result = host.runPluginModal(
			QStringLiteral("Title"),
			QJsonObject{ { "type", "button" }, { "id", "ok" } });
		QVERIFY(!result.has_value());
	}

	/* An ATLauncher pack's optional mods: QML ticks them on the request's
	 * own model, and Install (accept) or the pack's defaults (reject) is
	 * the answer. */
	void chooseOptionalModsTicksInPlace()
	{
		auto mod = [](const QString& name, bool selected) {
			ATLauncher::VersionMod m;
			m.name = name;
			m.optional = true;
			m.selected = selected;
			m.recommended = selected;
			m.hidden = false;
			m.library = false;
			m.effectively_hidden = false;
			m.client = true;
			return m;
		};
		QmlUiHost host;
		AtlOptionalModsModel mods({ mod(QStringLiteral("Shaders"), true),
									mod(QStringLiteral("Minimap"), false) });

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			QCOMPARE(request->kind(), QStringLiteral("optionalMods"));
			auto* model = qobject_cast<AtlOptionalModsModel*>(request->optionalMods());
			QVERIFY(model);
			model->toggle(1);
			request->accept();
		});
		QVERIFY(host.chooseOptionalMods(QStringLiteral("Pack"), mods));
		QCOMPARE(mods.selectedNames(),
				 QVector<QString>({ QStringLiteral("Shaders"), QStringLiteral("Minimap") }));

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			request->reject();
		});
		QVERIFY(!host.chooseOptionalMods(QStringLiteral("Pack"), mods));
	}

	void pickAccountBackingOutChoosesNothing()
	{
		QmlUiHost host;
		const QList<MinecraftAccountPtr> accounts{
			MinecraftAccount::createOffline(QStringLiteral("Steve")),
		};

		QTimer::singleShot(0, [&host]() {
			auto* request = qobject_cast<QmlUiRequest*>(host.current());
			QVERIFY(request);
			request->reject();
		});

		const auto choice = host.pickAccount(QStringLiteral("Title"),
											 QStringLiteral("Text"), accounts);
		QCOMPARE(choice.index, -1);
		QVERIFY(!choice.makeDefault);
	}
};

QTEST_GUILESS_MAIN(QmlUiHostTest)

#include "QmlUiHost_test.moc"
