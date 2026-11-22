/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QList>
#include <QSet>
#include <QString>

class Mod;

/*
 * What is in a mods-like folder, as comparison keys, for files that have no
 * sidecar in the ModMetadataIndex (dropped in by hand, or installed by another
 * launcher). A search result is "installed" when the provider's slug or its
 * name gives one of the keys. Everything here is local: the jar's own metadata
 * (mod id, name) and its file name; nothing is asked of a provider.
 */
namespace InstalledContentKeys
{
	/* One file, as far as it is known. `modId` and `name` are empty until the
	 * file's metadata has been read, and for files that carry none. */
	struct File {
		QString fileName;
		QString modId;
		QString name;
	};

	/* Lower-cased letters and digits only: "fabric-api", "Fabric API" and
	 * "fabric_api" all give "fabricapi". */
	QString key(const QString& text);

	/* The project part of a file name, as a key: "sodium-fabric-0.6.5+mc1.21.jar"
	 * gives "sodiumfabric"; "[1.20.1] Xaeros_Minimap_24.5.0_Forge.jar" gives
	 * "xaerosminimap". Empty when nothing but versions is left. */
	QString fileKey(const QString& fileName);

	/* The keys of @p files: each mod id, name and file key, and the file key
	 * again without a trailing loader word ("sodiumfabric" -> "sodium"). Keys
	 * of one character are left out. */
	QSet<QString> keysOf(const QList<File>& files);
	QSet<QString> keysOf(const QList<Mod>& mods);

	/* Whether a project with this slug and name is among @p keys. */
	bool contains(const QSet<QString>& keys, const QString& slug,
				  const QString& name);
} // namespace InstalledContentKeys
