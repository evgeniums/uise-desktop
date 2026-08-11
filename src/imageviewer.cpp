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
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/jumpedge.hpp>
#include <uise/desktop/circlebusy.hpp>
#include <uise/desktop/imageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* ImageViewerWidget *****************************/

class ImageViewerWidget_p
{
    public:

        //! Mirrors ChatDateSubtitle_p::State -- same fade-in-on-activity / fade-out-on-idle shape.
        enum class ControlsState : int
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

        QFrame* controlsFrame;
        QFrame* mainButtonsFrame;
        PushButton* rotate;
        PushButton* rotateClockwise;
        PushButton* flipHorizontal;
        PushButton* flipVertical;
        PushButton* zoomIn;
        PushButton* zoomOut;

        int angle=0;

        JumpEdge* prevButton;
        JumpEdge* nextButton;

        QLabel* styleSample;

        CircleBusy* busySpinner;

        qreal scale=1.0;

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

    updateButtonPositions();

    pimpl->busySpinner=new CircleBusy(pimpl->contentFrame);
    pimpl->busySpinner->stop();
    pimpl->busySpinner->setVisible(false);

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

bool ImageViewerWidget::event(QEvent* event)
{
    if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
    {
        switch (event->type())
        {
            case QEvent::MouseMove:
            case QEvent::HoverMove:
            case QEvent::Enter:
                notifyActivity();
                break;

            default:
                break;
        }
    }

    return WidgetQFrame::event(event);
}

//--------------------------------------------------------------------------

bool ImageViewerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (pimpl->controlsMode==AbstractImageViewer::ControlsMode::Overlay)
    {
        if (pimpl->view!=nullptr && watched==pimpl->view->viewport())
        {
            switch (event->type())
            {
                case QEvent::MouseMove:
                case QEvent::HoverMove:
                case QEvent::Enter:
                    notifyActivity();
                    break;

                default:
                    break;
            }
        }
        else if (watched==pimpl->activeBottomWidget && event->type()==QEvent::LayoutRequest)
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
        if (pimpl->view!=nullptr)
        {
            pimpl->view->viewport()->removeEventFilter(this);
            pimpl->view->viewport()->setMouseTracking(false);
        }
        setMouseTracking(false);

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
        setMouseTracking(true);
        if (pimpl->view!=nullptr)
        {
            pimpl->view->viewport()->setMouseTracking(true);
            pimpl->view->viewport()->installEventFilter(this);
        }

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
    bool hasPrev=pimpl->ctrl->currentImageIndex()>0;
    bool hasNext=(pimpl->ctrl->currentImageIndex()+1)<pimpl->ctrl->imageCount();

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
    m_widget->pimpl->scale=1.0;
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

    auto t=m_widget->pimpl->view->transform();
    t.scale(1.25, 1.25);
    m_widget->pimpl->scale=m_widget->pimpl->scale*1.25;
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::zoomOut()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.scale(0.8, 0.8);
    m_widget->pimpl->scale=m_widget->pimpl->scale*0.8;
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::doSelectImage()
{
    doReset();
    auto px=currentImage();
    if (!px.isNull())
    {
        m_widget->pimpl->imageItem=m_widget->pimpl->scene->addPixmap(px);
    }
    fitImage();
    updateBusySpinner();
    updatePrevNextButtons();
    m_widget->showControls();
    m_widget->setFocus();
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
    auto px=currentImage();
    if (!px.isNull() && m_widget->pimpl->imageItem!=nullptr)
    {
        m_widget->pimpl->scene->setSceneRect(m_widget->pimpl->imageItem->boundingRect());
        auto viewRect=m_widget->pimpl->view->rect();
        if (qFuzzyCompare(m_widget->pimpl->scale,1.0) && (px.width()>viewRect.width() || px.height() > viewRect.height()))
        {
            m_widget->pimpl->view->fitInView(m_widget->pimpl->imageItem, Qt::KeepAspectRatio);
        }
        m_widget->updateButtonPositions();
    }
}

//--------------------------------------------------------------------------

void ImageViewer::onPixmapUpdated(const PixmapKey& key)
{
    if (key==currentImageKey() && m_widget->pimpl->imageItem!=nullptr)
    {
        m_widget->pimpl->imageItem->setPixmap(currentImage());
        fitImage();
    }
    updateBusySpinner();
}

//--------------------------------------------------------------------------

void ImageViewer::updateBusySpinner()
{
    auto px=currentImage();
    if (px.isNull())
    {
        m_widget->pimpl->busySpinner->setVisible(true);
        m_widget->pimpl->busySpinner->start();
    }
    else
    {
        m_widget->pimpl->busySpinner->setVisible(false);
        m_widget->pimpl->busySpinner->stop();
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

UISE_DESKTOP_NAMESPACE_END
