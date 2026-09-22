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

#include <QString>

/*
 * A pack file the provider's API refuses to hand out, which the user has to
 * fetch by hand.
 *
 * `found` is written by whoever is watching the download folder, so this
 * travels from the install task out to the user interface and back with the
 * answer filled in. It used to be declared inside BlockedModsDialog.h, which
 * meant the install task included a QDialog header to describe its own data.
 */
struct BlockedMod {
	int projectId;
	int fileId;
	QString fileName;
	QString targetPath;
	bool found = false;
};
