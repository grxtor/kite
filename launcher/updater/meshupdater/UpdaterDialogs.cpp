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

#include "UpdaterDialogs.h"
#include "ui_SelectReleaseDialog.h"

#include <QHeaderView>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>

#include "HoeDown.h"
#include "MMCStrings.h"

namespace
{

	//! Column layout shared by both dialogs: a wide name, a snug date.
	void setUpTree(QTreeWidget* tree, const QString& firstColumn,
				   const QString& secondColumn)
	{
		tree->setColumnCount(2);
		tree->setHeaderLabels({firstColumn, secondColumn});
		tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
		tree->header()->setStretchLastSection(false);
	}

	//! Items carry the id of what they stand for; ids are stable, rows are not.
	constexpr int kIdRole = Qt::UserRole;

} // namespace

SelectReleaseDialog::SelectReleaseDialog(const Version& currentVersion,
										 const QList<GitHubRelease>& releases,
										 QWidget* parent)
	: QDialog(parent), m_releases(releases), m_currentVersion(currentVersion),
	  ui(new Ui::SelectReleaseDialog)
{
	ui->setupUi(this);

	ui->changelogTextBrowser->setOpenExternalLinks(true);
	ui->changelogTextBrowser->setLineWrapMode(QTextBrowser::WidgetWidth);
	ui->changelogTextBrowser->setVerticalScrollBarPolicy(
		Qt::ScrollBarAsNeeded);

	setUpTree(ui->versionsTree, tr("Version"), tr("Published Date"));

	ui->explainLabel->setText(tr("Select a version to install.\n"
								 "\n"
								 "Currently installed version: %1")
								  .arg(m_currentVersion.toString()));

	loadReleases();

	connect(ui->versionsTree, &QTreeWidget::currentItemChanged, this,
			&SelectReleaseDialog::selectionChanged);

	// The button box is wired to accept/reject in the .ui file; the labels are set here so they go
	// through our translations.
	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
	ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

SelectReleaseDialog::~SelectReleaseDialog()
{
	delete ui;
}

void SelectReleaseDialog::loadReleases()
{
	for (const GitHubRelease& release : m_releases) {
		appendRelease(release);
	}
}

void SelectReleaseDialog::appendRelease(const GitHubRelease& release)
{
	auto* item = new QTreeWidgetItem(ui->versionsTree);
	item->setText(0, release.tagName);
	item->setText(1, QLocale().toString(release.publishedAt.toLocalTime(),
										QLocale::ShortFormat));
	item->setData(0, kIdRole, QVariant::fromValue(release.id));
	item->setExpanded(true);

	ui->versionsTree->addTopLevelItem(item);
}

GitHubRelease
SelectReleaseDialog::releaseForItem(const QTreeWidgetItem* item) const
{
	if (!item)
		return {};

	const qint64 id = item->data(0, kIdRole).toLongLong();
	for (const GitHubRelease& release : m_releases) {
		if (release.id == id)
			return release;
	}
	return {};
}

void SelectReleaseDialog::selectionChanged(QTreeWidgetItem* current,
										   QTreeWidgetItem* /*previous*/)
{
	m_selectedRelease = releaseForItem(current);

	HoeDown markdown;
	ui->changelogTextBrowser->setHtml(Strings::htmlListPatch(
		markdown.process(m_selectedRelease.body.toUtf8())));
}

SelectReleaseAssetDialog::SelectReleaseAssetDialog(
	const QList<GitHubReleaseAsset>& assets, QWidget* parent)
	: QDialog(parent), m_assets(assets), ui(new Ui::SelectReleaseDialog)
{
	ui->setupUi(this);

	setUpTree(ui->versionsTree, tr("Version"), tr("Published Date"));

	ui->explainLabel->setText(tr("Select a version to install."));

	// An asset has no release notes; the pane would only ever be blank.
	ui->changelogTextBrowser->setHidden(true);

	loadAssets();

	connect(ui->versionsTree, &QTreeWidget::currentItemChanged, this,
			&SelectReleaseAssetDialog::selectionChanged);

	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
	ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}

SelectReleaseAssetDialog::~SelectReleaseAssetDialog()
{
	delete ui;
}

void SelectReleaseAssetDialog::loadAssets()
{
	for (const GitHubReleaseAsset& asset : m_assets) {
		appendAsset(asset);
	}
}

void SelectReleaseAssetDialog::appendAsset(const GitHubReleaseAsset& asset)
{
	auto* item = new QTreeWidgetItem(ui->versionsTree);
	item->setText(0, asset.name);
	item->setText(1, QLocale().toString(asset.updatedAt.toLocalTime(),
										QLocale::ShortFormat));
	item->setData(0, kIdRole, QVariant::fromValue(asset.id));
	item->setExpanded(true);

	ui->versionsTree->addTopLevelItem(item);
}

GitHubReleaseAsset
SelectReleaseAssetDialog::assetForItem(const QTreeWidgetItem* item) const
{
	if (!item)
		return {};

	const qint64 id = item->data(0, kIdRole).toLongLong();
	for (const GitHubReleaseAsset& asset : m_assets) {
		if (asset.id == id)
			return asset;
	}
	return {};
}

void SelectReleaseAssetDialog::selectionChanged(QTreeWidgetItem* current,
												QTreeWidgetItem* /*previous*/)
{
	m_selectedAsset = assetForItem(current);
}
