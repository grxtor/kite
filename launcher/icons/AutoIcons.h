/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/* The built-in icons the launcher hands out to instances that still have the
 * generic icon (iconKey "default", drawn as a grass block, or a bare platform
 * logo; see isGeneric()): without them a library of new instances is one
 * grass block after another. Only the QML looks Sahne, Sade and Gece draw
 * them (QmlShell::autoIcons); the instance's own iconKey is never written,
 * so picking an icon always wins, and Klasik and the widget UI keep the
 * grass block.
 *
 * Each instance keeps the icon it was given (the map is persisted by the
 * caller), and a new one gets one no other instance has while the pool
 * lasts, starting from a slot hashed from its id, so the same library hands
 * out the same icons on every machine. */
namespace AutoIcons
{
	struct Instance
	{
		QString id;
		QString iconKey;
	};

	/* The block and mob icons of the built-in set: no platform or loader
	 * logos, not the grass block itself, nothing that reads as a copy of
	 * another (squarecreeper next to creeper, dirt next to grass), and not
	 * magitech, a glossy badge among pixel blocks. */
	QStringList pool();

	/* Whether @p key is an icon nobody picked: the generic one, or the
	 * platform logo an import sets when the pack has no logo of its own
	 * ("flame" for CurseForge, "modrinth"), which every such pack shares. */
	bool isGeneric(const QString& key);

	/* Instance id -> icon key for every generic instance in @p instances.
	 * Entries of @p previous are kept while their instance is still generic
	 * and their icon is still in @p pool; the others are dropped. */
	QHash<QString, QString> assign(const QList<Instance>& instances,
								   const QHash<QString, QString>& previous,
								   const QStringList& pool);

	// The persisted form: a JSON object of id -> key. Bad input reads empty.
	QHash<QString, QString> fromJson(const QString& json);
	QString toJson(const QHash<QString, QString>& map);
} // namespace AutoIcons
