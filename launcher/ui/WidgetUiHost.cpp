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
#include <QDialog>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QVector>

#include "ui/dialogs/BlockedModsDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProfileSelectDialog.h"
#include "ui/dialogs/ProfileSetupDialog.h"
#include "ui/dialogs/UntrustedModsDialog.h"
#include "ui/dialogs/UpdateAvailableDialog.h"
#include "plugin/PluginUiRenderer.h"

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
	/* Indeterminate: the caller has nothing to report. Uncancellable too,
	 * unless the caller offers a skip (UiHost::showBusy()'s skipLabel), in
	 * which case the dialog's cancel button carries that label and calls
	 * it. */
	class ProgressDialogBusy final : public UiHost::BusyIndicator
	{
	  public:
		ProgressDialogBusy(const QString& text, const QString& skipLabel,
						   std::function<void()> onSkip)
			: m_dialog(text, QString(), 0, 0, QApplication::activeWindow())
		{
			m_dialog.setWindowTitle(text);
			m_dialog.setMinimumDuration(0);
			if (skipLabel.isEmpty() || !onSkip) {
				m_dialog.setCancelButton(nullptr);
			} else {
				m_dialog.setCancelButtonText(skipLabel);
				QObject::connect(&m_dialog, &QProgressDialog::canceled,
								 &m_dialog, std::move(onSkip));
			}
			m_dialog.adjustSize();
			m_dialog.show();
		}

	  private:
		QProgressDialog m_dialog;
	};
} // namespace

std::unique_ptr<UiHost::BusyIndicator>
WidgetUiHost::showBusy(const QString& text, const QString& skipLabel,
					   std::function<void()> onSkip)
{
	return std::make_unique<ProgressDialogBusy>(text, skipLabel,
												std::move(onSkip));
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

std::optional<QString> WidgetUiHost::askText(const QString& title,
											  const QString& text,
											  const QString& defaultValue)
{
	bool ok = false;
	QString result = QInputDialog::getText(activeWindow(), title, text,
											QLineEdit::Normal, defaultValue,
											&ok);
	if (!ok) {
		return std::nullopt;
	}
	return result;
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

UiHost::AccountChoice
WidgetUiHost::pickAccount(const QString& title, const QString& text,
						  const QList<MinecraftAccountPtr>& accounts)
{
	/* ProfileSelectDialog lists the launcher's own accounts rather than
	 * the ones passed in -- in practice the same list, which is what every
	 * caller passes; an account outside @p accounts counts as backing out.
	 * It has one message line, so the title leads it. */
	Q_UNUSED(text);
	ProfileSelectDialog dialog(title, ProfileSelectDialog::GlobalDefaultCheckbox,
							   activeWindow());
	AccountChoice choice;
	if (dialog.exec() != QDialog::Accepted) {
		return choice;
	}
	choice.index = accounts.indexOf(dialog.selectedAccount());
	choice.makeDefault = choice.index >= 0 && dialog.useAsGlobalDefault();
	return choice;
}

std::optional<UiHost::PluginModalResult>
WidgetUiHost::runPluginModal(const QString& title, const QJsonObject& document)
{
	QDialog dialog(activeWindow());
	dialog.setWindowTitle(title);
	auto* layout = new QVBoxLayout(&dialog);

	/* Any `click` event (a button, or a `link` if a plugin puts one in a
	 * modal document) closes the dialog with that node's id -- a modal is
	 * meant for small button-row prompts, not full pages. */
	QString clicked;
	auto renderer = PluginUiRenderer::build(
		QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact)),
		[&clicked, &dialog](const QString& nodeId, const QString& event,
							const QString& /*valueJson*/) {
			if (event == QLatin1String("click")) {
				clicked = nodeId;
				dialog.accept();
			}
		});
	layout->addWidget(renderer->rootWidget());

	if (dialog.exec() != QDialog::Accepted || clicked.isEmpty()) {
		return std::nullopt;
	}
	return PluginModalResult{ clicked, renderer->collectValues() };
}

bool WidgetUiHost::chooseOptionalMods(const QString& packName,
									  AtlOptionalModsModel& mods)
{
	/* The widget ATLauncher page asks with its own AtlOptionalModDialog
	 * and never installs through AtlModpackModel, the only caller of this;
	 * keep the pack's defaults rather than build a second dialog. */
	Q_UNUSED(packName);
	Q_UNUSED(mods);
	return false;
}

bool WidgetUiHost::setupProfile(MinecraftAccountPtr account)
{
	ProfileSetupDialog dialog(account, activeWindow());
	return dialog.exec() == QDialog::Accepted;
}

std::optional<QString> WidgetUiHost::pickFile(FilePickerMode mode,
											  const QString& title,
											  const QString& defaultPath,
											  const QString& filter)
{
	const QString result =
		mode == FilePickerMode::Open
			? QFileDialog::getOpenFileName(activeWindow(), title, QString(),
										   filter)
			: QFileDialog::getSaveFileName(activeWindow(), title, defaultPath,
										   filter);
	if (result.isEmpty()) {
		return std::nullopt;
	}
	return result;
}
