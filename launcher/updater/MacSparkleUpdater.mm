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

#include "MacSparkleUpdater.h"

#include <QMetaObject>
#include <QTimer>

#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>

/*! Answers Sparkle's questions: which appcast channels we accept, and whether a relaunch into an
 * update has to wait for a game. */
@interface KiteUpdaterDelegate : NSObject <SPUUpdaterDelegate>

@property(nonatomic, copy) NSSet<NSString*>* allowedChannels;
@property(nonatomic, assign) MacSparkleUpdater* owner;
//! Sparkle's go-ahead for a relaunch that is waiting for a game to end.
@property(nonatomic, copy) void (^postponedInstall)(void);

- (void)runPostponedInstall;

@end

@implementation KiteUpdaterDelegate

- (void)dealloc
{
	[_allowedChannels release];
	[_postponedInstall release];
	[super dealloc];
}

- (NSSet<NSString*>*)allowedChannelsForUpdater:(SPUUpdater*)updater
{
	return self.allowedChannels ?: [NSSet set];
}

- (BOOL)updater:(SPUUpdater*)updater
	shouldPostponeRelaunchForUpdate:(SUAppcastItem*)item
				 untilInvokingBlock:(void (^)(void))installHandler
{
	// Relaunching quits Kite, and with it a game started from it.
	if (!self.owner || !self.owner->gamesRunning())
		return NO;
	self.postponedInstall = installHandler;
	self.owner->notifyRelaunchPostponed();
	return YES;
}

- (void)updater:(SPUUpdater*)updater
	didFinishUpdateCycleForUpdateCheck:(SPUUpdateCheck)updateCheck
								 error:(NSError*)error
{
	if (self.owner)
		self.owner->notifyCycleFinished();
}

- (void)runPostponedInstall
{
	void (^install)(void) = [self.postponedInstall retain];
	self.postponedInstall = nil;
	if (install)
		install();
	[install release];
}

@end

class MacSparkleUpdater::Private
{
  public:
	SPUStandardUpdaterController* controller = nil;
	KiteUpdaterDelegate* delegate = nil;
	NSAutoreleasePool* pool = nil;
};

MacSparkleUpdater::MacSparkleUpdater() : priv(new MacSparkleUpdater::Private())
{
	// Qt does not set Cocoa up in every configuration; Sparkle needs a running AppKit and an
	// autorelease pool.
	NSApplicationLoad();
	priv->pool = [[NSAutoreleasePool alloc] init];

	priv->delegate = [[KiteUpdaterDelegate alloc] init];
	priv->delegate.owner = this;

	// startingUpdater:true arms Sparkle's own schedule (daily, SUScheduledCheckInterval), so no
	// timer of ours is involved.
	priv->controller = [[SPUStandardUpdaterController alloc]
		initWithStartingUpdater:true
				updaterDelegate:priv->delegate
			 userDriverDelegate:nil];

	// Kite checks at every start too. Sparkle allows that only right after starting the updater,
	// and only with automatic checks on; a found update shows Sparkle's window, nothing else does.
	if (priv->controller.updater.automaticallyChecksForUpdates) {
		[priv->controller.updater checkForUpdatesInBackground];
	}
}

MacSparkleUpdater::~MacSparkleUpdater()
{
	priv->delegate.owner = nullptr;
	[priv->controller release];
	[priv->delegate release];
	[priv->pool release];

	delete priv;
}

void MacSparkleUpdater::checkForUpdates()
{
	// Sparkle's own interactive path shows the progress and "up to date" alert itself, so this
	// class has no dialogs.
	[priv->controller checkForUpdates:nil];
}

bool MacSparkleUpdater::getAutomaticallyChecksForUpdates()
{
	return priv->controller.updater.automaticallyChecksForUpdates;
}

QDateTime MacSparkleUpdater::lastCheck()
{
	NSDate* date = priv->controller.updater.lastUpdateCheckDate;
	return date ? QDateTime::fromNSDate(date) : QDateTime();
}

void MacSparkleUpdater::setGamesRunning(bool running)
{
	ExternalUpdater::setGamesRunning(running);
	if (running)
		return;
	// Queued: this runs inside the launch controller's "finished" handling, and Sparkle's
	// go-ahead quits Kite.
	QMetaObject::invokeMethod(
		this,
		[this] {
			if (!m_gamesRunning)
				[priv->delegate runPostponedInstall];
		},
		Qt::QueuedConnection);
}

void MacSparkleUpdater::notifyRelaunchPostponed()
{
	// After Sparkle's callback has returned: what Application shows waits for the user.
	QTimer::singleShot(0, this, [this] { emit installWaitsForGame(); });
}

void MacSparkleUpdater::notifyCycleFinished()
{
	emit stateChanged();
}

QSet<QString> MacSparkleUpdater::getAllowedChannels()
{
	QSet<QString> channels;
	for (NSString* channel in priv->delegate.allowedChannels) {
		channels.insert(QString::fromNSString(channel));
	}
	return channels;
}

bool MacSparkleUpdater::getBetaAllowed()
{
	return getAllowedChannels().contains(QStringLiteral("beta"));
}

void MacSparkleUpdater::setAutomaticallyChecksForUpdates(bool check)
{
	priv->controller.updater.automaticallyChecksForUpdates = check ? YES : NO;
}

void MacSparkleUpdater::clearAllowedChannels()
{
	priv->delegate.allowedChannels = [NSSet set];
}

void MacSparkleUpdater::setAllowedChannel(const QString& channel)
{
	if (channel.isEmpty()) {
		clearAllowedChannels();
		return;
	}
	priv->delegate.allowedChannels =
		[NSSet setWithObject:channel.toNSString()];
}

void MacSparkleUpdater::setAllowedChannels(const QSet<QString>& channels)
{
	if (channels.isEmpty()) {
		clearAllowedChannels();
		return;
	}

	NSMutableSet<NSString*>* allowed =
		[NSMutableSet setWithCapacity:channels.count()];
	for (const QString& channel : channels) {
		[allowed addObject:channel.toNSString()];
	}
	priv->delegate.allowedChannels = allowed;
}

void MacSparkleUpdater::setBetaAllowed(bool allowed)
{
	if (allowed) {
		setAllowedChannel(QStringLiteral("beta"));
	} else {
		clearAllowedChannels();
	}
}
