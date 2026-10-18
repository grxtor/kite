/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "LanguageSelectionWidget.h"

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QLabel>
#include "Application.h"
#include "translations/TranslationsModel.h"

LanguageSelectionWidget::LanguageSelectionWidget(QWidget* parent)
	: QWidget(parent)
{
	verticalLayout = new QVBoxLayout(this);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	languageView = new QTreeView(this);
	languageView->setObjectName(QStringLiteral("languageView"));
	languageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	languageView->setAlternatingRowColors(true);
	languageView->setRootIsDecorated(false);
	languageView->setItemsExpandable(false);
	languageView->setWordWrap(true);
	languageView->header()->setCascadingSectionResizes(true);
	languageView->header()->setStretchLastSection(false);
	verticalLayout->addWidget(languageView);
	helpUsLabel = new QLabel(this);
	helpUsLabel->setObjectName(QStringLiteral("helpUsLabel"));
	helpUsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	helpUsLabel->setOpenExternalLinks(true);
	helpUsLabel->setWordWrap(true);
	verticalLayout->addWidget(helpUsLabel);

	auto translations = APPLICATION->translations();
	auto index = translations->selectedIndex();
	languageView->setModel(translations.get());
	languageView->setCurrentIndex(index);
	languageView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	languageView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	connect(languageView->selectionModel(),
			&QItemSelectionModel::currentRowChanged, this,
			&LanguageSelectionWidget::languageRowChanged);
	verticalLayout->setContentsMargins(0, 0, 0, 0);
}

QString LanguageSelectionWidget::getSelectedLanguageKey() const
{
	auto translations = APPLICATION->translations();
	return translations->data(languageView->currentIndex(), Qt::UserRole)
		.toString();
}

void LanguageSelectionWidget::retranslate()
{
	QString text = tr("Don't see your language or the quality is poor?<br/><a "
					  "href=\"%1\">Help us with translations!</a>")
					   .arg("https://github.com/grxtor/kite/wiki/"
							"Translating-Kite");
	helpUsLabel->setText(text);
}

void LanguageSelectionWidget::languageRowChanged(const QModelIndex& current,
												 const QModelIndex& previous)
{
	if (current == previous) {
		return;
	}
	auto translations = APPLICATION->translations();
	QString key = translations->data(current, Qt::UserRole).toString();
	translations->selectLanguage(key);
	translations->updateLanguage(key);
}
