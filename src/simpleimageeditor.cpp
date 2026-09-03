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
#include <QGraphicsItemGroup>
#include <QStyleOptionGraphicsItem>

#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include <QtColorWidgets/HueSlider>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/graphicsviewzoom.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/freehanddrawview.hpp>
#include <uise/desktop/simpleimageeditor.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

enum CropMenuId
{
    CropMenuOff=0,
    CropMenuSquare=1,
    CropMenuRectangular=2
};

}

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
        GraphicsViewZoom *zoom = nullptr;

        QFrame* controlsFrame;
        QFrame* mainButtonsFrame;
        PushButton* rotate;
        PushButton* rotateClockwise;
        PushButton* flipHorizontal;
        PushButton* flipVertical;
        PushButton* zoomIn;
        PushButton* zoomOut;
        PushButton* crop;
        DropdownMenu* cropMenu;
        std::shared_ptr<SvgIcon> cropOffIcon;
        std::shared_ptr<SvgIcon> cropSquareIcon;
        std::shared_ptr<SvgIcon> cropRectangularIcon;
        bool updatingCropMenu=false;
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

        QGraphicsItemGroup* itemGroup=nullptr;

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

    pimpl->zoom=new GraphicsViewZoom(pimpl->view,this);
    pimpl->zoom->setPanButtons(Qt::LeftButton|Qt::MiddleButton);
    pimpl->zoom->setPanFilter(
        [this](const QPoint& viewportPos)
        {
            // Freehand draw owns left-drag while enabled; a cropper (when present) owns drags
            // that land on its rect/handles (see CropRectItem::handleAt()) -- only the dimmed
            // margin outside it, or the whole image when there is no cropper, is a pan surface.
            if (pimpl->view->isFreeHandDrawEnabled())
            {
                return false;
            }
            if (pimpl->cropperItem==nullptr)
            {
                return true;
            }
            auto scenePos=pimpl->view->mapToScene(viewportPos);
            return pimpl->cropperItem->handleAt(scenePos)==CropRectItem::NoHandle;
        }
    );

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
    pimpl->cropOffIcon=Style::instance().svgIconLocator().icon("ImageEditor::crop",this);
    pimpl->cropSquareIcon=Style::instance().svgIconLocator().icon("ImageEditor::crop-square",this);
    pimpl->cropRectangularIcon=Style::instance().svgIconLocator().icon("ImageEditor::crop-rectangular",this);

    pimpl->crop=new PushButton(pimpl->mainButtonsFrame);
    pimpl->crop->setToolTip(tr("Cropping"));
    pimpl->crop->setSvgIcon(pimpl->cropSquareIcon);
    pimpl->crop->setVisible(false);
    ml->addWidget(pimpl->crop);

    pimpl->cropMenu=new DropdownMenu(this);
    auto cropMenuItemOff=MenuItem::checkable(CropMenuOff,tr("Cropping off"),false,pimpl->cropOffIcon);
    cropMenuItemOff.group=0;
    auto cropMenuItemSquare=MenuItem::checkable(CropMenuSquare,tr("Square cropping"),true,pimpl->cropSquareIcon);
    cropMenuItemSquare.group=0;
    auto cropMenuItemRectangular=MenuItem::checkable(CropMenuRectangular,tr("Rectangular cropping"),false,pimpl->cropRectangularIcon);
    cropMenuItemRectangular.group=0;
    pimpl->cropMenu->setItems({cropMenuItemOff,cropMenuItemSquare,cropMenuItemRectangular});
    pimpl->cropMenu->setCloseOnCheckableActivation(true);
    pimpl->cropMenu->attachTo(pimpl->crop);
    connect(
        pimpl->cropMenu,
        &DropdownMenu::itemToggled,
        this,
        [this](int id, bool checked)
        {
            if (!checked || pimpl->updatingCropMenu)
            {
                return;
            }
            switch (id)
            {
                case CropMenuOff:
                    pimpl->ctrl->setCropMode(AbstractImageEditor::CropMode::Off);
                    break;
                case CropMenuSquare:
                    pimpl->ctrl->setCropMode(AbstractImageEditor::CropMode::Square);
                    break;
                case CropMenuRectangular:
                    pimpl->ctrl->setCropMode(AbstractImageEditor::CropMode::Rectangular);
                    break;
            }
        }
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
            QPointer<SimpleImageEditorWidget> guard{this};
            if (guard && guard->pimpl->ctrl!=nullptr)
            {
                QFileDialog::Options options;
                if (!pimpl->ctrl->isNativeFileDialog())
                {
                    options=QFileDialog::DontUseNativeDialog;
                }

                auto filter=tr("Images (*.png *.jpg *.jpeg *.xpm *.tiff *.bmp);;All files (*.*)");
                auto filename=QFileDialog::getOpenFileName(this,tr("Select image file"),pimpl->ctrl->folder(),filter,nullptr,options);
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

    // Gesture/wheel zoom and drag-to-pan bypass zoomIn()/zoomOut() entirely (they act directly
    // on the view via GraphicsViewZoom's own event filter), so the crop rect needs the same
    // refresh those two get -- see refreshCropperForViewChange()'s own doc.
    connect(
        m_widget->pimpl->zoom,
        &GraphicsViewZoom::zoomChanged,
        this,
        &SimpleImageEditor::refreshCropperForViewChange
    );
    connect(
        m_widget->pimpl->zoom,
        &GraphicsViewZoom::panned,
        this,
        &SimpleImageEditor::refreshCropperForViewChange
    );

    return m_widget;
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateCropShape()
{
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        // Refreshed here too, not just in resetCropper() -- setSquare()/setEllipse() below
        // trigger adjustCropRect() on the EXISTING cropper item, which must know the CURRENT
        // zoom state rather than whatever was true when the item was last (re)constructed (the
        // zoom level can have changed since via zoomIn()/zoomOut(), which no longer rebuild the
        // cropper -- see refreshCropperForViewChange()).
        m_widget->pimpl->cropperItem->setLimitToVisibleArea(!m_widget->pimpl->zoom->isZoomed());
        m_widget->pimpl->cropperItem->setSquare(isSquareCrop());
        m_widget->pimpl->cropperItem->setEllipse(isEllipseCropPreview());
        m_widget->pimpl->cropperItem->update();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateCropEnabled()
{
    if (isCropEnabled())
    {
        resetCropper();
    }
    else
    {
        destroyCropper();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::updateCropButtonState()
{
    m_widget->pimpl->crop->setVisible(isCropButtonVisible());

    int checkedId=CropMenuSquare;
    std::shared_ptr<SvgIcon> icon=m_widget->pimpl->cropSquareIcon;
    switch (cropMode())
    {
        case CropMode::Off:
            icon=m_widget->pimpl->cropOffIcon;
            checkedId=CropMenuOff;
            break;
        case CropMode::Square:
            icon=m_widget->pimpl->cropSquareIcon;
            checkedId=CropMenuSquare;
            break;
        case CropMode::Rectangular:
            icon=m_widget->pimpl->cropRectangularIcon;
            checkedId=CropMenuRectangular;
            break;
    }
    m_widget->pimpl->crop->setSvgIcon(icon);

    m_widget->pimpl->updatingCropMenu=true;
    m_widget->pimpl->cropMenu->setItemChecked(CropMenuOff,checkedId==CropMenuOff);
    m_widget->pimpl->cropMenu->setItemChecked(CropMenuSquare,checkedId==CropMenuSquare);
    m_widget->pimpl->cropMenu->setItemChecked(CropMenuRectangular,checkedId==CropMenuRectangular);
    m_widget->pimpl->updatingCropMenu=false;
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
        // See updateCropShape()'s own comment -- same reasoning applies here.
        m_widget->pimpl->cropperItem->setLimitToVisibleArea(!m_widget->pimpl->zoom->isZoomed());
        m_widget->pimpl->cropperItem->setKeepAspectRatio(keepAspectRatio());
        m_widget->pimpl->cropperItem->update();
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::doLoadImage()
{
    destroyCropper();
    m_widget->pimpl->scene->clear();
    m_widget->pimpl->view->update();
    m_widget->pimpl->view->resetTransform();
    m_widget->pimpl->view->setSceneRect(QRectF{});
    m_widget->pimpl->cropperItem=nullptr;
    m_widget->pimpl->imageItem = nullptr;
    m_widget->pimpl->angle=0;
    m_widget->pimpl->controlsFrame->setVisible(false);
    m_widget->pimpl->itemGroup=m_widget->pimpl->scene->createItemGroup(QList<QGraphicsItem *>{});
    m_widget->pimpl->view->setItemGroup(m_widget->pimpl->itemGroup);
    m_widget->pimpl->zoom->setFitItem(nullptr);

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

    m_widget->pimpl->itemGroup->addToGroup(m_widget->pimpl->imageItem);
    m_widget->pimpl->zoom->setFitItem(m_widget->pimpl->itemGroup);

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
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return QPixmap{};
    }

    QRectF croppedRect;
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        auto items=m_widget->pimpl->scene->items();
        for (qsizetype i=0; i<items.count();i++)
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
    }
    else
    {
        croppedRect=m_widget->pimpl->itemGroup->sceneBoundingRect();
    }

    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->setVisible(false);
    }

    auto px=QPixmap{static_cast<int>(croppedRect.width()),static_cast<int>(croppedRect.height())};
    QPainter painter;
    painter.begin(&px);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_widget->pimpl->scene->render(&painter,px.rect(),croppedRect);
    painter.end();

    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->setVisible(true);
    }

#if 0
    auto viewRect=m_widget->pimpl->view->mapFromScene(croppedRect).boundingRect();
    auto transform = m_widget->pimpl->view->transform();
    auto scale_x = qSqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());
    qDebug() << "SimpleImageEditor::editedImage() croppedRect=" << croppedRect
                       << " viewRect=" << viewRect << " sceneRect=" << m_widget->pimpl->view->sceneRect()
                       << " px.size()" << px.size() << " isScaling()" << m_widget->pimpl->view->transform().isScaling()
                       << " isRotating="<<m_widget->pimpl->view->transform().isRotating()
                       << " scale_x="<<scale_x;
#endif

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

void SimpleImageEditor::destroyCropper()
{
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->scene->removeItem(m_widget->pimpl->cropperItem);
        delete m_widget->pimpl->cropperItem;
        m_widget->pimpl->cropperItem=nullptr;
    }
}

//--------------------------------------------------------------------------

void SimpleImageEditor::resetCropper()
{
    if (!isCropEnabled() || m_widget->pimpl->imageItem==nullptr || m_widget->pimpl->view->isFreeHandDrawEnabled())
    {
        return;
    }

    destroyCropper();

    m_widget->pimpl->cropperItem = new CropRectItem(m_widget->pimpl->view,m_widget->pimpl->imageItem);
    m_widget->pimpl->scene->addItem(m_widget->pimpl->cropperItem);
    // Before any setter/init() call below that triggers adjustCropRect() -- see
    // setLimitToVisibleArea()'s own doc: while zoomed in, the crop rect must be derived from the
    // full image bounds, not intersected with whatever sliver of the image the current pan/zoom
    // happens to have on screen.
    m_widget->pimpl->cropperItem->setLimitToVisibleArea(!m_widget->pimpl->zoom->isZoomed());
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

    auto r=m_widget->pimpl->itemGroup->boundingRect();
    m_widget->pimpl->angle-=90;
    m_widget->pimpl->itemGroup->setTransformOriginPoint(r.center());
    m_widget->pimpl->itemGroup->setRotation(m_widget->pimpl->angle);
    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::rotateClockwise()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }
    auto r=m_widget->pimpl->itemGroup->boundingRect();
    m_widget->pimpl->angle+=90;
    m_widget->pimpl->itemGroup->setTransformOriginPoint(r.center());
    m_widget->pimpl->itemGroup->setRotation(m_widget->pimpl->angle);
    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::flipHorizontal()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }
    QTransform transform = m_widget->pimpl->itemGroup->transform();
    QPointF center = m_widget->pimpl->itemGroup->mapToScene(m_widget->pimpl->itemGroup->boundingRect().center());
    transform.translate(center.x(), center.y());
    transform.scale(-1, 1);
    transform.translate(-center.x(), -center.y());
    m_widget->pimpl->itemGroup->setTransform(transform);
    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::flipVertical()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }
    QTransform transform = m_widget->pimpl->itemGroup->transform();
    QPointF center = m_widget->pimpl->itemGroup->mapToScene(m_widget->pimpl->itemGroup->boundingRect().center());
    transform.translate(center.x(), center.y());
    transform.scale(1, -1);
    transform.translate(-center.x(), -center.y());
    m_widget->pimpl->itemGroup->setTransform(transform);
    resetCropper();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::zoomIn()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    m_widget->pimpl->zoom->zoomIn();
    refreshCropperForViewChange();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::zoomOut()
{
    if (m_widget->pimpl->imageItem==nullptr)
    {
        return;
    }

    m_widget->pimpl->zoom->zoomOut();
    refreshCropperForViewChange();
}

//--------------------------------------------------------------------------

void SimpleImageEditor::refreshCropperForViewChange()
{
    // The crop rect lives in imageItem-group coordinates, so it stays glued to the image under
    // any view-level (purely visual) zoom/pan -- CropRectItem::paint()/getHandleType() already
    // re-derive handle/border sizes from the view's scale (see imagecropper.cpp), so a repaint is
    // all that's needed here. resetCropper() (destroy + recreate) is deliberately NOT called --
    // that used to run on every zoomIn()/zoomOut() and threw away the user's crop selection on
    // every step, which is the bug this replaces.
    if (m_widget->pimpl->cropperItem!=nullptr)
    {
        m_widget->pimpl->cropperItem->update();
    }
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

}
