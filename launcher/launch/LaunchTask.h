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

#pragma once
#include <QProcess>
#include <QObjectPtr.h>
#include "LogModel.h"
#include "BaseInstance.h"
#include "MessageLevel.h"
#include "LoggedProcess.h"
#include "LaunchStep.h"

class LaunchTask : public Task
{
	Q_OBJECT
  protected:
	explicit LaunchTask(InstancePtr instance);
	void init();

  public:
	enum State { NotStarted, Running, Waiting, Failed, Aborted, Finished };

  public: /* methods */
	static shared_qobject_ptr<LaunchTask> create(InstancePtr inst);
	virtual ~LaunchTask() {};

	void appendStep(shared_qobject_ptr<LaunchStep> step);
	void prependStep(shared_qobject_ptr<LaunchStep> step);
	void setCensorFilter(QMap<QString, QString> filter);
	/// Also hides @p secret in the log from now on, whatever the filter says.
	void addCensorEntry(const QString& secret, const QString& replacement);
	void setWrapperCommand(QString wrapperCommand);

	InstancePtr instance()
	{
		return m_instance;
	}

	QString wrapperCommand() const
	{
		return m_wrapperCommand;
	}

	void setPid(qint64 pid)
	{
		m_pid = pid;
	}

	qint64 pid()
	{
		return m_pid;
	}

	/**
	 * @brief the profile id the game is played as, for the friends feature:
	 * set only when that is the signed-in account's own identity (an online
	 * launch, no demo), "" for anything else (offline, a demo, not known).
	 */
	void setPlayerProfile(const QString& profileId)
	{
		m_playerProfile = profileId;
	}

	QString playerProfile() const
	{
		return m_playerProfile;
	}

	/**
	 * @brief prepare the process for launch (for multi-stage launch)
	 */
	virtual void executeTask() override;

	/**
	 * @brief launch the armed instance
	 */
	void proceed();

	/**
	 * @brief abort launch
	 */
	bool abort() override;

	bool canAbort() const override;

	/**
	 * @brief whether an abort has already been requested and accepted
	 *
	 * Both this and a launch step that refuses to be interrupted make
	 * canAbort() false, and the two need different things said to the user.
	 */
	bool isAborting() const
	{
		return state == LaunchTask::Aborted;
	}

	shared_qobject_ptr<LogModel> getLogModel();

  public:
	QString substituteVariables(const QString& cmd) const;
	QString censorPrivateInfo(QString in);

  protected: /* methods */
	virtual void emitFailed(QString reason) override;
	virtual void emitSucceeded() override;

  signals:
	/**
	 * @brief emitted when the launch preparations are done
	 */
	void readyForLaunch();

	void requestProgress(Task* task);

	void requestLogging();

	/**
	 * @brief emitted for every line the launch prints, game output
	 * included, after private values are censored. For code that reads the
	 * log as it goes (the friends feature) without owning the LogModel.
	 */
	void logLineReceived(const QString& line);

  public slots:
	void onLogLines(const QStringList& lines,
					MessageLevel::Enum defaultLevel = MessageLevel::Kite);
	void onLogLine(QString line,
				   MessageLevel::Enum defaultLevel = MessageLevel::Kite);
	void onReadyForLaunch();
	void onStepFinished();
	void onProgressReportingRequested();

  private: /*methods */
	void finalizeSteps(bool successful, const QString& error);

	/* Start @p step and, for as long as it runs, relay its status()/
	 * progress() onto our own - the same way Update relays the update
	 * task it wraps onto itself. This is what lets something outside the
	 * step list (InstanceList's launch progress tracking, in particular)
	 * watch the launch as a whole instead of having to know which step
	 * is currently running. A step that names no stage of its own shows
	 * as "Preparing to launch", with no progress.
	 */
	void startStep(const shared_qobject_ptr<LaunchStep>& step);

  protected: /* data */
	InstancePtr m_instance;
	shared_qobject_ptr<LogModel> m_logModel;
	QList<shared_qobject_ptr<LaunchStep>> m_steps;
	QMap<QString, QString> m_censorFilter;
	QString m_wrapperCommand;
	int currentStep = -1;
	State state = NotStarted;
	qint64 m_pid = -1;
	QString m_playerProfile;

  private: /* data */
	QMetaObject::Connection m_stepStatusConnection;
	QMetaObject::Connection m_stepProgressConnection;
};
