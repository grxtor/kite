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
/* Not declared by every platform's <unistd.h> (not by macOS's; see
 * environ(7)). Declared at namespace scope here: inside a function or an
 * unnamed namespace it would get internal linkage and not bind to libc's
 * symbol. */
extern char** environ;
#endif

namespace
{
	/* Async-signal-safe crash reporter launch: the handler may run on any
	 * thread in the middle of Qt's or the allocator's state, so it must not
	 * touch Qt, the heap or the log file. Everything is computed up front by
	 * rebuildCrashReporterInvocation(); the handler only reads the
	 * plain-old-data invocation and calls CreateProcessW or posix_spawn. Two
	 * slots (g_invocationStorage) and an atomic index (g_activeInvocation): a
	 * rebuild fills the inactive slot and publishes it with one atomic store;
	 * -1 means nothing built yet (a silent no-op). */
	constexpr int kMaxPathChars = 1024;
	constexpr int kMaxArgChars = 512;
#ifdef Q_OS_WIN
	// The full command line (quoted path + three quoted "--flag value" pairs).
	// quoteWindowsArg() can at most double an argument (backslash runs before
	// a quote or at the end) plus one character per embedded quote, hence
	// double the unescaped budget.
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
		// rebuilt together with them.
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

	/* Quotes one argument for CreateProcessW's lpCommandLine by
	 * CommandLineToArgvW's rules: backslashes matter only before a double
	 * quote (a run of N becomes 2N, plus one to escape the quote). apiKey is
	 * user-editable text, and an unescaped '"' would inject an argument into
	 * kite-crashreporter. */
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
		// Trailing backslashes are doubled so they do not escape the closing
		// quote appended below.
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

	/* Rebuilds the inactive slot from scratch and publishes it. GUI thread
	 * only: called once at startup (after Application and its settings exist)
	 * and whenever PasteEEAPIKey changes. */
	void rebuildCrashReporterInvocation()
	{
		QString crashReporterName = QStringLiteral("kite-crashreporter");
#ifdef Q_OS_WIN
		crashReporterName += QStringLiteral(".exe");
#endif
		const QString path = FS::PathCombine(
			QApplication::applicationDirPath(), crashReporterName);
		if (!QFile::exists(path)) {
			// Nothing to launch. A previously published invocation is left
			// alone: a stale-but-valid one beats none.
			return;
		}

		QString apiKey = QStringLiteral("public");
		if (APPLICATION && APPLICATION->settings()) {
			const QString key =
				APPLICATION->settings()->get("PasteEEAPIKey").toString();
			apiKey = (key != QLatin1String("kite") && !key.isEmpty())
						 ? key
						 : BuildConfig.PASTE_EE_KEY;
		}
		const QString logDir = QDir::currentPath();
		const QString name = BuildConfig.KITE_NAME;

		const int current = g_activeInvocation.load(std::memory_order_relaxed);
		const int nextSlot = current == 0 ? 1 : 0;
		CrashReporterInvocation& slot = g_invocationStorage[nextSlot];
		slot.valid = false;

#ifdef Q_OS_WIN
		if (!copyToBuffer(slot.path, kMaxPathChars, path.toStdWString())) {
			return;
		}
		// The command line's own first token is the module path too (a child
		// reads it via GetCommandLine()). apiKey is arbitrary user-editable
		// text, so every argument goes through quoteWindowsArg().
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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#ifdef Q_OS_LINUX
	{
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

		// Prefer Wayland only when a Wayland socket is actually accessible:
		// WAYLAND_DISPLAY is set only when the socket is forwarded, while
		// XDG_SESSION_TYPE can say "wayland" in a Flatpak sandbox without
		// --socket=wayland.
		if (!env.contains("QT_QPA_PLATFORM") &&
			env.contains("WAYLAND_DISPLAY")) {
			qputenv("QT_QPA_PLATFORM", "wayland");
		}

		// Use xdgdesktopportal for file dialogs and theming on all Linux DEs:
		// the old "gtk3" theme deadlocks on GNOME/Mutter with Qt 6.
		if (!env.contains("QT_QPA_PLATFORMTHEME")) {
			qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
		}
	}
#endif

	Application app(argc, argv);

	// Builds the invocation the signal handler uses and keeps it current, so a
	// crash after the user edits the PasteEE key still uploads with the new
	// one. The handler cannot recompute it itself.
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
			Q_INIT_RESOURCE(kite);

			int ret = app.exec();

			// _exit() once the event loop ends: cleanup already ran in
			// aboutToQuit. A normal return would run static destructors, and
			// plugin .mmco modules statically link Kite_logic with a duplicate
			// global `const Config BuildConfig` whose destructors corrupt the
			// heap after Qt is torn down ("corrupted double-linked list").
			_exit(ret);
		}
		case Application::Failed:
			// Plugins may be loaded by now: a normal return would run their
			// static destructors (see above), so end like the normal path.
			app.shutdownForExit();
			fflush(nullptr);
			_exit(1);
		case Application::Succeeded:
			return 0;
	}
}
