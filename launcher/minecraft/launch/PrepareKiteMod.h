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

#include <launch/LaunchStep.h>

#include <QTimer>
#include <memory>
#include <optional>

#include "QObjectPtr.h"
#include "minecraft/KiteModRules.h"
#include "net/NetJob.h"

class MinecraftInstance;

/*
 * The launch step that gets Kite's own Fabric mod ready: it asks the meta
 * server for the manifest, picks the build for this instance's Minecraft and
 * Fabric Loader, and downloads the jar into the cache when it is missing or
 * the wrong size, deleting the jars the manifest no longer lists. The manifest
 * request is cut short after a few seconds (see manifestTimeout()) and the
 * cached manifest is used instead, so an unreachable meta server cannot hold
 * a launch up. The jar is handed to the game by
 * MinecraftInstance::kiteModArguments(). Says in the launch log what it did.
 * It never fails the launch: without the mod the game starts the same, and
 * only an abort stops it. An offline launch does not run this step and uses
 * what is cached.
 */
class PrepareKiteMod : public LaunchStep
{
	Q_OBJECT
  public:
	explicit PrepareKiteMod(LaunchTask* parent) : LaunchStep(parent) {}
	~PrepareKiteMod() override = default;

	void executeTask() override;
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  private:
	/// How long to wait for the manifest before using the cached one. Longer
	/// when there is none yet, as then the mod would be missing this launch.
	static int manifestTimeout(bool haveCachedManifest);
	void manifestFetched();
	void download(const KiteModBuild& build);
	void downloaded(const KiteModBuild& build);
	void downloadFailed(const QString& reason);
	void finish(const QString& line);

	std::shared_ptr<MinecraftInstance> m_instance;
	QString m_cacheRoot;
	QString m_mcVersion;
	QString m_loaderVersion;
	NetJob::Ptr m_job;
	QTimer m_manifestTimer;
	bool m_aborted = false;
};
