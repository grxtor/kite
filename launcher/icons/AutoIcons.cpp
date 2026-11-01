/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "icons/AutoIcons.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace
{
	/* FNV-1a over the id's UTF-8: unlike qHash it is not seeded per run, so
	 * the first slot is stable across starts and machines. */
	quint32 stableHash(const QString& text)
	{
		const quint32 offsetBasis = 2166136261u;
		const quint32 prime = 16777619u;
		quint32 hash = offsetBasis;
		for (const char byte : text.toUtf8()) {
			hash ^= static_cast<quint8>(byte);
			hash *= prime;
		}
		return hash;
	}
} // namespace

QStringList AutoIcons::pool()
{
	return {QStringLiteral("kite_amethyst"), QStringLiteral("kite_brick"),
			QStringLiteral("kite_cobblestone"), QStringLiteral("kite_copper"),
			QStringLiteral("kite_cyan_ore"), QStringLiteral("kite_emerald_ore"),
			QStringLiteral("kite_glass"), QStringLiteral("kite_gold_ore"),
			QStringLiteral("kite_ice"), QStringLiteral("kite_lantern"),
			QStringLiteral("kite_log"), QStringLiteral("kite_magma"),
			QStringLiteral("kite_moss"), QStringLiteral("kite_obsidian"),
			QStringLiteral("kite_planks"), QStringLiteral("kite_ruby_ore"),
			QStringLiteral("kite_sand"), QStringLiteral("kite_snow")};
}

bool AutoIcons::isGeneric(const QString& key)
{
	return key.isEmpty() || key == QStringLiteral("default") || key == QStringLiteral("flame") ||
		   key == QStringLiteral("modrinth");
}

QHash<QString, QString> AutoIcons::assign(const QList<Instance>& instances,
										  const QHash<QString, QString>& previous,
										  const QStringList& pool)
{
	QHash<QString, QString> result;
	if (pool.isEmpty())
		return result;

	// In id order, so a contested slot never depends on list order.
	QList<Instance> generic;
	for (const Instance& instance : instances) {
		if (isGeneric(instance.iconKey))
			generic.append(instance);
	}
	std::sort(generic.begin(), generic.end(),
			  [](const Instance& a, const Instance& b) { return a.id < b.id; });

	QSet<QString> used;
	for (const Instance& instance : generic) {
		const QString kept = previous.value(instance.id);
		if (pool.contains(kept) && !used.contains(kept)) {
			result.insert(instance.id, kept);
			used.insert(kept);
		}
	}
	for (const Instance& instance : generic) {
		if (result.contains(instance.id))
			continue;
		const int start = static_cast<int>(stableHash(instance.id) % static_cast<quint32>(pool.size()));
		QString chosen = pool.at(start);
		for (int step = 0; step < pool.size(); ++step) {
			const QString candidate = pool.at((start + step) % pool.size());
			if (!used.contains(candidate)) {
				chosen = candidate;
				break;
			}
		}
		// More generic instances than icons share them, each keeping its hashed
		// slot.
		result.insert(instance.id, chosen);
		used.insert(chosen);
	}
	return result;
}

QHash<QString, QString> AutoIcons::fromJson(const QString& json)
{
	QHash<QString, QString> map;
	const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
	for (auto it = object.begin(); it != object.end(); ++it) {
		if (it.value().isString())
			map.insert(it.key(), it.value().toString());
	}
	return map;
}

QString AutoIcons::toJson(const QHash<QString, QString>& map)
{
	QJsonObject object;
	for (auto it = map.begin(); it != map.end(); ++it)
		object.insert(it.key(), it.value());
	return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
