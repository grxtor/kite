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

#include <QAbstractItemModel>
#include <QByteArray>

/* First role named @p name in @p model's roleNames(), or @p fallback (also if
 * @p model is null). */
inline int roleByName(const QAbstractItemModel* model, const QByteArray& name,
					  int fallback)
{
	if (!model) {
		return fallback;
	}
	const auto roles = model->roleNames();
	for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
		if (it.value() == name) {
			return it.key();
		}
	}
	return fallback;
}
