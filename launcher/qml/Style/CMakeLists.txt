# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## MeshMC.Style: the launcher's Qt Quick Controls style ########

# A qmldir'd QML module, same shape as launcher/qml/CMakeLists.txt's
# MeshMC_qml: one CMakeLists per module directory, RESOURCE_PREFIX spelled out
# by hand rather than via qt_policy(QTP0001) because that policy needs Qt
# 6.5+ and this project's floor is 6.4.
#
# This module is never linked into MeshMC_qml or added to the app's style
# selection here -- both are the integrator's job, done once every style/theme
# module in the launcher exists. Building it as its own static library keeps
# it that way: nothing in this file can accidentally wire itself in.
qt_add_library(MeshMC_qml_style STATIC)

qt_add_qml_module(MeshMC_qml_style
    URI MeshMC.Style
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    # QtQuick.Controls.Basic is the fallback: any control this style does not
    # define (DialogButtonBox, RoundButton, Drawer...) resolves to Basic's
    # instead of failing to load. Without it the style is only safe for the
    # exact set of controls it happens to ship today.
    IMPORTS
        MeshMC.Theme
        QtQuick.Controls.Basic
    DEPENDENCIES
        QtQuick.Templates
    QML_FILES
        BusyIndicator.qml
        Button.qml
        CheckBox.qml
        CheckMark.qml
        Chevron.qml
        ComboBox.qml
        Dialog.qml
        FocusRing.qml
        Frame.qml
        ItemDelegate.qml
        Label.qml
        Menu.qml
        MenuItem.qml
        MenuSeparator.qml
        Pane.qml
        Popup.qml
        ProgressBar.qml
        RadioButton.qml
        ScrollBar.qml
        ScrollIndicator.qml
        Slider.qml
        SpinBox.qml
        Switch.qml
        TabBar.qml
        TabButton.qml
        TextArea.qml
        TextField.qml
        ToolButton.qml
        ToolTip.qml
)

# Style QML files reach into QtQuick.Templates (T.Button etc.) and
# QtQuick.Controls.impl (IconLabel/IconImage/ColorImage) directly; the
# corresponding C++ modules still need to be linked for those imports to
# resolve at runtime. QuickControls2 additionally pulls in the styling
# infrastructure that makes a custom style like this one selectable at all.
target_link_libraries(MeshMC_qml_style PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickTemplates2
)
