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

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "java/JavaChecker.h"

class JavaInstallList;

/*
 * QML-facing Java management: the widget-free half of JavaPage and
 * JavaSettingsWidget. It owns the three jobs those pages share:
 *
 *  - detecting Java installs (the launcher's JavaInstallList, exposed as
 *    `installs` for a ListView);
 *  - testing one java binary the way JavaCommon::TestCheck does, off the GUI
 *    thread (JavaChecker spawns a process), reporting through
 *    testFinished();
 *  - listing and removing the runtimes MeshMC downloaded itself, which live
 *    under JavaUtils::managedJavaRoot() as <vendor>/<name-version>/.
 *
 * Downloading a runtime is JavaDownloadController's job. What a view writes
 * (JavaPath, JvmArgs, the vendor) goes through SettingsStore like any other
 * setting; nothing here touches settings.
 *
 * Several views (global Settings, one per instance) can share one
 * controller: a test is started with the caller's `token` and answered
 * with the same token, so each view only shows its own result.
 */
class JavaController : public QObject
{
	Q_OBJECT

	/// [{value, label}]: the vendors JavaAutoDownloadVendor can be set to.
	Q_PROPERTY(QVariantList vendors READ vendors CONSTANT)
	/// False in a build without the Java downloader: hide vendor and
	/// download controls.
	Q_PROPERTY(bool downloaderAvailable READ downloaderAvailable CONSTANT)
	/// Detected Java installs (JavaInstallList); roles include path,
	/// version, architecture, recommended. Null without a LauncherContext.
	Q_PROPERTY(QObject* installs READ installs CONSTANT)
	Q_PROPERTY(bool detecting READ detecting NOTIFY detectingChanged)
	/// Token of the test now running, or empty when none is.
	Q_PROPERTY(QString testingToken READ testingToken NOTIFY testingChanged)
	/// [{name, vendor, version, javaPath, root}] of downloaded runtimes.
	Q_PROPERTY(QVariantList managed READ managed NOTIFY managedChanged)
	/// Where downloaded runtimes live.
	Q_PROPERTY(QString managedRoot READ managedRoot CONSTANT)

  public:
	explicit JavaController(QObject* parent = nullptr);
	~JavaController() override;

	QVariantList vendors() const;
	bool downloaderAvailable() const;
	QObject* installs() const;
	bool detecting() const;
	QString testingToken() const;
	QVariantList managed() const;
	QString managedRoot() const;

	/// Runs the launcher's Java detection again (a fresh scan every time).
	Q_INVOKABLE void detect();

	/*!
	 * Checks the java binary at @p path, then, if that works, runs it again
	 * with @p args and the given memory (MiB; @p permGen only matters for a
	 * Java old enough to have PermGen). Answers with testFinished(). A test
	 * already running makes this a no-op.
	 */
	Q_INVOKABLE void test(const QString& token, const QString& path,
						  const QString& args, int minMem, int maxMem,
						  int permGen);

	/// Short, plain-English warning if @p args sets memory by hand, or an
	/// empty string if they are fine. See JavaArgs::setsMemoryFlags().
	Q_INVOKABLE QString argsWarning(const QString& args) const;

	/// Whether a Java of @p version (e.g. "1.7.0_80", "17.0.2") still takes
	/// the PermGen setting. An empty or unparseable version counts as no:
	/// the row only shows once a version is known.
	Q_INVOKABLE bool requiresPermGen(const QString& version) const;

	/// The local path of a file the user picked in a file dialog, cleaned
	/// up, or an empty string if it is not an executable file.
	Q_INVOKABLE QString pickedJava(const QUrl& url) const;

	/// Rescans managedRoot().
	Q_INVOKABLE void refreshManaged();

	/*!
	 * Deletes the downloaded runtime whose folder is @p root. Refuses (and
	 * returns an explanation) for anything that is not exactly
	 * <managedRoot>/<vendor>/<name>; returns an empty string on success.
	 */
	Q_INVOKABLE QString removeManaged(const QString& root);

	/// Downloaded runtimes under @p rootDir, as refreshManaged() lists
	/// them. Public so tests can point it at a temporary directory.
	static QVariantList scanManaged(const QString& rootDir);

	/// removeManaged() against an arbitrary @p rootDir.
	static QString removeManagedUnder(const QString& rootDir,
									  const QString& root);

	/// The vendor choices, as a static for JavaDownloadController.
	static QVariantList vendorList();

	/// Turns a finished check into the map testFinished() carries:
	/// {ok, path, version, architecture, vendor, warnings, error,
	/// requiresPermGen}.
	static QVariantMap describeCheck(const JavaCheckResult& result);

  signals:
	void detectingChanged();
	void testingChanged();
	void managedChanged();
	/// @p result is describeCheck()'s map for the first failing stage, or
	/// the successful second stage.
	void testFinished(const QString& token, const QVariantMap& result);

  private:
	void startSecondStage(const JavaCheckResult& first);
	void finishTest(const QVariantMap& result);

	bool m_detecting = false;
	QString m_testingToken;
	QVariantList m_managed;

	std::shared_ptr<JavaChecker> m_checker;
	// The arguments of the running test, kept for its second stage.
	QString m_args;
	int m_minMem = 0;
	int m_maxMem = 0;
	int m_permGen = 64;
};
