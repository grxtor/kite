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
#include <QString>
#include <QStringList>

#include "modplatform/BlockedMod.h"

/*
 * Questions the core needs answered by a human.
 *
 * Work that runs outside the user interface sometimes has to stop and ask --
 * an install task finding files the provider will not serve, or an update
 * that would overwrite an instance. Those decisions used to be taken by
 * constructing a QDialog inside the task, which put QtWidgets in the
 * dependency graph of code that has nothing to do with drawing.
 *
 * So the core states the question and the shell answers it. What the answer
 * looks like on screen -- a message box today, something in QML later -- is
 * none of the core's business.
 *
 * ON BLOCKING: these are synchronous on purpose. The callers are decision
 * points in the middle of a task, branching immediately on the answer, and
 * making them asynchronous would mean restructuring the control flow of
 * modpack installation for no gain today -- the widget implementation blocks
 * either way. The interface says nothing about how the answer is obtained,
 * so a future implementation is free to pump an event loop or to be called
 * from a worker thread. That cost is real and deferred, not hidden: a
 * blocking call reached from the GUI thread runs a nested event loop, with
 * the re-entrancy that implies.
 */
class UiHost
{
  public:
	enum class Severity { Information, Question, Warning, Critical };

	virtual ~UiHost() = default;

	/* Something the user only has to acknowledge. */
	virtual void message(const QString& title, const QString& text,
						 Severity severity) = 0;

	/* A two-way decision. True means the user agreed to go ahead.
	 *
	 * The labels are optional: leaving them empty gets the platform's own
	 * wording. Naming the actions is better where the question is not a
	 * plain yes/no -- "Remove saves" and "Keep saves" say what will happen,
	 * where "Yes" and "No" make the reader re-read the question. */
	virtual bool confirm(const QString& title, const QString& text,
						 Severity severity,
						 const QString& acceptLabel = QString(),
						 const QString& rejectLabel = QString()) = 0;

	/* A decision with more than two answers. Returns the index into
	 * `actions`, or -1 when the user backed out. */
	virtual int choose(const QString& title, const QString& text,
					   Severity severity, const QStringList& actions) = 0;

	/* Files the provider's API will not serve, which the user fetches by
	 * hand while the host watches the download folder. `mods` is updated in
	 * place. False means the user gave up. */
	virtual bool resolveBlockedMods(const QString& title, const QString& text,
									QList<BlockedMod>& mods) = 0;

	/* Files that failed the trust check. True means install them anyway. */
	virtual bool confirmUntrustedMods(const QStringList& suspectPaths) = 0;

	enum class UpdateChoice { Install, Later, Skip };

	/* A release being offered: the current version, the version on offer,
	 * and its release notes. Kept separate from choose() because the notes
	 * are formatted content -- Markdown today -- and a plain message-box
	 * body would flatten that formatting. */
	virtual UpdateChoice offerUpdate(const QString& currentVersion,
									 const QString& availableVersion,
									 const QString& releaseNotes) = 0;
};
