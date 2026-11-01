/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QObject>
#include <QString>

#include "tasks/Task.h"

/*
 * QML-facing log upload (paste.ee via PasteUpload). One upload at a time;
 * uploadText()/uploadFile() while `busy` is a no-op. The outcome arrives as
 * exactly one of uploaded(url) or failed(message); copying the link or showing
 * a toast is the caller's. The PasteEEAPIKey setting "kite" means the
 * launcher's own key (BuildConfig.PASTE_EE_KEY). PasteUpload censors tokens and
 * the home directory out of the text.
 */
class LogUploader : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

  public:
	explicit LogUploader(QObject* parent = nullptr);
	~LogUploader() override;

	bool busy() const
	{
		return m_task != nullptr;
	}

	/// The key PasteUpload is given for the PasteEEAPIKey setting value
	/// @p setting: "kite" becomes @p builtinKey, an empty value becomes
	/// paste.ee's anonymous "public" key, anything else is the user's own.
	static QString resolveApiKey(const QString& setting,
								 const QString& builtinKey);
	/// Empty if @p text can be uploaded with @p apiKey; otherwise the
	/// reason, worded for the user (nothing to upload, or over the key's
	/// size limit - 2 MB for "public", 12 MB for a real key).
	static QString validate(const QString& text, const QString& apiKey);
	/// @p path's text, gzip-decompressed when it ends in ".gz". Null string
	/// with @p error set when it cannot be read.
	static QString readLogFile(const QString& path, QString* error);

	/// Uploads @p text. Reports through uploaded()/failed(), never inline.
	Q_INVOKABLE void uploadText(const QString& text);
	Q_INVOKABLE void uploadFile(const QString& path);

  signals:
	void busyChanged();
	void uploaded(const QString& url);
	void failed(const QString& message);

  private:
	void start(const QString& text);
	void finish(bool ok, const QString& urlOrError);

	Task::Ptr m_task;
};
