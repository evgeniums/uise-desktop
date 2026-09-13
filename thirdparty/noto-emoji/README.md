# Noto Emoji (subset)

50 emoji SVGs vendored from [googlefonts/noto-emoji](https://github.com/googlefonts/noto-emoji),
used as the default embedded reaction icon pack (`ChatReactionPack` alias context, see
`resources/style/chatreactions.json`).

* Source: https://github.com/googlefonts/noto-emoji
* Commit: `8998f5dd683424a73e2314a8c1f1e359c19e8742`
* Path in upstream repo: `svg/emoji_u<codepoint>.svg`
* License: **Apache License, Version 2.0** — see `LICENSE` in this directory, copied verbatim from
  the upstream `svg/LICENSE` (the repository-root `LICENSE` is SIL OFL 1.1 and applies to the font
  binaries under `fonts/`, not to these SVGs).

Filenames are kept as the upstream `emoji_u<codepoint>.svg` form for license/provenance
traceability. Semantic icon ids used by `DefaultReactionIconPack` (`thumbsup`, `heart`, `joy`, ...)
are mapped to these files via the icon alias layer in `resources/style/chatreactions.json`, not by
renaming the files.

Multicolor art: these SVGs must **not** be passed through the `currentColor` recoloring path (see
`resources/style/light/chatreactions.json` / `dark/chatreactions.json` — the `ChatReactionPack`
context deliberately declares no `modes` block).

## Codepoints

### 7 basic (quick-bar)

| File | Codepoint | Semantic id | Emoji |
|---|---|---|---|
| emoji_u1f44d.svg | U+1F44D | thumbsup | 👍 |
| emoji_u2764.svg | U+2764 | heart | ❤️ |
| emoji_u1f602.svg | U+1F602 | joy | 😂 |
| emoji_u1f62e.svg | U+1F62E | open-mouth | 😮 |
| emoji_u1f622.svg | U+1F622 | cry | 😢 |
| emoji_u1f64f.svg | U+1F64F | pray | 🙏 |
| emoji_u1f525.svg | U+1F525 | fire | 🔥 |

### 43 common

| File | Codepoint | Semantic id |
|---|---|---|
| emoji_u1f600.svg | U+1F600 | grinning |
| emoji_u1f603.svg | U+1F603 | smile-open |
| emoji_u1f604.svg | U+1F604 | smile-eyes |
| emoji_u1f601.svg | U+1F601 | grin |
| emoji_u1f605.svg | U+1F605 | sweat-smile |
| emoji_u1f923.svg | U+1F923 | rofl |
| emoji_u1f60a.svg | U+1F60A | blush |
| emoji_u1f642.svg | U+1F642 | slight-smile |
| emoji_u1f609.svg | U+1F609 | wink |
| emoji_u1f60d.svg | U+1F60D | heart-eyes |
| emoji_u1f618.svg | U+1F618 | blow-kiss |
| emoji_u1f61c.svg | U+1F61C | wink-tongue |
| emoji_u1f917.svg | U+1F917 | hug |
| emoji_u1f914.svg | U+1F914 | thinking |
| emoji_u1f610.svg | U+1F610 | neutral |
| emoji_u1f634.svg | U+1F634 | sleeping |
| emoji_u1f60e.svg | U+1F60E | sunglasses |
| emoji_u1f973.svg | U+1F973 | party-face |
| emoji_u1f62d.svg | U+1F62D | sob |
| emoji_u1f631.svg | U+1F631 | scream |
| emoji_u1f621.svg | U+1F621 | rage |
| emoji_u1f92f.svg | U+1F92F | mind-blown |
| emoji_u1f44e.svg | U+1F44E | thumbsdown |
| emoji_u1f44f.svg | U+1F44F | clap |
| emoji_u1f64c.svg | U+1F64C | raised-hands |
| emoji_u1f44c.svg | U+1F44C | ok-hand |
| emoji_u270c.svg | U+270C | victory |
| emoji_u1f91d.svg | U+1F91D | handshake |
| emoji_u1f4aa.svg | U+1F4AA | muscle |
| emoji_u1f389.svg | U+1F389 | tada |
| emoji_u1f382.svg | U+1F382 | birthday |
| emoji_u1f381.svg | U+1F381 | gift |
| emoji_u1f4af.svg | U+1F4AF | hundred |
| emoji_u2705.svg | U+2705 | check |
| emoji_u274c.svg | U+274C | cross |
| emoji_u2b50.svg | U+2B50 | star |
| emoji_u1f494.svg | U+1F494 | broken-heart |
| emoji_u1f608.svg | U+1F608 | smiling-imp |
| emoji_u1f440.svg | U+1F440 | eyes |
| emoji_u1f921.svg | U+1F921 | clown |
| emoji_u1f4a9.svg | U+1F4A9 | poop |
| emoji_u1f680.svg | U+1F680 | rocket |
| emoji_u1f60f.svg | U+1F60F | smirk |
