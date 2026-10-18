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

#pragma once

#include <QDateTime>
#include <QString>

namespace UpdateLockFile
{

	//! Marker file names, relative to the data directory.
	inline constexpr auto kLockFileName = ".kite_update.lock";
	inline constexpr auto kFailMarkerName = ".kite_update.fail";
	inline constexpr auto kSuccessMarkerName = ".kite_update.success";
	inline constexpr auto kChangelogName = ".kite_update.changelog";

	//! Left in the unpacked release, telling the second stage where to install.
	inline constexpr auto kUnpackMarkerName = ".kite_updater_unpack.marker";

	//! Records where the pre-update installation was copied to.
	inline constexpr auto kBackupMarkerName = ".kite_update_backup_path.txt";

	//! Where the unpacked release is staged, under the data directory.
	inline constexpr auto kStagingDirName = "kite_update_release";

	//! The updater's own narrative of the last attempt, under logs/.
	inline constexpr auto kUpdateLogName = "kite_update.log";

	struct Contents {
		QDateTime timestamp;
		QString from;	  //!< Version being replaced.
		QString to;		  //!< Version being installed.
		QString target;	  //!< Installation root being written to.
		QString dataPath; //!< Data directory the update is coordinated in.
		int stage = 0;	  //!< Which pass wrote this: 1 or 2, 0 if unstated.
		qint64 pid = 0;	  //!< That pass's process id, 0 if unstated.
	};

	//! The pass that owns the lock while the installation is being written.
	inline constexpr int kInstallStage = 2;

	//! Absolute path of the lock file for \a dataDir.
	QString lockPath(const QString& dataDir);

	//! Absolute path of \a fileName within \a dataDir.
	QString markerPath(const QString& dataDir, const QString& fileName);

	//! Absolute path of the update log for \a dataDir.
	QString updateLogPath(const QString& dataDir);

	bool read(const QString& path, Contents* contents);

	//! Write a lock file. Returns false if it could not be written.
	bool write(const QString& path, const Contents& contents);

} // namespace UpdateLockFile
