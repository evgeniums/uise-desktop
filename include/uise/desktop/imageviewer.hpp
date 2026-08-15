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

/** @file uise/desktop/imageviewer.hpp
*
*  Declares ImageViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGE_VIEWER_HPP
#define UISE_DESKTOP_IMAGE_VIEWER_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractimageviewer.hpp>
#include <uise/desktop/imageanimator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ImageViewerWidget;

class UISE_DESKTOP_EXPORT ImageViewer : public AbstractImageViewer
{
    Q_OBJECT

    public:

        explicit ImageViewer(QObject* parent=nullptr);

        void setControlsMode(ControlsMode mode) override;

        void setBottomWidget(QWidget* widget) override;
        QWidget* bottomWidget() const override;

        //! True when the currently selected image has animated content loaded -- gates the
        //! toolbar's play/pause button (see updatePlayPauseButton()).
        bool isCurrentImageAnimated() const;

        //! True when the current image's animation is actually running -- mirrors ImageLabel::
        //! isPlaying(). Always false when !isCurrentImageAnimated().
        bool isCurrentImagePlaying() const;

        //! Set the mode governing when the current image's animation is allowed to play, default
        //! ImageAnimator::AnimationMode::Auto -- mirrors ImageLabel::setAnimationMode().
        void setAnimationMode(UISE_DESKTOP_NAMESPACE::ImageAnimator::AnimationMode mode);
        UISE_DESKTOP_NAMESPACE::ImageAnimator::AnimationMode animationMode() const;

        //! Set playback speed as a percentage of the source frame delays, default 100 -- mirrors
        //! ImageLabel::setAnimationSpeed().
        void setAnimationSpeed(int percent);
        int animationSpeed() const;

    signals:

        //! Fires whenever isCurrentImageAnimated() or the animation's play/pause state might have
        //! changed -- after navigation, animation content arriving asynchronously, or play()/
        //! pause()/togglePlay(). ChatImageViewer listens to this to keep ChatImageViewerControls'
        //! own play/pause button in sync, since it replaces this class' embedded toolbar entirely
        //! (see AbstractImageViewer::setBottomWidget()).
        void currentImageAnimationStateChanged();

    public slots:

        void reset() override;

        void zoomIn() override;
        void zoomOut() override;
        void flipVertical() override;
        void flipHorizontal() override;
        void rotate() override;
        void rotateClockwise() override;

        void fitImage() override;

        void showControls() override;
        void hideControls() override;

        //! Playback controls for the current image's animation, meaningful only when
        //! isCurrentImageAnimated() -- mirror ImageLabel::play()/pause()/stop()/togglePlay().
        void play();
        void pause();
        void stop();
        void togglePlay();

    private slots:

        void onPixmapUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;
        void onPixmapLoadingChanged(const UISE_DESKTOP_NAMESPACE::PixmapKey& key, bool loading) override;

        void onAnimatorFrameChanged();
        void onAnimatorPlayingChanged(bool playing);
        void onAnimatorAnimatedChanged(bool animated);

    protected:

        void doReset();
        void doSelectImage() override;
        Widget* doCreateActualWidget(QWidget* parent) override;

        void onAnimationUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;

        //! Create the QGraphicsPixmapItem lazily on first use, or update it in place -- shared by
        //! doSelectImage() and onPixmapUpdated() so a pixmap that arrives asynchronously (still
        //! loading at selection time) is picked up whenever it lands, not only re-applied if an
        //! item already happened to exist (see the B1 fix note on onPixmapUpdated()).
        //!
        //! When the current image has animated content loaded (m_animator->isAnimated()), the
        //! animator's current frame is used in place of currentImage() -- so both a fresh
        //! navigation and an ordinary frame tick from onAnimatorFrameChanged() route through this
        //! single method, and zoom/pan/rotate/flip (all QGraphicsView transforms, untouched by
        //! which pixmap is loaded into the item) keep working unchanged for animated content.
        void applyCurrentPixmap();

        //! Load/unload m_animator against currentImageAnimation(), tracked by m_animatorKey so
        //! repeated calls for the same still-current image are cheap no-ops. Called from
        //! doSelectImage() (a fresh navigation) and onAnimationUpdated() (animation content that
        //! arrives asynchronously after the image was already selected -- forces a re-check by
        //! invalidating m_animatorKey first).
        void syncAnimatorToCurrentImage();

        //! Show/hide the toolbar's play/pause button and refresh its icon -- called after
        //! anything that might change isCurrentImageAnimated()/m_animator->isPlaying().
        void updatePlayPauseButton();

        //! Refreshes both spinners and the prev/next buttons -- see AbstractImageViewer::
        //! onWindowChanged()'s doc. Any window/hasMore*/pending-navigation change routes here.
        void onWindowChanged() override;

        //! Three-way: a large centred blocking spinner while currentImage() is null (unchanged
        //! behaviour), a small non-blocking overlay spinner while an already-displayed image is
        //! still being improved (isCurrentImageLoading()) or navigation is pending past a loaded
        //! edge (isNavigationPending()), or nothing.
        void updateBusySpinner();
        void updatePrevNextButtons();

        /**
         * @brief Force an immediate recompute of the overlay bottom widget's/prev-next buttons'
         *  geometry, without waiting for Qt's own queued QEvent::LayoutRequest.
         *
         * ImageViewerWidget::updateButtonPositions() is private (only ImageViewer is friend, and
         * friendship is not inherited), so a subclass whose bottom widget's content can change
         * its own sizeHint well after doCreateActualWidget() ran (see ChatImageViewer, which
         * calls this after every setPreviews() on its album strip) needs this instead of relying
         * solely on the eventFilter()-driven QEvent::LayoutRequest reaction, which only fires on
         * the next event loop iteration.
         */
        void refreshOverlayGeometry();

        ImageViewerWidget* m_widget=nullptr;

    private:

        //! ImageAnimator subclass reporting this controller's widget's visibility/window-
        //! activation state -- defined in the .cpp; forward-declared here as a nested class so it
        //! has access to m_widget without needing a separate friendship grant from
        //! ImageViewerWidget (nested classes share their enclosing class's own access rights).
        class ViewerAnimator;

        std::unique_ptr<UISE_DESKTOP_NAMESPACE::ImageAnimator> m_animator;

        //! Key of the windowed image m_animator is currently loaded (or explicitly cleared) for --
        //! see syncAnimatorToCurrentImage(). A default-constructed (invalid) key means "not yet
        //! synced to the current image".
        PixmapKey m_animatorKey;
};

class ImageViewerWidget_p;
class UISE_DESKTOP_EXPORT ImageViewerWidget : public WidgetQFrame
{
    Q_OBJECT

    // QSS-tunable presentation knobs of the ControlsMode::Overlay fade, mirroring
    // ChatDateSubtitle's own show/hide/fade Q_PROPERTYs.
    Q_PROPERTY(qreal controlsOpacity READ controlsOpacity WRITE setControlsOpacity)
    Q_PROPERTY(int controlsFadeInDurationMs READ controlsFadeInDurationMs WRITE setControlsFadeInDurationMs)
    Q_PROPERTY(int controlsFadeOutDurationMs READ controlsFadeOutDurationMs WRITE setControlsFadeOutDurationMs)
    Q_PROPERTY(qreal controlsMaxOpacity READ controlsMaxOpacity WRITE setControlsMaxOpacity)
    Q_PROPERTY(int edgeNavigationZoneWidth READ edgeNavigationZoneWidth WRITE setEdgeNavigationZoneWidth)

    public:

        constexpr static const int DefaultControlsFadeInDurationMs=150;
        constexpr static const int DefaultControlsFadeOutDurationMs=300;
        constexpr static const qreal DefaultControlsMaxOpacity=1.0;
        constexpr static const int DefaultEdgeNavigationZoneWidth=96;

        ImageViewerWidget(ImageViewer* ctrl, QWidget* parent=nullptr);

        ~ImageViewerWidget();
        ImageViewerWidget(const ImageViewerWidget&)=delete;
        ImageViewerWidget(ImageViewerWidget&&)=delete;
        ImageViewerWidget& operator=(const ImageViewerWidget&)=delete;
        ImageViewerWidget& operator=(ImageViewerWidget&&)=delete;

        void setControlsMode(AbstractImageViewer::ControlsMode mode);
        AbstractImageViewer::ControlsMode controlsMode() const noexcept;

        void setBottomWidget(QWidget* widget);
        QWidget* bottomWidget() const;

        //! Current animated opacity of the overlay controls. Not meant to be set directly by client code.
        qreal controlsOpacity() const;
        void setControlsOpacity(qreal value);

        int controlsFadeInDurationMs() const;
        void setControlsFadeInDurationMs(int value);

        int controlsFadeOutDurationMs() const;
        void setControlsFadeOutDurationMs(int value);

        //! Maximum (fully visible) opacity to fade the overlay controls in to.
        qreal controlsMaxOpacity() const;
        void setControlsMaxOpacity(qreal value);

        //! Width, in pixels, of the click/hover zone along each edge of the image area that
        //! mirrors the prev/next buttons -- active only where hasPrev/hasNext is actually true,
        //! same as the buttons themselves. See ImageViewerWidget::isInPrevNavigationZone().
        int edgeNavigationZoneWidth() const;
        void setEdgeNavigationZoneWidth(int value);

    public slots:

        void showControls();
        void hideControls();

    protected:

        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        bool event(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        void updateButtonPositions();
        void applyControlsMode();
        void updateControlsVisibility();
        void fadeControlsIn();
        void fadeControlsOut();
        void notifyActivity();

        //! True if pos (in this widget's coordinates) lands on the bottom widget or the
        //! prev/next buttons -- walks childAt(pos) up to this, so it works whether the bottom
        //! widget is a Static-mode layout child or an Overlay-mode floating one, and naturally
        //! ignores hidden (e.g. faded-out) widgets.
        bool isOnControls(const QPoint& pos) const;

        //! Shared by mouseReleaseEvent() and the viewport eventFilter() branch: emits
        //! ctrl->viewerClicked() if this was a plain left-button click (not a drag, not on the
        //! controls) ending at pos (this-widget coordinates), unless pos lands in one of the
        //! edge navigation zones below, in which case the matching prev/next button's clicked()
        //! is emitted instead.
        void handlePotentialViewerClick(const QPoint& pos);

        //! True if pos (this-widget coordinates) lands in the left-edge strip that mirrors the
        //! prev button -- full image height, edgeNavigationZoneWidth() wide, active only while
        //! there is a previous image (independent of the button's own transient visibility
        //! during an Overlay-mode fade).
        bool isInPrevNavigationZone(const QPoint& pos) const;

        //! Right-edge counterpart of isInPrevNavigationZone(), mirroring the next button.
        bool isInNextNavigationZone(const QPoint& pos) const;

        //! Update the prev/next buttons' forced-hover state and the viewport cursor for pos
        //! (this-widget coordinates) -- called on every viewport/this MouseMove.
        void updateEdgeNavigationHover(const QPoint& pos);

        //! Reset whatever updateEdgeNavigationHover() last set -- called when the pointer
        //! leaves the viewport or this widget outright.
        void clearEdgeNavigationHover();

        std::unique_ptr<ImageViewerWidget_p> pimpl;

        friend class ImageViewer;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_VIEWER_HPP
