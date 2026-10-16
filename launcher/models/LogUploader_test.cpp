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

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "GZip.h"
#include "models/LogUploader.h"

namespace
{
bool writeFile(const QString& path, const QByteArray& contents)
{
	QFile file(path);
	return file.open(QIODevice::WriteOnly) &&
		   file.write(contents) == contents.size();
}
} // namespace

class LogUploaderTest : public QObject
{
	Q_OBJECT
  private slots:

	void meshmcSettingBecomesTheBuiltinKey()
	{
		QCOMPARE(LogUploader::resolveApiKey("meshmc", "builtin"), QString("builtin"));
		QCOMPARE(LogUploader::resolveApiKey("mine", "builtin"), QString("mine"));
	}

	void emptyKeysFallBackToThePublicKey()
	{
		QCOMPARE(LogUploader::resolveApiKey("", "builtin"), QString("public"));
		QCOMPARE(LogUploader::resolveApiKey("meshmc", ""), QString("public"));
	}

	void validateAcceptsAnOrdinaryLog()
	{
		QVERIFY(LogUploader::validate("[12:00:00] Starting\n", "public").isEmpty());
	}

	void validateRefusesAnEmptyLog()
	{
		QVERIFY(!LogUploader::validate("", "public").isEmpty());
		QVERIFY(!LogUploader::validate("  \n\t", "mine").isEmpty());
	}

	// paste.ee allows 2 MB anonymously and 12 MB with a key (see
	// PasteUpload::maxSize()): the same 3 MB log passes for one only.
	void validateAppliesTheSizeLimitOfTheKey()
	{
		const QString big = QStringLiteral("a log line\n").repeated(3 * 1024 * 1024 / 11 + 1);
		QVERIFY(!LogUploader::validate(big, "public").isEmpty());
		QVERIFY(LogUploader::validate(big, "mine").isEmpty());
		const QString huge = QStringLiteral("a log line\n").repeated(13 * 1024 * 1024 / 11 + 1);
		QVERIFY(!LogUploader::validate(huge, "mine").isEmpty());
	}

	void readLogFileReadsPlainText()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("latest.log");
		QVERIFY(writeFile(path, "hello\nworld\n"));
		QString error;
		QCOMPARE(LogUploader::readLogFile(path, &error), QString("hello\nworld\n"));
		QVERIFY(error.isEmpty());
	}

	void readLogFileUnzipsGzFiles()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QByteArray packed;
		QVERIFY(GZip::zip("zipped log\n", packed));
		const QString path = dir.filePath("2026-01-01-1.log.gz");
		QVERIFY(writeFile(path, packed));
		QString error;
		QCOMPARE(LogUploader::readLogFile(path, &error), QString("zipped log\n"));
	}

	void readLogFileReportsAMissingFile()
	{
		QString error;
		const QString text = LogUploader::readLogFile("/nonexistent/x.log", &error);
		QVERIFY(text.isNull());
		QVERIFY(!error.isEmpty());
	}

	// Error path: no launcher services in a unit test, so an upload cannot
	// start - it must say so once, not crash, and not stay busy.
	void uploadWithoutLauncherServicesFailsCleanly()
	{
		LogUploader uploader;
		QSignalSpy failed(&uploader, &LogUploader::failed);
		QSignalSpy uploaded(&uploader, &LogUploader::uploaded);
		uploader.uploadText("some log");
		QCOMPARE(failed.count(), 1);
		QCOMPARE(uploaded.count(), 0);
		QVERIFY(!uploader.busy());
		QVERIFY(!failed.first().first().toString().isEmpty());
	}

	void uploadFileOfAMissingFileFails()
	{
		LogUploader uploader;
		QSignalSpy failed(&uploader, &LogUploader::failed);
		uploader.uploadFile("/nonexistent/x.log");
		QCOMPARE(failed.count(), 1);
		QVERIFY(!uploader.busy());
	}
};

QTEST_GUILESS_MAIN(LogUploaderTest)

#include "LogUploader_test.moc"
