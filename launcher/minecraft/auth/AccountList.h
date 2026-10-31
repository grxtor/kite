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

#include "MinecraftAccount.h"

#include <QObject>
#include <QVariant>
#include <QAbstractListModel>
#include <QSharedPointer>
#include <QTimer>

/*!
 * List of available Mojang accounts.
 * This should be loaded in the background by Kite on startup.
 */
class AccountList : public QAbstractListModel
{
	Q_OBJECT
  public:
	enum ModelRoles { PointerRole = 0x34B1CB48 };

	// Roles for the QML account list: a QML ListView binds to column 0 only, so
	// these return the data of ProfileNameColumn, TypeColumn and StatusColumn
	// for any column. No role hands QML a raw pointer.
	enum QmlRoles {
		NameRole = Qt::UserRole + 10,
		ProfileNameRole,
		TypeRole,
		StatusRole,

		// IsMSARole is an unambiguous flag (TypeRole is a display string).
		// StateKeyRole is a stable, non-localized key for StatusRole's
		// translated string. AccountIdRole is the id for AccountFaceProvider
		// (image://accountface/<accountId>): the profile id, or the internalId
		// when there is none, as QmlShell::accountFace() does.
		IsDefaultRole,
		IsMSARole,
		StateKeyRole,
		AccountIdRole
	};

	enum VListColumns {
		// TODO: Add icon column.
		NameColumn = 0,
		ProfileNameColumn,
		TypeColumn,
		StatusColumn,

		NUM_COLUMNS
	};

	explicit AccountList(QObject* parent = 0);
	virtual ~AccountList() noexcept;

	const MinecraftAccountPtr at(int i) const;
	int count() const;

	QVariant data(const QModelIndex& index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation,
								int role) const override;
	virtual int rowCount(const QModelIndex& parent) const override;
	virtual int columnCount(const QModelIndex& parent) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value,
						 int role) override;
	QHash<int, QByteArray> roleNames() const override;

	void addAccount(const MinecraftAccountPtr account);
	void removeAccount(QModelIndex index);
	int findAccountByProfileId(const QString& profileId) const;
	/*!
	 * Returns the index of the first **offline** account whose offlineUsername
	 * matches \a username case-insensitively, or -1. MSA accounts are skipped.
	 */
	int findOfflineAccountByUsername(const QString& username) const;
	MinecraftAccountPtr
	getAccountByProfileName(const QString& profileName) const;
	QStringList profileNames() const;

	// requesting a refresh pushes it to the front of the queue
	void requestRefresh(QString accountId);
	// queuing a refresh will let it go to the back of the queue (unless it's
	// somewhere inside the queue already)
	void queueRefresh(QString accountId);
	// whether the account's refresh still waits in the queue; the list is
	// idle (isActive()) between two refreshes while it does
	bool isRefreshQueued(const QString& accountId) const
	{
		return m_refreshQueue.contains(accountId);
	}

	/*!
	 * Sets the path to load/save the list file from/to. If autosave is true the
	 * list saves whenever it changes. THIS FUNCTION DOES NOT LOAD THE LIST:
	 * call loadList() right after, or an autosave may overwrite the list you
	 * meant to load.
	 */
	void setListFilePath(QString path, bool autosave = false);

	bool loadList();
	bool loadV3(QJsonObject& root);
	bool saveList();

	MinecraftAccountPtr defaultAccount() const;
	void setDefaultAccount(MinecraftAccountPtr profileId);
	bool anyAccountIsValid();

	/// Whether a Microsoft account that owns Minecraft is in the list; offline
	/// accounts depend on it.
	bool anyMicrosoftAccountOwnsMinecraft();

	bool isActive() const;

  protected:
	void beginActivity();
	void endActivity();

  private:
	const char* m_name;
	uint32_t m_activityCount = 0;
  signals:
	void listChanged();
	void listActivityChanged();
	void defaultAccountChanged();
	void activityChanged(bool active);

  public slots:
	/** Called when one of the accounts changes. */
	void accountChanged();

	/** Called when a refresh/login task for the account starts or ends. */
	void accountActivityChanged(bool active);

	/**
	 * This is initially to run background account refresh tasks, or on a hourly
	 * timer
	 */
	void fillQueue();

  private slots:
	void tryNext();

	void authSucceeded();
	void authFailed(QString reason);

  protected:
	QList<QString> m_refreshQueue;
	QTimer* m_refreshTimer;
	QTimer* m_nextTimer;
	shared_qobject_ptr<AccountTask> m_currentTask;

	/*! Called whenever the list changes; emits listChanged() and autosaves. */
	void onListChanged();

	/*!
	 * Called whenever the active account changes; emits defaultAccountChanged()
	 * and autosaves.
	 */
	void onDefaultAccountChanged();

	QList<MinecraftAccountPtr> m_accounts;

	MinecraftAccountPtr m_defaultAccount;

	//! Path to the account list file. Empty string if there isn't one.
	QString m_listFilePath;

	/*! Autosave when the list changes; ignored if the path is blank. */
	bool m_autosave = false;
};
