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

    Q_SIGNALS:

        void reactionPicked(const QString& reactionId);
        void expandRequested();

    private:

        void rebuild();

        std::shared_ptr<AbstractReactionIconPack> m_pack;
        QStringList m_ownReactionIds;
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

        int galleryVisibleRows() const noexcept { return m_galleryVisibleRows; }
        void setGalleryVisibleRows(int value);

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
