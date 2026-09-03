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
#include <uise/desktop/imagepreviewstrip.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatImageViewerControls;
class ChatImageViewer_p;

/**
 * @brief Telegram-style chat image viewer: a flyweight ImageViewer whose bottom widget is a
 *  ChatImageViewerControls, kept in sync with the currently shown image.
 *
 * Images are loaded/grown via loadChatImages()/insertFetchedChatImages() -- NOT the inherited
 * AbstractImageViewer::loadImages()/insertFetchedImages(), which know nothing about the parallel
 * per-image metadata (ChatImage::sender/dateTime/messageId/previewKey) this class keeps alongside
 * the base's own windowed images, keyed by the same PixmapKey. Calling the base methods directly
 * desyncs that metadata for the affected keys; updateControls() degrades gracefully for any key it
 * has no metadata for (blank sender/datetime, no album) rather than crashing, but it is not a
 * supported way to load images into this class.
 *
 * The bottom widget's ImagePreviewStrip (ChatImageViewerControls::previewStrip()) shows either the
 * contiguous run of images sharing the current image's ChatImage::messageId (StripScope::Album,
 * the default, mirroring a Telegram album), or a plain, ungrouped neighbourhood of the current
 * image (StripScope::Continuous) -- see setStripScope(). Either way, prev/next always pages across
 * the whole loaded chat, not just the strip's own content.
 *
 * The strip's own Preview::key (see previewKeyFor()) lives in a DIFFERENT key space than the image
 * key clicking it must select -- ChatImageViewer_p::previewToImage is the bridge, rebuilt on every
 * updateControls() and consulted by selectImageForPreviewKey(), which is what actually answers a
 * click on a thumbnail.
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
            QString   messageId;    //!< Album grouping (StripScope::Album only): equal, non-empty
                                     //!< ids form one album.

            //! Explicit override of the strip thumbnail's key, when it cannot be derived from key
            //! by substituting the strip's own item size (see previewKeyFor()) -- e.g. an
            //! embedded/inline thumbnail addressed at a wholly different path. Left default
            //! (invalid) to use previewKeyFor()'s derivation instead.
            PixmapKey previewKey;

            //! Seed encoded animation content, forwarded verbatim into the base viewer's Image::
            //! animation -- see AbstractImageViewer::imageAnimation()'s producer-first/seed-
            //! fallback precedence. Left null when the caller expects a PixmapSource to supply it
            //! (or the image is not animated).
            AnimationContent animation;

            ChatImage(
                    PixmapKey key={},
                    QPixmap content={},
                    QString sender={},
                    QDateTime dateTime={},
                    QString messageId={},
                    PixmapKey previewKey={},
                    AnimationContent animation={}
                ) : key(std::move(key)),
                    content(std::move(content)),
                    sender(std::move(sender)),
                    dateTime(std::move(dateTime)),
                    messageId(std::move(messageId)),
                    previewKey(std::move(previewKey)),
                    animation(std::move(animation))
            {}
        };

        //! What the bottom widget's ImagePreviewStrip shows.
        enum class StripScope : uint8_t
        {
            Album,      //!< Default: the contiguous run of images sharing the current image's
                        //!< ChatImage::messageId (Telegram's own behaviour).
            Continuous  //!< A plain, ungrouped flow: [current-stripRadius, current+stripRadius]
                        //!< within the loaded window, ignoring messageId entirely.
        };
        Q_ENUM(StripScope)

        //! Default stripRadius() in StripScope::Continuous -- entries shown on each side of the
        //! current image, bounded to keep ImagePreviewStrip's own diff/relayout work cheap.
        constexpr static const size_t DefaultStripRadius=12;

        explicit ChatImageViewer(QObject* parent=nullptr);

        ~ChatImageViewer();
        ChatImageViewer(const ChatImageViewer&)=delete;
        ChatImageViewer(ChatImageViewer&&)=delete;
        ChatImageViewer& operator=(const ChatImageViewer&)=delete;
        ChatImageViewer& operator=(ChatImageViewer&&)=delete;

        //! Replace the whole window with a closed set -- see AbstractImageViewer::loadImages().
        void loadChatImages(std::vector<ChatImage> images);

        //! Replace the whole window with a possibly-open set -- see AbstractImageViewer::
        //! loadImages(images,hasMoreBefore,hasMoreAfter,firstPosition,totalCount), which this
        //! forwards to verbatim after splitting off the chat metadata.
        void loadChatImages(
            std::vector<ChatImage> images,
            bool hasMoreBefore,
            bool hasMoreAfter,
            qint64 firstPosition=0,
            qint64 totalCount=-1
        );

        //! Insert a page fetched in reply to AbstractImageViewer::imagesRequested() -- see
        //! AbstractImageViewer::insertFetchedImages(), which this forwards to.
        void insertFetchedChatImages(std::vector<ChatImage> images, UISE_DESKTOP_NAMESPACE::Direction direction, size_t requestedCount);

        //! Update an already-windowed image's chat metadata (and/or seed content) in place --
        //! mirrors AbstractImageViewer::updateImage(); false if key is not currently windowed.
        bool updateChatImage(const ChatImage& image);

        //! Remove every currently-windowed image whose ChatImage::messageId equals messageId
        //! (e.g. the message was deleted) -- mirrors AbstractImageViewer::removeImage() applied to
        //! each. @return how many images were actually removed.
        size_t removeImagesForMessage(const QString& messageId);

        //! The embedded bottom widget, for callers that need to reach it directly (e.g. to hide
        //! a menu action). Valid only after the actual widget has been created.
        ChatImageViewerControls* controls() const;

        /**
         * @brief Show "n of N" against a numbering window narrower than the loaded chain, e.g.
         *  when only a page of a much longer chat's images has been fetched so far.
         * @param firstImageNumber 1-based number of imageKey(0) within the whole chat.
         * @param totalCount Total image count of the whole chat.
         *
         * A thin adapter onto AbstractImageViewer::setWindowFirstPosition()/setTotalCountHint();
         * once set, the counter then keeps itself correct as the window slides with no further
         * calls needed, same as those two methods' own doc describes.
         */
        void setImageNumbering(size_t firstImageNumber, size_t totalCount);

        //! Undo setImageNumbering(); the counter goes back to deriving from imageCount()/
        //! currentImageIndex() alone.
        void resetImageNumbering();

        void setStripScope(StripScope scope);
        StripScope stripScope() const noexcept;

        void setStripRadius(size_t n);
        size_t stripRadius() const noexcept;

        //! Show/hide the "n of N" counter -- forwarded to ChatImageViewerControls::
        //! setCounterVisible(). Useful when the loaded window's own count (imageCount()/
        //! totalCountHint()) is not a number the caller wants to present to the user. Visible
        //! by default; updateControls() skips recomputing the counter text while hidden.
        void setCounterVisible(bool visible);
        bool isCounterVisible() const;

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
        //! Called automatically on currentImageIndexChanged()/windowChanged(); protected so a
        //! subclass can extend or force it, e.g. after a metadata-only update.
        virtual void updateControls();

        //! Key the album strip should resolve a thumbnail with when a ChatImage did not supply
        //! its own previewKey: the image key's own path+data(), with size substituted for the
        //! bottom widget's ImagePreviewStrip::itemSize(). Overridable for a backend whose
        //! thumbnail rung is not addressable that way. Default requires the widget to already
        //! exist (falls back to ImagePreviewStrip::DefaultItemSize otherwise).
        virtual PixmapKey previewKeyFor(const PixmapKey& imageKey) const;

        void onImageEvicted(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;
        void onImageRemoved(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;

    private slots:

        void onCurrentImageIndexChanged(size_t index);
        void onWindowChangedSlot();

        //! Keeps ChatImageViewerControls' own play/pause button in sync with ImageViewer::
        //! isCurrentImageAnimated()/the animation's running state -- see ImageViewer::
        //! currentImageAnimationStateChanged()'s doc for why this class needs its own copy of
        //! that logic instead of relying on the (replaced) embedded toolbar button.
        void onAnimationStateChanged();

    private:

        ImagePreviewStrip::Preview makePreview(size_t index) const;

        //! Resolves previewKey through ChatImageViewer_p::previewToImage (the strip's Preview::key
        //! space is NOT the image key space -- see previewKeyFor()) and selects that image. Silently
        //! does nothing if the key is not (or no longer) mapped, rather than falling back to image
        //! 0 the way AbstractImageViewer::selectImage(const PixmapKey&) does -- that fallback exists
        //! for DirectoryImagesViewer and must not leak into a stale/evicted strip click here.
        void selectImageForPreviewKey(const PixmapKey& previewKey);

        std::unique_ptr<ChatImageViewer_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATIMAGEVIEWER_HPP
