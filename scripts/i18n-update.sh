#!/usr/bin/env bash
#
# i18n-update.sh -- regenerate uise-desktop's translations/uise_<lang>.ts catalogs
# from the current tr()/QCoreApplication::translate() call sites in include/ and src/.
#
# uise-desktop is a standalone public repo: this script is intentionally minimal
# and self-contained (no dependency on whitemdesktop's richer scripts/i18n.py) so
# the repo stays buildable and translatable on its own. It only ever touches the
# .ts files already present in translations/ -- to add a new language, first
# create an empty translations/uise_<lang>.ts (e.g. copy translations/uise_en.ts
# and clear the <translation> text, or just run lupdate once with -ts pointing at
# the new file) then rerun this script.
#
# Usage:
#   scripts/i18n-update.sh              # update every translations/uise_*.ts
#   scripts/i18n-update.sh ru de        # update only the given languages
#
# Requires `lupdate` on PATH (ships with Qt; on this machine also available at
# /opt/homebrew/opt/qt/bin/lupdate).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TS_DIR="$ROOT_DIR/translations"

LUPDATE="${LUPDATE:-lupdate}"
if ! command -v "$LUPDATE" >/dev/null 2>&1; then
    for candidate in /opt/homebrew/opt/qt/bin/lupdate /opt/homebrew/bin/lupdate; do
        if [[ -x "$candidate" ]]; then
            LUPDATE="$candidate"
            break
        fi
    done
fi
if ! command -v "$LUPDATE" >/dev/null 2>&1; then
    echo "i18n-update: lupdate not found on PATH and no fallback candidate exists." >&2
    echo "             set LUPDATE=/path/to/lupdate or install Qt's linguist tools." >&2
    exit 1
fi

mkdir -p "$TS_DIR"

# Sorted, explicit file list -- NOT bare directories -- so repeated runs are
# byte-identical: lupdate's directory-recursion order is not guaranteed sorted,
# and an unsorted @lst-file would make every rerun reorder unrelated contexts.
LST_FILE="$(mktemp)"
trap 'rm -f "$LST_FILE"' EXIT
find "$ROOT_DIR/include/uise/desktop" "$ROOT_DIR/src" \
    -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.mm' \
               -o -name '*.h' -o -name '*.hpp' -o -name '*.hxx' -o -name '*.ipp' -o -name '*.ui' \) \
    | LC_ALL=C sort > "$LST_FILE"

if [[ "$#" -gt 0 ]]; then
    langs=("$@")
else
    langs=()
    for ts in "$TS_DIR"/uise_*.ts; do
        [[ -e "$ts" ]] || continue
        base="$(basename "$ts" .ts)"
        langs+=("${base#uise_}")
    done
fi

if [[ "${#langs[@]}" -eq 0 ]]; then
    echo "i18n-update: no translations/uise_*.ts found and no languages given on the command line."
    echo "             create translations/uise_<lang>.ts first (see the comment at the top of this script)."
    exit 1
fi

for lang in "${langs[@]}"; do
    ts_file="$TS_DIR/uise_${lang}.ts"
    echo "i18n-update: updating $ts_file"
    "$LUPDATE" "@$LST_FILE" \
        -extensions cpp,cc,cxx,mm,h,hpp,hxx,ipp,ui \
        -locations relative \
        -source-language en \
        -target-language "$lang" \
        -silent \
        -ts "$ts_file"
done

echo "i18n-update: done (${#langs[@]} language(s))."
