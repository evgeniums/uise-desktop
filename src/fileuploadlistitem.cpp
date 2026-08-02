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

#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QBoxLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/filesizeformat.hpp>
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

// Maps a lowercased file extension to a curated tabler "file-type-*" icon name (see
// thirdparty/tabler-icons/outline/file-type-*.svg). These are square (24x24 viewBox), unlike
// the file-icon-vectors "classic" set previously used here, which are portrait document
// silhouettes -- RoundedImage::paintEvent() fills a QBrush with the pixmap and draws it into
// a rect sized to the target box, so a non-square source renders visibly stretched/distorted
// once forced into a square (Row view) or wide (Image view) slot. Extensions without a close
// match fall back to the generic file icon rather than guessing at a wrong file type.
QString fileTypeIconName(const QString& suffix)
{
    static const std::map<QString,QString> mapping{
        {QStringLiteral("pdf"),QStringLiteral("pdf")},
        {QStringLiteral("doc"),QStringLiteral("doc")},
        {QStringLiteral("docx"),QStringLiteral("docx")},
        {QStringLiteral("xls"),QStringLiteral("xls")},
        {QStringLiteral("xlsx"),QStringLiteral("xls")},
        {QStringLiteral("ppt"),QStringLiteral("ppt")},
        {QStringLiteral("pptx"),QStringLiteral("ppt")},
        {QStringLiteral("txt"),QStringLiteral("txt")},
        {QStringLiteral("csv"),QStringLiteral("csv")},
        {QStringLiteral("zip"),QStringLiteral("zip")},
        {QStringLiteral("rar"),QStringLiteral("zip")},
        {QStringLiteral("7z"),QStringLiteral("zip")},
        {QStringLiteral("html"),QStringLiteral("html")},
        {QStringLiteral("htm"),QStringLiteral("html")},
        {QStringLiteral("css"),QStringLiteral("css")},
        {QStringLiteral("js"),QStringLiteral("js")},
        {QStringLiteral("jsx"),QStringLiteral("jsx")},
        {QStringLiteral("ts"),QStringLiteral("ts")},
        {QStringLiteral("tsx"),QStringLiteral("tsx")},
        {QStringLiteral("xml"),QStringLiteral("xml")},
        {QStringLiteral("svg"),QStringLiteral("svg")},
        {QStringLiteral("php"),QStringLiteral("php")},
        {QStringLiteral("sql"),QStringLiteral("sql")},
        {QStringLiteral("rs"),QStringLiteral("rs")},
        {QStringLiteral("vue"),QStringLiteral("vue")}
    };
    auto it=mapping.find(suffix);
    return (it!=mapping.end()) ? it->second : QString();
}

std::shared_ptr<SvgIcon> fileTypeIcon(const QString& suffix, QWidget* context)
{
    auto name=fileTypeIconName(suffix);
    if (name.isEmpty())
    {
        return menuIcon(QStringLiteral("file"),context);
    }

    auto path=QString(":/icons/tabler-icons/outline/file-type-%1.svg").arg(name);
    if (!QFile::exists(path))
    {
        return menuIcon(QStringLiteral("file"),context);
    }

    // these are plain tabler outline icons (stroke="currentColor"), loaded by resource path
    // rather than through a named context alias, so the usual JSON-driven per-mode color
    // resolution does not apply -- substitute currentColor by hand instead, for the theme
    // active right now. Unlike alias-resolved icons this bakes the color in at construction
    // time: it will not repaint itself on a later theme toggle until the item is refreshed.
    // isDarkTheme(), not checkDarkTheme(): the latter always re-detects the OS/application
    // palette live and ignores an explicitly-set uise style mode, so it would keep reporting
    // the OS theme even after the app is switched to the other one via setStyleSheetMode()
    auto color=Style::instance().isDarkTheme() ? QStringLiteral("#CCCCCC") : QStringLiteral("#444444");
    std::map<QString,QString> substitution{{QStringLiteral("currentColor"),color}};
    SvgIcon::ColorMap colorMap(substitution);
    std::map<IconVariant,SvgIcon::ColorMap> colorMaps{{IconMode::Normal,colorMap}};

    auto icon=std::make_shared<SvgIcon>();
    icon->addFile(path,colorMaps);
    return icon;
}

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

    std::shared_ptr<SvgIcon> fallbackIcon;
    if (!isImg)
    {
        fallbackIcon=fileTypeIcon(it.suffix().toLower(),this);
    }

    auto applyTo=[&px,&fallbackIcon](RoundedImage* w, const QSize& size)
    {
        w->setImageSize(size);
        if (!px.isNull())
        {
            w->setSvgIcon(nullptr);
            w->setPixmap(px.scaled(size,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));
        }
        else
        {
            w->setPixmap(QPixmap());
            w->setSvgIcon(fallbackIcon);
        }
    };

    applyTo(pimpl->imagePreview,QSize(220,160));
    applyTo(pimpl->rowPreview,QSize(40,40));
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
    auto x=pimpl->imageFrame->width()-sz.width()-margin;
    pimpl->buttonsBlock->setGeometry(x,margin,sz.width(),sz.height());
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
