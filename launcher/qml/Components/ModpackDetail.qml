// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One modpack, opened from Discover, laid out like a project page. `pack` is
 * the opened row; `detail` is the model's lazily loaded body, gallery and
 * version list.
 */
Item {
    id: root

    component Subheading: Copy {
        role: "subheading"
    }

    component SideCard: BoxSurface {
        id: sideCard
        property string title
        default property alias content: sideBody.data
        implicitHeight: sideColumn.implicitHeight + Theme.space.lg * 2

        Column {
            id: sideColumn
            x: Theme.space.lg
            y: Theme.space.lg
            width: parent.width - Theme.space.lg * 2
            spacing: Theme.space.md

            Subheading {
                text: sideCard.title
            }
            Column {
                id: sideBody
                width: parent.width
                spacing: Theme.space.md
            }
        }
    }

    component Fact: Column {
        property string label
        property string value
        visible: value.length > 0
        width: parent ? parent.width : 0
        spacing: Theme.space.xxs
        Copy {
            text: parent.label
            role: "caption"
            tone: "tertiary"
            font.weight: Font.Medium
        }
        Copy {
            width: parent.width
            text: parent.value
            wrapMode: Text.Wrap
            role: "label"
        }
    }

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher; owned by C++ so installs survive leaving
    // the page.
    property var installer: null
    // How `detail.body` reads: "markdown" (Modrinth) or "html" (CurseForge); the page shows
    // `detail.bodyHtml` either way.
    property string bodyFormat: "markdown"
    property var watcher: null
    // DiscoverPage's page frame, so the page lines up with its toolbar.
    property int contentX: Theme.space.xl + Theme.space.xs
    property int contentWidth: width - contentX * 2
    readonly property int textMeasure: 720
    readonly property int sideWidth: 300
    readonly property int columnGap: Theme.space.xl
    readonly property bool twoColumns: root.contentWidth >= 820
    readonly property int logoSize: 96
    readonly property int maxCompatVersions: 6

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property var gallery: detail && detail.gallery ? detail.gallery : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded

    // The provider's ISO last-modified date as an age, worded like the card's ("5 days ago"), or "" when there is none.
    readonly property string updatedText: Format.ageText(root.pack.updated || "")

    readonly property var loaderNames: ({ neoforge: "NeoForge", forge: "Forge", fabric: "Fabric",
                                          quilt: "Quilt", liteloader: "LiteLoader" })
    function loaderName(id) {
        return root.loaderNames[id] || Format.titleCase(id)
    }

    function distinct(field) {
        var seen = []
        for (var i = 0; i < root.versions.length; ++i) {
            var values = root.versions[i][field] || []
            for (var j = 0; j < values.length; ++j) {
                if (seen.indexOf(values[j]) < 0)
                    seen.push(values[j])
            }
        }
        return seen
    }
    readonly property var gameVersions: root.distinct("gameVersions")
    readonly property var loaders: root.distinct("loaders").map((l) => root.loaderName(l))

    function versionLabel(v) {
        var parts = []
        if (v.gameVersions && v.gameVersions.length > 0)
            parts.push(v.gameVersions[0])
        if (v.loaders && v.loaders.length > 0)
            parts.push(v.loaders.map(l => root.loaderName(l)).join(", "))
        // Many packs prefix versions with the pack's name ("All the Mods 10-8.3"); show "8.3" instead.
        var label = String(v.versionNumber || v.name || "")
        var title = root.pack && root.pack.title ? String(root.pack.title) : ""
        if (title.length > 0 && label.toLowerCase().startsWith(title.toLowerCase())) {
            var rest = label.slice(title.length).replace(/^[\s\-_:v]+/i, "")
            if (rest.length > 0)
                label = rest
        }
        return label + (parts.length > 0 ? " (" + parts.join(", ") + ")" : "")
    }

    function install() {
        if (!root.installer || versionBox.currentIndex < 0)
            return
        var v = root.versions[versionBox.currentIndex]
        root.watcher = root.installer(root.pack.projectId, v.id,
                                      nameField.text.length > 0 ? nameField.text : root.pack.title, "")
    }

    onPackChanged: {
        watcher = null
        nameField.text = pack.title || ""
        flick.contentY = 0
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.y + column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: root.contentX
            y: Theme.space.xl
            width: root.contentWidth
            spacing: Theme.space.xl

            // Pulled left by its padding plus the chevron's side bearing, so the chevron's ink
            // starts on the page's left edge.
            Button {
                // The chevron's stroke starts 8 of its 24 units in; rounded to whole pixels.
                readonly property int chevronBearing: Math.round(Theme.icon.sm / 3)
                x: -(leftPadding + chevronBearing)
                flat: true
                text: qsTr("Back to results")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                onClicked: root.backRequested()
            }

            RowLayout {
                width: parent.width
                spacing: Theme.space.xl

                Item {
                    Layout.preferredWidth: root.logoSize
                    Layout.preferredHeight: root.logoSize
                    Layout.alignment: Qt.AlignTop

                    Image {
                        id: logoImg
                        anchors.fill: parent
                        source: root.pack.logoUrl || ""
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        asynchronous: true
                        visible: status === Image.Ready
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: logoImg.status !== Image.Ready
                        iconName: "package"
                        size: Theme.icon.lg
                        color: Theme.palette.textTertiary
                    }
                    CornerMatte {
                        cornerRadius: Theme.radius.xl
                        matte: Theme.palette.canvas
                        outline: Theme.palette.border
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space.sm

                    Copy {
                        Layout.fillWidth: true
                        text: root.pack.title || ""
                        elide: Text.ElideRight
                        role: "display"
                    }
                    Copy {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.pack.description || ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        tone: "secondary"
                        lineHeight: Theme.type.body.lineHeight
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.space.xxs
                        spacing: Theme.space.lg

                        Repeater {
                            model: [
                                { icon: "user", text: root.pack.author ? qsTr("by %1").arg(root.pack.author) : "" },
                                { icon: "download", text: (root.pack.downloads || 0) > 0
                                      ? qsTr("%1 downloads").arg(Format.compactNumber(root.pack.downloads)) : "" },
                                { icon: "users", text: (root.pack.follows || 0) > 0
                                      ? qsTr("%1 followers").arg(Format.compactNumber(root.pack.follows)) : "" },
                                { icon: "clock", text: root.updatedText.length > 0 ? qsTr("Updated %1").arg(root.updatedText) : "" }
                            ].filter((fact) => fact.text.length > 0)
                            delegate: Row {
                                required property var modelData
                                spacing: Theme.space.xs
                                MeshIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    iconName: modelData.icon
                                    size: Theme.icon.sm
                                    color: Theme.palette.textTertiary
                                }
                                Copy {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.text
                                    role: "label"
                                    tone: "secondary"
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.space.xs
                        visible: (root.pack.categories || []).length > 0
                        Repeater {
                            model: root.pack.categories || []
                            delegate: Tag {
                                required property var modelData
                                text: Format.titleCase(String(modelData))
                            }
                        }
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignTop
                    visible: !root.installed
                    enabled: !root.installing && versionBox.currentIndex >= 0
                    variant: "primary"
                    buttonSize: "lg"
                    text: root.installing ? qsTr("Installing…") : qsTr("Install")
                    icon.source: Icons.url("download")
                    onClicked: root.install()
                }
                Button {
                    Layout.alignment: Qt.AlignTop
                    visible: root.installed
                    variant: "primary"
                    buttonSize: "lg"
                    text: qsTr("Show in library")
                    icon.source: Icons.url("library")
                    onClicked: root.showInstanceRequested(root.watcher.instanceId || "")
                }
            }

            NoticeBar {
                width: parent.width
                visible: !!root.watcher && root.watcher.failed
                text: root.watcher && root.watcher.error.length > 0
                      ? qsTr("The install failed: %1").arg(root.watcher.error)
                      : qsTr("The install failed.")
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.palette.divider
            }

            GridLayout {
                width: parent.width
                columns: root.twoColumns ? 2 : 1
                columnSpacing: root.columnGap
                rowSpacing: Theme.space.xl

                ColumnLayout {
                    Layout.row: 0
                    Layout.column: root.twoColumns ? 1 : 0
                    Layout.preferredWidth: root.twoColumns ? root.sideWidth : -1
                    Layout.fillWidth: !root.twoColumns
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space.md

                    SideCard {
                        Layout.fillWidth: true
                        title: qsTr("Install")

                        Column {
                            width: parent.width
                            spacing: Theme.space.xs
                            FieldLabel { text: qsTr("Version") }
                            ComboBox {
                                id: versionBox
                                width: parent.width
                                enabled: root.versions.length > 0 && !root.installing
                                model: root.versions.map(v => root.versionLabel(v))
                                displayText: root.detail && root.detail.loading && root.versions.length === 0
                                             ? qsTr("Loading versions…")
                                             : root.versions.length === 0 ? qsTr("No versions found") : currentText
                                Accessible.name: qsTr("Version")
                                // Newest first from either provider, alphas included: start on the
                                // version the author features.
                                onModelChanged: {
                                    var featured = root.versions.findIndex(v => v.featured)
                                    currentIndex = featured >= 0 ? featured : 0
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.space.xs
                            FieldLabel { text: qsTr("Instance name") }
                            TextField {
                                id: nameField
                                width: parent.width
                                enabled: !root.installing
                                selectByMouse: true
                                Accessible.name: qsTr("Instance name")
                            }
                        }

                        Column {
                            width: parent.width
                            visible: !!root.watcher
                            spacing: Theme.space.xs

                            Copy {
                                width: parent.width
                                text: root.installed ? qsTr("Installed")
                                    : root.installing ? (root.watcher.status || qsTr("Installing…"))
                                    : root.watcher && root.watcher.failed ? qsTr("Install failed")
                                    : ""
                                wrapMode: Text.Wrap
                                role: "label"
                                font.weight: Font.Medium
                            }
                            LaunchProgressBar {
                                width: parent.width
                                visible: root.installing
                                progress: root.watcher ? root.watcher.progress : -1
                            }
                            Copy {
                                width: parent.width
                                visible: text.length > 0
                                text: root.watcher && root.watcher.failed ? root.watcher.error
                                    : root.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""
                                wrapMode: Text.Wrap
                                role: "caption"
                                tone: "secondary"
                            }
                        }
                    }

                    SideCard {
                        Layout.fillWidth: true
                        visible: root.gameVersions.length > 0 || root.loaders.length > 0
                        title: qsTr("Compatibility")

                        Column {
                            width: parent.width
                            visible: root.gameVersions.length > 0
                            spacing: Theme.space.xs
                            FieldLabel { text: qsTr("Minecraft") }
                            Flow {
                                width: parent.width
                                spacing: Theme.space.xs
                                Repeater {
                                    model: root.gameVersions.slice(0, root.maxCompatVersions)
                                    delegate: Tag {
                                        required property var modelData
                                        text: String(modelData)
                                    }
                                }
                                Tag {
                                    visible: root.gameVersions.length > root.maxCompatVersions
                                    text: qsTr("+%1").arg(root.gameVersions.length - root.maxCompatVersions)
                                }
                            }
                        }
                        Column {
                            width: parent.width
                            visible: root.loaders.length > 0
                            spacing: Theme.space.xs
                            FieldLabel { text: qsTr("Loaders") }
                            Flow {
                                width: parent.width
                                spacing: Theme.space.xs
                                Repeater {
                                    model: root.loaders
                                    delegate: Tag {
                                        required property var modelData
                                        text: String(modelData)
                                    }
                                }
                            }
                        }
                    }

                    SideCard {
                        Layout.fillWidth: true
                        title: qsTr("Details")

                        Fact {
                            label: qsTr("Author")
                            value: root.pack.author || ""
                        }
                        Fact {
                            label: qsTr("Downloads")
                            value: (root.pack.downloads || 0) > 0 ? Number(root.pack.downloads).toLocaleString(Qt.locale(), "f", 0) : ""
                        }
                        Fact {
                            label: qsTr("Updated")
                            value: {
                                var ms = Date.parse(root.pack.updated || "")
                                return ms > 0 ? Format.calendarDate(new Date(ms)) : ""
                            }
                        }
                        Fact {
                            label: qsTr("Versions")
                            value: root.versions.length > 0 ? String(root.versions.length) : ""
                        }
                    }
                }

                ColumnLayout {
                    Layout.row: root.twoColumns ? 0 : 1
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space.xl

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.gallery.length > 0
                        spacing: Theme.space.md

                        Subheading {
                            text: qsTr("Gallery")
                        }

                        Flickable {
                            Layout.fillWidth: true
                            Layout.preferredHeight: galleryRow.height
                            contentWidth: galleryRow.width
                            contentHeight: height
                            boundsBehavior: Flickable.StopAtBounds
                            clip: true

                            Row {
                                id: galleryRow
                                spacing: Theme.space.sm
                                Repeater {
                                    model: root.gallery
                                    delegate: Item {
                                        id: shot
                                        required property var modelData
                                        width: 220
                                        height: Math.round(width * 9 / 16)

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.radius.md
                                            color: Theme.palette.surfaceSunken
                                        }
                                        Image {
                                            anchors.fill: parent
                                            source: shot.modelData.url || ""
                                            sourceSize: Qt.size(width * 2, height * 2)
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: true
                                            mipmap: true
                                            asynchronous: true
                                        }
                                        CornerMatte {
                                            cornerRadius: Theme.radius.md
                                            matte: Theme.palette.canvas
                                            outline: Theme.palette.border
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: !!root.detail && ((root.detail.body || "").length > 0 || root.detail.loading)
                        spacing: Theme.space.md

                        Subheading {
                            text: qsTr("Description")
                        }

                        BusyIndicator {
                            visible: !!root.detail && root.detail.loading
                            running: visible
                        }

                        // Not a Text: a failed remote image could crash it (see RemoteRichText).
                        RemoteRichText {
                            Layout.fillWidth: true
                            Layout.maximumWidth: root.textMeasure
                            source: root.detail ? (root.detail.bodyHtml || "") : ""
                            format: "html"
                            article: true
                            onLinkActivated: (link) => Qt.openUrlExternally(link)
                        }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }
}
