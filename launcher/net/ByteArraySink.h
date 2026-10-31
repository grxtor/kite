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

namespace Net
{
	/* Sink for downloads into an external QByteArray it does not own. */
	class ByteArraySink : public Sink
	{
	  public:
		ByteArraySink(QByteArray* output)
			: m_output(output) {
			  };

		virtual ~ByteArraySink()
		{
		}

	  public:
		JobStatus init(QNetworkRequest& request) override
		{
			m_output->clear();
			if (initAllValidators(request))
				return Job_InProgress;
			return Job_Failed;
		};

		JobStatus write(QByteArray& data) override
		{
			m_output->append(data);
			if (writeAllValidators(data))
				return Job_InProgress;
			return Job_Failed;
		}

		JobStatus abort() override
		{
			m_output->clear();
			failAllValidators();
			return Job_Failed;
		}

		JobStatus finalize(QNetworkReply& reply) override
		{
			if (finalizeAllValidators(reply))
				return Job_Finished;
			return Job_Failed;
		}

		bool hasLocalData() override
		{
			return false;
		}

	  private:
		QByteArray* m_output;
	};
} // namespace Net
