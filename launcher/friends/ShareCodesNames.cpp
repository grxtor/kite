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

/* ShareCodesController: the names of what a looked-up code holds, for its
 * preview. Only the first batch of each platform is asked for; a name that does
 * not come back stays the project's id. */

#include "ShareCodesController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ShareContent.h"
#include "core/LauncherContext.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/JsonPost.h"

namespace
{
	QList<ShareCodes::Entry> firstBatch(const QList<ShareCodes::Entry>& content,
										const QString& platform)
	{
		const auto batches = ShareContent::batches(content, platform);
		return batches.isEmpty() ? QList<ShareCodes::Entry>() : batches.first();
	}
} // namespace

void ShareCodesController::lookUpNames(const ShareCodes::Code& code)
{
	const QString shown = ShareCodes::display(code.code);
	const QList<ShareCodes::Entry> content = code.manifest.content;
	const auto curseForge = firstBatch(content, ShareCodes::Platform::CurseForge);
	/* Then CurseForge, then the list: each step reads the answer before the next asks. */
	auto askCurseForge = [this, shown, content, curseForge](const QJsonArray& modrinth) {
		if (curseForge.isEmpty()) {
			emit contentNamed(shown, ShareContent::contentNames(content, modrinth, {}));
			return;
		}
		m_namesPost.reset(new Net::JsonPost(
			tr("Share code content"), FlameApi::projectsUrl(),
			ShareContent::curseForgeIdsBody("modIds", ShareContent::projectIds(curseForge))));
		Net::JsonPost* post = m_namesPost.get();
		connect(post, &Task::finished, this, [this, post, shown, content, modrinth]() {
			const QJsonArray mods = post->wasSuccessful()
										? QJsonDocument::fromJson(post->response())
											  .object()
											  .value("data")
											  .toArray()
										: QJsonArray();
			emit contentNamed(shown, ShareContent::contentNames(content, modrinth, mods));
		});
		post->start();
	};
	const auto modrinth = firstBatch(content, ShareCodes::Platform::Modrinth);
	if (modrinth.isEmpty() || !LAUNCHER) {
		askCurseForge({});
		return;
	}
	m_namesBuffer.clear();
	m_namesJob = NetJob::Ptr(new NetJob(tr("Share code content"), LAUNCHER->network()));
	m_namesJob->addNetAction(Net::Download::makeByteArray(
		ModrinthApi::projectsUrl(ShareContent::projectIds(modrinth)), &m_namesBuffer));
	NetJob* job = m_namesJob.get();
	connect(job, &Task::finished, this, [this, job, askCurseForge]() {
		askCurseForge(job->wasSuccessful() ? QJsonDocument::fromJson(m_namesBuffer).array()
										   : QJsonArray());
	});
	job->start();
}
