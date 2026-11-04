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
 * The launch step that gets Kite's Fabric mod ready: it fetches the manifest
 * from the meta server, picks the build for this Minecraft and Fabric Loader,
 * downloads the jar into the cache if missing or the wrong size, and deletes
 * jars the manifest no longer lists. The manifest request is cut short after a
 * few seconds (manifestTimeout()) and the cached manifest is used instead, so
 * an unreachable server cannot hold a launch up. The jar reaches the game via
 * MinecraftInstance::kiteModArguments(). It logs what it did and never fails
 * the launch (only an abort stops it): without the mod the game starts the
 * same. An offline launch skips this step and uses what is cached.
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
	/// How long to wait for the manifest before using the cached one; longer
	/// when there is none yet, as the mod would then be missing this launch.
	static int manifestTimeout(bool haveCachedManifest);
	void manifestFetched();
	void download(const KiteModBuild& build);
	void downloaded(const KiteModBuild& build);
	/// The build's jar is on disk: says it is ready, or that it is left out
	/// when its own metadata rules this Minecraft out.
	void ready(const KiteModBuild& build);
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
