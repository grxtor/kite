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

#include "SetupWizard.h"

#include "LanguageWizardPage.h"
#include "JavaWizardPage.h"

#include "translations/TranslationsModel.h"
#include <Application.h>
#include <FileSystem.h>

#include <QAbstractButton>
#include <BuildConfig.h>

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent)
{
	setObjectName(QStringLiteral("SetupWizard"));
	resize(615, 659);
	// make it ugly everywhere to avoid variability in theming
	setWizardStyle(QWizard::ClassicStyle);
	setOptions(QWizard::NoCancelButton | QWizard::IndependentPages |
			   QWizard::HaveCustomButton1);

	retranslate();

	connect(this, &QWizard::currentIdChanged, this, &SetupWizard::pageChanged);
}

void SetupWizard::retranslate()
{
	setButtonText(QWizard::NextButton, tr("&Next >"));
	setButtonText(QWizard::BackButton, tr("< &Back"));
	setButtonText(QWizard::FinishButton, tr("&Finish"));
	setButtonText(QWizard::CustomButton1, tr("&Refresh"));
	setWindowTitle(tr("%1 Quick Setup").arg(BuildConfig.KITE_NAME));
}

BaseWizardPage* SetupWizard::getBasePage(int id)
{
	if (id == -1)
		return nullptr;
	auto pagePtr = page(id);
	if (!pagePtr)
		return nullptr;
	return dynamic_cast<BaseWizardPage*>(pagePtr);
}

BaseWizardPage* SetupWizard::getCurrentBasePage()
{
	return getBasePage(currentId());
}

void SetupWizard::pageChanged(int id)
{
	auto basePagePtr = getBasePage(id);
	if (!basePagePtr) {
		return;
	}
	if (basePagePtr->wantsRefreshButton()) {
		setButtonLayout({QWizard::CustomButton1, QWizard::Stretch,
						 QWizard::BackButton, QWizard::NextButton,
						 QWizard::FinishButton});
		auto customButton = button(QWizard::CustomButton1);
		connect(customButton, &QAbstractButton::pressed, [&]() {
			auto basePagePtr = getCurrentBasePage();
			if (basePagePtr) {
				basePagePtr->refresh();
			}
		});
	} else {
		setButtonLayout({QWizard::Stretch, QWizard::BackButton,
						 QWizard::NextButton, QWizard::FinishButton});
	}
}

void SetupWizard::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange) {
		retranslate();
	}
	QWizard::changeEvent(event);
}

SetupWizard::~SetupWizard() {}
