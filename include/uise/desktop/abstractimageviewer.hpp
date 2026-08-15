/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractimageviewer.hpp
*
*  Declares AbstractImageViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP
#define UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP

#include <memory>
#include <vector>
#include <cstdint>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractImageViewer_p;

/**
 * @brief Base class of a flyweight image viewer/browser.
 *
 * The viewer holds a bounded, contiguous *window* of images rather than the whole browsable set --
 * see loadImages(), insertFetchedImages(), hasMoreBefore()/hasMoreAfter(). A window-mutating caller
 * (a chat controller walking message history, a directory browser that already has everything)
 * either supplies the closed set up front via loadImages() (both hasMore* flags false, the classic
 * non-flyweight case -- DirectoryImagesViewer and the plain demos use only this) or connects to
 * imagesRequested() and replies via insertFetchedImages() as the user pages toward either end.
 *
 * Pixel data is resolved through a PixmapSource (setImageSource()), one PixmapKey per image: the
 * key's path identifies the image, its QSize the desired display size. A version-ladder source is
 * expected to call PixmapSource::updatePixmap()/setPixmapLoading() on the SAME key repeatedly as
 * better data arrives -- see Image::content's doc for how that interacts with an eagerly-supplied
 * pixmap, and PixmapSource::setPixmapLoading()'s doc for the optional "still improving" indicator.
 *
 * Key identity is (path, size) only (see PixmapKey/WithPathAndSize). The path must identify the
 * image's position in whatever sequence the caller is walking, not merely the underlying blob --
 * the same blob attached twice at two different positions needs two different keys, or one silently
 * shadows the other in the window.
 */
class UISE_DESKTOP_EXPORT AbstractImageViewer : public WidgetController
{
    Q_OBJECT

    public:

        struct Image
        {
            PixmapKey key;
            QPixmap content;

            //! Seed encoded animation content -- see imageAnimation()'s producer-first/seed-
            //! fallback precedence, which mirrors content/imagePixmap()'s own. Left null when the
            //! caller expects a PixmapSource to supply it (or the image is not animated).
            AnimationContent animation;

            Image(PixmapKey key={}, QPixmap content={}, AnimationContent animation={})
                : key(std::move(key)), content(std::move(content)), animation(std::move(animation))
            {}
        };

        //! How the navigation/toolbar controls are presented over the image.
        enum class ControlsMode : uint8_t
        {
            Static,   //!< Current behaviour: the bottom widget occupies layout space below the image.
            Overlay   //!< The bottom widget and prev/next buttons float over the image and fade out
                      //!< after a period of user inactivity, reappearing on mouse movement.
        };
        Q_ENUM(ControlsMode)

        //! Default size of the retained window, in images -- bounds only lightweight per-entry
        //! bookkeeping (a key, an optional seed pixmap, an idle PixmapConsumer), not decoded pixel
        //! memory. See ActiveWindowRadius for the actual memory-bounding knob.
        constexpr static const size_t DefaultWindowSize=64;

        //! Default maxCount passed with imagesRequested().
        constexpr static const size_t DefaultFetchCount=16;

        //! Request more images once the current one is within this many items of a loaded edge
        //! that still hasMore*().
        constexpr static const size_t DefaultPrefetchThreshold=6;

        //! Number of entries on each side of the current image that keep an acquired
        //! PixmapConsumer/PixmapProducer -- 2*radius+1 resident decodes is the real memory bound
        //! for full-resolution images, distinct from windowSize().
        constexpr static const size_t DefaultActiveWindowRadius=2;

        //! How long showNextImage()/showPrevImage() waits for a reply past a loaded edge before
        //! giving up (see isNavigationPending()).
        constexpr static const size_t DefaultPendingNavTimeoutMs=10000;

        AbstractImageViewer(QObject* parent=nullptr);

        ~AbstractImageViewer();

        AbstractImageViewer(const AbstractImageViewer&)=delete;
        AbstractImageViewer(AbstractImageViewer&&)=delete;
        AbstractImageViewer& operator=(const AbstractImageViewer&)=delete;
        AbstractImageViewer& operator=(AbstractImageViewer&&)=delete;

        //! Replace the whole window with a closed set: both hasMoreBefore()/hasMoreAfter() end up
        //! false, windowFirstPosition() is 0. This is the degenerate, non-flyweight case.
        void loadImages(std::vector<Image> images);

        /**
         * @brief Replace the whole window with a possibly-open set.
         * @param images New window content, in display order.
         * @param hasMoreBefore Whether more images exist before images.front().
         * @param hasMoreAfter Whether more images exist after images.back().
         * @param firstPosition Absolute position of images.front() -- see windowFirstPosition().
         * @param totalCount Total browsable count if known, else -1 -- see totalCountHint().
         */
        void loadImages(
            std::vector<Image> images,
            bool hasMoreBefore,
            bool hasMoreAfter,
            qint64 firstPosition=0,
            qint64 totalCount=-1
        );

        /**
         * @brief Insert a page fetched in reply to imagesRequested().
         * @param images Page content, always in display order regardless of direction.
         * @param direction END appends after the current back of the window, HOME prepends before
         *  the current front.
         * @param requestedCount The maxCount that was passed to imagesRequested(); 0 means this is
         *  an unsolicited insert rather than a reply (hasMoreBefore()/hasMoreAfter() are left
         *  untouched in that case). Otherwise images.size()<requestedCount marks that end of the
         *  browsable set as exhausted (hasMoreBefore()/hasMoreAfter() cleared accordingly),
         *  including the case of an empty page.
         *
         * A key already present in the window is treated as an in-place update (see updateImage()),
         * not a duplicate.
         */
        void insertFetchedImages(std::vector<Image> images, UISE_DESKTOP_NAMESPACE::Direction direction, size_t requestedCount);

        /**
         * @brief Update the seed content/metadata of an already-windowed image in place.
         * @return false if key is not currently in the window (a no-op; use containsImage() to
         *  check first if that distinction matters to the caller).
         *
         * This does NOT touch the resolved pixmap -- a version-ladder upgrade must go through
         * PixmapSource::updatePixmap() on the same key instead, see the class doc. Use this for
         * caller-side metadata changes (e.g. Image::content itself, when the caller now has a
         * better placeholder in hand).
         */
        bool updateImage(const Image& image);

        //! Remove a windowed image, e.g. because the underlying message/attachment was deleted.
        //! @return false if key was not in the window.
        bool removeImage(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        bool containsImage(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) const;

        size_t imageCount() const noexcept;

        QPixmap currentImage() const;

        PixmapKey currentImageKey() const;

        //! @param index Window-relative index (not an absolute position -- see
        //!  currentImagePosition()/windowFirstPosition()).
        PixmapKey imageKey(size_t index) const;

        //! Resolved pixmap for the image at window-relative index -- same producer-first,
        //! content-fallback precedence as currentImage() (D4), generalized to any windowed
        //! position, e.g. for a host that wants to seed a secondary view (a thumbnail strip) of
        //! images it isn't currently displaying full-size. QPixmap{} if index is out of range or
        //! nothing has resolved for that entry yet.
        QPixmap imagePixmap(size_t index) const;

        //! Resolved encoded animation content for the image at window-relative index -- same
        //! producer-first, seed-fallback precedence as imagePixmap(). Null (AnimationContent::
        //! isNull()) if index is out of range, the image is not animated, or nothing has resolved
        //! for it yet.
        AnimationContent imageAnimation(size_t index) const;

        //! imageAnimation() for the currently selected image.
        AnimationContent currentImageAnimation() const;

        virtual void setImageSource(std::shared_ptr<PixmapSource> imageSource);

        std::shared_ptr<PixmapSource> imageSource() const;

        //! Window-relative index of the current image. Stable only until the next window mutation
        //! (prepend/evict shift it) -- currentImageIndexChanged(size_t) fires on every such shift,
        //! not only when the displayed image itself changes; connect to
        //! currentImagePositionChanged(qint64) instead if that distinction matters.
        size_t currentImageIndex() const noexcept;

        //! Whether there is a previous image to show: either window-locally (currentImageIndex()>0)
        //! or via hasMoreBefore().
        bool hasPrevImage() const noexcept;

        //! Whether there is a next image to show: either window-locally or via hasMoreAfter().
        bool hasNextImage() const noexcept;

        bool hasMoreBefore() const noexcept;
        bool hasMoreAfter() const noexcept;

        //! Explicitly declare whether more images exist before the window, e.g. when the caller
        //! already knows the answer (its first page came back short). insertFetchedImages() also
        //! maintains this automatically from requestedCount.
        void setHasMoreBefore(bool enable);
        void setHasMoreAfter(bool enable);

        //! Absolute position of the window's first (index 0) image within the whole browsable
        //! sequence, when that is meaningful to the caller (e.g. a chat's global image ordering).
        //! Maintained automatically as the window slides: a prepend of k lowers it by k, a front
        //! eviction of k raises it by k; back-end mutations never change it.
        qint64 windowFirstPosition() const noexcept;

        //! windowFirstPosition()+currentImageIndex(), or -1 on an empty window.
        qint64 currentImagePosition() const noexcept;

        //! No-op if position falls outside the current window.
        void selectImagePosition(qint64 position);

        //! Explicitly set/correct windowFirstPosition() -- normally maintained automatically as
        //! the window slides (see the getter's doc). Exists for a host that already knows the
        //! true absolute position when it hands over a load, e.g. ChatImageViewer::
        //! setImageNumbering(), which is built directly on top of this.
        void setWindowFirstPosition(qint64 position);

        //! Total browsable count if known to the caller (e.g. "200" for a 200-image chat), else -1.
        //! Purely informational -- e.g. for a "N of M" counter -- the viewer never fetches based on
        //! it. Not kept in sync with out-of-window removeImage() calls the caller makes elsewhere;
        //! the caller must call this again after those.
        void setTotalCountHint(qint64 count);
        qint64 totalCountHint() const noexcept;

        void setWindowSize(size_t n);
        size_t windowSize() const noexcept;

        void setFetchCount(size_t n);
        size_t fetchCount() const noexcept;

        void setPrefetchThreshold(size_t n);
        size_t prefetchThreshold() const noexcept;

        void setActiveWindowRadius(size_t n);
        size_t activeWindowRadius() const noexcept;

        void setPendingNavTimeoutMs(size_t ms);
        size_t pendingNavTimeoutMs() const noexcept;

        //! Clear both in-flight-request flags without waiting for a reply -- call after a fetch the
        //! caller knows has failed (e.g. a bridge call errored out), otherwise that end of the
        //! window is permanently prevented from requesting again.
        void cancelPendingRequests();

        //! Whether the currently displayed image's own pixmap is still being improved by the
        //! source -- see PixmapSource::setPixmapLoading().
        bool isCurrentImageLoading() const;

        //! Whether showNextImage()/showPrevImage() is waiting on a fetch past a loaded edge.
        bool isNavigationPending() const noexcept;

        //! Switch between the embedded static toolbar and a floating, auto-hiding overlay.
        //! Default is ControlsMode::Static, so existing callers see no behavioural change.
        virtual void setControlsMode(ControlsMode mode);

        ControlsMode controlsMode() const noexcept;

        /**
         * @brief Replace the embedded bottom toolbar with a caller-supplied widget.
         * @param widget New bottom widget; the viewer takes ownership (reparents it). Passing
         *  nullptr restores the viewer's own embedded toolbar.
         *
         * Base implementation is a no-op: only a concrete viewer with an actual widget surface
         * (see ImageViewer) can host one.
         */
        virtual void setBottomWidget(QWidget* widget)
        {
            std::ignore=widget;
        }

        //! Get the widget set via setBottomWidget(), or nullptr if none/the embedded toolbar is used.
        virtual QWidget* bottomWidget() const
        {
            return nullptr;
        }

        //! Idle delay, in ControlsMode::Overlay, before the controls fade out. Default 2500 ms.
        void setControlsAutoHideDelayMs(int ms) noexcept
        {
            m_controlsAutoHideDelayMs=ms;
        }

        int controlsAutoHideDelayMs() const noexcept
        {
            return m_controlsAutoHideDelayMs;
        }

    signals:

        void currentImageIndexChanged(size_t index);

        //! Fires whenever currentImagePosition() changes, i.e. the DISPLAYED image actually
        //! changed -- unlike currentImageIndexChanged(), never fires on a pure index shift caused
        //! by a prepend/eviction. This is the signal a controller should key its own state off.
        void currentImagePositionChanged(qint64 position);

        //! Fires on any window mutation: insert, evict, remove, or a hasMore*()/totalCountHint()
        //! change -- a coarser signal than currentImage*Changed(), useful for a host that just
        //! wants to know "something about the browsable range changed".
        void windowChanged();

        void currentImageLoadingChanged(bool loading);

        //! The viewer needs more images to keep browsing in direction.
        //! @param anchor Key of the current edge entry (window.back() for END, window.front() for
        //!  HOME), or a default-constructed (invalid) key if the window is empty.
        //! @param maxCount Requested page size; hand back to insertFetchedImages() as requestedCount.
        //!
        //! anchor is the verbatim stored key, including whatever WithPath::data() payload the
        //! caller originally attached to it (e.g. a keyset cursor) -- that payload is not used for
        //! window identity and rides along untouched for exactly this purpose.
        void imagesRequested(const UISE_DESKTOP_NAMESPACE::PixmapKey& anchor, size_t maxCount, UISE_DESKTOP_NAMESPACE::Direction direction);

        //! Emitted once when the user presses Escape (see ImageViewerWidget::keyPressEvent()).
        //! A host wrapping the viewer in a modal dialog is expected to close that dialog on this.
        void closeRequested();

        //! Emitted on a left-button click that lands on the image area rather than on the bottom
        //! widget or the prev/next buttons (see ImageViewerWidget::mouseReleaseEvent()). A
        //! fullscreen host (see ChatImageViewerWindow) closes on it.
        void viewerClicked();

    public slots:

        virtual void reset() {}

        virtual void zoomIn() {}
        virtual void zoomOut() {}
        virtual void flipVertical() {}
        virtual void flipHorizontal() {}
        virtual void rotate() {}
        virtual void rotateClockwise() {}

        //! Alias of rotate() (which rotates counterclockwise) under an unambiguous name; not
        //! itself virtual so a single definition always dispatches to whatever rotate() resolves to.
        void rotateCounterclockwise()
        {
            rotate();
        }

        virtual void fitImage() {}

        //! Not itself virtual -- a single definition always dispatches to whatever
        //! closeRequested() is connected to; see ImageViewerWidget::keyPressEvent()'s Escape case.
        void requestClose()
        {
            emit closeRequested();
        }

        //! Advances within the loaded window, or -- when at the loaded edge and hasNextImage() is
        //! true only because of hasMoreAfter() -- requests more and advances once the reply lands
        //! (see isNavigationPending()). No-op if hasNextImage() is false.
        void showNextImage();
        void showPrevImage();

        //! @param index Window-relative index; out-of-range is clamped to the last valid index.
        void selectImage(size_t index);
        void selectImage(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        //! In ControlsMode::Overlay, show the controls and (re)arm the auto-hide timer. No-op in Static mode.
        virtual void showControls() {}

        //! In ControlsMode::Overlay, fade the controls out immediately. No-op in Static mode.
        virtual void hideControls() {}

    protected:

        virtual void doSelectImage() =0;

        //! Called after any mutation of the window, hasMore* flags, or pending-navigation state --
        //! the ImageViewer override refreshes the busy/overlay spinners and prev/next buttons here.
        virtual void onWindowChanged() {}

        //! Called when an entry is dropped purely because the window shrank back under windowSize()
        //! (the data is still logically browsable, just not resident) -- distinct from
        //! onImageRemoved(), which means the underlying image is gone for good.
        virtual void onImageEvicted(const UISE_DESKTOP_NAMESPACE::PixmapKey& key)
        {
            std::ignore=key;
        }

        virtual void onImageRemoved(const UISE_DESKTOP_NAMESPACE::PixmapKey& key)
        {
            std::ignore=key;
        }

    protected slots:

        virtual void onPixmapUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key)
        {
            std::ignore=key;
        }

        virtual void onPixmapLoadingChanged(const UISE_DESKTOP_NAMESPACE::PixmapKey& key, bool loading)
        {
            std::ignore=key;
            std::ignore=loading;
        }

        //! Called when the resolved animation content for a windowed entry changes -- see
        //! imageAnimation(). Default no-op; ImageViewer overrides it to pick up animation content
        //! that arrives asynchronously after the image was already selected.
        virtual void onAnimationUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key)
        {
            std::ignore=key;
        }

    private:

        //! Re-resolve pimpl's cached window-relative index of currentKey after a mutation.
        void reindexCurrent();

        //! Wire a freshly-constructed entry's consumer signals to this viewer's slots. Done once
        //! per entry (idempotent) regardless of how many times its producer is later
        //! acquired/released. No-op if key is not currently in the window.
        void wireEntry(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        //! Acquire producers for entries within activeWindowRadius() of the current index, release
        //! (but keep resident) everything else.
        void refreshActiveProducers();

        //! Drop entries beyond windowSize() from whichever end is farther from the current index.
        void trimWindow();

        //! Pop and release one entry from the front (fromFront=true) or back of the window,
        //! marking that end's hasMore* flag true (an eviction always means more data exists there
        //! by construction) and calling onImageEvicted().
        void evictEntry(bool fromFront);

        //! Common tail of every window-mutating operation: reindex, optionally re-select the
        //! current image, refresh active producers, notify, and consider fetching more.
        void updateCurrent(bool imageChanged);

        //! Request more images in either direction if warranted -- see DefaultPrefetchThreshold.
        void maybeRequestMore();

        //! Insert or update entries in display order at either end of the window; returns the
        //! number of genuinely new (non-duplicate) keys inserted.
        size_t mergeImages(std::vector<Image>& images, UISE_DESKTOP_NAMESPACE::Direction direction);

        //! Key of the window's current HOME/END edge entry, or an invalid key if empty.
        PixmapKey edgeAnchor(UISE_DESKTOP_NAMESPACE::Direction direction) const;

        void resolvePendingNav();
        void onPendingNavTimeout();

        std::unique_ptr<AbstractImageViewer_p> pimpl;

        ControlsMode m_controlsMode=ControlsMode::Static;
        int m_controlsAutoHideDelayMs=2500;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP
