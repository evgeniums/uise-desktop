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

/** @file uise/desktop/chatmessagereactions.hpp
*
*  Declares ChatMessageReactionChip, ChatMessageReactionsRow and ChatMessageReactions.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEREACTIONS_HPP
#define UISE_DESKTOP_CHATMESSAGEREACTIONS_HPP

#include <memory>
#include <vector>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class RoundedImage;
class AvatarWidget;

/**
 * @brief One reaction chip -- an icon plus either up to 3 small avatars or a numeric count (see
 *  ChatReactionDisplayMode), or, in "ellipsis" mode, just an overflow "..." glyph.
 *
 * A plain clickable Frame, no ripple/animation -- the reactions task spec's first iteration is
 * "static icons without any animations". Click handling mirrors IconTextButton's own
 * press-then-release-inside pattern (see icontextbutton.cpp's mousePressEvent()/
 * mouseReleaseEvent()): a press alone never fires clicked(), only a release still inside the
 * chip's rect does, so a press dragged out cancels it like every other button in this library.
 */
class UISE_DESKTOP_EXPORT ChatMessageReactionChip : public Frame
{
    Q_OBJECT

    Q_PROPERTY(int avatarRingBorderWidth READ avatarRingBorderWidth WRITE setAvatarRingBorderWidth)

    public:

        explicit ChatMessageReactionChip(QWidget* parent=nullptr);
        ~ChatMessageReactionChip() override;

        ChatMessageReactionChip(const ChatMessageReactionChip&) = delete;
        ChatMessageReactionChip(ChatMessageReactionChip&&) = delete;
        ChatMessageReactionChip& operator=(const ChatMessageReactionChip&) = delete;
        ChatMessageReactionChip& operator=(ChatMessageReactionChip&&) = delete;

        /**
         * @brief Set the reaction this chip represents.
         *
         * If reaction.icon() is null, the icon is resolved through
         * ReactionIconPacks::instance().icon(reaction.id()) instead -- see chatreaction.hpp's own
         * doc comment on ChatReaction::icon().
         */
        void setReaction(const ChatReaction& reaction);

        const ChatReaction& reaction() const noexcept
        {
            return m_reaction;
        }

        //! Which of avatars/count this chip shows -- see ChatReactionDisplayMode. Ignored while
        //! isEllipsis() is true.
        void setDisplayMode(ChatReactionDisplayMode mode);

        ChatReactionDisplayMode displayMode() const noexcept
        {
            return m_mode;
        }

        /**
         * @brief Turn this chip into the trailing "..." overflow indicator.
         * @param enable When true, the icon/count/avatars are all hidden and only a "more"
         *  glyph (ChatReactions::more) is shown; reaction()/setReaction() are then ignored for
         *  display purposes (kept only so the row can still track identity/click bookkeeping).
         *  clicked() is still emitted the same way -- ChatMessageReactionsRow is what tells the
         *  two cases apart (see its own moreRequested()/toggleRequested() signals).
         */
        void setEllipsis(bool enable);

        bool isEllipsis() const noexcept
        {
            return m_ellipsis;
        }

        /**
         * @brief Enable/disable click handling.
         * @param enable False makes the chip fully mouse-transparent (WA_TransparentForMouseEvents)
         *  so a click passes through to whatever is behind it -- e.g. a chat message in
         *  multi-select mode, where a tap anywhere on the bubble toggles selection instead of
         *  acting on the chip underneath it (see AbstractChatMessage::mousePressEvent()).
         */
        void setInteractive(bool enable);

        bool isInteractive() const noexcept
        {
            return m_interactive;
        }

        /**
         * @brief Width of the solid backdrop ring painted behind each avatar (see
         *  updateAvatars()'s own comment on why overlapping same-coloured avatars need one at
         *  all), in pixels on each side. Default 1.
         *
         * Deliberately QSS-tunable (qproperty-avatarRingBorderWidth) like this row's sibling
         * knobs -- unlike those, changing it also changes each ring's effective SIZE (icon size
         * plus 2x this value), so updateAvatars() recomputes and re-applies the ring's own
         * border-radius in C++ every time rather than relying on a QSS border-radius rule that
         * would otherwise have to be kept in sync with this property by hand.
         */
        void setAvatarRingBorderWidth(int value);

        int avatarRingBorderWidth() const noexcept
        {
            return m_avatarRingBorderWidth;
        }

    Q_SIGNALS:

        void clicked();

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void setHovered(bool enable);
        void updateVisibility();
        void updateAvatars();

        ChatReaction m_reaction;
        ChatReactionDisplayMode m_mode=ChatReactionDisplayMode::Counts;
        bool m_ellipsis=false;
        bool m_interactive=true;
        bool m_pressed=false;
        int m_avatarRingBorderWidth=1;

        RoundedImage* m_icon;
        QLabel* m_count;
        QFrame* m_avatarsFrame;
        std::vector<AvatarWidget*> m_avatarWidgets; // reused across setReaction() calls, <= 3
        // One ring frame per avatar, same pool discipline -- see updateAvatars()'s own comment
        // on why each ring is a separate sibling widget rather than a border on the avatar
        // itself, and why creation ORDER (ring before its own avatar) is load-bearing.
        std::vector<QFrame*> m_avatarRings;
        RoundedImage* m_moreIcon;
};

/**
 * @brief The wrappable row of reaction chips shown at the bottom of a chat message bubble.
 *
 * No QLayout -- chips are positioned with manual QRects from flowPack() (utils/flowpack.hpp),
 * matching ChatMessageImages' own manual-rect idiom for the same reason (see flowPack()'s own
 * doc comment on why a QLayout does not fit this codebase's bubble negotiation pipeline).
 *
 * Standalone and reusable outside a chat bubble -- ChatMessageReactions (the
 * AbstractChatMessageReactions section) is a thin wrapper around exactly one of these.
 */
class UISE_DESKTOP_EXPORT ChatMessageReactionsRow : public Frame
{
    Q_OBJECT

    Q_PROPERTY(int maxVisibleChips READ maxVisibleChips WRITE setMaxVisibleChips)
    Q_PROPERTY(int maxAvatarChipTypes READ maxAvatarChipTypes WRITE setMaxAvatarChipTypes)
    Q_PROPERTY(int maxAvatarsPerChip READ maxAvatarsPerChip WRITE setMaxAvatarsPerChip)
    Q_PROPERTY(int chipSpacing READ chipSpacing WRITE setChipSpacing)
    Q_PROPERTY(int rowSpacing READ rowSpacing WRITE setRowSpacing)
    Q_PROPERTY(int tailGap READ tailGap WRITE setTailGap)

    public:

        //! How ChatReactionDisplayMode is picked -- see setReactions()'s own doc comment for the
        //! Auto policy's exact rule.
        enum class DisplayModePolicy : uint8_t
        {
            Auto,
            ForceAvatars,
            ForceCounts
        };

        explicit ChatMessageReactionsRow(QWidget* parent=nullptr);
        ~ChatMessageReactionsRow() override;

        ChatMessageReactionsRow(const ChatMessageReactionsRow&) = delete;
        ChatMessageReactionsRow(ChatMessageReactionsRow&&) = delete;
        ChatMessageReactionsRow& operator=(const ChatMessageReactionsRow&) = delete;
        ChatMessageReactionsRow& operator=(ChatMessageReactionsRow&&) = delete;

        /**
         * @brief Replace the reaction set and repack the row for the current width.
         *
         * The avatars-vs-count presentation (ChatReactionDisplayMode) is decided ONCE here for
         * every chip uniformly, from displayModePolicy():
         *  - ForceAvatars/ForceCounts always win outright.
         *  - Auto (the default) picks Avatars only when ALL of: reactions.size() is at most
         *    maxAvatarChipTypes(), every reaction's count() is at most maxAvatarsPerChip(), AND
         *    every reaction actually carries avatars() (a host that has counts but has not
         *    resolved avatar data yet must not render blank/placeholder avatar circles) --
         *    otherwise Counts.
         */
        void setReactions(ChatReactions reactions);

        const ChatReactions& reactions() const noexcept
        {
            return m_reactions;
        }

        void setDisplayModePolicy(DisplayModePolicy policy);

        DisplayModePolicy displayModePolicy() const noexcept
        {
            return m_displayModePolicy;
        }

        //! The mode actually in effect for the current reactions() -- meaningful only after at
        //! least one setReactions() call.
        ChatReactionDisplayMode displayMode() const noexcept
        {
            return m_displayMode;
        }

        /**
         * @brief Repack for a given width budget and return the packed width.
         * @param maxWidth Width budget, forwarded to FlowPackOptions::maxWidth.
         * @param reservedTailWidth Width to keep free at the end of the last chip row, forwarded
         *  to FlowPackOptions::reservedTailWidth -- see tailFits().
         * @return The packed content width (<=maxWidth), i.e. FlowPackResult::totalSize.width().
         *
         * A no-op re-measurement when nothing has changed since the last call at this exact
         * (maxWidth,reservedTailWidth) pair and the same reactions()/DPR -- see the class's own
         * repack-pin comment in chatmessagereactions.cpp.
         */
        int packForWidth(int maxWidth, int reservedTailWidth=0);

        //! Bounding rect of the last packed row, in this widget's own coordinates (i.e. already
        //! offset by contentsMargins()) -- see AbstractChatMessageReactions::lastTextLineRect().
        //! Invalid before the first packForWidth() call.
        QRect lastRowRect() const noexcept
        {
            return m_lastRowRect;
        }

        //! Size of the packed content, contentsMargins() included -- what sizeHint() reports.
        QSize packedSize() const noexcept
        {
            return m_packedSize;
        }

        //! False when the last packForWidth()'s reservedTailWidth could not be honoured -- see
        //! FlowPackResult::tailFits.
        bool tailFits() const noexcept
        {
            return m_tailFits;
        }

        bool isEmpty() const noexcept
        {
            return m_reactions.empty();
        }

        //! Propagated to every chip -- see ChatMessageReactionChip::setInteractive().
        void setInteractive(bool enable);

        int maxVisibleChips() const noexcept { return m_maxVisibleChips; }
        void setMaxVisibleChips(int value);

        int maxAvatarChipTypes() const noexcept { return m_maxAvatarChipTypes; }
        void setMaxAvatarChipTypes(int value);

        int maxAvatarsPerChip() const noexcept { return m_maxAvatarsPerChip; }
        void setMaxAvatarsPerChip(int value);

        int chipSpacing() const noexcept { return m_chipSpacing; }
        void setChipSpacing(int value);

        int rowSpacing() const noexcept { return m_rowSpacing; }
        void setRowSpacing(int value);

        //! Extra width reserved for the caller's own trailing content, applied on TOP of whatever
        //! reservedTailWidth is passed to packForWidth() -- see FlowPackOptions::reservedTailWidth.
        int tailGap() const noexcept { return m_tailGap; }
        void setTailGap(int value);

        QSize sizeHint() const override;

    Q_SIGNALS:

        //! A non-ellipsis chip was clicked. currentlyOwn is the reaction's OWN state as currently
        //! rendered (i.e. after any not-yet-confirmed optimistic toggle already applied by this
        //! row -- see setReactions()'s doc comment on why this row is deliberately NOT stateless).
        //! There is no timeout/rollback here beyond the next setReactions() call: a host that
        //! never calls it back leaves the optimistic state showing, which is the correct failure
        //! mode for a pure view.
        void toggleRequested(const QString& reactionId, bool currentlyOwn);

        //! The trailing "..." overflow chip was clicked.
        void moreRequested();

    protected:

        /**
         * @brief Repack for the widget's own current width.
         *
         * Only the path used when this row is driven by ordinary Qt geometry (the standalone
         * demo, or any host that just places it in a normal layout) -- a bubble-hosted
         * ChatMessageReactions instead calls packForWidth() explicitly with a width the
         * negotiation pipeline computed, since by the time THAT call happens this widget may not
         * even have been resize()'d to its final width yet.
         */
        void resizeEvent(QResizeEvent* event) override;

    private:

        void repack();
        void onChipClicked(ChatMessageReactionChip* chip);
        void applyOptimisticToggle(const QString& reactionId);
        void ensureChipCount(size_t count);

        ChatReactions m_reactions;
        DisplayModePolicy m_displayModePolicy=DisplayModePolicy::Auto;
        ChatReactionDisplayMode m_displayMode=ChatReactionDisplayMode::Counts;

        std::vector<ChatMessageReactionChip*> m_chips; // reused; m_chips.back() is the ellipsis
                                                        // chip when it is currently shown
        bool m_ellipsisShown=false;

        int m_maxVisibleChips=0;
        int m_maxAvatarChipTypes=2;
        int m_maxAvatarsPerChip=3;
        int m_chipSpacing=6;
        int m_rowSpacing=4;
        int m_tailGap=10;

        QRect m_lastRowRect;
        QSize m_packedSize;
        bool m_tailFits=true;
        bool m_interactive=true;

        // repack() memoization -- see chatmessagereactions.cpp's own comment, mirroring
        // ChatMessageImages' lastLayoutForMaxWidth/lastLayoutDpr pin.
        int m_packedForWidth=-1;
        int m_packedReservedTailWidth=-1;
        qreal m_packedDpr=0.0;
        size_t m_packedReactionsRevision=0;
        size_t m_reactionsRevision=0;
};

/**
 * @brief The chat bubble's reactions section -- the 6th ChatMessageContentSection slot, between
 *  comment() and bottom() (see AbstractChatMessageContent::setReactions()/rebuildSections()).
 *
 * A thin AbstractChatMessageReactions wrapper around one ChatMessageReactionsRow, implementing
 * the negotiation overrides the way ChatMessageImages implements its own (bubbleWidthHint(),
 * updateMaximumBubbleWidth(), lastTextLineRect(), ownWidthCeiling()).
 */
class UISE_DESKTOP_EXPORT ChatMessageReactions : public AbstractChatMessageReactions
{
    Q_OBJECT

    Q_PROPERTY(int maxBubbleWidth READ maxBubbleWidth WRITE setMaxBubbleWidth)

    public:

        explicit ChatMessageReactions(QWidget* parent=nullptr);
        ~ChatMessageReactions() override;

        ChatMessageReactions(const ChatMessageReactions&) = delete;
        ChatMessageReactions(ChatMessageReactions&&) = delete;
        ChatMessageReactions& operator=(const ChatMessageReactions&) = delete;
        ChatMessageReactions& operator=(ChatMessageReactions&&) = delete;

        void setReactions(ChatReactions reactions) override;

        const ChatReactions& reactions() const override
        {
            return m_row->reactions();
        }

        bool isEmpty() const override
        {
            return m_row->isEmpty();
        }

        void setInteractive(bool enable) override;

        //! Own width ceiling -- see ChatMessageContentSection::ownWidthCeiling()'s doc comment on
        //! why the TRAILING section's ceiling must not be left unbounded. Defaults to 500 in
        //! chatreactions.qss, matching uise--AbstractChatMessageText's own cap.
        int maxBubbleWidth() const noexcept
        {
            return m_maxBubbleWidth;
        }

        void setMaxBubbleWidth(int value) noexcept
        {
            m_maxBubbleWidth=value;
        }

        int bubbleWidthHint(int forMaxWidth) override;
        void updateMaximumBubbleWidth() override;
        QRect lastTextLineRect() const override;
        int ownWidthCeiling() const override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

        ChatMessageReactionsRow* row() const noexcept
        {
            return m_row;
        }

    private:

        ChatMessageReactionsRow* m_row;
        int m_maxBubbleWidth=500;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEREACTIONS_HPP
