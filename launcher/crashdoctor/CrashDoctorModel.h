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
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class MinecraftInstance;

/*
 * One instance's crash doctor for QML (InstanceDetails::crashDoctor): the
 * causes of its last crash and the fixes for them. Fixer actions run here;
 * flows (install, update, upload) and pages are the Log tab's to run, with
 * the parameters given here.
 *
 * Each cause: ruleId, title, explanation, line (the log line that shows it),
 * count, rows (one line per finding of a grouped card), fixes. Each fix:
 * action, label, params, runner ("fixer", "flow", "navigation"), confirm
 * (the question to ask first, "" for none).
 */
class CrashDoctorModel : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool hasReport READ hasReport NOTIFY changed)
	Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
	Q_PROPERTY(QVariantList causes READ causes NOTIFY changed)
	Q_PROPERTY(QString crashReport READ crashReport NOTIFY changed)
	Q_PROPERTY(int exitCode READ exitCode NOTIFY changed)
	Q_PROPERTY(bool hasExitCode READ hasExitCode NOTIFY changed)
	Q_PROPERTY(QString undoAction READ undoAction NOTIFY undoChanged)

  public:
	explicit CrashDoctorModel(std::shared_ptr<MinecraftInstance> instance,
							  QObject* parent = nullptr);
	~CrashDoctorModel() override;

	bool hasReport() const;
	bool busy() const;
	QVariantList causes() const;
	/// The crash report file of the run (absolute), or "".
	QString crashReport() const;
	int exitCode() const;
	bool hasExitCode() const;
	/// The action the last fix of this instance was, while it can be undone.
	QString undoAction() const;

	/// Reads the last run's files again (logs/latest.log, crash report).
	Q_INVOKABLE void analyse();
	/// Runs fix @p fixIndex of cause @p causeIndex if it is a Fixer action;
	/// {ok, message}. Ask first when the fix has a confirm question.
	Q_INVOKABLE QVariantMap applyFix(int causeIndex, int fixIndex);
	/// Undoes the last fix; {ok, message}.
	Q_INVOKABLE QVariantMap undo();
	/// The report is set aside until the next crash.
	Q_INVOKABLE void dismiss();
	/// The mods folder keys (file names) of the files of @p modIds, for the
	/// install flow's replacements; "" for an id with no file.
	Q_INVOKABLE QStringList fileKeysFor(const QStringList& modIds) const;
	/// The file name of the crash report, or "".
	Q_INVOKABLE QString crashReportName() const;

  signals:
	void changed();
	void busyChanged();
	void undoChanged();

  private:
	std::shared_ptr<MinecraftInstance> m_instance;
	QString m_id;
};
