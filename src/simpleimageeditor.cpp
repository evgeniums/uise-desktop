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

/** @file uise/desktop/simpleimageeditor.cpp
*
*  Defines SimpleImageEditor.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QFileDialog>
#include <QTimer>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QStyleOptionGraphicsItem>

#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include <QtColorWidgets/HueSlider>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/freehanddrawview.hpp>
#include <uise/desktop/simpleimageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* SimpleImageEditorWidget *****************************/

class SimpleImageEditorWidget_p
{
    public:

        SimpleImageEditor* ctrl;

        QBoxLayout* layout;

        FreeHandDrawView *view;
        QGraphicsScene *scene;
        QGraphicsPixmapItem *imageItem = nullptr;
        CropRectItem *cropperItem = nullptr;

        QFrame* controlsFrame;
        QFrame* mainButtonsFrame;
        PushButton* rotate;
        PushButton* rotateClockwise;
        PushButton* flipHorizontal;
        PushButton* flipVertical;
        PushButton* zoomIn;
        PushButton* zoomOut;
        PushButton* freeHandDraw;

        QFrame* freeHandDrawFrame;
        PushButton* freeHandDrawUndo;
        PushButton* freeHandDrawRedo;
        PushButton* freeHandDrawAccept;
        QSpinBox* freeHandDrawPenWidth;
        PushButton* freeHandDrawCancel;
        color_widgets::HueSlider* freeHandColor;

        QFrame* fileBrowserFrame;
        QLineEdit* filenameEdit;
        PushButton* browseFile;

        int angle=0;
};

//--------------------------------------------------------------------------

SimpleImageEditorWidget::SimpleImageEditorWidget(SimpleImageEditor* ctrl, QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<SimpleImageEditorWidget_p>())
{
    pimpl->ctrl=ctrl;

    pimpl->layout=Layout::vertical(this);

    pimpl->view = new FreeHandDrawView(this);
    pimpl->scene = new QGraphicsScene(this);
    pimpl->view->setScene(pimpl->scene);
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
        &AbstractImageEditor::rotate
    );
    pimpl->rotateClockwise=new PushButton(pimpl->mainButtonsFrame);
    pimpl->rotateClockwise->setToolTip(tr("Rotate clockwise"));
    pimpl->rotateClockwise->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::rotate-clockwise",this));
    ml->addWidget(pimpl->rotateClockwise);
    connect(
        pimpl->rotateClockwise,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageEditor::rotateClockwise
    );
    pimpl->flipHorizontal=new PushButton(pimpl->mainButtonsFrame);
    pimpl->flipHorizontal->setToolTip(tr("Flip horizontally"));
    pimpl->flipHorizontal->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::flip-horizontal",this));
    ml->addWidget(pimpl->flipHorizontal);
    connect(
        pimpl->flipHorizontal,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageEditor::flipHorizontal
    );
    pimpl->flipVertical=new PushButton(pimpl->mainButtonsFrame);
    pimpl->flipVertical->setToolTip(tr("Flip vertically"));
    pimpl->flipVertical->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::flip-vertical",this));
    ml->addWidget(pimpl->flipVertical);
    connect(
        pimpl->flipVertical,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageEditor::flipVertical
    );
    pimpl->zoomIn=new PushButton(pimpl->mainButtonsFrame);
    pimpl->zoomIn->setToolTip(tr("Zoom in"));
    pimpl->zoomIn->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::zoom-in",this));
    ml->addWidget(pimpl->zoomIn);
    connect(
        pimpl->zoomIn,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageEditor::zoomIn
        );
    pimpl->zoomOut=new PushButton(pimpl->mainButtonsFrame);
    pimpl->zoomOut->setToolTip(tr("Zoom out"));
    pimpl->zoomOut->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::zoom-out",this));
    ml->addWidget(pimpl->zoomOut);
    connect(
        pimpl->zoomOut,
        &PushButton::clicked,
        pimpl->ctrl,
        &AbstractImageEditor::zoomOut
    );
    pimpl->freeHandDraw=new PushButton(pimpl->mainButtonsFrame);
    pimpl->freeHandDraw->setToolTip(tr("Freehand draw"));
    pimpl->freeHandDraw->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::brush",this));
    ml->addWidget(pimpl->freeHandDraw);
    connect(
        pimpl->freeHandDraw,
        &PushButton::toggled,
        pimpl->ctrl,
        &AbstractImageEditor::setFreeHandDrawMode
    );
    pimpl->freeHandDraw->setCheckable(true);

    pimpl->freeHandDrawFrame=new QFrame(pimpl->controlsFrame);
    pimpl->freeHandDrawFrame->setObjectName("freeHandDrawFrame");
    pimpl->freeHandDrawFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    auto fhwl=Layout::horizontal(pimpl->freeHandDrawFrame);
    cl->addWidget(pimpl->freeHandDrawFrame);

    pimpl->freeHandDrawAccept=new PushButton(pimpl->controlsFrame);
    pimpl->freeHandDrawAccept->setToolTip(tr("Accept freehand drawing"));
    pimpl->freeHandDrawAccept->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::accept",this));
    fhwl->addWidget(pimpl->freeHandDrawAccept);
    connect(
        pimpl->freeHandDrawAccept,
        &PushButton::clicked,
        pimpl->ctrl,
        &SimpleImageEditor::acceptFreeHandDraw
    );
    pimpl->freeHandDrawCancel=new PushButton(pimpl->controlsFrame);
    pimpl->freeHandDrawCancel->setToolTip(tr("Cancel freehand drawing"));
    pimpl->freeHandDrawCancel->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::cancel",this));
    fhwl->addWidget(pimpl->freeHandDrawCancel);
    connect(
        pimpl->freeHandDrawCancel,
        &PushButton::clicked,
        pimpl->ctrl,
        &SimpleImageEditor::cancelFreeHandDraw
    );
    pimpl->freeHandDrawUndo=new PushButton(pimpl->controlsFrame);
    pimpl->freeHandDrawUndo->setToolTip(tr("Undo"));
    pimpl->freeHandDrawUndo->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::undo",this));
    fhwl->addWidget(pimpl->freeHandDrawUndo);
    connect(
        pimpl->freeHandDrawUndo,
        &PushButton::clicked,
        pimpl->view,
        &FreeHandDrawView::undoHandDraw
    );
    pimpl->freeHandDrawRedo=new PushButton(pimpl->controlsFrame);
    pimpl->freeHandDrawRedo->setToolTip(tr("Redo"));
    pimpl->freeHandDrawRedo->setSvgIcon(Style::instance().svgIconLocator().icon("ImageEditor::redo",this));
    fhwl->addWidget(pimpl->freeHandDrawRedo);
    connect(
        pimpl->freeHandDrawRedo,
        &PushButton::clicked,
        pimpl->view,
        &FreeHandDrawView::redoHandDraw
    );
    pimpl->freeHandColor=new color_widgets::HueSlider(pimpl->controlsFrame);
    pimpl->freeHandColor->setObjectName("freeHandColor");
    pimpl->freeHandColor->setToolTip(tr("Pen color"));
    fhwl->addWidget(pimpl->freeHandColor);
    connect(
        pimpl->freeHandColor,
        &color_widgets::HueSlider::colorChanged,
        pimpl->view,
        &FreeHandDrawView::setPenColor
    );
    pimpl->freeHandDrawPenWidth=new QSpinBox(pimpl->controlsFrame);
    pimpl->freeHandDrawPenWidth->setObjectName("freeHanPenWidth");
    pimpl->freeHandDrawPenWidth->setToolTip(tr("Pen width"));
    pimpl->freeHandDrawPenWidth->setMinimum(2);
    pimpl->freeHandDrawPenWidth->setMaximum(100);
    pimpl->freeHandDrawPenWidth->setValue(pimpl->view->penWidth());
    fhwl->addWidget(pimpl->freeHandDrawPenWidth);
    connect(
        pimpl->freeHandDrawPenWidth,
        &QSpinBox::valueChanged,
        pimpl->view,
        &FreeHandDrawView::setPenWidth
    );

    pimpl->freeHandDrawFrame->setVisible(false);

    cl->addStretch(1);
    pimpl->controlsFrame->setVisible(false);

    pimpl->fileBrowserFrame=new QFrame(this);
    pimpl->fileBrowserFrame->setObjectName("fileBrowserFrame");
    pimpl->layout->addWidget(pimpl->fileBrowserFrame);
    auto fl=Layout::horizontal(pimpl->fileBrowserFrame);
    pimpl->filenameEdit=new QLineEdit(pimpl->fileBrowserFrame);
    pimpl->filenameEdit->setObjectName("fileBrowserFrame");
    pimpl->filenameEdit->setPlaceholderText(tr("Select image file"));
    fl->addWidget(pimpl->filenameEdit,1);
    pimpl->browseFile=new PushButton(pimpl->fileBrowserFrame);
    pimpl->browseFile->setObjectName("fileBrowserFrame");
    pimpl->browseFile->setText(tr("Browse..."));
    fl->addWidget(pimpl->browseFile);

    connect(
        pimpl->browseFile,
        &PushButton::clicked,
        this,
        [this]()
        {
            QPointer<QObject> guard{this};

            auto filter=tr("Images (*.png *.jpg *.jpeg *.xpm *.tiff *.bmp);;All files (*.*)");
            auto filename=QFileDialog::getOpenFileName(this,tr("Select image file"),pimpl->ctrl->folder(),filter);
            if (guard)
            {
                pimpl->filenameEdit->setText(filename);
                pimpl->ctrl->loadImageFromFile(filename);
                QFileInfo finf{filename};
                if (finf.exists())
                {
                    pimpl->ctrl->setFolder(finf.absolutePath());
                }
            }
        }
    );
}

//--------------------------------------------------------------------------

SimpleImageEditorWidget::~SimpleImageEditorWidget()
{}

/************************** SimpleImageEditor *****************************/

//--------------------------------------------------------------------------

Widget* SimpleImageEditor::doCreateActualWidget(QWidget* parent)
{
    m_widget=new SimpleImageEditorWidget(this,parent);
    doUpdateFilenameState();
    return m_widget;
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateCropShape()
{
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->setSquare(isSquareCrop());
        m_widget->pimpl->cropperItem->setEllipse(isEllipseCropPreview());
        m_widget->pimpl->cropperItem->update();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateImageSizeLimits()
{
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->setMinimumImageSize(minimumImageSize());
        m_widget->pimpl->cropperItem->update();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateAspectRatio()
{
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->setKeepAspectRatio(keepAspectRatio());
        m_widget->pimpl->cropperItem->update();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::doLoadImage()
{
    m_widget->pimpl->scene->clear();
    m_widget->pimpl->view->update();
    m_widget->pimpl->view->resetTransform();
    m_widget->pimpl->view->setSceneRect(QRectF{});
    m_widget->pimpl->cropperItem=nullptr;
    m_widget->pimpl->imageItem = nullptr;
    m_widget->pimpl->angle=0;
    m_widget->pimpl->controlsFrame->setVisible(false);

    auto px=originalImage();
    if (px.isNull())
    {
        return;
    }
    px.setDevicePixelRatio(1.0);
    m_widget->pimpl->controlsFrame->setVisible(true);

    m_widget->pimpl->imageItem = m_widget->pimpl->scene->addPixmap(px);
    m_widget->pimpl->scene->setSceneRect(m_widget->pimpl->imageItem->boundingRect());
    auto viewRect=m_widget->pimpl->view->rect();

    // qDebug() << " SimpleImageEditor::doLoadImage() size=" << px.size()
    //                    << " viewRect="<<viewRect
    //                    << " imageBoundingRect="<<m_widget->pimpl->imageItem->boundingRect()
    //                    << " sceneRect="<<m_widget->pimpl->scene->sceneRect()
    //                    << " pixmapRatio=" << px.devicePixelRatio();

    if (px.width()>viewRect.width() || px.height() > viewRect.height())
    {
        m_widget->pimpl->view->fitInView(m_widget->pimpl->imageItem, Qt::KeepAspectRatio);
    }

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateFilenameState()
{
    doUpdateFilenameState();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::doUpdateFilenameState()
{
    m_widget->pimpl->fileBrowserFrame->setEnabled(isFilenameEditable());
    m_widget->pimpl->fileBrowserFrame->setVisible(isFilenameVisible());
}

//--------------------------------------------------------------------------

QPixmap SimpleImageEditor::editedImage()
{
    if (m_widget->pimpl->cropperItem==nullptr)
    {
        return QPixmap{};
    }

    QRectF croppedRect;
    auto items=m_widget->pimpl->scene->items();
    for (size_t i=0; i<items.count();i++)
    {
        auto* item=items.at(i);
        if (item->type()==CropRectItem::Type)
        {
            auto cropper=qgraphicsitem_cast<CropRectItem*>(item);
            croppedRect=cropper->getCropAreaCoordinates();
            break;
        }
    }

    if (!croppedRect.isValid())
    {
        return QPixmap{};
    }

    auto viewRect=m_widget->pimpl->view->mapFromScene(croppedRect).boundingRect();
    m_widget->pimpl->cropperItem->setVisible(false);

    QPixmap px;
#if 0
    if (m_widget->pimpl->view->transform().isRotating())
#endif
    {
        px=QPixmap{static_cast<int>(viewRect.width()),static_cast<int>(viewRect.height())};
        QPainter painter;
        painter.begin(&px);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        m_widget->pimpl->view->render(&painter,px.rect(),viewRect);
        painter.end();
    }
#if 0
    else
    {
        px=QPixmap{static_cast<int>(croppedRect.width()),static_cast<int>(croppedRect.height())};
        QPainter painter;
        painter.begin(&px);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        m_widget->pimpl->scene->render(&painter,px.rect(),croppedRect);
        painter.end();
    }
#endif
    m_widget->pimpl->cropperItem->setVisible(true);

    auto transform = m_widget->pimpl->view->transform();
    auto scale_x = qSqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());

    qDebug() << "SimpleImageEditor::editedImage() croppedRect=" << croppedRect
                       << " viewRect=" << viewRect << " sceneRect=" << m_widget->pimpl->view->sceneRect()
                       << " px.size()" << px.size() << " isScaling()" << m_widget->pimpl->view->transform().isScaling()
                       << " isRotating="<<m_widget->pimpl->view->transform().isRotating()
                       << " scale_x="<<scale_x;

    if (maximumImageSize().isValid())
    {
        if (px.width()>maximumImageSize().width())
        {
            px=px.scaledToWidth(maximumImageSize().width(),Qt::SmoothTransformation);
        }
        if (px.height()>maximumImageSize().height())
        {
            px=px.scaledToHeight(maximumImageSize().height(),Qt::SmoothTransformation);
        }
    }

    return px;
}

//--------------------------------------------------------------------------

void SimpleImageEditor::resetCropper()
{
    if (!isCropEnabled() || m_widget->pimpl->imageItem==nullptr || m_widget->pimpl->view->isFreeHandDrawEnabled())
    {
        return;
    }

    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->scene->removeItem(m_widget->pimpl->cropperItem);
        delete m_widget->pimpl->cropperItem;
        m_widget->pimpl->cropperItem=nullptr;
    }

    m_widget->pimpl->cropperItem = new CropRectItem(m_widget->pimpl->view,m_widget->pimpl->imageItem);
    m_widget->pimpl->scene->addItem(m_widget->pimpl->cropperItem);
    m_widget->pimpl->cropperItem->setKeepAspectRatio(keepAspectRatio());
    m_widget->pimpl->cropperItem->setSquare(isSquareCrop());
    m_widget->pimpl->cropperItem->setEllipse(isEllipseCropPreview());
    m_widget->pimpl->cropperItem->setMinimumImageSize(minimumImageSize());
    m_widget->pimpl->cropperItem->init();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::rotate()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.rotate(-90);
    m_widget->pimpl->angle-=90;
    m_widget->pimpl->view->setTransform(t);

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::rotateClockwise()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.rotate(90);
    m_widget->pimpl->angle+=90;
    m_widget->pimpl->view->setTransform(t);

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::flipHorizontal()
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

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::flipVertical()
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

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::zoomIn()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.scale(1.25, 1.25);
    m_widget->pimpl->view->setTransform(t);

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::zoomOut()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    auto t=m_widget->pimpl->view->transform();
    t.scale(0.8, 0.8);
    m_widget->pimpl->view->setTransform(t);

    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::setFreeHandDrawMode(bool enable)
{
    m_widget->pimpl->view->setFreeHandDrawEnabled(enable);

    m_widget->pimpl->freeHandDraw->blockSignals(true);
    m_widget->pimpl->freeHandDraw->setChecked(enable);
    m_widget->pimpl->freeHandDraw->setEnabled(!enable);
    m_widget->pimpl->freeHandDraw->blockSignals(false);

    m_widget->pimpl->freeHandDrawFrame->setVisible(enable);

    if (enable)
    {
        if (m_widget->pimpl->cropperItem!=nullptr)
        {
            m_widget->pimpl->cropperItem->setVisible(false);
        }
    }
    else
    {
        resetCropper();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::acceptFreeHandDraw()
{
    m_widget->pimpl->view->acceptHandDraw();
    setFreeHandDrawMode(false);
}

//--------------------------------------------------------------------------

void SimpleImageEditor::cancelFreeHandDraw()
{
    m_widget->pimpl->view->cancelHandDraw();
    setFreeHandDrawMode(false);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
