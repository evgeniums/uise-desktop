/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/fileuploadlistitem.cpp
*
*  Defines FileUploadListItem.
*
*/

/****************************************************************************/

#include <QFileInfo>
#include <QLabel>
#include <QBoxLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QGuiApplication>
#include <QScreen>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/filesizeformat.hpp>
#include <uise/desktop/utils/filetypeicon.hpp>
#include <uise/desktop/utils/pixmapscale.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/editablelabel.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/fileuploadlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

enum class MenuAction : int
{
    EditImage=1,
    RenameFile=2,
    Remove=3
};

std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("FileUpload::%1").arg(alias),context);
}

// Row-view chip: stays a fixed square (list rows must stay aligned), centre-cropped rather than
// showing the true aspect ratio.
const QSize RowPreviewSize{40,40};

// Image-view bounding box the true-aspect preview is fitted into. Taller than it is wide
// relative to the old fixed 220x160 box so a portrait source isn't squeezed down to a sliver
// before even reaching updateListAreaHeight()'s own maxListAreaHeight clamp (fileupload.qss).
const QSize ImagePreviewBox{220,260};

}

//--------------------------------------------------------------------------

class FileUploadListItem_p
{
    public:

        FileUploadItem item;
        FileUploadListItem::View view=FileUploadListItem::View::Image;

        QFrame* rowFrame=nullptr;
        QBoxLayout* rowLayout=nullptr;
        RoundedImage* rowPreview=nullptr;
        EditableLabelText* nameLabel=nullptr;
        QLabel* rowInfoLabel=nullptr;

        QFrame* imageFrame=nullptr;
        QBoxLayout* imageLayout=nullptr;
        RoundedImage* imagePreview=nullptr;
        QLabel* imageInfoLabel=nullptr;

        QFrame* buttonsBlock=nullptr;
        IconTextButton* menuButton=nullptr;
        IconTextButton* removeButton=nullptr;
        QPointer<DropdownMenu> menu;
};

//--------------------------------------------------------------------------

FileUploadListItem::FileUploadListItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FileUploadListItem_p>())
{
    setAttribute(Qt::WA_Hover,true);

    auto* topLayout=Layout::vertical(this);

    // --- Row view: thumbnail/icon + name + info, in a horizontal row ---

    pimpl->rowFrame=new QFrame(this);
    pimpl->rowFrame->setObjectName("rowPage");
    pimpl->rowLayout=Layout::horizontal(pimpl->rowFrame);
    topLayout->addWidget(pimpl->rowFrame);

    pimpl->rowPreview=new RoundedImage(pimpl->rowFrame);
    pimpl->rowPreview->setObjectName("preview");
    pimpl->rowPreview->setAutoSize(false);
    // a modest, fixed corner radius: RoundedImage defaults to width/2 (a circle/ellipse),
    // which is wrong for a rectangular file/image thumbnail
    pimpl->rowPreview->setCornersRadius(6,6);
    pimpl->rowLayout->addWidget(pimpl->rowPreview);

    auto* textColumn=new QFrame(pimpl->rowFrame);
    textColumn->setObjectName("textColumn");
    auto* textColumnLayout=Layout::vertical(textColumn);
    pimpl->rowLayout->addWidget(textColumn,1);

    pimpl->nameLabel=new EditableLabelText(textColumn);
    pimpl->nameLabel->setObjectName("nameLabel");
    pimpl->nameLabel->setEditable(true);
    textColumnLayout->addWidget(pimpl->nameLabel);
    connect(
        pimpl->nameLabel,
        &EditableLabelText::valueChanged,
        this,
        [this](const QString& text)
        {
            pimpl->item.setFileName(text);
            updateNameLabel();
            emit renameRequested(text);
        }
    );
    pimpl->nameLabel->setEditButtonAlwaysHidden(true);

    pimpl->rowInfoLabel=new QLabel(textColumn);
    pimpl->rowInfoLabel->setObjectName("infoLabel");
    textColumnLayout->addWidget(pimpl->rowInfoLabel);

    // --- Image view: a big preview above its size/content-size info ---

    pimpl->imageFrame=new QFrame(this);
    pimpl->imageFrame->setObjectName("imagePage");
    pimpl->imageLayout=Layout::vertical(pimpl->imageFrame);
    topLayout->addWidget(pimpl->imageFrame);

    pimpl->imagePreview=new RoundedImage(pimpl->imageFrame);
    pimpl->imagePreview->setObjectName("preview");
    pimpl->imagePreview->setAutoSize(false);
    pimpl->imagePreview->setCornersRadius(8,8);
    pimpl->imagePreview->setCursor(Qt::PointingHandCursor);
    pimpl->imagePreview->installEventFilter(this);
    // Left-aligned: updatePreview() sizes the preview to its true (variable) aspect ratio rather
    // than a fixed box, and a QBoxLayout already left-aligns a narrower-than-box (portrait)
    // preview by default with no alignment flag needed.
    pimpl->imageLayout->addWidget(pimpl->imagePreview);

    pimpl->imageInfoLabel=new QLabel(pimpl->imageFrame);
    pimpl->imageInfoLabel->setObjectName("infoLabel");
    pimpl->imageLayout->addWidget(pimpl->imageInfoLabel);

    // --- buttons block: embedded in the row in View::Row, floating in View::Image ---

    pimpl->buttonsBlock=new QFrame(this);
    pimpl->buttonsBlock->setObjectName("itemButtons");
    auto* buttonsLayout=Layout::horizontal(pimpl->buttonsBlock);

    pimpl->menuButton=new IconTextButton(
        menuIcon(QStringLiteral("menu"),pimpl->buttonsBlock),
        pimpl->buttonsBlock,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    buttonsLayout->addWidget(pimpl->menuButton);

    pimpl->removeButton=new IconTextButton(
        menuIcon(QStringLiteral("remove"),pimpl->buttonsBlock),
        pimpl->buttonsBlock,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->removeButton->setObjectName("removeButton");
    pimpl->removeButton->setText(QString());
    buttonsLayout->addWidget(pimpl->removeButton);
    connect(pimpl->removeButton,&IconTextButton::clicked,this,&FileUploadListItem::removeRequested);

    // DropdownMenu is constructed parentless, like FastSwitchButtonDropdown: DropdownFrame
    // reparents itself lazily to the trigger's actual window() on first opening, so
    // constructing it with a parent this early (before `this` is embedded anywhere real)
    // would just capture the wrong window
    pimpl->menu=new DropdownMenu();
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&FileUploadListItem::onMenuItemTriggered);

    setView(View::Row);
}

//--------------------------------------------------------------------------

FileUploadListItem::~FileUploadListItem()
{
    if (!pimpl->menu.isNull())
    {
        destroyWidget(pimpl->menu);
    }
}

//--------------------------------------------------------------------------

void FileUploadListItem::setItem(const FileUploadItem& item)
{
    pimpl->item=item;
    refresh();
}

//--------------------------------------------------------------------------

const FileUploadItem& FileUploadListItem::item() const
{
    return pimpl->item;
}

//--------------------------------------------------------------------------

void FileUploadListItem::refresh()
{
    updatePreview();
    updateInfoLabels();

    pimpl->nameLabel->setValue(pimpl->item.fileName());
    updateNameLabel();

    rebuildMenu();
}

//--------------------------------------------------------------------------

void FileUploadListItem::setView(View view)
{
    if (pimpl->view==view)
    {
        return;
    }
    pimpl->view=view;

    if (view==View::Row)
    {
        pimpl->imageFrame->setVisible(false);
        pimpl->rowFrame->setVisible(true);

        pimpl->buttonsBlock->setParent(pimpl->rowFrame);
        pimpl->rowLayout->addWidget(pimpl->buttonsBlock);
        pimpl->buttonsBlock->setVisible(true);
    }
    else
    {
        pimpl->rowFrame->setVisible(false);
        pimpl->imageFrame->setVisible(true);

        pimpl->rowLayout->removeWidget(pimpl->buttonsBlock);
        pimpl->buttonsBlock->setParent(pimpl->imageFrame);
        pimpl->buttonsBlock->setVisible(true);
        pimpl->buttonsBlock->raise();
        repositionButtonsBlock();
    }

    setProperty("view",view==View::Row ? QStringLiteral("row") : QStringLiteral("image"));
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

FileUploadListItem::View FileUploadListItem::view() const noexcept
{
    return pimpl->view;
}

//--------------------------------------------------------------------------

RoundedImage* FileUploadListItem::preview() const
{
    return (pimpl->view==View::Image) ? pimpl->imagePreview : pimpl->rowPreview;
}

//--------------------------------------------------------------------------

IconTextButton* FileUploadListItem::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void FileUploadListItem::beginRename()
{
    if (pimpl->view!=View::Row)
    {
        // the name field only lives in View::Row's layout; renaming from View::Image
        // switches to Row first so the edit is actually visible
        setView(View::Row);
    }
    pimpl->nameLabel->edit();
}

//--------------------------------------------------------------------------

void FileUploadListItem::closeMenu()
{
    if (!pimpl->menu.isNull())
    {
        pimpl->menu->closeDropdown(true);
    }
}

//--------------------------------------------------------------------------

void FileUploadListItem::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    if (pimpl->view==View::Image)
    {
        repositionButtonsBlock();
    }
    updateNameLabel();
}

//--------------------------------------------------------------------------

bool FileUploadListItem::eventFilter(QObject* obj, QEvent* event)
{
    if (obj==pimpl->imagePreview && event->type()==QEvent::MouseButtonPress)
    {
        auto* me=static_cast<QMouseEvent*>(event);
        if (me->button()==Qt::LeftButton)
        {
            emit previewClicked();
        }
    }
    return QFrame::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

void FileUploadListItem::rebuildMenu()
{
    std::vector<MenuItem> items;
    const auto& it=pimpl->item;

    if (it.isImage())
    {
        items.push_back(MenuItem(static_cast<int>(MenuAction::EditImage),tr("Edit image"),menuIcon(QStringLiteral("editImage"),this)));
    }
    if (it.type()==FileUploadItem::Type::File)
    {
        items.push_back(MenuItem(static_cast<int>(MenuAction::RenameFile),tr("Rename file"),menuIcon(QStringLiteral("rename"),this)));
    }
    // reuse the same "x" icon as the inline remove button, not the trash icon -- trash reads
    // as filesystem deletion here, which Remove explicitly is not (see the class doc comment)
    items.push_back(MenuItem(static_cast<int>(MenuAction::Remove),tr("Remove"),menuIcon(QStringLiteral("remove"),this)));

    pimpl->menu->setItems(std::move(items));
}

//--------------------------------------------------------------------------

void FileUploadListItem::updatePreview()
{
    const auto& it=pimpl->item;
    const auto isImg=it.isImage();

    // decode/scale once and apply to both preview widgets regardless of which is currently
    // visible -- a mode toggle then never has to redecode, and refresh() stays simple
    QPixmap px;
    if (isImg)
    {
        auto image=it.image();
        if (!image.isNull())
        {
            px=QPixmap::fromImage(image);
        }
    }

    if (px.isNull())
    {
        // non-image (or a decode failure): both slots fall back to the same fixed-size file-type
        // icon they always used -- no aspect ratio to preserve for a generic file glyph.
        auto fallbackIcon=fileTypeIcon(it.suffix().toLower(),this,QStringLiteral("FileUpload::file"));
        pimpl->rowPreview->setImageSize(RowPreviewSize);
        pimpl->rowPreview->setPixmap(QPixmap());
        pimpl->rowPreview->setSvgIcon(fallbackIcon);
        pimpl->imagePreview->setImageSize(ImagePreviewBox);
        pimpl->imagePreview->setPixmap(QPixmap());
        pimpl->imagePreview->setSvgIcon(fallbackIcon);
    }
    else
    {
        // RoundedImage::paintEvent() paints via a brush-textured drawRoundedRect() -- a QBrush
        // texture fill DOES respect QPixmap::devicePixelRatio() (unlike an untagged pixmap,
        // which Qt treats as 1 raw pixel = 1 LOGICAL unit for tiling purposes, not 1 raw pixel =
        // 1 PHYSICAL device pixel). Every pixmap below is produced at physical-pixel resolution
        // (scaled up from logical box sizes by dpr, matching RoundedImage::setImageSize()'s own
        // m_size=size*pixelRatio) for Retina sharpness, so it MUST be tagged with that same dpr
        // before setPixmap() -- an untagged one is misread as dpr times too large, so only its
        // top-left 1/dpr-by-1/dpr corner ends up inside the (correctly logical-sized) rect,
        // upscaled-then-cropped rather than shown whole.
        const qreal dpr=qApp->primaryScreen()->devicePixelRatio();
        auto toPhysical=[dpr](const QSize& logical)
        {
            return QSize(qRound(logical.width()*dpr),qRound(logical.height()*dpr));
        };

        pimpl->rowPreview->setSvgIcon(nullptr);
        pimpl->rowPreview->setImageSize(RowPreviewSize);
        auto rowPixmap=scaledAndCropped(px,toPhysical(RowPreviewSize));
        rowPixmap.setDevicePixelRatio(dpr);
        pimpl->rowPreview->setPixmap(rowPixmap);

        // True aspect ratio, fitted inside ImagePreviewBox: the pixmap's OWN resulting size
        // drives the widget's fixed size (via setImageSize()), so nothing is cropped and nothing
        // is letterboxed -- a portrait source ends up narrower than the box, a landscape one
        // shorter.
        auto scaledPhysical=scaledToFit(px,toPhysical(ImagePreviewBox));
        QSize resultLogical(
            qRound(scaledPhysical.width()/dpr),
            qRound(scaledPhysical.height()/dpr)
        );
        scaledPhysical.setDevicePixelRatio(dpr);
        pimpl->imagePreview->setSvgIcon(nullptr);
        pimpl->imagePreview->setImageSize(resultLogical);
        pimpl->imagePreview->setPixmap(scaledPhysical);
    }

    if (pimpl->view==View::Image)
    {
        // The preview's fixed size may just have changed (a different image, or the same image
        // re-edited to a different crop) -- re-run the layout immediately (setFixedSize() only
        // POSTS a deferred LayoutRequest, uise-desktop's own recurring gotcha with Qt layouts)
        // so repositionButtonsBlock() reads the preview's up-to-date geometry rather than last
        // frame's.
        pimpl->imageLayout->activate();
        repositionButtonsBlock();
    }
}

//--------------------------------------------------------------------------

void FileUploadListItem::updateInfoLabels()
{
    const auto& it=pimpl->item;
    auto sizeText=formatFileSize(it.size());

    if (it.isImage())
    {
        auto sz=it.pixelSize();
        if (sz.isValid() && sz.width()>0 && sz.height()>0)
        {
            pimpl->imageInfoLabel->setText(QString("%1x%2, %3").arg(sz.width()).arg(sz.height()).arg(sizeText));
        }
        else
        {
            pimpl->imageInfoLabel->setText(sizeText);
        }
    }
    else
    {
        pimpl->imageInfoLabel->setText(sizeText);
    }

    pimpl->rowInfoLabel->setText(sizeText);
}

//--------------------------------------------------------------------------

void FileUploadListItem::updateNameLabel()
{
    auto full=pimpl->nameLabel->value();
    auto* lbl=pimpl->nameLabel->label();
    if (lbl->width()>0)
    {
        lbl->setText(lbl->fontMetrics().elidedText(full,Qt::ElideMiddle,lbl->width()));
    }
    else
    {
        // not laid out yet -- the next resizeEvent() will re-elide against a real width
        lbl->setText(full);
    }
}

//--------------------------------------------------------------------------

void FileUploadListItem::repositionButtonsBlock()
{
    if (pimpl->buttonsBlock->parentWidget()!=pimpl->imageFrame)
    {
        return;
    }
    constexpr int margin=4;
    auto sz=pimpl->buttonsBlock->sizeHint();
    // Anchored to the PREVIEW's own geometry, not the frame's -- since updatePreview() now sizes
    // the preview to its true (possibly narrower-than-frame, left-aligned) aspect ratio, anchoring
    // to imageFrame->width() would float the buttons out past a portrait preview's right edge.
    auto previewGeom=pimpl->imagePreview->geometry();
    auto x=previewGeom.x()+previewGeom.width()-sz.width()-margin;
    auto y=previewGeom.y()+margin;
    pimpl->buttonsBlock->setGeometry(x,y,sz.width(),sz.height());
}

//--------------------------------------------------------------------------

void FileUploadListItem::onMenuItemTriggered(int id)
{
    switch (static_cast<MenuAction>(id))
    {
        case (MenuAction::EditImage):
            emit editRequested();
            break;

        case (MenuAction::RenameFile):
            beginRename();
            break;

        case (MenuAction::Remove):
            emit removeRequested();
            break;
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
