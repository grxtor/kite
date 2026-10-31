/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * LogIngester: pulls the latest log and crash-report text out of an instance for the analysis engine.
 */

#pragma once

#include "plugin/sdk/mmco_cxx_sdk.hpp"

class LogIngester
{
  public:
	struct Bundle {
		QString combinedText;
		QStringList sources; // file paths we read
		bool fromLatestLog = false;
		bool fromCrashReport = false;
	};

	Bundle ingestForInstance(const QString& instancePath) const;

	/* For testing — pull from an arbitrary path or directory. */
	Bundle ingestFromPath(const QString& path) const;
};
