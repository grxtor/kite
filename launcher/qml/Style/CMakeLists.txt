# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

# Kite.Style, the launcher's Qt Quick Controls style. RESOURCE_PREFIX is
# spelled out because qt_policy(QTP0001) needs Qt 6.5.
qt_add_library(Kite_qml_style STATIC)

qt_add_qml_module(Kite_qml_style
    URI Kite.Style
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    # Basic is the fallback: a control this style does not define
    # (DialogButtonBox, RoundButton, Drawer...) resolves to Basic's instead of
    # failing to load.
    IMPORTS
        Kite.Theme
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
        PopupShadow.qml
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

# The style QML imports QtQuick.Templates and QtQuick.Controls.impl directly,
# so their C++ modules must be linked. QuickControls2 adds the styling
# infrastructure that makes a custom style selectable.
target_link_libraries(Kite_qml_style PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickTemplates2
)
