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

/** @file uise/desktop/reactioniconpack.hpp
*
*  Declares AbstractReactionIconPack, DefaultReactionIconPack and ReactionIconPacks.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_REACTIONICONPACK_HPP
#define UISE_DESKTOP_REACTIONICONPACK_HPP

#include <memory>
#include <vector>

#include <QString>
#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr()/QT_TRANSLATE_NOOP calls in this file under
// an unqualified context that does not match what moc (a real preprocessor) resolves at runtime
// -- translations for every keyword here would silently stay in English. Do not revert to the
// macro form. See chatfileitem.hpp for the same rule applied to a widget-facing header.
namespace uise {

class SvgIcon;

/**
 * @brief One icon in a reaction pack, resolved and ready to hand to a ChatReaction.
 */
struct UISE_DESKTOP_EXPORT ReactionIconInfo
{
    QString iconId;                    //!< Half of ChatReactionId::make() -- unique within the pack.
    std::shared_ptr<SvgIcon> icon;
    QStringList keywords;              //!< Localized search keywords, see AbstractReactionIconPack::search().

    /**
     * @brief The Unicode emoji character this icon corresponds to, e.g. "\U0001F44D" for
     *  "thumbsup" -- the literal, directly-insertable glyph, not a shortcode or hex string.
     *
     * Empty for an icon with no standard emoji equivalent (a pack is free to ship icons Unicode
     * has no codepoint for). Not guaranteed unique pack-wide the way iconId is -- see
     * AbstractReactionIconPack::findByCode()'s own doc comment on why a duplicate silently wins
     * whichever entry the pack lists first, exactly like iconId already does for id().
     *
     * Meant for future markdown/rich-text rendering that embeds this SAME pack's icons inline in
     * message text, either by literal emoji character (looked up here to find the matching pack
     * graphic) or by an explicit "<packUri>@<iconId>"-style reference (ChatReactionId already
     * provides that exact scheme) -- uise-desktop does not implement that rendering itself yet.
     */
    QString emojiCode;
};

/**
 * @brief Provider seam between a reaction icon widget (ChatMessageReactionChip,
 *  ChatReactionGallery, ...) and wherever a pack's icons and keywords actually come from.
 *
 * uise-desktop ships DefaultReactionIconPack, backed by the embedded Noto Emoji subset
 * (thirdparty/noto-emoji/) and resolved through the normal SvgIconLocator alias mechanism. A host
 * that later wants a db-backed, downloadable pack (see the reactions task spec's icon pack
 * registries) implements this interface itself and registers it with ReactionIconPacks --
 * uise-desktop never needs to know what a pack controller is.
 */
class UISE_DESKTOP_EXPORT AbstractReactionIconPack
{
    public:

        virtual ~AbstractReactionIconPack() = default;

        AbstractReactionIconPack(const AbstractReactionIconPack&) = delete;
        AbstractReactionIconPack(AbstractReactionIconPack&&) = delete;
        AbstractReactionIconPack& operator=(const AbstractReactionIconPack&) = delete;
        AbstractReactionIconPack& operator=(AbstractReactionIconPack&&) = delete;

        //! Pack identity -- the URI half of a reaction id (ChatReactionId::packUri()). Empty
        //! means "the default pack" (see ReactionIconPacks::defaultPack()).
        virtual QString uri() const =0;

        virtual size_t count() const =0;

        /**
         * @brief Get an icon by its position in the pack, in the pack's own display order.
         * @return nullptr if index is out of range.
         */
        virtual const ReactionIconInfo* at(size_t index) const =0;

        /**
         * @brief Look an icon up by id.
         * @return nullptr if not found.
         */
        virtual const ReactionIconInfo* find(const QString& iconId) const =0;

        /**
         * @brief Look an icon up by its ReactionIconInfo::emojiCode.
         * @param emojiCode The literal Unicode emoji character, as stored in emojiCode() --
         *  not a shortcode, not a hex codepoint string.
         * @return nullptr if not found (including for an empty emojiCode, or one no icon in this
         *  pack carries). If more than one icon happens to share the same emojiCode, the pack's
         *  first one (in at()'s own display order) wins -- the same "first match wins" rule
         *  find() already applies to iconId, and for the same reason: this is a fast reverse
         *  lookup for a UI/text-scanning need, not a uniqueness guarantee the pack enforces.
         */
        virtual const ReactionIconInfo* findByCode(const QString& emojiCode) const =0;

        /**
         * @brief Search icons by keyword PREFIX (see ChatReactionGallery's search box).
         * @param prefix Case-insensitive prefix, matched against each keyword's individual words
         *  (so "heart" matches the keyword "red heart") as well as the plain iconId.
         * @return Indices into at(), in the pack's own display order (not match-quality order --
         *  reordering icons under the cursor as the user types would be disorienting). Empty
         *  prefix returns every index, in order.
         */
        virtual std::vector<size_t> search(const QString& prefix) const =0;

        //! The pack's own recommended "7 basic" icons for the gallery's collapsed quick bar and
        //! the initial "recently used" row, in display order.
        virtual std::vector<QString> basicIconIds() const =0;

        //! Rebuild any translated keyword index after a QEvent::LanguageChange. Default no-op --
        //! only meaningful for a pack whose keywords are actually localized (DefaultReactionIconPack).
        virtual void retranslate() {}

    protected:

        AbstractReactionIconPack() = default;
};

/**
 * @brief The embedded default reaction icon pack -- Noto Emoji SVGs (thirdparty/noto-emoji/),
 *  resolved through the "ChatReactionPack" icon alias context (resources/style/chatreactions.json).
 *
 * uri() is empty: this is THE default pack referred to by an empty pack-URI half of a reaction
 * id, and by ReactionIconPacks::defaultPack() before any host registers another.
 */
class UISE_DESKTOP_EXPORT DefaultReactionIconPack : public AbstractReactionIconPack
{
    public:

        DefaultReactionIconPack();
        ~DefaultReactionIconPack() override;

        DefaultReactionIconPack(const DefaultReactionIconPack&) = delete;
        DefaultReactionIconPack(DefaultReactionIconPack&&) = delete;
        DefaultReactionIconPack& operator=(const DefaultReactionIconPack&) = delete;
        DefaultReactionIconPack& operator=(DefaultReactionIconPack&&) = delete;

        QString uri() const override;
        size_t count() const override;
        const ReactionIconInfo* at(size_t index) const override;
        const ReactionIconInfo* find(const QString& iconId) const override;
        const ReactionIconInfo* findByCode(const QString& emojiCode) const override;
        std::vector<size_t> search(const QString& prefix) const override;
        std::vector<QString> basicIconIds() const override;
        void retranslate() override;

    private:

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl;
};

/**
 * @brief Filtering VIEW over another pack, exposing only entries whose
 *  ReactionIconInfo::emojiCode is non-empty.
 *
 * What MessageEditingMode::Markdown needs: that mode's document is markdown SOURCE, so the only
 * thing an emoji pick can insert there is the literal Unicode character -- an icon carrying no
 * code has simply nothing to insert, and offering it in the gallery would produce a click that
 * silently does nothing.
 *
 * Implemented as a decorator rather than as a flag on ChatReactionGallery deliberately.
 * search() is documented to return indices into at(), and the gallery relies on exactly that
 * (search() then at(index)); a flag would have to filter the RETURNED index vector while still
 * indexing the UNFILTERED at(), putting "which indices are legal" in two classes at once.
 * Filtering the pack instead keeps that invariant true by construction, and covers the gallery's
 * three independent pack readers (the grid, the "recently used" bar, and the quick bar, which
 * goes through basicIconIds()+find() rather than search()) in one place.
 *
 * Against the shipped DefaultReactionIconPack this is currently an identity view -- all 50 icons
 * carry a code. It exists because ReactionIconPacks::setDefaultPackUri() lets a host REPLACE the
 * default pack, and a replacement is free to ship codeless icons.
 */
class UISE_DESKTOP_EXPORT EmojiCodeReactionIconPack : public AbstractReactionIconPack
{
    public:

        /**
         * @brief Constructor.
         * @param source Pack to filter. Held by shared_ptr, so entry pointers returned by at()/
         *  find()/findByCode() (which point into the SOURCE's own storage) stay valid.
         */
        explicit EmojiCodeReactionIconPack(std::shared_ptr<AbstractReactionIconPack> source);

        ~EmojiCodeReactionIconPack() override;

        EmojiCodeReactionIconPack(const EmojiCodeReactionIconPack&) = delete;
        EmojiCodeReactionIconPack(EmojiCodeReactionIconPack&&) = delete;
        EmojiCodeReactionIconPack& operator=(const EmojiCodeReactionIconPack&) = delete;
        EmojiCodeReactionIconPack& operator=(EmojiCodeReactionIconPack&&) = delete;

        //! The SOURCE's uri -- a filtered view is still the same pack, and a reaction id built
        //! from an icon picked here must resolve through ReactionIconPacks exactly as one
        //! picked from the unfiltered pack does.
        QString uri() const override;

        size_t count() const override;
        const ReactionIconInfo* at(size_t index) const override;
        const ReactionIconInfo* find(const QString& iconId) const override;
        const ReactionIconInfo* findByCode(const QString& emojiCode) const override;
        std::vector<size_t> search(const QString& prefix) const override;
        std::vector<QString> basicIconIds() const override;
        void retranslate() override;

        //! The pack being filtered.
        std::shared_ptr<AbstractReactionIconPack> source() const;

    private:

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl;
};

/**
 * @brief Process-wide registry of reaction icon packs, resolving a ChatReaction's icon when the
 *  reaction itself does not carry one (see ChatReaction::icon()).
 *
 * Mirrors Style::instance()'s singleton shape. DefaultReactionIconPack is pre-registered under
 * the empty URI; a host registers additional packs (or replaces the default one) via
 * registerPack().
 */
class UISE_DESKTOP_EXPORT ReactionIconPacks
{
    public:

        static ReactionIconPacks& instance();

        ReactionIconPacks(const ReactionIconPacks&) = delete;
        ReactionIconPacks(ReactionIconPacks&&) = delete;
        ReactionIconPacks& operator=(const ReactionIconPacks&) = delete;
        ReactionIconPacks& operator=(ReactionIconPacks&&) = delete;

        //! Register a pack under its own uri(), replacing any pack previously registered under
        //! the same uri().
        void registerPack(std::shared_ptr<AbstractReactionIconPack> pack);

        //! @return The pack registered under this uri(), or nullptr if none is.
        std::shared_ptr<AbstractReactionIconPack> pack(const QString& uri) const;

        //! @return The pack registered under the empty uri() -- see setDefaultPackUri().
        std::shared_ptr<AbstractReactionIconPack> defaultPack() const;

        //! Change which registered pack's uri() is treated as "the default" by defaultPack().
        //! The pack itself must already be registered under this uri() via registerPack().
        void setDefaultPackUri(const QString& uri);

        /**
         * @brief Resolve a reaction id to its full pack entry.
         * @param reactionId "<icon id>@<pack URI>" (or a bare icon id -- see ChatReactionId).
         * @return The matching entry, or nullptr if the pack or the icon id within it is not
         *  registered/found. The pointer is owned by the pack and stays valid as long as the
         *  pack is registered and not retranslate()d.
         *
         * icon() below is a thin wrapper over this. Inline emoji rendering needs the whole entry
         * rather than just the icon -- emojiCode in particular, both to write an <img>'s alt text
         * and to substitute a default-pack image back to a plain character on export (see
         * emojiUrlScheme() in markdownrenderer.hpp).
         */
        const ReactionIconInfo* iconInfo(const QString& reactionId) const;

        /**
         * @brief Resolve a reaction id's icon.
         * @param reactionId "<icon id>@<pack URI>" (or a bare icon id -- see ChatReactionId).
         * @return icon() from the matching pack/entry, or nullptr if the pack or the icon id
         *  within it is not registered/found.
         */
        std::shared_ptr<SvgIcon> icon(const QString& reactionId) const;

        void retranslateAll();

    private:

        ReactionIconPacks();
        ~ReactionIconPacks();

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl;
};

}

#endif // UISE_DESKTOP_REACTIONICONPACK_HPP
