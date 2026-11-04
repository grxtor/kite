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

#pragma once

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>

#include "InstanceCreationTask.h"
#include "QObjectPtr.h"
#include "ShareCodes.h"
#include "ShareContent.h"
#include "net/NetJob.h"

namespace Net
{
	class JsonPost;
}

/*
 * A new instance from a share code: the code's Minecraft version and loader
 * (InstanceCreationTask), then each piece of content at the exact version
 * the code names, looked up on Modrinth or CurseForge and downloaded into the
 * staged instance before it is committed. Content no platform has any more
 * is left out and listed in missing(); the instance is made either way.
 */
class ShareCodeInstanceTask : public InstanceCreationTask
{
	Q_OBJECT

  public:
	ShareCodeInstanceTask(BaseVersionPtr version, const QString& loaderUid,
						  const QString& loaderVersion,
						  ShareCodes::Manifest manifest);
	~ShareCodeInstanceTask() override;

	/// The names (or ids, for content no platform knows) left out; set once it finished.
	QStringList missing() const;

  protected:
	void executeTask() override;
	void instanceBuilt(std::shared_ptr<MinecraftInstance> instance) override;

  private:
	using Answer = std::function<void(const QByteArray&)>;
	/// One request at a time; an answer that is not one is an empty body.
	void get(const QUrl& url, Answer done);
	void post(const QUrl& url, const QByteArray& body, Answer done);
	void resolveNext();
	void installNext();

	ShareCodes::Manifest m_manifest;
	QList<std::function<void()>> m_steps;
	ShareContent::Resolved m_resolved;
	std::shared_ptr<MinecraftInstance> m_instance;
	QStringList m_kindsLeft;
	QStringList m_failedFiles;

	QByteArray m_buffer;
	NetJob::Ptr m_job;
	/* Replaced from inside the previous request's own answer, so it goes with deleteLater(). */
	unique_qobject_ptr<Net::JsonPost> m_post;
	Task::Ptr m_download;
};
