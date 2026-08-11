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

/** @file uise/desktop/chatimageviewer.hpp
*
*  Declares ChatImageViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATIMAGEVIEWER_HPP
#define UISE_DESKTOP_CHATIMAGEVIEWER_HPP

#include <memory>
#include <vector>

#include <QDateTime>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/imageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatImageViewerControls;
class ChatImageViewer_p;

/**
 * @brief Telegram-style chat image viewer: an ImageViewer whose bottom widget is a
 *  ChatImageViewerControls, kept in sync with the currently shown image.
 *
 * Images are loaded via loadChatImages()/insertChatImages()/appendChatImages()/
 * prependChatImages() -- NOT the inherited AbstractImageViewer::loadImages()/insertImages(),
 * which know nothing about the parallel per-image metadata (ChatImage::sender/dateTime/
 * messageId) this class keeps alongside the base's own key list. Calling the base methods
 * directly desyncs that metadata; updateControls() degrades gracefully (blank sender/datetime,
 * no album) rather than crashing if that ever happens, but it is not a supported way to load
 * images into this class.
 *
 * The album strip shown by ChatImageViewerControls::previewStrip() is the contiguous run of
 * images sharing the current image's ChatImage::messageId -- so prev/next can page across the
 * whole loaded chat while the strip only ever shows the current message's own attachments.
 */
class UISE_DESKTOP_EXPORT ChatImageViewer : public ImageViewer
{
    Q_OBJECT

    public:

        //! One image plus the chat metadata ChatImageViewerControls needs to display it.
        struct ChatImage
        {
            PixmapKey key;
            QPixmap   content;
            QString   sender;
            QDateTime dateTime;
            QString   messageId;   //!< Album grouping: equal, non-empty ids form one album.

            ChatImage(
                    PixmapKey key={},
                    QPixmap content={},
                    QString sender={},
                    QDateTime dateTime={},
                    QString messageId={}
                ) : key(std::move(key)),
                    content(std::move(content)),
                    sender(std::move(sender)),
                    dateTime(std::move(dateTime)),
                    messageId(std::move(messageId))
            {}
        };

        explicit ChatImageViewer(QObject* parent=nullptr);

        ~ChatImageViewer();
        ChatImageViewer(const ChatImageViewer&)=delete;
        ChatImageViewer(ChatImageViewer&&)=delete;
        ChatImageViewer& operator=(const ChatImageViewer&)=delete;
        ChatImageViewer& operator=(ChatImageViewer&&)=delete;

        void loadChatImages(std::vector<ChatImage> images);
        void insertChatImages(size_t index, std::vector<ChatImage> images);
        void appendChatImages(std::vector<ChatImage> images);
        void prependChatImages(std::vector<ChatImage> images);

        //! The embedded bottom widget, for callers that need to reach it directly (e.g. to hide
        //! a menu action). Valid only after the actual widget has been created.
        ChatImageViewerControls* controls() const;

        /**
         * @brief Show "n of N" against a numbering window narrower than the loaded chain, e.g.
         *  when only a page of a much longer chat's images has been fetched so far.
         * @param firstImageNumber 1-based number of imageKey(0) within the whole chat.
         * @param totalCount Total image count of the whole chat.
         *
         * Without a call to this, the counter derives directly from imageCount()/
         * currentImageIndex() -- correct as long as the whole chat is loaded at once.
         */
        void setImageNumbering(size_t firstImageNumber, size_t totalCount);

        //! Undo setImageNumbering(); the counter goes back to deriving from imageCount().
        void resetImageNumbering();

        void setImageSource(std::shared_ptr<PixmapSource> imageSource) override;

    signals:

        void saveAsRequested(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);
        void copyRequested(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);
        void forwardRequested(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);
        void goToMessageRequested(const QString& messageId);
        void deleteMessageRequested(const QString& messageId);

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override;

        //! Refresh the bottom widget's counter/sender/datetime/album from the current image.
        //! Called automatically on currentImageIndexChanged(); protected so a subclass can
        //! extend or force it, e.g. after a metadata-only update.
        virtual void updateControls();

    private slots:

        void onCurrentImageIndexChanged(size_t index);

    private:

        std::unique_ptr<ChatImageViewer_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATIMAGEVIEWER_HPP
