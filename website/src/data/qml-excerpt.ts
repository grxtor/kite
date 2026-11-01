// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

/** Where the excerpt comes from, relative to the Kite repository. */
export const QML_EXCERPT_FILE = 'launcher/qml/Components/PlayButton.qml';

/**
 * A shortened excerpt of Kite's Play button, kept in step with the file by
 * hand. Comments and properties that don't help the point are left out.
 */
export const QML_EXCERPT = `AbstractButton {
    id: control

    property bool running: false
    property bool busy: false

    text: busy ? qsTr("Starting…") : running ? qsTr("Stop") : qsTr("Play")
    hoverEnabled: true

    readonly property color fill: running
        ? (down ? Qt.darker(Theme.palette.danger, 1.12)
                : hovered ? Qt.lighter(Theme.palette.danger, 1.08)
                          : Theme.palette.danger)
        : (down ? Theme.palette.accentPressed
                : hovered ? Theme.palette.accentHover
                          : Theme.palette.accent)

    background: KiteShape {
        radius: Theme.radius.md
        color: control.fill
        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast }
        }
    }
}`;
