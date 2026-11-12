/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QByteArray>
#include <QString>

class BaseInstance;

/*
 * An instance imported with Link plays in another launcher's game folder
 * (LinkedGameDir). Whatever copies such an instance (Duplicate, the .zip
 * export) makes one that stands on its own: the linked folder's files come
 * along as a Copy import takes them (ImportCopy.h: the player's things, never
 * the other launcher's own files or sign-in), and the copy's instance.cfg loses
 * the link.
 */
namespace LauncherImport
{
	/* @p instance's game folder when it lies outside the instance's own
	 * folder (it is linked); "" otherwise. */
	QString outsideGameRoot(const BaseInstance& instance);

	/* The instance.cfg text @p config without the link. */
	QByteArray withoutLink(const QByteArray& config);
} // namespace LauncherImport
