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

#include <QColor>

/* WCAG 2.1 contrast math for the QML theme. Ratios are computed from the
 * formula and asserted in Contrast_test.cpp, never transcribed. QtGui
 * only, so core and QML can both use it. */
namespace Contrast
{

	/** WCAG 2.1 relative luminance of @p color, in [0, 1]. Ignores alpha:
	 * composite a translucent colour with compositeOver() first. */
	qreal relativeLuminance(const QColor& color);

	/** WCAG 2.1 contrast ratio in [1, 21]; symmetric in its arguments. */
	qreal ratio(const QColor& a, const QColor& b);

	/** Whether @p a on @p b clears WCAG AA: 4.5:1, or 3:1 for @p largeText
	 * (18pt+, or 14pt+ bold; the caller classifies the text). */
	bool meetsAA(const QColor& a, const QColor& b, bool largeText = false);

	/** Whether @p a on @p b clears WCAG AAA: 7:1, or 4.5:1 for @p largeText. */
	bool meetsAAA(const QColor& a, const QColor& b, bool largeText = false);

	/** Flattens @p foreground onto @p background with the foreground's alpha
	 * ("over" compositing), giving an opaque colour whose contrast is
	 * meaningful. @p background is treated as opaque. */
	QColor compositeOver(const QColor& foreground, const QColor& background);

} // namespace Contrast
