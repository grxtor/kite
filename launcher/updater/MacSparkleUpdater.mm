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

#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>

/*! Answers Sparkle's questions about which appcast channels we accept. */
@interface KiteUpdaterDelegate : NSObject <SPUUpdaterDelegate>

@property(nonatomic, copy) NSSet<NSString*>* allowedChannels;

@end

@implementation KiteUpdaterDelegate

- (NSSet<NSString*>*)allowedChannelsForUpdater:(SPUUpdater*)updater
{
	return self.allowedChannels ?: [NSSet set];
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

	// startingUpdater:true arms Sparkle's own schedule, so no timer of ours is involved.
	priv->controller = [[SPUStandardUpdaterController alloc]
		initWithStartingUpdater:true
				updaterDelegate:priv->delegate
			 userDriverDelegate:nil];
}

MacSparkleUpdater::~MacSparkleUpdater()
{
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

double MacSparkleUpdater::getUpdateCheckInterval()
{
	return priv->controller.updater.updateCheckInterval;
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

void MacSparkleUpdater::setUpdateCheckInterval(double seconds)
{
	priv->controller.updater.updateCheckInterval = seconds;
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
