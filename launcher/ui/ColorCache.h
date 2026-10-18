/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QtGui/QColor>
#include <rainbow.h>
#include <MessageLevel.h>
#include <QMap>

class ColorCache
{
  public:
	ColorCache(QColor front, QColor back, qreal bias)
	{
		m_front = front;
		m_back = back;
		m_bias = bias;
	};

	void addColor(int key, QColor color)
	{
		m_colors[key] = {color, blend(color), blendBackground(color)};
	}

	void setForeground(QColor front)
	{
		if (m_front != front) {
			m_front = front;
			recolorAll();
		}
	}

	void setBackground(QColor back)
	{
		if (m_back != back) {
			m_back = back;
			recolorAll();
		}
	}

	QColor getFront(int key)
	{
		auto iter = m_colors.find(key);
		if (iter == m_colors.end()) {
			return QColor();
		}
		return (*iter).front;
	}

	QColor getBack(int key)
	{
		auto iter = m_colors.find(key);
		if (iter == m_colors.end()) {
			return QColor();
		}
		return (*iter).back;
	}

	/**
	 * Blend the color with the front color, adapting to the back color
	 */
	QColor blend(QColor color);

	/**
	 * Blend the color with the back color
	 */
	QColor blendBackground(QColor color);

  protected:
	void recolorAll();

  protected:
	struct ColorEntry {
		QColor original;
		QColor front;
		QColor back;
	};

  protected:
	qreal m_bias;
	QColor m_front;
	QColor m_back;
	QMap<int, ColorEntry> m_colors;
};

class LogColorCache : public ColorCache
{
  public:
	LogColorCache(QColor front, QColor back) : ColorCache(front, back, 1.0)
	{
		addColor((int)MessageLevel::Kite, QColor("purple"));
		addColor((int)MessageLevel::Debug, QColor("green"));
		addColor((int)MessageLevel::Warning, QColor("orange"));
		addColor((int)MessageLevel::Error, QColor("red"));
		addColor((int)MessageLevel::Fatal, QColor("red"));
		addColor((int)MessageLevel::Message, front);
	}

	QColor getFront(MessageLevel::Enum level)
	{
		if (!m_colors.contains((int)level)) {
			return ColorCache::getFront((int)MessageLevel::Message);
		}
		return ColorCache::getFront((int)level);
	}

	QColor getBack(MessageLevel::Enum level)
	{
		if (level == MessageLevel::Fatal) {
			return QColor(Qt::black);
		}
		return QColor(Qt::transparent);
	}
};
