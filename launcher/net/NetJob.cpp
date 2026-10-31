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

#include "NetJob.h"
#include "Download.h"

#include <QDebug>
#include <QLocale>

TaskStepProgressList NetJob::getStepProgress() const
{
	TaskStepProgressList steps;
	steps.reserve(parts_progress.size());
	for (auto& slot : parts_progress) {
		if (slot.step) {
			steps.append(slot.step);
		}
	}
	return steps;
}

void NetJob::emitPartStep(int index, TaskStepState state)
{
	auto& slot = parts_progress[index];
	if (!slot.step) {
		return;
	}

	QString name;
	if (downloads.size() == 1 && !objectName().isEmpty()) {
		// A single-file job is better named by its purpose than by the URL tail
		// (often just an id).
		name = objectName();
	} else {
		name = downloads[index]->url().fileName();
		if (name.isEmpty()) {
			name = downloads[index]->url().toString();
		}
	}

	QLocale locale;
	QString details;
	if (slot.total_progress > 0) {
		details = tr("%1 of %2")
					  .arg(locale.formattedDataSize(slot.current_progress),
						   locale.formattedDataSize(slot.total_progress));
	} else if (slot.current_progress > 0) {
		details = locale.formattedDataSize(slot.current_progress);
	}

	slot.step->state = state;
	slot.step->current = slot.current_progress;
	slot.step->total = slot.total_progress;
	slot.step->status = name;
	slot.step->details = details;
	emit stepProgress(*slot.step);
}

void NetJob::updateStatus()
{
	const QString done = QString::number(m_done.size());
	const QString all = QString::number(downloads.size());

	if (objectName().isEmpty()) {
		setStatus(tr("%1 out of %2 files done").arg(done, all));
		return;
	}
	setStatus(tr("%1: %2 out of %3 files done").arg(objectName(), done, all));
}

void NetJob::partSucceeded(int index)
{
	auto& slot = parts_progress[index];
	partProgress(index, slot.total_progress, slot.total_progress);

	m_doing.remove(index);
	m_done.insert(index);
	emitPartStep(index, TaskStepState::Succeeded);
	updateStatus();
	downloads[index].get()->disconnect(this);
	startMoreParts();
}

void NetJob::partFailed(int index)
{
	m_doing.remove(index);
	auto& slot = parts_progress[index];
	if (slot.failures == 3) {
		m_failed.insert(index);
		emitPartStep(index, TaskStepState::Failed);
	} else {
		slot.failures++;
		m_todo.enqueue(index);
		emitPartStep(index, TaskStepState::Waiting);
	}
	updateStatus();
	downloads[index].get()->disconnect(this);
	startMoreParts();
}

void NetJob::partAborted(int index)
{
	m_aborted = true;
	m_doing.remove(index);
	m_failed.insert(index);
	emitPartStep(index, TaskStepState::Failed);
	downloads[index].get()->disconnect(this);
	startMoreParts();
}

void NetJob::partProgress(int index, qint64 bytesReceived, qint64 bytesTotal)
{
	auto& slot = parts_progress[index];
	slot.current_progress = bytesReceived;
	slot.total_progress = bytesTotal;

	// Files not yet picked up get no line; a thousand assets would drown
	// everything else.
	if (m_doing.contains(index)) {
		emitPartStep(index, TaskStepState::Running);
	}

	int done = m_done.size();
	int doing = m_doing.size();
	int all = parts_progress.size();

	qint64 bytesAll = 0;
	qint64 bytesTotalAll = 0;
	for (auto& partIdx : m_doing) {
		auto part = parts_progress[partIdx];
		if (part.total_progress <= 0) {
			continue;
		}
		bytesAll += part.current_progress;
		bytesTotalAll += part.total_progress;
	}

	qint64 inprogress =
		(bytesTotalAll == 0) ? 0 : (bytesAll * 1000) / bytesTotalAll;
	auto current = done * 1000 + doing * inprogress;
	auto current_total = all * 1000;
	// HACK: never jump backwards. FAIL: breaks when the size is unknown and
	// jumps to 1000, so reset 1000 to in-progress.
	if (m_current_progress == 1000) {
		m_current_progress = inprogress;
	}
	if (m_current_progress > current) {
		current = m_current_progress;
	}
	m_current_progress = current;
	setProgress(current, current_total);
}

void NetJob::executeTask()
{
	// Delays early failures so they can be caught more easily.
	QMetaObject::invokeMethod(this, "startMoreParts", Qt::QueuedConnection);
}

void NetJob::startMoreParts()
{
	if (!isRunning()) {
		// A job can be filled with running downloads and started much later.
		return;
	}
	// Check for final conditions when the queue is empty.
	if (!m_todo.size()) {
		if (!m_doing.size()) {
			if (!m_failed.size()) {
				emitSucceeded();
			} else if (m_aborted) {
				emitAborted();
			} else {
				emitFailed(tr("Job '%1' failed to process:\n%2")
							   .arg(objectName())
							   .arg(getFailedFiles().join("\n")));
			}
		}
		return;
	}
	while (m_doing.size() < 6) {
		if (!m_todo.size())
			return;
		int doThis = m_todo.dequeue();
		m_doing.insert(doThis);
		// A line right away, so it shows as running before the first bytes.
		emitPartStep(doThis, TaskStepState::Running);
		updateStatus();
		auto part = downloads[doThis];
		connect(part.get(), &NetAction::succeeded, this,
				&NetJob::partSucceeded);
		connect(part.get(), &NetAction::failed, this, &NetJob::partFailed);
		connect(part.get(), &NetAction::aborted, this, &NetJob::partAborted);
		connect(part.get(), &NetAction::netActionProgress, this,
				&NetJob::partProgress);
		part->start(m_network);
	}
}

QStringList NetJob::getFailedFiles()
{
	QStringList failed;
	for (auto index : m_failed) {
		failed.push_back(downloads[index]->url().toString());
	}
	failed.sort();
	return failed;
}

bool NetJob::canAbort() const
{
	bool canFullyAbort = true;
	for (auto index : m_todo) {
		auto part = downloads[index];
		canFullyAbort &= part->canAbort();
	}
	for (auto index : m_doing) {
		auto part = downloads[index];
		canFullyAbort &= part->canAbort();
	}
	return canFullyAbort;
}

bool NetJob::abort()
{
	bool fullyAborted = true;
	m_failed.unite(QSet<int>(m_todo.begin(), m_todo.end()));
	m_todo.clear();
	auto toKill = m_doing.values();
	for (auto index : toKill) {
		auto part = downloads[index];
		fullyAborted &= part->abort();
	}
	return fullyAborted;
}

bool NetJob::addNetAction(NetAction::Ptr action)
{
	action->m_index_within_job = downloads.size();
	downloads.append(action);
	part_info pi;
	pi.step = std::make_shared<TaskStepProgress>();
	parts_progress.append(pi);
	partProgress(parts_progress.count() - 1, action->currentProgress(),
				 action->totalProgress());

	if (action->isRunning()) {
		connect(action.get(), &NetAction::succeeded, this,
				&NetJob::partSucceeded);
		connect(action.get(), &NetAction::failed, this, &NetJob::partFailed);
		connect(action.get(), &NetAction::netActionProgress, this,
				&NetJob::partProgress);
	} else {
		m_todo.append(parts_progress.size() - 1);
	}
	return true;
}

NetJob::~NetJob() = default;
