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

#include "plugin/PluginUiRenderer.h"

#include <QAction>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>

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
