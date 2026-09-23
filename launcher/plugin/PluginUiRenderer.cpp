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

#include "plugin/PluginUiRenderer.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
	/* Encode `s` as a JSON string literal (quoted, escaped) — used for
	 * every value_json payload whose value is a string. */
	QString jsonQuoteString(const QString& s)
	{
		QJsonArray tmp;
		tmp.append(s);
		QByteArray bytes = QJsonDocument(tmp).toJson(QJsonDocument::Compact);
		if (bytes.size() >= 2)
			bytes = bytes.mid(1, bytes.size() - 2);
		return QString::fromUtf8(bytes);
	}

	/* Clear and rebuild a `list` node's rows. Shared by initial render
	 * and setRows() so the two never drift apart. */
	void populateListRows(QTreeWidget* tree, const QJsonArray& rows)
	{
		tree->clear();
		for (const QJsonValue& rv : rows) {
			const QJsonObject row = rv.toObject();
			const QJsonArray cells = row.value(QStringLiteral("cells")).toArray();
			auto* item = new QTreeWidgetItem(tree);
			for (int i = 0; i < cells.size(); ++i)
				item->setText(i, cells.at(i).toString());
			item->setData(0, Qt::UserRole, row.value(QStringLiteral("id")).toString());
			if (row.contains(QStringLiteral("data")))
				item->setData(0, Qt::UserRole + 1,
							  row.value(QStringLiteral("data")).toString());
		}
	}
} // namespace

PluginUiRenderer::RenderedSurface::RenderedSurface(EventSink sink)
	: m_sink(std::move(sink)), m_root(new QWidget())
{
}

PluginUiRenderer::RenderedSurface::~RenderedSurface()
{
	/* QObject::~QObject() unlinks itself from its parent's children
	 * list before this destructor returns, so this is safe whether or
	 * not m_root has since been parented into a page/dialog that Qt
	 * will also tear down. */
	delete m_root;
}

void PluginUiRenderer::RenderedSurface::clearRoot()
{
	m_nodes.clear();
	if (QLayout* old = m_root->layout()) {
		QLayoutItem* item;
		while ((item = old->takeAt(0)) != nullptr) {
			if (QWidget* w = item->widget())
				delete w;
			delete item;
		}
		delete old;
	}
}

QWidget* PluginUiRenderer::RenderedSurface::wrapWithLabel(const QString& label,
														  QWidget* control)
{
	if (label.isEmpty())
		return control;
	auto* row = new QWidget();
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(new QLabel(label));
	layout->addWidget(control, 1);
	return row;
}

QWidget* PluginUiRenderer::RenderedSurface::renderNode(const QJsonObject& node)
{
	const QString type = node.value(QStringLiteral("type")).toString();
	const QString id = node.value(QStringLiteral("id")).toString();
	const QJsonObject props = node.value(QStringLiteral("props")).toObject();

	QWidget* widget = nullptr;	   /* inserted into the parent layout */
	QWidget* indexTarget = nullptr; /* touched by set()/collectValues() */
	EventSink sink = m_sink;

	if (type == QLatin1String("column") || type == QLatin1String("row")) {
		auto* container = new QWidget();
		QBoxLayout* layout =
			(type == QLatin1String("row"))
				? static_cast<QBoxLayout*>(new QHBoxLayout(container))
				: static_cast<QBoxLayout*>(new QVBoxLayout(container));
		layout->setContentsMargins(0, 0, 0, 0);
		const QJsonArray children = node.value(QStringLiteral("children")).toArray();
		for (const QJsonValue& c : children) {
			if (QWidget* cw = renderNode(c.toObject()))
				layout->addWidget(cw);
		}
		widget = indexTarget = container;
	} else if (type == QLatin1String("section")) {
		auto* group = new QGroupBox(props.value(QStringLiteral("title")).toString());
		auto* layout = new QVBoxLayout(group);
		const QJsonArray children = node.value(QStringLiteral("children")).toArray();
		for (const QJsonValue& c : children) {
			if (QWidget* cw = renderNode(c.toObject()))
				layout->addWidget(cw);
		}
		widget = indexTarget = group;
	} else if (type == QLatin1String("heading")) {
		auto* label = new QLabel(props.value(QStringLiteral("text")).toString());
		QFont f = label->font();
		f.setBold(true);
		f.setPointSize(f.pointSize() + 2);
		label->setFont(f);
		label->setWordWrap(true);
		widget = indexTarget = label;
	} else if (type == QLatin1String("text")) {
		auto* label = new QLabel();
		label->setWordWrap(true);
		const QString format =
			props.value(QStringLiteral("format")).toString(QStringLiteral("plain"));
		label->setText(props.value(QStringLiteral("text")).toString());
		if (format == QLatin1String("markdown")) {
			label->setTextFormat(Qt::MarkdownText);
			label->setTextInteractionFlags(Qt::TextBrowserInteraction);
			label->setOpenExternalLinks(false);
			QString nodeId = id;
			QObject::connect(label, &QLabel::linkActivated, label,
							 [sink, nodeId](const QString& link) {
								 if (sink)
									 sink(nodeId, QStringLiteral("click"),
										 jsonQuoteString(link));
							 });
		} else {
			label->setTextFormat(Qt::PlainText);
		}
		widget = indexTarget = label;
	} else if (type == QLatin1String("separator")) {
		auto* line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		widget = indexTarget = line;
	} else if (type == QLatin1String("progress")) {
		auto* bar = new QProgressBar();
		const int value = props.value(QStringLiteral("value")).toInt(-1);
		if (value < 0)
			bar->setRange(0, 0);
		else {
			bar->setRange(0, 100);
			bar->setValue(qBound(0, value, 100));
		}
		widget = indexTarget = bar;
	} else if (type == QLatin1String("button")) {
		auto* btn = new QPushButton(props.value(QStringLiteral("label")).toString());
		btn->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
		btn->setProperty("mmcoStyle",
						 props.value(QStringLiteral("style"))
							 .toString(QStringLiteral("default")));
		QString nodeId = id;
		QObject::connect(btn, &QPushButton::clicked, btn, [sink, nodeId]() {
			if (sink)
				sink(nodeId, QStringLiteral("click"), QString());
		});
		widget = indexTarget = btn;
	} else if (type == QLatin1String("toggle")) {
		auto* chk = new QCheckBox(props.value(QStringLiteral("label")).toString());
		chk->setChecked(props.value(QStringLiteral("value")).toBool(false));
		chk->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
		QString nodeId = id;
		QObject::connect(chk, &QCheckBox::toggled, chk, [sink, nodeId](bool v) {
			if (sink)
				sink(nodeId, QStringLiteral("change"),
					v ? QStringLiteral("true") : QStringLiteral("false"));
		});
		widget = indexTarget = chk;
	} else if (type == QLatin1String("text_field")) {
		auto* edit = new QLineEdit(props.value(QStringLiteral("value")).toString());
		edit->setPlaceholderText(props.value(QStringLiteral("placeholder")).toString());
		edit->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
		QString nodeId = id;
		QObject::connect(edit, &QLineEdit::editingFinished, edit,
						 [sink, nodeId, edit]() {
							 if (sink)
								 sink(nodeId, QStringLiteral("change"),
									 jsonQuoteString(edit->text()));
						 });
		indexTarget = edit;
		widget = wrapWithLabel(props.value(QStringLiteral("label")).toString(), edit);
	} else if (type == QLatin1String("number_field")) {
		auto* spin = new QDoubleSpinBox();
		const int decimals = props.value(QStringLiteral("decimals")).toInt(0);
		spin->setDecimals(decimals);
		spin->setRange(props.value(QStringLiteral("min")).toDouble(0),
					  props.value(QStringLiteral("max")).toDouble(100));
		spin->setSingleStep(props.value(QStringLiteral("step")).toDouble(1));
		spin->setValue(props.value(QStringLiteral("value")).toDouble(0));
		spin->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
		QString nodeId = id;
		QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
						 spin, [sink, nodeId, decimals](double v) {
							 if (sink)
								 sink(nodeId, QStringLiteral("change"),
									 QString::number(v, 'f', decimals));
						 });
		indexTarget = spin;
		widget = wrapWithLabel(props.value(QStringLiteral("label")).toString(), spin);
	} else if (type == QLatin1String("choice")) {
		auto* combo = new QComboBox();
		const QJsonArray options = props.value(QStringLiteral("options")).toArray();
		for (const QJsonValue& opt : options) {
			if (opt.isObject()) {
				const QJsonObject o = opt.toObject();
				combo->addItem(o.value(QStringLiteral("label")).toString(),
							  o.value(QStringLiteral("id")).toString());
			} else {
				const QString s = opt.toString();
				combo->addItem(s, s);
			}
		}
		const int idx = qMax(
			0, combo->findData(props.value(QStringLiteral("value")).toString()));
		combo->setCurrentIndex(idx);
		combo->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
		QString nodeId = id;
		QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
						 combo, [sink, nodeId, combo](int i) {
							 if (sink)
								 sink(nodeId, QStringLiteral("change"),
									 jsonQuoteString(combo->itemData(i).toString()));
						 });
		indexTarget = combo;
		widget = wrapWithLabel(props.value(QStringLiteral("label")).toString(), combo);
	} else if (type == QLatin1String("list")) {
		auto* tree = new QTreeWidget();
		tree->setRootIsDecorated(false);
		tree->setAlternatingRowColors(true);
		tree->setSelectionMode(QAbstractItemView::SingleSelection);
		const QJsonArray columns = props.value(QStringLiteral("columns")).toArray();
		QStringList headers;
		for (const QJsonValue& c : columns)
			headers << c.toString();
		tree->setHeaderLabels(headers);
		if (!headers.isEmpty()) {
			tree->header()->setStretchLastSection(false);
			tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
			for (int i = 1; i < headers.size(); ++i)
				tree->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
		}
		populateListRows(tree, props.value(QStringLiteral("rows")).toArray());
		QString nodeId = id;
		QObject::connect(tree, &QTreeWidget::itemSelectionChanged, tree,
						 [sink, nodeId, tree]() {
							 if (!sink)
								 return;
							 const auto items = tree->selectedItems();
							 const QString rowId =
								 items.isEmpty()
									 ? QString()
									 : items.first()->data(0, Qt::UserRole).toString();
							 sink(nodeId, QStringLiteral("select"),
								 jsonQuoteString(rowId));
						 });
		QObject::connect(tree, &QTreeWidget::itemActivated, tree,
						 [sink, nodeId](QTreeWidgetItem* item, int) {
							 if (sink && item)
								 sink(nodeId, QStringLiteral("activate"),
									 jsonQuoteString(item->data(0, Qt::UserRole).toString()));
						 });
		widget = indexTarget = tree;
	} else if (type == QLatin1String("link")) {
		auto* label = new QLabel();
		const QString text = props.value(QStringLiteral("text")).toString();
		const QString href = props.value(QStringLiteral("href")).toString();
		label->setText(QStringLiteral("<a href=\"%1\">%2</a>")
						  .arg(href.toHtmlEscaped(), text.toHtmlEscaped()));
		label->setTextFormat(Qt::RichText);
		label->setTextInteractionFlags(Qt::TextBrowserInteraction);
		label->setOpenExternalLinks(false);
		QString nodeId = id;
		QObject::connect(label, &QLabel::linkActivated, label,
						 [sink, nodeId](const QString& link) {
							 if (sink)
								 sink(nodeId, QStringLiteral("click"),
									 jsonQuoteString(link));
						 });
		widget = indexTarget = label;
	} else {
		qWarning().noquote() << "[PluginUiRenderer] Unknown node type:" << type;
		return nullptr;
	}

	if (widget)
		widget->setVisible(props.value(QStringLiteral("visible")).toBool(true));
	if (!id.isEmpty() && indexTarget) {
		/* Stable objectName so tests (and any future QSS theming) can
		 * findChild<T>(id) instead of reaching into m_nodes. */
		indexTarget->setObjectName(id);
		m_nodes.insert(id, NodeEntry{indexTarget, type});
	}
	return widget;
}

bool PluginUiRenderer::RenderedSurface::setDocument(const QJsonObject& doc)
{
	clearRoot();
	const QJsonObject root = doc.value(QStringLiteral("root")).toObject();
	if (root.isEmpty()) {
		qWarning() << "[PluginUiRenderer] document has no \"root\" node";
		new QVBoxLayout(m_root); /* keep m_root layout-valid but empty */
		return false;
	}
	QWidget* content = renderNode(root);
	auto* layout = new QVBoxLayout(m_root);
	layout->setContentsMargins(0, 0, 0, 0);
	if (content) {
		layout->addWidget(content);
		return true;
	}
	qWarning() << "[PluginUiRenderer] failed to render root node";
	return false;
}

bool PluginUiRenderer::RenderedSurface::setNodeProps(const QString& nodeId,
													 const QJsonObject& props)
{
	auto it = m_nodes.find(nodeId);
	if (it == m_nodes.end() || !it->widget)
		return false;
	QWidget* w = it->widget;
	const QString& type = it->type;

	if (props.contains(QStringLiteral("visible")))
		w->setVisible(props.value(QStringLiteral("visible")).toBool(true));
	if (props.contains(QStringLiteral("enabled")))
		w->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));

	if (type == QLatin1String("section")) {
		if (auto* group = qobject_cast<QGroupBox*>(w)) {
			if (props.contains(QStringLiteral("title")))
				group->setTitle(props.value(QStringLiteral("title")).toString());
		}
	} else if (type == QLatin1String("heading") || type == QLatin1String("text")) {
		if (auto* label = qobject_cast<QLabel*>(w)) {
			if (props.contains(QStringLiteral("text")))
				label->setText(props.value(QStringLiteral("text")).toString());
		}
	} else if (type == QLatin1String("progress")) {
		if (auto* bar = qobject_cast<QProgressBar*>(w)) {
			if (props.contains(QStringLiteral("value"))) {
				const int value = props.value(QStringLiteral("value")).toInt(-1);
				if (value < 0)
					bar->setRange(0, 0);
				else {
					bar->setRange(0, 100);
					bar->setValue(qBound(0, value, 100));
				}
			}
		}
	} else if (type == QLatin1String("button")) {
		if (auto* btn = qobject_cast<QPushButton*>(w)) {
			if (props.contains(QStringLiteral("label")))
				btn->setText(props.value(QStringLiteral("label")).toString());
			if (props.contains(QStringLiteral("style")))
				btn->setProperty("mmcoStyle",
								 props.value(QStringLiteral("style")).toString());
		}
	} else if (type == QLatin1String("toggle")) {
		if (auto* chk = qobject_cast<QCheckBox*>(w)) {
			if (props.contains(QStringLiteral("label")))
				chk->setText(props.value(QStringLiteral("label")).toString());
			if (props.contains(QStringLiteral("value"))) {
				const QSignalBlocker blocker(chk);
				chk->setChecked(props.value(QStringLiteral("value")).toBool(false));
			}
		}
	} else if (type == QLatin1String("text_field")) {
		if (auto* edit = qobject_cast<QLineEdit*>(w)) {
			if (props.contains(QStringLiteral("value"))) {
				const QSignalBlocker blocker(edit);
				edit->setText(props.value(QStringLiteral("value")).toString());
			}
			if (props.contains(QStringLiteral("placeholder")))
				edit->setPlaceholderText(
					props.value(QStringLiteral("placeholder")).toString());
		}
	} else if (type == QLatin1String("number_field")) {
		if (auto* spin = qobject_cast<QDoubleSpinBox*>(w)) {
			if (props.contains(QStringLiteral("min")))
				spin->setMinimum(props.value(QStringLiteral("min")).toDouble(0));
			if (props.contains(QStringLiteral("max")))
				spin->setMaximum(props.value(QStringLiteral("max")).toDouble(100));
			if (props.contains(QStringLiteral("value"))) {
				const QSignalBlocker blocker(spin);
				spin->setValue(props.value(QStringLiteral("value")).toDouble(0));
			}
		}
	} else if (type == QLatin1String("choice")) {
		if (auto* combo = qobject_cast<QComboBox*>(w)) {
			if (props.contains(QStringLiteral("value"))) {
				const int idx = combo->findData(
					props.value(QStringLiteral("value")).toString());
				if (idx >= 0) {
					const QSignalBlocker blocker(combo);
					combo->setCurrentIndex(idx);
				}
			}
		}
	}
	/* "list" rows go through setRows(), not setNodeProps(). */
	return true;
}

bool PluginUiRenderer::RenderedSurface::setRows(const QString& nodeId,
												const QJsonArray& rows)
{
	auto it = m_nodes.find(nodeId);
	if (it == m_nodes.end() || it->type != QLatin1String("list"))
		return false;
	auto* tree = qobject_cast<QTreeWidget*>(it->widget);
	if (!tree)
		return false;
	populateListRows(tree, rows);
	return true;
}

QString PluginUiRenderer::RenderedSurface::nodeType(const QString& nodeId) const
{
	auto it = m_nodes.find(nodeId);
	return it == m_nodes.end() ? QString() : it->type;
}

QJsonObject PluginUiRenderer::RenderedSurface::collectValues() const
{
	QJsonObject out;
	for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
		const QString& type = it->type;
		QWidget* w = it->widget;
		if (type == QLatin1String("toggle")) {
			if (auto* chk = qobject_cast<QCheckBox*>(w))
				out.insert(it.key(), chk->isChecked());
		} else if (type == QLatin1String("text_field")) {
			if (auto* edit = qobject_cast<QLineEdit*>(w))
				out.insert(it.key(), edit->text());
		} else if (type == QLatin1String("number_field")) {
			if (auto* spin = qobject_cast<QDoubleSpinBox*>(w))
				out.insert(it.key(), spin->value());
		} else if (type == QLatin1String("choice")) {
			if (auto* combo = qobject_cast<QComboBox*>(w))
				out.insert(it.key(), combo->currentData().toString());
		}
	}
	return out;
}

std::unique_ptr<PluginUiRenderer::RenderedSurface>
PluginUiRenderer::build(const QString& jsonDoc, EventSink sink)
{
	auto surface = std::make_unique<RenderedSurface>(std::move(sink));
	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(jsonDoc.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject()) {
		qWarning().noquote() << "[PluginUiRenderer] invalid JSON document:"
							 << err.errorString();
		return surface;
	}
	surface->setDocument(jd.object());
	return surface;
}

bool PluginUiRenderer::buildTrayMenu(QMenu* menu, const QString& jsonDoc,
									 EventSink sink)
{
	if (!menu)
		return false;
	menu->clear();

	QJsonParseError err{};
	const QJsonDocument jd = QJsonDocument::fromJson(jsonDoc.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !jd.isObject()) {
		qWarning().noquote() << "[PluginUiRenderer] invalid tray menu JSON:"
							 << err.errorString();
		return false;
	}

	/* Recursive helper: `section` items become nested QMenus. */
	std::function<void(QMenu*, const QJsonArray&)> populate =
		[&](QMenu* m, const QJsonArray& arr) {
			for (const QJsonValue& iv : arr) {
				const QJsonObject item = iv.toObject();
				const QString type = item.value(QStringLiteral("type")).toString();
				const QJsonObject props = item.value(QStringLiteral("props")).toObject();
				if (type == QLatin1String("separator")) {
					m->addSeparator();
				} else if (type == QLatin1String("section")) {
					QMenu* sub =
						m->addMenu(props.value(QStringLiteral("title")).toString());
					populate(sub, item.value(QStringLiteral("children")).toArray());
				} else if (type == QLatin1String("button")) {
					const QString id = item.value(QStringLiteral("id")).toString();
					QAction* act =
						m->addAction(props.value(QStringLiteral("label")).toString());
					act->setEnabled(props.value(QStringLiteral("enabled")).toBool(true));
					QObject::connect(act, &QAction::triggered, act,
									 [sink, id]() {
										 if (sink)
											 sink(id, QStringLiteral("click"), QString());
									 });
				}
			}
		};
	populate(menu, jd.object().value(QStringLiteral("items")).toArray());
	return true;
}
