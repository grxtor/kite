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
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "minecraft/mod/ModMetadataIndex.h"
#include "net/NetJob.h"

namespace Net
{
	class JsonPost;
}

/*
 * What a platform knows about an installed project that the file itself may
 * not say: its logo, its one-line summary and the readable version number.
 * The list shows these for files without metadata of their own (shader packs,
 * most resource packs), so a sidecar that lacks them (files installed before
 * the sidecar recorded them, or by another tool) is filled in from the
 * platform: one batched request per kind of lookup and platform, the answer
 * written into the sidecars.
 */
namespace ContentInfo
{
	struct Project {
		QString name;
		QString slug;
		QString iconUrl;
		QString description;
	};

	/// A Modrinth project (title/description/icon_url) or a CurseForge mod
	/// (name/summary/logo.thumbnailUrl).
	Project projectFrom(const QJsonObject& object);
	/// "id" as a string, whether the platform sends a string or a number.
	QString idOf(const QJsonObject& object);

	/// id -> project, from Modrinth's array or CurseForge's {"data": [...]}.
	QHash<QString, Project> parseProjects(const QByteArray& body);
	/// Version / file id -> the version a person reads: Modrinth's
	/// version_number, CurseForge's displayName.
	QHash<QString, QString> parseVersionNumbers(const QByteArray& body);

	/// The version number is missing (and a version id says which to ask
	/// for), or neither logo nor summary is known. Only Modrinth and
	/// CurseForge entries.
	bool needsInfo(const ModMetadataIndex::Entry& entry);
	/// True the first time @p what is asked about this session, false after:
	/// what a run could not get is not asked for again until the next launch.
	/// GUI thread only.
	bool firstAskThisSession(const QString& what);
	/// Fills what @p entry lacks from the answers; what it has stays. True if
	/// anything changed.
	bool fill(ModMetadataIndex::Entry& entry,
			  const QHash<QString, Project>& projects,
			  const QHash<QString, QString>& versionNumbers);
} // namespace ContentInfo

/*
 * One run of that lookup for one folder's index. Asks once per file and
 * session (a failed or offline attempt is retried on the next launch, never
 * sooner), is silent on every error, and deletes itself when done. `filled`
 * names the files whose sidecars changed.
 */
class ContentInfoBackfill : public QObject
{
	Q_OBJECT
  public:
	ContentInfoBackfill(std::shared_ptr<ModMetadataIndex> index,
						QObject* parent = nullptr);
	~ContentInfoBackfill() override;

	/// Asks for what the index's entries lack; does nothing (and deletes
	/// itself) if none lacks anything or there is no network.
	void start();

  signals:
	void filled(const QStringList& keys);

  private:
	void startModrinth(const QStringList& projects, const QStringList& versions);
	void startCurseForge(const QStringList& projects, const QStringList& files);
	void onePartDone();
	void apply();

	std::shared_ptr<ModMetadataIndex> m_index;
	QList<ModMetadataIndex::Entry> m_wanted;
	/// platform -> answers
	QHash<QString, QHash<QString, ContentInfo::Project>> m_projects;
	QHash<QString, QHash<QString, QString>> m_versions;
	int m_pending = 0;

	NetJob::Ptr m_modrinthJob;
	/// Sized before the job is built: the downloads write through pointers.
	QList<QByteArray> m_modrinthBodies;
	int m_modrinthProjectParts = 0;
	std::unique_ptr<Net::JsonPost> m_cfProjects;
	std::unique_ptr<Net::JsonPost> m_cfFiles;
};
