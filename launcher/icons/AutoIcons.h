/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/* The built-in icons handed to instances that still have the generic icon
 * (iconKey "default" or a bare platform logo; see isGeneric()). Only the
 * QML looks Sahne, Sade and Gece draw them (QmlShell::autoIcons); the
 * instance's iconKey is never written, so picking an icon always wins.
 *
 * The map is persisted by the caller. A new instance gets an icon no other
 * has while the pool lasts, from a slot hashed from its id so every
 * machine hands out the same icons. */
namespace AutoIcons
{
	struct Instance
	{
		QString id;
		QString iconKey;
	};

	/* The block and mob icons of the built-in set: no platform or loader logos,
	 * not the grass block, no lookalikes (squarecreeper next to creeper), not
	 * magitech, a glossy badge among pixel blocks. */
	QStringList pool();

	/* Whether @p key is an icon nobody picked: the generic one, or the platform
	 * logo an import sets when the pack has none ("flame", "modrinth"). */
	bool isGeneric(const QString& key);

	/* Instance id -> icon key for every generic instance in @p instances.
	 * Entries of @p previous are kept while their instance is still generic
	 * and the icon is still in @p pool. */
	QHash<QString, QString> assign(const QList<Instance>& instances,
								   const QHash<QString, QString>& previous,
								   const QStringList& pool);

	// The persisted form: a JSON object of id -> key. Bad input reads empty.
	QHash<QString, QString> fromJson(const QString& json);
	QString toJson(const QHash<QString, QString>& map);
} // namespace AutoIcons
