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

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "screenshots/Screenshot.h"
#include "tasks/Task.h"

/*
 * QML-facing Imgur upload for screenshots - the widget-free replacement for
 * ScreenshotsPage::on_actionUpload_triggered(), which runs the same
 * ImgurUpload / ImgurAlbumCreation net actions behind a modal
 * ProgressDialog and message boxes.
 *
 * One file is uploaded on its own and reported as that picture's link;
 * several are uploaded one by one and gathered into an album, reported as
 * the album's link. The outcome arrives as exactly one of uploaded(url,
 * count) or failed(message); copying the link and saying so is the caller's.
 *
 * `available` is false in a build whose Imgur client id is empty (a local
 * build without one): uploads cannot work there, so the page hides its
 * Upload action rather than offering one that always fails.
 */
class ScreenshotUploader : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool available READ available CONSTANT)
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

  public:
	explicit ScreenshotUploader(QObject* parent = nullptr);
	~ScreenshotUploader() override;

	/// Whether this build has an Imgur client id to upload with.
	bool available() const;
	bool busy() const
	{
		return m_task != nullptr;
	}

	/// The album page for Imgur album id @p albumId.
	static QString albumUrl(const QString& albumId);
	/// Empty if every path in @p paths is a readable file and there is at
	/// least one; otherwise the reason, worded for the user.
	static QString validate(const QStringList& paths);

	/// Uploads the pictures at @p paths (one: a link to it; several: an
	/// album). Reports through uploaded()/failed(), never inline. A call
	/// while `busy` is ignored.
	Q_INVOKABLE void upload(const QStringList& paths);

  signals:
	void busyChanged();
	/// @p url is the picture's (or album's) link, @p count how many pictures
	/// were uploaded.
	void uploaded(const QString& url, int count);
	/// @p message is a sentence fit to show the user.
	void failed(const QString& message);

  private:
	void finish(bool ok, const QString& urlOrError, int count);

	Task::Ptr m_task;
};
