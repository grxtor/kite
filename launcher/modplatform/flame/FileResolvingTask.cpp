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

#include "FileResolvingTask.h"
#include "Json.h"
#include "modplatform/flame/FlameApi.h"

Flame::FileResolvingTask::FileResolvingTask(
	shared_qobject_ptr<QNetworkAccessManager> network,
	Flame::Manifest& toProcess)
	: m_network(network), m_toProcess(toProcess)
{
}

void Flame::FileResolvingTask::executeTask()
{
	setStatus(tr("Resolving mod IDs..."));
	setProgress(0, m_toProcess.files.size());
	m_dljob = new NetJob("Mod id resolver", m_network);
	results.resize(m_toProcess.files.size());
	int index = 0;
	for (auto& file : m_toProcess.files) {
		auto projectIdStr = QString::number(file.projectId);
		auto fileIdStr = QString::number(file.fileId);
		auto dl = Net::Download::makeByteArray(
			FlameApi::fileUrl(projectIdStr, fileIdStr), &results[index]);
		m_dljob->addNetAction(dl);
		index++;
	}
	connect(m_dljob.get(), &NetJob::finished, this,
			&Flame::FileResolvingTask::netJobFinished);
	propagateStepsFrom(m_dljob.get());
	m_dljob->start();
}

void Flame::FileResolvingTask::netJobFinished()
{
	int index = 0;
	int unresolved = 0;
	for (auto& bytes : results) {
		auto& out = m_toProcess.files[index];
		try {
			if (!out.parseFromBytes(bytes)) {
				unresolved++;
				qWarning() << "Resolving of" << out.projectId << out.fileId
						   << "failed: mod may have restricted downloads";
			}
		} catch (const JSONValidationError& e) {
			unresolved++;
			qCritical() << "Resolving of" << out.projectId << out.fileId
						<< "failed because of a parsing error:";
			qCritical() << e.cause();
			qCritical() << "JSON:";
			qCritical() << bytes;
		}
		index++;
	}
	if (unresolved > 0) {
		qWarning() << unresolved
				   << "mod(s) could not be resolved (restricted downloads). "
					  "They will be skipped.";
	}
	emitSucceeded();
}
