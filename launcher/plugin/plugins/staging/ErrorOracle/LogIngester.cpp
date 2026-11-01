/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0 */

#include "LogIngester.h"

namespace
{
	QString readFileTail(const QString& path, int maxBytes = 512 * 1024)
	{
		QFile f(path);
		if (!f.open(QIODevice::ReadOnly))
			return {};
		qint64 size = f.size();
		if (size > maxBytes)
			f.seek(size - maxBytes);
		return QString::fromUtf8(f.readAll());
	}

	QString latestCrashReport(const QString& crashDir)
	{
		QDir d(crashDir);
		if (!d.exists())
			return {};
		const auto files = d.entryInfoList({"*.txt"}, QDir::Files,
										   QDir::Time | QDir::Reversed);
		if (files.isEmpty())
			return {};
		return files.last().absoluteFilePath();
	}
} // namespace

LogIngester::Bundle
LogIngester::ingestForInstance(const QString& instancePath) const
{
	Bundle b;
	QString gameRoot = QDir(instancePath).filePath(".minecraft");

	// 1. latest.log
	QString latestLog = QDir(gameRoot).filePath("logs/latest.log");
	if (QFileInfo::exists(latestLog)) {
		b.combinedText += "==== logs/latest.log (tail) ====\n";
		b.combinedText += readFileTail(latestLog);
		b.combinedText += "\n\n";
		b.sources.append(latestLog);
		b.fromLatestLog = true;
	}

	// 2. most-recent crash report (if any)
	QString crashFile =
		latestCrashReport(QDir(gameRoot).filePath("crash-reports"));
	if (!crashFile.isEmpty()) {
		b.combinedText += "==== " + crashFile + " ====\n";
		b.combinedText += readFileTail(crashFile);
		b.combinedText += "\n\n";
		b.sources.append(crashFile);
		b.fromCrashReport = true;
	}

	// 3. Forge / Fabric specific debug log
	QString debugLog = QDir(gameRoot).filePath("logs/debug.log");
	if (QFileInfo::exists(debugLog)) {
		b.combinedText += "==== logs/debug.log (tail) ====\n";
		b.combinedText += readFileTail(debugLog, 256 * 1024);
		b.sources.append(debugLog);
	}

	return b;
}
