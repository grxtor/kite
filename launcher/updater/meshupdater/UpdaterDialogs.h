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

#include <QDialog>

#include "GitHubRelease.h"
#include "Version.h"

class QTreeWidgetItem;

namespace Ui
{
	class SelectReleaseDialog;
}

/*! "Which release would you like?" Shown for --select-ui and whenever something other than the
 * newest release is to be installed. A tree rather than a combo box, so the release notes pane
 * updates with the selection. */
class SelectReleaseDialog : public QDialog
{
	Q_OBJECT

  public:
	SelectReleaseDialog(const Version& currentVersion,
						const QList<GitHubRelease>& releases,
						QWidget* parent = nullptr);
	~SelectReleaseDialog() override;

	/*! The release the user settled on; invalid when cancelled or accepted with nothing selected
	 * (the caller checks, no invented default). */
	GitHubRelease selectedRelease() const
	{
		return m_selectedRelease;
	}

  private slots:
	void selectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

  private:
	void loadReleases();
	void appendRelease(const GitHubRelease& release);
	GitHubRelease releaseForItem(const QTreeWidgetItem* item) const;

	QList<GitHubRelease> m_releases;
	GitHubRelease m_selectedRelease;
	Version m_currentVersion;

	Ui::SelectReleaseDialog* ui;
};

/*! "Which of these files?" Reached only when one release carries several artifacts that match this
 * installation; guessing is worse than asking, since they differ in what the user ends up with.
 * Reuses the release dialog's layout with the notes pane hidden. */
class SelectReleaseAssetDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit SelectReleaseAssetDialog(const QList<GitHubReleaseAsset>& assets,
									  QWidget* parent = nullptr);
	~SelectReleaseAssetDialog() override;

	GitHubReleaseAsset selectedAsset() const
	{
		return m_selectedAsset;
	}

  private slots:
	void selectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

  private:
	void loadAssets();
	void appendAsset(const GitHubReleaseAsset& asset);
	GitHubReleaseAsset assetForItem(const QTreeWidgetItem* item) const;

	QList<GitHubReleaseAsset> m_assets;
	GitHubReleaseAsset m_selectedAsset;

	Ui::SelectReleaseDialog* ui;
};
