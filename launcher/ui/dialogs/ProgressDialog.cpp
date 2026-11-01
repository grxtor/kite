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

#include "ProgressDialog.h"
#include "ui_ProgressDialog.h"

#include <QCloseEvent>
#include <QPoint>
#include <QDebug>

#include "tasks/Task.h"

ProgressDialog::ProgressDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::ProgressDialog)
{
	ui->setupUi(this);
	this->setWindowFlags(this->windowFlags() &
						 ~Qt::WindowContextHelpButtonHint);
	setAttribute(Qt::WidgetAttribute::WA_QuitOnClose, true);
	changeProgress(0, 100);
	updateSize(true);
}

ProgressDialog::~ProgressDialog()
{
	// The task can outlive the dialog, and it must not talk to a half torn
	// down one on the way out.
	for (auto& connection : m_task_connections) {
		disconnect(connection);
	}
	delete ui;
}

void ProgressDialog::updateSize(bool recenterParent)
{
	QSize old_size = size();
	QPoint old_pos = pos();

	// Add up what actually has to fit: the status line and the overall bar.
	const int spacing = ui->verticalLayout->spacing();
	int min_height = ui->detailsLabel->minimumSize().height() + (spacing * 2);
	min_height += ui->taskProgressBar->minimumSize().height() + spacing;
	min_height = qMax(min_height, 60);

	setMinimumSize(QSize(480, min_height));
	adjustSize();

	QSize new_size = size();
	QWidget* owner = parentWidget();

	if (recenterParent && owner) {
		move(qMax(0, owner->x() + ((owner->width() - new_size.width()) / 2)),
			 qMax(0,
				  owner->y() + ((owner->height() - new_size.height()) / 2)));
		return;
	}

	if (old_size == new_size || !isVisible()) {
		// Moving a dialog that was never shown counts as placing it by hand,
		// which would rob it of being centred later on.
		return;
	}

	// Grow around the middle, otherwise the dialog walks across the screen as
	// its text changes.
	QSize diff = old_size - new_size;
	move(qMax(0, old_pos.x() + (diff.width() / 2)),
		 qMax(0, old_pos.y() + (diff.height() / 2)));
}

int ProgressDialog::execWithTask(Task* task)
{
	this->task = task;
	QDialog::DialogCode result{};

	if (!task) {
		qDebug() << "Programmer error: progress dialog created with null task.";
		return Accepted;
	}

	if (handleImmediateResult(result)) {
		return result;
	}

	// Connect signals.
	m_task_connections.append(
		connect(task, &Task::started, this, &ProgressDialog::onTaskStarted));
	m_task_connections.append(
		connect(task, &Task::failed, this, &ProgressDialog::onTaskFailed));
	m_task_connections.append(connect(task, &Task::succeeded, this,
									  &ProgressDialog::onTaskSucceeded));
	m_task_connections.append(
		connect(task, &Task::status, this, &ProgressDialog::changeStatus));
	m_task_connections.append(
		connect(task, &Task::details, this, &ProgressDialog::changeDetails));
	m_task_connections.append(
		connect(task, &Task::progress, this, &ProgressDialog::changeProgress));

	updateSize();

	if (task->isRunning()) {
		// Joined a task that is already under way, so catch up on where it
		// has got to.
		changeStatus(task->getStatus());
		changeDetails(task->getDetails());
		changeProgress(task->getProgress(), task->getTotalProgress());
	} else {
		// Start it from inside the dialog's own event loop. Kicking it off
		// beforehand would leave the user staring at a frozen window for as
		// long as the task takes to get going.
		QMetaObject::invokeMethod(task, &Task::start, Qt::QueuedConnection);
	}

	return QDialog::exec();
}

bool ProgressDialog::handleImmediateResult(QDialog::DialogCode& result)
{
	if (task->isFinished()) {
		if (task->wasSuccessful()) {
			result = QDialog::Accepted;
		} else {
			result = QDialog::Rejected;
		}
		return true;
	}
	return false;
}

void ProgressDialog::onTaskStarted() {}

void ProgressDialog::onTaskFailed(QString failure)
{
	Q_UNUSED(failure);
	reject();
	hide();
}

void ProgressDialog::onTaskSucceeded()
{
	accept();
	hide();
}

void ProgressDialog::changeStatus(const QString& status)
{
	ui->statusLabel->setText(status);
	ui->statusLabel->adjustSize();
	updateSize();
}

void ProgressDialog::changeDetails(const QString& details)
{
	ui->detailsLabel->setText(details);
	ui->detailsLabel->adjustSize();
	updateSize();
}

void ProgressDialog::changeProgress(qint64 current, qint64 total)
{
	ui->taskProgressBar->setMaximum(total);
	ui->taskProgressBar->setValue(current);
}

void ProgressDialog::closeEvent(QCloseEvent* e)
{
	if (task && task->isRunning()) {
		e->ignore();
	} else {
		QDialog::closeEvent(e);
	}
}
