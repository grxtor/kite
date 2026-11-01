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

#include <QCryptographicHash>
#include <QString>
#include <QUrl>

#include "tasks/Task.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class ReleaseDownload : public Task
{
	Q_OBJECT

  public:
	ReleaseDownload(QNetworkAccessManager* network, const QUrl& url,
					const QString& path, QObject* parent = nullptr);
	~ReleaseDownload() override;

	/*! Size the release listing promised, in bytes; 0 means unknown. Checked after the transfer,
	 * since a proxy or CDN can return a truncated body with a good status code. */
	void setExpectedSize(qint64 bytes)
	{
		m_expectedSize = bytes;
	}

	/*! Lowercase hex SHA-256 the file must have; empty means unchecked. Compared after the
	 * transfer, before the file takes its real name. */
	void setExpectedSha256(const QString& hex)
	{
		m_expectedSha256 = hex.toLower();
	}

	bool canAbort() const override
	{
		return true;
	}

	QString filePath() const
	{
		return m_path;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private slots:
	void onReadyRead();
	void onDownloadProgress(qint64 received, qint64 total);
	void onFinished();

  private:
	//! Close the reply and the part file, keeping neither.
	void cleanUp();

	//! Report a failure and leave nothing half-written behind.
	void failWith(const QString& reason);

	QNetworkAccessManager* m_network;
	QUrl m_url;
	QString m_path;
	QString m_partPath;

	QNetworkReply* m_reply = nullptr;
	QFile* m_file = nullptr;

	qint64 m_expectedSize = 0;
	QString m_expectedSha256;
	QCryptographicHash m_hash{QCryptographicHash::Sha256};
	qint64 m_received = 0;
	bool m_aborted = false;
};
