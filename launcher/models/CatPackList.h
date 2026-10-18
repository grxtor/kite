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

#include <QObject>
#include <QString>
#include <QVariantList>

/*
 * The cat pictures the launcher can show, for QML: the same packs the
 * classic Appearance page offers (the built-in ones, and anything the user
 * dropped into the data folder's "catpacks" folder). Which pack is in use
 * is the BackgroundCat setting and whether one shows at all is TheCat; both
 * are plain settings, so nothing here touches them.
 *
 * Seasonal pictures (Christmas, Halloween) are already picked by the pack
 * itself, so `source` is today's picture.
 */
class CatPackList : public QObject
{
	Q_OBJECT

	/// [{id, name, source}] in a stable order; `source` is a URL an Image
	/// can load (qrc: for the built-in ones, file: for the user's).
	Q_PROPERTY(QVariantList packs READ packs NOTIFY packsChanged)
	/// Where users put their own packs.
	Q_PROPERTY(QString folder READ folder CONSTANT)

  public:
	explicit CatPackList(QObject* parent = nullptr);
	~CatPackList() override;

	QVariantList packs() const
	{
		return m_packs;
	}
	QString folder() const;

	/// Looks for packs again, e.g. after the user added one to the folder.
	Q_INVOKABLE void refresh();

	/// The picture URL of pack @p id, or of the first pack if there is no
	/// such pack (a setting left over from a pack that was removed).
	Q_INVOKABLE QString sourceFor(const QString& id) const;

	/// A resource path (":/backgrounds/kitteh") or a file path as the URL
	/// QML loads it from.
	static QString toUrl(const QString& path);

	/// Every pack under @p folder as the rows of `packs`, with seasonal
	/// pictures chosen for @p date. Public so a test can point it at a
	/// temporary folder.
	static QVariantList list(const QString& folder, const QDate& date);

  signals:
	void packsChanged();

  private:
	QVariantList m_packs;
};
