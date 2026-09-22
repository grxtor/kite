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

#include <QFileIconProvider>

/*
 * Four icons: file, folder, and a link variant of each.
 *
 * QFileSystemModel's own provider asks the platform for the icon that
 * belongs to each individual file, which on Windows means a shell call
 * per entry - and the export dialogs point a file system model at a
 * whole instance directory, thousands of files deep. The result was a
 * dialog that took seconds to open and stuttered while scrolling, to
 * show icons nobody is picking files by.
 *
 * The style's standard pixmaps say the one thing the tree actually
 * needs - is this a folder or a file - and cost nothing.
 */
class FastFileIconProvider : public QFileIconProvider
{
  public:
	QIcon icon(const QFileInfo& info) const override;
};
