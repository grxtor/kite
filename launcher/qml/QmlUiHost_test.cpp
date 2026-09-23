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

#include <QSignalSpy>
#include <QTimer>
#include <memory>

#include "qml/QmlUiHost.h"

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
};

QTEST_GUILESS_MAIN(QmlUiHostTest)

#include "QmlUiHost_test.moc"
