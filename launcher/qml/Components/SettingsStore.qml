// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

pragma Singleton

import QtQuick

// The launcher-wide settings; the shell sets `adapter` once at startup.
// Setting rows use this unless they are given another `source`.
SettingsSource {}
