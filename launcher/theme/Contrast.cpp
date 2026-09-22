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

#include "theme/Contrast.h"

#include <QtMath>

namespace Contrast
{

	namespace
	{
		/* WCAG 2.1 SC 1.4.3: linearises a single sRGB channel already
		 * normalised to [0, 1]. The 0.03928 breakpoint and the two branches
		 * either side of it come straight from the spec -- there is no
		 * simpler formula that covers both of them at once. */
		qreal linearize(qreal channel)
		{
			return channel <= 0.03928 ? channel / 12.92
									   : qPow((channel + 0.055) / 1.055, 2.4);
		}
	}

	qreal relativeLuminance(const QColor& color)
	{
		return 0.2126 * linearize(color.redF()) +
			   0.7152 * linearize(color.greenF()) +
			   0.0722 * linearize(color.blueF());
	}

	qreal ratio(const QColor& a, const QColor& b)
	{
		const qreal la = relativeLuminance(a);
		const qreal lb = relativeLuminance(b);
		const qreal lighter = qMax(la, lb);
		const qreal darker = qMin(la, lb);
		return (lighter + 0.05) / (darker + 0.05);
	}

	bool meetsAA(const QColor& a, const QColor& b, bool largeText)
	{
		return ratio(a, b) >= (largeText ? 3.0 : 4.5);
	}

	bool meetsAAA(const QColor& a, const QColor& b, bool largeText)
	{
		return ratio(a, b) >= (largeText ? 4.5 : 7.0);
	}

	QColor compositeOver(const QColor& foreground, const QColor& background)
	{
		const qreal alpha = foreground.alphaF();
		const qreal behind = 1.0 - alpha;
		return QColor::fromRgbF(
			foreground.redF() * alpha + background.redF() * behind,
			foreground.greenF() * alpha + background.greenF() * behind,
			foreground.blueF() * alpha + background.blueF() * behind);
	}

} // namespace Contrast
