// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * A line or two of quiet explanation in a card or side panel: wrapped secondary
 * text at the label size. Give it a width.
 */
Copy {
    role: "label"
    tone: "secondary"
    wrapMode: Text.Wrap
    lineHeight: spec.lineHeight
}
