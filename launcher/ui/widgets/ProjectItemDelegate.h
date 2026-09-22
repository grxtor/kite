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

#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

/* For ProjectItemRole: the data roles this delegate reads off the model's
 * data() when painting a row. Declared with the model rather than here,
 * since the model is what produces them. */
#include "modplatform/ContentProviderModel.h"

/* Draws one search result: optional checkbox, icon, title, description.
 *
 * Rows are sized by the view's iconSize rather than by this delegate, so
 * a view using it should set an icon size big enough for two lines of
 * description (48x48 is what the download dialog uses). The description
 * silently drops to a single elided line when the row is too short for
 * two, so a smaller icon size degrades gracefully instead of overflowing.
 *
 * The checkbox is painted only when the model supplies
 * Qt::CheckStateRole. Clicks inside it are swallowed and reported via
 * checkboxClicked() rather than going through setData(), so toggling a
 * row never disturbs the current selection or triggers the view's
 * double-click handler. */
class ProjectItemDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

  public:
	explicit ProjectItemDelegate(QWidget* parent = nullptr);

	void paint(QPainter* painter, const QStyleOptionViewItem& option,
			   const QModelIndex& index) const override;

	bool editorEvent(QEvent* event, QAbstractItemModel* model,
					 const QStyleOptionViewItem& option,
					 const QModelIndex& index) override;

  signals:
	void checkboxClicked(const QModelIndex& index);

  private:
	/* Where the checkbox goes: hard left, vertically centred. */
	QStyleOptionViewItem checkboxOption(const QStyleOptionViewItem& option,
										const QStyle* style) const;
};
