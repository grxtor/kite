# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
#
# dmgbuild settings for Kite's disk image: the app on the left, a link to
# Applications on the right, the sky behind them, and Kite's licence shown
# before the image mounts. Paths come in as -D defines:
#
#   dmgbuild -s dmg_settings.py -D app=Kite.app -D background=dmg-background.png \
#            -D icon=Kite.icns -D license=LICENSE "Kite 1.2.0" Kite.dmg
#
# dmgbuild pairs dmg-background.png with dmg-background@2x.png on its own.
import os.path

app = defines["app"]
app_name = os.path.basename(app)

# Window and icon geometry, in points; dmg-background.html draws for these.
# The window's height counts its title bar, 32 pt from macOS 26 and shorter
# before. Asking for the shortest lets the sky's bottom edge slip under the
# window on newer releases instead of leaving a blank strip on older ones.
BACKGROUND_SIZE = (700, 460)
TITLE_BAR = 22
ICON_SIZE = 128
ICON_Y = 300
ICON_X = (190, 510)

format = "ULMO"
files = [app]
symlinks = {"Applications": "/Applications"}
hide_extensions = [app_name]
icon = defines["icon"]
background = defines["background"]

window_rect = ((200, 120), (BACKGROUND_SIZE[0], BACKGROUND_SIZE[1] + TITLE_BAR))
default_view = "icon-view"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
icon_size = ICON_SIZE
text_size = 13
icon_locations = {
    app_name: (ICON_X[0], ICON_Y),
    "Applications": (ICON_X[1], ICON_Y),
}

LICENSE_HEADER = """Kite
Copyright (C) 2026 grxtor

Kite is free software, released under the Apache License 2.0 below.

NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT.

"""

with open(defines["license"], encoding="utf-8") as f:
    license = {
        "default-language": "en_US",
        "licenses": {"en_US": LICENSE_HEADER + f.read()},
    }
