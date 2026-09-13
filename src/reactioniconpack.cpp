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

#include <QCoreApplication>

#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>

namespace uise {

namespace {

//! One embedded icon's id, keyword list and "is one of the 7 basics" marker. Keywords are wrapped
//! in QT_TRANSLATE_NOOP so lupdate collects them into translations/uise_*.ts under the
//! "ReactionIconPack" context (matched by the QCoreApplication::translate() call in
//! buildIconTable() below) -- see reactioniconpack.hpp's own comment on why this file must open
//! its namespace literally rather than via the UISE_DESKTOP_NAMESPACE_BEGIN macro.
struct RawIconEntry
{
    const char* iconId;
    char32_t codepoint;  //!< Unicode codepoint of the matching emoji, e.g. 0x1F44D for "thumbsup"
                         //!< -- matches thirdparty/noto-emoji/README.md's own table exactly (that
                         //!< is also where the .svg file this maps to, via the ChatReactionPack
                         //!< alias context, gets its "emoji_u<codepoint>.svg" name from). Every
                         //!< entry here is a single BMP or supplementary-plane codepoint, never a
                         //!< multi-codepoint ZWJ/variation-selector sequence -- see that same
                         //!< README for which upstream file was actually downloaded for each id.
    bool basic;
    std::initializer_list<const char*> keywords;
};

// clang-format off
const RawIconEntry rawIcons[]={
    // 7 basic (quick-bar) icons -- kept first so DefaultReactionIconPack::basicIconIds() can just
    // take the leading N entries in pack order.
    {"thumbsup",   0x1F44D, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","thumbs up"), QT_TRANSLATE_NOOP("ReactionIconPack","like"), QT_TRANSLATE_NOOP("ReactionIconPack","yes")}},
    {"heart",      0x2764,  true,  {QT_TRANSLATE_NOOP("ReactionIconPack","heart"), QT_TRANSLATE_NOOP("ReactionIconPack","love"), QT_TRANSLATE_NOOP("ReactionIconPack","red heart")}},
    {"joy",        0x1F602, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","laugh"), QT_TRANSLATE_NOOP("ReactionIconPack","joy"), QT_TRANSLATE_NOOP("ReactionIconPack","tears of joy"), QT_TRANSLATE_NOOP("ReactionIconPack","lol")}},
    {"open-mouth", 0x1F62E, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","surprised"), QT_TRANSLATE_NOOP("ReactionIconPack","wow"), QT_TRANSLATE_NOOP("ReactionIconPack","open mouth")}},
    {"cry",        0x1F622, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","cry"), QT_TRANSLATE_NOOP("ReactionIconPack","sad"), QT_TRANSLATE_NOOP("ReactionIconPack","tears")}},
    {"pray",       0x1F64F, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","pray"), QT_TRANSLATE_NOOP("ReactionIconPack","please"), QT_TRANSLATE_NOOP("ReactionIconPack","thanks")}},
    {"fire",       0x1F525, true,  {QT_TRANSLATE_NOOP("ReactionIconPack","fire"), QT_TRANSLATE_NOOP("ReactionIconPack","hot"), QT_TRANSLATE_NOOP("ReactionIconPack","lit")}},

    // 43 common icons.
    {"grinning",     0x1F600, false, {QT_TRANSLATE_NOOP("ReactionIconPack","grin"), QT_TRANSLATE_NOOP("ReactionIconPack","happy"), QT_TRANSLATE_NOOP("ReactionIconPack","grinning face")}},
    {"smile-open",   0x1F603, false, {QT_TRANSLATE_NOOP("ReactionIconPack","smile"), QT_TRANSLATE_NOOP("ReactionIconPack","happy")}},
    {"smile-eyes",   0x1F604, false, {QT_TRANSLATE_NOOP("ReactionIconPack","smile"), QT_TRANSLATE_NOOP("ReactionIconPack","smiling eyes")}},
    {"grin",         0x1F601, false, {QT_TRANSLATE_NOOP("ReactionIconPack","grin"), QT_TRANSLATE_NOOP("ReactionIconPack","big smile")}},
    {"sweat-smile",  0x1F605, false, {QT_TRANSLATE_NOOP("ReactionIconPack","sweat"), QT_TRANSLATE_NOOP("ReactionIconPack","nervous"), QT_TRANSLATE_NOOP("ReactionIconPack","phew")}},
    {"rofl",         0x1F923, false, {QT_TRANSLATE_NOOP("ReactionIconPack","rofl"), QT_TRANSLATE_NOOP("ReactionIconPack","laughing"), QT_TRANSLATE_NOOP("ReactionIconPack","rolling on the floor")}},
    {"blush",        0x1F60A, false, {QT_TRANSLATE_NOOP("ReactionIconPack","blush"), QT_TRANSLATE_NOOP("ReactionIconPack","shy"), QT_TRANSLATE_NOOP("ReactionIconPack","embarrassed")}},
    {"slight-smile", 0x1F642, false, {QT_TRANSLATE_NOOP("ReactionIconPack","slight smile"), QT_TRANSLATE_NOOP("ReactionIconPack","smile")}},
    {"wink",         0x1F609, false, {QT_TRANSLATE_NOOP("ReactionIconPack","wink"), QT_TRANSLATE_NOOP("ReactionIconPack","flirt")}},
    {"heart-eyes",   0x1F60D, false, {QT_TRANSLATE_NOOP("ReactionIconPack","heart eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","love"), QT_TRANSLATE_NOOP("ReactionIconPack","in love")}},
    {"blow-kiss",    0x1F618, false, {QT_TRANSLATE_NOOP("ReactionIconPack","kiss"), QT_TRANSLATE_NOOP("ReactionIconPack","blow a kiss")}},
    {"wink-tongue",  0x1F61C, false, {QT_TRANSLATE_NOOP("ReactionIconPack","wink"), QT_TRANSLATE_NOOP("ReactionIconPack","tongue"), QT_TRANSLATE_NOOP("ReactionIconPack","silly")}},
    {"hug",          0x1F917, false, {QT_TRANSLATE_NOOP("ReactionIconPack","hug"), QT_TRANSLATE_NOOP("ReactionIconPack","hugging")}},
    {"thinking",     0x1F914, false, {QT_TRANSLATE_NOOP("ReactionIconPack","thinking"), QT_TRANSLATE_NOOP("ReactionIconPack","hmm")}},
    {"neutral",      0x1F610, false, {QT_TRANSLATE_NOOP("ReactionIconPack","neutral"), QT_TRANSLATE_NOOP("ReactionIconPack","meh")}},
    {"sleeping",     0x1F634, false, {QT_TRANSLATE_NOOP("ReactionIconPack","sleeping"), QT_TRANSLATE_NOOP("ReactionIconPack","tired")}},
    {"sunglasses",   0x1F60E, false, {QT_TRANSLATE_NOOP("ReactionIconPack","cool"), QT_TRANSLATE_NOOP("ReactionIconPack","sunglasses")}},
    {"party-face",   0x1F973, false, {QT_TRANSLATE_NOOP("ReactionIconPack","party"), QT_TRANSLATE_NOOP("ReactionIconPack","celebrate")}},
    {"sob",          0x1F62D, false, {QT_TRANSLATE_NOOP("ReactionIconPack","sob"), QT_TRANSLATE_NOOP("ReactionIconPack","crying"), QT_TRANSLATE_NOOP("ReactionIconPack","sad")}},
    {"scream",       0x1F631, false, {QT_TRANSLATE_NOOP("ReactionIconPack","scream"), QT_TRANSLATE_NOOP("ReactionIconPack","shocked"), QT_TRANSLATE_NOOP("ReactionIconPack","fear")}},
    {"rage",         0x1F621, false, {QT_TRANSLATE_NOOP("ReactionIconPack","angry"), QT_TRANSLATE_NOOP("ReactionIconPack","rage"), QT_TRANSLATE_NOOP("ReactionIconPack","mad")}},
    {"mind-blown",   0x1F92F, false, {QT_TRANSLATE_NOOP("ReactionIconPack","mind blown"), QT_TRANSLATE_NOOP("ReactionIconPack","wow")}},
    {"thumbsdown",   0x1F44E, false, {QT_TRANSLATE_NOOP("ReactionIconPack","thumbs down"), QT_TRANSLATE_NOOP("ReactionIconPack","dislike"), QT_TRANSLATE_NOOP("ReactionIconPack","no")}},
    {"clap",         0x1F44F, false, {QT_TRANSLATE_NOOP("ReactionIconPack","clap"), QT_TRANSLATE_NOOP("ReactionIconPack","applause"), QT_TRANSLATE_NOOP("ReactionIconPack","well done")}},
    {"raised-hands", 0x1F64C, false, {QT_TRANSLATE_NOOP("ReactionIconPack","raised hands"), QT_TRANSLATE_NOOP("ReactionIconPack","celebration"), QT_TRANSLATE_NOOP("ReactionIconPack","yay")}},
    {"ok-hand",      0x1F44C, false, {QT_TRANSLATE_NOOP("ReactionIconPack","ok"), QT_TRANSLATE_NOOP("ReactionIconPack","ok hand"), QT_TRANSLATE_NOOP("ReactionIconPack","perfect")}},
    {"victory",      0x270C,  false, {QT_TRANSLATE_NOOP("ReactionIconPack","victory"), QT_TRANSLATE_NOOP("ReactionIconPack","peace")}},
    {"handshake",    0x1F91D, false, {QT_TRANSLATE_NOOP("ReactionIconPack","handshake"), QT_TRANSLATE_NOOP("ReactionIconPack","deal"), QT_TRANSLATE_NOOP("ReactionIconPack","agreement")}},
    {"muscle",       0x1F4AA, false, {QT_TRANSLATE_NOOP("ReactionIconPack","muscle"), QT_TRANSLATE_NOOP("ReactionIconPack","strong"), QT_TRANSLATE_NOOP("ReactionIconPack","flex")}},
    {"tada",         0x1F389, false, {QT_TRANSLATE_NOOP("ReactionIconPack","tada"), QT_TRANSLATE_NOOP("ReactionIconPack","party popper"), QT_TRANSLATE_NOOP("ReactionIconPack","celebrate")}},
    {"birthday",     0x1F382, false, {QT_TRANSLATE_NOOP("ReactionIconPack","birthday"), QT_TRANSLATE_NOOP("ReactionIconPack","cake")}},
    {"gift",         0x1F381, false, {QT_TRANSLATE_NOOP("ReactionIconPack","gift"), QT_TRANSLATE_NOOP("ReactionIconPack","present")}},
    {"hundred",      0x1F4AF, false, {QT_TRANSLATE_NOOP("ReactionIconPack","100"), QT_TRANSLATE_NOOP("ReactionIconPack","hundred"), QT_TRANSLATE_NOOP("ReactionIconPack","perfect score")}},
    {"check",        0x2705,  false, {QT_TRANSLATE_NOOP("ReactionIconPack","check"), QT_TRANSLATE_NOOP("ReactionIconPack","done"), QT_TRANSLATE_NOOP("ReactionIconPack","correct")}},
    {"cross",        0x274C,  false, {QT_TRANSLATE_NOOP("ReactionIconPack","cross"), QT_TRANSLATE_NOOP("ReactionIconPack","wrong"), QT_TRANSLATE_NOOP("ReactionIconPack","no")}},
    {"star",         0x2B50,  false, {QT_TRANSLATE_NOOP("ReactionIconPack","star"), QT_TRANSLATE_NOOP("ReactionIconPack","favorite")}},
    {"broken-heart", 0x1F494, false, {QT_TRANSLATE_NOOP("ReactionIconPack","broken heart"), QT_TRANSLATE_NOOP("ReactionIconPack","heartbroken"), QT_TRANSLATE_NOOP("ReactionIconPack","sad")}},
    {"smiling-imp",  0x1F608, false, {QT_TRANSLATE_NOOP("ReactionIconPack","devil"), QT_TRANSLATE_NOOP("ReactionIconPack","mischief")}},
    {"eyes",         0x1F440, false, {QT_TRANSLATE_NOOP("ReactionIconPack","eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","look"), QT_TRANSLATE_NOOP("ReactionIconPack","watching")}},
    {"clown",        0x1F921, false, {QT_TRANSLATE_NOOP("ReactionIconPack","clown"), QT_TRANSLATE_NOOP("ReactionIconPack","joke")}},
    {"poop",         0x1F4A9, false, {QT_TRANSLATE_NOOP("ReactionIconPack","poop"), QT_TRANSLATE_NOOP("ReactionIconPack","funny")}},
    {"rocket",       0x1F680, false, {QT_TRANSLATE_NOOP("ReactionIconPack","rocket"), QT_TRANSLATE_NOOP("ReactionIconPack","launch"), QT_TRANSLATE_NOOP("ReactionIconPack","fast")}},
    {"smirk",        0x1F60F, false, {QT_TRANSLATE_NOOP("ReactionIconPack","smirk"), QT_TRANSLATE_NOOP("ReactionIconPack","smug")}},

    // Four added to bring the pack to 54, which is exactly the emoji picker's 9x6 grid -- at 50 it
    // left four empty cells in the last row. Two faces and one hand fill the most obvious gaps in
    // the existing set; sparkles joins fire/star/hundred as a non-figurative "reaction" symbol.
    {"roll-eyes",    0x1F644, false, {QT_TRANSLATE_NOOP("ReactionIconPack","roll eyes"), QT_TRANSLATE_NOOP("ReactionIconPack","eyeroll"), QT_TRANSLATE_NOOP("ReactionIconPack","whatever")}},
    {"grimacing",    0x1F62C, false, {QT_TRANSLATE_NOOP("ReactionIconPack","grimace"), QT_TRANSLATE_NOOP("ReactionIconPack","awkward"), QT_TRANSLATE_NOOP("ReactionIconPack","yikes")}},
    {"sparkles",     0x2728,  false, {QT_TRANSLATE_NOOP("ReactionIconPack","sparkles"), QT_TRANSLATE_NOOP("ReactionIconPack","shiny"), QT_TRANSLATE_NOOP("ReactionIconPack","magic")}},
    {"crossed-fingers", 0x1F91E, false, {QT_TRANSLATE_NOOP("ReactionIconPack","crossed fingers"), QT_TRANSLATE_NOOP("ReactionIconPack","good luck"), QT_TRANSLATE_NOOP("ReactionIconPack","hope")}},
};
// clang-format on

constexpr size_t rawIconCount=sizeof(rawIcons)/sizeof(rawIcons[0]);

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

} // anonymous namespace

//--------------------------------------------------------------------------

class DefaultReactionIconPack::Pimpl
{
    public:

        std::vector<ReactionIconInfo> icons;
        std::map<QString,size_t> idToIndex;
        std::map<QString,size_t> codeToIndex; //!< emojiCode -> icon index, see findByCode()
        std::vector<std::pair<QString,size_t>> prefixIndex; //!< sorted (foldedWord, iconIndex)

        void build()
        {
            icons.clear();
            idToIndex.clear();
            codeToIndex.clear();
            icons.reserve(rawIconCount);

            for (size_t i=0; i<rawIconCount; ++i)
            {
                const auto& raw=rawIcons[i];

                ReactionIconInfo info;
                info.iconId=QString::fromLatin1(raw.iconId);
                info.icon=Style::instance().svgIconLocator().icon(
                    QStringLiteral("ChatReactionPack::%1").arg(info.iconId)
                );
                for (const auto* keyword : raw.keywords)
                {
                    info.keywords << QCoreApplication::translate("ReactionIconPack",keyword);
                }
                info.emojiCode=QString::fromUcs4(&raw.codepoint,1);

                idToIndex.emplace(info.iconId,icons.size());
                // emplace, not operator[] -- a duplicate code (none among the current 50) leaves
                // the first-listed entry winning, matching findByCode()'s documented contract.
                codeToIndex.emplace(info.emojiCode,icons.size());
                icons.push_back(std::move(info));
            }

            buildPrefixIndex();
        }

        void buildPrefixIndex()
        {
            prefixIndex.clear();

            for (size_t i=0; i<rawIconCount; ++i)
            {
                const auto& raw=rawIcons[i];

                // The icon id itself is always a match target ("thumbsup" finds thumbsup even
                // though no keyword literally repeats it).
                indexKeyword(prefixIndex,QString::fromLatin1(raw.iconId),i);

                for (const auto* keyword : raw.keywords)
                {
                    // Insert BOTH the raw English literal and its current translation, so an
                    // English search term still works in a localized UI (see
                    // AbstractReactionIconPack::search()'s doc comment).
                    indexKeyword(prefixIndex,QString::fromUtf8(keyword),i);
                    indexKeyword(prefixIndex,QCoreApplication::translate("ReactionIconPack",keyword),i);
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
    for (size_t i=0; i<rawIconCount && rawIcons[i].basic; ++i)
    {
        result.push_back(pimpl->icons[i].iconId);
    }
    return result;
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
