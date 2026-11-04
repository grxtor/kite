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

#include "UpdateLockFile.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

QString UpdateLockFile::markerPath(const QString& dataDir,
								   const QString& fileName)
{
	return QDir(dataDir).absoluteFilePath(fileName);
}

QString UpdateLockFile::lockPath(const QString& dataDir)
{
	return markerPath(dataDir, QLatin1String(kLockFileName));
}

QString UpdateLockFile::updateLogPath(const QString& dataDir)
{
	return QDir(dataDir).absoluteFilePath(
		QStringLiteral("logs/%1").arg(QLatin1String(kUpdateLogName)));
}

QString UpdateLockFile::managedBy(const QString& installRoot)
{
	QFile file(QDir(installRoot).absoluteFilePath(
		QLatin1String(kManagedByFileName)));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return {};
	const QString name = QString::fromUtf8(file.readLine(64)).trimmed();
	static const QRegularExpression plainWord(
		QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$"));
	if (!plainWord.match(name).hasMatch()) {
		qWarning() << "Ignoring" << file.fileName()
				   << ": it does not name a package manager.";
		return {};
	}
	return name;
}

bool UpdateLockFile::read(const QString& path, Contents* contents)
{
	Q_ASSERT(contents);
	*contents = Contents();

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "Could not read the update lock file" << path << ":"
				   << file.errorString();
		return false;
	}

	QTextStream stream(&file);
	while (!stream.atEnd()) {
		const QString line = stream.readLine();
		const int separator = line.indexOf(QLatin1Char('='));
		if (separator <= 0)
			continue;

		const QString key = line.left(separator).trimmed().toUpper();
		const QString value = line.mid(separator + 1).trimmed();

		if (key == QLatin1String("TIMESTAMP")) {
			contents->timestamp = QDateTime::fromString(value, Qt::ISODate);
		} else if (key == QLatin1String("FROM")) {
			contents->from = value;
		} else if (key == QLatin1String("TO")) {
			contents->to = value;
		} else if (key == QLatin1String("TARGET")) {
			contents->target = value;
		} else if (key == QLatin1String("DATA_PATH")) {
			contents->dataPath = value;
		} else if (key == QLatin1String("STAGE")) {
			contents->stage = value.toInt();
		} else if (key == QLatin1String("PID")) {
			contents->pid = value.toLongLong();
		}
		// Unknown keys are ignored rather than rejected, so a newer updater
		// can add a field without an older launcher refusing to start.
	}

	return true;
}

bool UpdateLockFile::write(const QString& path, const Contents& contents)
{
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		qWarning() << "Could not create the directory for the update lock file"
				   << path;
		return false;
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
				   QIODevice::Text)) {
		qWarning() << "Could not write the update lock file" << path << ":"
				   << file.errorString();
		return false;
	}

	QTextStream stream(&file);
	stream << "TIMESTAMP=" << contents.timestamp.toString(Qt::ISODate) << '\n'
		   << "FROM=" << contents.from << '\n'
		   << "TO=" << contents.to << '\n'
		   << "TARGET=" << contents.target << '\n'
		   << "DATA_PATH=" << contents.dataPath << '\n'
		   << "STAGE=" << contents.stage << '\n'
		   << "PID=" << contents.pid << '\n';
	stream.flush();

	if (file.error() != QFileDevice::NoError) {
		qWarning() << "Could not write the update lock file" << path << ":"
				   << file.errorString();
		return false;
	}

	return true;
}
