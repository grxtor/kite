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

#include "models/SkinsController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

#include "DesktopServices.h"
#include "core/LauncherContext.h"
#include "minecraft/skins/DefaultSkins.h"
#include "minecraft/skins/ProfileSkinImport.h"
#include "minecraft/skins/SkinEntry.h"
#include "minecraft/skins/SkinLibrary.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "tasks/TaskWatcher.h"

namespace
{
	QString temporarySkinPath()
	{
		return QDir::temp().filePath(
			QStringLiteral("meshmc-skin-%1.png")
				.arg(QUuid::createUuid().toString(QUuid::Id128)));
	}

	/* A free "<base>.png" in @p dir: "<base> (2).png" and so on when
	 * taken, so an import never replaces a skin the user already has. */
	QString freeSkinPath(const QDir& dir, QString base)
	{
		base = base.trimmed();
		if (base.isEmpty()) {
			base = QStringLiteral("skin");
		}
		QString candidate = dir.filePath(base + QStringLiteral(".png"));
		for (int n = 2; QFileInfo::exists(candidate); ++n) {
			candidate =
				dir.filePath(QStringLiteral("%1 (%2).png").arg(base).arg(n));
		}
		return candidate;
	}

	/* A skin downloaded from a link: fetched to a temporary file, checked
	 * to be a skin (a server can answer 200 with an error page), then
	 * copied into the library under the link's file name. */
	class SkinUrlImportTask final : public Task
	{
	  public:
		SkinUrlImportTask(QUrl url, SkinLibrary* library)
			: m_url(std::move(url)), m_library(library)
		{
		}

	  protected:
		void executeTask() override
		{
			m_temp = temporarySkinPath();
			m_job.reset(new NetJob(SkinsController::tr("Download skin"),
								   LAUNCHER->network()));
			m_job->addNetAction(Net::Download::makeFile(m_url, m_temp));
			connect(m_job.get(), &Task::finished, this, [this]() { finish(); });
			setStatus(SkinsController::tr("Downloading the skin…"));
			m_job->start();
		}

	  private:
		void finish()
		{
			const bool downloaded = QFileInfo::exists(m_temp);
			if (!downloaded || !SkinEntry(m_temp).isUsable()) {
				QFile::remove(m_temp);
				emitFailed(downloaded
							   ? SkinsController::tr("That link is not a skin. Skins are 64x64 or 64x32 PNG files.")
							   : SkinsController::tr("The skin could not be downloaded."));
				return;
			}
			if (!m_library) {
				QFile::remove(m_temp);
				emitFailed(SkinsController::tr("The skin library was closed."));
				return;
			}
			const QString base = QFileInfo(m_url.fileName()).completeBaseName();
			const QString problem = m_library->importFile(
				m_temp,
				(base.isEmpty() ? QStringLiteral("skin") : base) +
					QStringLiteral(".png"));
			QFile::remove(m_temp);
			if (problem.isEmpty()) {
				emitSucceeded();
			} else {
				emitFailed(problem);
			}
		}

		QUrl m_url;
		QPointer<SkinLibrary> m_library;
		QString m_temp;
		shared_qobject_ptr<NetJob> m_job;
	};

	/* Another player's skin, by name (ProfileSkinImport), written into the
	 * library with what the profile knows and the PNG does not: the arm
	 * width and the texture's own url. */
	class SkinUsernameImportTask final : public Task
	{
	  public:
		SkinUsernameImportTask(QString username, SkinLibrary* library)
			: m_username(std::move(username)), m_library(library)
		{
		}

	  protected:
		void executeTask() override
		{
			if (!m_library) {
				emitFailed(SkinsController::tr("The skin library was closed."));
				return;
			}
			m_target = freeSkinPath(QDir(m_library->directory()), m_username);
			m_import.reset(new ProfileSkinImport(nullptr, m_username, m_target));
			connect(m_import.get(), &Task::status, this,
					[this](const QString& status) { setStatus(status); });
			connect(m_import.get(), &Task::succeeded, this,
					[this]() { finish(); });
			connect(m_import.get(), &Task::failed, this,
					[this](const QString& reason) {
						QFile::remove(m_target);
						emitFailed(reason.isEmpty()
									   ? SkinsController::tr("No skin was found for that name.")
									   : reason);
					});
			m_import->start();
		}

	  private:
		void finish()
		{
			SkinEntry imported(m_target);
			if (!imported.isUsable() || !m_library) {
				QFile::remove(m_target);
				emitFailed(SkinsController::tr("That player's skin could not be read."));
				return;
			}
			imported.setArms(m_import->arms());
			imported.setTextureUrl(m_import->textureUrl());
			m_library->mergeEntry(imported);
			emitSucceeded();
		}

		QString m_username;
		QPointer<SkinLibrary> m_library;
		QString m_target;
		shared_qobject_ptr<ProfileSkinImport> m_import;
	};

	/* The image urls of the skin PNG at @p path drawn with @p slim arms:
	 * its normalised texture (for a 3D preview), its flat sprite and its
	 * front view (for a flat one), through AccountFaceProvider. */
	QVariantMap imageUrls(const QString& path, bool slim)
	{
		// Percent-encoded whole, slashes included, so a path with spaces or
		// '#' survives the trip through an image:// url intact.
		const QString encoded =
			QString::fromLatin1(QUrl::toPercentEncoding(path));
		return QVariantMap{
			{ QStringLiteral("textureUrl"),
			  QStringLiteral("image://accountface/skinfile/") + encoded },
			{ QStringLiteral("thumbnailUrl"),
			  (slim ? QStringLiteral("image://accountface/skinthumbslim/")
					: QStringLiteral("image://accountface/skinthumb/")) +
				  encoded },
			{ QStringLiteral("bodyUrl"),
			  (slim ? QStringLiteral("image://accountface/skinbodyslim/")
					: QStringLiteral("image://accountface/skinbody/")) +
				  encoded },
		};
	}

	QObject* watch(Task* task, const QString& title, QObject* parent)
	{
		auto* watcher = new TaskWatcher(Task::Ptr(task), parent);
		watcher->setTitle(title);
		task->start();
		return watcher;
	}
} // namespace

SkinsController::SkinsController(MinecraftAccountPtr account,
								 const QString& directory, QObject* parent)
	: QObject(parent), m_account(account),
	  m_library(std::make_unique<SkinLibrary>(nullptr, directory, account))
{
	auto* library = m_library.get();
	connect(library, &QAbstractItemModel::modelReset, this,
			&SkinsController::skinsChanged);
	connect(library, &QAbstractItemModel::rowsInserted, this,
			&SkinsController::skinsChanged);
	connect(library, &QAbstractItemModel::rowsRemoved, this,
			&SkinsController::skinsChanged);
	connect(library, &QAbstractItemModel::dataChanged, this,
			&SkinsController::skinsChanged);

	connect(DefaultSkins::instance(), &DefaultSkins::galleryChanged, this,
			&SkinsController::defaultSkinsChanged);
	/* Which default skin is worn follows the profile (a refresh after
	 * wearing one). */
	if (m_account) {
		connect(m_account.get(), &MinecraftAccount::changed, this,
				&SkinsController::defaultSkinsChanged);
	}
}

SkinsController::~SkinsController() = default;

QVariantList SkinsController::skins() const
{
	QVariantList list;
	const int worn = m_library->indexOfAccountSkin();
	for (int row = 0; row < m_library->rowCount(); ++row) {
		const QString name =
			m_library->data(m_library->index(row, 0), SkinLibrary::NameRole)
				.toString();
		const SkinEntry* entry = m_library->entry(name);
		if (!entry) {
			continue;
		}
		const bool slim = entry->arms() == SkinEntry::Arms::Slim;
		QVariantMap skin = imageUrls(entry->path(), slim);
		skin.insert(QStringLiteral("name"), name);
		skin.insert(QStringLiteral("path"), entry->path());
		skin.insert(QStringLiteral("url"), QUrl::fromLocalFile(entry->path()));
		skin.insert(QStringLiteral("slim"), slim);
		skin.insert(QStringLiteral("worn"), row == worn);
		list.append(skin);
	}
	return list;
}

QVariantList SkinsController::defaultSkins() const
{
	QVariantList list;
	for (const DefaultSkins::Character& character :
		 DefaultSkins::characters()) {
		QVariantList variants;
		for (const bool slim : { false, true }) {
			const QString path = DefaultSkins::galleryPath(character.name, slim);
			const bool ready = QFileInfo::exists(path);
			QVariantMap variant = ready ? imageUrls(path, slim) : QVariantMap();
			variant.insert(QStringLiteral("name"), character.name);
			variant.insert(QStringLiteral("path"), path);
			variant.insert(QStringLiteral("slim"), slim);
			variant.insert(QStringLiteral("ready"), ready);
			variant.insert(
				QStringLiteral("worn"),
				isWorn(DefaultSkins::galleryUrl(character.name, slim)));
			variants.append(variant);
		}
		list.append(QVariantMap{
			{ QStringLiteral("name"), character.name },
			{ QStringLiteral("slim"), character.slim },
			{ QStringLiteral("variants"), variants },
		});
	}
	return list;
}

bool SkinsController::defaultSkinsLoading() const
{
	return DefaultSkins::instance()->galleryLoading();
}

bool SkinsController::isWorn(const QString& url) const
{
	if (!m_account || !m_account->accountData()) {
		return false;
	}
	const Skin& worn = m_account->accountData()->minecraftProfile.skin;
	/* By the texture's hash: the profile hands its url out over plain
	 * http as often as not. */
	return !worn.url.isEmpty() &&
		   QUrl(worn.url).fileName() == QUrl(url).fileName();
}

QString SkinsController::directory() const
{
	return m_library->directory();
}

QString SkinsController::localPath(const QString& fileUrlOrPath)
{
	const QUrl url(fileUrlOrPath);
	return url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
}

QString SkinsController::importFile(const QString& fileUrlOrPath)
{
	const QString path = localPath(fileUrlOrPath);
	// importFile() trusts a ".png" name; a skin is checked here either way,
	// so a wrong-sized PNG is refused now instead of listed and never used.
	if (!SkinEntry(path).isUsable()) {
		return tr("Skins are 64x64 or 64x32 PNG files.");
	}
	return m_library->importFile(path);
}

QObject* SkinsController::importFromUrl(const QString& link)
{
	// A link as written, scheme and all: guessing one would send plain
	// words off as a download.
	const QUrl url(link.trimmed(), QUrl::StrictMode);
	if (!url.isValid() || url.host().isEmpty() ||
		(url.scheme() != QLatin1String("http") &&
		 url.scheme() != QLatin1String("https"))) {
		return nullptr;
	}
	return watch(new SkinUrlImportTask(url, m_library.get()),
				 tr("Skin from a link"), this);
}

QObject* SkinsController::importFromUsername(const QString& username)
{
	const QString name = username.trimmed();
	if (name.isEmpty()) {
		return nullptr;
	}
	return watch(new SkinUsernameImportTask(name, m_library.get()),
				 tr("Skin of %1").arg(name), this);
}

bool SkinsController::rename(const QString& name, const QString& newName)
{
	const QString trimmed = newName.trimmed();
	const int row = m_library->indexOfName(name);
	if (row < 0 || trimmed.isEmpty() || trimmed == name) {
		return false;
	}
	// SkinLibrary turns an edit of the name into the file rename, and
	// refuses a name that is taken.
	return m_library->setData(m_library->index(row, 0), trimmed, Qt::EditRole);
}

QString SkinsController::remove(const QString& name)
{
	const int row = m_library->indexOfName(name);
	if (row < 0) {
		return tr("That skin is not in the library any more.");
	}
	if (row == m_library->indexOfAccountSkin()) {
		return tr("This is the skin you are wearing, so it cannot be deleted.");
	}
	if (m_library->remove(name, true) || m_library->remove(name, false)) {
		return QString();
	}
	return tr("The skin's file could not be deleted.");
}

void SkinsController::setSlim(const QString& name, bool slim)
{
	SkinEntry* entry = m_library->entry(name);
	if (!entry) {
		return;
	}
	entry->setArms(slim ? SkinEntry::Arms::Slim : SkinEntry::Arms::Classic);
	m_library->save();
	emit skinsChanged();
}

void SkinsController::openFolder()
{
	DesktopServices::openDirectory(m_library->directory(), true);
}

void SkinsController::markWorn(const QString& name)
{
	SkinEntry* entry = m_library->entry(name);
	if (!entry || !m_account || !m_account->accountData()) {
		return;
	}
	entry->setTextureUrl(m_account->accountData()->minecraftProfile.skin.url);
	m_library->save();
	emit skinsChanged();
}
