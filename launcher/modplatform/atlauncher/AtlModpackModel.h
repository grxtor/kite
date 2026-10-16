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

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariant>
#include <functional>
#include <optional>

#include <net/NetJob.h>

#include "meta/VersionList.h"
#include "modplatform/atlauncher/ATLPackIndex.h"
#include "modplatform/atlauncher/ATLPackInstallTask.h"

class ModrinthModpackDetail;
class AtlOptionalModsModel;

/* ATLauncher modpacks for QML - the widget-free replacement for
 * ui/pages/modplatform/atlauncher/AtlListModel.h + AtlFilterModel.h,
 * which back AtlPage.
 *
 * The same shape as ModrinthModpackModel, FlameModpackModel and
 * FtbModpackModel (properties, invokables, role names), so the Discover
 * page shows any of them through one set of bindings. ATLauncher serves
 * its whole catalogue as one file (packsnew.json); searching and sorting
 * happen here, over it, with the widget page's filters: public packs with
 * a published version, no "system" packs. Packs are addressed by their
 * safe name, which is what the install task and the logo address take.
 *
 * Installing asks two questions the widget page answered with its own
 * dialogs (ATLauncher::UserInteractionSupport):
 *   - Which optional mods: an AtlOptionalModsModel, handed to
 *     setOptionalModsHandler()'s callback - the UiHost question the
 *     integrator adds (see the WP8 wiring note). Without one, the pack's
 *     own default selection is installed.
 *   - Which loader version, for the few packs that leave it open: asked
 *     through UiHost::choose() with the recommended and newest few
 *     (versionChoices()); without a host, the recommended one.
 */
class AtlModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	/* "" = any version; otherwise packs with a version for it. */
	Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion
				   NOTIFY gameVersionChanged)
	/* Kept for the shared bindings; ATLauncher's catalogue does not say
	 * which loader a pack uses, so it is not used to filter. */
	Q_PROPERTY(
		QString loader READ loader WRITE setLoader NOTIFY loaderChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(QObject* detail READ detail CONSTANT)
	Q_PROPERTY(QString providerName READ providerName CONSTANT)
	/// "markdown": a pack's description is plain text, which reads as such.
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
	Q_PROPERTY(bool keyMissing READ keyMissing CONSTANT)

  public:
	enum Roles {
		ProjectIdRole = Qt::UserRole + 1,
		SlugRole,
		TitleRole,
		DescriptionRole,
		AuthorRole,
		LogoUrlRole,
		DownloadsRole,
		FollowsRole,
		UpdatedRole,
		LatestVersionRole,
		GameVersionsRole,
		CategoriesRole,
		GalleryUrlRole,
		AccentColorRole,
	};
	Q_ENUM(Roles)

	/* Answers "which optional mods" with the ticked names, or nullopt to
	 * keep the pack's defaults. Runs while the install waits (UiHost's
	 * blocking convention). */
	using OptionalModsHandler = std::function<std::optional<QVector<QString>>(
		const QString& packName, AtlOptionalModsModel& mods)>;

	explicit AtlModpackModel(QObject* parent = nullptr);
	~AtlModpackModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	using QAbstractItemModel::canFetchMore;
	using QAbstractItemModel::fetchMore;
	using QAbstractItemModel::sort;

	QString query() const
	{
		return m_query;
	}
	void setQuery(const QString& query);
	QString sort() const
	{
		return m_sort;
	}
	void setSort(const QString& sort);
	QVariantList sortOptions() const
	{
		return m_sortOptions;
	}
	QString gameVersion() const
	{
		return m_gameVersion;
	}
	void setGameVersion(const QString& gameVersion);
	QString loader() const
	{
		return m_loader;
	}
	void setLoader(const QString& loader);
	bool searching() const
	{
		return m_loading;
	}
	bool canFetchMore() const
	{
		return false;
	}
	int count() const
	{
		return int(m_rows.size());
	}
	QString error() const
	{
		return m_error;
	}
	QObject* detail() const;
	QString providerName() const
	{
		return QStringLiteral("ATLauncher");
	}
	QString bodyFormat() const
	{
		return QStringLiteral("markdown");
	}
	bool keyMissing() const
	{
		return false;
	}

	/// See OptionalModsHandler; set by the shell once UiHost can ask.
	void setOptionalModsHandler(OptionalModsHandler handler);

	/// Loads the catalogue the first time; filters and sorts every time.
	Q_INVOKABLE void search();
	Q_INVOKABLE void fetchMore() {}
	/// Fills `detail` from the loaded pack; no request.
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Starts ATLauncher::PackInstallTask for version `versionId` (one of
	 * the ids in `detail.versions`) of pack `projectId` (its safe name)
	 * and returns a TaskWatcher for it (parented to this model); nullptr
	 * when either names nothing loaded. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* The catalogue -> the packs AtlPage listed: public, not "system",
	 * with a published version. Static, for tests. */
	static QList<ATLauncher::IndexedPack> parseCatalogue(const QByteArray& bytes);
	/* Positions in @p packs that match @p query and @p gameVersion, in
	 * @p sort order ("popularity", "name", "gameVersion"). Static. */
	static QList<int> filterAndSort(const QList<ATLauncher::IndexedPack>& packs,
									const QString& query,
									const QString& gameVersion,
									const QString& sort);
	/* The `versions` of the detail, as the catalogue lists them (newest
	 * first), in the shared shape. Static. */
	static QVariantList versionEntries(const ATLauncher::IndexedPack& pack);
	/* Where ATLauncher keeps a pack's square logo. */
	static QString logoUrl(const ATLauncher::IndexedPack& pack);
	/* The loader versions to offer when a pack leaves its loader open:
	 * those for @p minecraftVersion (any when empty), recommended ones
	 * first, at most @p limit. Static. */
	static QList<Meta::VersionPtr>
	versionChoices(const QVector<Meta::VersionPtr>& versions,
				   const QString& minecraftVersion, int limit);

  signals:
	void queryChanged();
	void sortChanged();
	void gameVersionChanged();
	void loaderChanged();
	void searchingChanged();
	void canFetchMoreChanged();
	void countChanged();
	void errorChanged();

  private:
	void setLoading(bool loading);
	void setError(const QString& error);
	void refilter();
	const ATLauncher::IndexedPack* packFor(const QString& projectId) const;
	QString resolveIconKey(const ATLauncher::IndexedPack& pack) const;

  private:
	QList<ATLauncher::IndexedPack> m_packs;
	QList<int> m_rows;

	QString m_query;
	QString m_sort = QStringLiteral("popularity");
	QVariantList m_sortOptions;
	QString m_gameVersion;
	QString m_loader;
	bool m_loading = false;
	bool m_loaded = false;
	QString m_error;
	NetJob::Ptr m_job;
	QByteArray m_response;

	ModrinthModpackDetail* m_detail = nullptr;
	OptionalModsHandler m_optionalModsHandler;
};
