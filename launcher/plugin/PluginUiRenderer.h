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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <functional>
#include <memory>

class QWidget;
class QMenu;

/*
 * PluginUiRenderer — turns an "mmco-ui/1" JSON document (the format
 * MMCOContext::ui_surface_create / ui_surface_update / ui_modal_run
 * take, see PluginAPI.h S33) into a live QWidget tree, MeshMC_logic
 * side (it builds real widgets, so it cannot live in the Qt-free SDK).
 *
 * Fourteen node types are understood:
 *   Containers (have "children"):        column, row, section
 *   Content leaves:                      heading, text, separator,
 *                                         progress
 *   Interactive leaves:                  button, toggle, text_field,
 *                                         number_field, choice, list,
 *                                         link
 *
 * Every node may carry a plugin-assigned "id" (used for events and for
 * PluginManager::api_ui_surface_set / _set_rows) and a flat "props"
 * object. No colour/font/geometry prop exists on any node — the host
 * owns all presentation.
 *
 * A RenderedSurface owns exactly one top-level QWidget (rootWidget()),
 * created once and never replaced — setDocument() rebuilds everything
 * *inside* it, so a caller that has parented rootWidget() somewhere
 * never has to reparent anything again. RenderedSurface does not
 * delete rootWidget() itself; whoever parents it into a page/dialog
 * owns it the normal Qt way (parent-child cascade on destruction).
 */
class PluginUiRenderer
{
  public:
	/* nodeId, event ("click" / "change" / "select" / "activate"),
	 * value_json (a JSON-encoded value, or "" when not applicable). */
	using EventSink = std::function<void(
		const QString& nodeId, const QString& event, const QString& valueJson)>;

	class RenderedSurface
	{
	  public:
		explicit RenderedSurface(EventSink sink);
		~RenderedSurface();

		RenderedSurface(const RenderedSurface&) = delete;
		RenderedSurface& operator=(const RenderedSurface&) = delete;

		/* The single stable top-level widget. Never null after
		 * construction; never replaced by any of the calls below. */
		QWidget* rootWidget() const
		{
			return m_root;
		}

		/* Parse and (re)build the whole tree from an "mmco-ui/1"
		 * document. Returns false on malformed JSON / missing root,
		 * in which case rootWidget() is left empty. */
		bool setDocument(const QJsonObject& doc);

		/* Patch one node's `props` in place (merged over the existing
		 * props for that node). Returns false if node_id is unknown or
		 * the node type doesn't understand one of the given keys. */
		bool setNodeProps(const QString& nodeId, const QJsonObject& props);

		/* Replace a `list` node's `rows` array in place, leaving its
		 * `columns` untouched. Returns false if node_id doesn't name a
		 * `list` node. */
		bool setRows(const QString& nodeId, const QJsonArray& rows);

		/* Look up the node type for `nodeId` ("button", "toggle", …),
		 * or an empty string if unknown. Used by ui_modal_run to spot
		 * which button fired. */
		QString nodeType(const QString& nodeId) const;

		/* Current value of every interactive node (toggle/text_field/
		 * number_field/choice), keyed by node id — used to build
		 * ui_modal_run's "fields" result object. */
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
		QWidget* m_root; /* stable identity; content rebuilt in place */
		QHash<QString, NodeEntry> m_nodes;
	};

	/* Parse `jsonDoc` (an "mmco-ui/1" document) and build a live
	 * surface. Never returns null — a malformed document renders as an
	 * empty surface (setDocument()'s return value already logged the
	 * problem via qWarning). */
	static std::unique_ptr<RenderedSurface> build(const QString& jsonDoc,
												  EventSink sink);

	/* Parse a tray-menu document (small tree of button/separator/section
	 * nodes — section = submenu) and populate `menu` with QActions wired
	 * to `sink(nodeId, "click", "")`. Returns false on malformed JSON. */
	static bool buildTrayMenu(QMenu* menu, const QString& jsonDoc,
							 EventSink sink);
};
