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

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"

#include <QDir>
#include <QProcessEnvironment>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <unistd.h>
/* Not declared by every platform's <unistd.h> (notably not by macOS's -
 * see environ(7): "the following line placed in the source file should be
 * sufficient to obtain the definition"). Declared here rather than inside a
 * function or an unnamed namespace, both of which would give this specific
 * declaration internal linkage and quietly fail to bind to the real,
 * externally-linked libc symbol. */
extern char** environ;
#endif

// #define BREAK_INFINITE_LOOP
// #define BREAK_EXCEPTION
// #define BREAK_RETURN

#ifdef BREAK_INFINITE_LOOP
#include <thread>
#include <chrono>
#endif

namespace
{
	/*
	 * Async-signal-safe crash reporter launch.
	 *
	 * launchCrashReporter() used to build its argument list and spawn the
	 * reporter process from inside crashSignalHandler() itself, using
	 * QFile::exists(), APPLICATION->settings(), QDir::currentPath(),
	 * logFile->flush() and QProcess::startDetached() - none of which are
	 * async-signal-safe, and a crash can land the handler on any thread, at
	 * any point in the middle of Qt's or the allocator's own state.
	 * Everything the reporter needs is instead computed once, up front
	 * (rebuildCrashReporterInvocation(), called after Application exists and
	 * again whenever PasteEEAPIKey changes); the handler only reads the
	 * already-built, plain-old-data invocation and calls the OS
	 * process-creation primitive directly - CreateProcessW on Windows,
	 * posix_spawn elsewhere - neither of which touches Qt, the heap, or the
	 * log file.
	 *
	 * DOUBLE BUFFERING. Two CrashReporterInvocation slots
	 * (g_invocationStorage) and an atomic index (g_activeInvocation) into
	 * them: a rebuild always fills the slot that is *not* currently active,
	 * then publishes it with a single atomic store. A signal landing
	 * mid-rebuild therefore always reads either the old, complete invocation
	 * or the new, complete one - never a half-written one. -1 means nothing
	 * has been built yet (matches the original's silent no-op when the
	 * reporter binary was not found).
	 */
	constexpr int kMaxPathChars = 1024;
	constexpr int kMaxArgChars = 512;
#ifdef Q_OS_WIN
	// The full command line (quoted path + three quoted "--flag value"
	// pairs). quoteWindowsArg() below can at most double an argument's
	// length (a run of backslashes right before a quote, or at the end of
	// the argument, doubles) plus one extra character per embedded quote,
	// so this doubles the previous, unescaped budget for headroom.
	constexpr int kMaxCommandLineChars =
		2 * (kMaxPathChars + 3 * kMaxArgChars) + 64;
#endif

	struct CrashReporterInvocation {
		bool valid = false;
#ifdef Q_OS_WIN
		wchar_t path[kMaxPathChars] = {};
		// CreateProcessW's lpCommandLine must be writable memory (it may
		// rewrite separators internally); never a literal or a const
		// buffer.
		wchar_t commandLine[kMaxCommandLineChars] = {};
#else
		char path[kMaxPathChars] = {};
		char logDirArg[kMaxArgChars] = {};
		char nameArg[kMaxArgChars] = {};
		char apiKeyArg[kMaxArgChars] = {};
		// Pointers into the members above (plus two string-literal flags),
		// rebuilt in lock-step with them so they always describe this same
		// slot's strings.
		char* argv[8] = {};
#endif
	};

	CrashReporterInvocation g_invocationStorage[2];
	std::atomic<int> g_activeInvocation{-1};

#ifdef Q_OS_WIN
	bool copyToBuffer(wchar_t* dest, int destCharCount, const std::wstring& src)
	{
		if (static_cast<int>(src.size()) >= destCharCount) {
			return false;
		}
		std::memcpy(dest, src.c_str(), (src.size() + 1) * sizeof(wchar_t));
		return true;
	}

	/* Quotes and escapes a single argument for CreateProcessW's
	 * lpCommandLine, following the same backslash/quote rules
	 * CommandLineToArgvW uses to parse it back apart: a backslash is only
	 * special when it immediately precedes a double quote (an embedded one,
	 * or the closing quote this function appends) - a run of N backslashes
	 * there must become 2N to stay literal, plus one more to escape the
	 * quote itself; everywhere else a backslash is an ordinary character.
	 * This matters because apiKey comes straight from the user-editable
	 * PasteEEAPIKey setting - arbitrary text that may contain a '"' - and
	 * unlike the POSIX branch's explicit argv[], CreateProcessW re-parses a
	 * single command-line string, so an unescaped quote would let it inject
	 * an extra argument into meshmc-crashreporter's parsed argv. */
	QString quoteWindowsArg(const QString& arg)
	{
		QString result = QStringLiteral("\"");
		int backslashes = 0;
		for (const QChar& ch : arg) {
			if (ch == QLatin1Char('\\')) {
				++backslashes;
				continue;
			}
			if (ch == QLatin1Char('"')) {
				result += QString(backslashes * 2 + 1, QLatin1Char('\\'));
				backslashes = 0;
				result += ch;
				continue;
			}
			result += QString(backslashes, QLatin1Char('\\'));
			backslashes = 0;
			result += ch;
		}
		// Trailing backslashes must be doubled so they escape themselves
		// rather than the closing quote appended below.
		result += QString(backslashes * 2, QLatin1Char('\\'));
		result += QLatin1Char('"');
		return result;
	}
#else
	bool copyToBuffer(char* dest, int destCharCount, const QByteArray& src)
	{
		if (src.size() >= destCharCount) {
			return false;
		}
		std::memcpy(dest, src.constData(), static_cast<size_t>(src.size()));
		dest[src.size()] = '\0';
		return true;
	}
#endif

	/* Rebuilds the currently-inactive slot from scratch and publishes it -
	 * see the class comment above. GUI thread only: called once at startup
	 * (after Application, and therefore its settings, exist) and again
	 * whenever the PasteEEAPIKey setting changes. */
	void rebuildCrashReporterInvocation()
	{
		QString crashReporterName = QStringLiteral("meshmc-crashreporter");
#ifdef Q_OS_WIN
		crashReporterName += QStringLiteral(".exe");
#endif
		const QString path = FS::PathCombine(
			QApplication::applicationDirPath(), crashReporterName);
		if (!QFile::exists(path)) {
			// Nothing to launch - same early return the original had. Any
			// previously-published invocation (e.g. from before the binary
			// was removed, which should not normally happen) is left alone
			// rather than invalidated, since a stale-but-valid invocation is
			// safer to fall back on than none at all.
			return;
		}

		QString apiKey = QStringLiteral("public");
		if (APPLICATION && APPLICATION->settings()) {
			const QString key =
				APPLICATION->settings()->get("PasteEEAPIKey").toString();
			apiKey = (key != QLatin1String("meshmc") && !key.isEmpty())
						 ? key
						 : BuildConfig.PASTE_EE_KEY;
		}
		const QString logDir = QDir::currentPath();
		const QString name = BuildConfig.MESHMC_NAME;

		const int current = g_activeInvocation.load(std::memory_order_relaxed);
		const int nextSlot = current == 0 ? 1 : 0;
		CrashReporterInvocation& slot = g_invocationStorage[nextSlot];
		slot.valid = false;

#ifdef Q_OS_WIN
		if (!copyToBuffer(slot.path, kMaxPathChars, path.toStdWString())) {
			return;
		}
		// Conventionally, the command line's own first token is the module
		// path too (a child reads that back via GetCommandLine()). apiKey
		// in particular is arbitrary, user-editable text (the PasteEEAPIKey
		// setting), so every argument is escaped with quoteWindowsArg()
		// rather than just wrapped in literal quotes - see its comment.
		const QString commandLine =
			QStringLiteral("%1 --logdir %2 --name %3 --apikey %4")
				.arg(quoteWindowsArg(path), quoteWindowsArg(logDir),
					 quoteWindowsArg(name), quoteWindowsArg(apiKey));
		if (!copyToBuffer(slot.commandLine, kMaxCommandLineChars,
						  commandLine.toStdWString())) {
			return;
		}
#else
		if (!copyToBuffer(slot.path, kMaxPathChars, path.toLocal8Bit())) {
			return;
		}
		if (!copyToBuffer(slot.logDirArg, kMaxArgChars, logDir.toLocal8Bit())) {
			return;
		}
		if (!copyToBuffer(slot.nameArg, kMaxArgChars, name.toLocal8Bit())) {
			return;
		}
		if (!copyToBuffer(slot.apiKeyArg, kMaxArgChars, apiKey.toLocal8Bit())) {
			return;
		}
		int i = 0;
		slot.argv[i++] = slot.path;
		slot.argv[i++] = const_cast<char*>("--logdir");
		slot.argv[i++] = slot.logDirArg;
		slot.argv[i++] = const_cast<char*>("--name");
		slot.argv[i++] = slot.nameArg;
		slot.argv[i++] = const_cast<char*>("--apikey");
		slot.argv[i++] = slot.apiKeyArg;
		slot.argv[i] = nullptr;
#endif

		slot.valid = true;
		// release: everything written to `slot` above must be visible to
		// whichever thread's signal handler next acquire-loads this index.
		g_activeInvocation.store(nextSlot, std::memory_order_release);
	}
} // namespace

static void crashSignalHandler(int sig)
{
	// Re-set default handler to avoid infinite loops
	signal(sig, SIG_DFL);

	const int slot = g_activeInvocation.load(std::memory_order_acquire);
	if (slot >= 0) {
		CrashReporterInvocation& invocation = g_invocationStorage[slot];
		if (invocation.valid) {
#ifdef Q_OS_WIN
			STARTUPINFOW startupInfo;
			std::memset(&startupInfo, 0, sizeof(startupInfo));
			startupInfo.cb = sizeof(startupInfo);
			PROCESS_INFORMATION processInfo;
			std::memset(&processInfo, 0, sizeof(processInfo));
			if (CreateProcessW(invocation.path, invocation.commandLine,
								nullptr, nullptr, FALSE, DETACHED_PROCESS,
								nullptr, nullptr, &startupInfo,
								&processInfo)) {
				CloseHandle(processInfo.hProcess);
				CloseHandle(processInfo.hThread);
			}
#else
			pid_t childPid = 0;
			posix_spawn(&childPid, invocation.path, nullptr, nullptr,
						invocation.argv, environ);
#endif
		}
	}

	// Re-raise the signal so the default handler produces a core dump etc.
	raise(sig);
}

int main(int argc, char* argv[])
{
#ifdef BREAK_INFINITE_LOOP
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
#endif
#ifdef BREAK_EXCEPTION
	throw 42;
#endif
#ifdef BREAK_RETURN
	return 42;
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	// initialize Qt
#ifdef Q_OS_LINUX
	{
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

		// Prefer Wayland backend only when a Wayland socket is actually
		// accessible. WAYLAND_DISPLAY is set by the compositor/Flatpak
		// only when the socket is forwarded; XDG_SESSION_TYPE alone is
		// not sufficient because Flatpak sandboxes may report "wayland"
		// there even when --socket=wayland is not granted.
		if (!env.contains("QT_QPA_PLATFORM") &&
			env.contains("WAYLAND_DISPLAY")) {
			qputenv("QT_QPA_PLATFORM", "wayland");
		}

		// Use xdgdesktopportal for file dialogs and theming on all
		// Linux DEs.  The old "gtk3" theme forced Qt to use GTK3's
		// native file-dialog integration, which deadlocks/freezes
		// on GNOME/Mutter (and other GTK-based compositors) with
		// Qt 6.  xdgdesktopportal delegates to the DE's own portal
		// implementation and works everywhere.
		if (!env.contains("QT_QPA_PLATFORMTHEME")) {
			qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
		}
	}
#endif

	Application app(argc, argv);

	// Build the crash reporter invocation the signal handler below will use,
	// and keep it current: a crash right after the user edits the PasteEE
	// key in Settings should still upload with the new one. See
	// rebuildCrashReporterInvocation()'s comment for why this cannot simply
	// be recomputed inside the handler itself.
	rebuildCrashReporterInvocation();
	if (app.settings()) {
		QObject::connect(app.settings().get(), &SettingsObject::SettingChanged,
						  &app, [](const Setting& setting, const QVariant&) {
							  if (setting.id() ==
								  QLatin1String("PasteEEAPIKey")) {
								  rebuildCrashReporterInvocation();
							  }
						  });
	}

	// Install crash signal handlers to launch meshmc-crashreporter
	signal(SIGSEGV, crashSignalHandler);
	signal(SIGABRT, crashSignalHandler);
#ifndef Q_OS_WIN
	signal(SIGBUS, crashSignalHandler);
#endif

	switch (app.status()) {
		case Application::StartingUp:
		case Application::Initialized: {
			Q_INIT_RESOURCE(multimc);
			Q_INIT_RESOURCE(backgrounds);
			Q_INIT_RESOURCE(documents);
			Q_INIT_RESOURCE(meshmc);
			Q_INIT_RESOURCE(shaders);

			Q_INIT_RESOURCE(pe_dark);
			Q_INIT_RESOURCE(pe_light);
			Q_INIT_RESOURCE(pe_blue);
			Q_INIT_RESOURCE(pe_colored);
			Q_INIT_RESOURCE(breeze_dark);
			Q_INIT_RESOURCE(breeze_light);
			Q_INIT_RESOURCE(OSX);
			Q_INIT_RESOURCE(iOS);
			Q_INIT_RESOURCE(flat);
			Q_INIT_RESOURCE(flat_white);

			int ret = app.exec();

			// Use _exit() to terminate immediately after the Qt event
			// loop ends.  All meaningful cleanup (instance save, plugin
			// shutdown, log flush/close) has already happened in the
			// Application::aboutToQuit handler while Qt was still alive.
			//
			// A normal return would run C++ static destructors via
			// exit()/__cxa_finalize.  Plugin .mmco modules statically
			// link MeshMC_logic, which embeds a duplicate global
			// `const Config BuildConfig` (non-trivial dtor with ~20
			// QString members).  Those duplicate destructors corrupt
			// the heap when they run after Qt is torn down, causing
			// glibc's "corrupted double-linked list" abort.
			//
			// _exit() bypasses all atexit handlers and static dtors,
			// avoiding the double-destruction entirely.  The OS
			// reclaims all process memory.
			_exit(ret);
		}
		case Application::Failed:
			return 1;
		case Application::Succeeded:
			return 0;
	}
}
