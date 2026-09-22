/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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
#include <QModelIndex>
#include <memory>

#include "ui/FastFileIconProvider.h"

class BaseInstance;
class FileIgnoreProxy;
typedef std::shared_ptr<BaseInstance> InstancePtr;

namespace Ui
{
	class ExportInstanceDialog;
}

/*
 * Exporting an instance as a plain zip.
 *
 * The tree is the instance directory with a checkbox on every entry;
 * what the user unchecks is remembered in the instance's `.packignore`
 * so the next export starts where the last one left off. Files nobody
 * would want in an export - logs, caches, `.DS_Store` - are not offered
 * as a choice at all; see FileIgnoreProxy for why the two kinds of
 * exclusion are kept apart.
 *
 * The writing itself is a task (MMCZip::ExportToZipTask) shown in a
 * progress dialog: it used to run inline, which froze the window for as
 * long as the instance took to compress and left no way to stop it.
 */
class ExportInstanceDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit ExportInstanceDialog(InstancePtr instance,
								  QWidget* parent = nullptr);
	~ExportInstanceDialog() override;

	void done(int result) override;

  private:
	/* Runs the export and closes the dialog with the result. Never
	 * leaves the dialog open on failure without saying why. */
	void doExport();
	QString ignoreFileName();

  private:
	Ui::ExportInstanceDialog* ui;
	InstancePtr m_instance;
	FileIgnoreProxy* m_proxyModel;
	FastFileIconProvider m_icons;

  private slots:
	void rowsInserted(QModelIndex parent, int top, int bottom);
};
