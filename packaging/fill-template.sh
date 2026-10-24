#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Fills one of the package manager templates next to this script for a
# published release and prints the result.
#
#   fill-template.sh <template> <tag>
#
# @TAG@ becomes the tag (v11.0.0) and @VERSION@ the tag without its "v"
# (11.0.0). @SHA256:<asset>@ becomes the SHA-256 of that release asset, which
# is downloaded to compute it; <asset> may itself contain @TAG@.

set -euo pipefail

template=$1
tag=$2
release="https://github.com/${GITHUB_REPOSITORY:-grxtor/kite}/releases/download/${tag}"

out=$(sed -e "s/@TAG@/${tag}/g" -e "s/@VERSION@/${tag#v}/g" "$template")

for placeholder in $(grep -o '@SHA256:[^@]*@' <<<"$out" | sort -u); do
    asset=${placeholder#@SHA256:}
    asset=${asset%@}
    sum=$(curl -fsSL "${release}/${asset}" | sha256sum | cut -d' ' -f1)
    out=${out//"$placeholder"/$sum}
done

printf '%s\n' "$out"
