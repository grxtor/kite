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

#include "LogUploader.h"

#include <QFile>

#include "BuildConfig.h"
#include "GZip.h"
#include "core/LauncherContext.h"
#include "net/PasteUpload.h"
#include "settings/SettingsObject.h"

namespace
{
	const QString kBuiltinKeySetting = QStringLiteral("meshmc");
	const QString kPublicKey = QStringLiteral("public");
} // namespace

LogUploader::LogUploader(QObject* parent) : QObject(parent) {}

LogUploader::~LogUploader()
{
	if (m_task) {
		m_task->disconnect(this);
	}
}

QString LogUploader::resolveApiKey(const QString& setting,
								   const QString& builtinKey)
{
	if (setting == kBuiltinKeySetting) {
		return builtinKey.isEmpty() ? kPublicKey : builtinKey;
	}
	return setting.isEmpty() ? kPublicKey : setting;
}

QString LogUploader::validate(const QString& text, const QString& apiKey)
{
	if (text.trimmed().isEmpty()) {
		return tr("There is nothing in this log to upload.");
	}
	PasteUpload probe(text, apiKey);
	if (!probe.validateText()) {
		return tr("This log is too big to upload. Open the folder and share "
				  "the file by hand.");
	}
	return QString();
}

QString LogUploader::readLogFile(const QString& path, QString* error)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly)) {
		if (error) {
			*error = tr("The log file could not be read.");
		}
		return QString();
	}
	const QByteArray raw = file.readAll();
	if (!path.endsWith(QStringLiteral(".gz"))) {
		return QString::fromUtf8(raw);
	}
	QByteArray plain;
	if (!GZip::unzip(raw, plain)) {
		if (error) {
			*error = tr("The log file could not be read.");
		}
		return QString();
	}
	return QString::fromUtf8(plain);
}

void LogUploader::uploadText(const QString& text)
{
	if (m_task) {
		return;
	}
	start(text);
}

void LogUploader::uploadFile(const QString& path)
{
	if (m_task) {
		return;
	}
	QString error;
	const QString text = readLogFile(path, &error);
	if (text.isNull()) {
		emit failed(error);
		return;
	}
	start(text);
}

void LogUploader::start(const QString& text)
{
	auto* launcher = LauncherContext::instance();
	if (!launcher || !launcher->settings()) {
		emit failed(tr("Uploading is not available right now."));
		return;
	}
	const QString key = resolveApiKey(
		launcher->settings()->get(QStringLiteral("PasteEEAPIKey")).toString(),
		BuildConfig.PASTE_EE_KEY);
	const QString problem = validate(text, key);
	if (!problem.isEmpty()) {
		emit failed(problem);
		return;
	}

	auto* paste = new PasteUpload(text, key);
	m_task = Task::Ptr(paste);
	connect(paste, &Task::succeeded, this,
			[this, paste] { finish(true, paste->pasteLink()); });
	connect(paste, &Task::failed, this, [this](const QString& reason) {
		finish(false, reason);
	});
	emit busyChanged();
	paste->start();
}

void LogUploader::finish(bool ok, const QString& urlOrError)
{
	// Drop the task after this slot returns - it is still emitting.
	Task::Ptr done = std::move(m_task);
	m_task.reset();
	emit busyChanged();
	if (ok) {
		emit uploaded(urlOrError);
	} else {
		emit failed(urlOrError.isEmpty()
						? tr("The upload failed. Check your connection and try again.")
						: tr("The upload failed: %1").arg(urlOrError));
	}
}
