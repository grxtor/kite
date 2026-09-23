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

#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QUrl>

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

	void test_accountSkinInfo_outOfRangeOrOffline_isInvalid()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Steve"));

		QVERIFY(!controller.accountSkinInfo(5).value("valid").toBool());
		// Row 0 exists, but is offline, not a Microsoft account.
		QVERIFY(!controller.accountSkinInfo(0).value("valid").toBool());
		// -1 without the demo route set is just another out-of-range row.
		qunsetenv("MESHMC_QML_ROUTE");
		QVERIFY(!controller.skinDemoRequested());
		QVERIFY(!controller.accountSkinInfo(-1).value("valid").toBool());
	}

	// qml-preview-tools' snapshot account file only ever has offline
	// accounts, so this is the only way the skin/cape editor can be
	// rendered for review -- see AccountsController::skinDemoRequested()'s
	// own comment.
	void test_accountSkinInfo_demoRoute_fillsRowMinusOne()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);

		qputenv("MESHMC_QML_ROUTE", "page=accounts;accounts-demo=msa");
		QVERIFY(controller.skinDemoRequested());

		const QVariantMap demo = controller.accountSkinInfo(-1);
		QVERIFY(demo.value("valid").toBool());
		QCOMPARE(demo.value("capes").toList().size(), 2);

		qunsetenv("MESHMC_QML_ROUTE");
	}

	void test_accountSkinInfo_msaAccount_readsProfile()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		auto msa = MinecraftAccount::createBlankMSA();
		msa->accountData()->minecraftProfile.skin.variant = "SLIM";
		msa->accountData()->minecraftProfile.currentCape = "cape-1";
		Cape cape;
		cape.id = "cape-1";
		cape.alias = "Cool Cape";
		cape.url = "https://example.test/cape.png";
		msa->accountData()->minecraftProfile.capes.append(cape);
		list->addAccount(msa);

		const QVariantMap info = controller.accountSkinInfo(0);
		QVERIFY(info.value("valid").toBool());
		QVERIFY(info.value("slim").toBool());
		QCOMPARE(info.value("currentCapeId").toString(),
				 QStringLiteral("cape-1"));

		const QVariantList capes = info.value("capes").toList();
		QCOMPARE(capes.size(), 1);
		const QVariantMap firstCape = capes.first().toMap();
		QCOMPARE(firstCape.value("id").toString(), QStringLiteral("cape-1"));
		QCOMPARE(firstCape.value("alias").toString(),
				 QStringLiteral("Cool Cape"));
		QCOMPARE(firstCape.value("url").toString(),
				 QStringLiteral("https://example.test/cape.png"));
	}

	void test_validateSkinFile_rejectsWrongSizedImage()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);

		QTemporaryFile file(QDir::tempPath() +
							QStringLiteral("/AccountsControllerTest_XXXXXX.png"));
		QVERIFY(file.open());
		const QString path = file.fileName();
		file.close();

		QImage badImage(16, 16, QImage::Format_ARGB32);
		badImage.fill(Qt::transparent);
		QVERIFY(badImage.save(path, "PNG"));

		QVERIFY(!controller.validateSkinFile(path).isEmpty());
	}

	void test_validateSkinFile_acceptsSkinSizedImage_andFileUrl()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);

		QTemporaryFile file(QDir::tempPath() +
							QStringLiteral("/AccountsControllerTest_XXXXXX.png"));
		QVERIFY(file.open());
		const QString path = file.fileName();
		file.close();

		QImage goodImage(64, 32, QImage::Format_ARGB32);
		goodImage.fill(Qt::transparent);
		QVERIFY(goodImage.save(path, "PNG"));

		QVERIFY(controller.validateSkinFile(path).isEmpty());
		// A QML FileDialog hands out a "file://" url rather than a bare path.
		QVERIFY(controller.validateSkinFile(QUrl::fromLocalFile(path).toString())
					.isEmpty());
	}

	// changeSkin()/resetSkin()/changeCape() all start a real network task
	// (SkinUpload/SkinDelete/CapeChange -> LAUNCHER->network()) once past
	// their guard clauses - not safe without a LauncherContext, and not
	// performed here per instructions not to make network calls in tests.
	// Only the row-out-of-range and non-Microsoft-account guards, which
	// return before starting anything, are exercised.
	void test_changeSkin_resetSkin_changeCape_outOfRangeOrNonMSA_areNoOp()
	{
		auto list = shared_qobject_ptr<AccountList>(new AccountList());
		AccountsController controller(list);
		list->addAccount(MinecraftAccount::createOffline("Steve"));

		QVERIFY(!controller.changeSkin(5, "/nonexistent.png", false));
		QVERIFY(!controller.changeSkin(0, "/nonexistent.png", false));
		QVERIFY(!controller.resetSkin(5));
		QVERIFY(!controller.resetSkin(0));
		QVERIFY(!controller.changeCape(5, "cape-1"));
		QVERIFY(!controller.changeCape(0, "cape-1"));
	}
};

QTEST_GUILESS_MAIN(AccountsControllerTest)

#include "AccountsController_test.moc"
