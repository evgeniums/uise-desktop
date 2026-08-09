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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatfileitem.hpp>
#include <uise/desktop/imagelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class IconTextButton;
class AbstractLoadControl;

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

        AbstractLoadControl* loadControl() const;

        /**
         * @brief The drop-down menu button floating over the tile's top-right corner.
         */
        IconTextButton* menuButton() const;

        /**
         * @brief Set when animated content of the preview is allowed to play.
         * @param mode Forwarded to the underlying ImageLabel, re-evaluated immediately.
         */
        void setAnimationMode(ImageLabel::AnimationMode mode);

        ImageLabel::AnimationMode animationMode() const noexcept;

        /**
         * @brief Close the per-item drop-down menu if open, without animation.
         *
         * See ChatMessageFileItem::closeMenu() -- same rationale.
         */
        void closeMenu();

    signals:

        /**
         * @brief Emitted when the preview is clicked.
         */
        void clicked();

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

    private:

        void rebuildMenu();
        void updatePreview();
        void repositionOverlays();

    private slots:

        void onMenuItemTriggered(int id);

    private:

        std::unique_ptr<ChatMessageImageItem_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEIMAGEITEM_HPP
