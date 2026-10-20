/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "BuildConfig.h"
#include "translations/TranslationsModel.h"

/*
 * Covers TranslationsModel: roleNames() -- the QML-facing addition
 * QmlShell::languages() hands to the QML shell's own onboarding language
 * picker (see QmlShell.h's languages Q_PROPERTY comment) -- the languages
 * compiled into the launcher (:/i18n/kite_<lang>.qm, see
 * launcher/CMakeLists.txt) and the data dir override, all without the
 * network: with an empty TRANSLATIONS_BASE_URL nothing is downloaded, which
 * downloadIndex() would otherwise show by dereferencing the (here absent)
 * LAUNCHER.
 */
class TranslationsModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		m_dir = std::make_unique<QTemporaryDir>();
		QVERIFY(m_dir->isValid());
		m_model = std::make_unique<TranslationsModel>(m_dir->path());
	}

	void cleanup()
	{
		m_model.reset();
		m_dir.reset();
	}

	void roleNamesExposeLanguageKeyNameAndCompleteness()
	{
		const auto roles = m_model->roleNames();
		QCOMPARE(roles.value(Qt::UserRole), QByteArray("languageKey"));
		QCOMPARE(roles.value(TranslationsModel::NameRole), QByteArray("name"));
		QCOMPARE(roles.value(TranslationsModel::CompletenessRole),
				 QByteArray("completeness"));
	}

	void englishIsListedFirstAndSelectedByDefault()
	{
		QVERIFY(m_model->rowCount() >= 1);
		QCOMPARE(keyAt(0), QStringLiteral("en_US"));
		QCOMPARE(m_model->selectedLanguage(), QStringLiteral("en_US"));
	}

	void nothingIsDownloadedWithoutABaseUrl()
	{
		QVERIFY(BuildConfig.TRANSLATIONS_BASE_URL.isEmpty());
		m_model->downloadIndex();
		m_model->updateLanguage(QStringLiteral("tr"));
	}

	void bundledLanguagesAreListedWithTheirNativeName()
	{
		if (!QFile::exists(":/i18n/kite_tr.qm")) {
			QSKIP("built without bundled translations");
		}
		const int row = rowOf(QStringLiteral("tr"));
		QVERIFY(row >= 0);
		const QModelIndex index = m_model->index(row);
		QCOMPARE(m_model->data(index, TranslationsModel::NameRole).toString(),
				 QStringLiteral("T\u00fcrk\u00e7e"));
		QVERIFY(!m_model->data(index, TranslationsModel::CompletenessRole)
					 .toString()
					 .isEmpty());
	}

	void bundledLanguageLoadsFromTheResource()
	{
		if (!QFile::exists(":/i18n/kite_tr.qm")) {
			QSKIP("built without bundled translations");
		}
		QVERIFY(m_model->selectLanguage(QStringLiteral("tr")));
		QCOMPARE(m_model->selectedLanguage(), QStringLiteral("tr"));
		QVERIFY(m_model->selectLanguage(QStringLiteral("en_US")));
	}

	void poFileInDataDirIsListedAndLoaded()
	{
		QFile po(QDir(m_dir->path()).filePath("xx.po"));
		QVERIFY(po.open(QIODevice::WriteOnly));
		po.write("msgid \"\"\nmsgstr \"Content-Type: text/plain; "
				 "charset=UTF-8\\n\"\n\n"
				 "msgctxt \"Ctx\"\nmsgid \"Settings\"\n"
				 "msgstr \"Ayarlar\"\n");
		po.close();

		// The directory watcher is asynchronous; reselecting reloads it.
		m_model.reset();
		m_model = std::make_unique<TranslationsModel>(m_dir->path());
		QVERIFY(rowOf(QStringLiteral("xx")) >= 0);
		QVERIFY(m_model->selectLanguage(QStringLiteral("xx")));
		QCOMPARE(QCoreApplication::translate("Ctx", "Settings"),
				 QStringLiteral("Ayarlar"));
		m_model->selectLanguage(QStringLiteral("en_US"));
		QCOMPARE(QCoreApplication::translate("Ctx", "Settings"),
				 QStringLiteral("Settings"));
	}

	void staleMmcQmInDataDirIsIgnored()
	{
		// What the old Project Tick download left behind.
		QFile qm(QDir(m_dir->path()).filePath("mmc_tr.qm"));
		QVERIFY(qm.open(QIODevice::WriteOnly));
		qm.close();
		m_model.reset();
		m_model = std::make_unique<TranslationsModel>(m_dir->path());
		if (QFile::exists(":/i18n/kite_tr.qm")) {
			QVERIFY(rowOf(QStringLiteral("tr")) >= 0);
		} else {
			QCOMPARE(rowOf(QStringLiteral("tr")), -1);
		}
	}

  private:
	QString keyAt(int row) const
	{
		return m_model->data(m_model->index(row), Qt::UserRole).toString();
	}

	int rowOf(const QString& key) const
	{
		for (int row = 0; row < m_model->rowCount(); ++row) {
			if (keyAt(row) == key) {
				return row;
			}
		}
		return -1;
	}

	std::unique_ptr<QTemporaryDir> m_dir;
	std::unique_ptr<TranslationsModel> m_model;
};

QTEST_GUILESS_MAIN(TranslationsModelTest)

#include "TranslationsModel_test.moc"
