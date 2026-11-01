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

#include "Sys.h"

#include <QString>

#include <fstream>
#include <limits>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif

#ifdef Q_OS_MACOS
#include <sys/sysctl.h>
#endif

uint64_t Sys::getSystemRam()
{
#ifdef Q_OS_WINDOWS
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return (uint64_t)status.ullTotalPhys;
#elif defined(Q_OS_UNIX)
	std::string token;
#ifdef Q_OS_LINUX
	std::ifstream file("/proc/meminfo");
	while (file >> token) {
		if (token == "MemTotal:") {
			uint64_t mem;
			if (file >> mem) {
				return mem * 1024ull;
			} else {
				return 0;
			}
		}
		file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
#elif defined(Q_OS_FREEBSD)
	char buff[512];
	FILE* fp = popen("sysctl hw.physmem", "r");
	if (fp != NULL) {
		while (fgets(buff, 512, fp) != NULL) {
			std::string str(buff);
			uint64_t mem = std::stoull(str.substr(12, std::string::npos));
			return mem * 1024ull;
		}
	}
#elif defined(Q_OS_MACOS)
	uint64_t memsize;
	size_t memsizesize = sizeof(memsize);
	if (!sysctlbyname("hw.memsize", &memsize, &memsizesize, NULL, 0)) {
		return memsize;
	} else {
		return 0;
	}
#endif
#endif
	return 0;
}

bool Sys::isCPU64bit()
{
#ifdef Q_OS_MACOS
	return true;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
	return isSystem64bit();
#elif defined(Q_OS_WINDOWS)
	SYSTEM_INFO info;
	ZeroMemory(&info, sizeof(SYSTEM_INFO));
	GetNativeSystemInfo(&info);
	auto arch = info.wProcessorArchitecture;
	return arch == PROCESSOR_ARCHITECTURE_AMD64 ||
		   arch == PROCESSOR_ARCHITECTURE_IA64;
#endif
}

bool Sys::isSystem64bit()
{
#ifdef Q_OS_MACOS
	return true;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
	return QSysInfo::currentCpuArchitecture() == "x86_64";
#elif defined(_WIN64)
	return true;
#elif defined(_WIN32)
	BOOL f64 = false;
	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
	return false;
#endif
}
