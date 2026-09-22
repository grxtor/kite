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

#include "ui/WidgetUiHost.h"

#include <QApplication>
#include <QAbstractButton>
#include <QMessageBox>
#include <QVector>

#include "ui/dialogs/BlockedModsDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/UntrustedModsDialog.h"

namespace
{
	QMessageBox::Icon toIcon(UiHost::Severity severity)
	{
		switch (severity) {
			case UiHost::Severity::Information:
				return QMessageBox::Information;
			case UiHost::Severity::Question:
				return QMessageBox::Question;
			case UiHost::Severity::Warning:
				return QMessageBox::Warning;
			case UiHost::Severity::Critical:
				return QMessageBox::Critical;
		}
		return QMessageBox::NoIcon;
	}

	QWidget* activeWindow()
	{
		return QApplication::activeWindow();
	}
} // namespace

void WidgetUiHost::message(const QString& title, const QString& text,
						   Severity severity)
{
	CustomMessageBox::selectable(activeWindow(), title, text,
								 toIcon(severity), QMessageBox::Ok,
								 QMessageBox::Ok)
		->exec();
}

bool WidgetUiHost::confirm(const QString& title, const QString& text,
						   Severity severity, const QString& acceptLabel,
						   const QString& rejectLabel)
{
	auto* box = CustomMessageBox::selectable(
		activeWindow(), title, text, toIcon(severity),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (!acceptLabel.isEmpty()) {
		if (auto* accept = box->button(QMessageBox::Yes)) {
			accept->setText(acceptLabel);
		}
	}
	if (!rejectLabel.isEmpty()) {
		if (auto* reject = box->button(QMessageBox::No)) {
			reject->setText(rejectLabel);
		}
	}

	return box->exec() == QMessageBox::Yes;
}

int WidgetUiHost::choose(const QString& title, const QString& text,
						 Severity severity, const QStringList& actions)
{
	auto* box =
		CustomMessageBox::selectable(activeWindow(), title, text,
									 toIcon(severity), QMessageBox::Cancel,
									 QMessageBox::Cancel);

	/* AcceptRole for every action: the roles would otherwise decide the
	 * button order for us, and the caller's order is the meaningful one. */
	QVector<QAbstractButton*> buttons;
	buttons.reserve(actions.size());
	for (const QString& action : actions) {
		buttons.append(box->addButton(action, QMessageBox::AcceptRole));
	}

	box->exec();

	const int index = buttons.indexOf(box->clickedButton());
	return index; /* -1 when Cancel or the window's close button was used */
}

bool WidgetUiHost::resolveBlockedMods(const QString& title,
									  const QString& text,
									  QList<BlockedMod>& mods)
{
	BlockedModsDialog dialog(activeWindow(), title, text, mods);
	return dialog.exec() == QDialog::Accepted;
}

bool WidgetUiHost::confirmUntrustedMods(const QStringList& suspectPaths)
{
	UntrustedModsDialog dialog(suspectPaths, activeWindow());
	return dialog.exec() == QDialog::Accepted;
}
