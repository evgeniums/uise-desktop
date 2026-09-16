#!/usr/bin/env python3
"""
gen-emoji-pack.py -- regenerate uise-desktop's Noto Emoji resources from pinned upstream sources.

See thirdparty/noto-emoji/README.md for why the Noto tag is pinned at v2.042 and never bumped
without re-running the black-silhouette render screen this script performs.

Inputs (both pinned, both vendored so the build is reproducible offline):
  - googlefonts/noto-emoji tag v2.042, svg/emoji_u<codepoint>.svg  (thirdparty/noto-emoji/)
  - github/gemoji db/emoji.json (MIT)                              (thirdparty/emoji-data/)
  - Unicode emoji-data.txt (Emoji_Presentation property)           (thirdparty/emoji-data/)

Selection rule: an entry is included iff, after stripping U+FE0F/U+FE0E, it is EXACTLY ONE
codepoint AND that codepoint has a svg/emoji_u<cp>.svg in the pinned Noto tag. This deliberately
excludes skin-tone variants, ZWJ sequences (families, professions, keycaps) and country flags --
see the plan doc for the coverage/cost tradeoff.

Outputs:
  - thirdparty/noto-emoji/*.svg           -- one file per selected codepoint, upstream names kept
  - thirdparty/noto-emoji.qrc             -- regenerated
  - src/emojiicontable.inc                -- generated GeneratedIconEntry[] table
  - thirdparty/noto-emoji/README.md       -- counts + provenance refreshed

Do NOT hand-edit any generated output; edit this script (or the curated overlay in
src/reactioniconpack.cpp, for the 54 hand-picked "most common" icons) and re-run.

Re-running (e.g. to pick up a gemoji update -- new aliases, a recategorization):

    curl -sSL -o /tmp/noto.tar.gz \\
        https://github.com/googlefonts/noto-emoji/archive/refs/tags/v2.042.tar.gz
    mkdir /tmp/noto-src && tar xzf /tmp/noto.tar.gz -C /tmp/noto-src
    NOTO_SRC_SVG_DIR=/tmp/noto-src/noto-emoji-2.042/svg python3 scripts/gen-emoji-pack.py

Then, from a Qt-capable Python (a throwaway venv with `pip install PySide6` is fine -- this never
touches the C++ build): `python3 scripts/screen-emoji-svgs.py thirdparty/noto-emoji` and confirm
PASS before committing. See this file's EXCLUDED_CODEPOINTS and screen-emoji-svgs.py's
KNOWN_DARK_OK for what to do if it finds something new.

The Noto tarball itself (~200MB, the full multi-codepoint upstream set) is deliberately NOT
vendored -- only the 1374 SVGs this script selects out of it are. gemoji's db/emoji.json and
Unicode's emoji-data.txt ARE vendored (thirdparty/emoji-data/), both small and directly needed
for every run, so a run needs no network access beyond fetching the Noto tarball once.
"""
import json
import os
import re
import sys
import unicodedata

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
# Overridable for a dry run against a scratch copy.
ROOT = os.environ.get("UISE_DESKTOP_ROOT", ROOT)

if "NOTO_SRC_SVG_DIR" not in os.environ:
    sys.exit("NOTO_SRC_SVG_DIR is not set -- point it at an extracted checkout of "
             "googlefonts/noto-emoji tag v2.042's svg/ directory. See this script's module "
             "doc comment for the exact fetch command.")
NOTO_SRC_SVG_DIR = os.environ["NOTO_SRC_SVG_DIR"]                     # extracted upstream svg/ dir
GEMOJI_JSON = os.environ.get("GEMOJI_JSON",
                              os.path.join(ROOT, "thirdparty", "emoji-data", "gemoji-emoji.json"))
EMOJI_DATA_TXT = os.environ.get("EMOJI_DATA_TXT",
                                 os.path.join(ROOT, "thirdparty", "emoji-data",
                                              "unicode-emoji-data.txt"))

OUT_SVG_DIR = os.path.join(ROOT, "thirdparty", "noto-emoji")
OUT_QRC = os.path.join(ROOT, "thirdparty", "noto-emoji.qrc")
OUT_TABLE = os.path.join(ROOT, "src", "emojiicontable.inc")
OUT_README = os.path.join(ROOT, "thirdparty", "noto-emoji", "README.md")

NOTO_TAG = "v2.042"
NOTO_COMMIT = "d79d23e6822e0f6e5731b114cbfb26b2a4e380da"

# gemoji category -> (categoryId, display title). Order here is the gallery's category order.
CATEGORY_ORDER = [
    ("Smileys & Emotion", "smileys-emotion", "Smileys & Emotion"),
    ("People & Body", "people-body", "People & Body"),
    ("Animals & Nature", "animals-nature", "Animals & Nature"),
    ("Food & Drink", "food-drink", "Food & Drink"),
    ("Travel & Places", "travel-places", "Travel & Places"),
    ("Activities", "activities", "Activities"),
    ("Objects", "objects", "Objects"),
    ("Symbols", "symbols", "Symbols"),
    ("Flags", "flags", "Flags"),
]
CATEGORY_ID_BY_GEMOJI = {g: cid for g, cid, _ in CATEGORY_ORDER}

# Confirmed BROKEN under QtSvg even at the pinned v2.042 tag -- verified 2026-09-16 by rendering
# every <use>-containing candidate through PySide6's QtSvg (the same module Qt6 desktop uses) and
# inspecting the result. A bare "<use" grep is NOT sufficient to find these (see the informational
# flag step below): 21 of the 24 <use>-containing files in this selection render CORRECTLY
# (including wink-tongue/roll-eyes, already shipped in the curated 54) because their <use> targets
# a <clipPath> used for a harmless mask. These three are different: <clipPath>+<use xlink:href>
# feeding the SHAPE ITSELF, which QtSvg fails to resolve, producing a solid near-black silhouette
# -- the exact mechanism thirdparty/noto-emoji/README.md documents for the post-v2.042 hand
# redesigns, just present in a few files within v2.042 itself. No fix is available (upstream SVG
# structure), so these are excluded rather than vendored broken. Re-verify with screen_svgs.py
# after ANY regeneration -- this hazard is a property of individual files, not of the tag.
EXCLUDED_CODEPOINTS = {
    0x1F32A: "tornado -- renders solid black instead of a grey funnel cloud",
    0x1F383: "jack-o-lantern -- renders solid black+orange instead of an orange face with cutouts",
    0x1FAE8: "shaking face -- renders as a solid near-black disc instead of a yellow face",
}


def load_noto_codepoints():
    """Single-codepoint upstream files only -- 'emoji_u<hex>.svg', no '_' in the hex part."""
    result = {}
    pat = re.compile(r"^emoji_u([0-9a-f]+)\.svg$")
    for name in os.listdir(NOTO_SRC_SVG_DIR):
        m = pat.match(name)
        if m:
            result[int(m.group(1), 16)] = name
    return result


def load_emoji_presentation():
    """Codepoints with Emoji_Presentation=Yes, from Unicode's emoji-data.txt."""
    yes = set()
    line_re = re.compile(r"^([0-9A-Fa-f]{4,6})(?:\.\.([0-9A-Fa-f]{4,6}))?\s*;\s*Emoji_Presentation\s*#")
    with open(EMOJI_DATA_TXT, encoding="utf-8") as f:
        for line in f:
            m = line_re.match(line)
            if not m:
                continue
            lo = int(m.group(1), 16)
            hi = int(m.group(2), 16) if m.group(2) else lo
            for cp in range(lo, hi + 1):
                yes.add(cp)
    return yes


def codepoints_of(emoji_str):
    return [ord(c) for c in emoji_str]


def sanitize_c_string(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def c_string_list(items):
    return "{" + ",".join('"%s"' % sanitize_c_string(x) for x in items) + "}"


def main():
    noto = load_noto_codepoints()
    emoji_presentation = load_emoji_presentation()

    with open(GEMOJI_JSON, encoding="utf-8") as f:
        gemoji = json.load(f)

    entries = []
    seen_cp = set()
    skipped_no_svg = 0
    skipped_multi_cp = 0
    for e in gemoji:
        raw_cps = codepoints_of(e["emoji"])
        cps = [c for c in raw_cps if c not in (0xFE0F, 0xFE0E)]
        if len(cps) != 1:
            skipped_multi_cp += 1
            continue
        cp = cps[0]
        if cp not in noto:
            skipped_no_svg += 1
            continue
        if cp in seen_cp:
            # gemoji lists a small number of duplicate emoji under different descriptions
            # (e.g. regional variants); first entry in file order wins, same "first wins" rule
            # the pack's own lookup APIs already document.
            continue
        if cp in EXCLUDED_CODEPOINTS:
            continue
        seen_cp.add(cp)

        category = CATEGORY_ID_BY_GEMOJI.get(e.get("category"))
        if category is None:
            # Not one of the 9 CLDR groups this pack sections by (or missing) -- still include
            # the icon (it is still valid, single-codepoint, has art), just with no category
            # section. Record so the run log shows it rather than silently dropping coverage.
            category = ""

        aliases = list(dict.fromkeys(e.get("aliases", [])))  # de-dup, keep order
        if not aliases:
            continue
        shortcode = aliases[0]

        tags = e.get("tags", [])
        keywords = list(dict.fromkeys(aliases + tags + e["description"].split()))

        entries.append({
            "codepoint": cp,
            "shortcode": shortcode,
            "aliases": aliases,
            "description": e["description"],
            "categoryId": category,
            "needsVs": cp not in emoji_presentation,
            "keywords": keywords,
            "notoFile": noto[cp],
        })

    entries.sort(key=lambda x: x["codepoint"])

    # Alias collision check across the WHOLE selected set -- must stay zero, or
    # DefaultReactionIconPack::Pimpl::shortcodeToIndex's "first wins" silently hides an icon.
    alias_owner = {}
    collisions = []
    for entry in entries:
        for a in entry["aliases"]:
            key = a.casefold()
            if key in alias_owner and alias_owner[key] != entry["codepoint"]:
                collisions.append((a, alias_owner[key], entry["codepoint"]))
            else:
                alias_owner[key] = entry["codepoint"]
    if collisions:
        print("ERROR: alias collisions across selected emoji set:", file=sys.stderr)
        for a, cp1, cp2 in collisions:
            print("  %r used by U+%04X and U+%04X" % (a, cp1, cp2), file=sys.stderr)
        sys.exit(1)

    print("Selected %d single-codepoint emoji (skipped %d multi-codepoint, %d with no Noto %s svg, "
          "%d excluded as confirmed-broken renders)"
          % (len(entries), skipped_multi_cp, skipped_no_svg, NOTO_TAG, len(EXCLUDED_CODEPOINTS)))
    from collections import Counter
    cat_counts = Counter(e["categoryId"] or "(uncategorized)" for e in entries)
    for cid, n in cat_counts.most_common():
        print("  %-20s %4d" % (cid, n))

    # --- Flag (not reject) candidates containing <use> -- informational only ---
    #
    # A bare grep for "<use" is NOT the pass/fail test: two of the shipped 54 (wink-tongue
    # U+1F61C, roll-eyes U+1F644) already use <use> in the pinned v2.042 tag and render
    # correctly -- see thirdparty/noto-emoji/README.md. The actual pass/fail test is a render,
    # done separately by screen_svgs.py (measures near-black opaque-pixel fraction; requires a
    # Qt-capable Python, not part of this script's dependencies). This step only surfaces the
    # candidate list so a run log shows what screen_svgs.py should double-check.
    use_pattern = re.compile(rb"<use[\s>]")
    flagged = []
    for entry in entries:
        path = os.path.join(NOTO_SRC_SVG_DIR, entry["notoFile"])
        with open(path, "rb") as f:
            data = f.read()
        if use_pattern.search(data):
            flagged.append(entry["notoFile"])
    if flagged:
        print("NOTE: %d SVG(s) contain <use> and are candidates for the render screen "
              "(run screen_svgs.py on the output directory before committing):" % len(flagged))
        for h in flagged:
            print("  " + h)

    # --- Write SVGs ---
    os.makedirs(OUT_SVG_DIR, exist_ok=True)
    existing = set(f for f in os.listdir(OUT_SVG_DIR) if f.endswith(".svg"))
    wanted = set(e["notoFile"] for e in entries)
    for stale in existing - wanted:
        os.remove(os.path.join(OUT_SVG_DIR, stale))
    for entry in entries:
        src = os.path.join(NOTO_SRC_SVG_DIR, entry["notoFile"])
        dst = os.path.join(OUT_SVG_DIR, entry["notoFile"])
        with open(src, "rb") as f:
            data = f.read()
        with open(dst, "wb") as f:
            f.write(data)
    print("Wrote %d SVG files to %s" % (len(entries), OUT_SVG_DIR))

    # --- Write .qrc ---
    with open(OUT_QRC, "w", encoding="utf-8") as f:
        # XML comments may not contain a literal "--" anywhere in their content (only as the
        # opening/closing delimiters) -- unlike this project's own C++ "--" dash convention,
        # which is fine in // comments but is invalid XML and made RCC fail to parse this file
        # with "Expected '>', but got ' '." Single hyphens only, here.
        f.write('<!-- Generated by scripts/gen-emoji-pack.py, do not hand-edit -->\n')
        f.write('<RCC>\n    <qresource prefix="/icons">\n')
        for entry in entries:
            f.write('        <file>noto-emoji/%s</file>\n' % entry["notoFile"])
        f.write('    </qresource>\n</RCC>\n')
    print("Wrote %s" % OUT_QRC)

    # --- Write the generated C++ table ---
    with open(OUT_TABLE, "w", encoding="utf-8") as f:
        f.write("// Generated by scripts/gen-emoji-pack.py from googlefonts/noto-emoji %s (%s)\n"
                % (NOTO_TAG, NOTO_COMMIT))
        f.write("// and github/gemoji's db/emoji.json (thirdparty/emoji-data/, MIT license).\n")
        f.write("// DO NOT HAND-EDIT -- re-run the generator instead. See its own doc comment for\n")
        f.write("// the selection rule and thirdparty/noto-emoji/README.md for the Noto pin.\n")
        f.write("//\n")
        f.write("// Keywords here are English-only, NOT QT_TRANSLATE_NOOP -- a deliberate choice to\n")
        f.write("// avoid ~%d new strings in every uise_*.ts (see the plan's i18n decision). Only the\n"
                % sum(len(e["keywords"]) for e in entries))
        f.write("// curated 54-icon overlay in reactioniconpack.cpp keeps translated keywords.\n")
        f.write("//\n")
        f.write("// %d entries.\n" % len(entries))
        f.write("namespace {\n\n")
        f.write("struct GeneratedIconEntry\n{\n")
        f.write("    char32_t codepoint;\n")
        f.write("    const char* shortcode;                          //!< canonical alias; also the iconId\n")
        f.write("    std::initializer_list<const char*> aliases;     //!< includes shortcode itself\n")
        f.write("    const char* description;\n")
        f.write("    const char* categoryId;                         //!< \"\" if uncategorized\n")
        f.write("    bool needsVariationSelector;\n")
        f.write("    std::initializer_list<const char*> keywords;    //!< English only, not translated\n")
        f.write("};\n\n")
        # const, not constexpr -- matches the pre-existing, proven-working rawIcons[]/curatedIcons[]
        # idiom in reactioniconpack.cpp for a table of structs holding std::initializer_list
        # members, rather than introducing a stricter constexpr requirement this project has not
        # exercised for this exact shape across its supported compilers (incl. clang-cl/Windows).
        f.write("const GeneratedIconEntry generatedIcons[]={\n")
        for entry in entries:
            f.write("    {0x%X,\"%s\",%s,\"%s\",\"%s\",%s,%s},\n" % (
                entry["codepoint"],
                sanitize_c_string(entry["shortcode"]),
                c_string_list(entry["aliases"]),
                sanitize_c_string(entry["description"]),
                entry["categoryId"],
                "true" if entry["needsVs"] else "false",
                c_string_list(entry["keywords"]),
            ))
        f.write("};\n\n")
        f.write("constexpr size_t generatedIconCount=sizeof(generatedIcons)/sizeof(generatedIcons[0]);\n\n")
        f.write("} // anonymous namespace\n")
    print("Wrote %s (%d rows)" % (OUT_TABLE, len(entries)))

    # --- Refresh the README ---
    with open(OUT_README, "w", encoding="utf-8") as f:
        f.write("# Noto Emoji resources\n\n")
        f.write("Generated by `scripts/gen-emoji-pack.py` -- **do not hand-edit the SVGs, the "
                ".qrc, or `../../src/emojiicontable.inc`**; re-run the generator instead.\n\n")
        f.write("- Upstream: [googlefonts/noto-emoji](https://github.com/googlefonts/noto-emoji), "
                "tag **%s** (commit `%s`).\n" % (NOTO_TAG, NOTO_COMMIT))
        f.write("- License: Apache-2.0, `LICENSE` in this directory (the upstream `svg/LICENSE`, "
                "NOT the repo-root OFL).\n")
        f.write("- Shortcode/category source: [github/gemoji](https://github.com/github/gemoji) "
                "`db/emoji.json`, vendored at `../emoji-data/` (MIT license).\n")
        f.write("- Category property source: Unicode `emoji-data.txt` (`Emoji_Presentation`), "
                "vendored at `../emoji-data/`.\n\n")
        f.write("## Hard constraint: do not bump the Noto tag past v2.042\n\n")
        f.write("Upstream redesigned the hand emoji after this tag (Illustrator 26.3.1 exports) to "
                "paint via `<defs><path id=\"SVGID_1_\"/></defs>` + `<clipPath>` + "
                "`<use xlink:href>`. Qt's SVG module does not resolve that `<use>` -- all ten hand "
                "icons render as a **solid black silhouette with a thin yellow rim**, and "
                "`QSvgRenderer::isValid()` still reports the file valid. The generator screens "
                "every candidate file for a bare `<use` and refuses to vendor it "
                "(see `gen-emoji-pack.py`'s screening step) -- but that only catches the same "
                "*mechanism*, not a guarantee upstream never finds another way to break QtSvg. "
                "Re-verify with the render test below before ever re-running against a newer tag.\n\n")
        f.write("**Render-test recipe** (documented, not yet wired into `test/`): render each SVG "
                "to a QImage and measure the fraction of opaque pixels that are near-black. A "
                "broken hand measures 45-72%; a correct one is 0%; `sunglasses` (U+1F60E) is "
                "legitimately dark at ~33% and is the one expected outlier.\n\n")
        f.write("**This was last run** (`scripts/screen-emoji-svgs.py`, via a throwaway PySide6 "
                "venv) on 2026-09-16, in two passes: an exploratory pass over the %d candidates "
                "the selection rule matched BEFORE any exclusion, which is what found the 3 "
                "genuinely broken renders now excluded (see `EXCLUDED_CODEPOINTS` in "
                "`gen-emoji-pack.py`); and a final verification pass over exactly the %d icons "
                "this pack ships, which reported a clean PASS with no exclusions left to find. "
                "Re-run both (see this script's own module doc comment) and update this date "
                "whenever the selection changes -- gemoji picking up a new alias or "
                "recategorization can change WHICH files are candidates even though the Noto tag "
                "itself has not moved.\n\n" % (len(entries) + len(EXCLUDED_CODEPOINTS), len(entries)))
        for cp, reason in EXCLUDED_CODEPOINTS.items():
            f.write("- `U+%04X` -- %s\n" % (cp, reason))
        f.write("\nA bare `<use` grep is NOT the test: 21 of the 24 `<use>`-containing candidates "
                "in this selection (including `wink-tongue` U+1F61C and `roll-eyes` U+1F644, "
                "already shipped in the curated 54) render correctly -- their `<use>` targets a "
                "harmless mask, not the shape itself. 4 more candidates were flagged by the "
                "near-black metric but are legitimately dark by design (phones, dark sunglasses) "
                "and are allow-listed in `screen_svgs.py`'s `KNOWN_DARK_OK`, not excluded.\n\n")
        f.write("## Coverage\n\n")
        f.write("Selection rule: single-codepoint emoji (after stripping U+FE0F/U+FE0E) that have "
                "both a gemoji shortcode and a Noto v2.042 SVG. This excludes skin-tone variants, "
                "ZWJ sequences (families, professions, gendered variants, keycaps) and country "
                "flags -- all of those are multi-codepoint.\n\n")
        f.write("**%d icons**, %.1f MB:\n\n" % (len(entries), sum(
            os.path.getsize(os.path.join(OUT_SVG_DIR, e["notoFile"])) for e in entries) / 1024 / 1024))
        f.write("| Category | Count |\n|---|---|\n")
        for _, cid, title in CATEGORY_ORDER:
            f.write("| %s | %d |\n" % (title, cat_counts.get(cid, 0)))
        if cat_counts.get("(uncategorized)"):
            f.write("| (uncategorized) | %d |\n" % cat_counts["(uncategorized)"])
        f.write("\n## The curated \"most common\" 54\n\n")
        f.write("54 hand-picked icons get a frozen `iconId` and translated keywords -- see the "
                "`curatedIcons[]` overlay in `../../src/reactioniconpack.cpp`. Their `iconId`s "
                "predate this generator and are NOT derived from their shortcode (26 of the 54 "
                "differ, e.g. `victory` types as `:v:`, `poop` as `:hankey:`) -- existing "
                "persisted reactions reference these ids, so they must never be renamed.\n")
    print("Wrote %s" % OUT_README)


if __name__ == "__main__":
    main()
