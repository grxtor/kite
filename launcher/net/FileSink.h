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
#include "Sink.h"
#include <QSaveFile>

namespace Net
{
	class FileSink : public Sink
	{
	  public: /* con/des */
		FileSink(QString filename);
		virtual ~FileSink();

	  public: /* methods */
		JobStatus init(QNetworkRequest& request) override;
		JobStatus write(QByteArray& data) override;
		JobStatus abort() override;
		JobStatus finalize(QNetworkReply& reply) override;
		bool hasLocalData() override;

	  protected: /* methods */
		virtual JobStatus initCache(QNetworkRequest&);
		virtual JobStatus finalizeCache(QNetworkReply& reply);

	  protected: /* data */
		QString m_filename;
		bool wroteAnyData = false;
		/* Bytes handed to the file, so finalize() can tell a complete transfer
		 * from one the server cut short. Counted here because a QSaveFile reports
		 * on its staging file. */
		qint64 m_bytesWritten = 0;
		std::unique_ptr<QSaveFile> m_output_file;
	};
} // namespace Net
