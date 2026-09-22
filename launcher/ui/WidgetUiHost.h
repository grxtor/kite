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

#include "core/UiHost.h"

/*
 * Answers the core's questions with QtWidgets dialogs.
 *
 * Parented to whatever window is active at the moment the question is asked
 * rather than to a widget handed over in advance: the tasks that ask these
 * questions outlive any particular window, and the old code threaded a
 * QWidget* through the task API purely to have something to parent to.
 */
class WidgetUiHost final : public UiHost
{
  public:
	std::unique_ptr<BusyIndicator> showBusy(const QString& text) override;

	void message(const QString& title, const QString& text,
				 Severity severity) override;

	bool confirm(const QString& title, const QString& text,
				 Severity severity, const QString& acceptLabel = QString(),
				 const QString& rejectLabel = QString()) override;

	int choose(const QString& title, const QString& text, Severity severity,
			   const QStringList& actions) override;

	bool resolveBlockedMods(const QString& title, const QString& text,
							QList<BlockedMod>& mods) override;

	bool confirmUntrustedMods(const QStringList& suspectPaths) override;

	UpdateChoice offerUpdate(const QString& currentVersion,
							 const QString& availableVersion,
							 const QString& releaseNotes) override;
};
