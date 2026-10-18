/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include <QIcon>
#include "Application.h"
#include "BuildConfig.h"

#include <net/NetJob.h>

#include "HoeDown.h"
#include "MMCStrings.h"

namespace
{
	// Credits
	QString getCreditsHtml()
	{
		QFile dataFile(":/documents/credits.html");
		if (!dataFile.open(QIODevice::ReadOnly)) {
			qWarning() << "Failed to open file" << dataFile.fileName()
					   << "for reading:" << dataFile.errorString();
			return {};
		}
		QString fileContent = QString::fromUtf8(dataFile.readAll());
		dataFile.close();

		return fileContent.arg(
			QObject::tr("%1 Developers").arg(BuildConfig.KITE_DISPLAYNAME),
			QObject::tr("MultiMC Developers"));
	}

	QString getLicenseHtml()
	{
		QFile dataFile(":/documents/COPYING.md");
		if (dataFile.open(QIODevice::ReadOnly)) {
			HoeDown hoedown;
			QString output = hoedown.process(dataFile.readAll());
			dataFile.close();
			return output;
		} else {
			qWarning() << "Failed to open file" << dataFile.fileName()
					   << "for reading:" << dataFile.errorString();
			return QString();
		}
	}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent), ui(new Ui::AboutDialog)
{
	ui->setupUi(this);

	QString launcherName = BuildConfig.KITE_DISPLAYNAME;

	setWindowTitle(tr("About %1").arg(launcherName));

	QString chtml = getCreditsHtml();
	ui->creditsText->setHtml(Strings::htmlListPatch(chtml));

	QString lhtml = getLicenseHtml();
	ui->licenseText->setHtml(Strings::htmlListPatch(lhtml));

	ui->urlLabel->setOpenExternalLinks(true);

	ui->icon->setPixmap(APPLICATION->getThemedIcon("logo").pixmap(64));
	ui->title->setText(launcherName);

	ui->versionLabel->setText(BuildConfig.printableVersionString());

	if (!BuildConfig.BUILD_PLATFORM.isEmpty())
		ui->platformLabel->setText(tr("Platform") + ": " +
								   BuildConfig.BUILD_PLATFORM);
	else
		ui->platformLabel->setVisible(false);

	if (!BuildConfig.GIT_COMMIT.isEmpty() &&
		BuildConfig.GIT_COMMIT != "GITDIR-NOTFOUND") {
		ui->commitLabel->setText(tr("Commit: %1").arg(BuildConfig.GIT_COMMIT));
	} else
		ui->commitLabel->setVisible(false);

	if (!BuildConfig.BUILD_DATE.isEmpty())
		ui->buildDateLabel->setText(
			tr("Build date: %1").arg(BuildConfig.BUILD_DATE));
	else
		ui->buildDateLabel->setVisible(false);

	if (!BuildConfig.VERSION_CHANNEL.isEmpty())
		ui->versionchannelLabel->setText(tr("Version Channel") + ": " +
								  BuildConfig.VERSION_CHANNEL);
	else
		ui->versionchannelLabel->setVisible(false);

	if (!BuildConfig.UPDATE_CHANNEL.isEmpty())
		ui->updatechannelLabel->setText(tr("Update Channel") + ": " +
								  BuildConfig.UPDATE_CHANNEL);
	else
		ui->updatechannelLabel->setVisible(false);

	
    ui->redistributionText->setHtml(tr(
"<p>We keep <b>Kite</b> open source because we believe it's important to be able to see the source code of a project like this, and we do this using the Apache license.</p>\n"
"<p>One reason we use the Apache license is that we don't want people using the name <b>Kite</b> when they fork the project. "
"This means people should examine the source code and remove all references to <b>Kite</b>, including the project icon and window titles (the title should not contain the phrase <b>Kite-fork</b>). "
"The Apache license covers reasonable use of the name; mentioning the project's origins in the About dialog and license is acceptable. However, it must be explicitly stated that the project is a fork, "
"which does not mean you have our approval.</p>\n<p>However, we give you the freedom to distribute this project as you wish, in any non-exclusive way, without changing its functionality, on a voluntary "
"basis to package managers, without expecting any financial gain. Take the project and distribute it wherever people can reach it. But abide by our restrictions."
    ));

	QString urlText(
		"<html><head/><body><p><a href=\"%1\">%1</a></p></body></html>");
	ui->urlLabel->setText(urlText.arg(BuildConfig.KITE_GIT));

	QString copyText("© 2026 %1");
	ui->copyLabel->setText(copyText.arg(BuildConfig.KITE_COPYRIGHT));

	connect(ui->closeButton, &QPushButton::clicked, this, &AboutDialog::close);

	connect(ui->aboutQt, &QPushButton::clicked, &QApplication::aboutQt);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
