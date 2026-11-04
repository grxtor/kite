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

#include "LauncherImportController.h"

#include <QLocale>
#include <QStorageInfo>
#include <QtConcurrentRun>

#include "DesktopServices.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "launcherimport/LauncherImportTask.h"
#include "launcherimport/LauncherScan.h"

using LauncherImport::FoundInstance;
using LauncherImport::FoundLauncher;
using LauncherImport::Mode;

namespace
{
	QString sizeText(qint64 bytes)
	{
		return QLocale().formattedDataSize(qMax<qint64>(0, bytes), 1,
										   QLocale::DataSizeTraditionalFormat);
	}
} // namespace

LauncherImportController::LauncherImportController(QObject* parent)
	: QObject(parent)
{
	connect(&m_scanWatcher, &QFutureWatcher<QList<FoundLauncher>>::finished, this,
			[this] { applyScan(m_scanWatcher.result()); });
}

LauncherImportController::~LauncherImportController()
{
	if (m_scanCancel) {
		m_scanCancel->store(true);
	}
	m_queue.clear();
	if (m_current) {
		m_current->abort();
	}
}

void LauncherImportController::scan()
{
	if (m_scanning || running()) {
		return;
	}
	m_scanning = true;
	emit scanningChanged();
	m_scanCancel = std::make_shared<std::atomic_bool>(false);
	auto cancel = m_scanCancel;
	const LauncherImport::SearchEnvironment env =
		LauncherImport::SearchEnvironment::current();
	m_scanWatcher.setFuture(QtConcurrent::run([env, cancel]() {
		QList<FoundLauncher> launchers = LauncherImport::scanAll(env, cancel.get());
		LauncherImport::countCopySizes(&launchers, cancel.get());
		return launchers;
	}));
}

void LauncherImportController::applyScan(const QList<FoundLauncher>& launchers)
{
	m_unsupported.clear();
	m_unreadable.clear();
	QList<FoundLauncher> readable;
	for (const FoundLauncher& launcher : launchers) {
		const QString name = LauncherImport::sourceName(launcher.source);
		if (!LauncherImport::sourceSupported(launcher.source)) {
			m_unsupported.append(name);
		} else if (!launcher.problem.isEmpty()) {
			m_unreadable.append(tr("%1: %2").arg(name, launcher.problem));
		} else {
			readable.append(launcher);
		}
	}
	m_unsupported.removeDuplicates();
	m_model.reset(readable, importedKeys());
	m_scanning = false;
	m_scanned = true;
	emit scanningChanged();
	emit listChanged();
	emit selectionChanged();
}

QSet<QString> LauncherImportController::importedKeys() const
{
	QSet<QString> keys;
	auto instances = LAUNCHER->instances();
	for (int i = 0; i < instances->count(); ++i) {
		const QString key = instances->at(i)->settings()->get(
			LauncherImport::kImportedFromKey).toString();
		if (!key.isEmpty()) {
			keys.insert(key);
		}
	}
	return keys;
}

int LauncherImportController::availableCount() const
{
	int count = 0;
	for (const auto& row : m_model.rows()) {
		count += row.instance.importable() && !row.imported ? 1 : 0;
	}
	return count;
}

int LauncherImportController::selectedCount() const
{
	int count = 0;
	for (const auto& row : m_model.rows()) {
		count += row.checked ? 1 : 0;
	}
	return count;
}

bool LauncherImportController::allSelected() const
{
	bool any = false;
	for (const auto& row : m_model.rows()) {
		if (row.instance.importable() && !row.imported) {
			any = true;
			if (!row.checked) {
				return false;
			}
		}
	}
	return any;
}

bool LauncherImportController::anyLinked() const
{
	for (const auto& row : m_model.rows()) {
		if (row.checked && row.mode == Mode::Link) {
			return true;
		}
	}
	return false;
}

qint64 LauncherImportController::selectedBytes() const
{
	qint64 bytes = 0;
	for (const auto& row : m_model.rows()) {
		if (row.checked && row.mode == Mode::Copy) {
			bytes += qMax<qint64>(0, row.instance.copyBytes);
		}
	}
	return bytes;
}

qint64 LauncherImportController::freeBytes() const
{
	QStorageInfo storage(LAUNCHER->instances()->primaryDir());
	return storage.isValid() ? storage.bytesAvailable() : -1;
}

QString LauncherImportController::selectedSizeText() const
{
	return sizeText(selectedBytes());
}

QString LauncherImportController::freeSpaceText() const
{
	const qint64 free = freeBytes();
	return free < 0 ? QString() : sizeText(free);
}

QString LauncherImportController::space() const
{
	const qint64 free = freeBytes();
	const qint64 needed = selectedBytes();
	if (free < 0 || needed == 0) {
		return QStringLiteral("ok");
	}
	if (needed > free) {
		return QStringLiteral("short");
	}
	return free - needed < kLowSpaceMarginBytes ? QStringLiteral("low")
												: QStringLiteral("ok");
}

void LauncherImportController::setChecked(const QString& key, bool checked)
{
	if (!running() && m_model.setChecked(m_model.indexOf(key), checked)) {
		emit selectionChanged();
	}
}

void LauncherImportController::setAllChecked(bool checked)
{
	if (running()) {
		return;
	}
	for (int i = 0; i < m_model.rows().size(); ++i) {
		const auto& row = m_model.rows().at(i);
		m_model.setChecked(i, checked && row.instance.importable() && !row.imported);
	}
	emit selectionChanged();
}

void LauncherImportController::setMode(const QString& key, const QString& mode)
{
	const Mode wanted = mode == QLatin1String("link") ? Mode::Link : Mode::Copy;
	if (!running() && m_model.setMode(m_model.indexOf(key), wanted)) {
		emit selectionChanged();
	}
}

void LauncherImportController::openFolder(const QString& key)
{
	const int row = m_model.indexOf(key);
	if (row >= 0) {
		DesktopServices::openDirectory(m_model.rows().at(row).instance.gameDir);
	}
}

QString LauncherImportController::iconFor(const FoundInstance& instance) const
{
	/* A Prism icon key only means something when Kite has the same icon. */
	const QString key = instance.iconKey;
	if (key.isEmpty() || !LAUNCHER->icons()->icon(key)) {
		return QStringLiteral("default");
	}
	return key;
}

bool LauncherImportController::start()
{
	if (running() || selectedCount() == 0 || space() == QLatin1String("short")) {
		return false;
	}
	m_queue.clear();
	m_runBytes = 0;
	for (const auto& row : m_model.rows()) {
		if (!row.checked) {
			continue;
		}
		Job job;
		job.request.instance = row.instance;
		job.request.mode = row.mode;
		job.request.name = row.instance.name;
		job.request.iconKey = iconFor(row.instance);
		job.bytes = row.mode == Mode::Copy ? qMax<qint64>(0, row.instance.copyBytes) : 0;
		m_runBytes += job.bytes;
		m_queue.append(job);
	}
	m_runCount = m_queue.size();
	m_finishedBytes = 0;
	m_currentDone = 0;
	m_imported = 0;
	m_failed = 0;
	m_cancelled = false;
	m_errors.clear();
	emit runningChanged();
	startNext();
	return true;
}

void LauncherImportController::cancel()
{
	if (!running()) {
		return;
	}
	m_cancelled = true;
	m_queue.clear();
	if (m_current) {
		m_current->abort();
	}
}

double LauncherImportController::progress() const
{
	if (m_runCount == 0) {
		return -1;
	}
	if (m_runBytes > 0) {
		return qBound(0.0, double(m_finishedBytes + m_currentDone) / double(m_runBytes), 1.0);
	}
	return double(m_imported + m_failed) / double(m_runCount);
}

void LauncherImportController::startNext()
{
	if (m_queue.isEmpty()) {
		m_current.reset();
		m_status.clear();
		emit progressChanged();
		emit runningChanged();
		emit selectionChanged();
		emit listChanged();
		emit finished(m_imported, m_failed, m_cancelled);
		return;
	}
	m_currentJob = m_queue.takeFirst();
	m_currentDone = 0;
	auto* task = new LauncherImportTask(m_currentJob.request);
	task->setName(m_currentJob.request.name);
	task->setIcon(m_currentJob.request.iconKey);
	task->setTargetDir(LAUNCHER->instances()->primaryDir());
	m_current = Task::Ptr(LAUNCHER->instances()->wrapInstanceTask(task));
	m_status = m_currentJob.request.mode == Mode::Link
				   ? tr("Linking %1").arg(m_currentJob.request.name)
				   : tr("Copying %1").arg(m_currentJob.request.name);
	connect(m_current.get(), &Task::progress, this, [this](qint64 current, qint64) {
		m_currentDone = qMin(current, m_currentJob.bytes);
		emit progressChanged();
	});
	connect(m_current.get(), &Task::succeeded, this, [this] { currentFinished(true, {}); });
	connect(m_current.get(), &Task::failed, this,
			[this](const QString& error) { currentFinished(false, error); });
	emit progressChanged();
	m_current->start();
}

void LauncherImportController::currentFinished(bool ok, const QString& error)
{
	/* Finishing slots run inside the task's own signal; the task is let go
	 * once that returns. */
	Task::Ptr done = m_current;
	m_current.reset();
	m_finishedBytes += m_currentJob.bytes;
	m_currentDone = 0;
	if (ok) {
		++m_imported;
		m_model.setImported(m_model.indexOf(m_currentJob.request.instance.key));
	} else if (!m_cancelled) {
		++m_failed;
		m_errors.append(tr("%1: %2").arg(m_currentJob.request.name, error));
	}
	QMetaObject::invokeMethod(
		this,
		[this, done] {
			Q_UNUSED(done);
			startNext();
		},
		Qt::QueuedConnection);
}
