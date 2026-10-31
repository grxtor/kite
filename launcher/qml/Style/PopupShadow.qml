// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

// A drop shadow for a rounded popup surface that is not a GlassSurface. Place
// as a child of the surface Rectangle with a matching `radius`. z is -1:
// children paint over their parent, and otherwise the shadow darkens the
// surface itself.
Shadow {
    z: -1
}
