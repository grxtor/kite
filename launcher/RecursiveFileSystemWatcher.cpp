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

#include "RecursiveFileSystemWatcher.h"

#include <QRegularExpression>
#include <QDebug>

RecursiveFileSystemWatcher::RecursiveFileSystemWatcher(QObject* parent)
	: QObject(parent), m_watcher(new QFileSystemWatcher(this))
{
	connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
			&RecursiveFileSystemWatcher::fileChange);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&RecursiveFileSystemWatcher::directoryChange);
}

void RecursiveFileSystemWatcher::setRootDir(const QDir& root)
{
	bool wasEnabled = m_isEnabled;
	disable();
	m_root = root;
	setFiles(scanRecursive(m_root));
	if (wasEnabled) {
		enable();
	}
}
void RecursiveFileSystemWatcher::setWatchFiles(const bool watchFiles)
{
	bool wasEnabled = m_isEnabled;
	disable();
	m_watchFiles = watchFiles;
	if (wasEnabled) {
		enable();
	}
}

void RecursiveFileSystemWatcher::enable()
{
	if (m_isEnabled) {
		return;
	}
	Q_ASSERT(m_root != QDir::root());
	addFilesToWatcherRecursive(m_root);
	m_isEnabled = true;
}
void RecursiveFileSystemWatcher::disable()
{
	if (!m_isEnabled) {
		return;
	}
	m_isEnabled = false;
	auto files = m_watcher->files();
	if (!files.isEmpty())
		m_watcher->removePaths(files);
	auto dirs = m_watcher->directories();
	if (!dirs.isEmpty())
		m_watcher->removePaths(dirs);
}

void RecursiveFileSystemWatcher::setFiles(const QStringList& files)
{
	if (files != m_files) {
		m_files = files;
		emit filesChanged();
	}
}

void RecursiveFileSystemWatcher::addFilesToWatcherRecursive(const QDir& dir)
{
	m_watcher->addPath(dir.absolutePath());
	if (m_recursive) {
		for (const QString& directory :
			 dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			addFilesToWatcherRecursive(dir.absoluteFilePath(directory));
		}
	}
	if (m_watchFiles) {
		for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
			m_watcher->addPath(info.absoluteFilePath());
		}
	}
}
QStringList RecursiveFileSystemWatcher::scanRecursive(const QDir& directory)
{
	QStringList ret;
	if (!m_matcher) {
		return {};
	}
	if (m_recursive) {
		for (const QString& dir : directory.entryList(
				 QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
			ret.append(scanRecursive(directory.absoluteFilePath(dir)));
		}
	}
	for (const QString& file :
		 directory.entryList(QDir::Files | QDir::Hidden)) {
		auto relPath =
			m_root.relativeFilePath(directory.absoluteFilePath(file));
		if (m_matcher->matches(relPath)) {
			ret.append(relPath);
		}
	}
	return ret;
}

void RecursiveFileSystemWatcher::fileChange(const QString& path)
{
	emit fileChanged(path);
}
void RecursiveFileSystemWatcher::directoryChange(const QString&)
{
	setFiles(scanRecursive(m_root));
}
