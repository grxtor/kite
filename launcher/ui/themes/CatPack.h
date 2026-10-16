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

#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

class CatPack
{
  public:
	virtual ~CatPack() {}
	virtual QString id() = 0;
	virtual QString name() = 0;
	virtual QString path() = 0;
	/// path() as it would be on @p date, for the seasonal pictures. Packs
	/// with only one picture return it whatever the date.
	virtual QString pathOn(const QDate& date)
	{
		Q_UNUSED(date);
		return path();
	}
};

class BasicCatPack : public CatPack
{
  public:
	BasicCatPack(const QString& id, const QString& name);

	QString id() override;
	QString name() override;
	QString path() override;
	QString pathOn(const QDate& date) override;

  private:
	QString m_id;
	QString m_name;
};

class FileCatPack : public CatPack
{
  public:
	explicit FileCatPack(const QFileInfo& fileInfo);

	QString id() override;
	QString name() override;
	QString path() override;

  private:
	QFileInfo m_fileInfo;
};

struct JsonCatPackVariant {
	QString path;
	int startMonth;
	int startDay;
	int endMonth;
	int endDay;
};

class JsonCatPack : public CatPack
{
  public:
	explicit JsonCatPack(const QFileInfo& manifestInfo);

	QString id() override;
	QString name() override;
	QString path() override;
	QString pathOn(const QDate& date) override;

  private:
	QString m_id;
	QString m_name;
	QString m_defaultPath;
	QList<JsonCatPackVariant> m_variants;
};

/*!
 * Every cat pack there is: the four that ship with MeshMC, then whatever is
 * in @p folder (the data folder's "catpacks"): loose pictures matching
 * @p imageFilters (e.g. "*.png"), and sub-folders holding either a
 * catpack.json or pictures. A pack whose id is already taken is left out,
 * and a manifest that cannot be read is skipped with a warning. Shared by
 * the classic ThemeManager and the QML shell's CatPackList.
 */
std::vector<std::unique_ptr<CatPack>>
discoverCatPacks(const QDir& folder, const QStringList& imageFilters);
