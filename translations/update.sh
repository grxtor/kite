#!/bin/sh
# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Regenerates translations/kite_*.ts from the sources with Qt's lupdate:
# new strings are added as unfinished, existing translations are kept, strings
# that no longer exist are dropped.
#
#   translations/update.sh            update every translations/kite_*.ts
#   translations/update.sh --new de_DE  start translations/kite_de.ts
#
# Sources: everything under launcher/ with the extensions .cpp .h .ui .qml
# .js, except unit tests (*_test.cpp) and anything under a testdata directory.
#
# lupdate options (keep them stable, the diffs of the .ts files depend on it):
#   -locations relative   file names are relative to the .ts file, and line
#                         numbers relative to the previous message
#   -no-obsolete          drop removed messages instead of marking them vanished
#   -target-language L    the locale written to the .ts header; for an existing
#                         file it is read back from its own header
#
# Environment: LUPDATE (path of lupdate, default: the one in PATH),
# TS_DIR (where the .ts files live, default: translations/ next to this
# script; sources are always the ones of this checkout).
#
# The .qm files are not made here; the build compiles the .ts files with
# lrelease and bundles them (launcher/CMakeLists.txt).
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
ts_dir=${TS_DIR:-$root/translations}
lupdate=${LUPDATE:-lupdate}

cd "$root"

list=$(mktemp)
trap 'rm -f "$list"' EXIT
find launcher -type f \
	\( -name '*.cpp' -o -name '*.h' -o -name '*.ui' -o -name '*.qml' -o -name '*.js' \) \
	! -name '*_test.cpp' ! -path '*/testdata/*' | LC_ALL=C sort | sed "s|^|$root/|" > "$list"

update() { # <ts file> <target language>
	"$lupdate" "@$list" -locations relative -no-obsolete \
		-target-language "$2" -ts "$1"
}

if [ "${1:-}" = "--new" ]; then
	[ $# -eq 2 ] || { echo "usage: $0 --new <locale, e.g. de_DE>" >&2; exit 2; }
	update "$ts_dir/kite_${2%%_*}.ts" "$2"
	exit 0
fi

found=0
for ts in "$ts_dir"/kite_*.ts; do
	[ -f "$ts" ] || continue
	found=1
	lang=$(sed -n 's/^<TS .*language="\([^"]*\)".*/\1/p' "$ts" | head -n 1)
	if [ -z "$lang" ]; then
		lang=$(basename "$ts" .ts)
		lang=${lang#kite_}
	fi
	update "$ts" "$lang"
done
[ "$found" -eq 1 ] || { echo "no kite_*.ts in $ts_dir" >&2; exit 1; }
