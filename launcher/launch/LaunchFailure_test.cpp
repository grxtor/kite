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

#include <QTest>

#include "core/UiHost.h"
#include "launch/LaunchFailure.h"

// What the player is told when a launch ends before the game ran, and what
// their answer turns into (LaunchFailure; Application::showLaunchFailure()
// acts on it).
namespace
{
	// Records the one question LaunchFailure asks and answers with `pick`.
	class RecordingHost final : public UiHost
	{
	  public:
		int pick = -1;
		int asked = 0;
		QString title;
		QString text;
		QStringList actions;

		int choose(const QString& askedTitle, const QString& askedText, Severity,
				   const QStringList& askedActions) override
		{
			++asked;
			title = askedTitle;
			text = askedText;
			actions = askedActions;
			return pick;
		}

		std::unique_ptr<BusyIndicator> showBusy(const QString&, const QString&,
												std::function<void()>) override
		{
			return std::make_unique<BusyIndicator>();
		}
		void message(const QString&, const QString&, Severity) override {}
		bool confirm(const QString&, const QString&, Severity, const QString&,
					 const QString&) override
		{
			return false;
		}
		AccountChoice pickAccount(const QString&, const QString&,
								  const QList<MinecraftAccountPtr>&) override
		{
			return {};
		}
		std::optional<QString> askText(const QString&, const QString&,
									   const QString&) override
		{
			return std::nullopt;
		}
		bool resolveBlockedMods(const QString&, const QString&,
								QList<BlockedMod>&) override
		{
			return false;
		}
		bool confirmUntrustedMods(const QStringList&) override
		{
			return false;
		}
		UpdateChoice offerUpdate(const UpdateOffer&) override
		{
			return UpdateChoice::Later;
		}
		bool setupProfile(MinecraftAccountPtr) override
		{
			return false;
		}
		std::optional<QString> pickFile(FilePickerMode, const QString&,
										const QString&, const QString&) override
		{
			return std::nullopt;
		}
		std::optional<PluginModalResult> runPluginModal(const QString&,
														const QJsonObject&) override
		{
			return std::nullopt;
		}
		bool chooseOptionalMods(const QString&, AtlOptionalModsModel&) override
		{
			return false;
		}
	};

	const QString kInstance = QStringLiteral("Skyblock");
	const QString kReason = QStringLiteral("Couldn't download the game files.");
} // namespace

class LaunchFailureTest : public QObject
{
	Q_OBJECT

  private slots:
	void quietFailureAsksNothing()
	{
		RecordingHost host;
		host.pick = 0;
		QCOMPARE(LaunchFailure::present(host, LaunchFailure::Kind::Quiet, kInstance, kReason),
				 LaunchFailure::Action::None);
		QCOMPARE(host.asked, 0);
	}

	// The failing step's reason reaches the player, under a title that names
	// the instance and says "start", not "crash".
	void failureBeforeStartReachesThePlayer()
	{
		RecordingHost host;
		LaunchFailure::present(host, LaunchFailure::Kind::BeforeStart, kInstance, kReason);
		QCOMPARE(host.asked, 1);
		QCOMPARE(host.title, QStringLiteral("Couldn't start Skyblock"));
		QVERIFY2(host.text.contains(kReason), qPrintable(host.text));
		QVERIFY(!host.title.contains(QStringLiteral("crash"), Qt::CaseInsensitive));
		QVERIFY(!host.text.contains(QStringLiteral("crash"), Qt::CaseInsensitive));
		QCOMPARE(host.actions, QStringList({QStringLiteral("Try again"), QStringLiteral("Show log")}));
	}

	void failureBeforeStartAnswers_data()
	{
		QTest::addColumn<int>("pick");
		QTest::addColumn<LaunchFailure::Action>("action");
		QTest::newRow("try again") << 0 << LaunchFailure::Action::Retry;
		QTest::newRow("show log") << 1 << LaunchFailure::Action::ShowLog;
		QTest::newRow("closed") << -1 << LaunchFailure::Action::None;
		QTest::newRow("out of range") << 7 << LaunchFailure::Action::None;
	}

	void failureBeforeStartAnswers()
	{
		QFETCH(int, pick);
		QFETCH(LaunchFailure::Action, action);
		RecordingHost host;
		host.pick = pick;
		QCOMPARE(LaunchFailure::present(host, LaunchFailure::Kind::BeforeStart, kInstance, kReason),
				 action);
	}

	// An account that cannot play: the one action is signing in.
	void accountProblemOffersSignIn()
	{
		RecordingHost host;
		host.pick = 0;
		const QString reason = QStringLiteral("Offline accounts need a Microsoft account.");
		QCOMPARE(LaunchFailure::present(host, LaunchFailure::Kind::Account, kInstance, reason),
				 LaunchFailure::Action::SignIn);
		QCOMPARE(host.title, QStringLiteral("Sign in to play"));
		QVERIFY(host.text.contains(kInstance));
		QVERIFY2(host.text.contains(reason), qPrintable(host.text));
		QCOMPARE(host.actions, QStringList({QStringLiteral("Sign in")}));
	}

	void failedRefreshOffersTryAgain()
	{
		RecordingHost host;
		host.pick = 0;
		QCOMPARE(LaunchFailure::present(host, LaunchFailure::Kind::Login, kInstance, QString()),
				 LaunchFailure::Action::Retry);
		QCOMPARE(host.title, QStringLiteral("Couldn't sign in"));
		QCOMPARE(host.actions, QStringList({QStringLiteral("Try again")}));
	}

	// No reason from the step: still words, and no dangling "Reason:".
	void emptyReasonStillSaysSomething()
	{
		const auto kinds = {LaunchFailure::Kind::Account, LaunchFailure::Kind::Login,
							LaunchFailure::Kind::BeforeStart};
		for (const auto kind : kinds) {
			const auto notice = LaunchFailure::notice(kind, kInstance, QStringLiteral("  \n"));
			QVERIFY(!notice.title.isEmpty());
			QVERIFY(!notice.text.trimmed().isEmpty());
			QVERIFY2(!notice.text.contains(QStringLiteral("Reason")), qPrintable(notice.text));
			QCOMPARE(notice.labels.size(), notice.actions.size());
			QVERIFY(!notice.labels.isEmpty());
		}
	}

	// LaunchController::login() refreshes an account that stays Errored this
	// often, then fails with Kind::Login instead of refreshing for ever.
	void accountRefreshIsCapped()
	{
		int refreshes = 0;
		while (LaunchFailure::mayRefreshAgain(refreshes)) {
			++refreshes;
			QVERIFY(refreshes <= 100);
		}
		QCOMPARE(refreshes, LaunchFailure::kMaxAccountRefreshes);
		QVERIFY(LaunchFailure::kMaxAccountRefreshes >= 1);
	}
};

QTEST_GUILESS_MAIN(LaunchFailureTest)

#include "LaunchFailure_test.moc"
