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

/** @file uise/desktop/chatimageviewercontrols.hpp
*
*  Declares ChatImageViewerControls.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATIMAGEVIEWERCONTROLS_HPP
#define UISE_DESKTOP_CHATIMAGEVIEWERCONTROLS_HPP

#include <memory>

#include <QDateTime>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/imagepreviewstrip.hpp>
#include <uise/desktop/dropdownmenu.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;
class IconTextButton;
class ChatImageViewerControls_p;

/**
 * @brief Telegram-style custom bottom widget for a chat image viewer, meant to be installed via
 *  AbstractImageViewer::setBottomWidget() (see ChatImageViewer, which does exactly that).
 *
 * Left to right: a text block (image counter on top, elided sender + separator + datetime below),
 * a centred ImagePreviewStrip showing the current message's album (hidden for a single-image
 * message, see ImagePreviewStrip::count()), and a right-hand toolbar (Save as / Rotate
 * counterclockwise / Zoom in / Zoom out / a checkable vertical-dots menu button).
 *
 * Purely presentational and signal-only, mirroring AbstractChatMessageImages' own shape: no file
 * dialog, clipboard, or navigation logic lives here -- every action (including the toolbar's own
 * Save as / Rotate / Zoom in / Zoom out) is emitted as a *Requested() signal for the host to
 * implement. ChatImageViewer is the one exception that wires rotate/zoom straight back into the
 * viewer itself, since those two are meaningful without any host involvement.
 */
class UISE_DESKTOP_EXPORT ChatImageViewerControls : public Frame
{
    Q_OBJECT

    public:

        //! Ids of the drop-down menu entries, mirroring ChatFileMenuAction's shared-vocabulary role.
        enum class MenuAction : int
        {
            GoToMessage=1,
            Copy=2,
            Forward=3,
            DeleteMessage=4,
            SaveAs=5
        };
        Q_ENUM(MenuAction)

        explicit ChatImageViewerControls(QWidget* parent=nullptr);

        ~ChatImageViewerControls();
        ChatImageViewerControls(const ChatImageViewerControls&)=delete;
        ChatImageViewerControls(ChatImageViewerControls&&)=delete;
        ChatImageViewerControls& operator=(const ChatImageViewerControls&)=delete;
        ChatImageViewerControls& operator=(ChatImageViewerControls&&)=delete;

        //! Set the "n of N" counter text.
        void setCounter(size_t number, size_t total);

        void setSender(QString sender);
        QString sender() const;

        void setDateTime(QDateTime dateTime);
        QDateTime dateTime() const;

        //! Forwarded to previewStrip()->setPreviews().
        void setPreviews(std::vector<ImagePreviewStrip::Preview> previews, int currentIndex=0);

        //! Forwarded to previewStrip()->setCurrentIndex().
        void setCurrentPreview(int index);

        //! Forwarded to previewStrip()->setImageSource().
        void setPreviewSource(std::shared_ptr<PixmapSource> source);

        ImagePreviewStrip* previewStrip() const;

        //! Live drop-down menu, for callers that need finer control than
        //! addMenuItem()/addMenuSeparator() below (reordering, per-item enable/disable, etc.).
        DropdownMenu* menu() const;

        //! Show/hide one of the built-in MenuAction entries.
        void setMenuActionVisible(MenuAction action, bool visible);

        /**
         * @brief Append a caller-defined entry to the menu, after the built-in MenuAction ones.
         * @param item Row to append. Its id must not collide with a MenuAction value (1-5) or
         *  with the id of another custom item already added -- ids are the only way triggering
         *  is told apart, see customMenuItemTriggered(). A checkable item works as normal
         *  (MenuItem::checkable()); this class does not track its checked state itself, the host
         *  does (via menu()->setItemChecked()/isItemChecked()).
         *
         * Equivalent to menu()->addItem(std::move(item)) -- provided as a named entry point so a
         * host does not need to reach into menu() just to add its own rows, but menu()->addItem()
         * remains available directly for the same effect.
         */
        void addMenuItem(MenuItem item);

        //! Append a separator row -- typically before a first custom item, to set it apart from
        //! the built-in ones. Equivalent to menu()->addSeparator().
        void addMenuSeparator();

        PushButton* saveAsButton() const;
        PushButton* rotateButton() const;
        PushButton* zoomInButton() const;
        PushButton* zoomOutButton() const;
        IconTextButton* menuButton() const;

    signals:

        //! A preview in the album strip was clicked; index is within the currently loaded album.
        void previewClicked(int index);

        //! Same event as previewClicked(), identifying the preview by key -- see
        //! ImagePreviewStrip::previewClickedKey(), which this simply forwards.
        void previewClickedKey(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        void saveAsRequested();
        void rotateRequested();
        void zoomInRequested();
        void zoomOutRequested();

        void goToMessageRequested();
        void copyRequested();
        void forwardRequested();
        void deleteMessageRequested();

        //! A menu row added via addMenuItem() (or directly via menu()->addItem()) was triggered;
        //! id is whatever MenuItem::id the host gave that row when adding it. Never emitted for
        //! the five built-in MenuAction rows, which each get their own dedicated signal above.
        void customMenuItemTriggered(int id);

    private slots:

        void onMenuItemTriggered(int id);

    private:

        std::unique_ptr<ChatImageViewerControls_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATIMAGEVIEWERCONTROLS_HPP
