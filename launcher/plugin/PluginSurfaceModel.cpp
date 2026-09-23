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

#include "plugin/PluginSurfaceModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaType>
#include <QSet>

namespace
{
	/* Mirrors PluginUiRenderer.cpp's (anonymous-namespace, so unreachable
	 * from here) jsonQuoteString(): the widget renderer's own convention
	 * for a string value_json (text_field/choice change, list select/
	 * activate). Duplicated rather than exported from PluginUiRenderer for
	 * one extra caller — six lines, and the convention itself is what
	 * needs to match, not the code. */
	QString quoteJsonString(const QString& s)
	{
		QJsonArray tmp;
		tmp.append(s);
		QByteArray bytes = QJsonDocument(tmp).toJson(QJsonDocument::Compact);
		if (bytes.size() >= 2)
			bytes = bytes.mid(1, bytes.size() - 2);
		return QString::fromUtf8(bytes);
	}
} // namespace

PluginSurfaceModel::PluginSurfaceModel(PluginManager* manager, int anchor,
									   const QString& anchorContext,
									   QObject* parent)
	: QAbstractListModel(parent), m_manager(manager), m_anchor(anchor),
	  m_anchorContext(anchorContext)
{
	if (m_manager) {
		connect(m_manager, &PluginManager::surfacesChanged, this,
				&PluginSurfaceModel::refresh);
	}
	/* Virtual dispatch during construction always resolves to this class,
	 * never a subclass's override — so a fetchSurfaces() test seam (see
	 * PluginSurfaceModel.h) only takes effect once construction has
	 * finished and the subclass calls refresh() itself, same as everyone
	 * else changing what fetchSurfaces() would return. This first call
	 * just seeds m_rows from `manager` (or empty, with a null one). */
	refresh();
}

int PluginSurfaceModel::anchor() const
{
	return m_anchor;
}

void PluginSurfaceModel::setAnchor(int anchor)
{
	if (m_anchor == anchor)
		return;
	m_anchor = anchor;
	emit anchorChanged();
	refresh();
}

QString PluginSurfaceModel::anchorContext() const
{
	return m_anchorContext;
}

void PluginSurfaceModel::setAnchorContext(const QString& anchorContext)
{
	if (m_anchorContext == anchorContext)
		return;
	m_anchorContext = anchorContext;
	emit anchorContextChanged();
	refresh();
}

int PluginSurfaceModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;
	return m_rows.size();
}

QVariant PluginSurfaceModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
		return QVariant();

	const Row& row = m_rows.at(index.row());
	switch (role) {
	case SurfaceIdRole:
		return row.surfaceId;
	case TitleRole:
		return row.title;
	case IconNameRole:
		return row.iconName;
	case AnchorRole:
		return row.anchor;
	case AnchorContextRole:
		return row.anchorContext;
	case DocumentRole:
		return row.document;
	case RevisionRole:
		return row.revision;
	default:
		return QVariant();
	}
}

QHash<int, QByteArray> PluginSurfaceModel::roleNames() const
{
	return {
		{ SurfaceIdRole, "surfaceId" },
		{ TitleRole, "title" },
		{ IconNameRole, "iconName" },
		{ AnchorRole, "anchor" },
		{ AnchorContextRole, "anchorContext" },
		{ DocumentRole, "document" },
		{ RevisionRole, "revision" },
	};
}

QList<PluginManager::SurfaceInfo> PluginSurfaceModel::fetchSurfaces() const
{
	if (!m_manager)
		return {};
	return m_manager->surfaces(m_anchor, m_anchorContext);
}

void PluginSurfaceModel::refresh()
{
	beginResetModel();

	const auto infos = fetchSurfaces();
	QSet<QString> seen;
	seen.reserve(infos.size());
	m_rows.clear();
	m_rows.reserve(infos.size());

	for (const auto& info : infos) {
		seen.insert(info.surfaceId);

		int& revision = m_revisions[info.surfaceId];
		QString& lastDocument = m_lastDocuments[info.surfaceId];
		if (lastDocument != info.document) {
			++revision;
			lastDocument = info.document;
		}

		Row row;
		row.surfaceId = info.surfaceId;
		row.title = info.title;
		row.iconName = info.iconName;
		row.anchor = info.anchor;
		row.anchorContext = info.anchorContext;
		row.revision = revision;

		QJsonParseError err{};
		const QJsonDocument doc =
			QJsonDocument::fromJson(info.document.toUtf8(), &err);
		if (err.error == QJsonParseError::NoError && doc.isObject())
			row.document = doc.object().toVariantMap();

		m_rows.append(row);
	}

	// Drop bookkeeping for surfaces that no longer exist, so a destroyed
	// surface's id doesn't linger in these maps forever.
	for (auto it = m_revisions.begin(); it != m_revisions.end();) {
		if (seen.contains(it.key()))
			++it;
		else
			it = m_revisions.erase(it);
	}
	for (auto it = m_lastDocuments.begin(); it != m_lastDocuments.end();) {
		if (seen.contains(it.key()))
			++it;
		else
			it = m_lastDocuments.erase(it);
	}

	endResetModel();
}

QString PluginSurfaceModel::valueToJson(const QVariant& value)
{
	if (!value.isValid() || value.isNull())
		return QString();

	switch (value.userType()) {
	case QMetaType::Bool:
		return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
	case QMetaType::Short:
	case QMetaType::UShort:
	case QMetaType::Int:
	case QMetaType::UInt:
	case QMetaType::Long:
	case QMetaType::ULong:
	case QMetaType::LongLong:
	case QMetaType::ULongLong:
		return QString::number(value.toLongLong());
	case QMetaType::Double:
	case QMetaType::Float:
		return QString::number(value.toDouble());
	default:
		break;
	}
	// Anything else -- a string, or a list row's id -- quoted the same way
	// a text_field/choice change or a list select/activate is.
	return quoteJsonString(value.toString());
}

void PluginSurfaceModel::sendEvent(const QString& surfaceId,
								   const QString& nodeId, const QString& event,
								   const QVariant& value)
{
	if (!m_manager)
		return;
	m_manager->deliverUiEvent(surfaceId, nodeId, event, valueToJson(value));
}
