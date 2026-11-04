/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QByteArray>
#include <QString>
#include <QStringConverter>
#include <QStringList>

/*
 * What a launched process prints, as Kite reads it. The game is asked to
 * print UTF-8 (jvmArguments()), and Minecraft's own log lines are UTF-8 on
 * every system. A line that is not valid UTF-8 comes from native code that
 * prints in the system code page (the JVM's crash lines with a user folder,
 * a Windows tool), so it is read in that one: cp1254 on a Turkish Windows.
 * On macOS and Linux the code page is UTF-8, so nothing changes there.
 */
namespace ProcessOutput
{
	/// Cuts a stream into lines and decodes each once it is whole, so a
	/// character split over two reads stays one character.
	class LineDecoder
	{
	  public:
		/// @p fallback reads the lines that are not UTF-8.
		explicit LineDecoder(QStringConverter::Encoding fallback = QStringConverter::System);

		/// The lines @p data completes, without their line ends ("\r" too).
		QStringList feed(const QByteArray& data);
		/// The line after the last line end, which no newline will finish
		/// (the process is gone); "" when there is none.
		QString finish();

	  private:
		QString decode(QByteArray line) const;

		QByteArray m_rest;
		QStringConverter::Encoding m_fallback;
	};

	/// The JVM arguments that make Java @p javaMajor print UTF-8 on stdout
	/// and stderr whatever the system code page; both kinds when it is
	/// unknown (0). No -Dfile.encoding: that would change how the game and
	/// its mods read their own files on Java 8 to 17.
	QStringList jvmArguments(int javaMajor);

	/// Java 18 (JEP 400) reads stdout.encoding; older ones sun.stdout.encoding.
	inline constexpr int kStdoutEncodingSince = 18;
} // namespace ProcessOutput
