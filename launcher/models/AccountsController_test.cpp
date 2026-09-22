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

#include <QSignalSpy>
#include <QTest>

#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "models/AccountsController.h"

/*
 * Covers what does not need a LauncherContext or a network round trip:
 * AccountList's QML roles (including the ones this page added) and the
 * parts of AccountsController that only touch AccountList/MinecraftAccount.
 *
 * MinecraftAccount::createBlankMSA() marks an account as MSA-typed without
 * performing any login - exactly what's needed to satisfy addOffline()'s
 * "at least one Microsoft account exists" rule in a test. Actually signing
 * in (AccountsController::loginMicrosoft()) goes through MSAStep, which
 * reaches LAUNCHER->network() - that needs a real LauncherContext and is
 * deliberately not exercised here, per instructions not to perform network
 * auth calls in tests.
 */
class AccountsControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_roleNames_includeQmlRoles()
	{
		AccountList list;
		auto roles = list.roleNames();
		QCOMPARE(roles.value(AccountList::NameRole), QByteArray("name"));
		QCOMPARE(roles.value(AccountList::ProfileNameRole),
				 QByteArray("profileName"));
		QCOMPARE(roles.value(AccountList::TypeRole), QByteArray("type"));
		QCOMPARE(roles.value(AccountList::StatusRole), QByteArray("status"));
		QCOMPARE(roles.value(AccountList::IsDefaultRole),
				 QByteArray("isDefault"));
		QCOMPARE(roles.value(AccountList::IsMSARole), QByteArray("isMSA"));
		QCOMPARE(roles.value(AccountList::StateKeyRole),
				 QByteArray("stateKey"));
		QCOMPARE(roles.value(AccountList::AccountIdRole),
				 QByteArray("accountId"));
	}

	void test_data_isMSA_and_accountId_distinguishAccountTypes()
	{
		AccountList list;
		auto offline = MinecraftAccount::createOffline("Steve");
		auto msa = MinecraftAccount::createBlankMSA();
		list.addAccount(offline);
		list.addAccount(msa);

		const QModelIndex offlineIdx = list.index(0);
		const QModelIndex msaIdx = list.index(1);

		QVERIFY(!list.data(offlineIdx, AccountList::IsMSARole).toBool());
		QVERIFY(list.data(msaIdx, AccountList::IsMSARole).toBool());

		// Neither has a Mojang profile id yet, so both fall back to
		// internalId() - and it must actually distinguish them.
		const QString offlineId =
			list.data(offlineIdx, AccountList::AccountIdRole).toString();
		const QString msaId =
			list.data(msaIdx, AccountList::AccountIdRole).toString();
		QCOMPARE(offlineId, offline->internalId());
		QCOMPARE(msaId, msa->internalId());
		QVERIFY(offlineId != msaId);
	}

	void test_data_isDefault_and_stateKey_trackState()
	{
		AccountList list;
		auto offline = MinecraftAccount::createOffline("Steve");
		list.addAccount(offline);
		const QModelIndex idx = list.index(0);

		QVERIFY(!list.data(idx, AccountList::IsDefaultRole).toBool());
		// createOffline() leaves new offline accounts already Online.
		QCOMPARE(list.data(idx, AccountList::StateKeyRole).toString(),
				 QStringLiteral("online"));

		list.setDefaultAccount(offline);
		QVERIFY(list.data(idx, AccountList::IsDefaultRole).toBool());
	}

	void test_hasDefault_and_defaultName_followTheList()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		QSignalSpy spy(&controller, &AccountsController::defaultChanged);

		QVERIFY(!controller.hasDefault());
		QVERIFY(controller.defaultName().isEmpty());

		auto account = MinecraftAccount::createOffline("Alex");
		list->addAccount(account);
		list->setDefaultAccount(account);

		QVERIFY(controller.hasDefault());
		QCOMPARE(controller.defaultName(), QStringLiteral("Alex"));
		QVERIFY(spy.count() > 0);
	}

	void test_setDefault_and_clearDefault_byRow()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);

		list->addAccount(MinecraftAccount::createOffline("Alex"));
		list->addAccount(MinecraftAccount::createOffline("Bob"));

		controller.setDefault(1);
		QCOMPARE(controller.defaultName(), QStringLiteral("Bob"));

		controller.clearDefault();
		QVERIFY(!controller.hasDefault());
	}

	void test_setDefault_outOfRange_isNoOp()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Alex"));

		controller.setDefault(5);
		controller.setDefault(-1);

		QVERIFY(!controller.hasDefault());
	}

	void test_remove_dropsTheRow()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Alex"));
		list->addAccount(MinecraftAccount::createOffline("Bob"));
		QCOMPARE(list->count(), 2);

		controller.remove(0);

		QCOMPARE(list->count(), 1);
		QCOMPARE(list->at(0)->profileName(), QStringLiteral("Bob"));
	}

	void test_remove_outOfRange_isNoOp()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Alex"));

		controller.remove(5);
		controller.remove(-1);

		QCOMPARE(list->count(), 1);
	}

	// refresh() on a valid row would start a real MSASilent/QOAuth2 network
	// task via AccountList::requestRefresh() -> tryNext() -> LAUNCHER-> - not
	// safe without a LauncherContext. Only the bounds check is exercised.
	void test_refresh_outOfRange_isNoOp()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Alex"));

		controller.refresh(5);
		controller.refresh(-1);

		QCOMPARE(list->count(), 1);
	}

	void test_addOffline_requiresAnMSAAccountFirst()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);

		QVERIFY(!controller.addOffline("Steve"));
		QCOMPARE(list->count(), 0);

		list->addAccount(MinecraftAccount::createBlankMSA());
		QVERIFY(controller.addOffline("Steve"));
		QCOMPARE(list->count(), 2);
	}

	void test_addOffline_rejectsEmptyOrWhitespaceUsername()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createBlankMSA());

		QVERIFY(!controller.addOffline(""));
		QVERIFY(!controller.addOffline("   "));
		QCOMPARE(list->count(), 1);
	}

	void test_addOffline_trimsUsername()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createBlankMSA());

		QVERIFY(controller.addOffline("  Steve  "));
		QCOMPARE(list->at(1)->profileName(), QStringLiteral("Steve"));
	}

	void test_addOffline_rejectsCaseInsensitiveDuplicate()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createBlankMSA());
		QVERIFY(controller.addOffline("Steve"));

		QVERIFY(!controller.addOffline("steve"));
		QCOMPARE(list->count(), 2);
	}
};

QTEST_GUILESS_MAIN(AccountsControllerTest)

#include "AccountsController_test.moc"
