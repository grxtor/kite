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

#include "CrashExitCodes.h"

#include <QCoreApplication>
#include <algorithm>
#include <iterator>
#include <limits>

namespace CrashDoctor::ExitCodes
{
	namespace
	{
		constexpr char kContext[] = "CrashExitCodes";
		constexpr int kHexDigits = 8;
		constexpr int kHexBase = 16;

		// Statuses a crashed game ends with; their names in winnt.h / ntstatus.h.
		const WindowsStatus kStatuses[] = {
			// STATUS_ACCESS_VIOLATION
			{0xC0000005u, QT_TRANSLATE_NOOP("CrashExitCodes", "memory access violation")},
			// STATUS_STACK_BUFFER_OVERRUN, also every __fastfail
			{0xC0000409u, QT_TRANSLATE_NOOP("CrashExitCodes", "stack buffer overrun")},
			// STATUS_ILLEGAL_INSTRUCTION
			{0xC000001Du, QT_TRANSLATE_NOOP("CrashExitCodes", "illegal instruction")},
			// STATUS_STACK_OVERFLOW
			{0xC00000FDu, QT_TRANSLATE_NOOP("CrashExitCodes", "stack overflow")},
			// STATUS_HEAP_CORRUPTION
			{0xC0000374u, QT_TRANSLATE_NOOP("CrashExitCodes", "heap corruption")},
			// STATUS_BREAKPOINT
			{0x80000003u, QT_TRANSLATE_NOOP("CrashExitCodes", "breakpoint")},
		};
	} // namespace

	std::optional<int> fromNumber(qint64 value)
	{
		if (value < std::numeric_limits<qint32>::min() || value > std::numeric_limits<quint32>::max()) {
			return std::nullopt;
		}
		return static_cast<qint32>(static_cast<quint32>(value));
	}

	const WindowsStatus* windowsStatus(qint64 exitCode)
	{
		const auto code = fromNumber(exitCode);
		if (!code) {
			return nullptr;
		}
		const auto unsignedCode = static_cast<quint32>(*code);
		const auto* found = std::find_if(std::begin(kStatuses), std::end(kStatuses),
										 [unsignedCode](const WindowsStatus& status) {
											 return status.code == unsignedCode;
										 });
		return found == std::end(kStatuses) ? nullptr : found;
	}

	QString hex(quint32 code)
	{
		return QStringLiteral("0x") +
			   QString::number(code, kHexBase).toUpper().rightJustified(kHexDigits, QLatin1Char('0'));
	}

	QString describe(int exitCode)
	{
		const WindowsStatus* status = windowsStatus(exitCode);
		if (!status) {
			return QString::number(exitCode);
		}
		return QStringLiteral("%1 (%2)").arg(hex(status->code),
											 QCoreApplication::translate(kContext, status->meaning));
	}
} // namespace CrashDoctor::ExitCodes
