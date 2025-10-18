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

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>

#include <QPushButton>
#include <QLineEdit>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/imagecropper.hpp>
#include <uise/desktop/simpleimageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* SimpleImageEditorWidget *****************************/

class SimpleImageEditorWidget_p
{
    public:

        SimpleImageEditor* ctrl;

        QBoxLayout* layout;

        QGraphicsView *view;
        QGraphicsScene *scene;
        QGraphicsPixmapItem *imageItem = nullptr;
        CropRectItem *cropperItem = nullptr;

        QFrame* fileBrowserFrame;
        QLineEdit* filenameEdit;
        QPushButton* browseFile;
};

//--------------------------------------------------------------------------

SimpleImageEditorWidget::SimpleImageEditorWidget(SimpleImageEditor* ctrl, QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<SimpleImageEditorWidget_p>())
{
    pimpl->ctrl=ctrl;

    pimpl->layout=Layout::vertical(this);

    pimpl->view = new QGraphicsView(this);
    pimpl->scene = new QGraphicsScene(this);
    pimpl->view->setScene(pimpl->scene);
    pimpl->layout->addWidget(pimpl->view,1);

    pimpl->fileBrowserFrame=new QFrame(this);
    pimpl->fileBrowserFrame->setObjectName("fileBrowserFrame");
    pimpl->layout->addWidget(pimpl->fileBrowserFrame);
    auto fl=Layout::horizontal(pimpl->fileBrowserFrame);
    pimpl->filenameEdit=new QLineEdit(pimpl->fileBrowserFrame);
    pimpl->filenameEdit->setObjectName("fileBrowserFrame");
    pimpl->filenameEdit->setPlaceholderText(tr("Select image file"));
    fl->addWidget(pimpl->filenameEdit,1);
    pimpl->browseFile=new QPushButton(pimpl->fileBrowserFrame);
    pimpl->browseFile->setObjectName("fileBrowserFrame");
    pimpl->browseFile->setText(tr("Browse..."));
    fl->addWidget(pimpl->browseFile);

    connect(
        pimpl->browseFile,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QPointer<QObject> guard{this};

            auto filter=tr("Images (*.png *.jpg *.svg *.jpeg *.xpm *.tiff *.bmp);;All files (*.*)");
            auto filename=QFileDialog::getOpenFileName(this,tr("Select image file"),pimpl->ctrl->folder(),filter);
            if (guard)
            {
                pimpl->filenameEdit->setText(filename);
                pimpl->ctrl->loadImageFromFile(filename);
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
        m_widget->pimpl->cropperItem->setEllipse(isEllipseCrop());
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

    auto px=originalImage();
    if (px.isNull())
    {
        return;
    }

    auto viewRect=m_widget->pimpl->view->rect();
    m_widget->pimpl->imageItem = m_widget->pimpl->scene->addPixmap(px);

    if (px.width()>viewRect.width() || px.height() > viewRect.height())
    {
        m_widget->pimpl->view->fitInView(m_widget->pimpl->imageItem, Qt::KeepAspectRatio);
    }

    auto imageRect=m_widget->pimpl->imageItem->boundingRect();
    m_widget->pimpl->scene->setSceneRect(imageRect);

    QRectF initialCrop=imageRect;
    m_widget->pimpl->cropperItem = new CropRectItem(imageRect,initialCrop, m_widget->pimpl->imageItem);
    m_widget->pimpl->scene->addItem(m_widget->pimpl->cropperItem);
    m_widget->pimpl->cropperItem->setZValue(1);
    m_widget->pimpl->cropperItem->setKeepAspectRatio(keepAspectRatio());
    m_widget->pimpl->cropperItem->setSquare(isSquareCrop());
    m_widget->pimpl->cropperItem->setEllipse(isEllipseCrop());
    m_widget->pimpl->cropperItem->setMinimumImageSize(minimumImageSize());
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
    return QPixmap{};
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
