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

/** @file uise/desktop/src/chatmessagefileitem.cpp
*
*  Defines ChatMessageFileItem.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/filesizeformat.hpp>
#include <uise/desktop/utils/filetypeicon.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/loadcontrol.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const QSize IconSlotSize{56,56};

std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageFiles::%1").arg(alias),context);
}

QPixmap scaledAndCropped(const QPixmap& src, const QSize& targetSize)
{
    auto scaled=src.scaled(targetSize,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    QRect cropRect(
        (scaled.width()-targetSize.width())/2,
        (scaled.height()-targetSize.height())/2,
        targetSize.width(),
        targetSize.height()
    );
    return scaled.copy(cropRect);
}

}

//--------------------------------------------------------------------------

class ChatMessageFileItem_p
{
    public:

        ChatFileItem item;
        bool incoming=false;

        QFrame* iconSlot=nullptr;
        RoundedImage* fileIcon=nullptr;
        AvatarWidget* imagePreview=nullptr;
        LoadControl* loadControl=nullptr;

        ElidedLabel* nameLabel=nullptr;
        QLabel* infoLabel=nullptr;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;
};

//--------------------------------------------------------------------------

ChatMessageFileItem::ChatMessageFileItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ChatMessageFileItem_p>())
{
    auto layout=Layout::horizontal(this);

    pimpl->iconSlot=new QFrame(this);
    pimpl->iconSlot->setObjectName("iconSlot");
    pimpl->iconSlot->setFixedSize(IconSlotSize);
    layout->addWidget(pimpl->iconSlot);

    pimpl->fileIcon=new RoundedImage(pimpl->iconSlot);
    pimpl->fileIcon->setObjectName("fileIcon");
    pimpl->fileIcon->setAutoSize(false);
    pimpl->fileIcon->setCornersRadius(8,8);
    pimpl->fileIcon->setImageSize(IconSlotSize);
    pimpl->fileIcon->setGeometry(QRect(QPoint(0,0),IconSlotSize));
    pimpl->fileIcon->setCursor(Qt::PointingHandCursor);
    pimpl->fileIcon->installEventFilter(this);

    pimpl->imagePreview=new AvatarWidget(pimpl->iconSlot);
    pimpl->imagePreview->setObjectName("imagePreview");
    pimpl->imagePreview->setAutoSize(false);
    pimpl->imagePreview->setImageSize(IconSlotSize);
    pimpl->imagePreview->setGeometry(QRect(QPoint(0,0),IconSlotSize));
    pimpl->imagePreview->setClickable(true);
    connect(pimpl->imagePreview,&AvatarWidget::clicked,this,&ChatMessageFileItem::clicked);

    pimpl->loadControl=new LoadControl(pimpl->iconSlot);
    pimpl->loadControl->setObjectName("loadControl");
    pimpl->loadControl->setGeometry(QRect(QPoint(0,0),IconSlotSize));
    connect(pimpl->loadControl,&LoadControl::clicked,this,&ChatMessageFileItem::loadControlClicked);

    auto textColumn=new QFrame(this);
    textColumn->setObjectName("textColumn");
    auto textColumnLayout=Layout::vertical(textColumn);
    layout->addWidget(textColumn,1);

    pimpl->nameLabel=new ElidedLabel(textColumn);
    pimpl->nameLabel->setObjectName("nameLabel");
    pimpl->nameLabel->setElideMode(Qt::ElideMiddle);
    pimpl->nameLabel->setCursor(Qt::PointingHandCursor);
    pimpl->nameLabel->installEventFilter(this);
    textColumnLayout->addWidget(pimpl->nameLabel);

    pimpl->infoLabel=new QLabel(textColumn);
    pimpl->infoLabel->setObjectName("infoLabel");
    textColumnLayout->addWidget(pimpl->infoLabel);

    pimpl->menuButton=new IconTextButton(
        menuIcon(QStringLiteral("menu"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    layout->addWidget(pimpl->menuButton);

    // DropdownMenu is constructed parentless, like FileUploadListItem's own per-item menu:
    // DropdownFrame reparents itself lazily to the trigger's actual window() on first opening,
    // so constructing it with a parent this early would just capture the wrong window
    pimpl->menu=new DropdownMenu();
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatMessageFileItem::onMenuItemTriggered);
}

//--------------------------------------------------------------------------

ChatMessageFileItem::~ChatMessageFileItem()
{
    if (!pimpl->menu.isNull())
    {
        destroyWidget(pimpl->menu);
    }
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::setItem(const ChatFileItem& item, bool incoming)
{
    pimpl->item=item;
    pimpl->incoming=incoming;
    refresh();
}

//--------------------------------------------------------------------------

const ChatFileItem& ChatMessageFileItem::item() const
{
    return pimpl->item;
}

//--------------------------------------------------------------------------

bool ChatMessageFileItem::isIncoming() const noexcept
{
    return pimpl->incoming;
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::refresh()
{
    updateIconSlot();
    updateInfoLabels();

    pimpl->nameLabel->setText(pimpl->item.fileName());

    rebuildMenu();
}

//--------------------------------------------------------------------------

AbstractLoadControl* ChatMessageFileItem::loadControl() const
{
    return pimpl->loadControl;
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageFileItem::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::closeMenu()
{
    if (!pimpl->menu.isNull())
    {
        pimpl->menu->closeDropdown(true);
    }
}

//--------------------------------------------------------------------------

bool ChatMessageFileItem::eventFilter(QObject* obj, QEvent* event)
{
    if ((obj==pimpl->fileIcon || obj==pimpl->nameLabel) && event->type()==QEvent::MouseButtonPress)
    {
        auto me=static_cast<QMouseEvent*>(event);
        if (me->button()==Qt::LeftButton)
        {
            emit clicked();
        }
    }
    return QFrame::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::rebuildMenu()
{
    std::vector<MenuItem> items;

    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::Open),tr("Open"),menuIcon(QStringLiteral("open"),this)));
    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::SaveAs),tr("Save as"),menuIcon(QStringLiteral("saveAs"),this)));
    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::Forward),tr("Forward"),menuIcon(QStringLiteral("forward"),this)));
    if (pimpl->item.isShowInFolderAvailable())
    {
        items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::ShowInFolder),tr("Show in folder"),menuIcon(QStringLiteral("showInFolder"),this)));
    }

    pimpl->menu->setItems(std::move(items));
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::updateIconSlot()
{
    const auto& it=pimpl->item;
    const auto ready=(it.state()==ChatFileTransferState::Ready);

    pimpl->fileIcon->setVisible(false);
    pimpl->imagePreview->setVisible(false);
    pimpl->loadControl->setVisible(false);

    if (!ready)
    {
        pimpl->loadControl->setVisible(true);
        pimpl->loadControl->setState(chatFileLoadControlState(it.state(),pimpl->incoming));
        if (it.state()==ChatFileTransferState::Running)
        {
            pimpl->loadControl->setProgress(it.transferred(),it.size());
        }
        return;
    }

    if (it.isImage())
    {
        pimpl->imagePreview->setVisible(true);
        auto preview=it.preview();
        if (!preview.isNull())
        {
            pimpl->imagePreview->setPixmap(scaledAndCropped(QPixmap::fromImage(preview),IconSlotSize));
        }
        else
        {
            pimpl->imagePreview->setPixmap(QPixmap());
        }
        return;
    }

    pimpl->fileIcon->setVisible(true);
    pimpl->fileIcon->setSvgIcon(fileTypeIcon(it.suffix().toLower(),this,QStringLiteral("ChatMessageFiles::file")));
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::updateInfoLabels()
{
    const auto& it=pimpl->item;
    auto sizeText=formatFileSize(it.size());

    if (it.state()==ChatFileTransferState::Running)
    {
        pimpl->infoLabel->setText(QStringLiteral("%1 / %2").arg(formatFileSize(it.transferred()),sizeText));
    }
    else
    {
        pimpl->infoLabel->setText(sizeText);
    }
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::onMenuItemTriggered(int id)
{
    emit menuTriggered(id);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
