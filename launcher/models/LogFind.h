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

class QAbstractItemModel;

/*
 * "Find" in a log list: which rows of a list model contain some text.
 *
 * LogTab.qml walks matches with next/previous and highlights them, so what
 * it needs from C++ is the list of matching row numbers - cheap to compute
 * here over a live log of tens of thousands of lines, slow in JavaScript.
 */
namespace LogFind
{
	/// Rows of @p model (a flat list) whose string value for @p role
	/// contains @p needle, ignoring case, in ascending order. Empty for a
	/// null model or an empty @p needle.
	QList<int> matchingRows(const QAbstractItemModel* model, int role,
							const QString& needle);
} // namespace LogFind
