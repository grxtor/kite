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

#include "plugin/MMCOFormat.h"
#include "plugin/PluginAPI.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <string>

/*
 * Result of running the signature verification step against a .mmco file.
 *
 * Used by PluginLoader to decide whether the module is allowed to load
 * based on the license/signature policy.
 */
enum class PluginSignatureState {
	NotChecked,	  /* Verification has not run yet */
	Absent,		  /* No trailer present in the .mmco file */
	Valid,		  /* Trailer present and signature verified by trusted key */
	Untrusted,	  /* Signature verified but signing key is not in the keyring */
	BadSignature, /* Signature did not verify against the file contents */
	Malformed,	  /* Trailer present but malformed / unreadable */
	Error,		  /* GPG backend error (gpgme failure, no keyring, etc.) */
};

/*
 * Why the loader refused to initialise a discovered module. `None` is the
 * "no problem" state. Disabled modules also use this enum so the UI can
 * show why a module is greyed out without scattering string keys around.
 */
enum class PluginDisableReason {
	None,
	UserDisabled,	   /* Explicitly disabled via the plugins dialog */
	SignatureRequired, /* Non-OSS license with no/invalid signature */
	SignatureInvalid,  /* Trailer is malformed or signature bad */
	DependencyMissing, /* A required dependency is not loaded */
	DependencyCycle,   /* This module is part of a dependency cycle */
	SupersededByCore,  /* Functionality moved into the launcher itself —
						  see plugin/CoreSupersededPlugins.h */
	AbiTooOld,		   /* Built against an ABI older than
						  MMCO_ABI_VERSION_MIN -- the module predates a
						  floor raise and needs the author to update it */
	AbiTooNew,		   /* Built against an ABI newer than
						  MMCO_ABI_VERSION -- built for a MeshMC newer
						  than this one */
};

struct PluginDependencyRecord {
	QString name;
	QString minVersion;
	bool optional = false;
};

/*
 * PluginMetadata holds the parsed information about a loaded .mmco module,
 * including its file path, the loaded library handle, and the module info
 * extracted from the mmco_module_info symbol.
 */

struct PluginMetadata {
	/* File system */
	QString filePath; /* Absolute path to the .mmco file */
	QString dataDir;  /* Plugin-private data directory */

	/* From MMCOModuleInfo */
	QString name;
	QString version;
	QString author;
	QString description;
	QString license;
	QString codeLink;
	uint32_t flags = 0;

	/* ABI 2 additions */
	QString iconSetResource;
	QString signingKeyId;
	QVector<PluginDependencyRecord> dependencies;

	/* Runtime state */
	void* libraryHandle = nullptr; /* dlopen/LoadLibrary handle */
	MMCOModuleInfo* moduleInfo = nullptr;

	/* Entry points resolved from the shared library */
	using InitFunc = int (*)(MMCOContext*);
	using UnloadFunc = void (*)();

	InitFunc initFunc = nullptr;
	UnloadFunc unloadFunc = nullptr;

	bool loaded = false;
	bool initialized = false;

	/* Signature verification result and any human-readable diagnostic
	 * from the GPG backend (empty when state == NotChecked / Absent). */
	PluginSignatureState signatureState = PluginSignatureState::NotChecked;
	QString signatureDetail;
	QString signatureFingerprint; /* Fingerprint of the signing key, if any */

	/* Set when the loader (or the user, via settings) decides the module
	 * should not be initialised. PluginManager honours this flag and
	 * skips mmco_init() for it. */
	bool disabled = false;
	PluginDisableReason disableReason = PluginDisableReason::None;
	QString disableDetail; /* free-form human-readable explanation */

	/* Convenience: unique identifier derived from file name */
	QString moduleId() const
	{
		// Strip path and extension to get a stable ID
		QString base = filePath.section('/', -1);
		if (base.endsWith(MMCO_EXTENSION))
			base.chop(static_cast<int>(strlen(MMCO_EXTENSION)));
		return base;
	}
};
