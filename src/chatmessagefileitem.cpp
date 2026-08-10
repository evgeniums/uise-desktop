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
#include <uise/desktop/utils/pixmapscale.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/loadcontrol.hpp>
#include <uise/desktop/loadcontrolmenu.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const QSize IconSlotSize{56,56};

// vertical gap between the file-name and size lines -- addSpacing() rather than the layout's
// own spacing() (kept at 0 by Layout::vertical's reset) so it applies only between these two
// widgets, not also between them and the top/bottom stretch spacers bracketing the pair
constexpr int TextLineSpacing=4;

std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageFiles::%1").arg(alias),context);
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
        LoadControlMenu* loadControl=nullptr;

        ElidedLabel* nameLabel=nullptr;
        QLabel* infoLabel=nullptr;
        QVBoxLayout* textColumnLayout=nullptr;
        Qt::Alignment textVerticalAlignment=Qt::AlignVCenter;

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

    pimpl->loadControl=new LoadControlMenu(pimpl->iconSlot);
    pimpl->loadControl->setObjectName("loadControl");
    pimpl->loadControl->setGeometry(QRect(QPoint(0,0),IconSlotSize));
    connect(pimpl->loadControl,&LoadControlMenu::clicked,this,&ChatMessageFileItem::loadControlClicked);
    connect(pimpl->loadControl,&LoadControlMenu::pauseRequested,this,&ChatMessageFileItem::pauseRequested);
    connect(pimpl->loadControl,&LoadControlMenu::cancelRequested,this,&ChatMessageFileItem::cancelRequested);

    auto textColumn=new QFrame(this);
    textColumn->setObjectName("textColumn");
    pimpl->textColumnLayout=Layout::vertical(textColumn);
    layout->addWidget(textColumn,1);

    // top/bottom spacers bracketing the two labels -- textColumn's own height follows the row's
    // (56px, driven by iconSlot), well beyond what two lines of text need, and a plain QVBoxLayout
    // with neither stretch would otherwise hand that leftover space to the labels themselves
    // (both default to a Preferred vertical size policy), stretching their cells and leaving the
    // text looking vertically centered *within each of those inflated cells* -- i.e. the name
    // appears top-ish and the size line bottom-ish with a gap between, not the two lines sitting
    // together. Routing all leftover space into these spacers instead keeps the labels at their
    // natural sizeHint height; see setTextVerticalAlignment() for how the top spacer's stretch
    // factor picks Top vs Center. Both start at stretch 1 (equal split -> centered), matching
    // pimpl->textVerticalAlignment's own Qt::AlignVCenter default -- kept in sync by hand rather
    // than via setTextVerticalAlignment(), which no-ops when the requested value already matches
    // the stored one.
    pimpl->textColumnLayout->addStretch(1);

    pimpl->nameLabel=new ElidedLabel(textColumn);
    pimpl->nameLabel->setObjectName("nameLabel");
    pimpl->nameLabel->setElideMode(Qt::ElideMiddle);
    pimpl->nameLabel->setCursor(Qt::PointingHandCursor);
    pimpl->nameLabel->installEventFilter(this);
    pimpl->textColumnLayout->addWidget(pimpl->nameLabel);

    pimpl->textColumnLayout->addSpacing(TextLineSpacing);

    pimpl->infoLabel=new QLabel(textColumn);
    pimpl->infoLabel->setObjectName("infoLabel");
    pimpl->textColumnLayout->addWidget(pimpl->infoLabel);

    pimpl->textColumnLayout->addStretch(1);

    pimpl->menuButton=new IconTextButton(
        menuIcon(QStringLiteral("menu"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    pimpl->menuButton->setCursor(Qt::PointingHandCursor);
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
    pimpl->loadControl->setFileDescription(pimpl->item.fileName(),pimpl->item.isImage(),pimpl->incoming);

    pimpl->nameLabel->setText(pimpl->item.fileName());

    rebuildMenu();
}

//--------------------------------------------------------------------------

AbstractLoadControl* ChatMessageFileItem::loadControl() const
{
    return pimpl->loadControl->loadControl();
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageFileItem::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::setTextVerticalAlignment(Qt::Alignment alignment)
{
    auto vAlign=alignment & Qt::AlignVertical_Mask;
    if (vAlign!=Qt::AlignTop && vAlign!=Qt::AlignVCenter)
    {
        vAlign=Qt::AlignTop;
    }

    if (pimpl->textVerticalAlignment==vAlign)
    {
        return;
    }
    pimpl->textVerticalAlignment=vAlign;

    // Top: all leftover space goes to the bottom spacer (top spacer stretch 0), keeping both
    // lines flush with the icon slot's top edge. Center: both spacers get an equal share, so the
    // leftover splits evenly above and below and the two-line block centers as a whole.
    pimpl->textColumnLayout->setStretch(0,vAlign==Qt::AlignVCenter?1:0);
}

//--------------------------------------------------------------------------

Qt::Alignment ChatMessageFileItem::textVerticalAlignment() const noexcept
{
    return pimpl->textVerticalAlignment;
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
    pimpl->menu->setItems(buildChatFileMenuItems(pimpl->item,false,pimpl->incoming,this));
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
        if (it.state()==ChatFileTransferState::Running || it.state()==ChatFileTransferState::Paused)
        {
            pimpl->loadControl->setProgress(it.transferred(),it.size());
        }
        else if (it.state()==ChatFileTransferState::Complete)
        {
            pimpl->loadControl->setProgress(100.0);
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
