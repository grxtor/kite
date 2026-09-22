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

#include <QColor>

/*
 * WCAG 2.1 contrast math for the token-based QML theme.
 *
 * Every colour token the theme exposes has to clear a minimum contrast
 * against whatever it sits on, and the ratios written by hand into design
 * notes for this project have already been wrong more than once. So a ratio
 * is never a table entry here: it is computed from the WCAG formula and
 * asserted against in Contrast_test.cpp instead of transcribed.
 *
 * Free functions rather than a class, because there is no state to hold --
 * only a pure conversion from colour to luminance and from luminance to
 * ratio. QtGui only: nothing here may reach into QtWidgets or ui/, so both
 * the core and the QML front end can use it.
 */
namespace Contrast
{

	/**
	 * WCAG 2.1 relative luminance of @p color, in [0, 1].
	 *
	 * Ignores alpha -- a translucent colour has no luminance of its own
	 * until it is known what it sits on. Run it through compositeOver()
	 * first if it carries transparency.
	 */
	qreal relativeLuminance(const QColor& color);

	/**
	 * WCAG 2.1 contrast ratio between @p a and @p b, in [1, 21].
	 *
	 * The lighter of the two colours is always the numerator, so the result
	 * does not depend on argument order: ratio(a, b) == ratio(b, a).
	 */
	qreal ratio(const QColor& a, const QColor& b);

	/**
	 * Whether @p a on @p b clears the WCAG AA minimum: 4.5:1, or 3:1 when
	 * @p largeText (WCAG's definition of large: 18pt+, or 14pt+ bold --
	 * classifying a token's text as one or the other is left to the caller,
	 * since this header has no notion of font metrics).
	 */
	bool meetsAA(const QColor& a, const QColor& b, bool largeText = false);

	/**
	 * Whether @p a on @p b clears the stricter WCAG AAA minimum: 7:1, or
	 * 4.5:1 when @p largeText.
	 */
	bool meetsAAA(const QColor& a, const QColor& b, bool largeText = false);

	/**
	 * Flattens @p foreground onto @p background using @p foreground's own
	 * alpha (standard "over" compositing), so the result is opaque and its
	 * contrast is actually meaningful.
	 *
	 * A token with alpha < 255 has no contrast in isolation -- the WCAG
	 * formula assumes two opaque colours -- so it has to be composited onto
	 * whatever it will actually render over before ratio(), meetsAA() or
	 * meetsAAA() say anything useful about it. @p background is treated as
	 * fully opaque regardless of its own alpha: compositing onto a second
	 * translucent layer would need that layer's own backdrop in turn, which
	 * is outside what this helper knows.
	 */
	QColor compositeOver(const QColor& foreground, const QColor& background);

} // namespace Contrast
