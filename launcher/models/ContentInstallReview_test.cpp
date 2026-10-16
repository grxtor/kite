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
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/mod/ModFolderModel.h"
#include "models/ContentInstallReview.h"

/*
 * The review before an install, fed through applyResolution() - the seam
 * the dependency resolver's answer goes through - so no provider is ever
 * asked anything. install() itself downloads, and is only checked for
 * refusing to run before the review is ready.
 */
class ContentInstallReviewTest : public QObject
{
	Q_OBJECT

  private:
	ModPlatform::SelectedMod picked() const
	{
		ModPlatform::SelectedMod mod;
		mod.name = "Sodium Extra";
		mod.projectId = "PtjYWJkn";
		mod.versionId = "v1";
		mod.fileName = "sodium-extra.jar";
		mod.platform = "modrinth";
		mod.versionType = "release";
		return mod;
	}

	ModPlatform::DependencyInfo dependency(const QString& name,
										   bool maybeInstalled) const
	{
		ModPlatform::DependencyInfo dep;
		dep.name = name;
		dep.projectId = name.toLower();
		dep.fileName = name.toLower() + ".jar";
		dep.platform = "curseforge";
		dep.requiredBy = {"Sodium Extra"};
		dep.maybeInstalled = maybeInstalled;
		return dep;
	}

	std::shared_ptr<ModFolderModel> folder()
	{
		return std::make_shared<ModFolderModel>(m_dir.path());
	}

	QTemporaryDir m_dir;

  private slots:
	void withoutDependenciesIsReadyAtOnce()
	{
		ContentInstallReview review(picked(), "1.21.4", QString(), folder(),
									/*resolveDependencies=*/false);
		QSignalSpy state(&review, &ContentInstallReview::stateChanged);
		QVERIFY(!review.ready());

		review.start();
		QVERIFY(review.ready());
		QVERIFY(!review.resolving());
		QVERIFY(!review.needsReview());
		QVERIFY(state.count() > 0);
		QCOMPARE(review.count(), 1);
		QCOMPARE(review.title(), QStringLiteral("Sodium Extra"));

		const auto items = review.downloadItems();
		QCOMPARE(items.size(), 1);
		QCOMPARE(items.first().fileName, QStringLiteral("sodium-extra.jar"));
		QVERIFY(!items.first().isDependency);
	}

	void listsDependenciesWithInstalledOnesUnticked()
	{
		ContentInstallReview review(picked(), "1.21.4", "fabric", folder(),
									true);
		review.applyResolution(
			{dependency("Sodium", true), dependency("Cloth Config", false)},
			{{"Missing Lib", "missing", "modrinth"}});

		QVERIFY(review.ready());
		QVERIFY(review.needsReview());
		QCOMPARE(review.count(), 3);
		QCOMPARE(review.dependencyCount(), 2);
		QCOMPARE(review.checkedCount(), 2);
		QCOMPARE(review.unresolved(), QStringList{"Missing Lib"});

		const QModelIndex sodium = review.index(1);
		QCOMPARE(sodium.data(ContentInstallReview::NameRole).toString(),
				 QStringLiteral("Sodium"));
		QVERIFY(sodium.data(ContentInstallReview::MaybeInstalledRole).toBool());
		QVERIFY(!sodium.data(ContentInstallReview::CheckedRole).toBool());
		QCOMPARE(sodium.data(ContentInstallReview::ProviderRole).toString(),
				 QStringLiteral("CurseForge"));
		QCOMPARE(sodium.data(ContentInstallReview::RequiredByRole).toString(),
				 QStringLiteral("Sodium Extra"));
		QCOMPARE(review.index(0).data(ContentInstallReview::ProviderRole).toString(),
				 QStringLiteral("Modrinth"));

		QStringList downloaded;
		for (const auto& item : review.downloadItems()) {
			downloaded << item.name;
		}
		QCOMPARE(downloaded, (QStringList{"Sodium Extra", "Cloth Config"}));
	}

	void tickingAndUnticking()
	{
		ContentInstallReview review(picked(), "1.21.4", "fabric", folder(),
									true);
		review.applyResolution(
			{dependency("Sodium", true), dependency("Cloth Config", false)},
			{});
		QSignalSpy checked(&review, &ContentInstallReview::checkedChanged);

		/* What was picked cannot be unticked. */
		review.setChecked(0, false);
		QCOMPARE(review.checkedCount(), 2);
		QCOMPARE(checked.count(), 0);

		review.setChecked(2, false);
		QCOMPARE(review.checkedCount(), 1);
		QCOMPARE(review.downloadItems().size(), 1);
		QCOMPARE(checked.count(), 1);

		review.setAllDependenciesChecked(true);
		QCOMPARE(review.checkedCount(), 3);

		/* Out of range is ignored. */
		review.setChecked(7, false);
		QCOMPARE(review.checkedCount(), 3);
	}

	void installRefusesBeforeReady()
	{
		ContentInstallReview review(picked(), "1.21.4", "fabric", folder(),
									true);
		QCOMPARE(review.install(), nullptr);
		QVERIFY(!review.ready());
	}

	void checkedDependencyCountLeavesThePickOut()
	{
		ContentInstallReview review(picked(), "1.21.4", "fabric", folder(),
									true);
		review.applyResolution(
			{dependency("Sodium", true), dependency("Cloth Config", false)},
			{});
		QCOMPARE(review.primaryCount(), 1);
		QCOMPARE(review.checkedDependencyCount(), 1);
		review.setAllDependenciesChecked(false);
		QCOMPARE(review.checkedDependencyCount(), 0);
		QCOMPARE(review.checkedCount(), 1);
	}

	void missingDependenciesHaveNoPick()
	{
		ModPlatform::SelectedMod installed = picked();
		std::unique_ptr<ContentInstallReview> review(
			ContentInstallReview::missingDependencies(
				{installed}, "Missing dependencies", "1.21.4", "fabric",
				folder()));
		QCOMPARE(review->primaryCount(), 0);
		QCOMPARE(review->title(), QStringLiteral("Missing dependencies"));
		QCOMPARE(review->count(), 0);

		review->applyResolution({dependency("Sodium", false)}, {});
		QCOMPARE(review->count(), 1);
		QCOMPARE(review->dependencyCount(), 1);
		QVERIFY(review->needsReview());

		/* Every row is optional here, the first one included. */
		review->setChecked(0, false);
		QCOMPARE(review->checkedCount(), 0);
		QVERIFY(review->downloadItems().isEmpty());
		/* Nothing ticked: nothing to install. */
		QCOMPARE(review->install(), nullptr);
	}

	void providerNames()
	{
		QCOMPARE(ContentInstallReview::providerName("modrinth"),
				 QStringLiteral("Modrinth"));
		QCOMPARE(ContentInstallReview::providerName("curseforge"),
				 QStringLiteral("CurseForge"));
		QCOMPARE(ContentInstallReview::providerName("other"),
				 QStringLiteral("other"));
	}
};

QTEST_GUILESS_MAIN(ContentInstallReviewTest)

#include "ContentInstallReview_test.moc"
