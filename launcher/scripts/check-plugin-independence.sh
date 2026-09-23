#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0
#
# check-plugin-independence.sh
#
# Guard rail for the MMCO ABI 3 promise: every in-tree `.mmco` plugin
# builds without dragging the launcher's own binary onto its link line.
# The script runs two complementary checks:
#
#   1. Source-level grep over plugins/   — forbids re-introducing the
#      back-doors that ABI 3 closed (APPLICATION->, Application::*,
#      direct #include "Application.h" / "BaseInstance.h" / …, BasePage
#      subclassing through the launcher tree, etc.).
#
#   2. Link-command grep over the build directory — confirms that the
#      `.mmco` Ninja edges never reference meshmc.exe / meshmc.lib /
#      MeshMC_logic.lib. Skipped if the build directory hasn't been
#      configured with Ninja yet (the CI invokes the script after the
#      regular build, so this is always available there).
#
# Exit codes:
#   0  — independence holds.
#   1  — at least one forbidden reference was found.
#   2  — invocation / environment error (e.g. running outside the repo).
#
# Usage:
#   check-plugin-independence.sh                 # auto-locate build dir
#   check-plugin-independence.sh build/          # explicit build dir
#   check-plugin-independence.sh --source-only   # skip link-line check
#
# The script is **read-only**: it never modifies source, build, or
# generated files. Safe to wire into pre-commit, CI, and ad-hoc local
# runs alike.
#
# NOTE: this script fully compatible to corebinutils

set -euo pipefail

PROG="$(basename "$0")"
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PLUGINS_DIR="$REPO_ROOT/launcher/plugin/plugins"

#-----------------------------------------------------------------------
# Argument parsing.
#-----------------------------------------------------------------------
SOURCE_ONLY=0
BUILD_DIR=""

usage() {
	cat <<EOF
$PROG — verify MMCO ABI 3 plugin independence.

Usage:
  $PROG [--source-only] [<build-dir>]

Options:
  --source-only   Run only the source-level grep; skip the build-edge
                  check. Useful as a pre-commit hook where no build
                  has been produced yet.
  <build-dir>     Path to the Ninja build directory. If omitted, the
                  script tries ./build, ./build-debug, ./build-release
                  in that order and uses the first one that exists.
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help) usage; exit 0 ;;
		--source-only) SOURCE_ONLY=1; shift ;;
		--) shift; break ;;
		-*) echo "$PROG: unknown option: $1" >&2; usage >&2; exit 2 ;;
		*)
			if [[ -n "$BUILD_DIR" ]]; then
				echo "$PROG: too many positional arguments" >&2
				exit 2
			fi
			BUILD_DIR="$1"
			shift
			;;
	esac
done

if [[ ! -d "$PLUGINS_DIR" ]]; then
	echo "$PROG: launcher/plugin/plugins not found under $REPO_ROOT" >&2
	exit 2
fi

#-----------------------------------------------------------------------
# Helpers.
#-----------------------------------------------------------------------
have_rg=0
if command -v rg >/dev/null 2>&1; then
	have_rg=1
fi

# grep_in_plugins <pattern>
#
# Runs the search against $PLUGINS_DIR while excluding:
#   - vendor/ subtrees (Discord IPC, gamemode header, …) which are
#     allowed to reference whatever they want.
#   - JSON rule packs (ErrorOracle ships text payloads containing the
#     literal "Application" word in advice strings — not code).
#   - Lines that obviously belong to a comment.  A pattern that only
#     appears inside a `//` line comment or a `* …` block-comment
#     continuation is a documentation hit, not a code one.  We strip
#     such lines with an awk filter so prose explaining why the old
#     idiom is forbidden does not fail the gate.
#
# Comment detection is intentionally conservative: a single-line
# `// foo()` match is filtered, and a line that starts with optional
# whitespace + `*` is filtered (block-comment continuation).  A line
# that contains `/* … */` *with code on the same line* is NOT filtered
# — that is exactly the case the gate should catch.  This handles the
# real-world false positives we see today (every match in the current
# codebase is inside a comment block explaining the ABI migration)
# without weakening the rule for actual code.
#
# The function does not exit on match; the caller aggregates results.
grep_in_plugins() {
	local pattern="$1"
	local raw
	if [[ $have_rg -eq 1 ]]; then
		raw="$(rg \
			--line-number \
			--with-filename \
			--no-heading \
			--color=never \
			--glob '!vendor/**' \
			--glob '!**/vendor/**' \
			--glob '!**/*.json' \
			-e "$pattern" \
			"$PLUGINS_DIR" 2>/dev/null || true)"
	else
		raw="$(grep \
			-rn \
			--include='*.c' \
			--include='*.cpp' \
			--include='*.cc' \
			--include='*.cxx' \
			--include='*.h' \
			--include='*.hpp' \
			--exclude-dir=vendor \
			-e "$pattern" \
			"$PLUGINS_DIR" 2>/dev/null || true)"
	fi
	if [[ -z "$raw" ]]; then
		return 0
	fi
	# Strip lines whose content portion (everything after the first
	# two colons emitted by grep / rg as "<file>:<line>:") is a pure
	# comment line.
	printf '%s\n' "$raw" | awk -F: '
	{
		# Reconstruct the matched line by joining fields 3..NF with ":"
		# so a colon inside the source text (string literal, ::, etc.)
		# is preserved.
		line = $3
		for (i = 4; i <= NF; i++) line = line ":" $i

		# Trim leading whitespace (tabs + spaces) to inspect the first
		# non-blank character.
		t = line
		sub(/^[ \t]+/, "", t)

		# `// …` line comments.
		if (t ~ /^\/\//) next

		# Block-comment continuation: `* …` or `*/` or ` */`.
		if (t ~ /^\*/) next

		# Block-comment opener with no code before it: `/* …` or just
		# `/**`.  We do not skip lines where `/*` is preceded by code,
		# because that is `foo(); /* explanation */` which the gate
		# legitimately wants to inspect.
		if (t ~ /^\/\*/) next

		print
	}'
}

#-----------------------------------------------------------------------
# Source-level check.
#-----------------------------------------------------------------------
#
# The forbidden idioms come from the ABI 3 contract: every reach into
# launcher state must go through ctx->… on MMCOContext. Anything that
# would resolve a launcher-private symbol at link time is fair game.
#
# Note: matches inside line comments ARE flagged. The check is
# deliberately strict — a line comment explaining how the old code
# used to call APPLICATION->settings() will trip this rail, and that
# is intended. Move the explanation into a block comment or a markdown
# doc instead of leaving a string the search can mistake for code.
SRC_FAIL=0
SRC_LOG="$(mktemp -t mmco-indep-src.XXXXXX)"
trap 'rm -f "$SRC_LOG"' EXIT

declare -a FORBIDDEN_PATTERNS=(
	# Application singleton back-doors.
	'APPLICATION->'
	'\bApplication::'
	# Header back-doors that pull in launcher private types.
	'#include "Application\.h"'
	'#include "BaseInstance\.h"'
	'#include "InstanceList\.h"'
	'#include "MMCZip\.h"'
	'#include "icons/IconList\.h"'
	'#include "icons/MMCIcon\.h"'
	'#include "minecraft/'
	'#include "settings/SettingsObject\.h"'
	'#include "tasks/SequentialTask\.h"'
	'#include "ui/dialogs/CustomMessageBox\.h"'
	'#include "ui/dialogs/ProgressDialog\.h"'
	'#include "ui/pages/instance/InstanceSettingsPage\.h"'
	'#include "ui/pages/BasePage\.h"'
	'#include "ui/pages/BasePageContainer\.h"'
	'#include "net/Mode\.h"'
)

for pat in "${FORBIDDEN_PATTERNS[@]}"; do
	matches="$(grep_in_plugins "$pat")"
	if [[ -n "$matches" ]]; then
		SRC_FAIL=1
		{
			echo "── forbidden: $pat ──"
			echo "$matches"
			echo
		} >>"$SRC_LOG"
	fi
done

if [[ $SRC_FAIL -ne 0 ]]; then
	echo "$PROG: source-level check FAILED — forbidden launcher reach-ins:" >&2
	echo >&2
	cat "$SRC_LOG" >&2
	echo "Replace the offending call with the matching MMCOContext entry." >&2
	echo "See documentation/handbook/meshmc/plugins/sdk-guide/abi-3-migration.md" >&2
	exit 1
fi

#-----------------------------------------------------------------------
# Link-edge check.
#-----------------------------------------------------------------------
if [[ $SOURCE_ONLY -ne 0 ]]; then
	echo "$PROG: source-level OK (link-edge check skipped: --source-only)"
	exit 0
fi

if [[ -z "$BUILD_DIR" ]]; then
	for candidate in build build-debug build-release out/build; do
		if [[ -d "$REPO_ROOT/$candidate" ]]; then
			BUILD_DIR="$REPO_ROOT/$candidate"
			break
		fi
	done
fi

if [[ -z "$BUILD_DIR" || ! -d "$BUILD_DIR" ]]; then
	echo "$PROG: source-level OK; no build directory found, link-edge check skipped."
	echo "Pass a build directory or run cmake first to enable the second pass."
	exit 0
fi

if ! command -v ninja >/dev/null 2>&1; then
	echo "$PROG: source-level OK; ninja(1) not on PATH, link-edge check skipped."
	echo "Install ninja to enable the second pass."
	exit 0
fi

if [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
	echo "$PROG: source-level OK; $BUILD_DIR is not a Ninja build, link-edge check skipped."
	exit 0
fi

#
# Enumerate every `.mmco` Ninja target and inspect its link command.
# We accept the absolute path the build emits, but normalise the search
# to file names so directory layouts on Windows / macOS / Linux all
# work.
#
LINK_FAIL=0
LINK_LOG="$(mktemp -t mmco-indep-link.XXXXXX)"
trap 'rm -f "$SRC_LOG" "$LINK_LOG"' EXIT

# Forbidden artefacts we never want to see on a plugin's link line.
declare -a FORBIDDEN_LINK_TOKENS=(
	'meshmc.lib'
	'meshmc.exe'
	'MeshMC_logic.lib'
	'MeshMC_logic.a'
	'libMeshMC_logic'
)

# `ninja -t targets all` is the most portable way to list every target;
# we then filter to those ending in `.mmco` (or `.mmco.dll` on Windows
# where CMake appends the platform suffix).
# A while-read loop, not mapfile: macOS runners ship bash 3.2, which has no
# mapfile.
MMCO_TARGETS=()
while IFS= read -r target; do
	[[ -n "$target" ]] && MMCO_TARGETS+=("$target")
done < <(
	ninja -C "$BUILD_DIR" -t targets all 2>/dev/null \
		| awk -F: '{print $1}' \
		| grep -E '\.mmco$|\.mmco\.dll$|\.mmco\.so$|\.mmco\.dylib$' \
		|| true
)

if [[ ${#MMCO_TARGETS[@]} -eq 0 ]]; then
	echo "$PROG: source-level OK; no .mmco Ninja targets in $BUILD_DIR — nothing to verify."
	exit 0
fi

for target in "${MMCO_TARGETS[@]}"; do
	cmds="$(ninja -C "$BUILD_DIR" -t commands "$target" 2>/dev/null || true)"
	if [[ -z "$cmds" ]]; then
		continue
	fi
	for tok in "${FORBIDDEN_LINK_TOKENS[@]}"; do
		hits="$(printf '%s\n' "$cmds" | grep -F -- "$tok" || true)"
		if [[ -n "$hits" ]]; then
			LINK_FAIL=1
			{
				echo "── $target: forbidden link-line token '$tok' ──"
				echo "$hits"
				echo
			} >>"$LINK_LOG"
		fi
	done
done

if [[ $LINK_FAIL -ne 0 ]]; then
	echo "$PROG: link-edge check FAILED — plugins still link against the launcher:" >&2
	echo >&2
	cat "$LINK_LOG" >&2
	echo "Inspect launcher/plugin/sdk/CMakeLists.txt and the per-plugin" >&2
	echo "CMakeLists for accidentally re-introduced target_link_libraries" >&2
	echo "against meshmc / MeshMC_logic." >&2
	exit 1
fi

echo "$PROG: plugin independence holds — ${#MMCO_TARGETS[@]} .mmco target(s) clean."
exit 0
