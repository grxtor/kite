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
	// Inside a .app bundle applicationDirPath() returns
	//   <bundle>/Contents/MacOS
	//
	// We install .mmco loadable bundles into
	//   <bundle>/Contents/Resources/mmcmodules
	//
	// Two earlier macOS layouts failed:
	//   * Contents/MacOS/mmcmodules — codesign treats every
	//     non-main-executable Mach-O under Contents/MacOS as a
	//     subcomponent and demands its own signature.
	//   * Contents/PlugIns/mmcmodules — codesign --strict walks
	//     PlugIns/ recursively and verifies each Mach-O's Apple
	//     signature covers the entire file. Our custom RSA trailer
	//     (scripts/mmco_sign.py) appended past __LINKEDIT trips
	//     that check with
	//       "main executable failed strict validation
	//        In subcomponent: .../mmcmodules/Foo.mmco"
	//
	// Contents/Resources/ is the only directory codesign treats as
	// opaque data: files there are hashed into CodeResources but
	// never parsed as code. dlopen() against them is fully
	// supported by the dyld loader — Apple's loader doesn't care
	// which bundle subdirectory a library lives in at runtime.
	//
	// The legacy Contents/PlugIns/mmcmodules and Contents/MacOS/
	// mmcmodules locations are probed as fallbacks so installs
	// that predate this commit keep working until the user
	// reinstalls.
	QDir bundleDir(appDir);
	if (bundleDir.cdUp()) { // MacOS -> Contents
		paths << bundleDir.filePath("Resources/mmcmodules");
		paths << bundleDir.filePath("PlugIns/mmcmodules"); // legacy
	}
	paths << QDir(appDir).filePath("mmcmodules"); // legacy
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
	auto path = FS::PathCombine(appDir, "..", "share", BuildConfig.MESHMC_NAME);
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
	paths << home + "/.local/share/MeshMC/mmcmodules";

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

		// Refuse modules whose job core has taken over. This outranks
		// the user disable list below: re-enabling such a module in the
		// dialog cannot make it safe to load, so we never let it look
		// like a user choice.
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
		// Apply user disable list. We keep the library loaded so that
		// the plugins dialog can still display the module's metadata
		// (name, version, signature state, etc.) — but flag it so the
		// manager refuses to call mmco_init() on it.
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

	// Open the shared library.
	//
	// RTLD_NODELETE prevents the C runtime from running the module's
	// static destructors during exit().  Plugin .mmco files statically
	// link MeshMC_logic which contains the global `const Config
	// BuildConfig` — a non-trivially-destructible object.  Without
	// RTLD_NODELETE the duplicate BuildConfig inside each .mmco would
	// be destroyed at exit(), corrupting the heap because the main
	// binary's copy was already torn down ("corrupted double-linked
	// list").
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

	// Resolve mmco_module_info
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

	// Validate magic
	if (info->magic != MMCO_MAGIC) {
		qWarning() << "[PluginLoader]" << path << "bad magic:" << Qt::hex
				   << info->magic << "(expected" << Qt::hex << MMCO_MAGIC
				   << ")";
		unloadModule(meta);
		return meta;
	}

	// Capture identity fields before the ABI gate below. `name` and
	// `version` sit in MMCOModuleInfo ahead of the "ABI 2 fields" block
	// (see MMCOFormat.h) and have kept the same offsets since the
	// format's first revision, so reading them here does not trust
	// anything beyond what the magic check above already trusts. A
	// module we are about to refuse for its ABI still deserves a name
	// and version in the plugins dialog instead of just vanishing.
	meta.name = QString::fromUtf8(info->name ? info->name : "");
	meta.version = QString::fromUtf8(info->version ? info->version : "");

	// Validate ABI version.
	//
	// Range-accept: any ABI from MMCO_ABI_VERSION_MIN through the
	// current MMCO_ABI_VERSION is loadable. Plugins built against an
	// older-but-still-supported ABI simply never use the
	// fields/hooks/entries the launcher added in a later revision —
	// those slots are appended to MMCOContext (never reordered), so a
	// stale plugin reads the slots it knows about and ignores the
	// rest. Reject anything newer than the launcher knows.
	constexpr uint32_t MMCO_ABI_VERSION_MIN = 2;
	if (info->abi_version < MMCO_ABI_VERSION_MIN ||
		info->abi_version > MMCO_ABI_VERSION) {
		qWarning() << "[PluginLoader]" << path
				   << "ABI version mismatch:" << info->abi_version
				   << "(host supports" << MMCO_ABI_VERSION_MIN << ".."
				   << MMCO_ABI_VERSION << ")";

		// Unlike the magic/missing-symbol failures above, this file IS a
		// real MMCO module — just one built for an ABI this launcher
		// cannot safely initialise. Keep it "loaded" (the library handle
		// stays open, same as the signature-policy disables further
		// down) so the plugins dialog still shows its name/version and
		// why it was refused, instead of the file silently disappearing.
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

	// Resolve mmco_init
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
			// Hard fail regardless of license — the trailer is corrupt
			// or forged. An attacker could ship a non-OSS module wrapped
			// in a tampered signature; refuse to load it.
			markDisabled(PluginDisableReason::SignatureInvalid,
						 detail.isEmpty()
							 ? QStringLiteral("Invalid module signature")
							 : detail);
			break;
		case PluginSignatureState::Error:
			// GPG backend errors are treated as untrusted for non-OSS modules
			// but allowed for OSS so a missing keyring doesn't break the
			// whole plugin system.
			if (!isOss) {
				markDisabled(PluginDisableReason::SignatureRequired,
							 QStringLiteral(
								 "Signature could not be verified (%1) and the "
								 "module is not under an OSS license")
								 .arg(detail));
			}
			break;
		case PluginSignatureState::NotChecked:
			// Should be unreachable — verifyFile() always sets one of the
			// terminal states.
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

	// QCoreApplication::translate() rather than tr(): PluginLoader is not
	// a QObject, so it has no tr() of its own, but the message is still
	// user-facing and needs to go through Qt's translation machinery.
	outDetail = QCoreApplication::translate(
					"PluginLoader",
					"“%1” was built for plugin ABI %2; this MeshMC "
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
