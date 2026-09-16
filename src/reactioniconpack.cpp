/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/reactioniconpack.cpp
*
*  Defines DefaultReactionIconPack and ReactionIconPacks.
*
*/

/****************************************************************************/

#include <algorithm>
#include <map>
#include <set>

#include <QCoreApplication>

#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>

namespace uise {

// Generated table -- DO NOT HAND-EDIT. Regenerate via scripts/gen-emoji-pack.py (see that
// script's own doc comment for inputs/provenance and thirdparty/noto-emoji/README.md for the
// Noto version pin). Opens and closes its own anonymous namespace, providing
// generatedIcons[]/generatedIconCount, keyed by codepoint.
#include "emojiicontable.inc"

namespace {

/**
 * @brief The 54 hand-picked "most common" icons, overlaid onto the generated table by codepoint.
 *
 * Carries only what must stay hand-maintained and cannot come from the generator: a FROZEN
 * iconId (existing persisted reactions reference these -- see reactioniconpack.hpp's own comment
 * on why shortcode is never derived from iconId, since 26 of these 54 differ from their upstream
 * shortcode), the "is one of the 7 basics" marker, and TRANSLATED keywords/description (the
 * generator's own keywords are deliberately English-only -- see emojiicontable.inc's header
 * comment for why). Everything else -- shortcode, aliases, category, needsVariationSelector --
 * comes from the matching generatedIcons[] row, joined by codepoint in Pimpl::build().
 *
 * Joining by codepoint rather than by name is what keeps these ids frozen: the generator's own
 * naming would call U+270C "v" and U+1F4A9 "hankey", silently orphaning any reaction already
 * persisted under "victory" or "poop".
 */
struct CuratedIconEntry
{
    const char* iconId;
    char32_t codepoint;   //!< Joins to a generatedIcons[] row -- see the struct doc comment.
    bool basic;
    const char* description;
    std::initializer_list<const char*> keywords;
};

// clang-format off
const CuratedIconEntry curatedIcons[]={
    // 7 basic (quick-bar) icons -- kept first so DefaultReactionIconPack::basicIconIds() can just
    // take the leading N entries in pack order.
    {"thumbsup",   0x1F44D, true,  QT_TRANSLATE_NOOP("ReactionIconPack","thumbs up"), {QT_TRANSLATE_NOOP("ReactionIconPack","thumbs up"), QT_TRANSLATE_NOOP("ReactionIconPack","like"), QT_TRANSLATE_NOOP("ReactionIconPack","yes")}},
    {"heart",      0x2764,  true,  QT_TRANSLATE_NOOP("ReactionIconPack","red heart"), {QT_TRANSLATE_NOOP("ReactionIconPack","heart"), QT_TRANSLATE_NOOP("ReactionIconPack","love"), QT_TRANSLATE_NOOP("ReactionIconPack","red heart")}},
    {"joy",        0x1F602, true,  QT_TRANSLATE_NOOP("ReactionIconPack","tears of joy"), {QT_TRANSLATE_NOOP("ReactionIconPack","laugh"), QT_TRANSLATE_NOOP("ReactionIconPack","joy"), QT_TRANSLATE_NOOP("ReactionIconPack","tears of joy"), QT_TRANSLATE_NOOP("ReactionIconPack","lol")}},
    {"open-mouth", 0x1F62E, true,  QT_TRANSLATE_NOOP("ReactionIconPack","open mouth"), {QT_TRANSLATE_NOOP("ReactionIconPack","surprised"), QT_TRANSLATE_NOOP("ReactionIconPack","wow"), QT_TRANSLATE_NOOP("ReactionIconPack","open mouth")}},
    {"cry",        0x1F622, true,  QT_TRANSLATE_NOOP("ReactionIconPack","crying"), {QT_TRANSLATE_NOOP("ReactionIconPack","cry"), QT_TRANSLATE_NOOP("ReactionIconPack","sad"), QT_TRANSLATE_NOOP("ReactionIconPack","tears")}},
    {"pray",       0x1F64F, true,  QT_TRANSLATE_NOOP("ReactionIconPack","pray"), {QT_TRANSLATE_NOOP("ReactionIconPack","pray"), QT_TRANSLATE_NOOP("ReactionIconPack","please"), QT_TRANSLATE_NOOP("ReactionIconPack","thanks")}},
    {"fire",       0x1F525, true,  QT_TRANSLATE_NOOP("ReactionIconPack","fire"), {QT_TRANSLATE_NOOP("ReactionIconPack","fire"), QT_TRANSLATE_NOOP("ReactionIconPack","hot"), QT_TRANSLATE_NOOP("ReactionIconPack","lit")}},

    // 43 common icons.
    {"grinning",     0x1F600, false, QT_TRANSLATE_NOOP("ReactionIconPack","grinning face"), {QT_TRANSLATE_NOOP("ReactionIconPack","grin"), QT_TRANSLATE_NOOP("ReactionIconPack","happy"), QT_TRANSLATE_NOOP("ReactionIconPack","grinning face")}},
    {"smile-open",   0x1F603, false, QT_TRANSLATE_NOOP("ReactionIconPack","smiley"), {QT_TRANSLATE_NOOP("ReactionIconPack","smile"), QT_TRANSLATE_NOOP("ReactionIconPack","happy")}},
    {"smile-eyes",   0x1F604, false, QT_TRANSLATE_NOOP("ReactionIconPack","smiling eyes"), {QT_TRANSLATE_NOOP("ReactionIconPack","smile"), QT_TRANSLATE_NOOP("ReactionIconPack","smiling eyes")}},
    {"grin",         0x1F601, false, QT_TRANSLATE_NOOP("ReactionIconPack","grin"), {QT_TRANSLATE_NOOP("ReactionIconPack","grin"), QT_TRANSLATE_NOOP("ReactionIconPack","big smile")}},
    {"sweat-smile",  0x1F605, false, QT_TRANSLATE_NOOP("ReactionIconPack","nervous sweat"), {QT_TRANSLATE_NOOP("ReactionIconPack","sweat"), QT_TRANSLATE_NOOP("ReactionIconPack","nervous"), QT_TRANSLATE_NOOP("ReactionIconPack","phew")}},
    {"rofl",         0x1F923, false, QT_TRANSLATE_NOOP("ReactionIconPack","rolling on the floor laughing"), {QT_TRANSLATE_NOOP("ReactionIconPack","rofl"), QT_TRANSLATE_NOOP("ReactionIconPack","laughing"), QT_TRANSLATE_NOOP("ReactionIconPack","rolling on the floor")}},
    {"blush",        0x1F60A, false, QT_TRANSLATE_NOOP("ReactionIconPack","blush"), {QT_TRANSLATE_NOOP("ReactionIconPack","blush"), QT_TRANSLATE_NOOP("ReactionIconPack","shy"), QT_TRANSLATE_NOOP("ReactionIconPack","embarrassed")}},
    {"slight-smile", 0x1F642, false, QT_TRANSLATE_NOOP("ReactionIconPack","slightly smiling face"), {QT_TRANSLATE_NOOP("ReactionIconPack","slight smile"), QT_TRANSLATE_NOOP("ReactionIconPack","smile")}},
    {"wink",         0x1F609, false, QT_TRANSLATE_NOOP("ReactionIconPack","wink"), {QT_TRANSLATE_NOOP("ReactionIconPack","wink"), QT_TRANSLATE_NOOP("ReactionIconPack","flirt")}},
    {"heart-eyes",   0x1F60D, false, QT_TRANSLATE_NOOP("ReactionIconPack","heart eyes"), {QT_TRANSLATE_NOOP("ReactionIconPack","heart eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","love"), QT_TRANSLATE_NOOP("ReactionIconPack","in love")}},
    {"blow-kiss",    0x1F618, false, QT_TRANSLATE_NOOP("ReactionIconPack","blowing a kiss"), {QT_TRANSLATE_NOOP("ReactionIconPack","kiss"), QT_TRANSLATE_NOOP("ReactionIconPack","blow a kiss")}},
    {"wink-tongue",  0x1F61C, false, QT_TRANSLATE_NOOP("ReactionIconPack","winking tongue"), {QT_TRANSLATE_NOOP("ReactionIconPack","wink"), QT_TRANSLATE_NOOP("ReactionIconPack","tongue"), QT_TRANSLATE_NOOP("ReactionIconPack","silly")}},
    {"hug",          0x1F917, false, QT_TRANSLATE_NOOP("ReactionIconPack","hugging face"), {QT_TRANSLATE_NOOP("ReactionIconPack","hug"), QT_TRANSLATE_NOOP("ReactionIconPack","hugging")}},
    {"thinking",     0x1F914, false, QT_TRANSLATE_NOOP("ReactionIconPack","thinking"), {QT_TRANSLATE_NOOP("ReactionIconPack","thinking"), QT_TRANSLATE_NOOP("ReactionIconPack","hmm")}},
    {"neutral",      0x1F610, false, QT_TRANSLATE_NOOP("ReactionIconPack","neutral face"), {QT_TRANSLATE_NOOP("ReactionIconPack","neutral"), QT_TRANSLATE_NOOP("ReactionIconPack","meh")}},
    {"sleeping",     0x1F634, false, QT_TRANSLATE_NOOP("ReactionIconPack","sleeping"), {QT_TRANSLATE_NOOP("ReactionIconPack","sleeping"), QT_TRANSLATE_NOOP("ReactionIconPack","tired")}},
    {"sunglasses",   0x1F60E, false, QT_TRANSLATE_NOOP("ReactionIconPack","cool"), {QT_TRANSLATE_NOOP("ReactionIconPack","cool"), QT_TRANSLATE_NOOP("ReactionIconPack","sunglasses")}},
    {"party-face",   0x1F973, false, QT_TRANSLATE_NOOP("ReactionIconPack","partying face"), {QT_TRANSLATE_NOOP("ReactionIconPack","party"), QT_TRANSLATE_NOOP("ReactionIconPack","celebrate")}},
    {"sob",          0x1F62D, false, QT_TRANSLATE_NOOP("ReactionIconPack","sobbing"), {QT_TRANSLATE_NOOP("ReactionIconPack","sob"), QT_TRANSLATE_NOOP("ReactionIconPack","crying"), QT_TRANSLATE_NOOP("ReactionIconPack","sad")}},
    {"scream",       0x1F631, false, QT_TRANSLATE_NOOP("ReactionIconPack","screaming in fear"), {QT_TRANSLATE_NOOP("ReactionIconPack","scream"), QT_TRANSLATE_NOOP("ReactionIconPack","shocked"), QT_TRANSLATE_NOOP("ReactionIconPack","fear")}},
    {"rage",         0x1F621, false, QT_TRANSLATE_NOOP("ReactionIconPack","angry"), {QT_TRANSLATE_NOOP("ReactionIconPack","angry"), QT_TRANSLATE_NOOP("ReactionIconPack","rage"), QT_TRANSLATE_NOOP("ReactionIconPack","mad")}},
    {"mind-blown",   0x1F92F, false, QT_TRANSLATE_NOOP("ReactionIconPack","mind blown"), {QT_TRANSLATE_NOOP("ReactionIconPack","mind blown"), QT_TRANSLATE_NOOP("ReactionIconPack","wow")}},
    {"thumbsdown",   0x1F44E, false, QT_TRANSLATE_NOOP("ReactionIconPack","thumbs down"), {QT_TRANSLATE_NOOP("ReactionIconPack","thumbs down"), QT_TRANSLATE_NOOP("ReactionIconPack","dislike"), QT_TRANSLATE_NOOP("ReactionIconPack","no")}},
    {"clap",         0x1F44F, false, QT_TRANSLATE_NOOP("ReactionIconPack","applause"), {QT_TRANSLATE_NOOP("ReactionIconPack","clap"), QT_TRANSLATE_NOOP("ReactionIconPack","applause"), QT_TRANSLATE_NOOP("ReactionIconPack","well done")}},
    {"raised-hands", 0x1F64C, false, QT_TRANSLATE_NOOP("ReactionIconPack","raising hands"), {QT_TRANSLATE_NOOP("ReactionIconPack","raised hands"), QT_TRANSLATE_NOOP("ReactionIconPack","celebration"), QT_TRANSLATE_NOOP("ReactionIconPack","yay")}},
    {"ok-hand",      0x1F44C, false, QT_TRANSLATE_NOOP("ReactionIconPack","OK hand"), {QT_TRANSLATE_NOOP("ReactionIconPack","ok"), QT_TRANSLATE_NOOP("ReactionIconPack","ok hand"), QT_TRANSLATE_NOOP("ReactionIconPack","perfect")}},
    {"victory",      0x270C,  false, QT_TRANSLATE_NOOP("ReactionIconPack","victory hand"), {QT_TRANSLATE_NOOP("ReactionIconPack","victory"), QT_TRANSLATE_NOOP("ReactionIconPack","peace")}},
    {"handshake",    0x1F91D, false, QT_TRANSLATE_NOOP("ReactionIconPack","handshake"), {QT_TRANSLATE_NOOP("ReactionIconPack","handshake"), QT_TRANSLATE_NOOP("ReactionIconPack","deal"), QT_TRANSLATE_NOOP("ReactionIconPack","agreement")}},
    {"muscle",       0x1F4AA, false, QT_TRANSLATE_NOOP("ReactionIconPack","flexed biceps"), {QT_TRANSLATE_NOOP("ReactionIconPack","muscle"), QT_TRANSLATE_NOOP("ReactionIconPack","strong"), QT_TRANSLATE_NOOP("ReactionIconPack","flex")}},
    {"tada",         0x1F389, false, QT_TRANSLATE_NOOP("ReactionIconPack","party popper"), {QT_TRANSLATE_NOOP("ReactionIconPack","tada"), QT_TRANSLATE_NOOP("ReactionIconPack","party popper"), QT_TRANSLATE_NOOP("ReactionIconPack","celebrate")}},
    {"birthday",     0x1F382, false, QT_TRANSLATE_NOOP("ReactionIconPack","birthday cake"), {QT_TRANSLATE_NOOP("ReactionIconPack","birthday"), QT_TRANSLATE_NOOP("ReactionIconPack","cake")}},
    {"gift",         0x1F381, false, QT_TRANSLATE_NOOP("ReactionIconPack","wrapped gift"), {QT_TRANSLATE_NOOP("ReactionIconPack","gift"), QT_TRANSLATE_NOOP("ReactionIconPack","present")}},
    {"hundred",      0x1F4AF, false, QT_TRANSLATE_NOOP("ReactionIconPack","hundred points"), {QT_TRANSLATE_NOOP("ReactionIconPack","100"), QT_TRANSLATE_NOOP("ReactionIconPack","hundred"), QT_TRANSLATE_NOOP("ReactionIconPack","perfect score")}},
    {"check",        0x2705,  false, QT_TRANSLATE_NOOP("ReactionIconPack","check mark"), {QT_TRANSLATE_NOOP("ReactionIconPack","check"), QT_TRANSLATE_NOOP("ReactionIconPack","done"), QT_TRANSLATE_NOOP("ReactionIconPack","correct")}},
    {"cross",        0x274C,  false, QT_TRANSLATE_NOOP("ReactionIconPack","cross mark"), {QT_TRANSLATE_NOOP("ReactionIconPack","cross"), QT_TRANSLATE_NOOP("ReactionIconPack","wrong"), QT_TRANSLATE_NOOP("ReactionIconPack","no")}},
    {"star",         0x2B50,  false, QT_TRANSLATE_NOOP("ReactionIconPack","star"), {QT_TRANSLATE_NOOP("ReactionIconPack","star"), QT_TRANSLATE_NOOP("ReactionIconPack","favorite")}},
    {"broken-heart", 0x1F494, false, QT_TRANSLATE_NOOP("ReactionIconPack","broken heart"), {QT_TRANSLATE_NOOP("ReactionIconPack","broken heart"), QT_TRANSLATE_NOOP("ReactionIconPack","heartbroken"), QT_TRANSLATE_NOOP("ReactionIconPack","sad")}},
    {"smiling-imp",  0x1F608, false, QT_TRANSLATE_NOOP("ReactionIconPack","smiling face with horns"), {QT_TRANSLATE_NOOP("ReactionIconPack","devil"), QT_TRANSLATE_NOOP("ReactionIconPack","mischief")}},
    {"eyes",         0x1F440, false, QT_TRANSLATE_NOOP("ReactionIconPack","eyes"), {QT_TRANSLATE_NOOP("ReactionIconPack","eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","look"), QT_TRANSLATE_NOOP("ReactionIconPack","watching")}},
    {"clown",        0x1F921, false, QT_TRANSLATE_NOOP("ReactionIconPack","clown face"), {QT_TRANSLATE_NOOP("ReactionIconPack","clown"), QT_TRANSLATE_NOOP("ReactionIconPack","joke")}},
    {"poop",         0x1F4A9, false, QT_TRANSLATE_NOOP("ReactionIconPack","pile of poo"), {QT_TRANSLATE_NOOP("ReactionIconPack","poop"), QT_TRANSLATE_NOOP("ReactionIconPack","funny")}},
    {"rocket",       0x1F680, false, QT_TRANSLATE_NOOP("ReactionIconPack","rocket"), {QT_TRANSLATE_NOOP("ReactionIconPack","rocket"), QT_TRANSLATE_NOOP("ReactionIconPack","launch"), QT_TRANSLATE_NOOP("ReactionIconPack","fast")}},
    {"smirk",        0x1F60F, false, QT_TRANSLATE_NOOP("ReactionIconPack","smirk"), {QT_TRANSLATE_NOOP("ReactionIconPack","smirk"), QT_TRANSLATE_NOOP("ReactionIconPack","smug")}},

    // Four added to bring the pack to 54, which is exactly the emoji picker's original 9x6 grid
    // (now one of the sections rather than the whole gallery) -- at 50 it left four empty cells
    // in the last row. Two faces and one hand fill the most obvious gaps in the existing set;
    // sparkles joins fire/star/hundred as a non-figurative "reaction" symbol.
    {"roll-eyes",    0x1F644, false, QT_TRANSLATE_NOOP("ReactionIconPack","rolling eyes"), {QT_TRANSLATE_NOOP("ReactionIconPack","roll eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","eyeroll"), QT_TRANSLATE_NOOP("ReactionIconPack","whatever")}},
    {"grimacing",    0x1F62C, false, QT_TRANSLATE_NOOP("ReactionIconPack","grimacing"), {QT_TRANSLATE_NOOP("ReactionIconPack","grimace"), QT_TRANSLATE_NOOP("ReactionIconPack","awkward"), QT_TRANSLATE_NOOP("ReactionIconPack","yikes")}},
    {"sparkles",     0x2728,  false, QT_TRANSLATE_NOOP("ReactionIconPack","sparkles"), {QT_TRANSLATE_NOOP("ReactionIconPack","sparkles"), QT_TRANSLATE_NOOP("ReactionIconPack","shiny"), QT_TRANSLATE_NOOP("ReactionIconPack","magic")}},
    {"crossed-fingers", 0x1F91E, false, QT_TRANSLATE_NOOP("ReactionIconPack","crossed fingers"), {QT_TRANSLATE_NOOP("ReactionIconPack","crossed fingers"), QT_TRANSLATE_NOOP("ReactionIconPack","good luck"), QT_TRANSLATE_NOOP("ReactionIconPack","hope")}},
};
// clang-format on

constexpr size_t curatedIconCount=sizeof(curatedIcons)/sizeof(curatedIcons[0]);

//! One category tab's stable id, (translatable) display title, and an optional override for
//! which icon represents it in the tab strip -- in the gallery's display order. "common" first,
//! then the nine CLDR groups gen-emoji-pack.py sections generatedIcons[] by -- keep this list's
//! ids in sync with CATEGORY_ORDER in that script.
struct CategoryDef
{
    const char* id;
    const char* title;

    //! iconId to use as this category's tab icon, or nullptr for the default (the first icon in
    //! the category's own list -- see buildCategories()). Needed because pass 1 in build()
    //! always inserts curated icons into their category's list BEFORE any generated one, so the
    //! default front() for a category that contains a curated icon is always one of the "Most
    //! common" tab's own 54 -- usually harmless (a different icon per tab, just also common), but
    //! "people-body"'s default happened to be "thumbsup", the SAME icon "common"'s own tab uses
    //! (it is curatedIcons[0]), so the two tabs looked identical. Overridden here rather than
    //! fixed by an automatic "skip anything also in common" rule, which would have picked
    //! something for every category more or less at random -- a curated choice reads better.
    const char* sampleIconIdOverride=nullptr;
};
const CategoryDef categoryDefs[]={
    {"common",           QT_TRANSLATE_NOOP("ReactionIconPack","Most common")},
    {"smileys-emotion",  QT_TRANSLATE_NOOP("ReactionIconPack","Smileys & Emotion")},
    {"people-body",      QT_TRANSLATE_NOOP("ReactionIconPack","People & Body"), "adult"},
    {"animals-nature",   QT_TRANSLATE_NOOP("ReactionIconPack","Animals & Nature")},
    {"food-drink",       QT_TRANSLATE_NOOP("ReactionIconPack","Food & Drink")},
    {"travel-places",    QT_TRANSLATE_NOOP("ReactionIconPack","Travel & Places")},
    {"activities",       QT_TRANSLATE_NOOP("ReactionIconPack","Activities")},
    {"objects",          QT_TRANSLATE_NOOP("ReactionIconPack","Objects")},
    {"symbols",          QT_TRANSLATE_NOOP("ReactionIconPack","Symbols")},
    {"flags",            QT_TRANSLATE_NOOP("ReactionIconPack","Flags")},
};
constexpr size_t categoryDefCount=sizeof(categoryDefs)/sizeof(categoryDefs[0]);

//! Splits on whitespace and inserts the folded key alongside the icon's index, so a multi-word
//! keyword like "red heart" is also found by a prefix of just "heart" (see
//! AbstractReactionIconPack::search()'s own doc comment).
void indexKeyword(std::vector<std::pair<QString,size_t>>& index, const QString& keyword, size_t iconIndex)
{
    const auto words=keyword.split(QChar(' '),Qt::SkipEmptyParts);
    for (const auto& word : words)
    {
        index.emplace_back(word.toCaseFolded(),iconIndex);
    }
}

//! The alias name this pack resolves every icon's art through, e.g. "ChatReactionPack::star".
//! Common to both the curated 54 (resolved via the existing "ChatReactionPack" JSON alias
//! context in resources/style/chatreactions.json, which the restyling override path reads) and
//! the generated remainder (resolved via a namePath registered directly in
//! Pimpl::ensureNamePathsRegistered() below) -- a single naming scheme lets at()/find()/etc.
//! resolve either kind identically, without caring which one an index happens to be.
QString iconAliasName(const QString& iconId)
{
    return QStringLiteral("ChatReactionPack::%1").arg(iconId);
}

} // anonymous namespace

//--------------------------------------------------------------------------

const ReactionIconInfo* AbstractReactionIconPack::findByShortcode(const QString& shortcode) const
{
    // Default O(n) body -- see this method's own doc comment in reactioniconpack.hpp for why it
    // is virtual-with-a-default rather than pure. DefaultReactionIconPack overrides this with an
    // O(log n) map lookup built over every alias; a smaller out-of-tree pack (or a test double)
    // gets a correct, if slower, answer for free.
    if (shortcode.isEmpty())
    {
        return nullptr;
    }
    const auto folded=shortcode.toCaseFolded();
    const auto n=count();
    for (size_t i=0; i<n; ++i)
    {
        const auto* info=at(i);
        if (info!=nullptr && !info->shortcode.isEmpty() && info->shortcode.toCaseFolded()==folded)
        {
            return info;
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

class DefaultReactionIconPack::Pimpl
{
    public:

        // NOT mutable: at()/find()/etc. reach icons through the pimpl-> indirection, which
        // already bypasses the owning DefaultReactionIconPack method's const-ness (a unique_ptr's
        // operator-> returns a non-const pointee regardless of the smart pointer's own
        // const-ness) -- so lazy resolution needs no mutable member, just a non-const helper.
        std::vector<ReactionIconInfo> icons;
        std::map<QString,size_t> idToIndex;
        std::map<QString,size_t> codeToIndex;      //!< emojiCode -> icon index, see findByCode()
        std::map<QString,size_t> shortcodeToIndex; //!< EVERY alias (not just the canonical
                                                    //!< shortcode) -> icon index, see
                                                    //!< findByShortcode()
        std::vector<std::pair<QString,size_t>> prefixIndex; //!< sorted (foldedWord, iconIndex)

        std::vector<AbstractReactionIconPack::Category> categoryList;
        std::map<QString,std::vector<size_t>> categoryIconLists;

        //! Lazily fills in ReactionIconInfo::icon on first use -- see reactioniconpack.hpp's own
        //! Part 3c reasoning: SvgIconLocator::icon() calls SvgIcon::addFile(), which reads and
        //! parses the SVG immediately, so resolving all ~1374 entries up front at construction
        //! would read and parse the whole pack (several MB of SVG) whether or not the gallery is
        //! ever opened.
        void resolveIcon(size_t index)
        {
            auto& info=icons[index];
            if (!info.icon)
            {
                info.icon=Style::instance().svgIconLocator().icon(iconAliasName(info.iconId));
            }
        }

        void build()
        {
            icons.clear();
            idToIndex.clear();
            codeToIndex.clear();
            shortcodeToIndex.clear();
            categoryList.clear();
            categoryIconLists.clear();
            icons.reserve(generatedIconCount);

            // codepoint -> index into generatedIcons[], so the curated pass below can find its
            // matching row without a linear scan per entry.
            std::map<char32_t,size_t> generatedByCodepoint;
            for (size_t i=0; i<generatedIconCount; ++i)
            {
                generatedByCodepoint.emplace(generatedIcons[i].codepoint,i);
            }

            auto addAliases=[this](const GeneratedIconEntry& gen, size_t iconIndex)
            {
                for (const auto* alias : gen.aliases)
                {
                    // emplace, not operator[] -- first-listed (and, within one entry's alias
                    // list, first-ALIAS) wins, matching findByCode()'s documented "first match
                    // wins" contract. gen-emoji-pack.py already asserts zero cross-icon alias
                    // collisions at generation time, so this only matters for is-already-present
                    // idempotency, never for picking a winner between two different icons.
                    shortcodeToIndex.emplace(QString::fromUtf8(alias).toCaseFolded(),iconIndex);
                }
            };

            // Pass 1: the 54 curated entries, in their own fixed display order, so
            // basicIconIds() can keep taking the leading N of them exactly as before.
            std::set<char32_t> curatedCodepoints;
            for (size_t i=0; i<curatedIconCount; ++i)
            {
                const auto& curated=curatedIcons[i];
                curatedCodepoints.insert(curated.codepoint);

                auto genIt=generatedByCodepoint.find(curated.codepoint);
                // Every curated codepoint is verified (by gen-emoji-pack.py's own selection
                // rule, and by hand at review time) to also be in the generated set -- but a
                // pack must not crash if a future edit to either table breaks that. Fall back to
                // the curated entry's own iconId as a degraded shortcode rather than leaving it
                // empty, so the icon stays findable by SOMETHING.
                const GeneratedIconEntry* gen=(genIt!=generatedByCodepoint.end())
                    ? &generatedIcons[genIt->second] : nullptr;

                ReactionIconInfo info;
                info.iconId=QString::fromLatin1(curated.iconId);
                for (const auto* keyword : curated.keywords)
                {
                    info.keywords << QCoreApplication::translate("ReactionIconPack",keyword);
                }
                info.emojiCode=QString::fromUcs4(&curated.codepoint,1);
                info.emojiText=info.emojiCode
                               +((gen!=nullptr && gen->needsVariationSelector)
                                     ? QString(QChar(0xFE0F))
                                     : QString());
                info.shortcode=(gen!=nullptr) ? QString::fromUtf8(gen->shortcode) : info.iconId;
                info.description=QCoreApplication::translate("ReactionIconPack",curated.description);

                const auto index=icons.size();
                idToIndex.emplace(info.iconId,index);
                codeToIndex.emplace(info.emojiCode,index);
                shortcodeToIndex.emplace(info.shortcode.toCaseFolded(),index);
                if (gen!=nullptr)
                {
                    addAliases(*gen,index);
                }
                categoryIconLists["common"].push_back(index);
                if (gen!=nullptr && gen->categoryId[0]!='\0')
                {
                    categoryIconLists[QString::fromLatin1(gen->categoryId)].push_back(index);
                }
                icons.push_back(std::move(info));
            }

            // Pass 2: every remaining generated entry (curated codepoints already added above
            // are skipped so each emoji has exactly ONE icons[] entry -- see the pack's own "one
            // entry per emoji" invariant; the duplication the gallery shows between "Most common"
            // and an icon's own category is a VIEW via categoryIconLists, not a second entry).
            for (size_t i=0; i<generatedIconCount; ++i)
            {
                const auto& gen=generatedIcons[i];
                if (curatedCodepoints.count(gen.codepoint)>0)
                {
                    continue;
                }

                ReactionIconInfo info;
                // The generated shortcode IS the iconId for a non-curated entry: unlike the
                // curated 54, nothing has ever persisted a reaction against one of these ids
                // before this pack existed, so there is no compatibility reason to invent a
                // separate internal name, and reusing the shortcode keeps one fewer table to
                // keep in sync. Never contains '@' (the charset gen-emoji-pack.py accepts for a
                // shortcode is [A-Za-z0-9_+-]), so it is always safe as ChatReactionId's id half.
                info.iconId=QString::fromUtf8(gen.shortcode);
                for (const auto* keyword : gen.keywords)
                {
                    // Plain QString::fromUtf8, deliberately NOT QCoreApplication::translate --
                    // see emojiicontable.inc's header comment for the i18n decision.
                    info.keywords << QString::fromUtf8(keyword);
                }
                info.emojiCode=QString::fromUcs4(&gen.codepoint,1);
                info.emojiText=info.emojiCode
                               +(gen.needsVariationSelector ? QString(QChar(0xFE0F)) : QString());
                info.shortcode=QString::fromUtf8(gen.shortcode);
                info.description=QString::fromUtf8(gen.description);

                const auto index=icons.size();
                idToIndex.emplace(info.iconId,index);
                codeToIndex.emplace(info.emojiCode,index);
                addAliases(gen,index);
                if (gen.categoryId[0]!='\0')
                {
                    categoryIconLists[QString::fromLatin1(gen.categoryId)].push_back(index);
                }
                icons.push_back(std::move(info));
            }

            // Register a direct file path for every NON-curated icon's alias name -- the curated
            // 54 keep resolving through the existing "ChatReactionPack" JSON alias context (see
            // resources/style/chatreactions.json), which the restyling override path reads and
            // which this must not bypass. This is cheap (just a map insert, see addNamePath()'s
            // own doc comment) and safe to redo on every build()/retranslate() -- it does NOT
            // read or parse any SVG; that happens lazily in resolveIcon().
            auto& locator=Style::instance().svgIconLocator();
            for (size_t i=0; i<generatedIconCount; ++i)
            {
                const auto& gen=generatedIcons[i];
                if (curatedCodepoints.count(gen.codepoint)>0)
                {
                    continue;
                }
                // Reconstructs thirdparty/noto-emoji/'s upstream filename from the codepoint
                // alone -- see gen-emoji-pack.py, which relies on the same reconstruction to
                // avoid a redundant filename column in this table. Lowercase hex (Qt's base-16
                // arg() default) zero-padded to a MINIMUM of 4 digits: upstream pads every BMP
                // codepoint below 0x1000 to 4 digits (U+00A9 COPYRIGHT SIGN is
                // "emoji_u00a9.svg", never "emoji_ua9.svg") but does NOT truncate a
                // supplementary-plane codepoint's natural 5-6 digits, and Qt's fieldWidth is a
                // minimum -- it pads shorter strings, never truncates longer ones -- so one rule
                // covers both. Verified against every codepoint actually below 0x1000 in this
                // pack (U+00A9, U+00AE).
                const auto path=QStringLiteral(":/icons/noto-emoji/emoji_u%1.svg")
                                     .arg(static_cast<uint>(gen.codepoint),4,16,QChar('0'));
                locator.addNamePath(iconAliasName(QString::fromUtf8(gen.shortcode)),path);
            }

            buildCategories();
            buildPrefixIndex();
        }

        void buildCategories()
        {
            categoryList.clear();
            for (size_t i=0; i<categoryDefCount; ++i)
            {
                const auto& def=categoryDefs[i];
                auto it=categoryIconLists.find(QString::fromLatin1(def.id));
                if (it==categoryIconLists.end() || it->second.empty())
                {
                    // Defensive, not expected: every category in categoryDefs has at least one
                    // icon in the shipped data (measured at generation time). A pack that somehow
                    // ends up with an empty category simply does not offer a tab for it, rather
                    // than offering an empty section.
                    continue;
                }

                AbstractReactionIconPack::Category category;
                category.id=QString::fromLatin1(def.id);
                category.title=QCoreApplication::translate("ReactionIconPack",def.title);
                category.sampleIconId=icons[it->second.front()].iconId;
                if (def.sampleIconIdOverride!=nullptr)
                {
                    // Verified against idToIndex, not just assumed -- a typo'd override id must
                    // degrade to the default sample rather than the tab silently offering no
                    // icon at all (find()-style callers already have that instinct baked in).
                    auto overrideIt=idToIndex.find(QString::fromLatin1(def.sampleIconIdOverride));
                    if (overrideIt!=idToIndex.end())
                    {
                        category.sampleIconId=icons[overrideIt->second].iconId;
                    }
                }
                categoryList.push_back(std::move(category));
            }
        }

        void buildPrefixIndex()
        {
            prefixIndex.clear();

            for (size_t i=0; i<icons.size(); ++i)
            {
                const auto& info=icons[i];

                // The icon id itself is always a match target ("thumbsup" finds thumbsup even
                // though no keyword literally repeats it) -- and, for the generated majority
                // where iconId IS the shortcode, this also covers shortcode search for free.
                indexKeyword(prefixIndex,info.iconId,i);
                if (info.shortcode!=info.iconId)
                {
                    indexKeyword(prefixIndex,info.shortcode,i);
                }

                for (const auto& keyword : info.keywords)
                {
                    indexKeyword(prefixIndex,keyword,i);
                }
            }

            // Curated keywords are ALSO indexed under their raw English literal (in addition to
            // whatever info.keywords holds, which is already the CURRENT translation) so an
            // English search term still works in a localized UI -- see search()'s own doc
            // comment. Only meaningful for the curated 54; the generated majority's keywords are
            // English already, so indexing them twice would just waste prefixIndex entries (the
            // std::unique() pass below would drop the duplicates anyway, but there is no reason
            // to build them in the first place).
            for (size_t i=0; i<curatedIconCount; ++i)
            {
                for (const auto* keyword : curatedIcons[i].keywords)
                {
                    indexKeyword(prefixIndex,QString::fromUtf8(keyword),i);
                }
            }

            std::sort(prefixIndex.begin(),prefixIndex.end());
            prefixIndex.erase(std::unique(prefixIndex.begin(),prefixIndex.end()),prefixIndex.end());
        }
};

//--------------------------------------------------------------------------

DefaultReactionIconPack::DefaultReactionIconPack()
    : pimpl(std::make_unique<Pimpl>())
{
    pimpl->build();
}

//--------------------------------------------------------------------------

DefaultReactionIconPack::~DefaultReactionIconPack()
{
}

//--------------------------------------------------------------------------

QString DefaultReactionIconPack::uri() const
{
    return QString{};
}

//--------------------------------------------------------------------------

size_t DefaultReactionIconPack::count() const
{
    return pimpl->icons.size();
}

//--------------------------------------------------------------------------

const ReactionIconInfo* DefaultReactionIconPack::at(size_t index) const
{
    if (index>=pimpl->icons.size())
    {
        return nullptr;
    }
    pimpl->resolveIcon(index);
    return &pimpl->icons[index];
}

//--------------------------------------------------------------------------

const ReactionIconInfo* DefaultReactionIconPack::find(const QString& iconId) const
{
    auto it=pimpl->idToIndex.find(iconId);
    if (it==pimpl->idToIndex.end())
    {
        return nullptr;
    }
    pimpl->resolveIcon(it->second);
    return &pimpl->icons[it->second];
}

//--------------------------------------------------------------------------

const ReactionIconInfo* DefaultReactionIconPack::findByCode(const QString& emojiCode) const
{
    auto it=pimpl->codeToIndex.find(emojiCode);
    if (it==pimpl->codeToIndex.end())
    {
        return nullptr;
    }
    pimpl->resolveIcon(it->second);
    return &pimpl->icons[it->second];
}

//--------------------------------------------------------------------------

const ReactionIconInfo* DefaultReactionIconPack::findByShortcode(const QString& shortcode) const
{
    if (shortcode.isEmpty())
    {
        return nullptr;
    }
    // Keyed on EVERY alias, not just the canonical shortcode -- see shortcodeToIndex's own
    // comment in Pimpl::build(). ":+1:" and ":thumbsup:" both resolve to the same entry.
    auto it=pimpl->shortcodeToIndex.find(shortcode.toCaseFolded());
    if (it==pimpl->shortcodeToIndex.end())
    {
        return nullptr;
    }
    pimpl->resolveIcon(it->second);
    return &pimpl->icons[it->second];
}

//--------------------------------------------------------------------------

std::vector<size_t> DefaultReactionIconPack::search(const QString& prefix) const
{
    std::vector<size_t> result;

    if (prefix.isEmpty())
    {
        result.reserve(pimpl->icons.size());
        for (size_t i=0; i<pimpl->icons.size(); ++i)
        {
            result.push_back(i);
        }
        return result;
    }

    const auto folded=prefix.toCaseFolded();
    auto it=std::lower_bound(pimpl->prefixIndex.begin(),pimpl->prefixIndex.end(),
                              std::make_pair(folded,size_t{0}));

    std::vector<bool> seen(pimpl->icons.size(),false);
    for (; it!=pimpl->prefixIndex.end() && it->first.startsWith(folded); ++it)
    {
        if (!seen[it->second])
        {
            seen[it->second]=true;
            result.push_back(it->second);
        }
    }

    // Emit in pack order, not match/insertion order -- see search()'s own doc comment.
    std::sort(result.begin(),result.end());
    return result;
}

//--------------------------------------------------------------------------

std::vector<QString> DefaultReactionIconPack::basicIconIds() const
{
    std::vector<QString> result;
    // The curated 54 are always icons[0..curatedIconCount) in their own fixed order (see
    // Pimpl::build()'s pass 1), so this can still just take the leading run of "basic" entries
    // exactly as it did when the whole pack WAS the curated 54.
    for (size_t i=0; i<curatedIconCount && curatedIcons[i].basic; ++i)
    {
        result.push_back(pimpl->icons[i].iconId);
    }
    return result;
}

//--------------------------------------------------------------------------

std::vector<AbstractReactionIconPack::Category> DefaultReactionIconPack::categories() const
{
    return pimpl->categoryList;
}

//--------------------------------------------------------------------------

std::vector<size_t> DefaultReactionIconPack::categoryIcons(const QString& categoryId) const
{
    auto it=pimpl->categoryIconLists.find(categoryId);
    if (it==pimpl->categoryIconLists.end())
    {
        return {};
    }
    return it->second;
}

//--------------------------------------------------------------------------

void DefaultReactionIconPack::retranslate()
{
    pimpl->build();
}

//--------------------------------------------------------------------------

class EmojiCodeReactionIconPack::Pimpl
{
    public:

        std::shared_ptr<AbstractReactionIconPack> source;

        //! our index -> source index, in the source's own display order.
        std::vector<size_t> toSource;

        //! source index -> our index, for remapping search() results.
        std::map<size_t,size_t> fromSource;

        void build()
        {
            toSource.clear();
            fromSource.clear();
            if (!source)
            {
                return;
            }
            const auto n=source->count();
            for (size_t i=0; i<n; ++i)
            {
                const auto* info=source->at(i);
                if (info!=nullptr && !info->emojiCode.isEmpty())
                {
                    fromSource.emplace(i,toSource.size());
                    toSource.push_back(i);
                }
            }
        }
};

//--------------------------------------------------------------------------

EmojiCodeReactionIconPack::EmojiCodeReactionIconPack(std::shared_ptr<AbstractReactionIconPack> source)
    : pimpl(std::make_unique<Pimpl>())
{
    pimpl->source=std::move(source);
    pimpl->build();
}

//--------------------------------------------------------------------------

EmojiCodeReactionIconPack::~EmojiCodeReactionIconPack()
{
}

//--------------------------------------------------------------------------

std::shared_ptr<AbstractReactionIconPack> EmojiCodeReactionIconPack::source() const
{
    return pimpl->source;
}

//--------------------------------------------------------------------------

QString EmojiCodeReactionIconPack::uri() const
{
    return pimpl->source ? pimpl->source->uri() : QString{};
}

//--------------------------------------------------------------------------

size_t EmojiCodeReactionIconPack::count() const
{
    return pimpl->toSource.size();
}

//--------------------------------------------------------------------------

const ReactionIconInfo* EmojiCodeReactionIconPack::at(size_t index) const
{
    if (index>=pimpl->toSource.size())
    {
        return nullptr;
    }
    return pimpl->source->at(pimpl->toSource[index]);
}

//--------------------------------------------------------------------------

const ReactionIconInfo* EmojiCodeReactionIconPack::find(const QString& iconId) const
{
    if (!pimpl->source)
    {
        return nullptr;
    }
    // Delegate, then re-check membership: an id the source knows but which carries no code is
    // not in this view at all, and must come back as "not found" rather than as a hidden entry
    // a caller could still reach by name.
    const auto* info=pimpl->source->find(iconId);
    if (info==nullptr || info->emojiCode.isEmpty())
    {
        return nullptr;
    }
    return info;
}

//--------------------------------------------------------------------------

const ReactionIconInfo* EmojiCodeReactionIconPack::findByCode(const QString& emojiCode) const
{
    if (!pimpl->source)
    {
        return nullptr;
    }
    // No membership re-check needed: an entry found BY a non-empty code necessarily has one.
    // An empty argument is already "not found" in every pack, see findByCode()'s doc comment.
    return pimpl->source->findByCode(emojiCode);
}

//--------------------------------------------------------------------------

const ReactionIconInfo* EmojiCodeReactionIconPack::findByShortcode(const QString& shortcode) const
{
    if (!pimpl->source)
    {
        return nullptr;
    }
    // Delegate, then re-check membership -- see this override's own doc comment in
    // reactioniconpack.hpp for why this follows find(), not findByCode().
    const auto* info=pimpl->source->findByShortcode(shortcode);
    if (info==nullptr || info->emojiCode.isEmpty())
    {
        return nullptr;
    }
    return info;
}

//--------------------------------------------------------------------------

std::vector<size_t> EmojiCodeReactionIconPack::search(const QString& prefix) const
{
    std::vector<size_t> result;
    if (!pimpl->source)
    {
        return result;
    }

    // The source's indices are meaningless to our caller -- remap every one that survives the
    // filter into OUR index space, so the "indices into at()" contract holds for this pack too.
    const auto sourceMatches=pimpl->source->search(prefix);
    result.reserve(sourceMatches.size());
    for (auto sourceIndex : sourceMatches)
    {
        auto it=pimpl->fromSource.find(sourceIndex);
        if (it!=pimpl->fromSource.end())
        {
            result.push_back(it->second);
        }
    }
    return result;
}

//--------------------------------------------------------------------------

std::vector<QString> EmojiCodeReactionIconPack::basicIconIds() const
{
    std::vector<QString> result;
    if (!pimpl->source)
    {
        return result;
    }
    for (const auto& iconId : pimpl->source->basicIconIds())
    {
        if (find(iconId)!=nullptr)
        {
            result.push_back(iconId);
        }
    }
    return result;
}

//--------------------------------------------------------------------------

std::vector<AbstractReactionIconPack::Category> EmojiCodeReactionIconPack::categories() const
{
    // Forwarded as-is -- categories are a source-pack concept (a section of icons, most of which
    // carry a code) and this view never needs to filter the category LIST itself, only the icon
    // indices within each one (categoryIcons(), below).
    return pimpl->source ? pimpl->source->categories() : std::vector<AbstractReactionIconPack::Category>{};
}

//--------------------------------------------------------------------------

std::vector<size_t> EmojiCodeReactionIconPack::categoryIcons(const QString& categoryId) const
{
    std::vector<size_t> result;
    if (!pimpl->source)
    {
        return result;
    }
    // Same remap-and-drop as search(): a source index this view filtered out (no emojiCode)
    // silently does not appear, rather than breaking the "indices into at()" contract.
    for (auto sourceIndex : pimpl->source->categoryIcons(categoryId))
    {
        auto it=pimpl->fromSource.find(sourceIndex);
        if (it!=pimpl->fromSource.end())
        {
            result.push_back(it->second);
        }
    }
    return result;
}

//--------------------------------------------------------------------------

void EmojiCodeReactionIconPack::retranslate()
{
    if (pimpl->source)
    {
        pimpl->source->retranslate();
    }
    // The source may have rebuilt its entries wholesale (DefaultReactionIconPack does), so every
    // index we hold is stale until this runs.
    pimpl->build();
}

//--------------------------------------------------------------------------

class ReactionIconPacks::Pimpl
{
    public:

        std::map<QString,std::shared_ptr<AbstractReactionIconPack>> packs;
        QString defaultUri;
};

//--------------------------------------------------------------------------

ReactionIconPacks::ReactionIconPacks()
    : pimpl(std::make_unique<Pimpl>())
{
    registerPack(std::make_shared<DefaultReactionIconPack>());
}

//--------------------------------------------------------------------------

ReactionIconPacks::~ReactionIconPacks()
{
}

//--------------------------------------------------------------------------

ReactionIconPacks& ReactionIconPacks::instance()
{
    static ReactionIconPacks inst;
    return inst;
}

//--------------------------------------------------------------------------

void ReactionIconPacks::registerPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    if (!pack)
    {
        return;
    }
    pimpl->packs[pack->uri()]=pack;
}

//--------------------------------------------------------------------------

std::shared_ptr<AbstractReactionIconPack> ReactionIconPacks::pack(const QString& uri) const
{
    auto it=pimpl->packs.find(uri);
    if (it==pimpl->packs.end())
    {
        return {};
    }
    return it->second;
}

//--------------------------------------------------------------------------

std::shared_ptr<AbstractReactionIconPack> ReactionIconPacks::defaultPack() const
{
    return pack(pimpl->defaultUri);
}

//--------------------------------------------------------------------------

void ReactionIconPacks::setDefaultPackUri(const QString& uri)
{
    pimpl->defaultUri=uri;
}

//--------------------------------------------------------------------------

const ReactionIconInfo* ReactionIconPacks::iconInfo(const QString& reactionId) const
{
    auto uri=ChatReactionId::packUri(reactionId);
    auto iconId=ChatReactionId::iconId(reactionId);

    auto p=pack(uri);
    if (!p)
    {
        return nullptr;
    }
    return p->find(iconId);
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> ReactionIconPacks::icon(const QString& reactionId) const
{
    const auto* info=iconInfo(reactionId);
    if (info==nullptr)
    {
        return {};
    }
    return info->icon;
}

//--------------------------------------------------------------------------

void ReactionIconPacks::retranslateAll()
{
    for (auto& [uri,p] : pimpl->packs)
    {
        p->retranslate();
    }
}

//--------------------------------------------------------------------------

}
