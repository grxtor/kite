// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

// A soft drop shadow for a rounded popup surface that is not drawn as a
// GlassSurface. Place as a child of the surface Rectangle with `radius`
// matching it. z is -1: children paint over their parent, and without it the
// shadow darkened the surface itself (a dark dialog drew #252525 instead of
// surfaceOverlay). The drawing itself is Theme's Shadow, shared with the
// cards and popups elsewhere.
Shadow {
    z: -1
}
