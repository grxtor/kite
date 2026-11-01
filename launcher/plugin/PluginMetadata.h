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

#pragma once

#include "plugin/MMCOFormat.h"
#include "plugin/PluginAPI.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <string>

/* Result of the signature verification step for a .mmco; PluginLoader combines it with the license
 * policy. */
enum class PluginSignatureState {
	NotChecked,	  /* Verification has not run yet */
	Absent,		  /* No trailer present in the .mmco file */
	Valid,		  /* Trailer present and signature verified by trusted key */
	Untrusted,	  /* Signature verified but signing key is not in the keyring */
	BadSignature, /* Signature did not verify against the file contents */
	Malformed,	  /* Trailer present but malformed / unreadable */
	Error,		  /* GPG backend error (gpgme failure, no keyring, etc.) */
};

/* Why the loader refused to initialise a discovered module (None = no problem); the UI shows it for
 * greyed-out modules. */
enum class PluginDisableReason {
	None,
	UserDisabled,	   /* Explicitly disabled in Settings */
	SignatureRequired, /* Non-OSS license with no/invalid signature */
	SignatureInvalid,  /* Trailer is malformed or signature bad */
	DependencyMissing, /* A required dependency is not loaded */
	DependencyCycle,   /* This module is part of a dependency cycle */
	SupersededByCore,  /* Functionality moved into the launcher itself */
	AbiTooOld,		   /* Built for an ABI below MMCO_ABI_VERSION_MIN; its author must update it */
	AbiTooNew,		   /* Built for an ABI above MMCO_ABI_VERSION, i.e. a newer Kite */
};

/* A short label for @p reason as the QML "Installed plugins" section shows it; translated in the
 * QObject context. */
inline QString pluginDisableReasonLabel(PluginDisableReason reason)
{
	switch (reason) {
		case PluginDisableReason::None:
			return QObject::tr("OK");
		case PluginDisableReason::UserDisabled:
			return QObject::tr("Disabled by user");
		case PluginDisableReason::SignatureRequired:
			return QObject::tr("Signature required");
		case PluginDisableReason::SignatureInvalid:
			return QObject::tr("Invalid signature");
		case PluginDisableReason::DependencyMissing:
			return QObject::tr("Missing dependency");
		case PluginDisableReason::DependencyCycle:
			return QObject::tr("Dependency cycle");
		case PluginDisableReason::SupersededByCore:
			return QObject::tr("Built into Kite");
		case PluginDisableReason::AbiTooOld:
			return QObject::tr("Plugin ABI too old");
		case PluginDisableReason::AbiTooNew:
			return QObject::tr("Plugin ABI too new");
	}
	return QString();
}

struct PluginDependencyRecord {
	QString name;
	QString minVersion;
	bool optional = false;
};

/* The parsed information about a loaded .mmco module: file path, library handle and the
 * mmco_module_info contents. */

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

	/* Signature verification result and the GPG backend's diagnostic (empty when NotChecked /
	 * Absent). */
	PluginSignatureState signatureState = PluginSignatureState::NotChecked;
	QString signatureDetail;
	QString signatureFingerprint; /* Fingerprint of the signing key, if any */

	/* Set when the loader or the user decides the module must not be initialised; PluginManager
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
