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

/** @file uise/desktop/chatmessageimageitem.hpp
*
*  Declares ChatMessageImageItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEIMAGEITEM_HPP
#define UISE_DESKTOP_CHATMESSAGEIMAGEITEM_HPP

#include <memory>

#include <QFrame>
#include <QList>
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatfileitem.hpp>
#include <uise/desktop/imagelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class IconTextButton;
class AbstractLoadControl;
class LoadControlMenu;

class ChatMessageImageItem_p;

/**
 * @brief One tile of an image chat message's album grid.
 *
 * A clickable, center-cropped preview filling whatever rect the owning ChatMessageImages gives
 * it (see albumLayout()), with a drop-down menu button floating over its top-right corner and,
 * while the image is not yet transferred, a LoadControlMenu -- wrapping an AbstractLoadControl
 * -- centered on top of it. That load control offers pause-or-cancel directly on click while
 * running -- see its own docs -- so this tile has no separate always-visible Cancel control of
 * its own; the drop-down menu still carries Cancel too, for a Pending/Paused/Failed item to
 * which that click-to-pause flow does not apply.
 */
class UISE_DESKTOP_EXPORT ChatMessageImageItem : public QFrame
{
    Q_OBJECT

    public:

        ChatMessageImageItem(QWidget* parent=nullptr);

        ~ChatMessageImageItem();

        ChatMessageImageItem(const ChatMessageImageItem&)=delete;
        ChatMessageImageItem(ChatMessageImageItem&&)=delete;
        ChatMessageImageItem& operator=(const ChatMessageImageItem&)=delete;
        ChatMessageImageItem& operator=(ChatMessageImageItem&&)=delete;

        /**
         * @brief Set the item to display.
         * @param item New item content.
         * @param incoming Direction of the owning message -- selects Download vs Upload
         *  for the load control of a not-yet-transferred item.
         */
        void setItem(const ChatFileItem& item, bool incoming);

        const ChatFileItem& item() const;

        bool isIncoming() const noexcept;

        /**
         * @brief Re-read item() into the preview/load-control/menu widgets.
         */
        void refresh();

        /**
         * @brief The wrapped load control overlay.
         *
         * Created lazily on first use -- e.g. the first time this tile's item is not yet
         * Ready (see refresh()) -- since a transferred, never-hovered tile never needs it.
         * Calling this accessor itself forces creation.
         */
        AbstractLoadControl* loadControl() const;

        /**
         * @brief The drop-down menu button floating over the tile's top-right corner.
         *
         * Created lazily on first hover (see updateMenuButtonVisibility()). Calling this
         * accessor itself forces creation.
         */
        IconTextButton* menuButton() const;

        /**
         * @brief Set when animated content of the preview is allowed to play.
         * @param mode Forwarded to the underlying ImageLabel, re-evaluated immediately.
         */
        void setAnimationMode(ImageLabel::AnimationMode mode);

        ImageLabel::AnimationMode animationMode() const noexcept;

        /**
         * @brief Set how far this tile may upscale its content beyond the item's own natural
         *  (logical) resolution, as a multiplier -- forwarded to utils/pixmapscale.hpp's
         *  scaledToFitPadded() and used to size the placeholder crop the same way, so the
         *  visible content box does not change size when a placeholder preview is replaced by
         *  real content. Applied purely at paint time, to THIS tile's own already-decided rect --
         *  never to album layout geometry (see albumLayout()'s own doc comment for why a
         *  whole-album resolution clamp was tried and reverted). Set by the owning
         *  ChatMessageImages (see its TileMaxUpscale constant).
         */
        void setMaxUpscale(qreal maxUpscale);

        qreal maxUpscale() const noexcept;

        /**
         * @brief Close the per-item drop-down menu if open, without animation.
         *
         * See ChatMessageFileItem::closeMenu() -- same rationale.
         */
        void closeMenu();

        /**
         * @brief Set whether the drop-down menu button is only shown while the tile is
         *  hovered (or its drop-down menu is open), hidden otherwise. Enabled by default.
         */
        void setMenuButtonVisibleOnHover(bool enable);

        bool menuButtonVisibleOnHover() const noexcept;

        /**
         * @brief Set whether pressing and dragging the preview past the drag threshold starts
         *  an outgoing file drag (see startDrag()) instead of a click. Default true.
         */
        void setDragEnabled(bool enable);

        bool isDragEnabled() const noexcept;

        /**
         * @brief Start an outgoing QDrag carrying urls, using this tile's own preview image as
         *  the drag pixmap. Called by the owner (ChatMessageImages) once it has resolved the
         *  urls for dragStartRequested() -- this tile never resolves its own content.
         */
        void startDrag(const QList<QUrl>& urls);

    signals:

        /**
         * @brief Emitted when the preview is clicked.
         */
        void clicked();

        /**
         * @brief Emitted on press, before it is known whether the gesture turns into a drag or
         *  a click -- the owner's cue to start resolving/exporting this tile's content so it is
         *  ready by the time dragStartRequested() (if any) arrives.
         */
        void dragPrepareRequested();

        /**
         * @brief Emitted once the press has moved past the drag threshold. The owner is
         *  expected to call startDrag() with whatever urls dragPrepareRequested() resolved, or
         *  do nothing if they are not ready yet.
         */
        void dragStartRequested();

        /**
         * @brief Emitted when the load control is clicked in any state but Running -- see
         *  LoadControlMenu::clicked().
         */
        void loadControlClicked();

        /**
         * @brief Emitted when a drop-down menu entry is triggered.
         * @param action One of ChatFileMenuAction.
         */
        void menuTriggered(int action);

        /**
         * @brief Forwarded from the load control's LoadControlMenu -- see
         *  ChatMessageFileItem::pauseRequested() for the full rationale, identical here.
         */
        void pauseRequested();
        void cancelRequested();

    protected:

        void resizeEvent(QResizeEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void rebuildMenu();
        void updatePreview();
        void repositionOverlays();
        void updateMenuButtonVisibility();

        //! Create the load control overlay on first use -- see loadControl()'s doc comment.
        //! const so the loadControl() accessor can call it directly: constness does not
        //! propagate through pimpl (a unique_ptr member), so mutating *pimpl here is legal.
        LoadControlMenu* ensureLoadControl() const;

        //! Create the menu button and its drop-down on first use -- see menuButton()'s doc
        //! comment. const for the same reason as ensureLoadControl().
        IconTextButton* ensureMenuButton() const;

        //! Toggle the "placeholder" dynamic property (and repolish) that switches this tile
        //! between normal rendering and the empty rounded-outline look used when there is no
        //! image content to show at all -- see chatmessagefiles.qss's [placeholder="true"] rule
        //! and updatePreview()'s own else branch.
        void setPlaceholderMode(bool enable);

    private slots:

        void onMenuItemTriggered(int id);

    private:

        std::unique_ptr<ChatMessageImageItem_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEIMAGEITEM_HPP
