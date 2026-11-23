/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
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
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "net/NetJob.h"

class ModFolderModel;

/*
 * Downloads, once, the platform logo of each file in a folder that has none
 * of its own (read from the archive, so only resolved files count) but whose
 * sidecar names one, into ModIconCache under keyForUrl(). Once there, the list
 * reads it from disk and this never asks for it again. One NetJob for the
 * folder, one attempt per URL per session, silent on errors; deletes itself.
 * `stored` names the files whose logo is now on disk.
 */
class ContentLogoFetch : public QObject
{
	Q_OBJECT
  public:
	ContentLogoFetch(std::shared_ptr<ModFolderModel> folder,
					 QObject* parent = nullptr);
	~ContentLogoFetch() override;

	void start();

  signals:
	void stored(const QStringList& keys);

  private:
	void finish();

	std::shared_ptr<ModFolderModel> m_folder;
	NetJob::Ptr m_job;
	QStringList m_urls;
	/// Sized before the job is built: the downloads write through pointers.
	QList<QByteArray> m_bodies;
	/// URL -> keys of the files showing it (two files of one project share it).
	QHash<QString, QStringList> m_keys;
};
