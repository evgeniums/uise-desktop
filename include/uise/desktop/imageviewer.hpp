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

UISE_DESKTOP_NAMESPACE_BEGIN

class ImageViewerWidget;

class UISE_DESKTOP_EXPORT ImageViewer : public AbstractImageViewer
{
    Q_OBJECT

    public:

        using AbstractImageViewer::AbstractImageViewer;

        void setControlsMode(ControlsMode mode) override;

        void setBottomWidget(QWidget* widget) override;
        QWidget* bottomWidget() const override;

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

    private slots:

        void onPixmapUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;

    protected:

        void doReset();
        void doSelectImage() override;
        Widget* doCreateActualWidget(QWidget* parent) override;

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

        ImageViewerWidget* m_widget;
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

    public:

        constexpr static const int DefaultControlsFadeInDurationMs=150;
        constexpr static const int DefaultControlsFadeOutDurationMs=300;
        constexpr static const qreal DefaultControlsMaxOpacity=1.0;

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
        //! controls) ending at pos (this-widget coordinates).
        void handlePotentialViewerClick(const QPoint& pos);

        std::unique_ptr<ImageViewerWidget_p> pimpl;

        friend class ImageViewer;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_VIEWER_HPP
