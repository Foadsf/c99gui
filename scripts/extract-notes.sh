#!/usr/bin/env bash
# extract-notes.sh - Extract release notes from CHANGELOG.md for a given version
# Usage: extract-notes.sh VERSION OUTFILE
# Strips a leading 'v' from version; exits 1 with message if section missing or empty

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 VERSION OUTFILE" >&2
    exit 1
fi

VERSION="$1"
OUTFILE="$2"

# Strip leading 'v' if present
VERSION="${VERSION#v}"

HEADING="## [${VERSION}]"
if ! grep -qF "${HEADING}" CHANGELOG.md 2>/dev/null; then
    HEADING="## [v${VERSION}]"
    if ! grep -qF "${HEADING}" CHANGELOG.md 2>/dev/null; then
        echo "ERROR: No changelog section found for version ${VERSION}" >&2
        exit 1
    fi
fi

# Extract from the heading to the next ## [ (exact fixed-string)
awk -v h="${HEADING}" '
    index($0, h) == 1 { found=1; next }
    found && index($0, "## [") == 1 { exit }
    found { print }
' CHANGELOG.md > "${OUTFILE}"

# Fail if notes are empty
if [ ! -s "${OUTFILE}" ]; then
    echo "ERROR: Extracted release notes are empty for version ${VERSION}" >&2
    exit 1
fi