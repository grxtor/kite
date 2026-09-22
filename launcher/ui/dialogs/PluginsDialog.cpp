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

#include "PluginsDialog.h"
#include "ui_PluginsDialog.h"

#include <QHeaderView>
#include <QIcon>
#include <QSignalBlocker>
#include <QTreeWidgetItem>

#include "Application.h"
#include "BuildConfig.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginSignature.h"

namespace
{

	/* Sentinel role used to stash the module index on the tree item, so
	 * we can resolve a row back to PluginManager::modules() without
	 * relying on the order being identical. */
	constexpr int ModuleIndexRole = Qt::UserRole + 1;

	QString disableReasonLabel(PluginDisableReason r)
	{
		switch (r) {
			case PluginDisableReason::None:
				return QObject::tr("OK");
			case PluginDisableReason::UserDisabled:
				return QObject::tr("Disabled by user");
			case PluginDisableReason::SignatureRequired:
				return QObject::tr("Signature required");
			case PluginDisableReason::SignatureInvalid:
				return QObject::tr("Invalid signature");
			case PluginDisableReason::DependencyMissing:
				return QObject::tr("Missing dependency");
			case PluginDisableReason::DependencyCycle:
				return QObject::tr("Dependency cycle");
			case PluginDisableReason::SupersededByCore:
				return QObject::tr("Built into MeshMC");
			case PluginDisableReason::AbiTooOld:
				return QObject::tr("Plugin ABI too old");
			case PluginDisableReason::AbiTooNew:
				return QObject::tr("Plugin ABI too new");
		}
		return QString();
	}

	QString buildModuleHtml(const PluginMetadata& mod)
	{
		QString html;
		html += QLatin1String(
			"<style>"
			"table { width: 100%; border-collapse: collapse; }"
			"td { padding: 4px 8px; vertical-align: top; }"
			"td.label { font-weight: bold; white-space: nowrap; width: 1%; }"
			"hr { border: none; border-top: 1px solid #ccc; margin: 10px 0; }"
			"</style>"
			"<table>");

		auto row = [&](const QString& label, const QString& value) {
			if (value.isEmpty())
				return;
			html += QStringLiteral(
						"<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(label.toHtmlEscaped(), value.toHtmlEscaped());
		};

		row(QObject::tr("Name:"), mod.name);
		row(QObject::tr("Version:"), mod.version);
		row(QObject::tr("Author:"), mod.author);
		row(QObject::tr("License:"), mod.license);
		row(QObject::tr("Description:"), mod.description);

		if (!mod.codeLink.isEmpty()) {
			QString cell;
			if (mod.codeLink.startsWith(QLatin1String("http://")) ||
				mod.codeLink.startsWith(QLatin1String("https://"))) {
				cell = QStringLiteral("<a href=\"%1\">%1</a>")
						   .arg(mod.codeLink.toHtmlEscaped());
			} else {
				cell = mod.codeLink.toHtmlEscaped();
			}
			html += QStringLiteral(
						"<tr><td class=\"label\">%1</td><td>%2</td></tr>")
						.arg(QObject::tr("Source Code:").toHtmlEscaped(), cell);
		}

		// Signature
		row(QObject::tr("Signature:"),
			QString::fromUtf8(PluginSignature::stateLabel(mod.signatureState)));
		row(QObject::tr("Signing Key:"), mod.signatureFingerprint);
		row(QObject::tr("Signature Detail:"), mod.signatureDetail);

		// Dependencies
		if (!mod.dependencies.isEmpty()) {
			QStringList parts;
			for (const auto& d : mod.dependencies) {
				QString s = d.name;
				if (!d.minVersion.isEmpty())
					s += QStringLiteral(" >= %1").arg(d.minVersion);
				if (d.optional)
					s += QObject::tr(" (optional)");
				parts.append(s);
			}
			row(QObject::tr("Dependencies:"), parts.join(QStringLiteral(", ")));
		}

		// Disable / status info
		if (mod.disabled) {
			row(QObject::tr("Status:"),
				QStringLiteral("%1 — %2").arg(
					disableReasonLabel(mod.disableReason), mod.disableDetail));
		} else if (mod.initialized) {
			row(QObject::tr("Status:"), QObject::tr("Loaded"));
		} else {
			row(QObject::tr("Status:"), QObject::tr("Discovered (not loaded)"));
		}

		html += QLatin1String("</table>");
		return html;
	}

} // namespace

PluginsDialog::PluginsDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::PluginsDialog)
{
	ui->setupUi(this);

	auto* tree = ui->pluginsTree;
	tree->header()->setStretchLastSection(true);
	tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	populateTree();

	connect(ui->pluginsTree, &QTreeWidget::itemSelectionChanged, this,
			&PluginsDialog::onSelectionChanged);
	connect(ui->pluginsTree, &QTreeWidget::itemChanged, this,
			&PluginsDialog::onItemChanged);
	connect(ui->closeButton, &QPushButton::clicked, this,
			&PluginsDialog::close);

	if (tree->topLevelItemCount() > 0)
		tree->setCurrentItem(tree->topLevelItem(0));
}

PluginsDialog::~PluginsDialog()
{
	delete ui;
}

void PluginsDialog::populateTree()
{
	auto* pm = APPLICATION->pluginManager();
	auto* tree = ui->pluginsTree;
	const QSignalBlocker block(tree); // silence itemChanged during build

	tree->clear();
	if (!pm)
		return;

	const auto& modules = pm->modules();
	for (int i = 0; i < modules.size(); ++i) {
		const auto& mod = modules[i];

		auto* item = new QTreeWidgetItem(tree);
		item->setData(0, ModuleIndexRole, i);

		// A module core has absorbed can never be turned back on, so
		// don't offer a checkbox that would quietly do nothing.
		const bool superseded =
			mod.disableReason == PluginDisableReason::SupersededByCore;
		if (superseded) {
			item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
			item->setCheckState(0, Qt::Unchecked);
		} else {
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			// disabled-by-user → unchecked. Anything else (signed-out,
			// dep-missing, cycle) is shown unchecked too but the user can
			// still toggle the box; we only persist the "user wants it
			// disabled" bit.
			const bool userDisabled = pm->isModuleDisabled(mod.name);
			item->setCheckState(0, userDisabled ? Qt::Unchecked : Qt::Checked);
		}

		item->setText(1, mod.name +
							 (mod.version.isEmpty()
								  ? QString()
								  : QStringLiteral(" %1").arg(mod.version)));
		item->setText(2, QString::fromUtf8(
							 PluginSignature::stateLabel(mod.signatureState)));
		if (mod.disabled)
			item->setText(3, disableReasonLabel(mod.disableReason));
		else if (mod.initialized)
			item->setText(3, tr("Loaded"));
		else
			item->setText(3, tr("Discovered"));

		// Grey out disabled modules (but not the checkbox itself).
		if (mod.disabled) {
			QFont f = item->font(1);
			f.setItalic(true);
			for (int c = 1; c < tree->columnCount(); ++c)
				item->setFont(c, f);
		}
	}
}

void PluginsDialog::onSelectionChanged()
{
	const auto items = ui->pluginsTree->selectedItems();
	if (items.isEmpty()) {
		ui->pluginsText->clear();
		return;
	}
	bool ok = false;
	const int row = items.first()->data(0, ModuleIndexRole).toInt(&ok);
	if (!ok)
		return;
	showDetailsForRow(row);
}

void PluginsDialog::onItemChanged(QTreeWidgetItem* item, int column)
{
	if (column != 0 || !item)
		return;
	auto* pm = APPLICATION->pluginManager();
	if (!pm)
		return;

	bool ok = false;
	const int row = item->data(0, ModuleIndexRole).toInt(&ok);
	if (!ok)
		return;
	if (row < 0 || row >= pm->modules().size())
		return;
	if (pm->modules().at(row).disableReason ==
		PluginDisableReason::SupersededByCore)
		return;

	const QString name = pm->modules().at(row).name;
	const bool wantEnabled = (item->checkState(0) == Qt::Checked);
	pm->setModuleDisabled(name, !wantEnabled);
}

void PluginsDialog::showDetailsForRow(int row)
{
	auto* pm = APPLICATION->pluginManager();
	if (!pm || row < 0 || row >= pm->modules().size()) {
		ui->pluginsText->clear();
		return;
	}
	ui->pluginsText->setHtml(buildModuleHtml(pm->modules().at(row)));
}
