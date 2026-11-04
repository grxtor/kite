/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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
#include <QPointer>
#include <QString>

#include "minecraft/mod/DroppedContent.h"

class ContentBrowser;
class ContentInstallReview;
class ContentProviderModel;
class MinecraftInstance;

/*
 * Installs the project a dropped Modrinth or CurseForge link names: looks it
 * up, picks the newest version that fits the instance's Minecraft version and
 * loader, and installs it with its required dependencies, the way the browse
 * view does. It has a browser of its own, so the browse view's search and
 * filters stay as they were. Deletes itself after finished().
 */
class ContentLinkInstall : public QObject
{
	Q_OBJECT

  public:
	ContentLinkInstall(MinecraftInstance* instance,
					   DroppedContent::ProjectLink link,
					   QObject* parent = nullptr);

	void start();

  signals:
	/// @p text is a sentence for a toast.
	void finished(bool ok, const QString& text);

  private:
	void onSearchState();
	void onVersions();
	void onReviewState();
	void finish(bool ok, const QString& text);

	MinecraftInstance* m_instance;
	DroppedContent::ProjectLink m_link;
	ContentBrowser* m_browser = nullptr;
	QPointer<ContentProviderModel> m_model;
	QPointer<ContentInstallReview> m_review;
	QString m_name;
	int m_row = -1;
	bool m_searched = false;
	bool m_versionsHandled = false;
	bool m_done = false;
};
