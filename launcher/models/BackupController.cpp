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

#include "BackupController.h"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrentRun>
#include <functional>

#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"

namespace
{
QString humanFileSize(qint64 bytes)
{
	if (bytes < 1024) {
		return QStringLiteral("%1 B").arg(bytes);
	}
	if (bytes < 1024 * 1024) {
		return QStringLiteral("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
	}
	if (bytes < 1024LL * 1024 * 1024) {
		return QStringLiteral("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
	}
	return QStringLiteral("%1 GiB")
		.arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
} // namespace

/* One row per backup, newest first - BackupManager::listBackups() already
 * sorts that way. A thin QAbstractListModel rather than a QVariantList
 * property: the list can be long on an instance backed up often, and a list
 * view should not have to rebuild every row's delegate on every refresh(). */
class BackupListModel : public QAbstractListModel
{
	Q_OBJECT
  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		FileNameRole,
		TimestampTextRole,
		SizeTextRole,
	};

	explicit BackupListModel(QObject* parent = nullptr)
		: QAbstractListModel(parent)
	{
	}

	void setEntries(const QList<BackupEntry>& entries)
	{
		beginResetModel();
		m_entries = entries;
		endResetModel();
	}

	QVariant data(const QModelIndex& index, int role) const override
	{
		const int row = index.row();
		if (row < 0 || row >= m_entries.size()) {
			return {};
		}
		const auto& entry = m_entries.at(row);
		switch (role) {
			case NameRole:
			case Qt::DisplayRole:
				return entry.name.isEmpty() ? entry.fileName : entry.name;
			case FileNameRole:
				return entry.fileName;
			case TimestampTextRole:
				return entry.timestamp.toString(
					QStringLiteral("yyyy-MM-dd HH:mm:ss"));
			case SizeTextRole:
				return humanFileSize(entry.sizeBytes);
			default:
				return {};
		}
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : m_entries.size();
	}

	QHash<int, QByteArray> roleNames() const override
	{
		return {
			{ NameRole, "name" },
			{ FileNameRole, "fileName" },
			{ TimestampTextRole, "timestampText" },
			{ SizeTextRole, "sizeText" },
		};
	}

  private:
	QList<BackupEntry> m_entries;
};

/* Runs one BackupManager operation (restore/export/import/delete) off the
 * GUI thread - the same QtConcurrent::run()+QFutureWatcher shape
 * BackupTask.cpp uses for createBackup(), which every one of those blocks
 * on for just as long on a large instance. Kept local rather than added to
 * backup/ itself: nothing here needs progress reporting the way a
 * compression pass does (see BackupManager::ProgressFn), only a plain
 * succeeded/failed at the end. */
class BackupJobTask : public Task
{
	Q_OBJECT
  public:
	using Fn = std::function<bool()>;

	BackupJobTask(QString status, Fn fn, QObject* parent = nullptr)
		: Task(parent), m_fn(std::move(fn))
	{
		setObjectName(QStringLiteral("BackupJobTask"));
		setStatus(status);
		setProgress(0, 0);
	}

	~BackupJobTask() override
	{
		disconnect(&m_watcher, nullptr, this, nullptr);
		if (m_future.isRunning()) {
			m_future.waitForFinished();
		}
	}

  protected:
	void executeTask() override
	{
		connect(&m_watcher, &QFutureWatcher<bool>::finished, this, [this] {
			if (m_future.result()) {
				setProgress(1, 1);
				emitSucceeded();
			} else {
				emitFailed(tr("The operation failed. See the launcher log "
							  "for details."));
			}
		});
		m_future = QtConcurrent::run(QThreadPool::globalInstance(), m_fn);
		m_watcher.setFuture(m_future);
	}

  private:
	Fn m_fn;
	QFuture<bool> m_future;
	QFutureWatcher<bool> m_watcher;
};

BackupController::BackupController(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance)),
	  m_manager(m_instance->id(), m_instance->instanceRoot())
{
	auto* list = new BackupListModel(this);
	m_model.reset(list);
	refresh();

	// Mirrors WorldDataPacksController's own unlocked/runningStatusChanged
	// wiring: `running` needs to track the instance live, not just at
	// restoreBackup() time, so the tab can warn before the click too.
	connect(m_instance.get(), &BaseInstance::runningStatusChanged, this,
			&BackupController::runningChanged);
}

BackupController::~BackupController() = default;

QObject* BackupController::model() const
{
	return m_model.get();
}

bool BackupController::running() const
{
	return m_instance && m_instance->isRunning();
}

void BackupController::refresh()
{
	m_entries = m_manager.listBackups();
	static_cast<BackupListModel*>(m_model.get())->setEntries(m_entries);
}

QObject* BackupController::createBackup(const QString& label)
{
	// BackupManager itself is cheap to copy (three QStrings) - captured by
	// value so the worker thread never touches `this`.
	BackupManager manager = m_manager;
	const QString labelCopy = label;

	auto* task = new BackupJobTask(
		tr("Creating backup…"),
		[manager, labelCopy]() mutable {
			return manager.createBackup(labelCopy).isValid();
		});

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Backup"));
	connect(watcher, &TaskWatcher::finished, this, [this](bool ok) {
		if (ok) {
			refresh();
		}
	});
	task->start();
	return watcher;
}

QObject* BackupController::restoreBackup(int row)
{
	if (m_instance->isRunning() || row < 0 || row >= m_entries.size()) {
		return nullptr;
	}
	const BackupEntry entry = m_entries.at(row);
	BackupManager manager = m_manager;

	auto* task = new BackupJobTask(tr("Restoring backup…"), [manager, entry]() mutable {
		return manager.restoreBackup(entry);
	});

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Restore"));
	connect(watcher, &TaskWatcher::finished, this, [this](bool ok) {
		if (ok) {
			refresh();
		}
	});
	task->start();
	return watcher;
}

QObject* BackupController::deleteBackup(int row)
{
	if (row < 0 || row >= m_entries.size()) {
		return nullptr;
	}
	const BackupEntry entry = m_entries.at(row);
	BackupManager manager = m_manager;

	auto* task = new BackupJobTask(tr("Deleting backup…"), [manager, entry]() mutable {
		return manager.deleteBackup(entry);
	});

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Delete"));
	connect(watcher, &TaskWatcher::finished, this, [this](bool ok) {
		if (ok) {
			refresh();
		}
	});
	task->start();
	return watcher;
}

QObject* BackupController::exportBackup(int row, const QString& destUrlOrPath)
{
	if (row < 0 || row >= m_entries.size()) {
		return nullptr;
	}
	const BackupEntry entry = m_entries.at(row);
	const QUrl url(destUrlOrPath);
	const QString dest = url.isLocalFile() ? url.toLocalFile() : destUrlOrPath;
	BackupManager manager = m_manager;

	auto* task = new BackupJobTask(tr("Exporting backup…"), [manager, entry, dest]() mutable {
		return manager.exportBackup(entry, dest);
	});

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Export"));
	task->start();
	return watcher;
}

QObject* BackupController::importBackup(const QString& fileUrlOrPath,
										const QString& label)
{
	const QUrl url(fileUrlOrPath);
	const QString src = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
	BackupManager manager = m_manager;
	const QString labelCopy = label;

	auto* task = new BackupJobTask(tr("Importing backup…"), [manager, src, labelCopy]() mutable {
		return manager.importBackup(src, labelCopy).isValid();
	});

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Import"));
	connect(watcher, &TaskWatcher::finished, this, [this](bool ok) {
		if (ok) {
			refresh();
		}
	});
	task->start();
	return watcher;
}

#include "BackupController.moc"
