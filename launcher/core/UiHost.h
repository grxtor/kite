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

#include <functional>
#include <memory>
#include <optional>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "minecraft/auth/MinecraftAccount.h"

class AtlOptionalModsModel;
#include "modplatform/BlockedMod.h"

/* Questions the core needs answered by a human; the core states them and the
 * shell answers, so no QtWidgets enters the core. These are synchronous on
 * purpose: callers branch on the answer mid-task, and a blocking call from the
 * GUI thread runs a nested event loop, with the re-entrancy that implies. */
class UiHost
{
  public:
	/* Shows that the launcher has not frozen while the caller blocks on
	 * something it cannot report progress for. The indication disappears when
	 * this object is destroyed, so an early return cannot leave it showing. */
	class BusyIndicator
	{
	  public:
		virtual ~BusyIndicator() = default;
	};

	enum class Severity { Information, Question, Warning, Critical };

	virtual ~UiHost() = default;

	/* Not a question. For work that blocks without being able to say how far
	 * along it is. With @p skipLabel set it also offers a button with that
	 * label calling @p onSkip ("Play offline" while an account refreshes); the
	 * caller decides what skipping means. Stays up until the returned object
	 * is destroyed. */
	virtual std::unique_ptr<BusyIndicator> showBusy(
		const QString& text, const QString& skipLabel = QString(),
		std::function<void()> onSkip = {}) = 0;

	/* Something the user only has to acknowledge. */
	virtual void message(const QString& title, const QString& text,
						 Severity severity) = 0;

	/* A two-way decision; true means the user agreed. The labels are optional:
	 * empty gets the platform's own wording. Naming the actions ("Remove
	 * saves" / "Keep saves") reads better than "Yes" / "No" where the question
	 * is not plain yes/no. */
	virtual bool confirm(const QString& title, const QString& text,
						 Severity severity,
						 const QString& acceptLabel = QString(),
						 const QString& rejectLabel = QString()) = 0;

	/* A decision with more than two answers. Returns the index into
	 * `actions`, or -1 when the user backed out. */
	virtual int choose(const QString& title, const QString& text,
					   Severity severity, const QStringList& actions) = 0;

	/* pickAccount()'s answer: an index into the accounts given (-1 when the
	 * user backed out) and whether to make it the default from now on. */
	struct AccountChoice {
		int index = -1;
		bool makeDefault = false;
	};

	/* Which of @p accounts to play with, plus a "make this the default" option
	 * (asked at launch when no account is the default). Making it the default is the caller's job; this only
	 * reports the wish. */
	virtual AccountChoice pickAccount(
		const QString& title, const QString& text,
		const QList<MinecraftAccountPtr>& accounts) = 0;

	/* A single line of free text, pre-filled with @p defaultValue. Returns the
	 * entered text, or nullopt if the user backed out. An empty string is a
	 * real answer; callers that want a fallback for it apply it themselves. */
	virtual std::optional<QString> askText(
		const QString& title, const QString& text,
		const QString& defaultValue = QString()) = 0;

	/* Files the provider's API will not serve, which the user fetches by
	 * hand while the host watches the download folder. `mods` is updated in
	 * place. False means the user gave up. */
	virtual bool resolveBlockedMods(const QString& title, const QString& text,
									QList<BlockedMod>& mods) = 0;

	/* Files that failed the trust check. True means install them anyway. */
	virtual bool confirmUntrustedMods(const QStringList& suspectPaths) = 0;

	enum class UpdateChoice { Install, Later, Skip };

	/* A release being offered: the current version, the version on offer and
	 * its release notes. Separate from choose() because the notes are
	 * formatted content (Markdown), which a plain message-box body would
	 * flatten. */
	virtual UpdateChoice offerUpdate(const QString& currentVersion,
									 const QString& availableVersion,
									 const QString& releaseNotes) = 0;

	/* A Microsoft account that owns Minecraft but has no profile (username)
	 * yet: checks name availability live as the user types and creates the
	 * profile once they accept. True
	 * once the profile has been created; false if the user backed out. */
	virtual bool setupProfile(MinecraftAccountPtr account) = 0;

	enum class FilePickerMode { Open, Save };

	/* A file to open, or a location/name to save to, for the plugin SDK's file
	 * dialogs. @p filter is a Qt filter string ("Images (*.png *.jpg)", groups
	 * separated by ";;"); @p defaultPath only matters for FilePickerMode::Save
	 * (a suggested filename). Returns the chosen path, or nullopt if
	 * cancelled. */
	virtual std::optional<QString> pickFile(FilePickerMode mode,
											const QString& title,
											const QString& defaultPath,
											const QString& filter) = 0;

	/* A plugin's one-off prompt (the plugin SDK's ui_modal_run):
	 * @p document is an "mmco-ui/1" node tree, drawn with the shell's own
	 * components. Clicking any node that reports a click (a button,
	 * usually) answers it. */
	struct PluginModalResult {
		QString clicked;
		/// The value of every input node at that moment, keyed by node id:
		/// toggles (bool), text fields (string), number fields (double),
		/// choices (the chosen option's value).
		QJsonObject fields;
	};
	/// nullopt when the user backed out instead.
	virtual std::optional<PluginModalResult> runPluginModal(
		const QString& title, const QJsonObject& document) = 0;

	/* Which of an ATLauncher pack's optional mods to install, while it
	 * installs. @p mods is ticked in place, under its own rules (see
	 * AtlOptionalModsModel). True means install what is ticked now; false
	 * means keep the pack's own defaults. */
	virtual bool chooseOptionalMods(const QString& packName,
									AtlOptionalModsModel& mods) = 0;
};
