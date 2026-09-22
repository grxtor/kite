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

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>

class Task;
class QTimer;

/**
 * Watches one Task's status()/progress()/finished() signals and turns them
 * into the pair of values a launch card wants to show: a human-readable
 * status line, and a 0..1 progress fraction (-1 while indeterminate, or
 * once nothing is being watched at all).
 *
 * Exists so InstanceList does not have to juggle per-instance
 * QMetaObject::Connection bookkeeping itself, and so this piece can be unit
 * tested against a plain Task subclass instead of a real launch.
 */
class LaunchProgressTracker : public QObject
{
	Q_OBJECT

  public:
	explicit LaunchProgressTracker(QObject* parent = nullptr);

	/**
	 * Start watching @p task instead of whatever was being watched
	 * before. A null task (or the task already being watched) is a
	 * no-op / clear(), same as when the watched task finishes or is
	 * destroyed on its own.
	 */
	void watch(Task* task);

	/// Stop watching and go back to the idle state (empty status(), -1
	/// progress()), announced right away rather than coalesced.
	void clear();

	/// Current human-readable status/step text; empty when idle.
	QString status() const
	{
		return m_status;
	}

	/// 0..1 when the watched task last reported determinate progress,
	/// -1 while indeterminate or idle.
	double progress() const
	{
		return m_progress;
	}

	/**
	 * Minimum spacing between changed() emissions, in ms. Defaults to
	 * ~10/s. Exposed so tests do not have to wait on the real interval.
	 */
	void setMinIntervalMs(int ms)
	{
		m_minIntervalMs = ms;
	}

  signals:
	/**
	 * status() and/or progress() have (probably) changed.
	 *
	 * Coalesced to at most once per the configured interval while the
	 * watched task keeps reporting, so a fast-moving download does not
	 * turn into a redraw storm. The transition to idle (clear(), or the
	 * watched task finishing/dying) is never delayed by this.
	 */
	void changed();

  private slots:
	void onStatus(const QString& status);
	void onProgress(qint64 current, qint64 total);
	void onFinished();

  private:
	void stopWatching();
	void scheduleEmit();
	void emitNow();

	Task* m_task = nullptr;
	QVector<QMetaObject::Connection> m_connections;
	QString m_status;
	double m_progress = -1;
	int m_minIntervalMs = 100;
	QElapsedTimer m_sinceLastEmit;
	QTimer* m_pendingTimer = nullptr;
};
