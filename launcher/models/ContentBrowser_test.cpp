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

#include "models/ContentBrowser.h"

#include "modplatform/ContentApi.h"
#include "modplatform/ContentProviderModel.h"

/* Both functions under test are public and static, and touch neither the
 * network nor a MinecraftInstance - see the class comment on
 * ContentBrowser::isVersionCompatible()/sortOptionsFor(). Constructing a
 * full ContentBrowser (which needs a MinecraftInstance) is impractical
 * here, the same reason models/InstanceDetails_test.cpp only exercises
 * InstanceLogBridge. */
class ContentBrowserTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_CompatibleWhenEverythingMatches()
	{
		ModPlatform::ContentVersion version;
		version.gameVersions = {"1.20.1", "1.20.4"};
		version.loaders = {"fabric", "quilt"};

		QVERIFY(ContentBrowser::isVersionCompatible(version, "1.20.1",
													 "fabric"));
	}

	void test_IncompatibleGameVersion()
	{
		ModPlatform::ContentVersion version;
		version.gameVersions = {"1.19.2"};
		version.loaders = {"fabric"};

		QVERIFY(!ContentBrowser::isVersionCompatible(version, "1.20.1",
													 "fabric"));
	}

	void test_IncompatibleLoader()
	{
		ModPlatform::ContentVersion version;
		version.gameVersions = {"1.20.1"};
		version.loaders = {"forge"};

		QVERIFY(!ContentBrowser::isVersionCompatible(version, "1.20.1",
													 "fabric"));
	}

	void test_LoaderMatchIsCaseInsensitive()
	{
		ModPlatform::ContentVersion version;
		version.gameVersions = {"1.20.1"};
		version.loaders = {"Fabric"};

		QVERIFY(ContentBrowser::isVersionCompatible(version, "1.20.1",
													 "fabric"));
	}

	/* A version that states neither is accepted for whichever half it is
	 * silent about, rather than flagged incompatible - see the field
	 * comments on ModPlatform::ContentVersion. */
	void test_UnstatedFieldsAreAccepted()
	{
		ModPlatform::ContentVersion version;
		QVERIFY(ContentBrowser::isVersionCompatible(version, "1.20.1",
													 "fabric"));

		ModPlatform::ContentVersion partial;
		partial.gameVersions = {"1.20.1"};
		/* No loaders stated at all - a library mod, say. */
		QVERIFY(ContentBrowser::isVersionCompatible(partial, "1.20.1",
													 "fabric"));
	}

	void test_EmptyCallerArgumentsAreAccepted()
	{
		/* An instance with no detected loader (detectInstanceProfile()
		 * came up empty) searches without a loader filter - the same
		 * version should not then be flagged incompatible for a loader
		 * nobody named. */
		ModPlatform::ContentVersion version;
		version.gameVersions = {"1.20.1"};
		version.loaders = {"forge"};
		QVERIFY(ContentBrowser::isVersionCompatible(version, "1.20.1", ""));
		QVERIFY(ContentBrowser::isVersionCompatible(version, "", "forge"));
	}

	void test_SortOptionsForModrinth()
	{
		/* Mirrors ModrinthApi::sortingMethods() (five string-valued
		 * sorts) - the point being that `id` is the position in the list,
		 * not the provider's own apiValue, since that is what
		 * ContentProviderModel::search()'s sortIndex parameter expects. */
		const QList<ModPlatform::SortingMethod> methods = {
			{"relevance", "Sort by Relevance"},
			{"downloads", "Sort by Downloads"},
			{"follows", "Sort by Follows"},
			{"newest", "Sort by Newest"},
			{"updated", "Sort by Last Updated"},
		};

		const QVariantList options = ContentBrowser::sortOptionsFor(methods);
		QCOMPARE(options.size(), 5);

		const QVariantMap first = options.at(0).toMap();
		QCOMPARE(first.value("id").toInt(), 0);
		QCOMPARE(first.value("label").toString(),
				 QString("Sort by Relevance"));

		const QVariantMap last = options.at(4).toMap();
		QCOMPARE(last.value("id").toInt(), 4);
		QCOMPARE(last.value("label").toString(),
				 QString("Sort by Last Updated"));
	}

	void test_SortOptionsForEmptyListIsEmpty()
	{
		QVERIFY(ContentBrowser::sortOptionsFor({}).isEmpty());
	}
};

QTEST_GUILESS_MAIN(ContentBrowserTest)

#include "ContentBrowser_test.moc"
