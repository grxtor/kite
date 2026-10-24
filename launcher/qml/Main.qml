// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme
import Kite.Components

ApplicationWindow {
    id: root

    required property var instanceModel
    required property var selection
    // QmlShell: actions (launchInstance, killInstance, ...), account summary
    // and the extra instance models the library needs.
    required property var shell

    readonly property string moduleName: "Kite"

    width: 1240
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    title: "Kite"
    color: Theme.palette.canvas

    // Whether launcher/qml/Skin3D was built at all (KITE_HAS_SKIN3D): the
    // QML shell has no other way to tell a Quick3D-less build apart, and
    // SkinPreview needs it to fall back to the flat view instead of
    // loading a module that is not there. Set from
    // QmlShell::rootProperties(); false is the right default for
    // QmlModule_test, which never sets it.
    property bool skin3DAvailable: false

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

    Binding {
        target: LauncherStore
        property: "iconRevision"
        value: root.shell ? root.shell.iconRevision : 0
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
            LauncherStore.icons = root.shell
            var mode = SettingsStore.string("UiThemeMode")
            if (mode === "dark" || mode === "light" || mode === "system")
                Theme.mode = mode
            var scheme = SettingsStore.string("UiPalette")
            if (scheme === "amethyst" || scheme === "ember" || scheme === "diamond" || scheme === "grass")
                Theme.scheme = scheme
        }
        onboardingLoader.active = !!root.shell && !!root.shell.setupSteps && root.shell.setupSteps.length > 0
        if (root.devRoute.length > 0)
            Qt.callLater(root.applyDevRoute)
    }

    // Startup route for review snapshots (KITE_QML_ROUTE): ";"-separated
    // steps such as "theme=light;scheme=ember;size=1400x900;page=settings;section=java",
    // "instance=<id>;tab=mods", "page=discover;detail=0", "newinstance",
    // "gallery" or "page=home;crashed=<id>" (marks that instance as having
    // crashed last time, for the Home page's chip -- see QmlShell's
    // debugMarkInstanceCrashed()). "nav=<path>" goes through navigate()
    // below ("nav=settings/java", "nav=instance/fabric/settings"), and
    // "import=<path or url>" acts like `kite --import` (onImportRequested).
    // "page=accounts;accounts-demo" opens the skin editor on demo data
    // ("accounts-demo=cape" / "=elytra" start on the player's back); that
    // step is read by AccountsController, not here.
    property string devRoute: ""
    function applyDevRoute() {
        // Routes reach into pages the route may not show; build them all.
        for (var slot of [libraryLoader, settingsLoader, discoverLoader, instanceLoader, accountsLoader])
            slot.built = true
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
            // The New instance dialog's mod loader, picked (after
            // newinstance): "loader=fabric".
            else if (key === "loader")
                root.shell.newInstance.loader = value
            else if (key === "nav")
                root.navigate(value)
            else if (key === "import")
                root.openImport(value)
            else if (key === "gallery")
                galleryLoader.active = true
            else if (key === "profilemenu")
                Qt.callLater(() => sidebar.openProfileMenu())
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
            // The Content tab's Add-ons and Kite Performance dialogs (after
            // instance=<id>).
            else if (key === "bundles")
                Qt.callLater(instancePage.review, "bundles")
            else if (key === "perfdialog")
                Qt.callLater(instancePage.review, "perfdialog")
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
            // The first-run flow over the shell with every step listed, on
            // a data dir that no longer needs it; nothing is saved unless a
            // step is finished.
            else if (key === "onboarding") {
                onboardingLoader.active = true
                onboardingLoader.item.steps = ["language", "java", "account", "done"]
                onboardingLoader.item.index = 0
                onboardingLoader.item.active = true
            }
            else if (key === "crashed")
                root.call("debugMarkInstanceCrashed", value)
            // A real click at window point "x,y[,right[,delayMs]]" -- see
            // QmlShell::debugClick().
            else if (key === "click")
                root.call("debugClick", value)
            // The icon picker, as the library's "Change icon" opens it.
            else if (key === "iconpicker") {
                var iconDialog = root.dialog(iconLoader)
                iconDialog.targetId = root.dockInstanceId
                iconDialog.current = value.length > 0 ? value : "grass"
                iconDialog.open()
            }
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

    // The dialog held by `loader`, built now if it is not yet.
    function dialog(loader) {
        loader.active = true
        return loader.item
    }
    // Null until first opened.
    readonly property QtObject newInstanceDialog: newInstanceLoader.item

    // mode is "create" (default) or "import" -- see NewInstanceDialog.qml.
    // source pre-fills import mode's file/URL field; empty otherwise.
    function openNewInstance(mode, source) {
        if (!root.shell || !root.shell.newInstance)
            return
        var instanceDialog = root.dialog(newInstanceLoader)
        instanceDialog.controller = root.shell.newInstance
        instanceDialog.mode = mode === "import" ? "import" : "create"
        instanceDialog.presetSource = source || ""
        instanceDialog.open()
    }

    // An import asked for from outside the window: `kite --import`, or a
    // second launcher passing a file on (QmlShell::requestImport()). With
    // the dialog already open it takes the new source in place, unless it
    // is still busy with the last one.
    function openImport(source) {
        if (!newInstanceDialog || !newInstanceDialog.opened) {
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
            root.page = "settings"
            if (parts.length > 1 && parts[1].length > 0)
                settingsPage.section = parts[1]
            return true
        case "instance":
            if (parts.length < 2 || parts[1].length === 0 || !root.shell
                    || typeof root.shell.instanceDetails !== "function"
                    || !root.shell.instanceDetails(parts[1]))
                return false
            root.openInstance(parts[1])
            if (parts.length > 2 && parts[2].length > 0 && instancePage)
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
    // instance's profiler. All of them need an account.
    function launchMode(id, mode) {
        if (root.sendToAccountsIfNone())
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
    function setPerformance(id, enabled) {
        if (root.shell && typeof root.shell.setPerformanceEnabled === "function")
            root.shell.setPerformanceEnabled(id, enabled)
    }
    // What a LaunchOptionsMenu offers for instance @p id; a page hands this
    // to its menu as `optionsFor` (see QmlShell::launchOptions()).
    function launchOptions(id) {
        return root.shell && typeof root.shell.launchOptions === "function"
                ? root.shell.launchOptions(id) : ({})
    }

    // The play bar shows on Home, Discover, Settings and Accounts, keeping
    // the selected instance one click away. Home has it too: the hero scrolls
    // away under Continue playing and the library, the bar stays. The
    // instance page's header is that page's Play, and the bar would only
    // repeat it there; the library page that once had it is no longer in the
    // rail.
    function dockVisibleFor(page) {
        return page === "home" || page === "discover" || page === "settings" || page === "accounts"
    }
    // Inside an instance, while Content shows its "Add content" browser,
    // that page's Install is the primary action. Moot while the instance
    // page has no bar at all, kept so bringing the bar back there keeps it.
    readonly property bool dockSuppressed: root.page === "instance" && !!instancePage && instancePage.browsing === true

    // The pages built on demand (PageSlot): null until first shown. Setting
    // root.page builds the page at once, so code that sends the user
    // somewhere and then hands the page something does it in that order.
    readonly property Item libraryPage: libraryLoader.item
    readonly property Item settingsPage: settingsLoader.item
    readonly property Item discoverPage: discoverLoader.item
    readonly property Item instancePage: instanceLoader.item
    readonly property Item accountsPage: accountsLoader.item

    function instanceGroups() {
        var groups = root.shell && root.shell.groups ? root.shell.groups : []
        return [""].concat(groups.filter(g => g.length > 0))
    }

    function call(name, arg) {
        if (root.shell && typeof root.shell[name] === "function")
            arg === undefined ? root.shell[name]() : root.shell[name](arg)
    }

    // Ctrl+F searches the library, which lives on Home; from another page
    // it goes there first and the field takes focus once Home is showing
    // (pageTransition). An instance page keeps its own Ctrl+F (LogTab).
    property bool focusHomeSearch: false
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.page !== "instance"
        onActivated: {
            if (root.page === "home") {
                homePage.focusSearch()
            } else {
                root.focusHomeSearch = true
                root.page = "home"
            }
        }
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
            if (instancePage)
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
        var renameDialog = root.dialog(renameLoader)
        renameDialog.targetId = id
        renameDialog.value = name
        renameDialog.open()
    }
    function askIcon(id, iconKey) {
        var iconDialog = root.dialog(iconLoader)
        iconDialog.targetId = id
        iconDialog.current = iconKey
        iconDialog.open()
    }
    function askGroup(id, group) {
        var groupDialog = root.dialog(groupLoader)
        groupDialog.targetId = id
        groupDialog.value = group
        groupDialog.suggestions = root.instanceGroups()
        groupDialog.open()
    }
    function askDelete(id, name) {
        var deleteDialog = root.dialog(deleteLoader)
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

    /*
     * The shell, Modrinth's frame: the sidebar, the top bar and -- on the
     * pages that have one -- the play bar are one surface (the brief's glass:
     * a hair lighter than the canvas, with a 1 px light line along its top),
     * and the page sits in an inset panel on the canvas, its left corners
     * rounded and a hairline round it, like a sheet laid into the frame.
     *
     * The panel is the page area, and everything inside it is the page's
     * own: the panel adds no padding, so a page pads itself (its content
     * scrolls up to the panel's top edge) and keeps its side margins as
     * before. Only the Library has a strip of the shell's inside the panel,
     * its toolbar (search, sort, view, New instance), above the page.
     */
    // Opaque, so the panel's corner matte can match it exactly: nothing but
    // the canvas is ever behind the shell, so glass over the canvas and this
    // tint of it are the same colour.
    readonly property color shellColor: Qt.tint(Theme.palette.canvas, Theme.glass.fill)
    // The panel's corner: Modrinth's "inset window" corner, the dialog's
    // radius.xl -- the one surface as big as a dialog.
    readonly property int panelRadius: Theme.radius.xl
    // The top bar: Modrinth's 48, one rail square tall -- the same line the
    // sidebar's brand band sits on.
    readonly property int shellBarHeight: Theme.control.heightLg

    Rectangle {
        anchors.fill: parent
        color: root.shellColor

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.glass.highlight
        }
    }

    // The rail's destinations. Home and Library become one page (designs/
    // phase3-scale.md, decision 1), so the rail has no Library of its own;
    // page=library still routes, and lights Home, until that lands.
    readonly property var navItems: [
        { id: "home", icon: "home", label: qsTr("Home") },
        { id: "discover", icon: "compass", label: qsTr("Discover") }
    ]

    SidebarNav {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        bandHeight: root.shellBarHeight
        surround: root.shellColor
        // The icon rail (Modrinth's) unless the labelled sidebar was asked
        // for -- the persisted toggle (SidebarNav's own CollapseToggle), on
        // every page. A narrow window keeps the rail regardless, rather than
        // squeezing the pages behind a labelled sidebar, and then offers no
        // toggle that could not act.
        canExpand: root.width >= 1000
        collapsed: !canExpand || SettingsStore.bool("UiSidebarCollapsed")
        items: root.navItems
        footerItems: [
            { id: "settings", icon: "settings", label: qsTr("Settings") }
        ]
        // A page the rail does not list lights the place it is reached from:
        // Home for the library and an instance (the breadcrumb's root too),
        // unless that instance's own tile is in the rail (currentInstanceId).
        currentId: root.navItems.some(item => item.id === root.page)
                   || root.page === "settings" || root.page === "accounts" ? root.page : "home"
        currentInstanceId: root.page === "instance" ? root.openedInstanceId : ""
        barBeside: playDock.visible
        recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
        onItemActivated: (id) => root.page = id
        // A recent instance opens its own page, as a library card does.
        onRecentActivated: (id) => root.openInstance(id)
        onRecentPlayRequested: (id) => root.launch(id)
        onNewInstanceRequested: root.openNewInstance()

        accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
        accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
        accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
        accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
        onOpenAccountsRequested: root.page = "accounts"
    }

    // The open instance's name, for the top bar's trail: read out of the
    // instance page's own one-row model, the way PlayDock reads its row.
    property string openedName: ""
    Repeater {
        model: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
        delegate: Item {
            required property string name
            visible: false
            onNameChanged: root.openedName = name
            Component.onCompleted: root.openedName = name
        }
    }

    // On every page, the instance page too: where you are. Inside an
    // instance the title is a trail back up -- Home, the instance, and "Add
    // content" while that is open.
    TopBar {
        id: topBar
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.shellBarHeight
        searchVisible: false
        title: root.page === "home" ? qsTr("Home")
             : root.page === "settings" ? qsTr("Settings")
             : root.page === "discover" ? qsTr("Discover")
             : root.page === "accounts" ? qsTr("Accounts")
             : root.page === "instance" ? (instancePage && instancePage.browsing ? qsTr("Add content") : root.openedName)
             : qsTr("Library")
        iconName: root.page === "settings" ? "settings"
                : root.page === "discover" ? "compass"
                : root.page === "accounts" ? "user"
                : root.page === "library" ? "library"
                : "home"
        trail: root.page !== "instance" ? []
             : instancePage && instancePage.browsing ? [qsTr("Home"), root.openedName] : [qsTr("Home")]
        onTrailActivated: (index) => {
            if (index === 0)
                root.page = "home"
            else
                instancePage.leaveBrowse()
        }
        count: root.page === "library" && root.instanceModel && root.instanceModel.count !== undefined
               ? root.instanceModel.count : -1
        // The library title row also carries the total play time.
        showPlayTime: root.page === "library"
    }

    // The page area. Clipped, and its rounded rectangle runs on past the
    // right edge -- and past the bottom while no play bar sits under it --
    // so only the corners against the shell round off.
    Item {
        id: panel
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: playDock.visible ? playDock.top : parent.bottom
        clip: true

        readonly property int overhangBottom: playDock.visible ? 0 : root.panelRadius

        Rectangle {
            id: panelFill
            width: parent.width + root.panelRadius
            height: parent.height + panel.overhangBottom
            radius: root.panelRadius
            color: Theme.palette.canvas
        }

        ColumnLayout {
            anchors.fill: parent
            // Inside the hairline, which the matte below redraws over the
            // page; nothing on the right (the window edge) or at the bottom
            // without a play bar.
            anchors.leftMargin: 1
            anchors.topMargin: 1
            anchors.bottomMargin: panel.overhangBottom > 0 ? 0 : 1
            spacing: 0

            // The library's toolbar: the search field gives up its width
            // first under pressure -- sort/view/New instance keep their own
            // size, since shrinking a combo box or a labelled button reads
            // as broken where a narrower search field still reads as a
            // search field.
            PageToolbar {
                id: libraryToolbar
                Layout.fillWidth: true
                // The panel adds no padding, so this does its own; the page
                // under it pads its own top as every page does.
                Layout.topMargin: Theme.space.xl
                visible: root.page === "library"
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                ComboBox {
                    id: sortCombo
                    implicitWidth: 152
                    model: SettingsStore.instanceSortModes
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
                    current: libraryPage ? libraryPage.viewMode : "grid"
                    onActivated: (value) => { if (libraryPage) libraryPage.viewMode = value }
                }

                Button {
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
                    ScriptAction {
                        script: {
                            if (root.focusHomeSearch && root.page === "home")
                                homePage.focusSearch()
                            root.focusHomeSearch = false
                        }
                    }
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
                        // The hero follows the dock's rule: the selection,
                        // else the instance played last, else the first.
                        heroModel: root.shell && root.shell.heroModel ? root.shell.heroModel : null
                        sectionModelFor: function (group) {
                            return root.shell && typeof root.shell.sectionModel === "function"
                                    ? root.shell.sectionModel(group) : null
                        }
                        selectedId: root.selectedId
                        library: root.shell && root.shell.library ? root.shell.library : null
                        launchOptionsFor: function (id) { return root.launchOptions(id) }
                        exportControllerFor: function (id) {
                            return root.shell && typeof root.shell.exportController === "function"
                                   ? root.shell.exportController(id) : null
                        }
                        shortcutControllerFor: function (id) {
                            return root.shell && typeof root.shell.shortcutController === "function"
                                   ? root.shell.shortcutController(id) : null
                        }
                        msaSignedIn: !!root.shell && root.shell.accountKind === "Microsoft"

                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onOpenInstanceRequested: (id) => root.openInstance(id)
                        onSelectRequested: (id) => root.selectedId = id
                        onSignInRequested: root.page = "accounts"
                        onFolderRequested: (id) => root.call("openInstanceFolder", id)
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        onLaunchModeRequested: (id, mode) => root.launchMode(id, mode)
                        onLaunchAsRequested: (id, accountId) => root.launchAs(id, accountId)
                        onProfilerChosen: (id, profilerKey) => root.setProfiler(id, profilerKey)
                        onPerformanceChosen: (id, enabled) => root.setPerformance(id, enabled)
                        onNavigateRequested: (target) => root.navigate(target)
                        onOpenInstanceWorldsRequested: (id) => {
                            root.openInstance(id)
                            instancePage.tab = "worlds"
                        }
                        onCreateRequested: root.openNewInstance()
                        onDiscoverRequested: root.page = "discover"
                    }

                    PageSlot {
                        id: libraryLoader
                        page: "library"
                        // A Loader is a focus scope: the page's own focus
                        // only counts with this set too.
                        focus: true
                        current: root.page
                        sourceComponent: LibraryPage {
                                focus: true
                                instanceModel: root.instanceModel
                                sectionModelFor: function (group) {
                                    return root.shell && typeof root.shell.sectionModel === "function"
                                            ? root.shell.sectionModel(group) : null
                                }
                                searchText: libraryToolbar.searchText
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
                                onClearSearchRequested: libraryToolbar.searchText = ""
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
                                onPerformanceChosen: (id, enabled) => root.setPerformance(id, enabled)
                                onNavigateRequested: (target) => root.navigate(target)
                        }
                    }

                    PageSlot {
                        id: settingsLoader
                        page: "settings"
                        current: root.page
                        sourceComponent: SettingsPage {
                                languages: root.shell && root.shell.languages ? root.shell.languages : null
                                selectLanguage: function (key) { root.shell.selectLanguage(key) }
                                pluginSurfaces: root.shell && typeof root.shell.pluginSurfaces === "function"
                                                ? root.shell.pluginSurfaces(0, "") : null
                                systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                                about: root.shell && root.shell.about ? root.shell.about : null
                                pluginList: root.shell && typeof root.shell.pluginList === "function"
                                            ? root.shell.pluginList() : null
                                launcherLogs: root.shell && root.shell.launcherLogs ? root.shell.launcherLogs : null
                                onOpenPathRequested: (path) => root.call("openPath", path)
                        }
                    }

                    PageSlot {
                        id: discoverLoader
                        page: "discover"
                        current: root.page
                        sourceComponent: DiscoverPage {
                                shown: discoverLoader.StackLayout.isCurrentItem
                                model: root.shell && root.shell.modpackModel ? root.shell.modpackModel : null
                                curseForgeModel: root.shell && root.shell.curseForgeModpackModel ? root.shell.curseForgeModpackModel : null
                                ftbModel: root.shell && root.shell.ftbModpackModel ? root.shell.ftbModpackModel : null
                                atlModel: root.shell && root.shell.atlModpackModel ? root.shell.atlModpackModel : null
                                technicModel: root.shell && root.shell.technicModpackModel ? root.shell.technicModpackModel : null
                                installer: function (projectId, versionId, name, group) {
                                    return root.shell.installModpack(projectId, versionId, name, group)
                                }
                                onShowInstanceRequested: (id) => {
                                    root.page = "home"
                                    if (id.length > 0)
                                        root.selectedId = id
                                }
                                onOtherPlatformsRequested: root.openNewInstance("import")
                        }
                    }

                    PageSlot {
                        id: instanceLoader
                        page: "instance"
                        current: root.page
                        sourceComponent: InstancePage {
                                headerModel: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
                                details: root.openedDetails
                                shell: root.shell
                                systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                                accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
                                accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
                                accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
                                accountsController: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                                onOpenAccountsRequested: root.page = "accounts"
                                // Back to where the breadcrumb's root points (Home).
                                onBackRequested: root.page = "home"
                                onOpenInstanceRequested: (id) => root.openInstance(id)
                                // The header's own Play and Stop: the one Play on
                                // this page, the play bar stays away (dockVisibleFor).
                                onPlayRequested: (id) => root.launch(id)
                                onStopRequested: (id) => root.call("killInstance", id)
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
                                onPerformanceChosen: (id, enabled) => root.setPerformance(id, enabled)
                                onNavigateRequested: (target) => root.navigate(target)
                        }
                    }

                    PageSlot {
                        id: accountsLoader
                        page: "accounts"
                        current: root.page
                        sourceComponent: AccountsPage {
                                controller: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                                // The 3D skin preview needs the Skin3D module.
                                skin3DAvailable: root.skin3DAvailable
                        }
                    }
                }

                // The still cat picture (Settings > Appearance > Cat picture).
                Loader {
                    anchors.fill: parent
                    active: SettingsStore.bool("TheCat")
                    source: active ? "qrc:/qt/qml/Kite/Components/CatPicture.qml" : ""
                }
            }
        }

        // Over the page: the shell's colour outside the rounded corners, so
        // nothing a page draws edge to edge (a scroll fade, a band) squares
        // them off, and the hairline on top of that.
        Item {
            width: panelFill.width
            height: panelFill.height

            CornerMatte {
                matte: root.shellColor
                cornerRadius: root.panelRadius
                outline: Theme.palette.border
            }
        }
    }

    // Part of the frame, under the panel, not a floating overlay: the page
    // area above shrinks by exactly this bar's height whenever it is
    // visible, so it can never cover a page's content -- including
    // Discover's, whose QML this change does not own. visible follows
    // pageHost.dockVisible rather than root.page directly, so it
    // appears/disappears at the same invisible mid-fade instant the page
    // itself swaps at, instead of jumping the layout while the outgoing
    // page is still visible.
    PlayDock {
        id: playDock
        surround: root.shellColor
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: implicitHeight
        visible: pageHost.dockVisible && !root.dockSuppressed
        rowModel: root.shell && root.shell.heroModel ? root.shell.heroModel : null
        instanceModel: root.instanceModel
        recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
        selectedId: root.selectedId
        onSelectRequested: (id) => root.selectedId = id
        onCreateRequested: root.openNewInstance()
        onLaunchRequested: (id) => root.launch(id)
        onStopRequested: (id) => root.call("killInstance", id)
        onCancelRequested: (id) => root.call("killInstance", id)
        launchOptionsFor: function (id) { return root.launchOptions(id) }
        onLaunchModeRequested: (id, mode) => root.launchMode(id, mode)
        onLaunchAsRequested: (id, accountId) => root.launchAs(id, accountId)
        onProfilerChosen: (id, profilerKey) => root.setProfiler(id, profilerKey)
        onPerformanceChosen: (id, enabled) => root.setPerformance(id, enabled)
        onNavigateRequested: (target) => root.navigate(target)
    }

    // The dialogs below are built the first time one is asked for
    // (root.dialog()), not at start: a closed Dialog still builds its whole
    // body.
    Loader {
        id: newInstanceLoader
        active: false
        sourceComponent: NewInstanceDialog {
            iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
            shell: root.shell
            onCreated: root.page = "home"
        }
    }

    // Instance management, from the library's menu.
    Loader {
        id: renameLoader
        active: false
        sourceComponent: PromptDialog {
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
    }

    Loader {
        id: groupLoader
        active: false
        sourceComponent: PromptDialog {
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
    }

    Loader {
        id: deleteLoader
        active: false
        sourceComponent: ConfirmDialog {
            property string targetId
            title: qsTr("Delete instance")
            confirmText: qsTr("Delete instance")
            onConfirmed: {
                if (root.shell.deleteInstance(targetId)) {
                    if (root.selectedId === targetId)
                        root.selectedId = ""
                    // Deleted from its own page: there is no page left to show.
                    if (root.page === "instance" && root.openedInstanceId === targetId)
                        root.page = "home"
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
                    toast.show(qsTr("The instance could not be deleted. Is it running, or are its files in use?"), "danger")
                }
            }
        }
    }

    Loader {
        id: iconLoader
        active: false
        sourceComponent: IconPickerDialog {
            property string targetId
            iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
            onPicked: (key) => root.shell.setInstanceIcon(targetId, key)
            onOpenFolderRequested: root.call("openPath", root.shell.iconsDir || "icons")
        }
    }

    Toast {
        id: toast
        // Over the page, clear of the play bar when there is one.
        anchors.bottomMargin: (playDock.visible ? playDock.height : 0) + Theme.space.xl
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
    // Only built when there is a first-run step to show (and for the
    // "onboarding" review route). Over everything else, as the view's own
    // z only orders it inside this Loader.
    Loader {
        id: onboardingLoader
        active: false
        anchors.fill: parent
        z: 800
        sourceComponent: OnboardingView {
            shell: root.shell
            onSignInRequested: {
                root.page = "accounts"
                accountsPage.startMicrosoftLogin()
            }
            onCreateInstanceRequested: root.openNewInstance()
            onDiscoverRequested: root.page = "discover"
            Component.onCompleted: start()
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
