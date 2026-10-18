// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One modpack, opened from Discover, laid out like a Modrinth project page:
 * a header with the pack's logo, name, pitch, its numbers and its tags,
 * and Install; under it the gallery and the description on the left, and on
 * the right the Install card (which version, what to call the instance, and
 * the install's progress), what the pack runs on, and its details. On a
 * narrow page the side cards move above the description.
 *
 * `pack` is the row that was opened (title, author, logo, galleryUrl,
 * accentColor...); `detail` is the model's lazily loaded body, gallery and
 * version list. Provider-neutral: Modrinth's and CurseForge's models give
 * the same roles and the same `detail` shape; only the raw body's format
 * differs (`bodyFormat`), and the page reads `bodyHtml`, the same text every
 * model already rewrote into one plain HTML article.
 *
 * Only the pack's own pictures appear here: its logo, and its gallery once
 * the detail has loaded. No picture, no picture slot -- the header never
 * stands an empty banner in for one.
 */
Item {
    id: root

    /*
     * A card in the side column: a title over its own content.
     */
    component SideCard: Rectangle {
        id: sideCard
        property string title
        default property alias content: sideBody.data
        implicitHeight: sideColumn.implicitHeight + Theme.space.lg * 2
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: sideColumn
            x: Theme.space.lg
            y: Theme.space.lg
            width: parent.width - Theme.space.lg * 2
            spacing: Theme.space.md

            Text {
                text: sideCard.title
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
            }
            Column {
                id: sideBody
                width: parent.width
                spacing: Theme.space.md
            }
        }
    }

    /*
     * One fact in a side card: a quiet label over its value.
     */
    component Fact: Column {
        property string label
        property string value
        visible: value.length > 0
        width: parent ? parent.width : 0
        spacing: Theme.space.xxs
        Text {
            text: parent.label
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
            font.weight: Font.Medium
        }
        Text {
            width: parent.width
            text: parent.value
            wrapMode: Text.Wrap
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
    }

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher, owned by C++ so
    // the install survives leaving this page.
    property var installer: null
    // "markdown" (Modrinth) or "html" (CurseForge): how `detail.body` reads.
    // The page itself shows `detail.bodyHtml`, which is HTML either way.
    property string bodyFormat: "markdown"
    // TaskWatcher of the install started from here, if any.
    property var watcher: null
    // DiscoverPage's page frame, so the page lines up with its toolbar.
    property int contentX: Theme.space.xl + Theme.space.xs
    property int contentWidth: width - contentX * 2
    // The description's own measure: lines of a comfortable length, and
    // the width its pictures scale down to.
    readonly property int textMeasure: 720
    readonly property int sideWidth: 300
    readonly property int columnGap: Theme.space.xl
    // Room for the description beside the side column; below it, one
    // column with the side cards first.
    readonly property bool twoColumns: root.contentWidth >= 820
    readonly property int logoSize: 96
    readonly property int maxCompatVersions: 6

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property var gallery: detail && detail.gallery ? detail.gallery : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded

    // The provider's ISO last-modified date as Format.lastPlayed() words it,
    // or "" when there is none.
    readonly property string updatedText: {
        var ms = Date.parse(root.pack.updated || "")
        return ms > 0 ? Format.lastPlayed(ms) : ""
    }

    // Loader ids as the projects spell their own names.
    readonly property var loaderNames: ({ neoforge: "NeoForge", forge: "Forge", fabric: "Fabric",
                                          quilt: "Quilt", liteloader: "LiteLoader" })
    function loaderName(id) {
        return root.loaderNames[id] || (id.length > 0 ? id.charAt(0).toUpperCase() + id.slice(1) : id)
    }

    // What the versions run on, newest first and each named once: the
    // Minecraft versions and the loaders, for the Compatibility card.
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
        // Many packs number their versions with the pack's own name in front
        // ("All the Mods 10-8.3"); repeating it on every row of the list only
        // pushes the part that differs to the end. Show "8.3" instead.
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

    function titleCase(word) {
        return word.length > 0 ? word.charAt(0).toUpperCase() + word.slice(1) : word
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
            // The shell's panel adds no padding; the page pads itself.
            y: Theme.space.lg
            width: root.contentWidth
            spacing: Theme.space.xl

            // Pulled left by its own padding plus the chevron's side
            // bearing, so the chevron's ink -- not the button's hover
            // plate, nor the empty left third of the glyph's box -- starts
            // exactly on the page's left edge, like the header below it.
            Button {
                // The chevron-left glyph's stroke starts 8 of its 24 units
                // in; rounded so the label stays on whole pixels.
                readonly property int chevronBearing: Math.round(Theme.icon.sm / 3)
                x: -(leftPadding + chevronBearing)
                flat: true
                text: qsTr("Back to results")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                onClicked: root.backRequested()
            }

            // -- Header: logo, name, pitch, numbers, tags, Install ----------
            RowLayout {
                width: parent.width
                spacing: Theme.space.xl

                Item {
                    Layout.preferredWidth: root.logoSize
                    Layout.preferredHeight: root.logoSize
                    Layout.alignment: Qt.AlignTop

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius.xl
                        color: Theme.dark ? Theme.palette.surfaceRaised : Theme.palette.surfaceSunken
                    }
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

                    Text {
                        Layout.fillWidth: true
                        text: root.pack.title || ""
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.heading
                        font.pixelSize: Theme.type.display.pixelSize
                        font.weight: Theme.type.display.weight
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.pack.description || ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                        lineHeight: Theme.type.body.lineHeight
                    }

                    // Who made it, how many have it, when it last changed --
                    // quiet icon-and-text facts, as on Modrinth's header.
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
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.text
                                    color: Theme.palette.textSecondary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
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
                                text: root.titleCase(String(modelData))
                            }
                        }
                    }
                }

                // The page's one primary action. Installs the version and
                // name chosen in the Install card -- the featured version
                // and the pack's own name until changed.
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

            // A failed install says so right under the button that started
            // it; Install can be pressed again.
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

            // -- Body: description on the left, side cards on the right ------
            GridLayout {
                width: parent.width
                columns: root.twoColumns ? 2 : 1
                columnSpacing: root.columnGap
                rowSpacing: Theme.space.xl

                // The side column: first on a narrow page, so Install stays
                // near the top.
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
                                // Newest first from either provider, alphas
                                // included: start on the version the author
                                // features, if there is one.
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

                        // Only once an install has started: progress, then
                        // the outcome. Before that the header's button says
                        // it all.
                        Column {
                            width: parent.width
                            visible: !!root.watcher
                            spacing: Theme.space.xs

                            Text {
                                width: parent.width
                                text: root.installed ? qsTr("Installed")
                                    : root.installing ? (root.watcher.status || qsTr("Installing…"))
                                    : root.watcher && root.watcher.failed ? qsTr("Install failed")
                                    : ""
                                wrapMode: Text.Wrap
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                                font.weight: Font.Medium
                            }
                            LaunchProgressBar {
                                width: parent.width
                                visible: root.installing
                                progress: root.watcher ? root.watcher.progress : -1
                            }
                            Text {
                                width: parent.width
                                visible: text.length > 0
                                text: root.watcher && root.watcher.failed ? root.watcher.error
                                    : root.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""
                                wrapMode: Text.Wrap
                                color: Theme.palette.textSecondary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.caption.pixelSize
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

                // The main column: gallery, then the pack's own page.
                ColumnLayout {
                    Layout.row: root.twoColumns ? 0 : 1
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space.xl

                    // Gallery strip -- only once the detail fetch actually
                    // has one.
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.gallery.length > 0
                        spacing: Theme.space.md

                        Text {
                            text: qsTr("Gallery")
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.heading
                            font.pixelSize: Theme.type.heading.pixelSize
                            font.weight: Theme.type.heading.weight
                        }

                        Flickable {
                            id: galleryFlick
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

                    // About: the pack's own page, as RichBody rewrote it
                    // (see RemoteRichText's `article`).
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: !!root.detail && ((root.detail.body || "").length > 0 || root.detail.loading)
                        spacing: Theme.space.md

                        Text {
                            text: qsTr("Description")
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.heading
                            font.pixelSize: Theme.type.heading.pixelSize
                            font.weight: Theme.type.heading.weight
                        }

                        BusyIndicator {
                            visible: !!root.detail && root.detail.loading
                            running: visible
                        }

                        // Not a Text: the body links images from anywhere,
                        // and a failed one could crash Text (see
                        // RemoteRichText).
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
