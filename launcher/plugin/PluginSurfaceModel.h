/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "plugin/PluginManager.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

/* QAbstractListModel over PluginManager::surfaces(anchor, anchorContext): one row per live surface at that
 * (anchor, anchorContext). QmlShell hands one out per pair via QmlShell::pluginSurfaces().
 *
 * Every PluginManager::surfacesChanged() does a full reset: the signal says nothing about what changed, so
 * diffing would need a shadow copy of every surface's document, for a handful of rows at most.
 *
 * `revision` lets a QML delegate bound to `document` tell that this surface changed: it is bumped per
 * surface whenever the raw document text differs from what the model last saw, and carried across resets
 * by surface id. */
class PluginSurfaceModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(int anchor READ anchor WRITE setAnchor NOTIFY anchorChanged)
	Q_PROPERTY(QString anchorContext READ anchorContext WRITE setAnchorContext
				   NOTIFY anchorContextChanged)

  public:
	enum Role {
		SurfaceIdRole = Qt::UserRole + 1,
		TitleRole,
		IconNameRole,
		AnchorRole,
		AnchorContextRole,
		DocumentRole,
		RevisionRole,
	};

	/* `manager` is not owned and must outlive the model (Application destroys its QmlShell before its
	 * PluginManager). `anchor` -1 means every anchor and a null `anchorContext` every context, as in
	 * PluginManager::surfaces() (an empty non-null QString matches only GLOBAL_SETTINGS surfaces). */
	explicit PluginSurfaceModel(PluginManager* manager, int anchor = -1,
								const QString& anchorContext = QString(),
								QObject* parent = nullptr);

	int anchor() const;
	void setAnchor(int anchor);
	QString anchorContext() const;
	void setAnchorContext(const QString& anchorContext);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* Serialises `value` as PluginUiRenderer does for value_json: bool becomes `true`/`false`, a number its
	 * plain text, anything else a quoted JSON string; an invalid QVariant (a button click) becomes "". */
	static QString valueToJson(const QVariant& value);

	/* Delivers a UI event to the plugin behind `surfaceId` (value goes through valueToJson()). No-op if
	 * surfaceId names no live surface or the model has no manager. */
	Q_INVOKABLE void sendEvent(const QString& surfaceId, const QString& nodeId,
							   const QString& event, const QVariant& value);

  signals:
	void anchorChanged();
	void anchorContextChanged();

  protected:
	/* Test seam: forwards to m_manager->surfaces(m_anchor, m_anchorContext) by default; tests override it
	 * to feed a canned list (PluginSurfaceModel_test.cpp), then call refresh(). */
	virtual QList<PluginManager::SurfaceInfo> fetchSurfaces() const;

	/* Re-reads fetchSurfaces() and resets the model. Connected to PluginManager::surfacesChanged() when
	 * `manager` is non-null. */
	void refresh();

  private:
	struct Row {
		QString surfaceId;
		QString title;
		QString iconName;
		int anchor = 0;
		QString anchorContext;
		QVariantMap document;
		int revision = 0;
	};

	PluginManager* m_manager;
	int m_anchor;
	QString m_anchorContext;
	QVector<Row> m_rows;

	/* Per-surface-id revision counters carried across refresh()es; pruned to the ids seen in the latest one. */
	QHash<QString, int> m_revisions;
	QHash<QString, QString> m_lastDocuments;
};
