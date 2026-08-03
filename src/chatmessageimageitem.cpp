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

/** @file uise/desktop/src/chatmessageimageitem.cpp
*
*  Defines ChatMessageImageItem.
*
*/

/****************************************************************************/

#include <QResizeEvent>
#include <QMouseEvent>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/loadcontrol.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/chatmessageimageitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// LoadControl's footprint is fixed by its own stylesheet (uise--LoadControl { min/max-width/
// height: 56px }, see resources/style/loadcontrol.qss) -- kept here as a plain constant, the
// same value ChatMessageFileItem's icon slot uses, rather than trusting sizeHint(), which a
// stylesheet-only min/max constraint does not necessarily make accurate.
const QSize LoadControlSize{56,56};

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

class ChatMessageImageItem_p
{
    public:

        ChatFileItem item;
        bool incoming=false;

        RoundedImage* preview=nullptr;
        LoadControl* loadControl=nullptr;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;
};

//--------------------------------------------------------------------------

ChatMessageImageItem::ChatMessageImageItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ChatMessageImageItem_p>())
{
    pimpl->preview=new RoundedImage(this);
    pimpl->preview->setObjectName("preview");
    pimpl->preview->setAutoSize(false);
    pimpl->preview->setCornersRadius(8,8);
    pimpl->preview->setCursor(Qt::PointingHandCursor);
    pimpl->preview->installEventFilter(this);

    pimpl->menuButton=new IconTextButton(
        menuIcon(QStringLiteral("menu"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());

    // DropdownMenu is constructed parentless, like FileUploadListItem's own per-item menu -- see
    // that class's constructor for why (DropdownFrame reparents itself lazily to the trigger's
    // actual window() on first opening)
    pimpl->menu=new DropdownMenu();
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatMessageImageItem::onMenuItemTriggered);

    pimpl->loadControl=new LoadControl(this);
    pimpl->loadControl->setObjectName("loadControl");
    connect(pimpl->loadControl,&LoadControl::clicked,this,&ChatMessageImageItem::loadControlClicked);
}

//--------------------------------------------------------------------------

ChatMessageImageItem::~ChatMessageImageItem()
{
    if (!pimpl->menu.isNull())
    {
        destroyWidget(pimpl->menu);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::setItem(const ChatFileItem& item, bool incoming)
{
    pimpl->item=item;
    pimpl->incoming=incoming;
    refresh();
}

//--------------------------------------------------------------------------

const ChatFileItem& ChatMessageImageItem::item() const
{
    return pimpl->item;
}

//--------------------------------------------------------------------------

bool ChatMessageImageItem::isIncoming() const noexcept
{
    return pimpl->incoming;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::refresh()
{
    updatePreview();

    // unlike ChatMessageFileItem's icon slot, the preview is always shown -- the load control
    // is an overlay centered on top of it while not ready, not a replacement for it, per the
    // task brief ("clickable ... image preview with overlayed menu button ... In case the image
    // is not downloaded/uploaded then AbstractLoadControl is shown in the image center")
    const auto ready=(pimpl->item.state()==ChatFileTransferState::Ready);
    pimpl->loadControl->setVisible(!ready);
    if (!ready)
    {
        pimpl->loadControl->setState(chatFileLoadControlState(pimpl->item.state(),pimpl->incoming));
        if (pimpl->item.state()==ChatFileTransferState::Running)
        {
            pimpl->loadControl->setProgress(pimpl->item.transferred(),pimpl->item.size());
        }
        pimpl->loadControl->raise();
    }

    rebuildMenu();
}

//--------------------------------------------------------------------------

AbstractLoadControl* ChatMessageImageItem::loadControl() const
{
    return pimpl->loadControl;
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageImageItem::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::closeMenu()
{
    if (!pimpl->menu.isNull())
    {
        pimpl->menu->closeDropdown(true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);

    pimpl->preview->setGeometry(rect());
    pimpl->preview->setImageSize(size());
    updatePreview();

    repositionOverlays();
}

//--------------------------------------------------------------------------

bool ChatMessageImageItem::eventFilter(QObject* obj, QEvent* event)
{
    if (obj==pimpl->preview && event->type()==QEvent::MouseButtonPress)
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

void ChatMessageImageItem::rebuildMenu()
{
    std::vector<MenuItem> items;

    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::Open),tr("Open with"),menuIcon(QStringLiteral("open"),this)));
    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::SaveAs),tr("Save as"),menuIcon(QStringLiteral("saveAs"),this)));
    items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::Forward),tr("Forward"),menuIcon(QStringLiteral("forward"),this)));
    if (pimpl->item.isShowInFolderAvailable())
    {
        items.push_back(MenuItem(static_cast<int>(ChatFileMenuAction::ShowInFolder),tr("Show in folder"),menuIcon(QStringLiteral("showInFolder"),this)));
    }

    pimpl->menu->setItems(std::move(items));
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::updatePreview()
{
    if (width()<=0 || height()<=0)
    {
        // not laid out yet -- the next resizeEvent() will re-render against a real size
        return;
    }

    auto preview=pimpl->item.preview();
    if (!preview.isNull())
    {
        pimpl->preview->setSvgIcon(nullptr);
        pimpl->preview->setPixmap(scaledAndCropped(QPixmap::fromImage(preview),size()));
    }
    else
    {
        pimpl->preview->setPixmap(QPixmap());
        pimpl->preview->setSvgIcon(Style::instance().svgIconLocator().icon(QStringLiteral("ChatMessageFiles::image"),this));
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::repositionOverlays()
{
    constexpr int margin=4;

    auto menuSize=pimpl->menuButton->sizeHint();
    pimpl->menuButton->setGeometry(width()-menuSize.width()-margin,margin,menuSize.width(),menuSize.height());
    pimpl->menuButton->raise();

    pimpl->loadControl->setGeometry(
        (width()-LoadControlSize.width())/2,
        (height()-LoadControlSize.height())/2,
        LoadControlSize.width(),
        LoadControlSize.height()
    );
    pimpl->loadControl->raise();
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::onMenuItemTriggered(int id)
{
    emit menuTriggered(id);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
