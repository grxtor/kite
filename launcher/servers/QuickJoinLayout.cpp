/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "QuickJoinLayout.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QVariantMap>

#include "servers/FeaturedServers.h"
#include "servers/ServerPinger.h"

namespace
{
	const QString kOwnPrefix = QStringLiteral("own:");

	QStringList stringsOf(const QJsonValue& value)
	{
		QStringList out;
		for (const QJsonValue& item : value.toArray()) {
			const QString text = item.toString().trimmed();
			if (!text.isEmpty() && !out.contains(text)) {
				out.append(text);
			}
		}
		return out;
	}

	QVariantMap ownEntry(const QuickJoinLayout::Own& own, bool hidden)
	{
		return QVariantMap{
			{"id", own.id},
			{"name", own.name.isEmpty() ? own.address : own.name},
			{"address", own.address},
			{"note", QString()},
			{"minVersion", QString()},
			{"maxVersion", QString()},
			{"versions", QStringList()},
			{"featured", false},
			{"own", true},
			{"ownId", own.id},
			{"hidden", hidden},
		};
	}

	/* Places each entry once, in the order asked, by the rules in the header. */
	class Merger
	{
	  public:
		Merger(const QVariantList& featured, const QuickJoinLayout::Layout& layout)
			: m_hidden(layout.hidden.cbegin(), layout.hidden.cend())
		{
			for (const QVariant& value : featured) {
				const QVariantMap server = value.toMap();
				const QString id = server.value("id").toString();
				m_featured.insert(id, server);
				m_featuredByKey.insert(ServerPinger::keyOf(server.value("address").toString()), id);
			}
			for (const QuickJoinLayout::Own& own : layout.own) {
				m_own.insert(own.id, own);
				m_ownByKey.insert(ServerPinger::keyOf(own.address), own.id);
			}
		}

		void place(const QString& id)
		{
			if (m_placed.contains(id)) {
				return;
			}
			if (m_featured.contains(id)) {
				placeFeatured(id);
			} else if (m_own.contains(id)) {
				const QuickJoinLayout::Own own = m_own.value(id);
				const QString twin = m_featuredByKey.value(ServerPinger::keyOf(own.address));
				if (!twin.isEmpty()) {
					placeFeatured(twin);
					return;
				}
				m_placed.insert(id);
				m_entries.append(ownEntry(own, m_hidden.contains(id)));
			}
			/* Anything else is a big server gone from the list: dropped. */
		}

		QVariantList entries() const
		{
			return m_entries;
		}

	  private:
		void placeFeatured(const QString& id)
		{
			if (m_placed.contains(id)) {
				return;
			}
			QVariantMap entry = m_featured.value(id);
			const QString ownId = m_ownByKey.value(ServerPinger::keyOf(entry.value("address").toString()));
			m_placed.insert(id);
			if (!ownId.isEmpty()) {
				m_placed.insert(ownId);
			}
			entry.insert("featured", true);
			entry.insert("own", !ownId.isEmpty());
			entry.insert("ownId", ownId);
			entry.insert("hidden", m_hidden.contains(id));
			m_entries.append(entry);
		}

		QHash<QString, QVariantMap> m_featured;
		QHash<QString, QString> m_featuredByKey;
		QHash<QString, QuickJoinLayout::Own> m_own;
		QHash<QString, QString> m_ownByKey;
		QSet<QString> m_hidden;
		QSet<QString> m_placed;
		QVariantList m_entries;
	};

	int indexOf(const QVariantList& entries, const QString& id)
	{
		for (int i = 0; i < entries.size(); ++i) {
			if (entries.at(i).toMap().value("id").toString() == id) {
				return i;
			}
		}
		return -1;
	}
} // namespace

QuickJoinLayout::Layout QuickJoinLayout::parse(const QString& json)
{
	Layout layout;
	const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
	layout.order = stringsOf(root.value("order"));
	layout.hidden = stringsOf(root.value("hidden"));
	QSet<QString> ids;
	for (const QJsonValue& value : root.value("own").toArray()) {
		const QJsonObject object = value.toObject();
		const QString address = object.value("address").toString().trimmed();
		const QString id = ownId(address);
		if (id.isEmpty() || ids.contains(id)) {
			continue;
		}
		ids.insert(id);
		layout.own.append({id, object.value("name").toString().trimmed(), address});
	}
	return layout;
}

QString QuickJoinLayout::serialize(const Layout& layout)
{
	QJsonArray own;
	for (const Own& server : layout.own) {
		own.append(QJsonObject{
			{"id", server.id}, {"name", server.name}, {"address", server.address}});
	}
	const QJsonObject root{
		{"order", QJsonArray::fromStringList(layout.order)},
		{"hidden", QJsonArray::fromStringList(layout.hidden)},
		{"own", own},
	};
	return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QVariantList QuickJoinLayout::merge(const QVariantList& featured, const Layout& layout)
{
	Merger merger(featured, layout);
	for (const QString& id : layout.order) {
		merger.place(id);
	}
	for (const QVariant& server : featured) {
		merger.place(server.toMap().value("id").toString());
	}
	for (const Own& own : layout.own) {
		merger.place(own.id);
	}
	return merger.entries();
}

QuickJoinLayout::Layout QuickJoinLayout::layoutOf(const QVariantList& entries)
{
	Layout layout;
	for (const QVariant& value : entries) {
		const QVariantMap entry = value.toMap();
		const QString id = entry.value("id").toString();
		const bool hidden = entry.value("hidden").toBool();
		const bool own = entry.value("own").toBool();
		const QString address = entry.value("address").toString();
		if (entry.value("featured").toBool()) {
			layout.order.append(id);
			if (hidden) {
				layout.hidden.append(id);
			}
		}
		if (!own) {
			continue;
		}
		const QString mine = ownId(address);
		if (mine.isEmpty()) {
			continue;
		}
		/* Next to its big twin, so it keeps the place once the twin is gone. */
		layout.order.append(mine);
		if (hidden && !entry.value("featured").toBool()) {
			layout.hidden.append(mine);
		}
		layout.own.append({mine, entry.value("name").toString().trimmed(), address});
	}
	return layout;
}

QVariantList QuickJoinLayout::moveShown(const QVariantList& entries, const QString& id, int to)
{
	const int from = indexOf(entries, id);
	if (from < 0) {
		return entries;
	}
	QVariantList rest = entries;
	const QVariant moving = rest.takeAt(from);
	QList<int> shownAt;
	for (int i = 0; i < rest.size(); ++i) {
		if (!rest.at(i).toMap().value("hidden").toBool()) {
			shownAt.append(i);
		}
	}
	const int target = qMax(0, to);
	const int insertAt = target < shownAt.size() ? shownAt.at(target)
						 : shownAt.isEmpty()	 ? int(rest.size())
												 : shownAt.last() + 1;
	rest.insert(insertAt, moving);
	return rest;
}

QString QuickJoinLayout::ownId(const QString& address)
{
	const QString key = ServerPinger::keyOf(address.trimmed());
	return key.isEmpty() ? QString() : kOwnPrefix + key;
}

QuickJoinLayout::QuickJoinLayout(FeaturedServers* featured, SettingsObjectPtr settings,
								 QObject* parent)
	: QObject(parent), m_featured(featured), m_settings(std::move(settings))
{
	if (m_featured) {
		connect(m_featured, &FeaturedServers::changed, this, &QuickJoinLayout::rebuild);
	}
	rebuild();
}

QVariantList QuickJoinLayout::featured() const
{
	return m_featured ? m_featured->servers() : QVariantList();
}

void QuickJoinLayout::rebuild()
{
	const QString stored = m_settings ? m_settings->get(kLayoutSetting).toString() : QString();
	m_entries = merge(featured(), parse(stored));
	emit changed();
}

void QuickJoinLayout::save(const Layout& layout)
{
	if (m_settings) {
		m_settings->set(kLayoutSetting, serialize(layout));
	}
	rebuild();
}

QVariantList QuickJoinLayout::shown() const
{
	QVariantList out;
	for (const QVariant& entry : m_entries) {
		if (!entry.toMap().value("hidden").toBool()) {
			out.append(entry);
		}
	}
	return out;
}

bool QuickJoinLayout::hintSeen() const
{
	return !m_settings || m_settings->get(kHintSetting).toBool();
}

void QuickJoinLayout::move(const QString& id, int to)
{
	save(layoutOf(moveShown(m_entries, id, to)));
}

void QuickJoinLayout::remove(const QString& id)
{
	const int at = indexOf(m_entries, id);
	if (at < 0) {
		return;
	}
	QVariantList entries = m_entries;
	QVariantMap entry = entries.at(at).toMap();
	if (entry.value("featured").toBool()) {
		entry.insert("hidden", true);
		entry.insert("own", false);
		entries[at] = entry;
	} else {
		entries.removeAt(at);
	}
	save(layoutOf(entries));
}

bool QuickJoinLayout::pin(const QString& name, const QString& address)
{
	const QString mine = ownId(address);
	if (mine.isEmpty()) {
		return false;
	}
	const QString key = ServerPinger::keyOf(address.trimmed());
	QVariantList entries = m_entries;
	for (int i = 0; i < entries.size(); ++i) {
		QVariantMap entry = entries.at(i).toMap();
		if (ServerPinger::keyOf(entry.value("address").toString()) != key) {
			continue;
		}
		entry.insert("hidden", false);
		entry.insert("own", true);
		entries[i] = entry;
		save(layoutOf(entries));
		return true;
	}
	const QString trimmed = name.trimmed();
	entries.append(ownEntry({mine, trimmed, address.trimmed()}, false));
	save(layoutOf(entries));
	return true;
}

void QuickJoinLayout::unpin(const QString& address)
{
	const QString key = ServerPinger::keyOf(address.trimmed());
	for (const QVariant& value : m_entries) {
		const QVariantMap entry = value.toMap();
		if (!key.isEmpty() && ServerPinger::keyOf(entry.value("address").toString()) == key) {
			remove(entry.value("id").toString());
			return;
		}
	}
}

bool QuickJoinLayout::isShown(const QString& address) const
{
	const QString key = ServerPinger::keyOf(address.trimmed());
	if (key.isEmpty()) {
		return false;
	}
	for (const QVariant& value : m_entries) {
		const QVariantMap entry = value.toMap();
		if (!entry.value("hidden").toBool() &&
			ServerPinger::keyOf(entry.value("address").toString()) == key) {
			return true;
		}
	}
	return false;
}

bool QuickJoinLayout::isAddress(const QString& address) const
{
	return !ownId(address).isEmpty();
}

void QuickJoinLayout::apply(const QVariantList& entries)
{
	save(layoutOf(entries));
}

QVariantList QuickJoinLayout::defaults() const
{
	return merge(featured(), Layout());
}

void QuickJoinLayout::dismissHint()
{
	if (m_settings && !hintSeen()) {
		m_settings->set(kHintSetting, true);
		emit changed();
	}
}
