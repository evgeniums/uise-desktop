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

/** @file uise/desktop/chatmessagefileitem.hpp
*
*  Declares ChatMessageFileItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEFILEITEM_HPP
#define UISE_DESKTOP_CHATMESSAGEFILEITEM_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatfileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class IconTextButton;
class AbstractLoadControl;
class LoadControlMenu;

class ChatMessageFileItem_p;

/**
 * @brief One row of a file chat message's contents list.
 *
 * Structured after FileUploadListItem's Row view: a fixed-size icon slot on the left (a
 * LoadControlMenu -- wrapping an AbstractLoadControl -- while the item is transferring, an
 * AvatarWidget preview for a completed image item, or a per-extension/generic file icon
 * otherwise -- exactly one visible at a time, swapped by show/hide so a progress tick never has
 * to rebuild anything), an elided-in-the-middle clickable file name with its size below, and a
 * drop-down menu button. The icon slot's own LoadControlMenu offers pause-or-cancel directly on
 * click while running -- see its own docs -- so this row has no separate always-visible Cancel
 * control of its own; the drop-down menu still carries Cancel too, for a Pending/Paused/Failed
 * item to which that click-to-pause flow does not apply.
 */
class UISE_DESKTOP_EXPORT ChatMessageFileItem : public QFrame
{
    Q_OBJECT

    public:

        ChatMessageFileItem(QWidget* parent=nullptr);

        ~ChatMessageFileItem();

        ChatMessageFileItem(const ChatMessageFileItem&)=delete;
        ChatMessageFileItem(ChatMessageFileItem&&)=delete;
        ChatMessageFileItem& operator=(const ChatMessageFileItem&)=delete;
        ChatMessageFileItem& operator=(ChatMessageFileItem&&)=delete;

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
         * @brief Re-read item() into the icon/name/info/menu widgets.
         *
         * Called by setItem(); also useful after mutating item() in place without replacing it
         * wholesale (e.g. a progress-only update -- see AbstractChatMessageFiles::updateItem()).
         */
        void refresh();

        /**
         * @brief The icon slot's load control overlay.
         *
         * Created lazily the first time this row's item is not yet Ready (see refresh()), since
         * an already-transferred row never needs it. Calling this accessor itself forces
         * creation.
         */
        AbstractLoadControl* loadControl() const;

        IconTextButton* menuButton() const;

        /**
         * @brief Set how the file-name/size two-line block is aligned within the row's height.
         * @param alignment Only the vertical component is read (anything else is ignored, and an
         *  unrecognized vertical component falls back to Qt::AlignTop). Qt::AlignVCenter (the
         *  default) centers the two-line block within the full row height; Qt::AlignTop keeps
         *  both lines flush with the icon slot's top edge instead.
         */
        void setTextVerticalAlignment(Qt::Alignment alignment);

        Qt::Alignment textVerticalAlignment() const noexcept;

        /**
         * @brief Close the per-item drop-down menu if open, without animation.
         *
         * Meant for a host embedding this item in a scrolling list to call whenever the list
         * scrolls: the menu is parented to window(), not to this item, so it would otherwise
         * float detached from its (moving) menu button. Mirrors FileUploadListItem::closeMenu().
         */
        void closeMenu();

    signals:

        /**
         * @brief Emitted when the file name or the icon/preview is clicked.
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
         * @brief Forwarded from the icon slot's LoadControlMenu -- see its own docs for the
         *  full pause-or-cancel-decision flow these two participate in. Each is also
         *  independently reachable via the drop-down menu's own Pause/Cancel entries
         *  (menuTriggered() with the matching ChatFileMenuAction); the host fans both trigger
         *  paths into the same AbstractChatMessageFiles signals. There is no resumeRequested()
         *  here -- LoadControlMenu never emits one (see its own docs), and resuming a paused
         *  item is a plain loadControlClicked() the host interprets itself, same as any other
         *  non-Running state.
         */
        void pauseRequested();
        void cancelRequested();

    protected:

        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        void rebuildMenu();
        void updateIconSlot();
        void updateInfoLabels();

        //! Create the load control overlay on first use -- see loadControl()'s doc comment.
        //! const so the loadControl() accessor can call it directly: constness does not
        //! propagate through pimpl (a unique_ptr member), so mutating *pimpl here is legal --
        //! see ChatMessageImageItem::ensureLoadControl()'s identical rationale.
        LoadControlMenu* ensureLoadControl() const;

    private slots:

        void onMenuItemTriggered(int id);

    private:

        std::unique_ptr<ChatMessageFileItem_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEFILEITEM_HPP
