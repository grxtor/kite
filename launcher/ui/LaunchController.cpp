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

#include "ui/LaunchController.h"
#include "minecraft/auth/AccountList.h"
#include "Application.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginHooks.h"

#include "ui/MainWindow.h"
#include "ui/InstanceWindow.h"
#include "ui/dialogs/ProfileSelectDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/EditAccountDialog.h"
#include "ui/dialogs/ProfileSetupDialog.h"

#include <QEventLoop>
#include <QStringList>
#include <QHostInfo>
#include <QList>
#include <QHostAddress>
#include <QPushButton>

#include "BuildConfig.h"
#include "ui/JavaCommon.h"
#include "tasks/Task.h"
#include "minecraft/auth/AccountTask.h"
#include "launch/steps/CreateBackup.h"
#include "launch/steps/TextPrint.h"

LaunchController::LaunchController(QObject* parent) : Task(parent) {}

void LaunchController::executeTask()
{
	if (!m_instance) {
		emitFailed(tr("No instance specified!"));
		return;
	}

	const QString jvmArgsWarning = JavaCommon::jvmArgsWarning(
		m_instance->settings()->get("JvmArgs").toString());
	if (!jvmArgsWarning.isEmpty()) {
		LAUNCHER->uiHost()->message(tr("JVM arguments warning"),
									jvmArgsWarning,
									UiHost::Severity::Warning);
	}

	login();
}

void LaunchController::decideAccount()
{
	auto accounts = APPLICATION->accounts();

	/* Demo was asked for up front (the Launch Demo entry), so there is no
	 * account question to ask: login() skips authentication entirely. All
	 * that is missing is a name to show in game. Borrowing the default
	 * account's profile name keeps the demo from being called "User" for
	 * someone who does have an account. */
	if (m_demoMode) {
		if (m_demoUsername.isEmpty()) {
			auto account = accounts->defaultAccount();
			QString profileName = account ? account->profileName() : QString();
			m_demoUsername = profileName.isEmpty() ? tr("User") : profileName;
		}
		return;
	}

	if (m_accountToUse) {
		return;
	}

	// Find an account to use.
	if (accounts->count() <= 0) {
		// Tell the user they need to log in at least one account in order to
		// play.
		const bool wantsAccountManager = LAUNCHER->uiHost()->confirm(
			tr("No Accounts"),
			tr("In order to play Minecraft, you must have at least one "
			   "Microsoft "
			   "account logged in."
			   "Would you like to open the account manager to add an "
			   "account now?"),
			UiHost::Severity::Information);

		if (wantsAccountManager) {
			// Open the account manager.
			APPLICATION->ShowGlobalSettings(m_parentWidget, "accounts");
		} else {
			// Offer demo mode as an alternative
			const bool wantsDemo = LAUNCHER->uiHost()->confirm(
				tr("No Account — Play Demo?"),
				tr("<b>No Microsoft account is linked.</b><br><br>"
				   "Without a Microsoft account you cannot play the full "
				   "version of Minecraft.<br><br>"
				   "<b>Demo Mode</b> lets you try Minecraft with the "
				   "following limitations:<br>"
				   "&nbsp;&bull;&nbsp;Only a small area of the world is "
				   "accessible<br>"
				   "&nbsp;&bull;&nbsp;Progress is not saved after the demo "
				   "ends<br>"
				   "&nbsp;&bull;&nbsp;Multiplayer is not available<br><br>"
				   "Would you like to launch Minecraft in Demo Mode?"),
				UiHost::Severity::Question, tr("Play Demo"), tr("Cancel"));

			if (wantsDemo) {
				auto username = LAUNCHER->uiHost()->askText(
					tr("Demo Mode — Choose Username"),
					tr("Enter a username to use in Demo Mode:"), tr("User"));
				if (!username) {
					// User cancelled username dialog → abort (login() will
					// handle the failure)
					return;
				}
				m_demoMode = true;
				m_demoUsername = username->trimmed().isEmpty()
									 ? tr("User")
									 : username->trimmed();
			} else {
				// User declined demo mode → abort (login() will handle the
				// failure)
				return;
			}
		}
	}

	if (m_demoMode) {
		return;
	}

	// If still no accounts after the dialog (e.g. user cancelled demo mode
	// or didn't add an account), bail out — login() will handle the failure.
	if (accounts->count() <= 0) {
		return;
	}

	m_accountToUse = accounts->defaultAccount();
	if (!m_accountToUse) {
		// If no default account is set, ask the user which one to use.
		if (APPLICATION->usingQmlShell()) {
			/* Plain choose() by account name -- QML has no equivalent of
			 * the widget dialog's "use as global default" checkbox below,
			 * so picking an account here never changes the default. Built
			 * from at(i) rather than accounts->profileNames(), which skips
			 * accounts with no profile name and would leave the chosen
			 * index pointing at the wrong account. */
			QStringList names;
			names.reserve(accounts->count());
			for (int i = 0; i < accounts->count(); ++i) {
				names.append(accounts->at(i)->accountDisplayString());
			}
			const int index = LAUNCHER->uiHost()->choose(
				tr("Which account would you like to use?"), QString(),
				UiHost::Severity::Question, names);
			if (index >= 0) {
				m_accountToUse = accounts->at(index);
			}
		} else {
			ProfileSelectDialog selectDialog(
				tr("Which account would you like to use?"),
				ProfileSelectDialog::GlobalDefaultCheckbox, m_parentWidget);

			selectDialog.exec();

			// Launch the instance with the selected account.
			m_accountToUse = selectDialog.selectedAccount();

			// If the user said to use the account as default, do that.
			if (selectDialog.useAsGlobalDefault() && m_accountToUse) {
				accounts->setDefaultAccount(m_accountToUse);
			}
		}
	}
}

void LaunchController::login()
{
	decideAccount();

	// Demo mode: bypass normal account login entirely
	if (m_demoMode) {
		m_session = std::make_shared<AuthSession>();
		m_session->wants_online = m_online;
		m_session->status = AuthSession::PlayableOnline;
		m_session->user_type = "legacy";
		m_session->MakeDemo();
		m_session->player_name = m_demoUsername; // use the chosen username
		launchInstance();
		return;
	}

	// if no account is selected, we bail
	if (!m_accountToUse) {
		emitFailed(tr("No account selected for launch."));
		return;
	}

	// we try empty password first :)
	QString password;
	// we loop until the user succeeds in logging in or gives up
	bool tryagain = true;
	// the failure. the default failure.
	const QString needLoginAgain =
		tr("Your account is currently not logged in. Please enter your "
		   "password to log in again. <br /> <br /> This could be caused by a "
		   "password change.");
	QString failReason = needLoginAgain;

	while (tryagain) {
		m_session = std::make_shared<AuthSession>();
		m_session->wants_online = m_online;
		m_accountToUse->fillSession(m_session);

		/* MMCO plugin hook: let plugins overwrite the freshly-filled
		 * session with custom auth-provider data (Drasl, Ely.by,
		 * LittleSkin, …). This is where authlib-injector-style
		 * launchers do their work. The hook runs *after* the host
		 * has set its defaults, so plugins always have a fully-formed
		 * baseline to mutate. */
		if (APPLICATION->pluginManager()) {
			const QByteArray accIdUtf8 = m_accountToUse->internalId().toUtf8();
			const QByteArray curName = m_session->player_name.toUtf8();
			const QByteArray curUuid = m_session->uuid.toUtf8();
			const QByteArray curType = m_session->user_type.toUtf8();

			MMCOSessionFillEvent fillEv{};
			fillEv.account_id = accIdUtf8.constData();
			fillEv.account_is_msa = m_accountToUse->isMSA() ? 1 : 0;
			fillEv.wants_online = m_session->wants_online ? 1 : 0;
			fillEv.current_player_name = curName.constData();
			fillEv.current_uuid = curUuid.constData();
			fillEv.current_user_type = curType.constData();
			fillEv.overwrite_access_token = nullptr;
			fillEv.overwrite_session = nullptr;
			fillEv.overwrite_player_name = nullptr;
			fillEv.overwrite_uuid = nullptr;
			fillEv.overwrite_user_type = nullptr;
			fillEv.overwrite_client_token = nullptr;
			fillEv.extra_user_properties = nullptr;

			APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_SESSION_FILL,
													   &fillEv);

			auto apply = [](QString& field, const char* override_value) {
				if (override_value)
					field = QString::fromUtf8(override_value);
			};
			apply(m_session->access_token, fillEv.overwrite_access_token);
			apply(m_session->session, fillEv.overwrite_session);
			apply(m_session->player_name, fillEv.overwrite_player_name);
			apply(m_session->uuid, fillEv.overwrite_uuid);
			apply(m_session->user_type, fillEv.overwrite_user_type);
			apply(m_session->client_token, fillEv.overwrite_client_token);
			if (fillEv.extra_user_properties)
				m_session->user_properties =
					QString::fromUtf8(fillEv.extra_user_properties);
		}

		switch (m_accountToUse->accountState()) {
			case AccountState::Offline: {
				m_session->wants_online = false;
				// NOTE: fallthrough is intentional
			}
			case AccountState::Online: {
				if (!m_session->wants_online) {
					if (m_accountToUse->isMSA()) {
						// MSA account in offline mode: ask for a player name
						auto name = LAUNCHER->uiHost()->askText(
							tr("Player name"),
							tr("Choose your offline mode player name."),
							m_session->player_name);
						if (!name) {
							tryagain = false;
							break;
						}
						QString usedname = m_session->player_name;
						if (name->length()) {
							usedname = *name;
						}
						m_session->MakeOffline(usedname);
					} else {
						// Offline account: username is already stored, just
						// launch
						m_session->MakeOffline(m_session->player_name);
					}
					// offline flavored game from here :3
				}
				if (m_accountToUse->ownsMinecraft()) {
					if (!m_accountToUse->hasProfile()) {
						// Now handle setting up a profile name here...
						ProfileSetupDialog dialog(m_accountToUse,
												  m_parentWidget);
						if (dialog.exec() == QDialog::Accepted) {
							tryagain = true;
							continue;
						} else {
							emitFailed(tr("Received undetermined session "
										  "status during login."));
							return;
						}
					}
					// we own Minecraft, there is a profile, it's all ready to
					// go!
					launchInstance();
					return;
				} else {
					// play demo ?
					const bool playDemo = LAUNCHER->uiHost()->confirm(
						tr("Play demo?"),
						tr("This account does not own Minecraft.\nYou "
						   "need to purchase the game first to play "
						   "it.\n\nDo you want to play the demo?"),
						UiHost::Severity::Warning, tr("Play Demo"),
						tr("Cancel"));
					if (playDemo) {
						// play demo here
						m_session->MakeDemo();
						launchInstance();
					} else {
						emitFailed(tr("Launch cancelled - account does not own "
									  "Minecraft."));
					}
				}
				return;
			}
			case AccountState::Errored:
				// This means some sort of soft error that we can fix with a
				// refresh ... so let's refresh.
			case AccountState::Unchecked: {
				m_accountToUse->refresh();
				// NOTE: fallthrough intentional
			}
			case AccountState::Working: {
				// refresh is in progress, we need to wait for it to finish to
				// proceed.
				auto task = m_accountToUse->currentTask();
				if (APPLICATION->usingQmlShell()) {
					/* No card to reflect this on -- there is no launch task
					 * yet, just an account refresh -- and no "Play Offline"
					 * skip affordance without a dedicated UiHost API for
					 * it, so this is a plain wait with a busy indication. */
					if (!task->isFinished()) {
						auto busy = LAUNCHER->uiHost()->showBusy(
							tr("Refreshing account…"));
						if (!task->isRunning()) {
							QMetaObject::invokeMethod(task.get(), &Task::start,
													  Qt::QueuedConnection);
						}
						QEventLoop loop;
						connect(task.get(), &Task::finished, &loop,
								&QEventLoop::quit);
						loop.exec();
					}
				} else {
					ProgressDialog progDialog(m_parentWidget);
					if (m_online) {
						progDialog.setSkipButton(true, tr("Play Offline"));
					}
					progDialog.execWithTask(task.get());
				}
				continue;
			}
			// FIXME: this is missing - the meaning is that the account is
			// queued for refresh and we should wait for that
			/*
			case AccountState::Queued: {
				return;
			}
			*/
			case AccountState::Expired: {
				auto errorString = tr("The account has expired and needs to be "
									  "logged into manually again.");
				LAUNCHER->uiHost()->message(tr("Account refresh failed"),
											errorString,
											UiHost::Severity::Warning);
				emitFailed(errorString);
				return;
			}
			case AccountState::Gone: {
				auto errorString =
					tr("The account no longer exists on the servers. It may "
					   "have been migrated, in which case please add the new "
					   "account you migrated this one to.");
				LAUNCHER->uiHost()->message(tr("Account gone"), errorString,
											UiHost::Severity::Warning);
				emitFailed(errorString);
				return;
			}
		}
	}
	emitFailed(tr("Failed to launch."));
}

void LaunchController::launchInstance()
{
	Q_ASSERT_X(m_instance != NULL, "launchInstance", "instance is NULL");
	Q_ASSERT_X(m_session.get() != nullptr, "launchInstance", "session is NULL");

	if (!m_instance->reloadSettings()) {
		LAUNCHER->uiHost()->message(tr("Error!"),
									tr("Couldn't load the instance profile."),
									UiHost::Severity::Critical);
		emitFailed(tr("Couldn't load the instance profile."));
		return;
	}

	m_launcher = m_instance->createLaunchTask(m_session, m_serverToJoin);
	if (!m_launcher) {
		emitFailed(tr("Couldn't instantiate a launcher."));
		return;
	}

	auto console = qobject_cast<InstanceWindow*>(m_parentWidget);
	auto showConsole = m_instance->settings()->get("ShowConsole").toBool();
	if (!console && showConsole) {
		APPLICATION->showInstanceLog(m_instance);
	}
	connect(m_launcher.get(), &LaunchTask::readyForLaunch, this,
			&LaunchController::readyForLaunch);
	connect(m_launcher.get(), &LaunchTask::succeeded, this,
			&LaunchController::onSucceeded);
	connect(m_launcher.get(), &LaunchTask::failed, this,
			&LaunchController::onFailed);
	connect(m_launcher.get(), &LaunchTask::requestProgress, this,
			&LaunchController::onProgressRequested);

	// Prepend Online and Auth Status
	QString online_mode;
	if (m_session->wants_online) {
		online_mode = "online";

		// Prepend Server Status
		QStringList servers = {"session.minecraft.net",
							   "textures.minecraft.net", "api.mojang.com"};
		QString resolved_servers = "";
		QHostInfo host_info;

		for (QString server : servers) {
			host_info = QHostInfo::fromName(server);
			resolved_servers =
				resolved_servers + server + " resolves to:\n    [";
			if (!host_info.addresses().isEmpty()) {
				for (QHostAddress address : host_info.addresses()) {
					resolved_servers = resolved_servers + address.toString();
					if (!host_info.addresses().endsWith(address)) {
						resolved_servers = resolved_servers + ", ";
					}
				}
			} else {
				resolved_servers = resolved_servers + "N/A";
			}
			resolved_servers = resolved_servers + "]\n\n";
		}
		m_launcher->prependStep(new TextPrint(
			m_launcher.get(), resolved_servers, MessageLevel::MeshMC));
	} else {
		online_mode = "offline";
	}

	m_launcher->prependStep(new TextPrint(
		m_launcher.get(), "Launched instance in " + online_mode + " mode\n",
		MessageLevel::MeshMC));

	// Opt-in pre-launch snapshot (MeshMC settings -> Features).
	//
	// A launch step, not an inline call: zipping a whole instance takes
	// long enough to freeze the window if it runs on the GUI thread, and
	// getting rid of exactly that freeze is why the BackupSystem plugin
	// moved its pre-launch work onto a background hook before it
	// graduated into core. As a step it runs off-thread, shows real
	// progress, and the launch waits for it.
	//
	// prependStep() calls stack in reverse, so this ends up between the
	// version banner and the "Launched instance in ..." line — before
	// any step that could touch the instance directory.
	if (APPLICATION->settings()->get("BackupBeforeLaunch").toBool()) {
		m_launcher->prependStep(new CreateBackup(m_launcher.get()));
	}

	// Prepend Version
	m_launcher->prependStep(new TextPrint(
		m_launcher.get(),
		BuildConfig.MESHMC_NAME +
			" version: " + BuildConfig.printableVersionString() + "\n\n",
		MessageLevel::MeshMC));

	// Dispatch pre-launch hook to plugins
	if (APPLICATION->pluginManager()) {
		APPLICATION->pluginManager()->clearPendingLaunchMods();
		QByteArray idUtf8 = m_instance->id().toUtf8();
		QByteArray nameUtf8 = m_instance->name().toUtf8();
		QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
		MMCOInstanceInfo hookInfo{};
		hookInfo.instance_id = idUtf8.constData();
		hookInfo.instance_name = nameUtf8.constData();
		hookInfo.instance_path = pathUtf8.constData();
		hookInfo.minecraft_version = nullptr;
		APPLICATION->pluginManager()->dispatchHook(
			MMCO_HOOK_INSTANCE_PRE_LAUNCH, &hookInfo);

		// Apply plugin-requested environment variables via qputenv.
		// CleanEnviroment() reads systemEnvironment() so these will be
		// inherited by the child process.
		auto pendingEnv = APPLICATION->pluginManager()->takePendingLaunchEnv();
		for (auto it = pendingEnv.constBegin(); it != pendingEnv.constEnd();
			 ++it) {
			qputenv(it.key().toUtf8().constData(), it.value().toUtf8());
		}

		// Apply plugin-requested wrapper command (save original for restore)
		QString pendingWrapper =
			APPLICATION->pluginManager()->takePendingLaunchWrapper();
		if (!pendingWrapper.isEmpty()) {
			auto wrapperCommand = m_instance->getWrapperCommand().trimmed();
			if (wrapperCommand.isEmpty()) {
				m_launcher->setWrapperCommand(pendingWrapper);
			} else {
				m_launcher->setWrapperCommand(pendingWrapper + " " +
											  wrapperCommand);
			}
		}

		// Restore env vars after the launch task finishes
		if (!pendingEnv.isEmpty()) {
			connect(m_launcher.get(), &Task::finished, this, [pendingEnv]() {
				for (auto it = pendingEnv.constBegin();
					 it != pendingEnv.constEnd(); ++it) {
					qunsetenv(it.key().toUtf8().constData());
				}
			});
		}
	}

	m_launcher->start();
}

void LaunchController::readyForLaunch()
{
	if (!m_profiler) {
		m_launcher->proceed();
		return;
	}

	QString error;
	if (!m_profiler->check(&error)) {
		m_launcher->abort();
		LAUNCHER->uiHost()->message(tr("Error!"),
									tr("Couldn't start profiler: %1").arg(error),
									UiHost::Severity::Critical);
		emitFailed("Profiler startup failed!");
		return;
	}
	BaseProfiler* profilerInstance =
		m_profiler->createProfiler(m_launcher->instance(), this);

	connect(profilerInstance, &BaseProfiler::readyToLaunch,
			[this](const QString& message) {
				LAUNCHER->uiHost()->message(
					tr("Waiting."),
					tr("The game launch is delayed until you press the "
					   "button. This is the right time to setup the "
					   "profiler, as the "
					   "profiler server is running now.\n\n%1")
						.arg(message),
					UiHost::Severity::Information);
				m_launcher->proceed();
			});
	connect(profilerInstance, &BaseProfiler::abortLaunch,
			[this](const QString& message) {
				LAUNCHER->uiHost()->message(
					tr("Error"),
					tr("Couldn't start the profiler: %1").arg(message),
					UiHost::Severity::Critical);
				m_launcher->abort();
				emitFailed("Profiler startup failed!");
			});
	profilerInstance->beginProfiling(m_launcher);
}

void LaunchController::onSucceeded()
{
	// Dispatch post-launch hook to plugins
	if (APPLICATION->pluginManager() && m_instance) {
		QByteArray idUtf8 = m_instance->id().toUtf8();
		QByteArray nameUtf8 = m_instance->name().toUtf8();
		QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
		MMCOInstanceInfo hookInfo{};
		hookInfo.instance_id = idUtf8.constData();
		hookInfo.instance_name = nameUtf8.constData();
		hookInfo.instance_path = pathUtf8.constData();
		hookInfo.minecraft_version = nullptr;
		APPLICATION->pluginManager()->dispatchHook(
			MMCO_HOOK_INSTANCE_POST_LAUNCH, &hookInfo);
	}

	emitSucceeded();
}

void LaunchController::onFailed(QString reason)
{
	if (m_instance->settings()->get("ShowConsoleOnError").toBool()) {
		APPLICATION->showInstanceLog(m_instance);
	}
	emitFailed(reason);
}

void LaunchController::onProgressRequested(Task* task)
{
	if (APPLICATION->usingQmlShell()) {
		/* The instance's card already tracks this task's status/progress
		 * (see InstanceList::trackLaunchProgress()) -- proceeding is all
		 * this step is actually waiting on; no dialog needed to make it
		 * visible, and no "Abort" affordance without a dedicated UiHost
		 * API for it. */
		m_launcher->proceed();
		return;
	}
	ProgressDialog progDialog(m_parentWidget);
	progDialog.setSkipButton(true, tr("Abort"));
	m_launcher->proceed();
	progDialog.execWithTask(task);
}

bool LaunchController::abort()
{
	if (!m_launcher) {
		return true;
	}
	if (!m_launcher->canAbort()) {
		// Returning false here used to be the whole story, and nobody looks
		// at the return value - so pressing Kill did nothing at all, with no
		// explanation. Say it here, where the reason is actually known,
		// instead of threading a result code through Application::kill().
		if (m_launcher->isAborting()) {
			LAUNCHER->uiHost()->message(
				tr("Already stopping"),
				tr("MeshMC is already shutting this instance down. Give it a "
				   "few seconds - if the game does not react, it gets killed "
				   "automatically."),
				UiHost::Severity::Information);
		} else {
			LAUNCHER->uiHost()->message(
				tr("Can't kill Minecraft"),
				tr("This instance is at a point in the launch process that "
				   "can't be interrupted. Please try again in a moment."),
				UiHost::Severity::Warning);
		}
		return false;
	}
	/* Widget note: CustomMessageBox::selectable() used to default this
	 * particular confirmation to Yes -- confirm() always defaults to the
	 * declining answer instead (see WidgetUiHost::confirm()), so pressing
	 * Enter here now cancels rather than kills the instance. */
	const bool confirmed = LAUNCHER->uiHost()->confirm(
		tr("Kill Minecraft?"),
		tr("This can cause the instance to get corrupted and "
		   "should only be used if Minecraft "
		   "is frozen for some reason"),
		UiHost::Severity::Question);
	if (confirmed) {
		return m_launcher->abort();
	}
	return false;
}
