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

#include "PlatformCoverFetcher.h"

#include <QDateTime>
#include <QDebug>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QThreadPool>
#include <QtConcurrentRun>

#include "BuildConfig.h"
#include "Json.h"
#include "core/LauncherContext.h"
#include "net/Download.h"
#include "net/NetJob.h"

namespace
{
// At most this many instances look their art up at once.
constexpr int kMaxRunning = 2;
constexpr qint64 kMaxImageBytes = 16 * 1024 * 1024;

// The reply as a JSON object, or empty when the request failed or it is not one.
QJsonObject parseObject(const QByteArray* reply)
{
	if (!reply) {
		return QJsonObject();
	}
	const auto document = Json::tryParse(*reply);
	return document ? document->object() : QJsonObject();
}

/* Saves @p bytes as the cover in @p root if they are a picture that suits a hero and the instance still exists,
 * replacing the previous cover; the file name, or empty. Reads and writes files: a worker runs it. */
QString saveCover(const QString& root, const QByteArray& bytes)
{
	if (bytes.isEmpty() || bytes.size() > kMaxImageBytes ||
		!QFileInfo::exists(QDir(root).filePath(QStringLiteral("instance.cfg")))) {
		return QString();
	}
	QBuffer buffer;
	buffer.setData(bytes);
	buffer.open(QIODevice::ReadOnly);
	QImageReader reader(&buffer);
	const QString format = QString::fromLatin1(reader.format()).toLower();
	if (format.isEmpty() || !PlatformCover::suitableAsCover(reader.size())) {
		return QString();
	}
	const QDir dir(root);
	const QString name =
		QStringLiteral("kite-cover.") +
		(format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format);
	const QString part = dir.filePath(QStringLiteral("kite-cover.part"));
	QFile file(part);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
		file.write(bytes) != bytes.size()) {
		file.close();
		QFile::remove(part);
		return QString();
	}
	file.close();
	for (const QString& old :
		 dir.entryList({QStringLiteral("kite-cover.*")}, QDir::Files)) {
		if (old != QLatin1String("kite-cover.part")) {
			QFile::remove(dir.filePath(old));
		}
	}
	if (!QFile::rename(part, dir.filePath(name))) {
		QFile::remove(part);
		return QString();
	}
	return name;
}
} // namespace

struct PlatformCoverFetcher::Run {
	QString id;
	QString root;
	PlatformCover::PackFacts facts;
	QList<PlatformCover::Lookup> lookups;
	int nextLookup = 0;
	QStringList images; ///< candidates of the lookup in hand, best first
	bool answered = false; ///< some lookup got a definite "nothing"
	bool skipped = false;  ///< a lookup was not made (no CurseForge key)
	bool failed = false;
	NetJob::Ptr job;
};

PlatformCoverFetcher::PlatformCoverFetcher(QObject* parent) : QObject(parent) {}

void PlatformCoverFetcher::request(const InstancePtr& inst)
{
	auto* context = LauncherContext::instance();
	if (!inst || !context || !context->network()) {
		return;
	}
	const QString id = inst->id();
	if (m_tried.contains(id) ||
		!PlatformCover::retryDue(inst->platformCoverCheckedAt(),
								 QDateTime::currentSecsSinceEpoch())) {
		return;
	}
	m_tried.insert(id);

	auto run = std::make_shared<Run>();
	run->id = id;
	run->root = inst->instanceRoot();
	PlatformCover::PackFacts& facts = run->facts;
	facts.packProvider = inst->managedPackProvider();
	facts.packId = inst->managedPackId();
	facts.packSlug = inst->managedPackSlug();
	facts.legacyProvider = inst->legacyPackProvider();
	facts.legacyId = inst->legacyPackId();
	facts.iconKey = inst->iconKey();
	facts.packTitles = {inst->legacyPackName(),
						inst->settings()->get("PackName").toString()};
	facts.instanceName = inst->name();

	m_queue.enqueue(run);
	pump();
}

void PlatformCoverFetcher::pump()
{
	while (m_running < kMaxRunning && !m_queue.isEmpty()) {
		++m_running;
		begin(m_queue.dequeue());
	}
}

void PlatformCoverFetcher::begin(const RunPtr& run)
{
	// The CurseForge manifest names the pack as the platform lists it, which
	// the instance and file names rarely do.
	auto* watcher = new QFutureWatcher<QString>(this);
	connect(watcher, &QFutureWatcher<QString>::finished, this,
			[this, run, watcher]() {
				const QString title = watcher->result();
				watcher->deleteLater();
				if (!title.isEmpty()) {
					run->facts.packTitles.prepend(title);
				}
				run->lookups = PlatformCover::lookupsFor(run->facts);
				next(run);
			});
	const QString root = run->root;
	watcher->setFuture(QtConcurrent::run(
		QThreadPool::globalInstance(),
		[root]() { return PlatformCover::flameManifestName(root); }));
}

void PlatformCoverFetcher::next(const RunPtr& run)
{
	while (run->nextLookup < run->lookups.size()) {
		const PlatformCover::Lookup lookup =
			run->lookups.at(run->nextLookup++);
		if (lookup.provider == PlatformCover::Provider::CurseForge &&
			BuildConfig.CURSEFORGE_API_KEY.isEmpty()) {
			if (!m_loggedNoKey) {
				m_loggedNoKey = true;
				qInfo() << "Pack covers: this build has no CurseForge API key, "
						   "so CurseForge art is skipped.";
			}
			run->skipped = true;
			continue;
		}
		if (lookup.byId()) {
			fetchProject(run, lookup.provider, lookup.projectId);
		} else {
			search(run, lookup);
		}
		return;
	}
	if (run->failed || run->skipped || !run->answered) {
		// Not everything was asked or answered: not recorded, so a later launch tries again.
		conclude(run, Outcome::Failed);
	} else {
		conclude(run, Outcome::NoArt);
	}
}

void PlatformCoverFetcher::search(const RunPtr& run,
								  const PlatformCover::Lookup& lookup)
{
	get(run, PlatformCover::searchUrl(lookup),
		[this, run, lookup](const QByteArray* reply) {
			const QJsonObject answer = parseObject(reply);
			if (answer.isEmpty()) {
				run->failed = true;
				conclude(run, Outcome::Failed);
				return;
			}
			const bool modrinth =
				lookup.provider == PlatformCover::Provider::Modrinth;
			const QJsonArray hits =
				answer.value(modrinth ? QStringLiteral("hits")
									: QStringLiteral("data"))
					.toArray();
			const QString id =
				modrinth ? PlatformCover::uniqueModrinthMatch(hits,
															  lookup.title)
						 : PlatformCover::uniqueFlameMatch(hits, lookup.title);
			if (id.isEmpty()) {
				run->answered = true;
				next(run);
				return;
			}
			fetchProject(run, lookup.provider, id);
		});
}

void PlatformCoverFetcher::fetchProject(const RunPtr& run,
										PlatformCover::Provider provider,
										const QString& id)
{
	get(run, PlatformCover::projectUrl(provider, id),
		[this, run, provider](const QByteArray* reply) {
			const QJsonObject project = parseObject(reply);
			if (project.isEmpty()) {
				run->failed = true;
				conclude(run, Outcome::Failed);
				return;
			}
			const QStringList formats = PlatformCover::decodableFormats();
			run->images =
				provider == PlatformCover::Provider::Modrinth
					? PlatformCover::modrinthImages(
						  project.value(QStringLiteral("gallery")).toArray(),
						  formats)
					: PlatformCover::flameImages(
						  project.value(QStringLiteral("data"))
							  .toObject()
							  .value(QStringLiteral("screenshots"))
							  .toArray(),
						  formats);
			downloadNext(run);
		});
}

void PlatformCoverFetcher::downloadNext(const RunPtr& run)
{
	if (run->images.isEmpty()) {
		// Nothing usable in this project: on to the next lookup.
		run->answered = true;
		next(run);
		return;
	}
	const QString url = run->images.takeFirst();
	get(run, QUrl(url), [this, run](const QByteArray* reply) {
		if (!reply) {
			run->failed = true;
			conclude(run, Outcome::Failed);
			return;
		}
		auto* watcher = new QFutureWatcher<QString>(this);
		connect(watcher, &QFutureWatcher<QString>::finished, this,
				[this, run, watcher]() {
					const QString name = watcher->result();
					watcher->deleteLater();
					if (name.isEmpty()) {
						// Too small or the wrong shape: the next picture.
						downloadNext(run);
					} else {
						conclude(run, Outcome::Found, name);
					}
				});
		const QString root = run->root;
		watcher->setFuture(QtConcurrent::run(
			QThreadPool::globalInstance(),
			[root, bytes = *reply]() { return saveCover(root, bytes); }));
	});
}

void PlatformCoverFetcher::get(const RunPtr& run, const QUrl& url,
							   std::function<void(const QByteArray*)> done)
{
	auto body = std::make_shared<QByteArray>();
	run->job.reset(new NetJob(QStringLiteral("PlatformCover(%1)").arg(run->id),
							  LauncherContext::instance()->network()));
	run->job->addNetAction(Net::Download::makeByteArray(url, body.get()));
	connect(run->job.get(), &NetJob::succeeded, this,
			[body, done]() { done(body.get()); });
	connect(run->job.get(), &NetJob::failed, this,
			[done](const QString&) { done(nullptr); });
	run->job->start();
}

void PlatformCoverFetcher::conclude(const RunPtr& run, Outcome outcome,
									const QString& fileName)
{
	// Drops the job, and with it the callbacks that hold this run.
	run->job.reset();
	--m_running;
	emit finished(run->id, outcome, fileName);
	pump();
}
