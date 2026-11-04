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

#include <QString>
#include <QtGlobal>
#include <optional>

/*
 * The exit codes a game ends with on Windows when native code broke: NTSTATUS
 * values, which QProcess hands over as negative numbers (0xC0000005 is
 * -1073741819) and a wrapper sometimes as unsigned ones (3221225477). The crash
 * doctor shows "Exit code 0xC0000005 (memory access violation)" for them
 * instead of the raw number; -1, 1 and every other code stay a plain number.
 */
namespace CrashDoctor::ExitCodes
{
	struct WindowsStatus {
		quint32 code = 0;
		/// Kite's words for it, translated in the context "CrashExitCodes".
		const char* meaning = nullptr;
	};

	/// The Windows status @p exitCode is, in either form, or null.
	const WindowsStatus* windowsStatus(qint64 exitCode);
	/// @p value as the int an exit code is kept in: an unsigned 32-bit form
	/// becomes its signed one; nothing outside 32 bits.
	std::optional<int> fromNumber(qint64 value);
	/// "0xC0000005"
	QString hex(quint32 code);
	/// "0xC0000005 (memory access violation)" for a Windows status, else the
	/// number ("1").
	QString describe(int exitCode);
} // namespace CrashDoctor::ExitCodes
