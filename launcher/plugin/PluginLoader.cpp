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

#include "FileSystem.h"
#include "BuildConfig.h"

#include "plugin/PluginLoader.h"
#include "plugin/PluginSignature.h"
#include "plugin/CoreSupersededPlugins.h"

#include "Logging.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PluginLoader::PluginLoader() = default;
PluginLoader::~PluginLoader() = default;

QStringList PluginLoader::defaultSearchPaths()
{
	QStringList paths;

	// In-tree: next to the binary
	QString appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_MAC
	// Inside a .app bundle applicationDirPath() is <bundle>/Contents/MacOS. Modules install into
	// <bundle>/Contents/Resources/mmcmodules, the only place codesign treats as opaque data: under
	// Contents/MacOS it demands a signature per Mach-O, and under Contents/PlugIns `codesign
	// --strict` rejects the custom signature trailer (scripts/mmco_sign.py) appended past
	// __LINKEDIT. dlopen() does not care where in the bundle a library lives. The PlugIns and MacOS
	// locations are legacy fallbacks for older installs.
	QDir bundleDir(appDir);
	if (bundleDir.cdUp()) { // MacOS -> Contents
		paths << bundleDir.filePath("Resources/mmcmodules");
		paths << bundleDir.filePath("PlugIns/mmcmodules"); // legacy
	}
	paths << QDir(appDir).filePath("mmcmodules"); // legacy
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
	auto path = FS::PathCombine(appDir, "..", "share", BuildConfig.KITE_NAME);
	paths << FS::PathCombine(path, "mmcmodules");
#else
	paths << QDir(appDir).filePath("mmcmodules");
#endif

	// User-local
#ifdef Q_OS_WIN
	QString localData =
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (!localData.isEmpty())
		paths << QDir(localData).filePath("mmcmodules");
#else
	// TODO: Edit this so that the system and user levels differentiate between
	// them in the about dialog.
	// ~/.local/lib/mmcmodules
	QString home = QDir::homePath();
	paths << home + "/.local/lib/mmcmodules";
	paths << home + "/.local/share/Kite/mmcmodules";

	// System-wide
	paths << "/usr/local/lib/mmcmodules";
	paths << "/usr/lib/mmcmodules";
	paths << "/usr/local/bin/mmcmodules";
	paths << "/usr/bin/mmcmodules";
#endif

	return paths;
}

QStringList PluginLoader::searchPaths() const
{
	QStringList paths = m_extraPaths;
	paths.append(defaultSearchPaths());
	return paths;
}

void PluginLoader::addSearchPath(const QString& path)
{
	if (!path.isEmpty() && !m_extraPaths.contains(path))
		m_extraPaths.prepend(path);
}

QVector<PluginMetadata>
PluginLoader::discoverModules(const QSet<QString>& disabledNames) const
{
	QVector<PluginMetadata> result;
	QSet<QString> seen; // avoid loading the same module twice

	for (const QString& dir : searchPaths()) {
		for (auto& meta : scanDirectory(dir, disabledNames)) {
			QString id = meta.moduleId();
			if (seen.contains(id)) {
				qCDebug(pluginsLog) << "Skipping duplicate module" << id
						 << "from" << meta.filePath;
				if (meta.libraryHandle)
					unloadModule(meta);
				continue;
			}
			seen.insert(id);
			result.append(std::move(meta));
		}
	}

	qCDebug(pluginsLog) << "Discovered" << result.size() << "module(s)";
	return result;
}

QVector<PluginMetadata>
PluginLoader::scanDirectory(const QString& dir,
							const QSet<QString>& disabledNames) const
{
	QVector<PluginMetadata> result;
	QDir d(dir);
	if (!d.exists()) {
		return result;
	}

	qCDebug(pluginsLog) << "Scanning" << dir;

	QDirIterator it(dir, {"*" MMCO_EXTENSION}, QDir::Files,
					QDirIterator::NoIteratorFlags);
	while (it.hasNext()) {
		QString path = it.next();
		auto meta = loadModule(path);
		if (!meta.loaded)
			continue;

		// Refuse modules whose job core has taken over. This outranks the user disable list:
		// re-enabling one cannot make it safe to load.
		const CoreSupersededPlugin* superseded =
			findCoreSupersededPlugin(meta.name);
		if (!superseded)
			superseded = findCoreSupersededPlugin(meta.moduleId());

		if (superseded) {
			meta.disabled = true;
			meta.disableReason = PluginDisableReason::SupersededByCore;
			meta.disableDetail = QString(superseded->detail);
			qInfo().noquote()
				<< "[PluginLoader] Not loading" << meta.name << "from"
				<< meta.filePath << "-" << meta.disableDetail;
		}
		// User disable list: the library stays loaded so the plugins dialog can show its metadata,
		// but the manager must not call mmco_init().
		else if (disabledNames.contains(meta.name.toLower()) ||
				 disabledNames.contains(meta.moduleId().toLower())) {
			meta.disabled = true;
			meta.disableReason = PluginDisableReason::UserDisabled;
			meta.disableDetail =
				QStringLiteral("Disabled by user in the plugins dialog");
		}

		result.append(std::move(meta));
	}

	return result;
}

PluginMetadata PluginLoader::loadModule(const QString& path) const
{
	PluginMetadata meta;
	meta.filePath = path;

	qCDebug(pluginsLog) << "Loading module:" << path;

	// RTLD_NODELETE keeps the module's static destructors from running at exit(): plugins
	// statically link Kite_logic, whose global `const Config BuildConfig` would be destroyed twice
	// and corrupt the heap.
#ifdef Q_OS_WIN
	HMODULE handle = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
	if (!handle) {
		qWarning() << "[PluginLoader] Failed to load" << path
				   << "- LoadLibrary error:" << GetLastError();
		return meta;
	}
	meta.libraryHandle = reinterpret_cast<void*>(handle);
#else
	void* handle = dlopen(path.toUtf8().constData(),
						  RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
	if (!handle) {
		qWarning() << "[PluginLoader] Failed to load" << path << "-"
				   << dlerror();
		return meta;
	}
	meta.libraryHandle = handle;
#endif

#ifdef Q_OS_WIN
	auto* info = reinterpret_cast<MMCOModuleInfo*>(GetProcAddress(
		static_cast<HMODULE>(meta.libraryHandle), "mmco_module_info"));
#else
	auto* info =
		reinterpret_cast<MMCOModuleInfo*>(dlsym(handle, "mmco_module_info"));
#endif

	if (!info) {
		qWarning() << "[PluginLoader]" << path
				   << "missing mmco_module_info symbol";
		unloadModule(meta);
		return meta;
	}

	if (info->magic != MMCO_MAGIC) {
		qWarning() << "[PluginLoader]" << path << "bad magic:" << Qt::hex
				   << info->magic << "(expected" << Qt::hex << MMCO_MAGIC
				   << ")";
		unloadModule(meta);
		return meta;
	}

	// Capture identity before the ABI gate: name and version sit ahead of the "ABI 2 fields" block
	// and have never moved, so reading them trusts nothing beyond the magic check. A module refused
	// for its ABI still deserves a name and version in the plugins dialog.
	meta.name = QString::fromUtf8(info->name ? info->name : "");
	meta.version = QString::fromUtf8(info->version ? info->version : "");

	// Any ABI from MMCO_ABI_VERSION_MIN through the current one loads: slots are only appended to
	// MMCOContext, never reordered, so an older plugin ignores what it does not know. Anything
	// newer is rejected.
	constexpr uint32_t MMCO_ABI_VERSION_MIN = 5;
	if (info->abi_version < MMCO_ABI_VERSION_MIN ||
		info->abi_version > MMCO_ABI_VERSION) {
		qWarning() << "[PluginLoader]" << path
				   << "ABI version mismatch:" << info->abi_version
				   << "(host supports" << MMCO_ABI_VERSION_MIN << ".."
				   << MMCO_ABI_VERSION << ")";

		// A real MMCO module built for an ABI this launcher cannot initialise: keep it "loaded"
		// (handle open, as for the signature-policy disables) so the dialog shows its name, version
		// and why it was refused.
		meta.loaded = true;
		meta.disabled = true;
		meta.disableReason =
			classifyAbiMismatch(meta.name, path, info->abi_version,
								MMCO_ABI_VERSION_MIN, MMCO_ABI_VERSION,
								meta.disableDetail);
		return meta;
	}

	meta.moduleInfo = info;
	meta.author = QString::fromUtf8(info->author ? info->author : "");
	meta.description =
		QString::fromUtf8(info->description ? info->description : "");
	meta.license = QString::fromUtf8(info->license ? info->license : "");
	meta.codeLink = QString::fromUtf8(info->code_link ? info->code_link : "");
	meta.flags = info->flags;

	// ABI 2 fields
	meta.iconSetResource = QString::fromUtf8(
		info->icon_set_resource ? info->icon_set_resource : "");
	meta.signingKeyId =
		QString::fromUtf8(info->signing_key_id ? info->signing_key_id : "");
	if (info->dependencies && info->dependency_count > 0) {
		meta.dependencies.reserve(static_cast<int>(info->dependency_count));
		for (uint32_t k = 0; k < info->dependency_count; ++k) {
			const MMCODependency& d = info->dependencies[k];
			PluginDependencyRecord rec;
			rec.name = QString::fromUtf8(d.name ? d.name : "");
			rec.minVersion =
				QString::fromUtf8(d.min_version ? d.min_version : "");
			rec.optional = (d.optional != 0);
			if (!rec.name.isEmpty())
				meta.dependencies.append(rec);
		}
	}

#ifdef Q_OS_WIN
	meta.initFunc = reinterpret_cast<PluginMetadata::InitFunc>(
		GetProcAddress(static_cast<HMODULE>(meta.libraryHandle), "mmco_init"));
	meta.unloadFunc =
		reinterpret_cast<PluginMetadata::UnloadFunc>(GetProcAddress(
			static_cast<HMODULE>(meta.libraryHandle), "mmco_unload"));
#else
	meta.initFunc =
		reinterpret_cast<PluginMetadata::InitFunc>(dlsym(handle, "mmco_init"));
	meta.unloadFunc = reinterpret_cast<PluginMetadata::UnloadFunc>(
		dlsym(handle, "mmco_unload"));
#endif

	if (!meta.initFunc) {
		qWarning() << "[PluginLoader]" << path << "missing mmco_init symbol";
		unloadModule(meta);
		return meta;
	}

	if (!meta.unloadFunc) {
		qWarning() << "[PluginLoader]" << path << "missing mmco_unload symbol";
		unloadModule(meta);
		return meta;
	}

	meta.loaded = true;
	qCDebug(pluginsLog).noquote().nospace()
		<< "Loaded module: " << meta.name << " v" << meta.version
		<< " by " << meta.author;

	// Trust pre-flight — sets meta.signatureState and may set meta.disabled.
	verifySignatureAndPolicy(meta);
	if (meta.disabled) {
		qWarning().noquote() << "[PluginLoader] Module" << meta.name
							 << "marked disabled:" << meta.disableDetail;
	}

	return meta;
}

void PluginLoader::verifySignatureAndPolicy(PluginMetadata& meta)
{
	QString detail;
	QString fingerprint;
	const PluginSignatureState state =
		PluginSignature::verifyFile(meta.filePath, detail, fingerprint);

	meta.signatureState = state;
	meta.signatureDetail = detail;
	meta.signatureFingerprint = fingerprint;

	const bool isOss = PluginSignature::isOpenSourceLicense(meta.license);

	auto markDisabled = [&](PluginDisableReason r, const QString& d) {
		meta.disabled = true;
		meta.disableReason = r;
		meta.disableDetail = d;
	};

	switch (state) {
		case PluginSignatureState::Valid:
			// Always accepted, regardless of license.
			break;
		case PluginSignatureState::Absent:
			if (!isOss) {
				markDisabled(PluginDisableReason::SignatureRequired,
							 QStringLiteral(
								 "Module is not under an OSS license (%1) and "
								 "carries no GPG signature")
								 .arg(meta.license.isEmpty()
										  ? QStringLiteral("unspecified")
										  : meta.license));
			}
			break;
		case PluginSignatureState::Untrusted:
			if (!isOss) {
				markDisabled(
					PluginDisableReason::SignatureRequired,
					QStringLiteral(
						"Signed by an untrusted key (%1) and not under an "
						"OSS license")
						.arg(detail));
			}
			break;
		case PluginSignatureState::BadSignature:
		case PluginSignatureState::Malformed:
			// Hard fail regardless of license: the trailer is corrupt or forged, e.g. a non-OSS
			// module wrapped in a tampered signature.
			markDisabled(PluginDisableReason::SignatureInvalid,
						 detail.isEmpty()
							 ? QStringLiteral("Invalid module signature")
							 : detail);
			break;
		case PluginSignatureState::Error:
			// GPG backend errors count as untrusted for non-OSS modules but are allowed for OSS, so
			// a missing keyring does not break the plugin system.
			if (!isOss) {
				markDisabled(PluginDisableReason::SignatureRequired,
							 QStringLiteral(
								 "Signature could not be verified (%1) and the "
								 "module is not under an OSS license")
								 .arg(detail));
			}
			break;
		case PluginSignatureState::NotChecked:
			// Unreachable: verifyFile() always sets a terminal state.
			break;
	}
}

PluginDisableReason
PluginLoader::classifyAbiMismatch(const QString& moduleName,
								  const QString& fallbackLabel,
								  uint32_t builtForAbi, uint32_t abiMin,
								  uint32_t abiMax, QString& outDetail)
{
	const QString label = moduleName.isEmpty() ? fallbackLabel : moduleName;

	// translate() rather than tr(): PluginLoader is not a QObject, but the message is user-facing.
	outDetail = QCoreApplication::translate(
					"PluginLoader",
					"“%1” was built for plugin ABI %2; this Kite "
					"supports %3–%4. It needs to be updated by its "
					"author.")
					.arg(label)
					.arg(builtForAbi)
					.arg(abiMin)
					.arg(abiMax);

	return builtForAbi < abiMin ? PluginDisableReason::AbiTooOld
								 : PluginDisableReason::AbiTooNew;
}

void PluginLoader::unloadModule(PluginMetadata& meta)
{
	if (!meta.libraryHandle)
		return;

#ifdef Q_OS_WIN
	FreeLibrary(static_cast<HMODULE>(meta.libraryHandle));
#else
	dlclose(meta.libraryHandle);
#endif

	meta.libraryHandle = nullptr;
	meta.moduleInfo = nullptr;
	meta.initFunc = nullptr;
	meta.unloadFunc = nullptr;
	meta.loaded = false;
	meta.initialized = false;
}
