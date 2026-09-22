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

#include <QDialog>
#include <QString>

#include <memory>

#include "ui/FastFileIconProvider.h"
#include "tasks/Task.h"

class FileIgnoreProxy;
class MinecraftInstance;

namespace Ui
{
	class ExportPackDialog;
}

/*
 * Exporting an instance as a modpack for one of the catalogues.
 *
 * The plain-zip export (ExportInstanceDialog) writes the instance out as
 * it is. A pack is a different thing: it carries a manifest naming the
 * mods it expects to be downloaded on install, so it needs a name, a
 * version and - since it is going to be published - a say in what does
 * and does not go in it. Hence the same checkable tree as the zip
 * export, sharing its `.packignore`, but rooted at the game directory
 * rather than the instance directory: nothing above `.minecraft` has any
 * meaning to another launcher.
 *
 * One dialog serves both formats because the questions are nearly the
 * same. The differences are that Modrinth publishes a summary and
 * CurseForge does not, CurseForge records an author and a recommended
 * memory figure and Modrinth does not, and only Modrinth requires the
 * version field to be filled in. Those fields are hidden rather than
 * disabled for the format that has no use for them - a greyed-out box
 * still asks a question - and what the user typed is remembered per
 * instance either way.
 */
class ExportPackDialog : public QDialog
{
	Q_OBJECT

  public:
	/*
	 * Which pack format is being written.
	 *
	 * Deliberately not the `"modrinth"` / `"curseforge"` strings the rest
	 * of the launcher identifies providers by: this is not a lookup into
	 * anything, it picks a layout and an output format, and a switch the
	 * compiler checks is worth more here than a string that agrees with
	 * ContentApi::id().
	 */
	enum class Format { ModrinthPack, CurseForgePack };

	explicit ExportPackDialog(MinecraftInstance* instance,
							  QWidget* parent = nullptr,
							  Format format = Format::ModrinthPack);
	~ExportPackDialog() override;

	void done(int result) override;

  private:
	/* Ask where the pack should go, honouring the format's extension.
	 * Empty when the user changed their mind. */
	QString askForOutputPath(const QString& packName);

	/* Build the export task for the current format, already filled in
	 * from the dialog. */
	std::unique_ptr<Task> buildExportTask(const QString& packName,
										  const QString& output);

	/* Persist what was typed, so the next export of this instance starts
	 * where this one left off. */
	void saveInputs();

	QString ignoreFileName() const;

  private:
	Ui::ExportPackDialog* ui;
	MinecraftInstance* m_instance;
	FileIgnoreProxy* m_proxyModel;
	FastFileIconProvider m_icons;
	const Format m_format;

  private slots:
	/* The Ok button is only meaningful once the manifest can be
	 * generated. */
	void validate();
};
