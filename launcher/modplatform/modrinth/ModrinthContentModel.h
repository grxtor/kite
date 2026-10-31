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

#include "modplatform/ContentProviderModel.h"

/* Modrinth search results. Everything except the shape of the JSON
 * lives in the base class. */
class ModrinthContentModel final : public ContentProviderModel
{
	Q_OBJECT

  public:
	explicit ModrinthContentModel(ModPlatform::ContentType contentType,
								  ModPlatform::SearchFilters filters,
								  QObject* parent = nullptr);

  protected:
	QList<ModPlatform::IndexedProject>
	parseSearchResponse(const QByteArray& bytes, int& totalHits) const override;

	ModPlatform::IndexedProject
	parseProjectResponse(const QByteArray& bytes) const override;

	QList<ModPlatform::ContentVersion>
	parseVersionsResponse(const QByteArray& bytes,
						  const ModPlatform::IndexedProject& project)
		const override;

	QString parseBodyResponse(const QByteArray& bytes) const override;

	QList<ModPlatform::Category>
	parseCategoriesResponse(const QByteArray& bytes) const override;

};
