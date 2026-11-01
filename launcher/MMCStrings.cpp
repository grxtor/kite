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

#include "MMCStrings.h"

/// TAKEN FROM Qt, because it doesn't expose it intelligently
static inline QChar getNextChar(const QString& s, int location)
{
	return (location < s.length()) ? s.at(location) : QChar();
}

/// TAKEN FROM Qt, because it doesn't expose it intelligently
int Strings::naturalCompare(const QString& s1, const QString& s2,
							Qt::CaseSensitivity cs)
{
	for (int l1 = 0, l2 = 0; l1 <= s1.count() && l2 <= s2.count(); ++l1, ++l2) {
		QChar c1 = getNextChar(s1, l1);
		while (c1.isSpace())
			c1 = getNextChar(s1, ++l1);
		QChar c2 = getNextChar(s2, l2);
		while (c2.isSpace())
			c2 = getNextChar(s2, ++l2);

		if (c1.isDigit() && c2.isDigit()) {
			while (c1.digitValue() == 0)
				c1 = getNextChar(s1, ++l1);
			while (c2.digitValue() == 0)
				c2 = getNextChar(s2, ++l2);

			int lookAheadLocation1 = l1;
			int lookAheadLocation2 = l2;
			int currentReturnValue = 0;
			for (QChar lookAhead1 = c1, lookAhead2 = c2;
				 (lookAheadLocation1 <= s1.length() &&
				  lookAheadLocation2 <= s2.length());
				 lookAhead1 = getNextChar(s1, ++lookAheadLocation1),
					   lookAhead2 = getNextChar(s2, ++lookAheadLocation2)) {
				bool is1ADigit = !lookAhead1.isNull() && lookAhead1.isDigit();
				bool is2ADigit = !lookAhead2.isNull() && lookAhead2.isDigit();
				if (!is1ADigit && !is2ADigit)
					break;
				if (!is1ADigit)
					return -1;
				if (!is2ADigit)
					return 1;
				if (currentReturnValue == 0) {
					if (lookAhead1 < lookAhead2) {
						currentReturnValue = -1;
					} else if (lookAhead1 > lookAhead2) {
						currentReturnValue = 1;
					}
				}
			}
			if (currentReturnValue != 0)
				return currentReturnValue;
		}
		if (cs == Qt::CaseInsensitive) {
			if (!c1.isLower())
				c1 = c1.toLower();
			if (!c2.isLower())
				c2 = c2.toLower();
		}
		int r = QString::localeAwareCompare(c1, c2);
		if (r < 0)
			return -1;
		if (r > 0)
			return 1;
	}
	return QString::compare(s1, s2, cs);
}

QString Strings::htmlListPatch(const QString& html)
{
	// QTextBrowser renders <ul>/<ol> lists with too much padding.
	// This function corrects this by adding reasonable CSS margin/padding to
	// the list elements.
	QString patched(html);
	patched.replace(
		QLatin1String("<ul"),
		QLatin1String("<ul style=\"margin-left: 0; padding-left: 20px;\""));
	patched.replace(
		QLatin1String("<ol"),
		QLatin1String("<ol style=\"margin-left: 0; padding-left: 20px;\""));
	patched.replace(QLatin1String("<li"),
					QLatin1String("<li style=\"margin-bottom: 4px;\""));
	return patched;
}

const char* Strings::logColor(QtMsgType type)
{
	switch (type) {
		case QtInfoMsg:
			return "\033[36m"; // cyan
		case QtDebugMsg:
			return "\033[32m"; // green
		case QtWarningMsg:
			return "\033[33m"; // yellow
		case QtCriticalMsg:
			return "\033[31m"; // red
		case QtFatalMsg:
			return "\033[1;31m"; // red again
		default:
			return "";
	}
}

const char* Strings::logColorReset()
{
	return "\033[0m";
}

const char* Strings::logColorFaint()
{
	return "\033[2m";
}

const char* Strings::logColorBold()
{
	return "\033[1m";
}
