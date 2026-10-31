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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <functional>
#include <memory>

class QWidget;
class QMenu;

/* Turns an "mmco-ui/1" JSON document (MMCOContext::ui_surface_create, PluginAPI.h S33) into a live QWidget
 * tree. It builds real widgets, so it lives in Kite_logic rather than the Qt-free SDK.
 *
 * Fourteen node types:
 *   containers (have "children")  column, row, section
 *   content leaves                heading, text, separator, progress
 *   interactive leaves            button, toggle, text_field, number_field, choice, list, link
 *
 * A node may carry a plugin-assigned "id" (events, api_ui_surface_set / _set_rows) and a flat "props"
 * object. No colour/font/geometry prop exists: the host owns presentation.
 *
 * A RenderedSurface owns one top-level QWidget (rootWidget()), created once and never replaced;
 * setDocument() rebuilds *inside* it, so a caller that parented it never reparents. RenderedSurface does
 * not delete rootWidget(): whoever parents it owns it the normal Qt way. */
class PluginUiRenderer
{
  public:
	/* nodeId, event ("click" / "change" / "select" / "activate"), value_json (JSON-encoded, or ""
	 * when not applicable). */
	using EventSink = std::function<void(
		const QString& nodeId, const QString& event, const QString& valueJson)>;

	class RenderedSurface
	{
	  public:
		explicit RenderedSurface(EventSink sink);
		~RenderedSurface();

		RenderedSurface(const RenderedSurface&) = delete;
		RenderedSurface& operator=(const RenderedSurface&) = delete;

		/* The stable top-level widget; never null after construction, never replaced. */
		QWidget* rootWidget() const
		{
			return m_root.data();
		}

		/* Parses and (re)builds the whole tree; false on malformed JSON or a missing root
		 * (rootWidget() is then left empty). */
		bool setDocument(const QJsonObject& doc);

		/* Patches one node's `props` in place (merged over the existing ones); false if node_id is
		 * unknown or the node type does not understand a key. */
		bool setNodeProps(const QString& nodeId, const QJsonObject& props);

		/* Replaces a `list` node's `rows`, leaving `columns` alone; false if node_id is not a
		 * `list`. */
		bool setRows(const QString& nodeId, const QJsonArray& rows);

		/* The node type for `nodeId` ("button", "toggle", ...), or empty if unknown; ui_modal_run
		 * uses it to spot which button fired. */
		QString nodeType(const QString& nodeId) const;

		/* Current value of every interactive node, keyed by node id (ui_modal_run's "fields"). */
		QJsonObject collectValues() const;

	  private:
		struct NodeEntry {
			QWidget* widget = nullptr; /* the interactive/leaf widget itself */
			QString type;
		};

		QWidget* renderNode(const QJsonObject& node);
		QWidget* wrapWithLabel(const QString& label, QWidget* control);
		void clearRoot();

		EventSink m_sink;
		/* Stable identity, content rebuilt in place. A QPointer because the parent may delete it
		 * first: a torn-down settings group box deletes its children before the RendererOwner
		 * sibling that owns this surface. */
		QPointer<QWidget> m_root;
		QHash<QString, NodeEntry> m_nodes;
	};

	/* Parses `jsonDoc` and builds a live surface. Never null: a malformed document renders empty
	 * (setDocument()'s failure is logged via qWarning). */
	static std::unique_ptr<RenderedSurface> build(const QString& jsonDoc,
												  EventSink sink);

	/* Parses a tray-menu document (button/separator/section nodes; section = submenu) and populates
	 * `menu` with QActions wired to sink(nodeId, "click", ""). False on malformed JSON. */
	static bool buildTrayMenu(QMenu* menu, const QString& jsonDoc,
							 EventSink sink);
};
