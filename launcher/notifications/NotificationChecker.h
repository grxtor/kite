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

#include <QObject>

#include "net/NetJob.h"
#include "net/Download.h"

class NotificationChecker : public QObject
{
	Q_OBJECT

  public:
	explicit NotificationChecker(QObject* parent = 0);

	void setNotificationsUrl(const QUrl& notificationsUrl);
	void setApplicationPlatform(QString platform);
	void setApplicationChannel(QString channel);
	void setApplicationFullVersion(QString version);

	struct NotificationEntry {
		int id;
		QString message;
		enum { Critical, Warning, Information } type;
		QString channel;
		QString platform;
		QString from;
		QString to;
	};

	QList<NotificationEntry> notificationEntries() const;

	/* The "ShownNotifications" setting: the ids of the notifications the
	 * user asked not to see again, as a comma-separated list. Shared by
	 * whichever UI shows them, so both read and write it the same way. */
	static QList<int> parseShownIds(const QString& setting);
	static QString formatShownIds(const QList<int>& ids);

  public slots:
	void checkForNotifications();

  private slots:
	void downloadSucceeded(int);

  signals:
	void notificationCheckFinished();

  private:
	bool entryApplies(const NotificationEntry& entry) const;

  private:
	QList<NotificationEntry> m_entries;
	QUrl m_notificationsUrl;
	NetJob::Ptr m_checkJob;
	Net::Download::Ptr m_download;

	QString m_appVersionChannel;
	QString m_appPlatform;
	QString m_appFullVersion;
};
