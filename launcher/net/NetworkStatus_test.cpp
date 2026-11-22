/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QSignalSpy>
#include <QTest>

#include "net/NetworkStatus.h"

using Reach = QNetworkInformation::Reachability;

/* NetworkStatus over a forced reachability: what counts as offline, and when
 * `online` follows it. The system's own backend is never asked to change. */
class NetworkStatusTest : public QObject
{
	Q_OBJECT

  private slots:
	void onlyDisconnectedIsOffline()
	{
		QVERIFY(!NetworkStatus::isOnline(Reach::Disconnected));
		QVERIFY(NetworkStatus::isOnline(Reach::Online));
		QVERIFY(NetworkStatus::isOnline(Reach::Site));
		QVERIFY(NetworkStatus::isOnline(Reach::Local));
		// No backend, or one that cannot tell: never "offline".
		QVERIFY(NetworkStatus::isOnline(Reach::Unknown));
	}

	void startsOnlineAndWaitsBeforeItSaysOffline()
	{
		NetworkStatus status(nullptr, 150, 0);
		QSignalSpy spy(&status, &NetworkStatus::onlineChanged);
		status.setReachability(Reach::Disconnected);
		// Inside the start-up delay nothing is reported yet.
		QVERIFY(status.online());
		QCOMPARE(spy.count(), 0);
		QTRY_VERIFY(!status.online());
		QCOMPARE(spy.count(), 1);
	}

	void unknownNeverGoesOffline()
	{
		NetworkStatus status(nullptr, 0, 0);
		QSignalSpy spy(&status, &NetworkStatus::onlineChanged);
		status.setReachability(Reach::Unknown);
		QTest::qWait(50);
		QVERIFY(status.online());
		QCOMPARE(spy.count(), 0);
	}

	void aShortDropIsNotReported()
	{
		NetworkStatus status(nullptr, 0, 200);
		status.setReachability(Reach::Online);
		QTest::qWait(20);
		QSignalSpy spy(&status, &NetworkStatus::onlineChanged);
		status.setReachability(Reach::Disconnected);
		status.setReachability(Reach::Online);
		QTest::qWait(300);
		QVERIFY(status.online());
		QCOMPARE(spy.count(), 0);
	}

	void comingBackIsReportedAtOnce()
	{
		NetworkStatus status(nullptr, 0, 0);
		status.setReachability(Reach::Disconnected);
		QTRY_VERIFY(!status.online());
		status.setReachability(Reach::Online);
		QVERIFY(status.online());
	}

	void recheckReadsWithoutTheDelay()
	{
		NetworkStatus status(nullptr, 60000, 60000);
		status.setReachability(Reach::Disconnected);
		QVERIFY(status.online());
		QVERIFY(!status.recheck());
		QVERIFY(!status.online());
		status.debugSetReachability(QStringLiteral("unknown"));
		QVERIFY(status.online());
	}
};

QTEST_GUILESS_MAIN(NetworkStatusTest)

#include "NetworkStatus_test.moc"
