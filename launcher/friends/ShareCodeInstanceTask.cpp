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

#include "ShareCodeInstanceTask.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentDownloadTask.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/JsonPost.h"

namespace
{
	/* Modrinth answers with an array; CurseForge with {"data": [...]}. */
	QJsonArray arrayOf(const QByteArray& body)
	{
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		return doc.isArray() ? doc.array()
							 : doc.object().value("data").toArray();
	}

	QString folderFor(MinecraftInstance& instance, const QString& kind)
	{
		if (kind == ShareCodes::Kind::Mod) {
			return instance.modsRoot();
		}
		if (kind == ShareCodes::Kind::ResourcePack) {
			return instance.resourcePacksDir();
		}
		if (kind == ShareCodes::Kind::ShaderPack) {
			return instance.shaderPacksDir();
		}
		return instance.dataPacksDir();
	}
} // namespace

ShareCodeInstanceTask::ShareCodeInstanceTask(BaseVersionPtr version,
											 const QString& loaderUid,
											 const QString& loaderVersion,
											 ShareCodes::Manifest manifest)
	: InstanceCreationTask(std::move(version), loaderUid, loaderVersion),
	  m_manifest(std::move(manifest))
{
}

ShareCodeInstanceTask::~ShareCodeInstanceTask() = default;

QStringList ShareCodeInstanceTask::missing() const
{
	return m_resolved.missingNames + m_failedFiles;
}

void ShareCodeInstanceTask::get(const QUrl& url, Answer done)
{
	m_buffer.clear();
	m_job = NetJob::Ptr(new NetJob(tr("Share code content"), LAUNCHER->network()));
	m_job->addNetAction(Net::Download::makeByteArray(url, &m_buffer));
	connect(m_job.get(), &NetJob::succeeded, this,
			[this, done]() { done(m_buffer); });
	connect(m_job.get(), &NetJob::failed, this, [done, url](QString reason) {
		qWarning() << "Share code: lookup failed" << url.host() << reason;
		done(QByteArray());
	});
	m_job->start();
}

void ShareCodeInstanceTask::post(const QUrl& url, const QByteArray& body,
								 Answer done)
{
	m_post = std::make_unique<Net::JsonPost>(tr("Share code content"), url, body);
	Net::JsonPost* request = m_post.get();
	connect(request, &Task::succeeded, this,
			[request, done]() { done(request->response()); });
	connect(request, &Task::failed, this, [done, url](QString reason) {
		qWarning() << "Share code: lookup failed" << url.host() << reason;
		done(QByteArray());
	});
	request->start();
}

void ShareCodeInstanceTask::executeTask()
{
	using ShareCodes::Platform::CurseForge;
	using ShareCodes::Platform::Modrinth;
	setStatus(tr("Looking up the shared content"));
	for (const auto& batch : ShareContent::batches(m_manifest.content, Modrinth)) {
		m_steps.append([this, batch]() {
			get(ModrinthApi::versionsUrl(ShareContent::fileIds(batch)),
				[this, batch](const QByteArray& versions) {
					const QJsonArray found = arrayOf(versions);
					get(ModrinthApi::projectsUrl(ShareContent::projectIds(batch)),
						[this, batch, found](const QByteArray& projects) {
							ShareContent::addModrinth(m_resolved, batch, found,
													  arrayOf(projects));
							resolveNext();
						});
				});
		});
	}
	for (const auto& batch : ShareContent::batches(m_manifest.content, CurseForge)) {
		m_steps.append([this, batch]() {
			post(FlameApi::filesUrl(),
				 ShareContent::curseForgeIdsBody("fileIds", ShareContent::fileIds(batch)),
				 [this, batch](const QByteArray& files) {
					 const QJsonArray found = arrayOf(files);
					 post(FlameApi::projectsUrl(),
						  ShareContent::curseForgeIdsBody(
							  "modIds", ShareContent::projectIds(batch)),
						  [this, batch, found](const QByteArray& mods) {
							  ShareContent::addCurseForge(m_resolved, batch, found,
														  arrayOf(mods));
							  resolveNext();
						  });
				 });
		});
	}
	resolveNext();
}

void ShareCodeInstanceTask::resolveNext()
{
	if (!m_steps.isEmpty()) {
		const auto step = m_steps.takeFirst();
		step();
		return;
	}
	InstanceCreationTask::executeTask();
}

void ShareCodeInstanceTask::instanceBuilt(
	std::shared_ptr<MinecraftInstance> instance)
{
	m_instance = std::move(instance);
	m_kindsLeft = {ShareCodes::Kind::Mod, ShareCodes::Kind::ResourcePack,
				   ShareCodes::Kind::ShaderPack, ShareCodes::Kind::DataPack};
	installNext();
}

void ShareCodeInstanceTask::installNext()
{
	while (!m_kindsLeft.isEmpty()) {
		const QString kind = m_kindsLeft.takeFirst();
		QList<ModPlatform::DownloadItem> items;
		for (const auto& item : m_resolved.items) {
			if (item.kind == kind) {
				items.append(item.download);
			}
		}
		if (items.isEmpty()) {
			continue;
		}
		const QString folder = folderFor(*m_instance, kind);
		auto* download = new ContentDownloadTask(items, folder);
		download->setMetadataIndex(std::make_shared<ModMetadataIndex>(QDir(folder)));
		m_download.reset(download);
		setStatus(tr("Downloading the shared content"));
		connect(download, &Task::progress, this,
				[this](qint64 current, qint64 total) { setProgress(current, total); });
		connect(download, &Task::finished, this, [this, download, items, folder]() {
			if (!download->wasSuccessful()) {
				/* What did land stays; the rest is named, not fatal. */
				logWarning(download->failReason());
				for (const auto& item : items) {
					if (!QFile::exists(QDir(folder).filePath(item.fileName))) {
						m_failedFiles.append(item.name);
					}
				}
			}
			installNext();
		});
		download->start();
		return;
	}
	downloadFiles(std::move(m_instance));
}
