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

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

#include "CrashCollector.h"
#include "CrashWatchList.h"

class CrashOutbox;

/*
 * What happens to a crash Kite could not explain, after the crash doctor said
 * so: it is kept on this computer (CrashWatch) for 30 days, and read again
 * when newer crash rules come. When they know it, the instance's Log tab says
 * "Fix found" with the fix (the service keeps that report) and a desktop
 * notification says so once (MMCO_HOOK_CRASH_FIX_FOUND). Matching happens here;
 * the server is never asked. Also sends the crash to Kite when the player asks
 * (CrashOutbox), or right after the crash when they allowed sending without
 * asking (CrashReportsAutoSend, off by default).
 */
class CrashFollowUp : public QObject
{
	Q_OBJECT

  public:
	/// Analyses a snapshot off the GUI thread and calls back on it.
	using Runner = std::function<void(const CrashDoctor::Snapshot&,
									  std::function<void(const CrashDoctor::Report&)>)>;

	CrashFollowUp(const QString& dir, Runner runner, QObject* parent = nullptr);
	~CrashFollowUp() override;

	CrashOutbox* outbox() const
	{
		return m_outbox;
	}
	bool hasCrashes() const
	{
		return !m_entries.isEmpty();
	}

	/// After every analysis: keeps a fresh crash nothing knew, and sends it
	/// when the player allowed that.
	void analysed(const CrashDoctor::Report& report, bool fresh, int catalogVersion);
	/// The player sends @p report; @p alwaysFromNow turns on sending without asking.
	void send(const CrashDoctor::Report& report, bool alwaysFromNow);
	/// Sent without asking this run.
	bool autoSent(const QString& signature) const
	{
		return m_autoSent.contains(signature);
	}

	/// Reads the kept crashes again that rules older than @p catalogVersion read.
	void catalogChanged(int catalogVersion);
	/// Once the launcher is up: sends what waits, shows the fixes found before.
	void start(int catalogVersion);
	/// @p instanceId launched (its solved crashes are done with), ran without
	/// crashing (all of its crashes are), or the player hid its report.
	void launched(const QString& instanceId);
	void ranClean(const QString& instanceId);
	void dismissed(const QString& instanceId);

	/// The notification's text: "Last Tuesday's Skies crash has a fix now."
	static QString fixFoundText(const QString& instanceName, const QDateTime& crashedAt,
								const QDateTime& now, const QString& language);

  signals:
	/// Newer rules know @p report's crash (its solvedLaterFor is set).
	void solved(const CrashDoctor::Report& report);

  private:
	void reread(const CrashWatch::Entry& entry, int catalogVersion, bool quiet);
	void reread(const CrashWatch::Entry& entry, int catalogVersion, bool quiet,
				const CrashDoctor::Report& report);
	void notify(const CrashWatch::Entry& entry, const CrashDoctor::Report& report);
	void replace(const QList<CrashWatch::Entry>& entries);
	void prune();

	QString m_path;
	Runner m_runner;
	CrashOutbox* m_outbox = nullptr;
	QList<CrashWatch::Entry> m_entries;
	QSet<QString> m_reading;
	QSet<QString> m_autoSent;
};
