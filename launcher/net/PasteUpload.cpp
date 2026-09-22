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

#include "PasteUpload.h"
#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "Logging.h"

#include <QDir>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

static QString applyFilters(QString logContent)
{
	QMap<QString, QString> filter;

	QString homePath = QDir::homePath();
	if (!homePath.isEmpty()) {
		filter[homePath] = "/home/<USERNAME>";
	}

	QString userName = qEnvironmentVariable("USER");
	if (userName.isEmpty()) {
		userName = qEnvironmentVariable("USERNAME");
	}

	if (!userName.isEmpty()) {
		filter["/" + userName + "/"] = "/<USERNAME>";
	}

	QList<QPair<QRegularExpression, QString>> regexFilters = {
		{QRegularExpression(
			 R"((?im)^.*(?:access_token|refresh_token|id_token|token|authorization|utoken|xsts|xbl|IssueInstant|NotAfter|DisplayClaims|xuid|uhs|xid|gtg|usr|utr|prv|ugc).*$)"),
		 "<CENSOR_AUTHLINE>"},
		{QRegularExpression(R"((?i)\bBearer\s+[A-Za-z0-9._~+/=-]{20,}\b)"),
		 "<CENSOR_BEARER>"},
		{QRegularExpression(
			 R"(\b[A-Za-z0-9_-]{10,}(?:\.[A-Za-z0-9_-]{10,}){2,4}\b)"),
		 "<CENSOR_JWT>"},
		{QRegularExpression(
			 R"(\beyJ[A-Za-z0-9_-]{10,}(?:\.[A-Za-z0-9_-]{10,}){2,5}\b)"),
		 "<CENSOR_JWT>"},
		{QRegularExpression(
			 R"((?i)\b(?:access_token|refresh_token|id_token|token)\b\s*[:=]\s*(?:\\?")?(?:[^"\\]|\\.){16,}(?:\\?")?)"),
		 "<CENSOR_JSONTOKEN>"},
		{QRegularExpression(
			 R"(\b[A-Za-z0-9_-]{16,}(?:\.[A-Za-z0-9_-]{8,}){2,6}\b)"),
		 "<CENSOR_TOKEN>"},
		{QRegularExpression(
			 R"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}\b)"),
		 "<CENSOR_UUID>"},
		{QRegularExpression(R"(\b[0-9a-fA-F]{32,64}\b)"), "<CENSOR_HEX>"},
		{QRegularExpression(R"(\b\d{10,20}\b)"), "<CENSOR_ID>"}};

	for (auto& pair : regexFilters) {
		auto it = pair.first.globalMatch(logContent);
		while (it.hasNext()) {
			auto m = it.next();
			QString matchedText = m.captured(0);
			if (matchedText.length() > 10) {
				filter[matchedText] = pair.second;
			}
		}
	}

	QStringList keys = filter.keys();
	std::sort(keys.begin(), keys.end(),
			  [](const QString& s1, const QString& s2) {
				  return s1.length() > s2.length();
			  });

	for (const QString& key : keys) {
		if (!key.isEmpty()) {
			logContent.replace(key, filter[key]);
		}
	}

	return logContent;
}

PasteUpload::PasteUpload(QString text, QString key)
{
	m_key = key;
	QString censoredText = applyFilters(text);
	QByteArray temp;
	QJsonObject topLevelObj;
	QJsonObject sectionObject;
	sectionObject.insert("contents", censoredText);
	QJsonArray sectionArray;
	sectionArray.append(sectionObject);
	topLevelObj.insert("description", "Log Upload");
	topLevelObj.insert("sections", sectionArray);
	QJsonDocument docOut;
	docOut.setObject(topLevelObj);
	m_jsonContent = docOut.toJson();
}

PasteUpload::~PasteUpload() {}

bool PasteUpload::validateText()
{
	return m_jsonContent.size() <= maxSize();
}

void PasteUpload::executeTask()
{
	QNetworkRequest request(QUrl("https://api.paste.ee/v1/pastes"));
	request.setHeader(QNetworkRequest::UserAgentHeader,
					  BuildConfig.USER_AGENT_UNCACHED);

	request.setRawHeader("Content-Type", "application/json");
	request.setRawHeader("Content-Length",
						 QByteArray::number(m_jsonContent.size()));
	request.setRawHeader("X-Auth-Token", m_key.toStdString().c_str());

	QNetworkReply* rep = LAUNCHER->network()->post(request, m_jsonContent);

	m_reply = std::shared_ptr<QNetworkReply>(rep);
	setStatus(tr("Uploading to paste.ee"));
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&PasteUpload::downloadError);
	connect(rep, &QNetworkReply::finished, this,
			&PasteUpload::downloadFinished);
}

void PasteUpload::downloadError(QNetworkReply::NetworkError error)
{
	// error happened during download.
	qCritical() << "Network error: " << error;
	emitFailed(m_reply->errorString());
}

void PasteUpload::downloadFinished()
{
	QByteArray data = m_reply->readAll();
	// if the download succeeded
	if (m_reply->error() == QNetworkReply::NetworkError::NoError) {
		m_reply.reset();
		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error != QJsonParseError::NoError) {
			emitFailed(jsonError.errorString());
			return;
		}
		if (!parseResult(doc)) {
			emitFailed(tr("paste.ee returned an error. Please consult the logs "
						  "for more information"));
			return;
		}
	}
	// else the download failed
	else {
		emitFailed(QString("Network error: %1").arg(m_reply->errorString()));
		m_reply.reset();
		return;
	}
	emitSucceeded();
}

bool PasteUpload::parseResult(QJsonDocument doc)
{
	auto object = doc.object();
	auto status = object.value("success").toBool();
	if (!status) {
		qCritical() << "paste.ee reported error:"
					<< QString(object.value("error").toString());
		return false;
	}
	m_pasteLink = object.value("link").toString();
	m_pasteID = object.value("id").toString();
	qCDebug(netLog) << m_pasteLink;
	return true;
}
