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
#include <java/JavaChecker.h>

class QWidget;

/**
 * Common UI bits for the java pages to use.
 */
namespace JavaCommon
{
	bool checkJVMArgs(QString args, QWidget* parent);

	// Show a dialog saying that the Java binary was not usable
	void javaBinaryWasBad(QWidget* parent, JavaCheckResult result);
	// Show a dialog saying that the Java binary was not usable because of bad
	// options
	void javaArgsWereBad(QWidget* parent, JavaCheckResult result);
	// Show a dialog saying that the Java binary was usable
	void javaWasOk(QWidget* parent, JavaCheckResult result);

	class TestCheck : public QObject
	{
		Q_OBJECT
	  public:
		TestCheck(QWidget* parent, QString path, QString args, int minMem,
				  int maxMem, int permGen)
			: m_parent(parent), m_path(path), m_args(args), m_minMem(minMem),
			  m_maxMem(maxMem), m_permGen(permGen)
		{
		}
		virtual ~TestCheck() {};

		void run();

	  signals:
		void finished();

	  private slots:
		void checkFinished(JavaCheckResult result);
		void checkFinishedWithArgs(JavaCheckResult result);

	  private:
		std::shared_ptr<JavaChecker> checker;
		QWidget* m_parent = nullptr;
		QString m_path;
		QString m_args;
		int m_minMem = 0;
		int m_maxMem = 0;
		int m_permGen = 64;
	};
} // namespace JavaCommon
