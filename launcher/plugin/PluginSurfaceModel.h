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

#include "plugin/PluginManager.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

/*
 * PluginSurfaceModel — a QAbstractListModel over
 * PluginManager::surfaces(anchor, anchorContext), the seam ABI 5's
 * declarative UI surfaces already expose for a renderer that isn't
 * PluginUiRenderer's QWidget tree. One row per live surface at the
 * (anchor, anchorContext) this model was created for; QmlShell hands one
 * out per pair via QmlShell::pluginSurfaces() (see that class for why the
 * factory indirection exists — this class lives in MeshMC_logic, next to
 * PluginManager, and MeshMC_qml cannot see either).
 *
 * Refresh strategy: every PluginManager::surfacesChanged() re-reads the
 * whole (anchor, anchorContext) slice and does a full beginResetModel() /
 * endResetModel(), rather than diffing old/new rows into targeted
 * inserts/removes/dataChanged(). surfacesChanged() carries no information
 * about *what* changed — not even which surface — so anything smarter
 * would mean tracking our own shadow copy of every surface's document just
 * to diff against it, for an event that fires at most once per user click
 * or plugin-initiated patch. A full reset is O(surfaces at this anchor),
 * which is a handful of rows in the worst case (see GLOBAL_SETTINGS
 * stacking every plugin's section into one page today) — simpler, and
 * cheap enough not to matter.
 *
 * `revision` exists so a QML delegate bound to `document` can tell *this*
 * surface's document changed (as opposed to some other row in the same
 * reset) without comparing the JSON itself: it is bumped once per surface
 * whenever that surface's raw document text differs from what the model
 * last saw, and carried forward across resets by surface id.
 */
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

	/*
	 * `manager` is not owned and must outlive this model — true for every
	 * instance QmlShell::pluginSurfaces() hands out, since Application
	 * destroys its QmlShell (and everything it cached) before its
	 * PluginManager (see Application.h's member order). `anchor` of -1
	 * means "every anchor"; a null (default-constructed) `anchorContext`
	 * means "every context" — both match PluginManager::surfaces()'s own
	 * filter semantics exactly (a real-but-empty QString("") matches only
	 * GLOBAL_SETTINGS surfaces, which always have an empty context).
	 */
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

	/*
	 * Serialises `value` the same way PluginUiRenderer.cpp's widget
	 * renderer serialises a node's live value into value_json for
	 * MMCOUiEventCallback: a bool becomes the bare `true`/`false` literal,
	 * a number becomes its plain text, anything else (string, a list row's
	 * id, …) becomes a quoted JSON string literal. An invalid/null QVariant
	 * — a button click, which carries no value — becomes the empty string,
	 * exactly like PluginUiRenderer's own button handler passes to sink().
	 */
	static QString valueToJson(const QVariant& value);

	/*
	 * Delivers a UI event to the plugin behind `surfaceId`, the QML
	 * equivalent of PluginUiRenderer's Qt signal handlers calling `sink()`
	 * — value is serialised with valueToJson() first. No-op (via
	 * PluginManager::deliverUiEvent()) if surfaceId names no live surface,
	 * or if this model was built without a manager (see the test seam
	 * below).
	 */
	Q_INVOKABLE void sendEvent(const QString& surfaceId, const QString& nodeId,
							   const QString& event, const QVariant& value);

  signals:
	void anchorChanged();
	void anchorContextChanged();

  protected:
	/*
	 * Seam for tests: by default forwards to
	 * `m_manager->surfaces(m_anchor, m_anchorContext)`. A test subclass
	 * overrides this to feed a canned SurfaceInfo list without a real
	 * PluginManager backed by loaded plugins (see
	 * PluginSurfaceModel_test.cpp), then calls the also-protected
	 * refresh() to re-read it.
	 */
	virtual QList<PluginManager::SurfaceInfo> fetchSurfaces() const;

	/* Re-reads fetchSurfaces() and resets the model; see the refresh
	 * strategy note above. Connected to PluginManager::surfacesChanged()
	 * in the constructor when `manager` is non-null. */
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

	/* Per-surface-id bookkeeping carried across refresh()es so `revision`
	 * keeps counting even though m_rows itself is rebuilt from scratch
	 * every time (see the refresh-strategy note above). Pruned to the
	 * surface ids seen in the most recent refresh() so a destroyed
	 * surface's id doesn't linger forever. */
	QHash<QString, int> m_revisions;
	QHash<QString, QString> m_lastDocuments;
};
