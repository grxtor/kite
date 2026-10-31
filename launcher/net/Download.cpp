/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "Download.h"

#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

#include "FileSystem.h"
#include "Logging.h"
#include "ChecksumValidator.h"
#include "MetaCacheSink.h"
#include "ByteArraySink.h"

#include "BuildConfig.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace Net
{

	QByteArray ModrinthDownloadMeta::toJson() const
	{
		QJsonObject obj;
		obj[QStringLiteral("reason")] = reason;
		/* Omitted rather than sent empty: an empty header would be a made-up
		 * claim. */
		if (!gameVersion.isEmpty()) {
			obj[QStringLiteral("game_version")] = gameVersion;
		}
		if (!loader.isEmpty()) {
			obj[QStringLiteral("loader")] = loader;
		}
		if (!dependentOn.isEmpty()) {
			obj[QStringLiteral("dependent_on")] = dependentOn;
		}
		return QJsonDocument(obj).toJson(QJsonDocument::Compact);
	}

	Download::Download() : NetAction()
	{
		m_status = Job_NotStarted;
	}

	Download::Ptr Download::makeCached(QUrl url, MetaEntryPtr entry,
									   Options options)
	{
		Download* dl = new Download();
		dl->m_url = url;
		dl->m_options = options;
		auto md5Node = new ChecksumValidator(QCryptographicHash::Md5);
		auto cachedNode = new MetaCacheSink(entry, md5Node);
		dl->m_sink.reset(cachedNode);
		dl->m_target_path = entry->getFullPath();
		return dl;
	}

	Download::Ptr Download::makeByteArray(QUrl url, QByteArray* output,
										  Options options)
	{
		Download* dl = new Download();
		dl->m_url = url;
		dl->m_options = options;
		dl->m_sink.reset(new ByteArraySink(output));
		return dl;
	}

	Download::Ptr Download::makeFile(QUrl url, QString path, Options options)
	{
		Download* dl = new Download();
		dl->m_url = url;
		dl->m_options = options;
		dl->m_sink.reset(new FileSink(path));
		return dl;
	}

	void Download::addValidator(Validator* v)
	{
		m_sink->addValidator(v);
	}

	void Download::startImpl()
	{
		if (m_status == Job_Aborted) {
			qCWarning(netLog) << "Attempt to start an aborted Download:"
							  << m_url.toString();
			emit aborted(m_index_within_job);
			return;
		}
		QNetworkRequest request(m_url);
		m_status = m_sink->init(request);
		switch (m_status) {
			case Job_Finished:
				emit succeeded(m_index_within_job);
				qCDebug(netLog) << "Download cache hit" << m_url.toString();
				return;
			case Job_InProgress:
				qCDebug(netLog) << "Downloading" << m_url.toString();
				break;
			case Job_Failed_Proceed: // this is meaningless in this context. We
			case Job_NotStarted:
			case Job_Failed:
				emit failed(m_index_within_job);
				return;
			case Job_Aborted:
				return;
		}

		request.setHeader(QNetworkRequest::UserAgentHeader,
						  BuildConfig.USER_AGENT);

		if (!BuildConfig.CURSEFORGE_API_KEY.isEmpty() &&
			m_url.host() == FlameApi::apiHost()) {
			request.setRawHeader("x-api-key",
								 BuildConfig.CURSEFORGE_API_KEY.toUtf8());
			request.setRawHeader("Accept", "application/json");
		}

		/* Attribution for the file, when the caller said what it is for and the
		 * host understands it. */
		if (!m_modrinthMeta.isEmpty() &&
			m_url.host() == ModrinthApi::cdnHost()) {
			request.setRawHeader("modrinth-download-meta",
								 m_modrinthMeta.toJson());
		}

		QNetworkReply* rep = m_network->get(request);

		m_reply.reset(rep);
		connect(rep, &QNetworkReply::downloadProgress, this,
				&Download::downloadProgress);
		connect(rep, &QNetworkReply::finished, this,
				&Download::downloadFinished);
		connect(rep, &QNetworkReply::errorOccurred, this,
				&Download::downloadError);
		connect(rep, &QNetworkReply::sslErrors, this, &Download::sslErrors);
		connect(rep, &QNetworkReply::readyRead, this,
				&Download::downloadReadyRead);
	}

	void Download::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
	{
		m_total_progress = bytesTotal;
		m_progress = bytesReceived;
		emit netActionProgress(m_index_within_job, bytesReceived, bytesTotal);
	}

	void Download::downloadError(QNetworkReply::NetworkError error)
	{
		if (error == QNetworkReply::OperationCanceledError) {
			qCCritical(netLog) << "Aborted" << m_url.toString();
			m_status = Job_Aborted;
		} else {
			if (m_options & Option::AcceptLocalFiles) {
				if (m_sink->hasLocalData()) {
					m_status = Job_Failed_Proceed;
					return;
				}
			}
			qCCritical(netLog) << "Failed" << m_url.toString()
							   << "with reason" << error;
			m_status = Job_Failed;
		}
	}

	void Download::sslErrors(const QList<QSslError>& errors)
	{
		int i = 1;
		for (auto error : errors) {
			qCCritical(netLog) << "Download" << m_url.toString()
							   << "SSL error #" << i << ":"
							   << error.errorString();
			auto cert = error.certificate();
			qCCritical(netLog) << "Certificate in question:\n"
							   << cert.toText();
			i++;
		}
	}

	bool Download::handleRedirect()
	{
		QUrl redirect =
			m_reply->header(QNetworkRequest::LocationHeader).toUrl();
		if (!redirect.isValid()) {
			if (!m_reply->hasRawHeader("Location")) {
				return false;
			}
			// A Location header that is not valid needs workarounds.
			QByteArray redirectBA = m_reply->rawHeader("Location");
			if (redirectBA.size() == 0) {
				return false;
			}
			QString redirectStr = QString::fromUtf8(redirectBA);

			if (redirectStr.startsWith("//")) {
				// URL begins with //: insert the scheme (QTBUG-41061, RFC 3986 4.2).
				redirectStr = m_reply->url().scheme() + ":" + redirectStr;
			} else if (redirectStr.startsWith("/")) {
				// URL begins with /: process as a relative URL.
				auto url = m_reply->url();
				url.setPath(redirectStr, QUrl::TolerantMode);
				redirectStr = url.toString();
			}

			// Parse in tolerant mode; Qt does not for the Location header.
			// FIXME: report Qt bug
			redirect = QUrl(redirectStr, QUrl::TolerantMode);
			if (!redirect.isValid()) {
				qCWarning(netLog)
					<< "Failed to parse redirect URL:" << redirectStr;
				downloadError(QNetworkReply::ProtocolFailure);
				return false;
			}
			qCDebug(netLog) << "Fixed location header:" << redirect;
		} else {
			qCDebug(netLog) << "Location header:" << redirect;
		}

		m_url = QUrl(redirect.toString());
		qCDebug(netLog) << "Following redirect to" << m_url.toString();
		start(m_network);
		return true;
	}

	void Download::downloadFinished()
	{
		if (handleRedirect()) {
			qCDebug(netLog) << "Download redirected:" << m_url.toString();
			return;
		}

		if (m_status == Job_Failed_Proceed) {
			qCDebug(netLog) << "Download failed but we are allowed to proceed:"
							<< m_url.toString();
			m_sink->abort();
			m_reply.reset();
			emit succeeded(m_index_within_job);
			return;
		} else if (m_status == Job_Failed) {
			qCDebug(netLog)
				<< "Download failed in previous step:" << m_url.toString();
			m_sink->abort();
			m_reply.reset();
			emit failed(m_index_within_job);
			return;
		} else if (m_status == Job_Aborted) {
			qCDebug(netLog) << "Download aborted in previous step:"
							<< m_url.toString();
			m_sink->abort();
			m_reply.reset();
			emit aborted(m_index_within_job);
			return;
		}

		auto data = m_reply->readAll();
		if (data.size()) {
			qCDebug(netLog) << "Writing extra" << data.size() << "bytes to"
							<< m_target_path;
			m_status = m_sink->write(data);
		}

		m_status = m_sink->finalize(*m_reply.get());
		if (m_status != Job_Finished) {
			qCDebug(netLog)
				<< "Download failed to finalize:" << m_url.toString();
			m_sink->abort();
			m_reply.reset();
			emit failed(m_index_within_job);
			return;
		}
		m_reply.reset();
		qCDebug(netLog) << "Download succeeded:" << m_url.toString();
		emit succeeded(m_index_within_job);
	}

	void Download::downloadReadyRead()
	{
		if (m_status == Job_InProgress) {
			auto data = m_reply->readAll();
			m_status = m_sink->write(data);
			if (m_status == Job_Failed) {
				qCCritical(netLog) << "Failed to process response chunk for"
								   << m_target_path;
			}
		} else {
			qCCritical(netLog) << "Cannot write to" << m_target_path
							   << "- illegal status" << m_status;
		}
	}

} // namespace Net

bool Net::Download::abort()
{
	if (m_reply) {
		m_reply->abort();
	} else {
		m_status = Job_Aborted;
	}
	return true;
}

bool Net::Download::canAbort()
{
	return true;
}
