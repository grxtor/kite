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

#include "ScreenshotUploader.h"

#include <QFileInfo>

#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "net/NetJob.h"
#include "screenshots/ImgurAlbumCreation.h"
#include "screenshots/ImgurUpload.h"
#include "tasks/SequentialTask.h"

ScreenshotUploader::ScreenshotUploader(QObject* parent) : QObject(parent) {}

ScreenshotUploader::~ScreenshotUploader()
{
	if (m_task) {
		m_task->disconnect(this);
	}
}

bool ScreenshotUploader::available() const
{
	return !BuildConfig.IMGUR_CLIENT_ID.trimmed().isEmpty();
}

QString ScreenshotUploader::albumUrl(const QString& albumId)
{
	return QStringLiteral("https://imgur.com/a/%1").arg(albumId);
}

QString ScreenshotUploader::validate(const QStringList& paths)
{
	if (paths.isEmpty()) {
		return tr("Pick a screenshot to upload first.");
	}
	for (const QString& path : paths) {
		const QFileInfo info(path);
		if (!info.isFile() || !info.isReadable()) {
			return tr("“%1” could not be read.").arg(info.fileName());
		}
	}
	return QString();
}

void ScreenshotUploader::upload(const QStringList& paths)
{
	if (m_task) {
		return;
	}
	const QString problem = validate(paths);
	if (!problem.isEmpty()) {
		emit failed(problem);
		return;
	}
	auto* launcher = LauncherContext::instance();
	if (!available() || !launcher || !launcher->network()) {
		emit failed(tr("Uploading to Imgur is not available in this build."));
		return;
	}

	QList<ScreenShot::Ptr> shots;
	auto uploads = NetJob::Ptr(new NetJob(QStringLiteral("Screenshot Upload"),
										  launcher->network()));
	for (const QString& path : paths) {
		auto shot = std::make_shared<ScreenShot>(QFileInfo(path));
		shots.append(shot);
		uploads->addNetAction(ImgurUpload::make(shot));
	}

	const int count = shots.size();
	Task::Ptr task;
	std::function<QString()> link;
	if (count == 1) {
		task = uploads;
		auto shot = shots.first();
		link = [shot] { return shot->m_url; };
	} else {
		auto album = ImgurAlbumCreation::make(shots);
		auto albumJob = NetJob::Ptr(new NetJob(
			QStringLiteral("Imgur Album Creation"), launcher->network()));
		albumJob->addNetAction(album);
		auto* sequence = new SequentialTask(nullptr, QStringLiteral("Screenshot Upload"));
		sequence->addTask(uploads);
		sequence->addTask(albumJob);
		task = Task::Ptr(sequence);
		link = [album] { return albumUrl(album->id()); };
	}

	m_task = task;
	connect(task.get(), &Task::succeeded, this, [this, link, count] {
		finish(true, link(), count);
	});
	connect(task.get(), &Task::failed, this,
			[this](const QString& reason) { finish(false, reason, 0); });
	emit busyChanged();
	task->start();
}

void ScreenshotUploader::finish(bool ok, const QString& urlOrError, int count)
{
	// Released after this slot returns - the task is still emitting.
	Task::Ptr done = std::move(m_task);
	m_task.reset();
	emit busyChanged();
	if (ok && !urlOrError.isEmpty()) {
		emit uploaded(urlOrError, count);
	} else if (ok) {
		emit failed(tr("Imgur did not return a link. Try again in a moment."));
	} else {
		emit failed(urlOrError.isEmpty()
						? tr("The upload failed. Check your connection and try again.")
						: tr("The upload failed: %1").arg(urlOrError));
	}
}
