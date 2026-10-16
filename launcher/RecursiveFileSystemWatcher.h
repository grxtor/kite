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

#include <QFileSystemWatcher>
#include <QDir>
#include "pathmatcher/IPathMatcher.h"

class RecursiveFileSystemWatcher : public QObject
{
	Q_OBJECT
  public:
	RecursiveFileSystemWatcher(QObject* parent);

	void setRootDir(const QDir& root);
	QDir rootDir() const
	{
		return m_root;
	}

	// WARNING: setting this to true may be bad for performance
	void setWatchFiles(const bool watchFiles);
	bool watchFiles() const
	{
		return m_watchFiles;
	}

	// On by default. Off: only @c root itself is listed and watched, not its
	// subfolders - for a folder (like the launcher's own data folder) that
	// holds far more than the files being looked for. Set before setRootDir().
	void setRecursive(const bool recursive)
	{
		m_recursive = recursive;
	}
	bool recursive() const
	{
		return m_recursive;
	}

	void setMatcher(IPathMatcher::Ptr matcher)
	{
		m_matcher = matcher;
	}

	QStringList files() const
	{
		return m_files;
	}

  signals:
	void filesChanged();
	void fileChanged(const QString& path);

  public slots:
	void enable();
	void disable();

  private:
	QDir m_root;
	bool m_watchFiles = false;
	bool m_recursive = true;
	bool m_isEnabled = false;
	IPathMatcher::Ptr m_matcher;

	QFileSystemWatcher* m_watcher;

	QStringList m_files;
	void setFiles(const QStringList& files);

	void addFilesToWatcherRecursive(const QDir& dir);
	QStringList scanRecursive(const QDir& dir);

  private slots:
	void fileChange(const QString& path);
	void directoryChange(const QString& path);
};
