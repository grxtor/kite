/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "Task.h"

#include <QDebug>
#include <QPointer>

Task::Task(QObject* parent) : QObject(parent) {}

void Task::setStatus(const QString& new_status)
{
	if (m_status != new_status) {
		m_status = new_status;
		emit status(m_status);
	}
}

void Task::setDetails(const QString& new_details)
{
	if (m_details != new_details) {
		m_details = new_details;
		emit details(m_details);
	}
}

void Task::setProgress(qint64 current, qint64 total)
{
	m_progress = current;
	m_progressTotal = total;
	emit progress(m_progress, m_progressTotal);
}

void Task::propagateStepProgress(TaskStepProgress const& step_progress)
{
	emit stepProgress(step_progress);
}

void Task::propagateStepsFrom(Task* other)
{
	if (!other) {
		return;
	}
	connect(other, &Task::stepProgress, this, &Task::propagateStepProgress,
			Qt::UniqueConnection);
}

void Task::start()
{
	switch (m_state) {
		case State::Inactive: {
			qDebug() << "Task" << describe() << "starting for the first time";
			break;
		}
		case State::AbortedByUser: {
			qDebug() << "Task" << describe()
					 << "restarting for after being aborted by user";
			break;
		}
		case State::Failed: {
			qDebug() << "Task" << describe()
					 << "restarting for after failing at first";
			break;
		}
		case State::Succeeded: {
			qDebug() << "Task" << describe()
					 << "restarting for after succeeding at first";
			break;
		}
		case State::Running: {
			qWarning() << "Kite tried to start task" << describe()
					   << "while it was already running!";
			return;
		}
	}
	// NOTE: only fall thorugh to here in end states
	m_state = State::Running;
	emit started();
	executeTask();
}

void Task::emitFailed(QString reason)
{
	// Don't fail twice.
	if (!isRunning()) {
		qCritical() << "Task" << describe()
					<< "failed while not running!!!!: " << reason;
		return;
	}
	m_state = State::Failed;
	m_failReason = reason;
	qCritical() << "Task" << describe() << "failed: " << reason;
	QPointer<Task> guard(this);
	emit failed(reason);
	if (guard)
		emit finished();
}

void Task::emitAborted()
{
	// Don't abort twice.
	if (!isRunning()) {
		qCritical() << "Task" << describe() << "aborted while not running!!!!";
		return;
	}
	m_state = State::AbortedByUser;
	m_failReason = "Aborted.";
	qDebug() << "Task" << describe() << "aborted.";
	QPointer<Task> guard(this);
	emit failed(m_failReason);
	if (guard)
		emit finished();
}

void Task::emitSucceeded()
{
	// Don't succeed twice.
	if (!isRunning()) {
		qCritical() << "Task" << describe()
					<< "succeeded while not running!!!!";
		return;
	}
	m_state = State::Succeeded;
	qDebug() << "Task" << describe() << "succeeded";
	QPointer<Task> guard(this);
	emit succeeded();
	if (guard)
		emit finished();
}

QString Task::describe()
{
	QString outStr;
	QTextStream out(&outStr);
	out << metaObject()->className() << QChar('(');
	auto name = objectName();
	if (name.isEmpty()) {
		out << QString("0x%1").arg((quintptr)this, 0, 16);
	} else {
		out << name;
	}
	out << QChar(')');
	out.flush();
	return outStr;
}

bool Task::isRunning() const
{
	return m_state == State::Running;
}

bool Task::isFinished() const
{
	return m_state != State::Running && m_state != State::Inactive;
}

bool Task::wasSuccessful() const
{
	return m_state == State::Succeeded;
}

bool Task::wasAborted() const
{
	return m_state == State::AbortedByUser;
}

QString Task::failReason() const
{
	return m_failReason;
}

void Task::logWarning(const QString& line)
{
	qWarning() << line;
	m_Warnings.append(line);
}

QStringList Task::warnings() const
{
	return m_Warnings;
}
