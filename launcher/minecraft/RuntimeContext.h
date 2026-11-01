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

#include <QMap>
#include <QString>

/*
 * What a launch runs on, as far as picking libraries goes: the OS and the
 * architecture of the Java that runs the game (natives must match the JVM, not
 * the machine: an x86_64 Java on Apple Silicon runs under Rosetta and loads
 * x86_64 natives).
 *
 * The meta names platforms as Mojang's files did when only x86 and x86_64
 * existed: bare "osx", "windows", "linux" mean those. Every other architecture
 * has its own name ("osx-arm64", "windows-arm64", "linux-arm32") and only that
 * applies to it. So LWJGL 3.2.2 is allowed except on "osx-arm64", where a 3.3.1
 * build with arm64 natives is allowed instead.
 */
struct RuntimeContext
{
	/// "osx", "windows", "linux" or "freebsd".
	QString system;
	/// The Java's architecture in the meta's words: "x86_64", "x86",
	/// "arm64", "arm32", or whatever else it reported.
	QString architecture;
	/// "32" or "64", for natives with ${arch} in their classifier.
	QString bitness;

	/// "<system>-<architecture>", e.g. "osx-arm64".
	QString classifier() const;
	/// Whether a rule or natives entry for the platform @p name applies:
	/// "<system>-<architecture>" always, the bare system name only on
	/// x86/x86_64.
	bool matches(const QString& name) const;
	/// The classifier to use out of @p natives (platform name -> classifier):
	/// the one for "<system>-<architecture>", else, on x86 and x86_64, the
	/// one for the bare system name. Null when none applies.
	QString nativeClassifier(const QMap<QString, QString>& natives) const;

	bool operator==(const RuntimeContext& other) const
	{
		return system == other.system && architecture == other.architecture &&
			   bitness == other.bitness;
	}
	bool operator!=(const RuntimeContext& other) const
	{
		return !(*this == other);
	}

	/// The meta's word for an architecture as Java's os.arch or Qt's QSysInfo
	/// name it: "amd64" is "x86_64", "aarch64" "arm64", "i686" "x86", "armhf"
	/// "arm32".
	static QString normalizeArchitecture(const QString& architecture);

	/// The context of this OS and a Java that reported @p javaArchitecture
	/// (os.arch) and @p bitness ("32"/"64"). An empty architecture (no Java
	/// checked yet) means the one Kite was built for, which the Java Kite
	/// downloads is for too.
	static RuntimeContext forJava(const QString& javaArchitecture,
								  const QString& bitness);
};
