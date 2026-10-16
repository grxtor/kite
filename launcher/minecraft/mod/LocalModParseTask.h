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
#include <functional>

#include <QRunnable>
#include <QDebug>
#include <QObject>
#include "Mod.h"
#include "ModDetails.h"

class LocalModParseTask : public QObject, public QRunnable
{
	Q_OBJECT
  public:
	struct Result {
		QString id;
		std::shared_ptr<ModDetails> details;
		/// ModIconCache key of this file's logo once it is in the cache;
		/// empty when it has none (or no cache directory was given).
		QString iconKey;
	};
	using ResultPtr = std::shared_ptr<Result>;
	ResultPtr result() const
	{
		return m_result;
	}

	/// @p iconCacheDir is where the file's logo is cached (see
	/// ModIconCache); empty skips looking for one.
	LocalModParseTask(int token, Mod::ModType type, const QFileInfo& modFile,
					  const QString& iconCacheDir = QString());
	void run();

  signals:
	void finished(int token);

  private:
	void processAsZip();
	void processAsFolder();
	void processAsLitemod();
	/// @p read returns the bytes of a file inside the mod, by its
	/// archive-relative path (empty if there is none).
	void cacheIcon(const std::function<QByteArray(const QString&)>& read);

  private:
	int m_token;
	Mod::ModType m_type;
	QFileInfo m_modFile;
	ResultPtr m_result;
	QString m_iconCacheDir;
};
