# Emoji shortcode/category data

Inputs to `scripts/gen-emoji-pack.py`, vendored here so a generator run needs no network access
beyond fetching the (deliberately NOT vendored, ~200MB) Noto tarball -- see that script's own doc
comment.

- **`gemoji-emoji.json`** -- [github/gemoji](https://github.com/github/gemoji)'s `db/emoji.json`.
  Supplies each emoji's canonical `:shortcode:` and aliases, its English description, and its CLDR
  category (Smileys & Emotion, People & Body, ...). MIT license
  ([upstream LICENSE](https://github.com/github/gemoji/blob/master/LICENSE)).
- **`unicode-emoji-data.txt`** -- Unicode's `emoji-data.txt`
  (`https://unicode.org/Public/16.0.0/ucd/emoji/emoji-data.txt`). Supplies the
  `Emoji_Presentation` property, used to compute `ReactionIconInfo::emojiText`'s VARIATION
  SELECTOR-16 requirement per icon (see `needsVariationSelector` in
  `../../src/emojiicontable.inc`'s generated rows). Unicode, Inc. data files license
  ([terms](https://www.unicode.org/license.txt)).

Neither file is hand-edited. Re-fetch and re-run the generator to pick up an upstream update.
