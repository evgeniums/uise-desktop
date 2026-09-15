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

/** @file uise/desktop/chatreactiongallery.hpp
*
*  Declares ChatReactionQuickBar, ChatReactionGallery and ChatReactionGalleryDropdown.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATREACTIONGALLERY_HPP
#define UISE_DESKTOP_CHATREACTIONGALLERY_HPP

#include <memory>
#include <vector>

#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/dropdownframe.hpp>

class QLabel;
class QGridLayout;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. See chatfileitem.hpp for the same rule
// applied to a widget-facing header.
namespace uise {

class PushButton;
class SearchLineEdit;
class ScrollArea;
class AbstractReactionIconPack;

/**
 * @brief Row of the pack's "7 basic" icons plus a chevron that expands to the full
 *  ChatReactionGallery -- the reactions task spec's "collapsed" gallery state. Reused, minus the
 *  chevron, as the expanded gallery's own "recently used" row (see setChevronVisible()).
 */
class UISE_DESKTOP_EXPORT ChatReactionQuickBar : public Frame
{
    Q_OBJECT

    public:

        explicit ChatReactionQuickBar(QWidget* parent=nullptr);
        ~ChatReactionQuickBar() override;

        ChatReactionQuickBar(const ChatReactionQuickBar&) = delete;
        ChatReactionQuickBar(ChatReactionQuickBar&&) = delete;
        ChatReactionQuickBar& operator=(const ChatReactionQuickBar&) = delete;
        ChatReactionQuickBar& operator=(ChatReactionQuickBar&&) = delete;

        //! Rebuilds the row from pack->basicIconIds(). Safe to call again (e.g. after
        //! retranslate() or a pack swap).
        void setPack(std::shared_ptr<AbstractReactionIconPack> pack);

        std::shared_ptr<AbstractReactionIconPack> pack() const noexcept
        {
            return m_pack;
        }

        //! Reaction ids the current user already has on this message -- their quick buttons are
        //! shown checked (see chatreactions.qss's QPushButton:checked rule).
        void setOwnReactionIds(QStringList ids);

        //! False (used for the "recently used" row inside ChatReactionGallery, which has no
        //! expand affordance of its own) hides #expandButton entirely. Default true.
        void setChevronVisible(bool enable);

        /**
         * @brief Show these bare icon ids at the FRONT of the row, ahead of the pack's own
         *  basicIconIds().
         *
         * Prepended, never a replacement: the basics stay and simply shift right, so a row that
         * has only ever seen one pick still shows a full row instead of collapsing to that single
         * icon. An id that is also one of the basics appears once, in its leading position --
         * dedup is on, and matters, because the basics are the seven emoji most likely to be
         * picked in the first place.
         *
         * Empty (the default) leaves the row exactly as it was before this existed: the basics
         * alone, in pack order. That is what the collapsed quick bar wants, and what the gallery's
         * recents row falls back to before any usage history exists.
         *
         * The composed row is capped at MaxRowIcons -- leading ids win the slots, so a long
         * enough history does eventually push the last basics off the right end. An id the
         * current pack cannot resolve is skipped, exactly as rebuild() already skips an
         * unresolvable basic, so a list persisted by a host under one pack degrades quietly under
         * another instead of leaving holes.
         *
         * Bare icon ids, NOT "iconId@packUri" reaction ids: the row resolves them through
         * pack()->find(), which is pack-relative. A host holding full ids passes them through
         * ChatReactionId::iconId() first.
         */
        void setLeadingIconIds(QStringList ids);

        QStringList leadingIconIds() const noexcept
        {
            return m_leadingIconIds;
        }

        //! Ceiling on the WHOLE composed row (leading ids plus basics). Matches the emoji
        //! gallery's own 9-column grid, so the recents row never outruns the grid beneath it.
        //! The collapsed quick bar is unaffected: it sets no leading ids and its pack ships
        //! seven basics.
        constexpr static const int MaxRowIcons=9;

    Q_SIGNALS:

        void reactionPicked(const QString& reactionId);
        void expandRequested();

    private:

        void rebuild();

        std::shared_ptr<AbstractReactionIconPack> m_pack;
        QStringList m_ownReactionIds;
        //! Shown ahead of m_pack->basicIconIds(), not instead of them -- see
        //! setLeadingIconIds().
        QStringList m_leadingIconIds;
        std::vector<PushButton*> m_buttons;
        PushButton* m_expandButton;
};

/**
 * @brief The expanded reaction gallery -- recently-used row, keyword search box, and a
 *  multi-row scrollable grid of every icon in the pack (task-chat-message-reactions.md's
 *  "expanded" gallery state).
 *
 * No virtualization: FlyweightListView is strictly 1-D and cannot lay out a grid, and a pack of a
 * few dozen to a few hundred PushButtons in a plain QGridLayout is trivial for Qt to lay out --
 * do not "optimize" this into a flyweight view later without an actual measured need.
 */
class UISE_DESKTOP_EXPORT ChatReactionGallery : public Frame
{
    Q_OBJECT

    Q_PROPERTY(int galleryColumns READ galleryColumns WRITE setGalleryColumns)
    Q_PROPERTY(int galleryVisibleRows READ galleryVisibleRows WRITE setGalleryVisibleRows)

    public:

        explicit ChatReactionGallery(QWidget* parent=nullptr);
        ~ChatReactionGallery() override;

        ChatReactionGallery(const ChatReactionGallery&) = delete;
        ChatReactionGallery(ChatReactionGallery&&) = delete;
        ChatReactionGallery& operator=(const ChatReactionGallery&) = delete;
        ChatReactionGallery& operator=(ChatReactionGallery&&) = delete;

        void setPack(std::shared_ptr<AbstractReactionIconPack> pack);

        std::shared_ptr<AbstractReactionIconPack> pack() const noexcept
        {
            return m_pack;
        }

        void setOwnReactionIds(QStringList ids);

        //! Clear the search box and re-show the whole pack.
        void resetSearch();

        int galleryColumns() const noexcept { return m_galleryColumns; }
        void setGalleryColumns(int value);

        //! Number of icon rows the scroll area shows before it starts scrolling. This is a real
        //! height clamp on the viewport -- without one the gallery is as tall as its whole pack.
        int galleryVisibleRows() const noexcept { return m_galleryVisibleRows; }
        void setGalleryVisibleRows(int value);

        /**
         * @brief Override the search box's placeholder text.
         * @param text New text, or an empty string to go back to the default tr("Search
         *  reactions").
         *
         * This widget is reused verbatim as an EMOJI picker (see EmojiGalleryDialog), where
         * every default string here names the wrong thing. Overriding beats forking the widget,
         * and beats renaming the defaults -- the reactions UI that already ships wants exactly
         * the wording it has.
         *
         * A QEvent::LanguageChange re-applies whatever was set here rather than reverting to the
         * default, so a host must re-set its own tr()'d text from its own changeEvent().
         */
        void setSearchPlaceholderText(const QString& text);
        QString searchPlaceholderText() const noexcept { return m_searchPlaceholderText; }

        //! Override the "nothing matched" label. Empty restores tr("No matching reactions").
        //! @see setSearchPlaceholderText()
        void setEmptyText(const QString& text);
        QString emptyText() const noexcept { return m_emptyText; }

        //! Override the recents section title. Empty restores tr("Recently used").
        //! @see setSearchPlaceholderText()
        void setRecentTitleText(const QString& text);
        QString recentTitleText() const noexcept { return m_recentTitleText; }

        /**
         * @brief Fill the "recently used" row from a host-supplied usage history.
         *
         * PREPENDED to the pack's basicIconIds(), never a replacement for them: the defaults stay
         * and shift right, so one pick cannot collapse the row to a single icon. Empty (the
         * default) leaves the row showing those basics alone, which is what it has always shown
         * -- this widget tracks no usage of its own and deliberately still does
         * not: it cannot know which of its picks a host actually acted on, nor where that host
         * would want a history kept. A host that wants real recents listens for reactionPicked(),
         * maintains its own most-recently-used list, and pushes it back here.
         *
         * Bare icon ids, most recent FIRST. Unresolvable ids are skipped rather than left as holes
         * in the row -- see ChatReactionQuickBar::setLeadingIconIds(), which this forwards to.
         * They are PREPENDED to the pack's basics, so the row keeps its default icons.
         */
        void setRecentIds(QStringList ids);
        QStringList recentIds() const;

    Q_SIGNALS:

        void reactionPicked(const QString& reactionId);

        //! This gallery's own natural size changed (a search filtered the grid to fewer/more
        //! rows, or the pack/recents were swapped) -- ChatReactionGalleryDropdown listens for
        //! this to call DropdownFrame::remeasure() while already open.
        void sizeChanged();

    protected:

        //! Rebuilds recents/search index on QEvent::LanguageChange -- AbstractReactionIconPack::
        //! retranslate() rebuilds the pack's own translated keyword index, this just re-runs the
        //! last search against it.
        void changeEvent(QEvent* event) override;

    private:

        void rebuildRecent();
        void rebuildGrid(const QString& searchPrefix);
        void onSearchTextChanged(const QString& text);
        PushButton* ensureGridCell(size_t index);

        //! Clamp the scroll viewport to galleryVisibleRows() rows, measured from a live cell's
        //! own size hint so the clamp tracks the theme rather than duplicating its numbers.
        void applyVisibleRowsHeight();

        std::shared_ptr<AbstractReactionIconPack> m_pack;
        QStringList m_ownReactionIds;

        // The task spec's "recently used reactions -- by default filled with 7 basic reactions":
        // this iteration has no actual usage-history tracking, so the recents row simply shows
        // the SAME pack->basicIconIds() the collapsed quick bar does -- a second
        // ChatReactionQuickBar instance (chevron hidden) is all that takes.
        QFrame* m_recentFrame;
        QLabel* m_recentTitle;
        ChatReactionQuickBar* m_recentBar;

        SearchLineEdit* m_searchEdit;
        ScrollArea* m_galleryScroll;
        QFrame* m_galleryGrid;
        QGridLayout* m_gridLayout;
        std::vector<PushButton*> m_gridCells; // pool, reused across rebuildGrid() calls
        QLabel* m_emptyLabel;

        int m_galleryColumns=8;
        int m_galleryVisibleRows=5;

        //! Host overrides for the three user-facing strings; empty means "use the default".
        //! @see setSearchPlaceholderText()
        QString m_searchPlaceholderText;
        QString m_emptyText;
        QString m_recentTitleText;
};

/**
 * @brief DropdownFrame hosting the collapsed ChatReactionQuickBar and expanded
 *  ChatReactionGallery as two pages of one popup, per the reactions task spec.
 *
 * The reactions task spec calls for this to "popup above chat message context menu" -- both
 * visible together, like a submenu flyout, not the menu closing first. DropdownMenu's own click
 * handling stands in the way of that for a PLAIN item: it always calls notifyActivated() right
 * after emitting itemTriggered() (see dropdownmenu.cpp), which closes chainRoot() and cascades to
 * every chained child -- chaining this gallery to a plain item's click would close it in the very
 * same call, before it ever gets to paint a frame.
 *
 * The fix is to make the menu's "React..." row CHECKABLE instead of plain. A checkable row goes
 * through DropdownMenu::onItemToggled() exclusively (never itemTriggered()/notifyActivated()), and
 * DropdownMenu::closeOnCheckableActivation() defaults to false, so toggling it leaves the menu
 * open -- which is exactly when chaining is safe and does what the task spec asks for:
 * @code
 * contextMenu->setItems({ MenuItem::checkable(ReactAction, tr("React...")), ... });
 *
 * connect(contextMenu, &DropdownMenu::itemToggled, ..., [contextMenu, gallery](int id, bool checked)
 * {
 *     if (id != ReactAction) { return; }
 *     if (!checked) { gallery->closeDropdown(); return; }
 *     gallery->setChainParent(contextMenu);
 *     gallery->popupAboveRect(contextMenu->fullRect());
 * });
 *
 * // Keep the row's own checked visual in sync with however the gallery closes (a pick, Escape,
 * // an outside click, or the menu itself closing and cascading down to its chained child) --
 * // setItemChecked() goes through a QSignalBlocker, so this can never re-trigger itemToggled().
 * connect(gallery, &DropdownFrame::hidden, contextMenu, [contextMenu]()
 * {
 *     contextMenu->setItemChecked(ReactAction, false);
 * });
 * @endcode
 *
 * A PLAIN item (setChainParent() NOT called, anchored via a captured fullRect() instead) remains
 * the right shape for a trigger that is expected to close its own menu on activation, same as any
 * other ordinary item -- the gallery then opens independently and outlives it.
 */
class UISE_DESKTOP_EXPORT ChatReactionGalleryDropdown : public DropdownFrame
{
    Q_OBJECT

    public:

        explicit ChatReactionGalleryDropdown(QWidget* parent=nullptr);
        ~ChatReactionGalleryDropdown() override;

        ChatReactionGalleryDropdown(const ChatReactionGalleryDropdown&) = delete;
        ChatReactionGalleryDropdown(ChatReactionGalleryDropdown&&) = delete;
        ChatReactionGalleryDropdown& operator=(const ChatReactionGalleryDropdown&) = delete;
        ChatReactionGalleryDropdown& operator=(ChatReactionGalleryDropdown&&) = delete;

        void setPack(std::shared_ptr<AbstractReactionIconPack> pack);
        void setOwnReactionIds(QStringList ids);

        //! Forwarded to BOTH pages, same as setPack()/setOwnReactionIds() -- to the collapsed
        //! quick bar via ChatReactionQuickBar::setLeadingIconIds() and to the expanded gallery's
        //! own recents row via ChatReactionGallery::setRecentIds() -- so whichever page
        //! setExpanded() shows next already has current data instead of needing a push on every
        //! switch. @see ChatReactionGallery::setRecentIds() for the bare-icon-id/prepend contract.
        void setRecentIds(QStringList ids);

        //! Switch between the collapsed quick bar and the expanded gallery. If the frame is
        //! currently open, this re-measures immediately (DropdownFrame::remeasureKeepingTopLeft())
        //! rather than waiting for the next opening -- the quick bar's own on-screen position
        //! never moves; the gallery unfolds downward beneath it (or folds back away) instead --
        //! see that method's own doc comment.
        void setExpanded(bool enable);

        bool isExpanded() const noexcept
        {
            return m_expanded;
        }

    Q_SIGNALS:

        /**
         * @brief A reaction was picked, from either the collapsed quick bar or the expanded
         *  gallery's own grid.
         *
         * Emitted BEFORE this popup starts closing itself: picking a reaction calls
         * DropdownFrame::notifyActivated() right after, which closes chainRoot() -- so a gallery
         * chained above a host's context menu (setChainParent()) takes that menu down with it
         * too. The host does not need to (and should not) call closeDropdown() itself here.
         */
        void reactionPicked(const QString& reactionId);

    private:

        ChatReactionQuickBar* m_quickBar;
        ChatReactionGallery* m_gallery;
        bool m_expanded=false;
};

}

#endif // UISE_DESKTOP_CHATREACTIONGALLERY_HPP
