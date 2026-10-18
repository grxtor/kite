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

#include "Logging.h"

#include "MMCStrings.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <cstdio>
#endif

Q_LOGGING_CATEGORY(netLog, "kite.net")
Q_LOGGING_CATEGORY(mojangLog, "kite.mojang")
Q_LOGGING_CATEGORY(metaLog, "kite.meta")
Q_LOGGING_CATEGORY(archiveLog, "kite.archive")
Q_LOGGING_CATEGORY(backupLog, "kite.backup")
Q_LOGGING_CATEGORY(iconsLog, "kite.icons")
Q_LOGGING_CATEGORY(javaLog, "kite.java")
Q_LOGGING_CATEGORY(launchLog, "kite.launch")
Q_LOGGING_CATEGORY(newsLog, "kite.news")
Q_LOGGING_CATEGORY(pluginsLog, "kite.plugins")
Q_LOGGING_CATEGORY(minecraftauthLog, "kite.minecraft.auth")
Q_LOGGING_CATEGORY(minecraftgoptionsLog, "kite.minecraft.gameoptions")
Q_LOGGING_CATEGORY(minecraftlaunchLog, "kite.minecraft.launch")

namespace
{
	bool g_consoleColour = false;
}

bool Logging::consoleColourEnabled()
{
	return g_consoleColour;
}

bool Logging::prepareConsoleColour()
{
	// Fail *towards* colour. Colour output used to be unconditional, so
	// anything this function cannot actually determine has to keep the old
	// behaviour instead of silently swallowing every colour in the log.
	g_consoleColour = true;

	// An explicit opt-out beats any detection. https://no-color.org
	if (qEnvironmentVariableIsSet("NO_COLOR")) {
		g_consoleColour = false;
		return false;
	}

#ifdef Q_OS_WIN
	// Question one: is stderr still pointing at a console, or did the user
	// redirect it? Escapes written into a file or a pipe are just noise.
	// GetFileType answers this on stderr's own handle - GetConsoleMode does
	// not, see below.
	const intptr_t osfHandle = _get_osfhandle(_fileno(stderr));
	if (osfHandle != -1 && osfHandle != -2) {
		const DWORD type = GetFileType(reinterpret_cast<HANDLE>(osfHandle));
		if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) {
			g_consoleColour = false;
			return false;
		}
	}

	// Question two: does the console understand escape sequences? That is a
	// question about the console screen buffer and it must NOT be asked of
	// stderr's handle. The launcher gets stderr from
	// freopen("CON", "w", stderr), i.e. a write-only console handle, and
	// GetConsoleMode on one of those fails with ERROR_ACCESS_DENIED - which
	// is exactly how an earlier version of this function managed to turn all
	// colour off on a console that was perfectly capable of it.
	// CONOUT$ opened for read+write is the handle that can be asked, and it
	// keeps working even when stdout and stderr are redirected elsewhere.
	// Measured, probe10.
	const HANDLE console =
		CreateFileW(L"CONOUT$", FILE_GENERIC_READ | FILE_GENERIC_WRITE,
					FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (console == INVALID_HANDLE_VALUE) {
		// No console attached at all, so nothing is being displayed and the
		// answer cannot be observed either way. Keep the old behaviour.
		return g_consoleColour;
	}

	DWORD mode = 0;
	if (!GetConsoleMode(console, &mode)) {
		CloseHandle(console);
		return g_consoleColour;
	}
	if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
		// The classic console host starts with this bit clear, and until it
		// is set the escapes get printed literally as "<ESC>[32m". Windows
		// Terminal already has it on, where this is a no-op.
		if (!SetConsoleMode(console,
							mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
			// A console host that cannot do it at all. Colourless is the
			// readable outcome here.
			g_consoleColour = false;
		}
	}
	CloseHandle(console);
#else
	// A stderr that is not a terminal is a redirect, and TERM=dumb is the
	// user saying the terminal cannot be trusted with escapes.
	g_consoleColour = isatty(fileno(stderr)) != 0 &&
					  qgetenv("TERM") != QByteArrayLiteral("dumb");
#endif

	return g_consoleColour;
}

QString Logging::messagePattern(bool coloured, bool withSourceLocation)
{
	// One letter per Qt message type. The letters, their order and the colon
	// after them are what the launcher has always printed, so old and new
	// logs stay comparable line for line.
	struct Level {
		const char* condition;
		QtMsgType type;
		char letter;
	};
	static const Level levels[] = {
		{"if-debug", QtDebugMsg, 'D'},
		{"if-info", QtInfoMsg, 'I'},
		{"if-warning", QtWarningMsg, 'W'},
		{"if-critical", QtCriticalMsg, 'C'},
		{"if-fatal", QtFatalMsg, 'F'},
	};

	// Emits an escape sequence only for the coloured variant, so the two
	// layouts cannot drift apart. All codes come from Strings, which stays
	// the single place that decides what a warning looks like.
	const auto ansi = [coloured](const char* code) {
		return coloured ? QLatin1String(code) : QLatin1String("");
	};

	// Visual hierarchy, and the reason a coloured line reads at a glance: the
	// timestamp and the source location are scaffolding and get dimmed, while
	// the level letter and the category tag are what the eye should land on.
	// Colouring only the level letter, as this used to, leaves the line
	// looking flat.
	//
	// %{time process} is Qt's seconds-since-process-start, right aligned in
	// six columns. That is one column wider than the launcher's old
	// hand-rolled "%5lld.%03lld", so lines shift by a single space.
	QString pattern = ansi(Strings::logColorFaint()) +
					  QStringLiteral("%{time process}") +
					  ansi(Strings::logColorReset()) + QLatin1Char(' ');

	for (const auto& level : levels) {
		pattern += QStringLiteral("%{");
		pattern += QLatin1String(level.condition);
		pattern += QStringLiteral("}");
		pattern += ansi(Strings::logColorBold());
		pattern += ansi(Strings::logColor(level.type));
		pattern += QLatin1Char(level.letter);
		pattern += QLatin1Char(':');
		pattern += ansi(Strings::logColorReset());
		pattern += QStringLiteral("%{endif}");
	}

	// Qt reports uncategorised messages as the "default" category and makes
	// %{if-category} false for them, so a plain qDebug() line comes out
	// unchanged and only opted-in subsystems gain a [tag]. Measured, probe9.
	pattern += QLatin1Char(' ');
	pattern += QStringLiteral("%{if-category}");
	pattern += ansi(Strings::logColorBold());
	pattern += QStringLiteral("[%{category}]");
	pattern += ansi(Strings::logColorReset());
	pattern += QStringLiteral(" %{endif}");
	pattern += QStringLiteral("%{message}");

	// Where the line came from. Qt reduces the compiler's full signature to a
	// bare function name, which is what makes this worth printing on MSVC at
	// all - the raw context.function there reads
	// "void __cdecl Net::Download::startImpl(void)".
	//
	// A message only has a source location if the translation unit that
	// raised it was compiled with QT_MESSAGELOGCONTEXT. launcher/CMakeLists.txt
	// defines it for every configuration, because Qt itself only defines it
	// while QT_NO_DEBUG is absent, i.e. in Debug builds - so official builds
	// would otherwise log no locations at all. Qt's own prebuilt libraries are
	// a different matter: we cannot recompile them, so their messages have no
	// location and are rendered without this suffix. Measured, probe9.
	if (withSourceLocation) {
		pattern += QLatin1Char(' ');
		pattern += ansi(Strings::logColorFaint());
		pattern += QStringLiteral("(%{function}:%{line})");
		pattern += ansi(Strings::logColorReset());
	}

	return pattern;
}
