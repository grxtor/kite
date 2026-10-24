/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <algorithm>

#include "core/LauncherContext.h"
#include "minecraft/mod/ModFolderModel.h"
#include "models/ContentInstallReview.h"

namespace
{
	constexpr int kHttpOk = 200;
	constexpr int kHttpNotFound = 404;
	/* Answers come off a zero-length timer, so a resolution is over in a
	 * few event-loop turns; this is only the ceiling for a loaded machine. */
	constexpr int kResolveTimeoutMs = 10000;

	/* What a CannedNetwork says to a request whose URL contains `urlHas`,
	 * and how long it takes to say it. */
	struct CannedAnswer {
		QString urlHas;
		QByteArray body;
		int delayMs = 0;
	};

	/* A reply that is made up: the body of a CannedAnswer, or a 404. */
	class CannedReply : public QNetworkReply
	{
	  public:
		CannedReply(const QNetworkRequest& request, bool found,
					const QByteArray& body, int delayMs, QObject* parent)
			: QNetworkReply(parent), m_found(found), m_body(body)
		{
			setRequest(request);
			setUrl(request.url());
			setOperation(QNetworkAccessManager::GetOperation);
			setAttribute(QNetworkRequest::HttpStatusCodeAttribute,
						 found ? kHttpOk : kHttpNotFound);
			open(QIODevice::ReadOnly);
			QTimer::singleShot(delayMs, this, &CannedReply::deliver);
		}

		void abort() override {}
		qint64 bytesAvailable() const override
		{
			return m_body.size() - m_read + QNetworkReply::bytesAvailable();
		}

	  protected:
		qint64 readData(char* data, qint64 maxSize) override
		{
			const qint64 count = qMin(maxSize, qint64(m_body.size()) - m_read);
			memcpy(data, m_body.constData() + m_read, size_t(count));
			m_read += count;
			return count;
		}

	  private:
		void deliver()
		{
			if (!m_found) {
				setError(ContentNotFoundError, QStringLiteral("not found"));
				emit errorOccurred(ContentNotFoundError);
			} else if (!m_body.isEmpty()) {
				emit readyRead();
			}
			setFinished(true);
			emit finished();
		}

		bool m_found;
		QByteArray m_body;
		qint64 m_read = 0;
	};

	/* A network that never leaves the machine: it answers from a table, in
	 * the order the table was filled, and says 404 to anything else. It
	 * keeps the URLs it was asked for. */
	class CannedNetwork : public QNetworkAccessManager
	{
	  public:
		void answer(const QString& urlHas, const QByteArray& body,
					int delayMs = 0)
		{
			m_answers.append({urlHas, body, delayMs});
		}
		QStringList requested() const
		{
			return m_requested;
		}

	  protected:
		QNetworkReply* createRequest(Operation, const QNetworkRequest& request,
									 QIODevice*) override
		{
			const QString url = request.url().toString();
			m_requested.append(url);
			for (const CannedAnswer& answer : m_answers) {
				if (url.contains(answer.urlHas)) {
					return new CannedReply(request, true, answer.body,
										   answer.delayMs, this);
				}
			}
			return new CannedReply(request, false, QByteArray(), 0, this);
		}

	  private:
		QList<CannedAnswer> m_answers;
		QStringList m_requested;
	};

	/* Just enough launcher for the dependency resolver, which asks for the
	 * network and nothing else. */
	class CannedLauncher : public LauncherContext
	{
	  public:
		CannedLauncher() : m_network(new CannedNetwork)
		{
			setInstance(this);
		}
		~CannedLauncher() override
		{
			setInstance(nullptr);
		}

		CannedNetwork* canned() const
		{
			return static_cast<CannedNetwork*>(m_network.get());
		}

		shared_qobject_ptr<QNetworkAccessManager> network() override
		{
			return m_network;
		}
		std::shared_ptr<SettingsObject> settings() const override
		{
			return {};
		}
		std::shared_ptr<InstanceList> instances() const override
		{
			return {};
		}
		std::shared_ptr<IconList> icons() const override
		{
			return {};
		}
		shared_qobject_ptr<AccountList> accounts() const override
		{
			return {};
		}
		std::shared_ptr<TranslationsModel> translations() override
		{
			return {};
		}
		std::shared_ptr<JavaInstallList> javalist() override
		{
			return {};
		}
		shared_qobject_ptr<HttpMetaCache> metacache() override
		{
			return {};
		}
		shared_qobject_ptr<Meta::Index> metadataIndex() override
		{
			return {};
		}
		QIcon getThemedIcon(const QString&) override
		{
			return QIcon();
		}
		QString getJarsPath() override
		{
			return QString();
		}
		QString msaClientId() const override
		{
			return QString();
		}
		void updateProxySettings(QString, QString, int, QString,
								 QString) override
		{
		}
		AuthRequestDecorator* authRequestDecorator() const override
		{
			return nullptr;
		}
		UiHost* uiHost() const override
		{
			return nullptr;
		}

	  private:
		shared_qobject_ptr<QNetworkAccessManager> m_network;
	};

	/* What Modrinth lists at /project/<x>/version: one release of project
	 * @p projectId, for @p loader on 1.21.4, with the one file @p file. */
	QByteArray modrinthVersions(const QString& projectId, const QString& name,
								const QString& file,
								const QString& loader = QStringLiteral("fabric"))
	{
		return QStringLiteral(
				   R"([{"id":"ver%1","project_id":"%1","name":"%2",)"
				   R"("version_type":"release","game_versions":["1.21.4"],)"
				   R"("loaders":["%3"],"date_published":"2025-01-01T00:00:00Z",)"
				   R"("dependencies":[],"files":[{"primary":true,)"
				   R"("filename":"%4","url":"https://cdn.example/%4",)"
				   R"("size":10,"hashes":{"sha1":"abc"}}]}])")
			.arg(projectId, name, loader, file)
			.toUtf8();
	}

	/* What CurseForge lists at /mods/<id>/files: one release of Fabric on
	 * 1.21.4 - or of @p loader, when it is another. */
	QByteArray curseForgeFiles(const QString& displayName, const QString& file,
							   const QString& loader = QStringLiteral("Fabric"))
	{
		return QStringLiteral(
				   R"({"data":[{"id":777,"fileName":"%2","displayName":"%1",)"
				   R"("downloadUrl":"https://cdn.example/%2","fileLength":5,)"
				   R"("releaseType":1,"fileDate":"2025-01-01T00:00:00Z",)"
				   R"("gameVersions":["1.21.4","%3"],)"
				   R"("hashes":[{"algo":1,"value":"abc"}],"dependencies":[]}]})")
			.arg(displayName, file, loader)
			.toUtf8();
	}

	/* A CurseForge search that finds one project. */
	QByteArray curseForgeHit(int id, const QString& slug, const QString& name)
	{
		return QStringLiteral(
				   R"({"data":[{"id":%1,"slug":"%2","name":"%3"}]})")
			.arg(QString::number(id), slug, name)
			.toUtf8();
	}
} // namespace

/*
 * The review before an install, fed through applyResolution() - the seam
 * the dependency resolver's answer goes through - so no provider is ever
 * asked anything. install() itself downloads, and is only checked for
 * refusing to run before the review is ready.
 *
 * The mod health check's wanted projects do go through the resolver, over a
 * CannedNetwork that answers from a table: the lookups, their fall-through
 * to CurseForge and what comes out are what is checked there.
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

	/* A review over no installed mods at all, asking only for what the mod
	 * health check found missing. */
	std::unique_ptr<ContentInstallReview> wantedOnly(
		const QList<DependencyResolver::WantedProject>& wanted)
	{
		std::unique_ptr<ContentInstallReview> review(
			ContentInstallReview::missingDependencies(
				{}, "Missing dependencies", "1.21.4", "fabric", folder()));
		review->setWantedProjects(wanted);
		return review;
	}

	QTemporaryDir m_dir;
	std::unique_ptr<CannedLauncher> m_launcher;

  private slots:
	void init()
	{
		m_launcher = std::make_unique<CannedLauncher>();
	}

	void cleanup()
	{
		m_launcher.reset();
	}

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

	void replacementsReplaceTheOutdatedFileAndStartTicked()
	{
		auto review = wantedOnly({});
		review->setReplacements({{"cloth-config", "cloth-config-old.jar"}});

		/* Both have another version on disk; only one of them is there to
		 * be replaced. */
		auto cloth = dependency("Cloth Config", /*maybeInstalled=*/true);
		cloth.slug = "cloth-config";
		auto menu = dependency("Mod Menu", /*maybeInstalled=*/true);
		menu.slug = "modmenu";
		auto fresh = dependency("Lib", /*maybeInstalled=*/false);
		fresh.slug = "lib";
		review->applyResolution({cloth, menu, fresh}, {});

		QCOMPARE(review->dependencyCount(), 3);
		const QModelIndex replacing = review->index(0);
		QVERIFY(replacing.data(ContentInstallReview::MaybeInstalledRole).toBool());
		QVERIFY(replacing.data(ContentInstallReview::CheckedRole).toBool());
		QCOMPARE(replacing.data(ContentInstallReview::ReplacesRole).toString(),
				 QStringLiteral("cloth-config-old.jar"));
		/* Another version of it is installed and nothing says to replace
		 * it: offered, unticked, as before. */
		QVERIFY(!review->index(1).data(ContentInstallReview::CheckedRole).toBool());
		QVERIFY(review->index(1).data(ContentInstallReview::ReplacesRole)
					.toString().isEmpty());
		QVERIFY(review->index(2).data(ContentInstallReview::CheckedRole).toBool());

		const auto items = review->downloadItems();
		QCOMPARE(items.size(), 2);
		QCOMPARE(items.at(0).name, QStringLiteral("Cloth Config"));
		QVERIFY(items.at(0).replaceExisting);
		QCOMPARE(items.at(0).replacesFileName,
				 QStringLiteral("cloth-config-old.jar"));
		QCOMPARE(items.at(1).name, QStringLiteral("Lib"));
		QVERIFY(!items.at(1).replaceExisting);
		QVERIFY(items.at(1).replacesFileName.isEmpty());
	}

	void wantedProjectsAloneStillNeedResolving()
	{
		m_launcher->canned()->answer(
			"/project/modmenu/version",
			modrinthVersions("mOdMeNuP", "v11.0.0 for 1.21.4", "modmenu-11.jar"));
		m_launcher->canned()->answer("/project/mOdMeNuP",
									 R"({"id":"mOdMeNuP","title":"Mod Menu"})");
		auto review = wantedOnly({{"modmenu", {"Alpha"}}});
		review->setReplacements({{"modmenu", "modmenu-9.jar"}});

		review->start();
		/* No installed mod to ask about, yet not settled at once: there is
		 * something to look for. */
		QVERIFY(review->resolving());
		QVERIFY(!review->ready());
		QVERIFY(!review->needsReview());
		QTRY_VERIFY_WITH_TIMEOUT(review->ready(), kResolveTimeoutMs);

		QCOMPARE(review->dependencyCount(), 1);
		const QModelIndex row = review->index(0);
		/* Listed under the project's title, not the version's tag. */
		QCOMPARE(row.data(ContentInstallReview::NameRole).toString(),
				 QStringLiteral("Mod Menu"));
		QCOMPARE(row.data(ContentInstallReview::ProviderRole).toString(),
				 QStringLiteral("Modrinth"));
		QCOMPARE(row.data(ContentInstallReview::RequiredByRole).toString(),
				 QStringLiteral("Alpha"));
		QVERIFY(row.data(ContentInstallReview::CheckedRole).toBool());

		/* Recorded under the project's own id, not the slug it was asked
		 * for by; the slug says which wanted id it answers. */
		const auto items = review->downloadItems();
		QCOMPARE(items.size(), 1);
		QCOMPARE(items.first().projectId, QStringLiteral("mOdMeNuP"));
		QCOMPARE(items.first().slug, QStringLiteral("modmenu"));
		QVERIFY(items.first().isDependency);
		QVERIFY(items.first().replaceExisting);
		QCOMPARE(items.first().replacesFileName, QStringLiteral("modmenu-9.jar"));

		/* One lookup, by slug, for this Minecraft version and loader, then
		 * the project's title by its id. */
		const QStringList asked = m_launcher->canned()->requested();
		QCOMPARE(asked.size(), 2);
		QVERIFY(asked.first().contains("/project/modmenu/version"));
		QVERIFY(asked.first().contains("1.21.4"));
		QVERIFY(asked.first().contains("fabric"));
		QVERIFY(asked.last().endsWith("/project/mOdMeNuP"));
	}

	void aModIdThatIsNotItsSlugIsAskedForBySlug()
	{
		m_launcher->canned()->answer(
			"/project/architectury-api/version",
			modrinthVersions("lhGA9TYQ", "13.0.8", "architectury-13.jar"));
		auto review = wantedOnly({{"architectury", {"Alpha"}}});

		review->start();
		QTRY_VERIFY_WITH_TIMEOUT(review->ready(), kResolveTimeoutMs);

		QCOMPARE(review->dependencyCount(), 1);
		/* The row still answers the id the jar asked for. */
		QCOMPARE(review->downloadItems().first().slug,
				 QStringLiteral("architectury"));
		QVERIFY(m_launcher->canned()->requested().first().contains(
			"/project/architectury-api/version"));
	}

	void curseForgeIsAskedWhenModrinthHasNoSuchProject()
	{
		auto* net = m_launcher->canned();
		net->answer("/mods/search", curseForgeHit(4242, "cloth-config", "Cloth Config API"));
		net->answer("/mods/4242/files",
					curseForgeFiles("Cloth Config 15.0", "cloth-config-15.jar"));
		auto review = wantedOnly({{"cloth-config", {"Alpha", "Beta"}}});
		review->setReplacements({{"cloth-config", "cloth-config-old.jar"}});

		review->start();
		QTRY_VERIFY_WITH_TIMEOUT(review->ready(), kResolveTimeoutMs);

		QCOMPARE(review->dependencyCount(), 1);
		QVERIFY(review->unresolved().isEmpty());
		const auto items = review->downloadItems();
		QCOMPARE(items.size(), 1);
		QCOMPARE(items.first().platform, QStringLiteral("curseforge"));
		QCOMPARE(items.first().projectId, QStringLiteral("4242"));
		QCOMPARE(items.first().name, QStringLiteral("Cloth Config 15.0"));
		QCOMPARE(items.first().slug, QStringLiteral("cloth-config"));
		QCOMPARE(items.first().replacesFileName,
				 QStringLiteral("cloth-config-old.jar"));
		QCOMPARE(review->index(0).data(ContentInstallReview::RequiredByRole).toString(),
				 QStringLiteral("Alpha, Beta"));

		/* Modrinth first (a 404 is retried like any failed download, so
		 * more than once), then the search by the id as name and slug, then
		 * the files of the project it found, each asked once. */
		const QStringList asked = net->requested();
		QVERIFY(asked.first().contains("/project/cloth-config/version"));
		QVERIFY(asked.at(asked.size() - 2).contains("/mods/search"));
		QVERIFY(asked.at(asked.size() - 2).contains("searchFilter=cloth-config"));
		QVERIFY(asked.last().contains("/mods/4242/files"));
		QCOMPARE(asked.filter("/mods/search").size(), 1);
	}

	void aWantedProjectNeitherProviderHasIsUnresolvedOnce_data()
	{
		QTest::addColumn<QByteArray>("modrinth");
		QTest::addColumn<QByteArray>("search");
		QTest::addColumn<QByteArray>("files");

		QTest::newRow("Modrinth 404s, CurseForge 404s")
			<< QByteArray() << QByteArray() << QByteArray();
		QTest::newRow("no Modrinth version for this loader")
			<< modrinthVersions("p", "Ghost", "ghost.jar", "forge")
			<< QByteArray() << QByteArray();
		QTest::newRow("CurseForge finds nothing by that slug or title")
			<< QByteArray() << QByteArray(R"({"data":[{"id":5,"slug":"other","name":"Other"}]})")
			<< QByteArray();
		QTest::newRow("CurseForge has it, but nothing for this loader")
			<< QByteArray() << curseForgeHit(9, "ghost", "Ghost")
			<< curseForgeFiles("Ghost", "ghost.jar", "Forge");
	}

	void aWantedProjectNeitherProviderHasIsUnresolvedOnce()
	{
		QFETCH(QByteArray, modrinth);
		QFETCH(QByteArray, search);
		QFETCH(QByteArray, files);
		auto* net = m_launcher->canned();
		if (!modrinth.isEmpty()) {
			net->answer("/project/ghost/version", modrinth);
		}
		if (!search.isEmpty()) {
			net->answer("/mods/search", search);
		}
		if (!files.isEmpty()) {
			net->answer("/mods/9/files", files);
		}
		/* Asked for twice, by two files. */
		auto review = wantedOnly({{"ghost", {"Alpha"}}, {"ghost", {"Beta"}}});

		review->start();
		QTRY_VERIFY_WITH_TIMEOUT(review->ready(), kResolveTimeoutMs);

		QCOMPARE(review->unresolved(), QStringList{"ghost"});
		QCOMPARE(review->dependencyCount(), 0);
		QVERIFY(review->needsReview());
		QVERIFY(review->downloadItems().isEmpty());
	}

	void aWantedProjectAnInstalledModAlreadyNeedsIsOneRow_data()
	{
		QTest::addColumn<int>("wantedDelayMs");
		QTest::addColumn<int>("projectDelayMs");

		/* The two answers can arrive in either order, or the wanted one in
		 * between the other chain claiming the project and recording it;
		 * the outcome is the same. */
		QTest::newRow("the wanted lookup answers first") << 0 << 0;
		QTest::newRow("the mod's own chain answers first") << 80 << 0;
		QTest::newRow("noted while the project is still on its way") << 30 << 120;
	}

	void aWantedProjectAnInstalledModAlreadyNeedsIsOneRow()
	{
		QFETCH(int, wantedDelayMs);
		QFETCH(int, projectDelayMs);
		auto* net = m_launcher->canned();
		/* An installed mod whose record says it needs project mOdMeNuP... */
		net->answer("/version/sverid01",
					R"({"id":"sverid01","project_id":"InstModP","name":"Installed 1.0",)"
					R"("game_versions":["1.21.4"],"loaders":["fabric"],"files":[],)"
					R"("dependencies":[{"project_id":"mOdMeNuP","dependency_type":"required"}]})");
		net->answer("/project/mOdMeNuP/version",
					modrinthVersions("mOdMeNuP", "Mod Menu 11.0.0", "modmenu-11.jar"),
					projectDelayMs);
		/* ...which the health check also asks for, by its mod id. */
		net->answer("/project/modmenu/version",
					modrinthVersions("mOdMeNuP", "Mod Menu 11.0.0", "modmenu-11.jar"),
					wantedDelayMs);

		ModPlatform::SelectedMod installed;
		installed.name = "Installed Mod";
		installed.projectId = "InstModP";
		installed.versionId = "sverid01";
		installed.platform = "modrinth";
		std::unique_ptr<ContentInstallReview> review(
			ContentInstallReview::missingDependencies(
				{installed}, "Missing dependencies", "1.21.4", "fabric", folder()));
		review->setWantedProjects({{"modmenu", {"Alpha"}}});
		review->setReplacements({{"modmenu", "modmenu-9.jar"}});

		review->start();
		QTRY_VERIFY_WITH_TIMEOUT(review->ready(), kResolveTimeoutMs);

		QCOMPARE(review->dependencyCount(), 1);
		QVERIFY(review->unresolved().isEmpty());
		/* Both asked for it, and it knows which wanted id it answers. */
		QStringList askedBy =
			review->index(0).data(ContentInstallReview::RequiredByRole).toString()
				.split(QStringLiteral(", "));
		std::sort(askedBy.begin(), askedBy.end());
		QCOMPARE(askedBy, (QStringList{"Alpha", "Installed Mod"}));
		const auto items = review->downloadItems();
		QCOMPARE(items.size(), 1);
		QCOMPARE(items.first().projectId, QStringLiteral("mOdMeNuP"));
		QCOMPARE(items.first().slug, QStringLiteral("modmenu"));
		QCOMPARE(items.first().replacesFileName, QStringLiteral("modmenu-9.jar"));
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
