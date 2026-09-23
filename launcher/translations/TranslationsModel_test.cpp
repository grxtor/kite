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

#include <QTemporaryDir>
#include <QTest>

#include "translations/TranslationsModel.h"

/*
 * Covers TranslationsModel::roleNames() -- the QML-facing addition
 * QmlShell::languages() hands to the QML shell's own onboarding language
 * picker (see QmlShell.h's languages Q_PROPERTY comment) -- without touching
 * the network (downloadIndex() is never called): the constructor's
 * reloadLocalFiles() only reads a local, empty directory here, same as
 * IconList_test.cpp does for IconList::roleNames().
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

	void builtinLanguageIsSelectedByDefault()
	{
		// Only "en_US" exists until an index download or local files add
		// more (see reloadLocalFiles()) -- so the model starts with exactly
		// one row, and its languageKey role is that default.
		QCOMPARE(m_model->rowCount(), 1);
		const QModelIndex index = m_model->index(0);
		QCOMPARE(m_model->data(index, Qt::UserRole).toString(),
				 QStringLiteral("en_US"));
	}

  private:
	std::unique_ptr<QTemporaryDir> m_dir;
	std::unique_ptr<TranslationsModel> m_model;
};

QTEST_GUILESS_MAIN(TranslationsModelTest)

#include "TranslationsModel_test.moc"
