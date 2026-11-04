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

#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <memory>

#include "LauncherImportModel.h"
#include "launcherimport/ImportBuild.h"
#include "tasks/Task.h"

/*
 * "From another launcher", for QML: finds the launchers on this computer and
 * their instances (in the background), keeps what the user ticks and whether
 * each comes as a copy or a link, checks the disk has room, and imports the
 * ticked ones one after another. Lives as long as the shell, so an import
 * carries on when the dialog that started it closes.
 *
 * Accounts are never read: Kite signs in itself. Nothing leaves the computer.
 */
class LauncherImportController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QObject* model READ model CONSTANT)
	Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
	/* Whether a scan has finished at least once. */
	Q_PROPERTY(bool scanned READ scanned NOTIFY scanningChanged)
	/* Rows that can be imported and are not already. */
	Q_PROPERTY(int availableCount READ availableCount NOTIFY listChanged)
	/* Launchers found that Kite cannot read yet, by name. */
	Q_PROPERTY(QStringList unsupported READ unsupported NOTIFY listChanged)
	/* Launchers whose list could not be read: "name: why". */
	Q_PROPERTY(QStringList unreadable READ unreadable NOTIFY listChanged)

	Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
	Q_PROPERTY(bool allSelected READ allSelected NOTIFY selectionChanged)
	Q_PROPERTY(bool anyLinked READ anyLinked NOTIFY selectionChanged)
	/* What the ticked copies take, and what the disk has free. */
	Q_PROPERTY(QString selectedSizeText READ selectedSizeText NOTIFY selectionChanged)
	Q_PROPERTY(QString freeSpaceText READ freeSpaceText NOTIFY selectionChanged)
	/* "ok", "low" (fits, with little left over) or "short" (does not fit). */
	Q_PROPERTY(QString space READ space NOTIFY selectionChanged)

	Q_PROPERTY(bool running READ running NOTIFY runningChanged)
	/* 0..1 over the whole run, -1 while it cannot tell. */
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(QString status READ status NOTIFY progressChanged)
	Q_PROPERTY(int importedCount READ importedCount NOTIFY progressChanged)
	Q_PROPERTY(int failedCount READ failedCount NOTIFY progressChanged)
	/* "name: why" for each instance that failed in the last run. */
	Q_PROPERTY(QStringList errors READ errors NOTIFY progressChanged)

  public:
	/* Free space to keep beyond what the copies take before warning. */
	static constexpr qint64 kLowSpaceMarginBytes = 2LL * 1024 * 1024 * 1024;

	explicit LauncherImportController(QObject* parent = nullptr);
	~LauncherImportController() override;

	QObject* model()
	{
		return &m_model;
	}
	bool scanning() const
	{
		return m_scanning;
	}
	bool scanned() const
	{
		return m_scanned;
	}
	int availableCount() const;
	QStringList unsupported() const
	{
		return m_unsupported;
	}
	QStringList unreadable() const
	{
		return m_unreadable;
	}
	int selectedCount() const;
	bool allSelected() const;
	bool anyLinked() const;
	QString selectedSizeText() const;
	QString freeSpaceText() const;
	QString space() const;
	bool running() const
	{
		return static_cast<bool>(m_current) || !m_queue.isEmpty();
	}
	double progress() const;
	QString status() const
	{
		return m_status;
	}
	int importedCount() const
	{
		return m_imported;
	}
	int failedCount() const
	{
		return m_failed;
	}
	QStringList errors() const
	{
		return m_errors;
	}

	/* Looks again; ignored while a scan or an import runs. */
	Q_INVOKABLE void scan();
	Q_INVOKABLE void setChecked(const QString& key, bool checked);
	/* Ticks every row that can be imported and is not already, or none. */
	Q_INVOKABLE void setAllChecked(bool checked);
	/* "copy" or "link". */
	Q_INVOKABLE void setMode(const QString& key, const QString& mode);
	/* Opens the instance's game folder in the file manager. */
	Q_INVOKABLE void openFolder(const QString& key);
	/* Imports the ticked rows; false when none is ticked, one already runs or
	 * the copies do not fit. */
	Q_INVOKABLE bool start();
	/* Stops the instance being imported and drops the rest. */
	Q_INVOKABLE void cancel();

  signals:
	void scanningChanged();
	void listChanged();
	void selectionChanged();
	void runningChanged();
	void progressChanged();
	/* A run is over. */
	void finished(int imported, int failed, bool cancelled);

  private:
	void applyScan(const QList<LauncherImport::FoundLauncher>& launchers);
	qint64 selectedBytes() const;
	qint64 freeBytes() const;
	QSet<QString> importedKeys() const;
	QString iconFor(const LauncherImport::FoundInstance& instance) const;
	void startNext();
	void currentFinished(bool ok, const QString& error);

	LauncherImportModel m_model;
	QFutureWatcher<QList<LauncherImport::FoundLauncher>> m_scanWatcher;
	std::shared_ptr<std::atomic_bool> m_scanCancel;
	bool m_scanning = false;
	bool m_scanned = false;
	QStringList m_unsupported;
	QStringList m_unreadable;

	struct Job {
		LauncherImport::BuildRequest request;
		qint64 bytes = 0;
	};
	QList<Job> m_queue;
	Task::Ptr m_current;
	Job m_currentJob;
	qint64 m_runBytes = 0;
	qint64 m_finishedBytes = 0;
	qint64 m_currentDone = 0;
	int m_runCount = 0;
	int m_imported = 0;
	int m_failed = 0;
	bool m_cancelled = false;
	QString m_status;
	QStringList m_errors;
};
