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

#include "ReleaseDownload.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "BuildConfig.h"

ReleaseDownload::ReleaseDownload(QNetworkAccessManager* network,
								 const QUrl& url, const QString& path,
								 QObject* parent)
	: Task(parent), m_network(network), m_url(url), m_path(path),
	  m_partPath(path + QLatin1String(".part"))
{
}

ReleaseDownload::~ReleaseDownload()
{
	cleanUp();
}

void ReleaseDownload::executeTask()
{
	setStatus(tr("Downloading %1").arg(m_url.fileName()));

	if (!m_network) {
		failWith(tr("No network access is configured."));
		return;
	}

	if (!QDir().mkpath(QFileInfo(m_partPath).absolutePath())) {
		failWith(tr("Could not create %1.")
					 .arg(QDir::toNativeSeparators(
						 QFileInfo(m_partPath).absolutePath())));
		return;
	}

	// A leftover part file is not resumed: appending to a stale prefix without a kept ETag would
	// produce a corrupt archive.
	QFile::remove(m_partPath);

	m_file = new QFile(m_partPath, this);
	if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		failWith(tr("Could not write %1: %2")
					 .arg(QDir::toNativeSeparators(m_partPath),
						  m_file->errorString()));
		return;
	}

	QNetworkRequest request(m_url);
	request.setHeader(QNetworkRequest::UserAgentHeader,
					  BuildConfig.USER_AGENT_UNCACHED);
	// Release assets sit behind a redirect to the object store; following it is mandatory.
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
						 QNetworkRequest::NoLessSafeRedirectPolicy);

	qDebug() << "Downloading" << m_url.toString() << "to" << m_partPath;

	m_reply = m_network->get(request);
	connect(m_reply, &QNetworkReply::readyRead, this,
			&ReleaseDownload::onReadyRead);
	connect(m_reply, &QNetworkReply::downloadProgress, this,
			&ReleaseDownload::onDownloadProgress);
	connect(m_reply, &QNetworkReply::finished, this,
			&ReleaseDownload::onFinished);

	reportAbortStatus();
}

void ReleaseDownload::onReadyRead()
{
	if (!m_reply || !m_file)
		return;

	const QByteArray chunk = m_reply->readAll();
	if (chunk.isEmpty())
		return;

	if (m_file->write(chunk) != chunk.size()) {
		// Out of disk, most likely. Stop: continuing would produce an archive that unpacks into a
		// broken installation.
		const QString reason = m_file->errorString();
		m_reply->abort();
		failWith(tr("Could not write %1: %2")
					 .arg(QDir::toNativeSeparators(m_partPath), reason));
	}
}

void ReleaseDownload::onDownloadProgress(qint64 received, qint64 total)
{
	m_received = received;
	setProgress(received, total);

	if (total > 0) {
		setDetails(tr("%1 of %2")
					   .arg(QLocale().formattedDataSize(received),
							QLocale().formattedDataSize(total)));
	} else {
		setDetails(QLocale().formattedDataSize(received));
	}
}

void ReleaseDownload::onFinished()
{
	if (m_aborted)
		return;

	if (!m_reply) {
		failWith(tr("The download ended unexpectedly."));
		return;
	}

	if (m_reply->error() != QNetworkReply::NoError) {
		failWith(m_reply->errorString());
		return;
	}

	// Anything still buffered in the reply.
	onReadyRead();

	if (!m_file || !m_file->flush()) {
		failWith(tr("Could not write %1: %2")
					 .arg(QDir::toNativeSeparators(m_partPath),
						  m_file ? m_file->errorString() : QString()));
		return;
	}

	const qint64 downloadedSize = m_file->size();
	m_file->close();

	if (m_expectedSize > 0 && downloadedSize != m_expectedSize) {
		// The listing gave the size; a mismatch means we did not get the file, whatever the status
		// code said.
		failWith(tr("The download is %1, but %2 was expected.")
					 .arg(QLocale().formattedDataSize(downloadedSize),
						  QLocale().formattedDataSize(m_expectedSize)));
		return;
	}

	// The real name only now, so a half-finished download is never mistaken for a complete
	// artifact.
	QFile::remove(m_path);
	if (!QFile::rename(m_partPath, m_path)) {
		failWith(tr("Could not move the download into place at %1.")
					 .arg(QDir::toNativeSeparators(m_path)));
		return;
	}

	qDebug() << "Downloaded" << downloadedSize << "bytes to" << m_path;

	cleanUp();
	emitSucceeded();
}

bool ReleaseDownload::abort()
{
	m_aborted = true;
	if (m_reply)
		m_reply->abort();
	cleanUp();
	QFile::remove(m_partPath);
	emitAborted();
	return true;
}

void ReleaseDownload::cleanUp()
{
	if (m_reply) {
		m_reply->disconnect(this);
		m_reply->deleteLater();
		m_reply = nullptr;
	}
	if (m_file) {
		if (m_file->isOpen())
			m_file->close();
		m_file->deleteLater();
		m_file = nullptr;
	}
}

void ReleaseDownload::failWith(const QString& reason)
{
	cleanUp();
	// The part file is the only trace of a failed attempt; keeping it would confuse the next one.
	QFile::remove(m_partPath);
	emitFailed(reason);
}
