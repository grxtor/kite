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

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "settings/SettingsObject.h"

class FeaturedServers;

/*
 * Home's quick-join bar as the player arranged it: the big servers
 * (FeaturedServers) and their own, in their order, some big ones hidden. Kept
 * in the QuickJoinLayout setting as
 *
 *   {"order": ["hypixel", "own:mc.example.com:25565"], "hidden": ["cubecraft"],
 *    "own": [{"id": "own:mc.example.com:25565", "name": "Ours",
 *             "address": "mc.example.com"}]}
 *
 * and merged with the big servers whenever either changes:
 *  - a big server new to the list comes at the end;
 *  - a big server the player hid stays hidden;
 *  - a big server gone from the list goes, unless the player pinned it as
 *    their own (then it stays, as theirs, where it was);
 *  - their own servers stay until they remove them.
 * An empty setting is the big servers as they come.
 *
 * An entry is a FeaturedServers.servers map (id, name, address, note,
 * minVersion, maxVersion, versions) plus `featured` (a big server), `own` (the
 * player added or pinned it), `ownId` and `hidden`. An own server's id is
 * ownId(); it takes any Minecraft version.
 */
class QuickJoinLayout : public QObject
{
	Q_OBJECT

	/// Every entry, hidden ones too, in the bar's order.
	Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
	/// The entries the bar shows.
	Q_PROPERTY(QVariantList shown READ shown NOTIFY changed)
	/// The one-time hint on arranging the bar was closed.
	Q_PROPERTY(bool hintSeen READ hintSeen NOTIFY changed)

  public:
	static constexpr const char* kLayoutSetting = "QuickJoinLayout";
	static constexpr const char* kHintSetting = "QuickJoinHintSeen";

	struct Own
	{
		QString id;
		QString name;
		QString address;
	};
	struct Layout
	{
		QStringList order;
		QStringList hidden;
		QList<Own> own;
	};

	/// The setting's value; an empty layout for "" or anything unreadable.
	static Layout parse(const QString& json);
	static QString serialize(const Layout& layout);
	/// The bar for the big servers @p featured (FeaturedServers.servers)
	/// arranged as @p layout says, by the rules above.
	static QVariantList merge(const QVariantList& featured, const Layout& layout);
	/// What the setting keeps for @p entries, given in order with their
	/// `hidden` and `own` as merge() names them.
	static Layout layoutOf(const QVariantList& entries);
	/// @p entries with shown entry @p id moved to position @p to among the
	/// shown ones; the hidden keep their places among the rest.
	static QVariantList moveShown(const QVariantList& entries, const QString& id,
								  int to);
	/// "own:" and ServerPinger::keyOf(@p address); "" for no host.
	static QString ownId(const QString& address);

	/// Follows @p featured's list; reads and writes @p settings, which needs
	/// kLayoutSetting and kHintSetting registered.
	QuickJoinLayout(FeaturedServers* featured, SettingsObjectPtr settings,
					QObject* parent = nullptr);

	QVariantList entries() const
	{
		return m_entries;
	}
	QVariantList shown() const;
	bool hintSeen() const;

	/// A drag in the bar dropped entry @p id at shown position @p to.
	Q_INVOKABLE void move(const QString& id, int to);
	/// "Remove from bar": a big server is hidden, one of the player's own
	/// removed (a pinned big server both).
	Q_INVOKABLE void remove(const QString& id);
	/// "Pin to top bar" and "Add server…": @p address joins the bar as the
	/// player's own, named @p name (else its address), at the end; a big
	/// server with that address is shown again and kept as theirs. False for
	/// an address with no host.
	Q_INVOKABLE bool pin(const QString& name, const QString& address);
	/// "Remove from top bar" for @p address, as remove() does for its entry.
	Q_INVOKABLE void unpin(const QString& address);
	/// Whether the bar shows @p address.
	Q_INVOKABLE bool isShown(const QString& address) const;
	/// Whether @p address names a server at all (it has a host).
	Q_INVOKABLE bool isAddress(const QString& address) const;
	/// ownId() for QML: two addresses of one server give the same.
	Q_INVOKABLE QString ownIdOf(const QString& address) const
	{
		return ownId(address);
	}
	/// The edit dialog's Save: @p entries as it left them (in order, with
	/// `hidden`; the own servers it removed left out or with `own` false).
	Q_INVOKABLE void apply(const QVariantList& entries);
	/// The big servers as they come, for the edit dialog's "Reset to default".
	Q_INVOKABLE QVariantList defaults() const;
	Q_INVOKABLE void dismissHint();

  signals:
	void changed();

  private:
	void rebuild();
	void save(const Layout& layout);
	QVariantList featured() const;

	QPointer<FeaturedServers> m_featured;
	SettingsObjectPtr m_settings;
	QVariantList m_entries;
};
