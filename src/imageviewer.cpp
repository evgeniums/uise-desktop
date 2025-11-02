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

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/imageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* ImageViewerWidget *****************************/

class ImageViewerWidget_p
{
    public:

        ImageViewer* ctrl;

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
};

//--------------------------------------------------------------------------

ImageViewerWidget::ImageViewerWidget(ImageViewer* ctrl, QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<ImageViewerWidget_p>())
{
    pimpl->ctrl=ctrl;

    pimpl->layout=Layout::vertical(this);

    pimpl->view = new QGraphicsView(this);
    pimpl->scene = new QGraphicsScene(this);
    pimpl->view->setScene(pimpl->scene);
    pimpl->layout->addWidget(pimpl->view,1);
    pimpl->scene->addItem(pimpl->imageItem);

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

    cl->addStretch(1);
}

//--------------------------------------------------------------------------

ImageViewerWidget::~ImageViewerWidget()
{}

/************************** ImageViewer *****************************/

//--------------------------------------------------------------------------

Widget* ImageViewer::doCreateActualWidget(QWidget* parent)
{
    m_widget=new ImageViewerWidget(this,parent);
    reset();
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
    m_widget->pimpl->view->setTransform(t);
}

//--------------------------------------------------------------------------

void ImageViewer::doSelectImage()
{
    doReset();
    auto px=currentImage();
    m_widget->pimpl->imageItem=m_widget->pimpl->scene->addPixmap(px);
    fitImage();
}

//--------------------------------------------------------------------------

void ImageViewer::fitImage()
{
    auto px=currentImage();
    if (!px.isNull())
    {
        m_widget->pimpl->scene->setSceneRect(m_widget->pimpl->imageItem->boundingRect());
        auto viewRect=m_widget->pimpl->view->rect();
        if (px.width()>viewRect.width() || px.height() > viewRect.height())
        {
            m_widget->pimpl->view->fitInView(m_widget->pimpl->imageItem, Qt::KeepAspectRatio);
        }
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
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
