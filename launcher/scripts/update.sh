#!/bin/sh
#
# Regenerates launcher/locale/template.pot from the sources in this repository.
#
# The translations used to live in a separate repository, where the launcher
# source tree had to be copied (or symlinked) into ./src first. Now that the
# translations are part of the launcher repository, the sources are scanned
# directly and there is no ./src any more. Crowdin syncs against
# locale/template.pot (see /crowdin.yml).
#
# Requires lupdate and lconvert from Qt (qttools). Override the binaries with
# LUPDATE_BIN / LCONVERT_BIN if they are not on PATH.
#
# The Windows counterpart is update.bat; both are kept behaviourally identical
# (same file list, same sort order, same output).

set -e

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(git -C "$ROOT" rev-parse --show-toplevel)

# Directories scanned for translatable strings, relative to the repository root.
# Override with e.g. SRC_DIRS="launcher" ./update.sh
SRC_DIRS=${SRC_DIRS:-"$REPO/launcher $REPO/launcher/plugin/plugins $REPO/launcher/crashreporter $REPO/launcher/updater"}

# plugins/staging/ is gated behind the MeshMC_STAGING_PLUGINS option (off by
# default), so those strings are not shipped and must not reach translators.
EXCLUDE_RE=${EXCLUDE_RE:-^plugins/staging/}

TEMPLATE_PO="$REPO/launcher/locale/template.pot"
BASE_LST_FILE="$REPO/base_lst_file"

# lupdate writes source locations relative to the .ts file, and lconvert copies
# them into the .pot verbatim (its -locations switch is ignored for non-.ts
# output). Keeping the intermediate .ts at the repository root is what makes the
# locations read "launcher/Foo.cpp:12" instead of "../launcher/Foo.cpp:12".
TEMPLATE_TS="$REPO/template.ts"

LCONVERT_BIN=${LCONVERT_BIN:-lconvert}
LUPDATE_BIN=${LUPDATE_BIN:-lupdate}

cleanup() {
    rm -f "$TEMPLATE_TS"
}
trap cleanup EXIT

cd "$REPO"

echo "Writing lst file..."
# Unquoted on purpose: SRC_DIRS is a whitespace separated list.
# shellcheck disable=SC2086
LC_ALL=C find $SRC_DIRS -type f \
    \( -iname \*.h -o -iname \*.cpp -o -iname \*.ui -o -iname \*.qml \) > "$BASE_LST_FILE.raw"
if [ -n "$EXCLUDE_RE" ]; then
    grep -Ev "$EXCLUDE_RE" "$BASE_LST_FILE.raw" | LC_ALL=C sort > "$BASE_LST_FILE"
else
    LC_ALL=C sort "$BASE_LST_FILE.raw" > "$BASE_LST_FILE"
fi
rm -f "$BASE_LST_FILE.raw"
echo "    $(wc -l < "$BASE_LST_FILE") files found"

echo "Generating new template..."
echo "    Generating .ts"
rm -f "$TEMPLATE_TS"
"$LUPDATE_BIN" "@$BASE_LST_FILE" -ts "$TEMPLATE_TS"

echo "    Converting .ts to .pot"
"$LCONVERT_BIN" "$TEMPLATE_TS" -o "$TEMPLATE_PO"

echo "All done!"
