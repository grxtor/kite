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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "BuildConfig.h"
#include "models/ScreenshotUploader.h"

class ScreenshotUploaderTest : public QObject
{
	Q_OBJECT
  private slots:

	void albumUrlPointsAtTheImgurAlbumPage()
	{
		QCOMPARE(ScreenshotUploader::albumUrl("abc123"),
				 QString("https://imgur.com/a/abc123"));
	}

	void availableFollowsTheBuildsImgurClientId()
	{
		ScreenshotUploader uploader;
		QCOMPARE(uploader.available(),
				 !BuildConfig.IMGUR_CLIENT_ID.trimmed().isEmpty());
	}

	void validateWantsAtLeastOneReadableFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString shot = dir.filePath("shot.png");
		QFile file(shot);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("png");
		file.close();

		QVERIFY(ScreenshotUploader::validate({shot}).isEmpty());
		QVERIFY(ScreenshotUploader::validate({shot, shot}).isEmpty());
		QVERIFY(!ScreenshotUploader::validate({}).isEmpty());
		const QString missing = ScreenshotUploader::validate({shot, dir.filePath("gone.png")});
		QVERIFY(missing.contains("gone.png"));
	}

	void uploadingNothingFailsOnceAndStaysIdle()
	{
		ScreenshotUploader uploader;
		QSignalSpy failed(&uploader, &ScreenshotUploader::failed);
		QSignalSpy uploaded(&uploader, &ScreenshotUploader::uploaded);
		uploader.upload({});
		QCOMPARE(failed.count(), 1);
		QCOMPARE(uploaded.count(), 0);
		QVERIFY(!uploader.busy());
	}

	// No launcher services (network) in a unit test: a valid file must be
	// refused with a message rather than crash.
	void uploadWithoutNetworkFailsCleanly()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString shot = dir.filePath("shot.png");
		QFile file(shot);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("png");
		file.close();

		ScreenshotUploader uploader;
		QSignalSpy failed(&uploader, &ScreenshotUploader::failed);
		uploader.upload({shot});
		QCOMPARE(failed.count(), 1);
		QVERIFY(!failed.first().first().toString().isEmpty());
		QVERIFY(!uploader.busy());
	}
};

QTEST_GUILESS_MAIN(ScreenshotUploaderTest)

#include "ScreenshotUploader_test.moc"
