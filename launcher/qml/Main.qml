// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme
import MeshMC.Components

ApplicationWindow {
    id: root

    required property var instanceModel
    required property var selection
    // QmlShell: actions (launchInstance, killInstance, ...), account summary
    // and the extra instance models the library needs.
    required property var shell

    readonly property string moduleName: "MeshMC"

    width: 1240
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    title: "MeshMC"
    color: Theme.palette.canvas

    // Whether launcher/qml/Cat was built at all (MESHMC_HAS_CAT): the QML
    // shell has no other way to tell a Quick3D-less build apart from one
    // where the cat is merely turned off, and Settings needs that
    // distinction to hide the cat's rows instead of offering a switch that
    // would do nothing. Set from QmlShell::rootProperties(); false is the
    // right default for QmlModule_test, which never sets it.
    property bool catAvailable: false

    // The macOS menu bar (AppMenuBar.qml), on the native Cocoa platform
    // only and with the same switch the widget bar uses -- anywhere else
    // Qt would draw it inside the window instead, so it stays hidden.
    readonly property bool nativeMenuBar: Qt.platform.os === "osx" && Qt.platform.pluginName === "cocoa"
                                          && SettingsStore.bool("MacNativeMenuBar")
    menuBar: AppMenuBar {
        visible: root.nativeMenuBar
        about: root.shell && root.shell.about ? root.shell.about : null
        updaterAvailable: !!root.shell && !!root.shell.updaterAvailable
        onNewInstanceRequested: (mode) => root.openNewInstance(mode)
        onNavigateRequested: (target) => root.navigate(target)
        onCheckForUpdatesRequested: root.call("checkForUpdates")
    }

    property string selectedId: ""
    // "home", "library", "discover", "settings", "instance" or "accounts".
    property string page: "home"
    // The instance the instance page shows.
    property string openedInstanceId: ""
    property var openedDetails: null

    function openInstance(id) {
        if (!root.shell || typeof root.shell.instanceDetails !== "function")
            return
        root.openedDetails = root.shell.instanceDetails(id)
        root.openedInstanceId = id
        root.selectedId = id
        root.page = "instance"
    }

    Component.onCompleted: {
        if (root.shell && root.shell.settings) {
            SettingsStore.adapter = root.shell.settings
            // Java management (JavaStore.qml), read by the Java settings
            // rows wherever they appear.
            JavaStore.controller = root.shell.javaController
            JavaStore.download = root.shell.javaDownload
            // Folder and update settings and the total play time
            // (LauncherStore.qml), read by Settings and the library.
            LauncherStore.folders = root.shell.folderSettings
            LauncherStore.updates = root.shell.updateSettings
            LauncherStore.playTime = root.shell.playTime
            LauncherStore.catPacks = root.shell.catPacks
            var mode = SettingsStore.string("UiThemeMode")
            if (mode === "dark" || mode === "light" || mode === "system")
                Theme.mode = mode
            var scheme = SettingsStore.string("UiPalette")
            if (scheme === "amethyst" || scheme === "ember" || scheme === "diamond" || scheme === "grass")
                Theme.scheme = scheme
        }
        onboarding.start()
        if (root.devRoute.length > 0)
            Qt.callLater(root.applyDevRoute)
    }

    // Startup route for review snapshots (MESHMC_QML_ROUTE): ";"-separated
    // steps such as "theme=light;scheme=ember;size=1400x900;page=settings;section=java",
    // "instance=<id>;tab=mods", "page=discover;detail=0", "newinstance",
    // "gallery" or "page=home;crashed=<id>" (marks that instance as having
    // crashed last time, for the Home page's chip -- see QmlShell's
    // debugMarkInstanceCrashed()). "nav=<path>" goes through navigate()
    // below ("nav=settings/java", "nav=instance/fabric/settings"), and
    // "import=<path or url>" acts like `meshmc --import` (onImportRequested).
    // "page=accounts;accounts-demo" opens the skin editor on demo data
    // ("accounts-demo=cape" / "=elytra" start on the player's back); that
    // step is read by AccountsController, not here.
    property string devRoute: ""
    function applyDevRoute() {
        var steps = root.devRoute.split(";")
        for (var i = 0; i < steps.length; ++i) {
            var eq = steps[i].indexOf("=")
            var key = (eq < 0 ? steps[i] : steps[i].slice(0, eq)).trim()
            var value = eq < 0 ? "" : steps[i].slice(eq + 1).trim()
            if (key === "theme")
                Theme.mode = value
            else if (key === "scheme")
                Theme.scheme = value
            else if (key === "size") {
                var wh = value.split("x")
                root.width = parseInt(wh[0])
                root.height = parseInt(wh[1])
            } else if (key === "page")
                root.page = value
            else if (key === "instance")
                root.openInstance(value)
            else if (key === "tab")
                instancePage.tab = value
            else if (key === "section")
                settingsPage.section = value
            else if (key === "select")
                root.selectedId = value
            else if (key === "detail")
                devDetail.start()
            else if (key === "newinstance")
                root.openNewInstance(value)
            else if (key === "nav")
                root.navigate(value)
            else if (key === "import")
                root.openImport(value)
            else if (key === "gallery")
                galleryLoader.active = true
            else if (key === "profilemenu")
                Qt.callLater(() => topBar.openProfileMenu())
            else if (key === "picker")
                Qt.callLater(() => playDock.openPicker())
            // The play bar's Play options menu, open; "launchmenu=playAs" or
            // "=profiler" opens that submenu as well.
            else if (key === "launchmenu")
                Qt.callLater(playDock.openLaunchMenu, value)
            // The play bar mid-launch, without a launch: "launching" shows
            // "Downloading assets" at 42 %, "launching=<stage>:<0..1>" any
            // stage and progress (-1 for no idea how far).
            else if (key === "launching") {
                var launchArgs = value.length > 0 ? value.split(":") : ["Downloading assets", "0.42"]
                playDock.previewLaunch = {
                    status: launchArgs[0],
                    progress: launchArgs.length > 1 ? parseFloat(launchArgs[1]) : -1
                }
            }
            // The Export dialog over instance <id>, optionally on a format:
            // "export=fabric" or "export=fabric:modrinth".
            else if (key === "export") {
                var exportArgs = value.split(":")
                Qt.callLater(libraryPage.exportInstance, exportArgs[0], exportArgs[1] || "")
            }
            // The Create shortcut dialog over instance <id>.
            else if (key === "shortcut")
                Qt.callLater(libraryPage.createShortcut, value)
            // The library card ⋯ menu for instance <id>, open.
            else if (key === "librarymenu")
                Qt.callLater(libraryPage.openInstanceMenu, value)
            // The group header ⋯ menu of group <name>, open.
            else if (key === "groupmenu")
                Qt.callLater(libraryPage.openGroupMenu, value)
            // The duplicate dialog over instance <id>.
            else if (key === "duplicate")
                Qt.callLater(libraryPage.duplicateInstance, value)
            // The instance page header ⋯ menu, open (after instance=<id>).
            else if (key === "instancemenu")
                Qt.callLater(instancePage.openHeaderMenu)
            // The Content tab's "Export list" dialog (after instance=<id>).
            else if (key === "exportlist")
                Qt.callLater(instancePage.openContentExportList)
            // Add content's Filters popup (after instance=<id>).
            else if (key === "browsefilters")
                Qt.callLater(instancePage.openBrowseFilters)
            // A tab's own state, after instance=<id>: "versionmore",
            // "newcomponent", "otherlogs", "logfind", "shotselect",
            // "shotrename" -- see InstancePage.review().
            else if (key === "review")
                Qt.callLater(instancePage.review, value)
            // A question only a real launch asks -- see QmlShell's
            // debugShowRequest(): "pickAccount" or "busySkip".
            else if (key === "ask")
                root.call("debugShowRequest", value)
            else if (key === "sidebar")
                SettingsStore.setValue("UiSidebarCollapsed", value === "collapsed")
            else if (key === "crashed")
                root.call("debugMarkInstanceCrashed", value)
            // Deterministic cat pose for review snapshots -- see
            // CatOverlay.qml's own `demo` property.
            // The icon picker, as the library's "Change icon" opens it.
            else if (key === "iconpicker") {
                iconDialog.targetId = root.dockInstanceId
                iconDialog.current = value.length > 0 ? value : "grass"
                iconDialog.open()
            }
            // Pins Home's header scene to "day", "golden" or "night".
            else if (key === "mood")
                homePage.moodOverride = value
            else if (key === "cat" && catLoader.item)
                catLoader.item.demo = value
        }
    }
    Timer {
        id: devDetail
        interval: 3500
        onTriggered: {
            var parts = root.devRoute.match(/detail=(\d+)/)
            discoverPage.openResult(parts ? parseInt(parts[1]) : 0)
        }
    }
    onSelectedIdChanged: {
        if (selectedId.length > 0)
            root.selection.selectOnly(selectedId)
        else
            root.selection.clear()
    }

    // mode is "create" (default) or "import" -- see NewInstanceDialog.qml.
    // source pre-fills import mode's file/URL field; empty otherwise.
    function openNewInstance(mode, source) {
        if (!root.shell || !root.shell.newInstance)
            return
        newInstanceDialog.controller = root.shell.newInstance
        newInstanceDialog.mode = mode === "import" ? "import" : "create"
        newInstanceDialog.presetSource = source || ""
        newInstanceDialog.open()
    }

    // An import asked for from outside the window: `meshmc --import`, or a
    // second launcher passing a file on (QmlShell::requestImport()). With
    // the dialog already open it takes the new source in place, unless it
    // is still busy with the last one.
    function openImport(source) {
        if (!newInstanceDialog.opened) {
            root.openNewInstance("import", source)
            return
        }
        if (newInstanceDialog.creating) {
            toast.show(qsTr("Another instance is still being set up. Try again once it is in your library."))
            return
        }
        newInstanceDialog.mode = "import"
        newInstanceDialog.setImportSource(source)
    }

    /*
     * Cross-page links: any page can send the user somewhere else in the
     * app with one call (an instance's Java settings pointing at the
     * launcher-wide Java section, say). A page declares a signal such as
     * `signal navigateRequested(string target)` and its handler here
     * forwards it: `onNavigateRequested: (target) => root.navigate(target)`.
     *
     * Targets are paths, given whole or split in two (navigate("settings",
     * "java") is navigate("settings/java")):
     *   "home", "library", "discover", "accounts"
     *   "settings" or "settings/<section>"  a SettingsPage.qml section id
     *                                       ("java", "proxy", "about", ...)
     *   "instance/<id>" or "instance/<id>/<tab>"
     *                                       an InstancePage.qml tab id
     *                                       ("settings", "content", "log", ...)
     * Instance ids are folder names and never contain "/". Returns false,
     * leaving the current page alone, for a path it does not know or an
     * instance that does not exist.
     */
    function navigate(page, sub) {
        var path = sub === undefined || sub === null || String(sub).length === 0
                ? String(page) : page + "/" + sub
        var parts = path.split("/")
        switch (parts[0]) {
        case "home":
        case "library":
        case "discover":
        case "accounts":
            root.page = parts[0]
            return true
        case "settings":
            if (parts.length > 1 && parts[1].length > 0)
                settingsPage.section = parts[1]
            root.page = "settings"
            return true
        case "instance":
            if (parts.length < 2 || parts[1].length === 0 || !root.shell
                    || typeof root.shell.instanceDetails !== "function"
                    || !root.shell.instanceDetails(parts[1]))
                return false
            root.openInstance(parts[1])
            if (parts.length > 2 && parts[2].length > 0)
                instancePage.tab = parts[2]
            return true
        }
        return false
    }

    // Play, everywhere in the shell. With no account at all the launch
    // flow could only ask whether to open the Accounts page; go there
    // straight away and say what is missing instead.
    function launch(id) {
        if (root.sendToAccountsIfNone())
            return
        root.call("launchInstance", id)
    }
    function sendToAccountsIfNone() {
        if (!root.shell || root.shell.accountCount !== 0)
            return false
        root.navigate("accounts")
        toast.show(qsTr("Sign in with the Microsoft account that owns Minecraft to play."))
        return true
    }

    // The other ways to play (LaunchOptionsMenu.qml): "offline" or "demo"
    // (QmlShell::launchInstanceMode()), as another account, and the
    // instance's profiler. The demo needs no account; the rest do.
    function launchMode(id, mode) {
        if (mode !== "demo" && root.sendToAccountsIfNone())
            return
        if (root.shell && typeof root.shell.launchInstanceMode === "function")
            root.shell.launchInstanceMode(id, mode)
    }
    function launchAs(id, accountId) {
        if (root.shell && typeof root.shell.launchInstanceAs === "function")
            root.shell.launchInstanceAs(id, accountId)
    }
    function setProfiler(id, profilerKey) {
        if (root.shell && typeof root.shell.setInstanceProfiler === "function")
            root.shell.setInstanceProfiler(id, profilerKey)
    }
    // What a LaunchOptionsMenu offers for instance @p id; a page hands this
    // to its menu as `optionsFor` (see QmlShell::launchOptions()).
    function launchOptions(id) {
        return root.shell && typeof root.shell.launchOptions === "function"
                ? root.shell.launchOptions(id) : ({})
    }

    // Home/Library/Discover/Instance keep the play bar; Settings/Accounts
    // hide it -- no "instance to play" on either, and the extra chrome would
    // just crowd two already form-heavy pages. Home needs it too: its own
    // "Jump back in" cards deliberately use a secondary Play, and the dock's
    // Play is what keeps that page down to one accent-filled control.
    function dockVisibleFor(page) {
        return page === "home" || page === "library" || page === "discover" || page === "instance"
    }

    function instanceGroups() {
        var groups = root.shell && root.shell.groups ? root.shell.groups : []
        return [""].concat(groups.filter(g => g.length > 0))
    }

    function call(name, arg) {
        if (root.shell && typeof root.shell[name] === "function")
            arg === undefined ? root.shell[name]() : root.shell[name](arg)
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: topBar.focusSearch()
    }
    // A page's link to Settings > Java (InstanceJavaSettings.qml), sent
    // through the JavaStore singleton the Java rows already share.
    Connections {
        target: JavaStore
        function onNavigateRequested(target) {
            root.navigate(target)
        }
    }

    // The game's console, asked for by the launch flow (ShowConsole, or a
    // crash): the instance page, on its Log tab.
    Connections {
        target: root.shell
        ignoreUnknownSignals: true
        function onOpenInstanceLog(id) {
            root.openInstance(id)
            instancePage.tab = "log"
        }
        function onImportRequested(source) {
            root.openImport(source)
        }
        // From C++ (QmlShell::requestNavigate()): a launch that needs an
        // account opens the Accounts page, for one.
        function onNavigateRequested(target) {
            root.navigate(target)
        }
    }

    Binding {
        target: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
        property: "instanceId"
        value: root.openedInstanceId.length > 0 ? root.openedInstanceId : "/"
        when: !!root.shell && !!root.shell.instancePageModel
    }

    // The play bar's instance: the selection if there is one, otherwise the
    // most recently played instance, otherwise just the first one -- the
    // same fallback the library's old hero card used. heroModel is free for
    // this now that the hero card itself is gone (see LibraryPage.qml).
    readonly property string dockInstanceId: root.selectedId.length > 0 ? root.selectedId
            : (root.shell && root.shell.recentModel && root.shell.recentModel.firstId ? root.shell.recentModel.firstId
            : (root.instanceModel && root.instanceModel.firstId ? root.instanceModel.firstId : ""))

    Binding {
        target: root.shell && root.shell.heroModel ? root.shell.heroModel : null
        property: "instanceId"
        value: root.dockInstanceId
        when: !!root.shell && !!root.shell.heroModel
    }

    // Instance management, shared by the library's card menu and the
    // instance page's own ⋯ menu.
    function askRename(id, name) {
        renameDialog.targetId = id
        renameDialog.value = name
        renameDialog.open()
    }
    function askIcon(id, iconKey) {
        iconDialog.targetId = id
        iconDialog.current = iconKey
        iconDialog.open()
    }
    function askGroup(id, group) {
        groupDialog.targetId = id
        groupDialog.value = group
        groupDialog.suggestions = root.instanceGroups()
        groupDialog.open()
    }
    function askDelete(id, name) {
        deleteDialog.targetId = id
        deleteDialog.text = qsTr("Delete \u201c%1\u201d? Its folder \u2014 worlds, mods, screenshots \u2014 is removed for good.").arg(name)
        deleteDialog.open()
    }

    Shortcut {
        sequences: [StandardKey.New]
        onActivated: root.openNewInstance()
    }

    // A zip, an .mrpack or a link dropped anywhere on the window opens
    // import with it, as the widget main window's drop did. First, so it
    // sits under everything else: NewInstanceDialog's own drop area, and
    // the library's group drop areas (which take only their own drag),
    // keep their drops.
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (!drop.hasUrls || drop.urls.length === 0)
                return
            root.openImport(drop.urls[0].toString())
            drop.accept()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SidebarNav {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            // An icon rail below this saves real width for the content
            // pages on the launcher's own minimum-width window, rather than
            // squeezing the library grid down to one column behind it. ORed
            // with the persisted manual toggle (see SidebarNav's own
            // collapseToggle), so a small window still forces the rail
            // regardless of what was last chosen.
            // Settings brings its own section list, so the app sidebar folds
            // to its icon rail there instead of standing beside it.
            collapsed: root.width < 1000 || root.page === "settings" || SettingsStore.bool("UiSidebarCollapsed")
            items: [
                { id: "home", icon: "home", label: qsTr("Home") },
                { id: "library", icon: "library", label: qsTr("Library") },
                { id: "discover", icon: "compass", label: qsTr("Discover") }
            ]
            footerItems: [
                { id: "settings", icon: "settings", label: qsTr("Settings") }
            ]
            currentId: root.page === "instance" ? "library" : root.page === "accounts" ? "" : root.page
            barBeside: pageHost.dockVisible
            recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
            onItemActivated: (id) => root.page = id
            onRecentActivated: (id) => {
                root.page = "library"
                root.selectedId = id
            }
            onRecentPlayRequested: (id) => root.launch(id)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            TopBar {
                id: topBar
                Layout.fillWidth: true
                // The instance page has its own banner and way back.
                visible: root.page !== "instance"
                title: root.page === "home" ? qsTr("Home")
                     : root.page === "settings" ? qsTr("Settings")
                     : root.page === "discover" ? qsTr("Discover")
                     : root.page === "accounts" ? qsTr("Accounts") : qsTr("Library")
                count: root.page === "library" && root.instanceModel && root.instanceModel.count !== undefined
                       ? root.instanceModel.count : -1
                searchVisible: root.page === "library"
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
                accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
                accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
                accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                onOpenAccountsRequested: root.page = "accounts"

                ComboBox {
                    id: sortCombo
                    visible: root.page === "library"
                    implicitWidth: 152
                    model: [
                        { value: "Name", label: qsTr("Name") },
                        { value: "LastLaunch", label: qsTr("Last played") },
                        { value: "TotalTimePlayed", label: qsTr("Time played") }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    // InstSortMode is the same launcher-wide setting the
                    // classic Settings page's "Sort instances by" choice
                    // writes -- QmlShell already re-sorts every instance
                    // proxy when it changes, so this needs no plumbing of
                    // its own beyond reading and writing it. Follows the
                    // setting rather than reading it once: it is still empty
                    // when this control completes (root's own
                    // Component.onCompleted points SettingsStore at the live
                    // adapter afterwards), and Settings > General changes it
                    // too.
                    readonly property string stored: SettingsStore.string("InstSortMode") || "Name"
                    function sync() { currentIndex = Math.max(0, indexOfValue(stored)) }
                    onStoredChanged: sync()
                    Component.onCompleted: sync()
                    onActivated: SettingsStore.setValue("InstSortMode", currentValue)

                    Accessible.name: qsTr("Sort instances by")
                }

                // Grid or list: two icon buttons, the current view lit.
                ViewModeToggle {
                    visible: root.page === "library"
                    current: libraryPage.viewMode
                    onActivated: (value) => libraryPage.viewMode = value
                }

                Button {
                    visible: root.page === "library"
                    text: qsTr("New instance")
                    icon.source: Icons.url("plus")
                    onClicked: root.openNewInstance()
                }
            }

            // A page change fades the outgoing page out, swaps
            // StackLayout's currentIndex at the (invisible) midpoint, then
            // fades the incoming one back in with a small upward slide --
            // StackLayout still flips each page's own `visible` at exactly
            // that swap instant, same as a plain binding would, so
            // DiscoverPage's visible-based lazy search is untouched and
            // Layout sizing still comes from the StackLayout underneath.
            Item {
                id: pageHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                readonly property var pageOrder: ["home", "library", "settings", "discover", "instance", "accounts"]
                // Whether the play bar shows for the page currently on
                // screen -- updated at the same invisible midpoint as
                // pageStack's own currentIndex (see pageTransition below),
                // never straight from root.page, so the bar never appears or
                // disappears while the old page is still visibly fading.
                property bool dockVisible: root.dockVisibleFor(root.page)

                transform: Translate { id: pageSlide }

                SequentialAnimation {
                    id: pageTransition
                    property int nextIndex: 0
                    NumberAnimation { target: pageHost; property: "opacity"; to: 0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                    PropertyAction { target: pageStack; property: "currentIndex"; value: pageTransition.nextIndex }
                    PropertyAction { target: pageHost; property: "dockVisible"; value: root.dockVisibleFor(pageHost.pageOrder[pageTransition.nextIndex]) }
                    ParallelAnimation {
                        NumberAnimation { target: pageHost; property: "opacity"; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                        NumberAnimation { target: pageSlide; property: "y"; from: Theme.space.sm; to: 0; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                    }
                }

                Connections {
                    target: root
                    function onPageChanged() {
                        pageTransition.nextIndex = pageHost.pageOrder.indexOf(root.page)
                        pageTransition.restart()
                    }
                }

                StackLayout {
                    id: pageStack
                    anchors.fill: parent
                    Component.onCompleted: currentIndex = pageHost.pageOrder.indexOf(root.page)

                    HomePage {
                        id: homePage
                        recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
                        instanceModel: root.instanceModel
                        recentWorldsModel: root.shell && root.shell.recentWorlds ? root.shell.recentWorlds : null
                        news: root.shell && root.shell.news ? root.shell.news : null
                        accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""

                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onOpenInstanceRequested: (id) => root.openInstance(id)
                        onOpenInstanceWorldsRequested: (id) => {
                            root.openInstance(id)
                            instancePage.tab = "worlds"
                        }
                        onCreateRequested: root.openNewInstance()
                        onDiscoverRequested: root.page = "discover"
                        onLibraryRequested: root.page = "library"
                    }

                    LibraryPage {
                        id: libraryPage
                        focus: true
                        instanceModel: root.instanceModel
                        sectionModelFor: function (group) {
                            return root.shell && typeof root.shell.sectionModel === "function"
                                    ? root.shell.sectionModel(group) : null
                        }
                        searchText: topBar.searchText
                        selectedId: root.selectedId
                        // Groups, undo delete and Duplicate with its own
                        // dialog (LibraryController).
                        library: root.shell && root.shell.library ? root.shell.library : null

                        onSelectRequested: (id) => root.selectedId = id
                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onEditRequested: (id) => root.openInstance(id)
                        onFolderRequested: (id) => root.call("openInstanceFolder", id)
                        onCreateRequested: root.openNewInstance()
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        onClearSearchRequested: topBar.searchText = ""
                        exportControllerFor: function (id) {
                            return root.shell && typeof root.shell.exportController === "function"
                                   ? root.shell.exportController(id) : null
                        }
                        shortcutControllerFor: function (id) {
                            return root.shell && typeof root.shell.shortcutController === "function"
                                   ? root.shell.shortcutController(id) : null
                        }
                        // Play options (LaunchOptionsMenu.qml) in the ⋯ menu.
                        launchOptionsFor: function (id) { return root.launchOptions(id) }
                        onLaunchModeRequested: (id, mode) => root.launchMode(id, mode)
                        onLaunchAsRequested: (id, accountId) => root.launchAs(id, accountId)
                        onProfilerChosen: (id, profilerKey) => root.setProfiler(id, profilerKey)
                        onNavigateRequested: (target) => root.navigate(target)
                    }

                    SettingsPage {
                        id: settingsPage
                        languages: root.shell && root.shell.languages ? root.shell.languages : null
                        selectLanguage: function (key) { root.shell.selectLanguage(key) }
                        pluginSurfaces: root.shell && typeof root.shell.pluginSurfaces === "function"
                                        ? root.shell.pluginSurfaces(0, "") : null
                        systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                        // Whether to show the Cat group at all -- see
                        // root.catAvailable's own comment.
                        catAvailable: root.catAvailable
                        about: root.shell && root.shell.about ? root.shell.about : null
                        pluginList: root.shell && typeof root.shell.pluginList === "function"
                                    ? root.shell.pluginList() : null
                        launcherLogs: root.shell && root.shell.launcherLogs ? root.shell.launcherLogs : null
                        onOpenPathRequested: (path) => root.call("openPath", path)
                    }

                    DiscoverPage {
                        id: discoverPage
                        model: root.shell && root.shell.modpackModel ? root.shell.modpackModel : null
                        curseForgeModel: root.shell && root.shell.curseForgeModpackModel ? root.shell.curseForgeModpackModel : null
                        ftbModel: root.shell && root.shell.ftbModpackModel ? root.shell.ftbModpackModel : null
                        atlModel: root.shell && root.shell.atlModpackModel ? root.shell.atlModpackModel : null
                        technicModel: root.shell && root.shell.technicModpackModel ? root.shell.technicModpackModel : null
                        installer: function (projectId, versionId, name, group) {
                            return root.shell.installModpack(projectId, versionId, name, group)
                        }
                        onShowInstanceRequested: (id) => {
                            root.page = "library"
                            if (id.length > 0)
                                root.selectedId = id
                        }
                        onOtherPlatformsRequested: root.openNewInstance("import")
                    }

                    InstancePage {
                        id: instancePage
                        headerModel: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
                        details: root.openedDetails
                        systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                        accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
                        accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
                        accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
                        accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                        onOpenAccountsRequested: root.page = "accounts"
                        onBackRequested: root.page = "library"
                        // The header's ⋯ menu manages the instance with the
                        // same dialogs as the library's own card menu.
                        manageable: true
                        library: root.shell && root.shell.library ? root.shell.library : null
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        // Two arguments -- root.call() only ever forwards
                        // one -- so this goes straight to the shell instead.
                        onJoinServerRequested: (id, address) => { if (root.shell) root.shell.joinServer(id, address) }
                        pluginSurfacesFor: function (anchor, instanceId) {
                            return root.shell && typeof root.shell.pluginSurfaces === "function"
                                   ? root.shell.pluginSurfaces(anchor, instanceId) : null
                        }
                        onOpenPathRequested: (path) => root.call("openPath", path)
                        exportControllerFor: function (id) {
                            return root.shell && typeof root.shell.exportController === "function"
                                   ? root.shell.exportController(id) : null
                        }
                        shortcutControllerFor: function (id) {
                            return root.shell && typeof root.shell.shortcutController === "function"
                                   ? root.shell.shortcutController(id) : null
                        }
                        // Play options (LaunchOptionsMenu.qml) in the ⋯ menu.
                        launchOptionsFor: function (id) { return root.launchOptions(id) }
                        onLaunchModeRequested: (id, mode) => root.launchMode(id, mode)
                        onLaunchAsRequested: (id, accountId) => root.launchAs(id, accountId)
                        onProfilerChosen: (id, profilerKey) => root.setProfiler(id, profilerKey)
                        onNavigateRequested: (target) => root.navigate(target)
                    }

                    AccountsPage {
                        id: accountsPage
                        controller: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                        // The 3D skin preview needs the cat's Quick3D module.
                        catAvailable: root.catAvailable
                    }
                }

                // The roaming 3D cat (launcher/qml/Cat/CatOverlay.qml), over
                // whichever page is showing. `active` only turns true when
                // the module actually exists (catAvailable, MESHMC_HAS_CAT)
                // and the setting is on, so a build without Qt Quick3D never
                // even resolves the qrc path below -- see the CMake option
                // MeshMC_ENABLE_CAT this all hangs off. Filling pageHost
                // rather than the whole window is what keeps its own
                // walkArea (CatOverlay.qml's default, unset here) right of
                // the sidebar, below the header and above the play bar
                // without this file needing to know any of their sizes.
                Loader {
                    id: catLoader
                    anchors.fill: parent
                    active: root.catAvailable && SettingsStore.bool("CatEnabled")
                    source: active ? "qrc:/qt/qml/MeshMC/Cat/CatOverlay.qml" : ""
                }

                // The still cat picture (Settings > Appearance > Cat picture).
                // Never together with the 3D cat: an old or hand-edited
                // config may have both on, and they share the same corner.
                Loader {
                    anchors.fill: parent
                    active: SettingsStore.bool("TheCat") && !catLoader.active
                    source: active ? "qrc:/qt/qml/MeshMC/Components/CatPicture.qml" : ""
                }
            }

            // A genuine row below the page area, not a floating overlay: the
            // StackLayout above shrinks by exactly this bar's height
            // whenever it is visible, so it can never cover a page's
            // content -- including Discover's, whose QML this change does
            // not own. visible follows pageHost.dockVisible rather than
            // root.page directly, so it appears/disappears at the same
            // invisible mid-fade instant the page itself swaps at, instead
            // of jumping the layout while the outgoing page is still
            // visible.
            PlayDock {
                id: playDock
                Layout.fillWidth: true
                visible: pageHost.dockVisible
                rowModel: root.shell && root.shell.heroModel ? root.shell.heroModel : null
                instanceModel: root.instanceModel
                recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
                selectedId: root.selectedId
                onSelectRequested: (id) => root.selectedId = id
                onLaunchRequested: (id) => root.launch(id)
                onStopRequested: (id) => root.call("killInstance", id)
                onCancelRequested: (id) => root.call("killInstance", id)
                launchOptionsFor: function (id) { return root.launchOptions(id) }
                onLaunchModeRequested: (id, mode) => root.launchMode(id, mode)
                onLaunchAsRequested: (id, accountId) => root.launchAs(id, accountId)
                onProfilerChosen: (id, profilerKey) => root.setProfiler(id, profilerKey)
                onNavigateRequested: (target) => root.navigate(target)
            }
        }
    }

    NewInstanceDialog {
        id: newInstanceDialog
        iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
        onCreated: root.page = "library"
    }

    // Instance management, from the library's menu.
    PromptDialog {
        id: renameDialog
        property string targetId
        title: qsTr("Rename instance")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.shell.renameInstance(targetId, text))
                close()
            else
                error = qsTr("That name cannot be used.")
        }
    }

    PromptDialog {
        id: groupDialog
        property string targetId
        title: qsTr("Move to group")
        label: qsTr("Type a new group or pick an existing one.")
        placeholder: qsTr("No group")
        confirmText: qsTr("Move")
        allowEmpty: true
        onSubmitted: (text) => {
            root.shell.setInstanceGroup(targetId, text)
            close()
        }
    }

    ConfirmDialog {
        id: deleteDialog
        property string targetId
        title: qsTr("Delete instance")
        confirmText: qsTr("Delete instance")
        onConfirmed: {
            if (root.shell.deleteInstance(targetId)) {
                if (root.selectedId === targetId)
                    root.selectedId = ""
                // Deleted from its own page: there is no page left to show.
                if (root.page === "instance" && root.openedInstanceId === targetId)
                    root.page = "library"
                // Undo is offered only when it went to the trash; a
                // platform without one deleted it for good.
                var library = root.shell.library
                if (library && library.canUndoDelete) {
                    var trashedName = library.lastDeletedName
                    toast.show(qsTr("Instance deleted."), "success", qsTr("Undo"), function () {
                        if (library.undoDelete())
                            toast.show(qsTr("“%1” is back in your library.").arg(trashedName), "success")
                        else
                            toast.show(qsTr("“%1” could not be put back. It is still in the trash.").arg(trashedName), "danger")
                    })
                } else {
                    toast.show(qsTr("Instance deleted."), "success")
                }
            } else {
                toast.show(qsTr("The instance could not be deleted. Is it running?"), "danger")
            }
        }
    }

    IconPickerDialog {
        id: iconDialog
        property string targetId
        iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
        onPicked: (key) => root.shell.setInstanceIcon(targetId, key)
        onOpenFolderRequested: root.call("openPath", root.shell.iconsDir || "icons")
    }

    Toast {
        id: toast
    }

    // Questions the core asks while it works, and its "please wait".
    UiRequestDialog {
        id: uiRequests
        host: root.shell && root.shell.uiHost ? root.shell.uiHost : null
        // Until this exists, the core keeps asking through the classic
        // dialogs rather than waiting on a question nobody can see.
        Component.onCompleted: if (host) host.setPresenterReady(true)
        Component.onDestruction: if (host) host.setPresenterReady(false)
    }

    BusyOverlay {
        busy: root.shell && root.shell.uiHost ? root.shell.uiHost.busy : false
        text: root.shell && root.shell.uiHost ? root.shell.uiHost.busyText : ""
        skipLabel: root.shell && root.shell.uiHost && root.shell.uiHost.busySkipLabel !== undefined
                   ? root.shell.uiHost.busySkipLabel : ""
        onSkipRequested: if (root.shell && root.shell.uiHost) root.shell.uiHost.skipBusy()
    }

    // First run: language, Java, account -- over everything else.
    OnboardingView {
        id: onboarding
        shell: root.shell
        onSignInRequested: {
            root.page = "accounts"
            accountsPage.startMicrosoftLogin()
        }
    }

    Loader {
        id: galleryLoader
        anchors.fill: parent
        z: 100
        active: false
        sourceComponent: Gallery {}
    }
}
