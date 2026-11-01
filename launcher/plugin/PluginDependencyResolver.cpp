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

#include "plugin/PluginDependencyResolver.h"

#include <QHash>
#include <QQueue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

int PluginDependencyResolver::compareVersions(const QString& a,
											  const QString& b)
{
	if (a.isEmpty() && b.isEmpty())
		return 0;
	if (a.isEmpty())
		return -1;
	if (b.isEmpty())
		return 1;

	// Split on non-alphanumeric separators ('.', '-', '+', '_'). Numeric
	// segments compare numerically, non-numeric segments compare
	// lexicographically.
	static const QRegularExpression sep(R"([.\-+_])");
	const QStringList la = a.split(sep, Qt::SkipEmptyParts);
	const QStringList lb = b.split(sep, Qt::SkipEmptyParts);

	const int n = qMax(la.size(), lb.size());
	for (int i = 0; i < n; ++i) {
		const QString sa = i < la.size() ? la[i] : QString();
		const QString sb = i < lb.size() ? lb[i] : QString();

		bool aIsNum = false;
		bool bIsNum = false;
		const qulonglong na = sa.toULongLong(&aIsNum);
		const qulonglong nb = sb.toULongLong(&bIsNum);

		if (aIsNum && bIsNum) {
			if (na != nb)
				return na < nb ? -1 : 1;
		} else {
			const int cmp = QString::compare(sa, sb, Qt::CaseInsensitive);
			if (cmp != 0)
				return cmp;
		}
	}
	return 0;
}

PluginDependencyResolver::Result
PluginDependencyResolver::resolve(QVector<PluginMetadata>& modules)
{
	Result out;

	// Name -> index lookup, lower-cased for case-insensitive matching.
	QHash<QString, int> byName;
	byName.reserve(modules.size());
	for (int i = 0; i < modules.size(); ++i) {
		byName.insert(modules[i].name.toLower(), i);
	}

	const int n = modules.size();

	// adj[i]: modules that depend on i. indegree[i]: unmet hard dependencies.
	QVector<QVector<int>> adj(n);
	QVector<int> indegree(n, 0);

	// Pre-pass: validate dependencies, mark modules missing required deps.
	for (int i = 0; i < n; ++i) {
		PluginMetadata& meta = modules[i];
		if (meta.disabled)
			continue;

		for (const auto& dep : meta.dependencies) {
			const auto it = byName.constFind(dep.name.toLower());
			if (it == byName.constEnd()) {
				if (!dep.optional) {
					meta.disabled = true;
					meta.disableReason = PluginDisableReason::DependencyMissing;
					meta.disableDetail =
						QStringLiteral("Required dependency '%1' not found")
							.arg(dep.name);
				}
				continue;
			}

			const int depIdx = *it;
			const PluginMetadata& depMeta = modules[depIdx];

			// A disabled dep (e.g. unsigned non-OSS) is effectively missing.
			if (depMeta.disabled) {
				if (!dep.optional) {
					meta.disabled = true;
					meta.disableReason = PluginDisableReason::DependencyMissing;
					meta.disableDetail =
						QStringLiteral(
							"Required dependency '%1' is disabled (%2)")
							.arg(dep.name, depMeta.disableDetail);
				}
				continue;
			}

			if (!dep.minVersion.isEmpty() &&
				compareVersions(depMeta.version, dep.minVersion) < 0) {
				if (!dep.optional) {
					meta.disabled = true;
					meta.disableReason = PluginDisableReason::DependencyMissing;
					meta.disableDetail =
						QStringLiteral(
							"Dependency '%1' is version %2, need >= %3")
							.arg(dep.name, depMeta.version, dep.minVersion);
				}
				continue;
			}

			// Edge: dep → meta. depMeta must load before meta.
			adj[depIdx].append(i);
			indegree[i] += 1;
		}
	}

	// Kahn's algorithm starting from all enabled modules with indegree 0.
	QQueue<int> ready;
	for (int i = 0; i < n; ++i) {
		if (modules[i].disabled)
			continue;
		if (indegree[i] == 0)
			ready.enqueue(i);
	}

	int visited = 0;
	while (!ready.isEmpty()) {
		const int i = ready.dequeue();
		out.loadOrder.append(i);
		++visited;
		for (int j : adj[i]) {
			if (modules[j].disabled)
				continue;
			if (--indegree[j] == 0)
				ready.enqueue(j);
		}
	}

	// Any remaining enabled modules with indegree > 0 are in a cycle.
	for (int i = 0; i < n; ++i) {
		if (modules[i].disabled)
			continue;
		if (indegree[i] > 0) {
			modules[i].disabled = true;
			modules[i].disableReason = PluginDisableReason::DependencyCycle;
			modules[i].disableDetail = QStringLiteral(
				"Module is part of a plugin dependency cycle and cannot "
				"be loaded");
		}
	}

	return out;
}
