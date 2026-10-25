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

#include "RuntimeContext.h"

#include <QSysInfo>

#include "OpSys.h"

namespace
{
	bool isX86(const QString& architecture)
	{
		return architecture == QLatin1String("x86_64") ||
			   architecture == QLatin1String("x86");
	}
} // namespace

QString RuntimeContext::classifier() const
{
	return system + QLatin1Char('-') + architecture;
}

bool RuntimeContext::matches(const QString& name) const
{
	return name == classifier() || (isX86(architecture) && name == system);
}

QString
RuntimeContext::nativeClassifier(const QMap<QString, QString>& natives) const
{
	auto it = natives.constFind(classifier());
	if (it == natives.constEnd() && isX86(architecture)) {
		it = natives.constFind(system);
	}
	return it == natives.constEnd() ? QString() : it.value();
}

QString RuntimeContext::normalizeArchitecture(const QString& architecture)
{
	const QString arch = architecture.trimmed().toLower();
	if (arch == QLatin1String("amd64") || arch == QLatin1String("x64")) {
		return QStringLiteral("x86_64");
	}
	if (arch == QLatin1String("i386") || arch == QLatin1String("i486") ||
		arch == QLatin1String("i586") || arch == QLatin1String("i686")) {
		return QStringLiteral("x86");
	}
	if (arch == QLatin1String("aarch64")) {
		return QStringLiteral("arm64");
	}
	if (arch == QLatin1String("arm") || arch == QLatin1String("armhf") ||
		arch.startsWith(QLatin1String("armv7"))) {
		return QStringLiteral("arm32");
	}
	return arch;
}

RuntimeContext RuntimeContext::forJava(const QString& javaArchitecture,
									   const QString& bitness)
{
	RuntimeContext context;
	context.system = OpSys_toString(currentSystem);
	context.architecture = normalizeArchitecture(
		javaArchitecture.isEmpty() ? QSysInfo::buildCpuArchitecture()
								   : javaArchitecture);
	context.bitness = bitness;
	if (javaArchitecture.isEmpty() || bitness.isEmpty()) {
		context.bitness = (context.architecture == QLatin1String("x86") ||
						   context.architecture == QLatin1String("arm32"))
							  ? QStringLiteral("32")
							  : QStringLiteral("64");
	}
	return context;
}
