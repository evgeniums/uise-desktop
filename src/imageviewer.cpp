/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/imageviewer.cpp
*
*/

/****************************************************************************/

#include <QPointer>
#include <QTimer>
#include <QLabel>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/graphicsviewzoom.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/jumpedge.hpp>
#include <uise/desktop/circlebusy.hpp>
#include <uise/desktop/imageviewer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/********************* ImageViewerWidget *****************************/

class ImageViewerWidget_p
{
    public:

        //! Mirrors ChatDateSubtitle_p::State -- same fade-in-on-activity / fade-out-on-idle shape.
        enum class ControlsState
        {
            Hidden,
            FadingIn,
            Visible,
            FadingOut
        };

        ImageViewer* ctrl;

        QFrame* contentFrame;
        QBoxLayout* layout;

        QGraphicsView *view;
        QGraphicsScene *scene;
        QGraphicsPixmapItem *imageItem = nullptr;
        GraphicsViewZoom *zoom = nullptr;

        QFrame* controlsFrame;
        QFrame* mainButtonsFrame;
        PushButton* rotate;
        PushButton* rotateClockwise;
        PushButton* flipHorizontal;
        PushButton* flipVertical;
        PushButton* zoomIn;
        PushButton* zoomOut;
        PushButton* playPause;

        int angle=0;

        JumpEdge* prevButton;
        JumpEdge* nextButton;

        QLabel* styleSample;

        //! Large, centred, blocking spinner shown while currentImage() is null -- unchanged from
        //! before the flyweight refactor.
        CircleBusy* busySpinner;

        //! Small, non-blocking "still improving" indicator shown over an already-displayed image
        //! -- see AbstractImageViewer::isCurrentImageLoading()/isNavigationPending(). loadingOverlayFrame
        //! is a plain QFrame backing (CircleBusy::paintEvent() never chains to QFrame::paintEvent(),
        //! so QSS background/border-radius on the spinner itself would not render -- see
        //! imageviewer.qss); loadingOverlay is constructed with disableParentWhenSpinning=false so
        //! navigation stays live while a better version is being fetched.
        QFrame* loadingOverlayFrame;
        CircleBusy* loadingOverlay;

        // --- ControlsMode::Overlay support ---

        AbstractImageViewer::ControlsMode controlsMode=AbstractImageViewer::ControlsMode::Static;

        //! Widget set via setBottomWidget(), or nullptr when the embedded controlsFrame is used.
        QWidget* customBottomWidget=nullptr;

        //! Whichever of controlsFrame/customBottomWidget is currently shown as the bottom widget.
        QWidget* activeBottomWidget=nullptr;

        //! Owned by activeBottomWidget once installed (Qt deletes it on the next setGraphicsEffect()
        //! call); only non-null while controlsMode==Overlay.
        QGraphicsOpacityEffect* bottomOpacityEffect=nullptr;
        QGraphicsOpacityEffect* prevOpacityEffect=nullptr;
        QGraphicsOpacityEffect* nextOpacityEffect=nullptr;

        QPropertyAnimation* controlsAnimation=nullptr;
        SingleShotTimer* controlsHideTimer=nullptr;

        ControlsState controlsState=ControlsState::Hidden;
        qreal controlsOpacity=0.0;

        int controlsFadeInDurationMs=ImageViewerWidget::DefaultControlsFadeInDurationMs;
        int controlsFadeOutDurationMs=ImageViewerWidget::DefaultControlsFadeOutDurationMs;
        qreal controlsMaxOpacity=ImageViewerWidget::DefaultControlsMaxOpacity;

        // --- viewerClicked() support ---

        bool pressIsLeftButton=false;
        QPoint pressPos;

        // --- edge navigation zone support ---

        int edgeNavigationZoneWidth=ImageViewerWidget::DefaultEdgeNavigationZoneWidth;
        bool prevZoneHovered=false;
        bool nextZoneHovered=false;
};

//--------------------------------------------------------------------------

ImageViewerWidget::ImageViewerWidget(ImageViewer* ctrl, QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<ImageViewerWidget_p>())
{
    pimpl->ctrl=ctrl;

    auto l=Layout::vertical(this);
    pimpl->contentFrame=new QFrame(this);
    l->addWidget(pimpl->contentFrame);
    pimpl->contentFrame->setObjectName("contentFrame");

    pimpl->layout=Layout::vertical(pimpl->contentFrame);
    pimpl->styleSample=new QLabel(this);
    pimpl->styleSample->setObjectName("viewerStyleSample");
    pimpl->styleSample->setVisible(false);

    pimpl->view = new QGraphicsView(this);
    pimpl->scene = new QGraphicsScene(this);
    pimpl->view->setScene(pimpl->scene);
    pimpl->view->setFocusPolicy(Qt::NoFocus);
    pimpl->layout->addWidget(pimpl->view,1);
    // Installed/enabled once here (not per controls-mode switch in applyControlsMode()) because
    // viewerClicked() detection and edge navigation hover (see mouseReleaseEvent()/eventFilter()/
    // updateEdgeNavigationHover()) must work in both Static and Overlay modes, not just Overlay's
    // auto-hide-on-activity tracking.
    pimpl->view->viewport()->installEventFilter(this);
    pimpl->view->viewport()->setMouseTracking(true);
    setMouseTracking(true);

    // Panning (see pimpl->zoom below) replaces visible scrollbars once zoomed in --
    // ScrollBarAsNeeded would otherwise make scrollbars appear/disappear as fitImage() and the
    // zoom clamp interact (each viewport resize this causes re-enters fitImage() via the
    // QEvent::Resize case in eventFilter() below), which is both visually noisy for a photo
    // viewer and a source of a few-pixel anchor nudge on the very first zoom step past fit.
    pimpl->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl->view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    pimpl->zoom=new GraphicsViewZoom(pimpl->view,this);
    // updateEdgeNavigationHover()/clearEdgeNavigationHover() already own the viewport cursor
    // (prev/next edge-navigation hover, and now also the pan open/closed-hand cursor -- see
    // isInPrevNavigationZone()) -- letting the helper also set it would fight over it on every
    // mouse move.
    pimpl->zoom->setCursorManaged(false);
    pimpl->zoom->setPanButtons(Qt::LeftButton|Qt::MiddleButton);
    // Let zoom-out continue past "fit" down to a small pixel floor instead of stopping there --
    // see GraphicsViewZoom::setMinDisplayPixels()'s own doc.
    pimpl->zoom->setMinDisplayPixels(ImageViewerWidget::DefaultMinDisplayPixels);

    pimpl->controlsFrame=new QFrame(this);
    pimpl->controlsFrame->setObjectName("controlsFrame");
    pimpl->layout->addWidget(pimpl->controlsFrame);
    pimpl->activeBottomWidget=pimpl->controlsFrame;
    auto cl=Layout::horizontal(pimpl->controlsFrame);
    cl->addStretch(1);

    pimpl->mainButtonsFrame=new QFrame(pimpl->controlsFrame);
    pimpl->mainButtonsFrame->setObjectName("mainButtonsFrame");
    auto ml=Layout::horizontal(pimpl->mainButtonsFrame);
    cl->addWidget(pimpl->mainButtonsFrame);

    pimpl->rotate=new PushButton(pimpl->mainButtonsFrame);
    pimpl->rotate->setToolTip(tr("Rotate"));
    pimpl->rotate->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::rotate",this));
    ml->addWidget(pimpl->rotate);
    connect(
        pimpl->rotate,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::rotate
    );
    pimpl->rotateClockwise=new PushButton(pimpl->mainButtonsFrame);
    pimpl->rotateClockwise->setToolTip(tr("Rotate clockwise"));
    pimpl->rotateClockwise->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::rotate-clockwise",this));
    ml->addWidget(pimpl->rotateClockwise);
    connect(
        pimpl->rotateClockwise,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::rotateClockwise
    );
    pimpl->flipHorizontal=new PushButton(pimpl->mainButtonsFrame);
    pimpl->flipHorizontal->setToolTip(tr("Flip horizontally"));
    pimpl->flipHorizontal->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::flip-horizontal",this));
    ml->addWidget(pimpl->flipHorizontal);
    connect(
        pimpl->flipHorizontal,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::flipHorizontal
    );
    pimpl->flipVertical=new PushButton(pimpl->mainButtonsFrame);
    pimpl->flipVertical->setToolTip(tr("Flip vertically"));
    pimpl->flipVertical->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::flip-vertical",this));
    ml->addWidget(pimpl->flipVertical);
    connect(
        pimpl->flipVertical,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::flipVertical
    );
    pimpl->zoomIn=new PushButton(pimpl->mainButtonsFrame);
    pimpl->zoomIn->setToolTip(tr("Zoom in"));
    pimpl->zoomIn->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::zoom-in",this));
    ml->addWidget(pimpl->zoomIn);
    connect(
        pimpl->zoomIn,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::zoomIn
        );
    pimpl->zoomOut=new PushButton(pimpl->mainButtonsFrame);
    pimpl->zoomOut->setToolTip(tr("Zoom out"));
    pimpl->zoomOut->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::zoom-out",this));
    ml->addWidget(pimpl->zoomOut);
    connect(
        pimpl->zoomOut,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageViewer::zoomOut
    );

    // Shown only for animated content (see ImageViewer::updatePlayPauseButton()) -- hidden by
    // default since most images are static.
    pimpl->playPause=new PushButton(pimpl->mainButtonsFrame);
    pimpl->playPause->setToolTip(tr("Play/pause"));
    pimpl->playPause->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::pause",this));
    pimpl->playPause->setVisible(false);
    ml->addWidget(pimpl->playPause);
    connect(
        pimpl->playPause,
        &PushButton::clicked,
        pimpl->ctrl,
        &ImageViewer::togglePlay
    );

    pimpl->prevButton=new JumpEdge(this);
    pimpl->prevButton->setOrientation(Qt::Horizontal);
    pimpl->prevButton->setDirection(Direction::HOME);

    pimpl->nextButton=new JumpEdge(this);
    pimpl->nextButton->setOrientation(Qt::Horizontal);
    pimpl->nextButton->setDirection(Direction::END);

    pimpl->prevOpacityEffect=new QGraphicsOpacityEffect(pimpl->prevButton);
    pimpl->prevOpacityEffect->setOpacity(1.0);
    pimpl->prevButton->setGraphicsEffect(pimpl->prevOpacityEffect);
    pimpl->nextOpacityEffect=new QGraphicsOpacityEffect(pimpl->nextButton);
    pimpl->nextOpacityEffect->setOpacity(1.0);
    pimpl->nextButton->setGraphicsEffect(pimpl->nextOpacityEffect);

    cl->addStretch(1);

    pimpl->busySpinner=new CircleBusy(pimpl->contentFrame);
    pimpl->busySpinner->setObjectName("busySpinner");
    pimpl->busySpinner->stop();
    pimpl->busySpinner->setVisible(false);

    pimpl->loadingOverlayFrame=new QFrame(pimpl->contentFrame);
    pimpl->loadingOverlayFrame->setObjectName("loadingOverlayFrame");
    pimpl->loadingOverlayFrame->setVisible(false);
    auto lol=Layout::vertical(pimpl->loadingOverlayFrame);
    // centerOnParent=false: positioned explicitly in updateButtonPositions() instead, in the
    // corner rather than over the middle of the image. disableParentWhenSpinning=false: this
    // spinner shows WHILE a usable image is already displayed, so CircleBusy's default of
    // disabling its parent while running (see CircleBusy::start()) would freeze navigation for no
    // reason -- the whole point is that the user can keep browsing while a better version loads.
    pimpl->loadingOverlay=new CircleBusy(pimpl->loadingOverlayFrame,false,false);
    pimpl->loadingOverlay->setObjectName("loadingOverlay");
    pimpl->loadingOverlay->stop();
    lol->addWidget(pimpl->loadingOverlay);

    // Must run after loadingOverlayFrame above is constructed -- it reads pimpl->loadingOverlayFrame
    // directly (see its own body).
    updateButtonPositions();

    pimpl->controlsAnimation=new QPropertyAnimation(this,"controlsOpacity",this);
    connect(
        pimpl->controlsAnimation,
        &QPropertyAnimation::finished,
        this,
        [this]()
        {
            if (qFuzzyIsNull(pimpl->controlsAnimation->endValue().toReal()))
            {
                pimpl->controlsState=ImageViewerWidget_p::ControlsState::Hidden;
            }
            else
            {
                pimpl->controlsState=ImageViewerWidget_p::ControlsState::Visible;
            }
            updateControlsVisibility();
        }
    );
    pimpl->controlsHideTimer=new SingleShotTimer(this);

    setFocusPolicy(Qt::StrongFocus);
}

//--------------------------------------------------------------------------

ImageViewerWidget::~ImageViewerWidget()
{}

//--------------------------------------------------------------------------

void ImageViewerWidget::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    pimpl->ctrl->fitImage();
    updateButtonPositions();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::updateButtonPositions()
{
    auto r=pimpl->view->contentsRect();
    auto m=pimpl->styleSample->contentsMargins();

    auto buttonSize=pimpl->prevButton->size();

    auto y=pimpl->view->y()+r.center().y()-buttonSize.height()/2;
    auto prevX=pimpl->view->x()+m.left()+r.left();
    auto nextX=pimpl->view->x()+r.right()-buttonSize.width()-m.right();

    pimpl->prevButton->move(prevX,y);
    pimpl->nextButton->move(nextX,y);

    // Top-right corner of the image area, same rect the prev/next buttons above are anchored
    // from -- see ImageViewer::updateBusySpinner() for when this is actually shown.
    auto overlaySize=pimpl->loadingOverlayFrame->sizeHint();
    if (!overlaySize.isValid() || overlaySize.isEmpty())
    {
        overlaySize=pimpl->loadingOverlayFrame->size();
    }
    auto overlayMargin=8;
    auto overlayX=pimpl->view->x()+r.right()-overlaySize.width()-overlayMargin;
    auto overlayY=pimpl->view->y()+r.top()+overlayMargin;
    pimpl->loadingOverlayFrame->setGeometry(overlayX,overlayY,overlaySize.width(),overlaySize.height());

    if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay && pimpl->activeBottomWidget!=nullptr)
    {
        auto* bw=pimpl->activeBottomWidget;
        auto h=bw->sizeHint().height();
        if (h<=0)
        {
            h=bw->height();
        }
        auto bx=pimpl->view->x()+r.left();
        auto by=pimpl->view->y()+r.bottom()-h+1;
        bw->setGeometry(bx,by,r.width(),h);
    }
}

//--------------------------------------------------------------------------

void ImageViewerWidget::keyPressEvent(QKeyEvent* event)
{
    // Qt maps the macOS Command key onto Qt::ControlModifier by default, so a single
    // ControlModifier check covers Ctrl on Windows/Linux and Cmd on macOS -- mirrors
    // calendar.cpp's own hasControlModifier().
    bool ctrlOrCmd=event->modifiers().testFlag(Qt::ControlModifier)
                   || event->modifiers().testFlag(Qt::MetaModifier);

    if (event->key()==Qt::Key_Left)
    {
        pimpl->ctrl->showPrevImage();
        event->accept();
    }
    else if (event->key()==Qt::Key_Right)
    {
        pimpl->ctrl->showNextImage();
        event->accept();
    }
    else if (ctrlOrCmd && (event->key()==Qt::Key_Plus || event->key()==Qt::Key_Equal))
    {
        pimpl->ctrl->zoomIn();
        event->accept();
    }
    else if (ctrlOrCmd && event->key()==Qt::Key_Minus)
    {
        pimpl->ctrl->zoomOut();
        event->accept();
    }
    else if (ctrlOrCmd && event->key()==Qt::Key_0)
    {
        // ctrl->resetZoom(), not ctrl->fitImage() -- fitImage() is a no-op while already zoomed
        // in (see its own doc), which is precisely the state the user is asking to leave here.
        pimpl->ctrl->resetZoom();
        event->accept();
    }
    else if (event->key()==Qt::Key_Escape)
    {
        // isAutoRepeat() guard so holding Escape down emits closeRequested() exactly once, not
        // once per OS key-repeat tick -- the host closing a wrapping dialog on it shouldn't need
        // its own re-entrancy guard for that.
        if (!event->isAutoRepeat())
        {
            pimpl->ctrl->requestClose();
        }
        event->accept();
    }
    else
    {
        QFrame::keyPressEvent(event);
    }
}

//--------------------------------------------------------------------------

void ImageViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        pimpl->pressIsLeftButton=true;
        pimpl->pressPos=event->pos();
    }
    else
    {
        pimpl->pressIsLeftButton=false;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        handlePotentialViewerClick(event->pos());
    }
    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::isOnControls(const QPoint& pos) const
{
    // Walking ancestors of childAt(pos), rather than comparing geometries, is mode-independent
    // (the bottom widget is a layout child of contentFrame in Static mode but a direct floating
    // child of this in Overlay mode) and naturally ignores hidden widgets -- childAt() never
    // returns one, so a faded-out overlay bar does not block a click meant for the image beneath it.
    auto* w=childAt(pos);
    while (w!=nullptr && w!=this)
    {
        if (w==pimpl->activeBottomWidget || w==pimpl->prevButton || w==pimpl->nextButton)
        {
            return true;
        }
        w=w->parentWidget();
    }
    return false;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::handlePotentialViewerClick(const QPoint& pos)
{
    if (!pimpl->pressIsLeftButton)
    {
        return;
    }
    pimpl->pressIsLeftButton=false;

    // A drag/pan that happens to end outside the controls should not read as a click.
    if ((pos-pimpl->pressPos).manhattanLength()>QApplication::startDragDistance())
    {
        return;
    }

    if (isOnControls(pos))
    {
        return;
    }

    if (isInPrevNavigationZone(pos))
    {
        emit pimpl->prevButton->clicked();
        return;
    }
    if (isInNextNavigationZone(pos))
    {
        emit pimpl->nextButton->clicked();
        return;
    }

    emit pimpl->ctrl->viewerClicked();
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::isInPrevNavigationZone(const QPoint& pos) const
{
    // While zoomed in (pannable), the whole image area is a drag-to-pan surface -- letting a
    // short drag that starts in the edge strip fire prev/next navigation instead (as it would at
    // fit scale) would be maddening. See updateEdgeNavigationHover()'s own pannable branch for
    // the cursor side of this.
    if (pimpl->zoom!=nullptr && pimpl->zoom->isPannable())
    {
        return false;
    }

    // hasPrevImage(), not prevButton->isVisible() -- matches updateControlsVisibility()'s own
    // definition of "enabled" rather than the button's transient visibility during an
    // Overlay-mode fade (hovering the zone itself fades the controls back in, see
    // updateEdgeNavigationHover()'s notifyActivity() call). Also correctly stays enabled at
    // index 0 when the flyweight window hasMoreBefore().
    if (!pimpl->ctrl->hasPrevImage())
    {
        return false;
    }
    auto r=pimpl->view->contentsRect().translated(pimpl->view->pos());
    if (pos.y()<r.top() || pos.y()>r.bottom())
    {
        return false;
    }
    return pos.x()>=r.left() && pos.x()<r.left()+pimpl->edgeNavigationZoneWidth;
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::isInNextNavigationZone(const QPoint& pos) const
{
    if (pimpl->zoom!=nullptr && pimpl->zoom->isPannable())
    {
        return false;
    }

    if (!pimpl->ctrl->hasNextImage())
    {
        return false;
    }
    auto r=pimpl->view->contentsRect().translated(pimpl->view->pos());
    if (pos.y()<r.top() || pos.y()>r.bottom())
    {
        return false;
    }
    return pos.x()<=r.right() && pos.x()>r.right()-pimpl->edgeNavigationZoneWidth;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::updateEdgeNavigationHover(const QPoint& pos)
{
    if (pimpl->zoom!=nullptr && pimpl->zoom->isPannable())
    {
        // Zoomed in: no forced hover on the prev/next buttons (isInPrevNavigationZone()/
        // isInNextNavigationZone() already return false in this state), and the cursor reflects
        // pan affordance/state instead of edge-navigation intent. GraphicsViewZoom itself never
        // touches the viewport cursor (setCursorManaged(false), see the constructor) specifically
        // so this stays the single owner of it.
        clearEdgeNavigationHover();
        if (pimpl->view!=nullptr)
        {
            pimpl->view->viewport()->setCursor(
                pimpl->zoom->isPanning() ? Qt::ClosedHandCursor : Qt::OpenHandCursor
            );
        }
        return;
    }

    auto prevHovered=isInPrevNavigationZone(pos);
    auto nextHovered=!prevHovered && isInNextNavigationZone(pos);

    if (prevHovered!=pimpl->prevZoneHovered)
    {
        pimpl->prevZoneHovered=prevHovered;
        pimpl->prevButton->setForceHovered(prevHovered);
    }
    if (nextHovered!=pimpl->nextZoneHovered)
    {
        pimpl->nextZoneHovered=nextHovered;
        pimpl->nextButton->setForceHovered(nextHovered);
    }

    if (pimpl->view!=nullptr)
    {
        auto* viewport=pimpl->view->viewport();
        if (prevHovered || nextHovered)
        {
            viewport->setCursor(Qt::PointingHandCursor);
        }
        else
        {
            viewport->unsetCursor();
        }
    }
}

//--------------------------------------------------------------------------

void ImageViewerWidget::clearEdgeNavigationHover()
{
    if (pimpl->prevZoneHovered)
    {
        pimpl->prevZoneHovered=false;
        pimpl->prevButton->setForceHovered(false);
    }
    if (pimpl->nextZoneHovered)
    {
        pimpl->nextZoneHovered=false;
        pimpl->nextButton->setForceHovered(false);
    }
    if (pimpl->view!=nullptr)
    {
        pimpl->view->viewport()->unsetCursor();
    }
}

//--------------------------------------------------------------------------

int ImageViewerWidget::edgeNavigationZoneWidth() const
{
    return pimpl->edgeNavigationZoneWidth;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setEdgeNavigationZoneWidth(int value)
{
    pimpl->edgeNavigationZoneWidth=value;
}

//--------------------------------------------------------------------------

qreal ImageViewerWidget::minDisplayPixels() const
{
    return pimpl->zoom->minDisplayPixels();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setMinDisplayPixels(qreal value)
{
    pimpl->zoom->setMinDisplayPixels(value);
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::MouseMove:
        {
            if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
            {
                notifyActivity();
            }
            auto* mouseEvent=static_cast<QMouseEvent*>(event);
            updateEdgeNavigationHover(mouseEvent->pos());
            break;
        }

        case QEvent::HoverMove:
        case QEvent::Enter:
            if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
            {
                notifyActivity();
            }
            break;

        case QEvent::Leave:
            clearEdgeNavigationHover();
            break;

        default:
            break;
    }

    return WidgetQFrame::event(event);
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::eventFilter(QObject* watched, QEvent* event)
{
    // watched==viewport() branches run in both control modes -- viewerClicked() detection and
    // edge navigation hover/click are not Overlay-only features, unlike the activity-notify
    // (auto-hide) reaction below.
    if (pimpl->view!=nullptr && watched==pimpl->view->viewport())
    {
        switch (event->type())
        {
            case QEvent::Resize:
                // The viewport's OWN resize, not the outer view's/this widget's (resizeEvent()
                // above already calls fitImage() on those, too early during the very first
                // show -- see fitImage()'s own comment on view->viewport()->rect() for the full
                // layout-order explanation). This is the authoritative point at which
                // QGraphicsView::fitInView() will actually have a correctly-sized viewport to
                // measure against, so re-running fitImage() here is what makes a session whose
                // full-size pixmap is already set BEFORE the window's first show (e.g.
                // ChatImageViewerController::openStandalone(), which never goes through an
                // async producer callback arriving after layout the way the chained viewer
                // does) end up at the right initial zoom instead of stuck at whatever fitImage()
                // computed against the stale placeholder viewport during construction.
                pimpl->ctrl->fitImage();
                break;

            case QEvent::MouseMove:
            {
                if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
                {
                    notifyActivity();
                }
                auto* mouseEvent=static_cast<QMouseEvent*>(event);
                updateEdgeNavigationHover(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
                break;
            }

            case QEvent::HoverMove:
            case QEvent::Enter:
                if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
                {
                    notifyActivity();
                }
                break;

            case QEvent::Leave:
                clearEdgeNavigationHover();
                break;

            case QEvent::MouseButtonPress:
            {
                auto* mouseEvent=static_cast<QMouseEvent*>(event);
                if (mouseEvent->button()==Qt::LeftButton)
                {
                    pimpl->pressIsLeftButton=true;
                    pimpl->pressPos=mapFromGlobal(mouseEvent->globalPosition().toPoint());
                }
                else
                {
                    pimpl->pressIsLeftButton=false;
                }
                break;
            }

            case QEvent::MouseButtonRelease:
            {
                auto* mouseEvent=static_cast<QMouseEvent*>(event);
                if (mouseEvent->button()==Qt::LeftButton)
                {
                    handlePotentialViewerClick(mapFromGlobal(mouseEvent->globalPosition().toPoint()));
                }
                break;
            }

            default:
                break;
        }
    }
    else if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay
             && watched==pimpl->activeBottomWidget && event->type()==QEvent::LayoutRequest)
    {
        // The bottom widget is unmanaged by any outer QLayout while overlaid (we position it
        // ourselves via setGeometry() in updateButtonPositions()), so when ITS OWN internal
        // layout decides its sizeHint changed (e.g. ChatImageViewerControls' album strip
        // gaining/losing rows), Qt delivers this event straight to the widget instead of an
        // ancestor layout silently absorbing it -- without reacting to it here, the bar stays
        // sized/positioned from whatever it was the last time updateButtonPositions() happened
        // to run, which is what showed up as broken/glitchy layout right after content changed.
        updateButtonPositions();
    }

    return WidgetQFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setControlsMode(AbstractImageViewer::ControlsMode mode)
{
    if (pimpl->controlsMode==mode)
    {
        return;
    }

    pimpl->controlsMode=mode;
    applyControlsMode();
}

//--------------------------------------------------------------------------

AbstractImageViewer::ControlsMode ImageViewerWidget::controlsMode() const noexcept
{
    return pimpl->controlsMode;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setBottomWidget(QWidget* widget)
{
    auto* newWidget=widget!=nullptr ? widget : static_cast<QWidget*>(pimpl->controlsFrame);
    if (newWidget==pimpl->activeBottomWidget)
    {
        pimpl->customBottomWidget=widget;
        return;
    }

    auto* oldWidget=pimpl->activeBottomWidget;
    if (oldWidget!=nullptr)
    {
        pimpl->layout->removeWidget(oldWidget);
        oldWidget->removeEventFilter(this);
        // Deletes any overlay opacity effect the old widget had -- pimpl->bottomOpacityEffect
        // would otherwise dangle, so drop it too; applyControlsMode() below installs a fresh one
        // on the new widget if still needed.
        oldWidget->setGraphicsEffect(nullptr);
        pimpl->bottomOpacityEffect=nullptr;
        oldWidget->setParent(this);
        oldWidget->setVisible(false);
    }

    pimpl->customBottomWidget=widget;
    pimpl->activeBottomWidget=newWidget;

    applyControlsMode();
}

//--------------------------------------------------------------------------

QWidget* ImageViewerWidget::bottomWidget() const
{
    return pimpl->customBottomWidget;
}

//--------------------------------------------------------------------------

qreal ImageViewerWidget::controlsOpacity() const
{
    return pimpl->controlsOpacity;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setControlsOpacity(qreal value)
{
    pimpl->controlsOpacity=value;

    if (pimpl->bottomOpacityEffect!=nullptr)
    {
        pimpl->bottomOpacityEffect->setOpacity(value);
    }
    pimpl->prevOpacityEffect->setOpacity(value);
    pimpl->nextOpacityEffect->setOpacity(value);
}

//--------------------------------------------------------------------------

int ImageViewerWidget::controlsFadeInDurationMs() const
{
    return pimpl->controlsFadeInDurationMs;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setControlsFadeInDurationMs(int value)
{
    pimpl->controlsFadeInDurationMs=value;
}

//--------------------------------------------------------------------------

int ImageViewerWidget::controlsFadeOutDurationMs() const
{
    return pimpl->controlsFadeOutDurationMs;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setControlsFadeOutDurationMs(int value)
{
    pimpl->controlsFadeOutDurationMs=value;
}

//--------------------------------------------------------------------------

qreal ImageViewerWidget::controlsMaxOpacity() const
{
    return pimpl->controlsMaxOpacity;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::setControlsMaxOpacity(qreal value)
{
    pimpl->controlsMaxOpacity=value;
}

//--------------------------------------------------------------------------

void ImageViewerWidget::applyControlsMode()
{
    pimpl->controlsAnimation->stop();
    pimpl->controlsHideTimer->cancel();

    // Lets QSS give the embedded controlsFrame an overlay-appropriate look (e.g. a translucent
    // dark backing) only while it is actually floating over the image -- see imageviewer.qss.
    setProperty("overlay",pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay);
    Style::repolishRecursive(this);

    auto* bw=pimpl->activeBottomWidget;

    if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Static)
    {
        // Note: the viewport event filter and mouse tracking both stay on (see the constructor)
        // even in Static mode -- they are also how viewerClicked() and the edge navigation zones
        // detect clicks/hover on the image area, not just Overlay's own activity tracking.
        if (bw!=nullptr)
        {
            bw->removeEventFilter(this);
            bw->setGraphicsEffect(nullptr);
            pimpl->bottomOpacityEffect=nullptr;
            bw->setParent(pimpl->contentFrame);
            pimpl->layout->addWidget(bw);
            bw->setVisible(true);
        }
        pimpl->prevOpacityEffect->setOpacity(1.0);
        pimpl->nextOpacityEffect->setOpacity(1.0);

        pimpl->controlsState=ImageViewerWidget_p::ControlsState::Visible;
    }
    else
    {
        if (bw!=nullptr)
        {
            pimpl->layout->removeWidget(bw);
            bw->setParent(this);

            pimpl->bottomOpacityEffect=new QGraphicsOpacityEffect(bw);
            bw->setGraphicsEffect(pimpl->bottomOpacityEffect);

            // See eventFilter()'s QEvent::LayoutRequest branch: bw is unmanaged by any outer
            // QLayout here, so this is what lets its own internal content changes (e.g. the
            // album strip's sizeHint changing) trigger a fresh updateButtonPositions() call.
            bw->installEventFilter(this);
        }

        pimpl->controlsState=ImageViewerWidget_p::ControlsState::Hidden;
        setControlsOpacity(0.0);
        showControls();
    }

    updateControlsVisibility();
    updateButtonPositions();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::updateControlsVisibility()
{
    // hasPrevImage()/hasNextImage(), not raw index comparisons -- a flyweight window can have
    // more images before/after the loaded range (see AbstractImageViewer::hasMoreBefore()/
    // hasMoreAfter()), which must keep the button enabled even at index 0 or the last loaded index.
    bool hasPrev=pimpl->ctrl->hasPrevImage();
    bool hasNext=pimpl->ctrl->hasNextImage();

    if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Static)
    {
        pimpl->prevButton->setVisible(hasPrev);
        pimpl->nextButton->setVisible(hasNext);
        return;
    }

    bool show=pimpl->controlsState==ImageViewerWidget_p::ControlsState::Visible
            || pimpl->controlsState==ImageViewerWidget_p::ControlsState::FadingIn;

    if (pimpl->activeBottomWidget!=nullptr)
    {
        pimpl->activeBottomWidget->setVisible(show);
    }

    pimpl->prevButton->setVisible(show && hasPrev);
    pimpl->nextButton->setVisible(show && hasNext);
}

//--------------------------------------------------------------------------

void ImageViewerWidget::fadeControlsIn()
{
    if (pimpl->controlsMode!=AbstractImageViewer::ControlsMode::Overlay)
    {
        return;
    }

    pimpl->controlsState=ImageViewerWidget_p::ControlsState::FadingIn;
    updateControlsVisibility();
    updateButtonPositions();

    if (pimpl->activeBottomWidget!=nullptr)
    {
        pimpl->activeBottomWidget->raise();
    }
    pimpl->prevButton->raise();
    pimpl->nextButton->raise();

    pimpl->controlsAnimation->stop();
    pimpl->controlsAnimation->setDuration(controlsFadeInDurationMs());
    pimpl->controlsAnimation->setStartValue(controlsOpacity());
    pimpl->controlsAnimation->setEndValue(controlsMaxOpacity());
    pimpl->controlsAnimation->start();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::fadeControlsOut()
{
    if (pimpl->controlsMode!=AbstractImageViewer::ControlsMode::Overlay)
    {
        return;
    }

    // Never fade out while the pointer is resting on one of the overlay widgets -- re-arm instead.
    if ((pimpl->activeBottomWidget!=nullptr && pimpl->activeBottomWidget->underMouse())
        || pimpl->prevButton->underMouse()
        || pimpl->nextButton->underMouse())
    {
        pimpl->controlsHideTimer->shot(
            static_cast<size_t>(pimpl->ctrl->controlsAutoHideDelayMs()),
            [this](){fadeControlsOut();},
            true
        );
        return;
    }

    pimpl->controlsState=ImageViewerWidget_p::ControlsState::FadingOut;

    pimpl->controlsAnimation->stop();
    pimpl->controlsAnimation->setDuration(controlsFadeOutDurationMs());
    pimpl->controlsAnimation->setStartValue(controlsOpacity());
    pimpl->controlsAnimation->setEndValue(0.0);
    pimpl->controlsAnimation->start();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::notifyActivity()
{
    showControls();
}

//--------------------------------------------------------------------------

void ImageViewerWidget::showControls()
{
    if (pimpl->controlsMode!=AbstractImageViewer::ControlsMode::Overlay)
    {
        return;
    }

    if (pimpl->controlsState==ImageViewerWidget_p::ControlsState::Hidden
        || pimpl->controlsState==ImageViewerWidget_p::ControlsState::FadingOut)
    {
        fadeControlsIn();
    }

    pimpl->controlsHideTimer->shot(
        static_cast<size_t>(pimpl->ctrl->controlsAutoHideDelayMs()),
        [this](){fadeControlsOut();},
        true
    );
}

//--------------------------------------------------------------------------

void ImageViewerWidget::hideControls()
{
    if (pimpl->controlsMode!=AbstractImageViewer::ControlsMode::Overlay)
    {
        return;
    }

    pimpl->controlsHideTimer->cancel();
    fadeControlsOut();
}

/************************** ImageViewer *****************************/

//--------------------------------------------------------------------------

//! ImageAnimator subclass reporting m_widget's own visibility/window-activation state -- see the
//! forward declaration's doc in imageviewer.hpp. isHovered() is deliberately left at the base's
//! default (always false): AnimationMode::OnHover reads naturally for a small thumbnail (see
//! ImageLabel) but not for a full-screen viewer, and this feature's toolbar-driven design (Auto
//! play + an explicit play/pause button) never needs it.
class ImageViewer::ViewerAnimator : public ImageAnimator
{
    public:

        explicit ViewerAnimator(ImageViewer* owner)
            // No QObject parent: same ownership rationale as ImageLabel's own animator member --
            // owner holds this exclusively via std::unique_ptr.
            : ImageAnimator(nullptr),
              m_owner(owner)
        {}

    protected:

        bool isWidgetVisible() const override
        {
            return m_owner->m_widget!=nullptr && m_owner->m_widget->isVisible();
        }

        bool isWindowActive() const override
        {
            return m_owner->m_widget==nullptr
                   || m_owner->m_widget->window()==nullptr
                   || m_owner->m_widget->window()->isActiveWindow();
        }

    private:

        ImageViewer* m_owner;
};

//--------------------------------------------------------------------------

ImageViewer::ImageViewer(QObject* parent)
    : AbstractImageViewer(parent)
{
    m_animator=std::make_unique<ViewerAnimator>(this);

    connect(
        m_animator.get(),
        &ImageAnimator::frameChanged,
        this,
        &ImageViewer::onAnimatorFrameChanged
    );
    connect(
        m_animator.get(),
        &ImageAnimator::playingChanged,
        this,
        &ImageViewer::onAnimatorPlayingChanged
    );
    connect(
        m_animator.get(),
        &ImageAnimator::animatedChanged,
        this,
        &ImageViewer::onAnimatorAnimatedChanged
    );
}

//--------------------------------------------------------------------------

Widget* ImageViewer::doCreateActualWidget(QWidget* parent)
{
    m_widget=new ImageViewerWidget(this,parent);
    reset();

    connect(
        m_widget->pimpl->prevButton,
        &JumpEdge::clicked,
        this,
        &ImageViewer::showPrevImage
    );

    connect(
        m_widget->pimpl->nextButton,
        &JumpEdge::clicked,
        this,
        &ImageViewer::showNextImage
    );

    connect(
        m_widget->pimpl->zoom,
        &GraphicsViewZoom::zoomChanged,
        this,
        &ImageViewer::zoomChanged
    );

    m_widget->setControlsMode(controlsMode());

    updateBusySpinner();
    m_widget->updateButtonPositions();
    updatePrevNextButtons();

    return m_widget;
}

//--------------------------------------------------------------------------

void ImageViewer::reset()
{
    doReset();
}

//--------------------------------------------------------------------------

void ImageViewer::doReset()
{
    m_widget->pimpl->scene->clear();
    m_widget->pimpl->view->update();
    m_widget->pimpl->view->resetTransform();
    m_widget->pimpl->view->setSceneRect(QRectF{});
    m_widget->pimpl->imageItem = nullptr;
    m_widget->pimpl->angle=0;
    m_widget->pimpl->zoom->setFitItem(nullptr);

    m_animator->clear();
    m_animatorKey=PixmapKey{};
}

//--------------------------------------------------------------------------

void ImageViewer::rotate()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.rotate(-90);
    m_widget->pimpl->angle-=90;
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::rotateClockwise()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.rotate(90);
    m_widget->pimpl->angle+=90;
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::flipHorizontal()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    if (m_widget->pimpl->angle%180==0)
    {
        t.scale(-1, 1);
    }
    else
    {
        t.scale(1, -1);
    }
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::flipVertical()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    if (m_widget->pimpl->angle%180==0)
    {
        t.scale(1, -1);
    }
    else
    {
        t.scale(-1, 1);
    }
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::zoomIn()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    m_widget->pimpl->zoom->zoomIn();
}

//--------------------------------------------------------------------------

void ImageViewer::zoomOut()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    m_widget->pimpl->zoom->zoomOut();
}

//--------------------------------------------------------------------------

void ImageViewer::resetZoom()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    m_widget->pimpl->zoom->resetZoom();
    m_widget->updateButtonPositions();
}

//--------------------------------------------------------------------------

void ImageViewer::doSelectImage()
{
    doReset();
    syncAnimatorToCurrentImage();
    applyCurrentPixmap();
    fitImage();
    updateBusySpinner();
    updatePrevNextButtons();
    updatePlayPauseButton();
    m_widget->showControls();
    m_widget->setFocus();
}

//--------------------------------------------------------------------------

void ImageViewer::applyCurrentPixmap()
{
    QPixmap px;
    if (m_animator->isAnimated())
    {
        auto frame=m_animator->currentFrame();
        if (!frame.isNull())
        {
            px=QPixmap::fromImage(frame);
        }
    }
    if (px.isNull())
    {
        px=currentImage();
    }

    if (px.isNull())
    {
        if (m_widget->pimpl->imageItem!=nullptr)
        {
            m_widget->pimpl->scene->removeItem(m_widget->pimpl->imageItem);
            delete m_widget->pimpl->imageItem;
            m_widget->pimpl->imageItem=nullptr;
            m_widget->pimpl->zoom->setFitItem(nullptr);
        }
        return;
    }

    // Create the scene item lazily the first time a non-null pixmap becomes available, rather
    // than only at doSelectImage() time -- doSelectImage() itself calls this too, so a
    // still-loading image (no producer pixmap yet, no seed content) that only gets one later via
    // onPixmapUpdated() now actually appears instead of being silently dropped forever (the
    // previous onPixmapUpdated() required imageItem to already be non-null to update it).
    if (m_widget->pimpl->imageItem==nullptr)
    {
        m_widget->pimpl->imageItem=m_widget->pimpl->scene->addPixmap(px);
        // addPixmap() defaults to Qt::FastTransformation (nearest-neighbour); QGraphicsPixmapItem::
        // paint() sets/clears QPainter::SmoothPixmapTransform straight from this flag, so the
        // view's own render hints cannot override it. Below "fit" this item can now be scaled down
        // to minDisplayPixels() (see the constructor's setMinDisplayPixels() call), where nearest-
        // neighbour looks noticeably worse than at/above fit.
        m_widget->pimpl->imageItem->setTransformationMode(Qt::SmoothTransformation);
    }
    else
    {
        m_widget->pimpl->imageItem->setPixmap(px);
    }
    m_widget->pimpl->zoom->setFitItem(m_widget->pimpl->imageItem);
}

//--------------------------------------------------------------------------

void ImageViewer::syncAnimatorToCurrentImage()
{
    auto key=currentImageKey();
    if (key==m_animatorKey)
    {
        return;
    }
    m_animatorKey=key;

    auto animation=currentImageAnimation();
    if (animation.isNull())
    {
        m_animator->clear();
        return;
    }
    m_animator->loadContent(animation);
}

//--------------------------------------------------------------------------

void ImageViewer::onAnimationUpdated(const PixmapKey& key)
{
    if (m_widget==nullptr || !(key==currentImageKey()))
    {
        return;
    }

    // Force syncAnimatorToCurrentImage() to reconsider even though it already ran for this key at
    // selection time -- this override exists specifically for animation content that arrives
    // asynchronously (a version-ladder-style source) after the image was already selected with
    // none yet available.
    m_animatorKey=PixmapKey{};
    syncAnimatorToCurrentImage();

    applyCurrentPixmap();
    fitImage();
    updatePlayPauseButton();
}

//--------------------------------------------------------------------------

bool ImageViewer::isCurrentImageAnimated() const
{
    return m_animator->isAnimated();
}

//--------------------------------------------------------------------------

bool ImageViewer::isCurrentImagePlaying() const
{
    return m_animator->isPlaying();
}

//--------------------------------------------------------------------------

void ImageViewer::setAnimationMode(ImageAnimator::AnimationMode mode)
{
    m_animator->setAnimationMode(mode);
}

//--------------------------------------------------------------------------

ImageAnimator::AnimationMode ImageViewer::animationMode() const
{
    return m_animator->animationMode();
}

//--------------------------------------------------------------------------

void ImageViewer::setAnimationSpeed(int percent)
{
    m_animator->setAnimationSpeed(percent);
}

//--------------------------------------------------------------------------

int ImageViewer::animationSpeed() const
{
    return m_animator->animationSpeed();
}

//--------------------------------------------------------------------------

void ImageViewer::play()
{
    m_animator->play();
}

//--------------------------------------------------------------------------

void ImageViewer::pause()
{
    m_animator->pause();
}

//--------------------------------------------------------------------------

void ImageViewer::stop()
{
    m_animator->stop();
}

//--------------------------------------------------------------------------

void ImageViewer::togglePlay()
{
    m_animator->togglePlay();
}

//--------------------------------------------------------------------------

void ImageViewer::onAnimatorFrameChanged()
{
    if (m_widget==nullptr)
    {
        return;
    }
    // A frame tick during ongoing playback -- update the displayed pixmap in place without
    // re-fitting (fitImage() would reset the user's current zoom/pan on every frame).
    applyCurrentPixmap();
}

//--------------------------------------------------------------------------

void ImageViewer::onAnimatorPlayingChanged(bool playing)
{
    std::ignore=playing;
    updatePlayPauseButton();
}

//--------------------------------------------------------------------------

void ImageViewer::onAnimatorAnimatedChanged(bool animated)
{
    std::ignore=animated;
    updatePlayPauseButton();
}

//--------------------------------------------------------------------------

void ImageViewer::updatePlayPauseButton()
{
    if (m_widget!=nullptr)
    {
        bool animated=m_animator->isAnimated();
        m_widget->pimpl->playPause->setVisible(animated);
        if (animated)
        {
            auto iconName=m_animator->isPlaying() ? QStringLiteral("pause") : QStringLiteral("play");
            m_widget->pimpl->playPause->setSvgIcon(
                Style::instance().svgIconLocator().icon(QString("ImageEditor::%1").arg(iconName),m_widget)
            );
        }
    }

    // Emitted regardless of m_widget's existence -- ChatImageViewer's own bottom-widget button
    // (ChatImageViewerControls, which replaces this class' embedded toolbar entirely) listens for
    // this independently of whether the embedded playPause button above exists.
    emit currentImageAnimationStateChanged();
}

//--------------------------------------------------------------------------

void ImageViewer::updatePrevNextButtons()
{
    m_widget->updateControlsVisibility();
}

//--------------------------------------------------------------------------

void ImageViewer::refreshOverlayGeometry()
{
    if (m_widget!=nullptr)
    {
        m_widget->updateButtonPositions();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::fitImage()
{
    if (m_widget==nullptr)
    {
        return;
    }

    // imageItem!=nullptr alone, not also a currentImage()-non-null check -- applyCurrentPixmap()
    // can populate imageItem purely from an animator frame while currentImage() is still null
    // (animated-only content, see its own m_animator->isAnimated() branch), and that content
    // needs fitting exactly the same as a static image does.
    if (m_widget->pimpl->imageItem!=nullptr)
    {
        auto* zoom=m_widget->pimpl->zoom;

        // Keep whatever scene point the viewport is currently centred on pinned in place across
        // the scene rect change below -- otherwise a higher-resolution pixmap landing
        // asynchronously while zoomed in (a version-ladder upgrade, see PixmapSource) would jump
        // the visible area.
        zoom->keepViewportCenter(
            [this]()
            {
                m_widget->pimpl->scene->setSceneRect(m_widget->pimpl->imageItem->boundingRect());
            }
        );

        // GraphicsViewZoom::baselineScale()/fitToItem() measure against view->viewport()->rect(),
        // not view->rect() -- during the widget's first show the outer view is laid out (and
        // resized) a step ahead of its viewport child (QAbstractScrollArea only relays a resize
        // into the viewport once ITS OWN QEvent::Resize is delivered, see the
        // ImageViewerWidget::eventFilter() QEvent::Resize case below, which exists specifically
        // to re-run this once that catches up), so this stays correct across the same
        // layout-order hazard that motivated measuring against the viewport in the first place.
        //
        // isUserZoomed(), not isZoomed() -- isZoomed() is derived purely from the current
        // transform vs. a freshly computed baselineScale(), so right after doReset() (identity
        // transform) it reads true for any image larger than the viewport, indistinguishable from
        // a deliberate zoom. isUserZoomed() tracks actual zoom actions instead, so a never-yet-
        // fitted view always gets fitted here, while a real user zoom survives every subsequent
        // resize/async-pixmap-upgrade call to this function untouched.
        if (!zoom->isUserZoomed())
        {
            zoom->fitToItem();
        }

        m_widget->updateButtonPositions();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::onPixmapUpdated(const PixmapKey& key)
{
    if (key==currentImageKey())
    {
        applyCurrentPixmap();
        fitImage();
    }
    updateBusySpinner();
}

//--------------------------------------------------------------------------

void ImageViewer::onPixmapLoadingChanged(const PixmapKey& key, bool loading)
{
    std::ignore=loading;
    if (key==currentImageKey())
    {
        updateBusySpinner();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::onWindowChanged()
{
    if (m_widget==nullptr)
    {
        return;
    }
    updateBusySpinner();
    updatePrevNextButtons();
}

//--------------------------------------------------------------------------

void ImageViewer::updateBusySpinner()
{
    if (imageCount()==0)
    {
        // Defensive guard -- an empty window has nothing to load and never will (its
        // currentKey is null, so no fetch can ever resolve one), so a spinner is never the
        // right state here regardless of who emptied it. The one caller who empties the
        // window this way (ChatImageViewerController::onChatMessageEvent()) already closes
        // the viewer itself; this only stops any other/future emptying path from stranding
        // the blocking spinner below.
        m_widget->pimpl->busySpinner->setVisible(false);
        m_widget->pimpl->busySpinner->stop();
        m_widget->pimpl->loadingOverlayFrame->setVisible(false);
        m_widget->pimpl->loadingOverlay->stop();
        return;
    }

    auto px=currentImage();
    if (px.isNull() && !m_animator->isAnimated())
    {
        // No usable pixmap at all yet -- the original, large, centred, blocking spinner. An
        // animated image with no static poster pixmap (content never seeded/delivered, only
        // animation) still counts as "usable": applyCurrentPixmap() already shows its first/
        // current frame instead of currentImage() in that case.
        m_widget->pimpl->busySpinner->setVisible(true);
        m_widget->pimpl->busySpinner->start();
        m_widget->pimpl->loadingOverlayFrame->setVisible(false);
        m_widget->pimpl->loadingOverlay->stop();
        return;
    }

    m_widget->pimpl->busySpinner->setVisible(false);
    m_widget->pimpl->busySpinner->stop();

    // A usable (possibly seed/lower-rung) pixmap is already shown -- only the small, non-blocking
    // corner overlay is appropriate here, and only while a better version is still being fetched
    // (isCurrentImageLoading(), see PixmapSource::setPixmapLoading()) or navigation is waiting on
    // a fetch past a loaded window edge (isNavigationPending()).
    bool showOverlay=isCurrentImageLoading() || isNavigationPending();
    m_widget->pimpl->loadingOverlayFrame->setVisible(showOverlay);
    if (showOverlay)
    {
        m_widget->pimpl->loadingOverlay->start();
    }
    else
    {
        m_widget->pimpl->loadingOverlay->stop();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::setControlsMode(ControlsMode mode)
{
    AbstractImageViewer::setControlsMode(mode);
    if (m_widget!=nullptr)
    {
        m_widget->setControlsMode(mode);
    }
}

//--------------------------------------------------------------------------

void ImageViewer::setBottomWidget(QWidget* widget)
{
    if (m_widget!=nullptr)
    {
        m_widget->setBottomWidget(widget);
    }
}

//--------------------------------------------------------------------------

QWidget* ImageViewer::bottomWidget() const
{
    if (m_widget!=nullptr)
    {
        return m_widget->bottomWidget();
    }
    return nullptr;
}

//--------------------------------------------------------------------------

void ImageViewer::showControls()
{
    if (m_widget!=nullptr)
    {
        m_widget->showControls();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::hideControls()
{
    if (m_widget!=nullptr)
    {
        m_widget->hideControls();
    }
}

//--------------------------------------------------------------------------

}
