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
#include <QString>
#include <QMap>
#include <QStringList>

template <char Tseparator> class SeparatorPrefixTree
{
  public:
	SeparatorPrefixTree(QStringList paths)
	{
		insert(paths);
	}

	SeparatorPrefixTree(bool contained = false)
	{
		m_contained = contained;
	}

	void insert(QStringList paths)
	{
		for (auto& path : paths) {
			insert(path);
		}
	}

	SeparatorPrefixTree& insert(QString path)
	{
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			children[path] = SeparatorPrefixTree(true);
			return children[path];
		} else {
			auto prefix = path.left(sepIndex);
			if (!children.contains(prefix)) {
				children[prefix] = SeparatorPrefixTree(false);
			}
			return children[prefix].insert(path.mid(sepIndex + 1));
		}
	}

	bool contains(QString path) const
	{
		auto node = find(path);
		return node != nullptr;
	}

	bool covers(QString path) const
	{
		if (m_contained) {
			return true;
		}
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			auto found = children.find(path);
			if (found == children.end()) {
				return false;
			}
			return (*found).covers(QString());
		} else {
			auto prefix = path.left(sepIndex);
			auto found = children.find(prefix);
			if (found == children.end()) {
				return false;
			}
			return (*found).covers(path.mid(sepIndex + 1));
		}
	}

	QString cover(QString path) const
	{
		if (m_contained) {
			return QString("");
		}
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			auto found = children.find(path);
			if (found == children.end()) {
				return QString();
			}
			auto nested = (*found).cover(QString());
			if (nested.isNull()) {
				return nested;
			}
			if (nested.isEmpty())
				return path;
			return path + Tseparator + nested;
		} else {
			auto prefix = path.left(sepIndex);
			auto found = children.find(prefix);
			if (found == children.end()) {
				return QString();
			}
			auto nested = (*found).cover(path.mid(sepIndex + 1));
			if (nested.isNull()) {
				return nested;
			}
			if (nested.isEmpty())
				return prefix;
			return prefix + Tseparator + nested;
		}
	}

	bool exists(QString path) const
	{
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			auto found = children.find(path);
			if (found == children.end()) {
				return false;
			}
			return true;
		} else {
			auto prefix = path.left(sepIndex);
			auto found = children.find(prefix);
			if (found == children.end()) {
				return false;
			}
			return (*found).exists(path.mid(sepIndex + 1));
		}
	}

	const SeparatorPrefixTree* find(QString path) const
	{
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			auto found = children.find(path);
			if (found == children.end()) {
				return nullptr;
			}
			return &(*found);
		} else {
			auto prefix = path.left(sepIndex);
			auto found = children.find(prefix);
			if (found == children.end()) {
				return nullptr;
			}
			return (*found).find(path.mid(sepIndex + 1));
		}
	}

	bool leaf() const
	{
		return children.isEmpty();
	}

	/// Whether this node is actually contained in the tree, or purely
	/// structural.
	bool contained() const
	{
		return m_contained;
	}

	bool remove(QString path)
	{
		return removeInternal(path) != Failed;
	}

	void clear()
	{
		children.clear();
	}

	QStringList toStringList() const
	{
		QStringList collected;
		// collecting these is more expensive.
		auto iter = children.begin();
		while (iter != children.end()) {
			QStringList list = iter.value().toStringList();
			for (int i = 0; i < list.size(); i++) {
				list[i] = iter.key() + Tseparator + list[i];
			}
			collected.append(list);
			if ((*iter).m_contained) {
				collected.append(iter.key());
			}
			iter++;
		}
		return collected;
	}

  private:
	enum Removal { Failed, Succeeded, HasChildren };
	Removal removeInternal(QString path = QString())
	{
		if (path.isEmpty()) {
			if (!m_contained) {
				clear();
				return Succeeded;
			}
			m_contained = false;
			if (children.size()) {
				return HasChildren;
			}
			return Succeeded;
		}
		Removal remStatus = Failed;
		QString childToRemove;
		auto sepIndex = path.indexOf(Tseparator);
		if (sepIndex == -1) {
			childToRemove = path;
			auto found = children.find(childToRemove);
			if (found == children.end()) {
				return Failed;
			}
			remStatus = (*found).removeInternal();
		} else {
			childToRemove = path.left(sepIndex);
			auto found = children.find(childToRemove);
			if (found == children.end()) {
				return Failed;
			}
			remStatus = (*found).removeInternal(path.mid(sepIndex + 1));
		}
		switch (remStatus) {
			case Failed:
			case HasChildren: {
				return remStatus;
			}
			case Succeeded: {
				children.remove(childToRemove);
				if (m_contained) {
					return HasChildren;
				}
				if (children.size()) {
					return HasChildren;
				}
				return Succeeded;
			}
		}
		return Failed;
	}

  private:
	QMap<QString, SeparatorPrefixTree<Tseparator>> children;
	bool m_contained = false;
};
