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

#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>

#include "BaseInstance.h"
#include "modplatform/PlatformCover.h"

// Fetches a modpack's own picture into its instance folder in the background; the caller applies finished().
class PlatformCoverFetcher : public QObject
{
	Q_OBJECT

  public:
	enum class Outcome {
		Found, ///< fileName is in the instance folder
		NoArt, ///< asked; the platform has none (or none usable)
		Failed ///< network or answer trouble, or nothing to ask; not recorded
	};

	explicit PlatformCoverFetcher(QObject* parent = nullptr);

	// Looks for @p inst's pack art unless tried this run or none was found within a week. GUI thread.
	void request(const InstancePtr& inst);

  signals:
	void finished(const QString& instanceId,
				  PlatformCoverFetcher::Outcome outcome,
				  const QString& fileName);

  private:
	struct Run;
	using RunPtr = std::shared_ptr<Run>;

	void pump();
	void begin(const RunPtr& run);
	void next(const RunPtr& run);
	void search(const RunPtr& run, const PlatformCover::Lookup& lookup);
	void fetchProject(const RunPtr& run, PlatformCover::Provider provider,
					  const QString& id);
	void downloadNext(const RunPtr& run);
	void conclude(const RunPtr& run, Outcome outcome,
				  const QString& fileName = QString());
	/* @p done gets the reply, or null when the request failed. */
	void get(const RunPtr& run, const QUrl& url,
			 std::function<void(const QByteArray*)> done);

	QSet<QString> m_tried;
	QQueue<RunPtr> m_queue;
	int m_running = 0;
	bool m_loggedNoKey = false;
};
