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
#include <QProgressDialog>
#include <QVector>

#include "ui/dialogs/BlockedModsDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/UntrustedModsDialog.h"
#include "ui/dialogs/UpdateAvailableDialog.h"

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

namespace
{
	/* Indeterminate and uncancellable: the caller has nothing to report and
	 * nothing it could abort. */
	class ProgressDialogBusy final : public UiHost::BusyIndicator
	{
	  public:
		explicit ProgressDialogBusy(const QString& text)
			: m_dialog(text, QString(), 0, 0, QApplication::activeWindow())
		{
			m_dialog.setWindowTitle(text);
			m_dialog.setMinimumDuration(0);
			m_dialog.setCancelButton(nullptr);
			m_dialog.adjustSize();
			m_dialog.show();
		}

	  private:
		QProgressDialog m_dialog;
	};
} // namespace

std::unique_ptr<UiHost::BusyIndicator>
WidgetUiHost::showBusy(const QString& text)
{
	return std::make_unique<ProgressDialogBusy>(text);
}

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

UiHost::UpdateChoice WidgetUiHost::offerUpdate(const QString& currentVersion,
											   const QString& availableVersion,
											   const QString& releaseNotes)
{
	UpdateAvailableDialog dialog(currentVersion, availableVersion,
								 releaseNotes, activeWindow());
	switch (dialog.exec()) {
		case UpdateAvailableDialog::Install:
			return UpdateChoice::Install;
		case UpdateAvailableDialog::Skip:
			return UpdateChoice::Skip;
		default:
			/* DontInstall, or the window was simply closed -- both leave
			 * the offer standing for next time. */
			return UpdateChoice::Later;
	}
}
