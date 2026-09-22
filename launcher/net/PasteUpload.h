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
#include "tasks/Task.h"
#include <QNetworkReply>
#include <QBuffer>
#include <memory>

class PasteUpload : public Task
{
	Q_OBJECT
  public:
	PasteUpload(QString text, QString key = "public");
	virtual ~PasteUpload();

	QString pasteLink()
	{
		return m_pasteLink;
	}
	QString pasteID()
	{
		return m_pasteID;
	}
	int maxSize()
	{
		// 2MB for paste.ee - public
		if (m_key == "public")
			return 1024 * 1024 * 2;
		// 12MB for paste.ee - with actual key
		return 1024 * 1024 * 12;
	}
	bool validateText();

  protected:
	virtual void executeTask();

  private:
	bool parseResult(QJsonDocument doc);
	QString m_error;
	QString m_pasteID;
	QString m_pasteLink;
	QString m_key;
	QByteArray m_jsonContent;
	std::shared_ptr<QNetworkReply> m_reply;
  public slots:
	void downloadError(QNetworkReply::NetworkError);
	void downloadFinished();
};
