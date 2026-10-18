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

#include "CrashReportDialog.h"

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef KITE_HAS_QRENCODE
#include <qrencode.h>
#endif

CrashReportDialog::CrashReportDialog(const QString& pasteLink,
									 const QString& logContent, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Kite Crash Report"));
	setMinimumSize(550, 450);

	auto* mainLayout = new QVBoxLayout(this);

	// Error message
	m_messageLabel = new QLabel(this);
	m_messageLabel->setText(
		tr("<h2 style='color: #cc0000;'>Kite has failed</h2>"
		   "<p>An unexpected error occurred and Kite had to close.</p>"
		   "<p>Your logs have been uploaded to paste.ee for "
		   "troubleshooting.</p>"));
	m_messageLabel->setAlignment(Qt::AlignCenter);
	m_messageLabel->setWordWrap(true);
	mainLayout->addWidget(m_messageLabel);

	// QR codes area
	auto* qrGroupBox = new QGroupBox(tr("QR Codes"), this);
	auto* qrLayout = new QHBoxLayout(qrGroupBox);

	// QR for full log text (truncated to fit QR capacity)
	// QR version 40 can hold ~4296 bytes; we keep the last 2000 chars
	// so the most recent (and most relevant) log lines are included.
	QString truncatedLog = logContent;
	if (truncatedLog.size() > 2000) {
		truncatedLog = tr("[...truncated...]\n") + logContent.right(2000);
	}

	auto* qrLogGroup = new QVBoxLayout();
	m_qrLogLabel = new QLabel(this);
	m_qrLogLabel->setAlignment(Qt::AlignCenter);
	m_qrLogLabel->setMinimumSize(200, 200);
	auto logQR = generateQRCode(truncatedLog, 200);
	if (!logQR.isNull()) {
		m_qrLogLabel->setPixmap(logQR);
	} else {
		m_qrLogLabel->setText(tr("[QR Code - requires libqrencode]"));
	}
	auto* logQRTitle = new QLabel(tr("Full Log"), this);
	logQRTitle->setAlignment(Qt::AlignCenter);
	qrLogGroup->addWidget(m_qrLogLabel);
	qrLogGroup->addWidget(logQRTitle);
	qrLayout->addLayout(qrLogGroup);

	// QR for paste.ee link
	auto* qrPasteGroup = new QVBoxLayout();
	m_qrPasteLabel = new QLabel(this);
	m_qrPasteLabel->setAlignment(Qt::AlignCenter);
	m_qrPasteLabel->setMinimumSize(200, 200);
	auto pasteQR = generateQRCode(pasteLink, 200);
	if (!pasteQR.isNull()) {
		m_qrPasteLabel->setPixmap(pasteQR);
	} else {
		m_qrPasteLabel->setText(tr("[QR Code - requires libqrencode]"));
	}
	auto* pasteQRTitle = new QLabel(tr("Paste.ee Log"), this);
	pasteQRTitle->setAlignment(Qt::AlignCenter);
	qrPasteGroup->addWidget(m_qrPasteLabel);
	qrPasteGroup->addWidget(pasteQRTitle);
	qrLayout->addLayout(qrPasteGroup);

	mainLayout->addWidget(qrGroupBox);

	// Paste.ee link
	m_pasteLinkLabel = new QLabel(this);
	m_pasteLinkLabel->setText(
		tr("Paste.ee link: <a href=\"%1\">%1</a>").arg(pasteLink));
	m_pasteLinkLabel->setOpenExternalLinks(true);
	m_pasteLinkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_pasteLinkLabel->setAlignment(Qt::AlignCenter);
	m_pasteLinkLabel->setWordWrap(true);
	mainLayout->addWidget(m_pasteLinkLabel);

	// Buttons
	auto* buttonLayout = new QHBoxLayout();

	auto* copyLinkBtn = new QPushButton(tr("Copy Link"), this);
	connect(copyLinkBtn, &QPushButton::clicked, this,
			[pasteLink]() { QApplication::clipboard()->setText(pasteLink); });
	buttonLayout->addWidget(copyLinkBtn);

	auto* openBtn = new QPushButton(tr("Open in Browser"), this);
	connect(openBtn, &QPushButton::clicked, this,
			[pasteLink]() { QDesktopServices::openUrl(QUrl(pasteLink)); });
	buttonLayout->addWidget(openBtn);

	auto* closeBtn = new QPushButton(tr("Close"), this);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
	buttonLayout->addWidget(closeBtn);

	mainLayout->addLayout(buttonLayout);
}

QPixmap CrashReportDialog::generateQRCode(const QString& text, int size)
{
#ifdef KITE_HAS_QRENCODE
	QRcode* qr = QRcode_encodeString(text.toUtf8().constData(), 0, QR_ECLEVEL_M,
									 QR_MODE_8, 1);
	if (!qr) {
		return QPixmap();
	}

	int qrWidth = qr->width;
	int scale = size / qrWidth;
	if (scale < 1)
		scale = 1;

	QImage image(qrWidth * scale, qrWidth * scale, QImage::Format_RGB32);
	image.fill(Qt::white);

	for (int y = 0; y < qrWidth; y++) {
		for (int x = 0; x < qrWidth; x++) {
			if (qr->data[y * qrWidth + x] & 1) {
				for (int sy = 0; sy < scale; sy++) {
					for (int sx = 0; sx < scale; sx++) {
						image.setPixel(x * scale + sx, y * scale + sy,
									   qRgb(0, 0, 0));
					}
				}
			}
		}
	}

	QRcode_free(qr);
	return QPixmap::fromImage(
		image.scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
#else
	Q_UNUSED(text)
	Q_UNUSED(size)
	return QPixmap();
#endif
}
