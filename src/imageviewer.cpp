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
#include <QKeyEvent>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <uise/desktop/utils/layout.hpp>
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

    cl->addStretch(1);

    updateButtonPositions();

    pimpl->busySpinner=new CircleBusy(pimpl->contentFrame);
    pimpl->busySpinner->stop();
    pimpl->busySpinner->setVisible(false);

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
    else
    {
        QFrame::keyPressEvent(event);
    }
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
    m_widget->setFocus();
}

//--------------------------------------------------------------------------

void ImageViewer::updatePrevNextButtons()
{
    m_widget->pimpl->prevButton->setVisible(currentImageIndex()>0);
    m_widget->pimpl->nextButton->setVisible((currentImageIndex()+1)<imageCount());
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

UISE_DESKTOP_NAMESPACE_END
