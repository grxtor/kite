/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "KiteLogsDialog.h"
#include "ui_KiteLogsDialog.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QShortcut>

#include "Application.h"
#include "BuildConfig.h"
#include "ui/GuiUtil.h"

KiteLogsDialog::KiteLogsDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::KiteLogsDialog),
	  m_liveWatcher(new QFileSystemWatcher(this))
{
	ui->setupUi(this);
	setWindowTitle(tr("Kite Logs"));
	resize(750, 550);

	connect(m_liveWatcher, &QFileSystemWatcher::fileChanged, this,
			&KiteLogsDialog::onLogFileChanged);

	auto findShortcut = new QShortcut(QKeySequence(QKeySequence::Find), this);
	connect(findShortcut, &QShortcut::activated, this,
			[this]() { ui->searchBar->setFocus(); });

	connect(ui->searchBar, &QLineEdit::returnPressed, this,
			&KiteLogsDialog::on_findButton_clicked);

	populateLogList();
}

KiteLogsDialog::~KiteLogsDialog()
{
	delete ui;
}

QString KiteLogsDialog::logDirectory() const
{
	return QDir::currentPath();
}

QString KiteLogsDialog::logFilePath(const QString& name) const
{
	return QDir(logDirectory()).absoluteFilePath(name);
}

void KiteLogsDialog::populateLogList()
{
	ui->selectLogBox->clear();

	// Stop watching old file
	if (m_watching0Log) {
		auto watched = m_liveWatcher->files();
		if (!watched.isEmpty())
			m_liveWatcher->removePaths(watched);
		m_watching0Log = false;
	}

	QString baseName = BuildConfig.KITE_NAME;
	QDir dir(logDirectory());
	QStringList logFiles;

	// Kite-0.log through Kite-4.log
	for (int i = 0; i <= 4; i++) {
		QString fileName = QString("%1-%2.log").arg(baseName).arg(i);
		if (dir.exists(fileName)) {
			logFiles.append(fileName);
		}
	}

	if (logFiles.isEmpty()) {
		setControlsEnabled(false);
		return;
	}

	ui->selectLogBox->addItems(logFiles);

	if (!m_currentFile.isEmpty()) {
		int idx = ui->selectLogBox->findText(m_currentFile);
		if (idx != -1)
			ui->selectLogBox->setCurrentIndex(idx);
		else
			ui->selectLogBox->setCurrentIndex(0);
	} else {
		ui->selectLogBox->setCurrentIndex(0);
	}
}

void KiteLogsDialog::on_selectLogBox_currentIndexChanged(int index)
{
	// Stop watching previous file
	if (m_watching0Log) {
		auto watched = m_liveWatcher->files();
		if (!watched.isEmpty())
			m_liveWatcher->removePaths(watched);
		m_watching0Log = false;
	}

	if (index < 0) {
		m_currentFile.clear();
		ui->textBrowser->clear();
		setControlsEnabled(false);
		return;
	}

	m_currentFile = ui->selectLogBox->itemText(index);
	setControlsEnabled(true);
	loadSelectedLog();

	// Watch Kite-0.log for live updates
	QString baseName = BuildConfig.KITE_NAME;
	if (m_currentFile == QString("%1-0.log").arg(baseName)) {
		QString fullPath = logFilePath(m_currentFile);
		m_liveWatcher->addPath(fullPath);
		m_watching0Log = true;
	}
}

void KiteLogsDialog::loadSelectedLog()
{
	if (m_currentFile.isEmpty()) {
		setControlsEnabled(false);
		return;
	}

	QString fullPath = logFilePath(m_currentFile);
	QFile file(fullPath);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		QMessageBox::critical(this, tr("Error"),
							  tr("Unable to open %1 for reading: %2")
								  .arg(m_currentFile, file.errorString()));
		setControlsEnabled(false);
		return;
	}

	if (file.size() > (1024ll * 1024ll * 12ll)) {
		ui->textBrowser->setPlainText(
			tr("The file (%1) is too big. You may want to open it in a viewer "
			   "optimized for large files.")
				.arg(m_currentFile));
		return;
	}

	QString content = QString::fromUtf8(file.readAll());

	QString fontFamily = APPLICATION->settings()->get("ConsoleFont").toString();
	bool conversionOk = false;
	int fontSize =
		APPLICATION->settings()->get("ConsoleFontSize").toInt(&conversionOk);
	if (!conversionOk) {
		fontSize = 11;
	}
	QTextDocument* doc = ui->textBrowser->document();
	doc->setDefaultFont(QFont(fontFamily, fontSize));
	ui->textBrowser->setPlainText(content);

	// Scroll to bottom for 0.log (live log)
	QString baseName = BuildConfig.KITE_NAME;
	if (m_currentFile == QString("%1-0.log").arg(baseName)) {
		ui->textBrowser->moveCursor(QTextCursor::End);
		ui->textBrowser->ensureCursorVisible();
	}
}

void KiteLogsDialog::on_btnReload_clicked()
{
	populateLogList();
	loadSelectedLog();
}

void KiteLogsDialog::on_btnCopy_clicked()
{
	GuiUtil::setClipboardText(ui->textBrowser->toPlainText());
}

void KiteLogsDialog::on_btnUpload_clicked()
{
	GuiUtil::uploadPaste(ui->textBrowser->toPlainText(), this);
}

void KiteLogsDialog::on_btnDelete_clicked()
{
	if (m_currentFile.isEmpty()) {
		return;
	}
	if (QMessageBox::question(
			this, tr("Delete"),
			tr("Do you really want to delete %1?").arg(m_currentFile),
			QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
		return;
	}
	QFile file(logFilePath(m_currentFile));
	if (!file.remove()) {
		QMessageBox::critical(this, tr("Error"),
							  tr("Unable to delete %1: %2")
								  .arg(m_currentFile, file.errorString()));
	}
	m_currentFile.clear();
	populateLogList();
}

void KiteLogsDialog::on_btnClean_clicked()
{
	QString baseName = BuildConfig.KITE_NAME;
	QStringList toDelete;
	QDir dir(logDirectory());

	for (int i = 0; i <= 4; i++) {
		QString fileName = QString("%1-%2.log").arg(baseName).arg(i);
		if (dir.exists(fileName)) {
			toDelete.append(fileName);
		}
	}

	if (toDelete.isEmpty())
		return;

	QMessageBox* messageBox = new QMessageBox(this);
	messageBox->setWindowTitle(tr("Clean up"));
	messageBox->setText(tr("Do you really want to delete all log files?\n%1")
							.arg(toDelete.join('\n')));
	messageBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	messageBox->setDefaultButton(QMessageBox::Ok);
	messageBox->setIcon(QMessageBox::Question);

	if (messageBox->exec() != QMessageBox::Ok) {
		return;
	}

	QStringList failed;
	for (const auto& item : toDelete) {
		QFile file(logFilePath(item));
		if (!file.remove()) {
			failed.push_back(item);
		}
	}

	if (!failed.empty()) {
		QMessageBox::critical(
			this, tr("Error"),
			tr("Couldn't delete some files:\n%1").arg(failed.join('\n')));
	}

	m_currentFile.clear();
	populateLogList();
}

void KiteLogsDialog::on_findButton_clicked()
{
	auto modifiers = QApplication::keyboardModifiers();
	bool reverse = modifiers & Qt::ShiftModifier;
	ui->textBrowser->find(ui->searchBar->text(),
						  reverse ? QTextDocument::FindFlag::FindBackward
								  : QTextDocument::FindFlag(0));
}

void KiteLogsDialog::onLogFileChanged(const QString& path)
{
	Q_UNUSED(path)
	// Reload current log when 0.log changes
	if (m_watching0Log) {
		loadSelectedLog();
		// QFileSystemWatcher may stop watching after a file is modified
		// (rewritten), re-add it
		QString baseName = BuildConfig.KITE_NAME;
		QString fullPath = logFilePath(QString("%1-0.log").arg(baseName));
		if (QFile::exists(fullPath)) {
			auto watched = m_liveWatcher->files();
			if (!watched.contains(fullPath)) {
				m_liveWatcher->addPath(fullPath);
			}
		}
	}
}

void KiteLogsDialog::setControlsEnabled(bool enabled)
{
	ui->btnReload->setEnabled(true); // always allow reload
	ui->btnCopy->setEnabled(enabled);
	ui->btnUpload->setEnabled(enabled);
	ui->btnDelete->setEnabled(enabled);
	ui->btnClean->setEnabled(enabled);
	ui->textBrowser->setEnabled(enabled);
}
