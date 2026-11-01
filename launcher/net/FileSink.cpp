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

#include "FileSink.h"
#include <QFile>
#include <QFileInfo>
#include "FileSystem.h"

namespace Net
{

	FileSink::FileSink(QString filename) : m_filename(filename)
	{
	}

	FileSink::~FileSink()
	{
	}

	JobStatus FileSink::init(QNetworkRequest& request)
	{
		auto result = initCache(request);
		if (result != Job_InProgress) {
			return result;
		}
		if (!FS::ensureFilePathExists(m_filename)) {
			qCritical() << "Could not create folder for " + m_filename;
			return Job_Failed;
		}
		wroteAnyData = false;
		/* Per attempt: a redirect or retry starts a new file, and carrying the
		 * old count over would compare two transfers' sum against one length. */
		m_bytesWritten = 0;
		m_output_file.reset(new QSaveFile(m_filename));
		if (!m_output_file->open(QIODevice::WriteOnly)) {
			qCritical() << "Could not open " + m_filename + " for writing";
			return Job_Failed;
		}

		if (initAllValidators(request))
			return Job_InProgress;
		return Job_Failed;
	}

	JobStatus FileSink::initCache(QNetworkRequest&)
	{
		return Job_InProgress;
	}

	JobStatus FileSink::write(QByteArray& data)
	{
		if (!writeAllValidators(data) ||
			m_output_file->write(data) != data.size()) {
			qCritical() << "Failed writing into " + m_filename;
			m_output_file->cancelWriting();
			m_output_file.reset();
			wroteAnyData = false;
			return Job_Failed;
		}
		wroteAnyData = true;
		m_bytesWritten += data.size();
		return Job_InProgress;
	}

	JobStatus FileSink::abort()
	{
		/* A failed write already dropped the file and Download calls abort()
		 * next, so a missing file is normal here. */
		if (m_output_file) {
			m_output_file->cancelWriting();
			m_output_file.reset();
		}
		failAllValidators();
		return Job_Failed;
	}

	JobStatus FileSink::finalize(QNetworkReply& reply)
	{
		bool gotFile = false;
		QVariant statusCodeV =
			reply.attribute(QNetworkRequest::HttpStatusCodeAttribute);
		bool validStatus = false;
		int statusCode = statusCodeV.toInt(&validStatus);
		if (validStatus) {
			// leaves out 304 Not Modified
			gotFile = statusCode == 200 || statusCode == 203;
		}

		/* An error response's body is not the file: committing it would write an
		 * error page under the download's name and, via the metacache, mark it
		 * fresh. 304 keeps its meaning (no body, local copy stands). Everything
		 * else is refused, including a 206 slice (no range was requested). */
		if (validStatus && !gotFile && statusCode != 304) {
			qCritical() << "Refusing to store the body of HTTP" << statusCode
						<< "as" << m_filename;
			failAllValidators();
			if (m_output_file) {
				m_output_file->cancelWriting();
				m_output_file.reset();
			}
			return Job_Failed;
		}
		/* A failed write already cancelled the file; nothing to commit. */
		if ((gotFile || wroteAnyData) && !m_output_file) {
			qCritical() << "Nothing left to commit for" << m_filename;
			failAllValidators();
			return Job_Failed;
		}
		// Commit if any data was written; an empty file is fine if the status
		// said we got one.
		if (gotFile || wroteAnyData) {
			/* Refuse a transfer the server cut short. Qt reports "finished" even if
			 * the body is truncated, and committing it would mark a broken file fresh
			 * in the metacache so the download is never retried.
			 *
			 * Only checked with a Content-Length and no Content-Encoding: chunked
			 * transfer has no length, and with an encoding the length counts encoded
			 * bytes while we count decoded ones. */
			if (wroteAnyData && !reply.hasRawHeader("Content-Encoding")) {
				const QVariant lengthHeader =
					reply.header(QNetworkRequest::ContentLengthHeader);
				bool haveLength = false;
				const qint64 expected = lengthHeader.toLongLong(&haveLength);
				if (haveLength && expected > 0 &&
					m_bytesWritten != expected) {
					qCritical()
						<< "Incomplete download for" << m_filename << "- got"
						<< m_bytesWritten << "of" << expected << "bytes";
					failAllValidators();
					m_output_file->cancelWriting();
					m_output_file.reset();
					return Job_Failed;
				}
			}

			// Validators run only for real downloads, not cache hits.
			if (!finalizeAllValidators(reply))
				return Job_Failed;
			if (!m_output_file->commit()) {
				qCritical() << "Failed to commit changes to " << m_filename;
				m_output_file->cancelWriting();
				return Job_Failed;
			}
		}
		m_output_file.reset();

		return finalizeCache(reply);
	}

	JobStatus FileSink::finalizeCache(QNetworkReply&)
	{
		return Job_Finished;
	}

	bool FileSink::hasLocalData()
	{
		QFileInfo info(m_filename);
		return info.exists() && info.size() != 0;
	}
} // namespace Net
