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

#include "VerifyJavaInstall.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>

#include <tuple>

#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <minecraft/VersionFilterData.h>

#include "core/LauncherContext.h"
#include "FileSystem.h"
#include "Json.h"
#include "java/JavaUtils.h"

#ifndef Kite_DISABLE_JAVA_DOWNLOADER
#include "BuildConfig.h"
#include <optional>
#include "net/Download.h"
#endif

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace
{
	struct ProbedJava
	{
		JavaVersion version;
		QString arch; // os.arch, as the JVM reports it
	};

	std::optional<ProbedJava> probeJava(const QString& javaPath)
	{
		const auto checkerJar =
			FS::PathCombine(LAUNCHER->getJarsPath(), "JavaCheck.jar");
		if (!QFileInfo::exists(checkerJar)) {
			return std::nullopt;
		}

		QProcess process;
		process.setProgram(javaPath);
		process.setArguments({"-jar", checkerJar});
		process.setProcessEnvironment(CleanEnviroment());
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.start();

		if (!process.waitForFinished(15000) ||
			process.exitStatus() != QProcess::NormalExit ||
			process.exitCode() != 0) {
			return std::nullopt;
		}

		const auto stdoutData =
			QString::fromLocal8Bit(process.readAllStandardOutput());
		const auto lines = stdoutData.split('\n', Qt::SkipEmptyParts);
		QString version;
		QString arch;
		for (const auto& rawLine : lines) {
			const auto line = rawLine.trimmed();
			if (line.startsWith("java.version=")) {
				version = line.mid(QString("java.version=").size());
			} else if (line.startsWith("os.arch=")) {
				arch = line.mid(QString("os.arch=").size());
			}
		}
		if (version.isEmpty()) {
			return std::nullopt;
		}
		return ProbedJava{JavaVersion(version), arch};
	}

	// Whether a JVM that reports os.arch @p arch runs natively for the
	// architecture Kite was built for, the one its Java downloads are for
	// too. On an Apple Silicon Mac an x86_64 Java only runs under Rosetta.
	bool runsNatively(const QString& arch)
	{
		static const QHash<QString, QString> javaToQt = {
			{"aarch64", "arm64"},
			{"amd64", "x86_64"},
			{"x86", "i386"},
			{"i686", "i386"},
		};
		return javaToQt.value(arch, arch) == QSysInfo::buildCpuArchitecture();
	}

	// Whether @p javaBinary sits in a Java installation (<home>/bin/java,
	// with the "release" file every JDK and JRE has in <home>) rather than
	// being a stub or a link elsewhere.
	bool isJavaInstallation(const QString& javaBinary)
	{
		return QFileInfo(QDir(QFileInfo(javaBinary).absolutePath())
							 .filePath(QStringLiteral("../release")))
			.isFile();
	}
} // namespace

int VerifyJavaInstall::determineRequiredJavaMajor() const
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	auto minecraftComponent =
		m_inst->getPackProfile()->getComponent("net.minecraft");

	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java25BeginsDate)
		return 25;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java21BeginsDate)
		return 21;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java17BeginsDate)
		return 17;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java16BeginsDate)
		return 16;
	if (minecraftComponent->getReleaseDateTime() >=
		g_VersionFilterData.java8BeginsDate)
		return 8;
	return 0;
}

QString VerifyJavaInstall::javaInstallDir() const
{
	return JavaUtils::managedJavaRoot();
}

VerifyJavaInstall::InstalledJava
VerifyJavaInstall::findInstalledJava(int requiredMajor) const
{
	JavaUtils javaUtils;
	QList<QString> systemJavas = javaUtils.FindJavaPaths();
	QSet<QString> seenPaths;
	// The closest major at or above the requirement, not the first one
	// found: with Java 17 and 25 installed, 1.20.4 gets 17. Between two of
	// the same major, one that runs natively comes first -- but never before
	// a closer major: an x86_64 Java under Rosetta can load the natives of
	// every Minecraft version, an arm64 one only those of 1.19 and later.
	// Then one inside a Java installation comes before a stub like macOS's
	// /usr/bin/java, which runs whichever Java is newest and so can change
	// under the version recorded for it.
	InstalledJava best;
	std::tuple<int, bool, bool> bestRank; // lower is better
	for (const QString& javaPath : systemJavas) {
		QString resolved = FS::ResolveExecutable(javaPath);
		if (resolved.isEmpty() || seenPaths.contains(resolved))
			continue;

		seenPaths.insert(resolved);
		const auto probed = probeJava(resolved);
		if (!probed.has_value() || probed->version.major() < requiredMajor)
			continue;
		const int major = probed->version.major();
		const auto rank = std::make_tuple(major, !runsNatively(probed->arch),
										  !isJavaInstallation(resolved));
		if (best.path.isEmpty() || rank < bestRank) {
			best = {resolved, major};
			bestRank = rank;
			if (rank == std::make_tuple(requiredMajor, false, false))
				break;
		}
	}

	return best;
}

void VerifyJavaInstall::executeTask()
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());

	auto javaVersion = m_inst->getJavaVersion();
	int requiredMajor = determineRequiredJavaMajor();
	// CheckJava lets a launch through with no usable Java when none is set
	// up (the welcome screen's "let Kite download the right version"), so a
	// recorded version alone does not mean there is a Java to run.
	const bool javaPathResolves =
		!QStandardPaths::findExecutable(
			 FS::ResolveExecutable(
				 m_inst->settings()->get("JavaPath").toString()))
			 .isEmpty();

	// No Java requirement or already met
	if (javaPathResolves &&
		(requiredMajor == 0 || javaVersion.major() >= requiredMajor)) {
		emitSucceeded();
		return;
	}

	if (javaPathResolves) {
		// Java version insufficient — try to find an already-downloaded one
		emit logLine(tr("Current Java version %1 does not meet the "
						"requirement of Java %2.")
						 .arg(javaVersion.toString())
						 .arg(requiredMajor),
					 MessageLevel::Warning);
	} else {
		// Minecraft before 1.12 states no requirement; it runs on Java 8.
		if (requiredMajor == 0) {
			requiredMajor = 8;
		}
		emit logLine(
			tr("No Java is set up. Looking for Java %1.").arg(requiredMajor),
			MessageLevel::Kite);
	}

	const auto existingJava = findInstalledJava(requiredMajor);
	// With no Java set up, each version gets the major it asks for, the way
	// the official launcher does it, and that major is downloaded when it is
	// not installed. A newer one is not enough for old versions: the
	// LaunchWrapper of Forge up to 1.12.2 does not run on Java 9 and later.
	// An installed newer one is still used if no download works out.
	if (!existingJava.path.isEmpty() &&
		(javaPathResolves || existingJava.major == requiredMajor)) {
		emit logLine(tr("Found installed Java %1 at: %2")
						 .arg(existingJava.major)
						 .arg(existingJava.path),
					 MessageLevel::Kite);
		setJavaPathAndSucceed(existingJava.path);
		return;
	}
	m_fallbackJava = existingJava;

#ifndef Kite_DISABLE_JAVA_DOWNLOADER
	// Check runtime setting for auto-download
	if (!m_inst->settings()->get("JavaAutoDownload").toBool()) {
		failOrUseFallback(
			tr("Java %1 is required but not installed. Automatic Java "
			   "download is disabled.")
				.arg(requiredMajor));
		return;
	}
	// Not found — auto-download
	emit logLine(
		tr("No installed Java %1 found. Downloading...").arg(requiredMajor),
		MessageLevel::Kite);
	autoDownloadJava(requiredMajor);
#else
	failOrUseFallback(
		tr("Java %1 is required but not installed. Please install it manually.")
			.arg(requiredMajor));
#endif
}

#ifndef Kite_DISABLE_JAVA_DOWNLOADER
void VerifyJavaInstall::autoDownloadJava(int requiredMajor)
{
	// Get vendor from instance settings (falls through to global if not
	// overridden)
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	m_preferredVendor =
		m_inst->settings()->get("JavaAutoDownloadVendor").toString();
	if (m_preferredVendor.isEmpty()) {
		m_preferredVendor = "net.minecraft.java";
	}
	emit logLine(tr("Using Java vendor: %1").arg(m_preferredVendor),
				 MessageLevel::Kite);
	fetchVersionList(requiredMajor);
}

void VerifyJavaInstall::fetchVersionList(int requiredMajor)
{
	m_fetchData.clear();
	QString uid = m_preferredVendor;
	QString url = QString("%1%2/index.json").arg(BuildConfig.META_URL, uid);

	m_fetchJob = new NetJob(tr("Fetch Java versions"), LAUNCHER->network());
	auto dl = Net::Download::makeByteArray(QUrl(url), &m_fetchData);
	m_fetchJob->addNetAction(dl);

	connect(
		m_fetchJob.get(), &NetJob::succeeded, this,
		[this, uid, requiredMajor]() {
			m_fetchJob.reset();

			QJsonDocument doc;
			try {
				doc = Json::requireDocument(m_fetchData);
			} catch (const Exception& e) {
				failOrUseFallback(
					tr("Failed to parse Java version list from meta server: %1")
						.arg(e.cause()));
				return;
			}
			if (!doc.isObject()) {
				failOrUseFallback(
					tr("Failed to parse Java version list from meta server."));
				return;
			}

			auto versions = JavaDownload::parseVersionIndex(doc.object(), uid);

			// Find the matching version (e.g., "java25" for requiredMajor=25)
			QString targetVersionId = QString("java%1").arg(requiredMajor);
			bool found = false;
			for (const auto& ver : versions) {
				if (ver.versionId == targetVersionId) {
					found = true;
					fetchRuntimes(ver.versionId, requiredMajor);
					return;
				}
			}

			if (!found) {
				failOrUseFallback(
					tr("Java %1 is not available for download from the "
					   "selected vendor. Please install it manually or "
					   "try a different vendor in settings.")
						.arg(requiredMajor));
			}
		});

	connect(m_fetchJob.get(), &NetJob::failed, this,
			[this, requiredMajor](QString reason) {
				failOrUseFallback(
					tr("Failed to fetch Java version list: %1. Please "
					   "install Java %2 manually.")
						.arg(reason)
						.arg(requiredMajor));
				m_fetchJob.reset();
			});

	m_fetchJob->start();
}

void VerifyJavaInstall::fetchRuntimes(const QString& versionId,
									  int requiredMajor)
{
	m_fetchData.clear();
	QString uid = m_preferredVendor;
	QString url =
		QString("%1%2/%3.json").arg(BuildConfig.META_URL, uid, versionId);

	m_fetchJob =
		new NetJob(tr("Fetch Java runtime details"), LAUNCHER->network());
	auto dl = Net::Download::makeByteArray(QUrl(url), &m_fetchData);
	m_fetchJob->addNetAction(dl);

	connect(m_fetchJob.get(), &NetJob::succeeded, this,
			[this, requiredMajor]() {
				auto fetchJob = std::move(m_fetchJob);

				QJsonDocument doc;
				try {
					doc = Json::requireDocument(m_fetchData);
				} catch (const Exception& e) {
					failOrUseFallback(
						tr("Failed to parse Java runtime details: %1")
							.arg(e.cause()));
					return;
				}
				if (!doc.isObject()) {
					failOrUseFallback(
						tr("Failed to parse Java runtime details."));
					return;
				}

				auto allRuntimes = JavaDownload::parseRuntimes(doc.object());
				QString myOS = JavaDownload::currentRuntimeOS();

				// Filter for current platform
				for (const auto& rt : allRuntimes) {
					if (rt.runtimeOS == myOS) {
						emit logLine(tr("Downloading %1 (%2)...")
										 .arg(rt.name, rt.version.toString()),
									 MessageLevel::Kite);
						startDownload(rt, requiredMajor);
						return;
					}
				}

				failOrUseFallback(
					tr("No Java %1 download available for your platform "
					   "(%2). Please install it manually.")
						.arg(requiredMajor)
						.arg(myOS));
			});

	connect(m_fetchJob.get(), &NetJob::failed, this, [this](QString reason) {
		failOrUseFallback(
			tr("Failed to fetch Java runtime details: %1").arg(reason));
		m_fetchJob.reset();
	});

	m_fetchJob->start();
}

void VerifyJavaInstall::startDownload(const JavaDownload::RuntimeEntry& runtime,
									  int requiredMajor)
{
	QString dirName =
		QString("%1-%2").arg(runtime.name, runtime.version.toString());
	QString targetDir =
		FS::PathCombine(javaInstallDir(), runtime.vendor, dirName);

	m_downloadTask =
		std::make_unique<JavaDownloadTask>(runtime, targetDir, this);

	connect(m_downloadTask.get(), &Task::succeeded, this, [this]() {
		QString javaPath = m_downloadTask->installedJavaPath();

		if (javaPath.isEmpty()) {
			failOrUseFallback(
				tr("Java was downloaded but the binary could not be found."));
			return;
		}

		emit logLine(tr("Java downloaded and installed at: %1").arg(javaPath),
					 MessageLevel::Kite);
		setJavaPathAndSucceed(javaPath);
	});

	connect(m_downloadTask.get(), &Task::failed, this,
			[this, requiredMajor](const QString& reason) {
				failOrUseFallback(tr("Failed to download Java %1: %2")
									  .arg(requiredMajor)
									  .arg(reason));
				m_downloadTask.reset();
			});

	connect(m_downloadTask.get(), &Task::status, this,
			[this](const QString& status) {
				emit logLine(status, MessageLevel::Kite);
			});

	m_downloadTask->start();
}

#endif

void VerifyJavaInstall::failOrUseFallback(const QString& reason)
{
	if (m_fallbackJava.path.isEmpty()) {
		emitFailed(reason);
		return;
	}
	emit logLine(reason, MessageLevel::Warning);
	emit logLine(tr("Using the installed Java %1 at %2 instead.")
					 .arg(m_fallbackJava.major)
					 .arg(m_fallbackJava.path),
				 MessageLevel::Kite);
	setJavaPathAndSucceed(m_fallbackJava.path);
}

void VerifyJavaInstall::setJavaPathAndSucceed(const QString& javaPath)
{
	auto m_inst =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	// Set Java path override on the instance only, not globally
	m_inst->settings()->set("OverrideJavaLocation", true);
	m_inst->settings()->set("JavaPath", javaPath);
	emit logLine(tr("Java path set to: %1").arg(javaPath),
				 MessageLevel::Kite);

	// What CheckJava records for a Java it checks, recorded for this one
	// too: the native libraries and the classpath follow the architecture,
	// and the JVM arguments follow the version (an unknown one adds
	// -XX:PermSize, which modern Java refuses to start with). Without this
	// they would still describe the Java used before, or none at all.
	m_javaChecker = new JavaChecker();
	m_javaChecker->m_path = javaPath;
	connect(m_javaChecker.get(), &JavaChecker::checkFinished, this,
			[this, javaPath](JavaCheckResult result) {
				m_javaChecker.reset();
				if (result.validity != JavaCheckResult::Validity::Valid) {
					emitFailed(tr("Could not start the Java at %1.")
								   .arg(javaPath));
					return;
				}
				auto settings = m_parent->instance()->settings();
				settings->set("JavaVersion", result.javaVersion.toString());
				settings->set("JavaArchitecture", result.mojangPlatform);
				settings->set("JavaVendor", result.javaVendor);
				settings->set("JavaTimestamp",
							  QFileInfo(javaPath)
								  .lastModified()
								  .toMSecsSinceEpoch());
				emitSucceeded();
			});
	m_javaChecker->performCheck();
}
