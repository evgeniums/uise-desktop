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
#include <QPointer>
#include <QStyle>

#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/imagelabel.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/loadcontrol.hpp>
#include <uise/desktop/loadcontrolmenu.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/pixmapscale.hpp>
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

//! Local path to feed ImageLabel::setImageFile() for animated content, empty otherwise.
//
// Restricted to formats that can actually animate: a plain JPEG/PNG tile keeps rendering
// item.preview() (a small pre-decoded thumbnail) rather than decoding the full-size original.
// A static image of one of these MIME types is harmless too -- ImageLabel demotes single-frame
// content to its still path on its own.
QString animatableLocalPath(const ChatFileItem& item)
{
    if (item.state()!=ChatFileTransferState::Ready || item.localPath().isEmpty())
    {
        return {};
    }

    const auto mime=item.mimeType();
    if (mime==QLatin1String("image/gif")
        || mime==QLatin1String("image/webp")
        || mime==QLatin1String("image/apng")
        || mime==QLatin1String("image/avif"))
    {
        return item.localPath();
    }

    return {};
}

}

//--------------------------------------------------------------------------

class ChatMessageImageItem_p
{
    public:

        ChatFileItem item;
        bool incoming=false;

        ImageLabel* preview=nullptr;
        LoadControlMenu* loadControl=nullptr;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;

        //! Path currently loaded into preview via setImageFile(), empty when the still
        //! (scaledAndCropped(item.preview())/svg-fallback) path is in use instead.
        QString loadedPath;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;
};

//--------------------------------------------------------------------------

ChatMessageImageItem::ChatMessageImageItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ChatMessageImageItem_p>())
{
    pimpl->preview=new ImageLabel(this);
    pimpl->preview->setObjectName("preview");
    pimpl->preview->setAutoSize(false);
    pimpl->preview->setCornersRadius(8,8);
    pimpl->preview->setCursor(Qt::PointingHandCursor);
    pimpl->preview->setClickable(true);
    pimpl->preview->setAnimationMode(pimpl->animationMode);
    connect(pimpl->preview,&ImageLabel::clicked,this,&ChatMessageImageItem::clicked);

    // floats over the tile's top-right corner, positioned by repositionOverlays() -- not added
    // to any layout of `this` (this widget has none)
    pimpl->menuButton=new IconTextButton(
        menuIcon(QStringLiteral("menu"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    pimpl->menuButton->setCursor(Qt::PointingHandCursor);

    // DropdownMenu is constructed parentless, like FileUploadListItem's own per-item menu -- see
    // that class's constructor for why (DropdownFrame reparents itself lazily to the trigger's
    // actual window() on first opening)
    pimpl->menu=new DropdownMenu();
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatMessageImageItem::onMenuItemTriggered);

    pimpl->loadControl=new LoadControlMenu(this);
    pimpl->loadControl->setObjectName("loadControl");
    connect(pimpl->loadControl,&LoadControlMenu::clicked,this,&ChatMessageImageItem::loadControlClicked);
    connect(pimpl->loadControl,&LoadControlMenu::pauseRequested,this,&ChatMessageImageItem::pauseRequested);
    connect(pimpl->loadControl,&LoadControlMenu::cancelRequested,this,&ChatMessageImageItem::cancelRequested);
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
        pimpl->loadControl->loadControl()->setClickable(isChatFileLoadControlClickable(pimpl->item.state()));
        // See ChatMessageFileItem::updateIconSlot()'s identical fix for why every non-
        // transferring state must explicitly zero progress, not just skip setProgress(): it's
        // a sticky member on the reused per-row LoadControl, and paintEvent() draws the arc
        // unconditionally from it.
        if (pimpl->item.state()==ChatFileTransferState::Running
            || pimpl->item.state()==ChatFileTransferState::Paused
            || pimpl->item.state()==ChatFileTransferState::Pending)
        {
            pimpl->loadControl->setProgress(pimpl->item.transferred(),pimpl->item.size());
        }
        else
        {
            pimpl->loadControl->setProgress(0.0);
        }
        pimpl->loadControl->raise();
    }
    pimpl->loadControl->setFileDescription(pimpl->item.fileName(),pimpl->item.isImage(),pimpl->incoming);

    rebuildMenu();
}

//--------------------------------------------------------------------------

AbstractLoadControl* ChatMessageImageItem::loadControl() const
{
    return pimpl->loadControl->loadControl();
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageImageItem::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::setAnimationMode(ImageLabel::AnimationMode mode)
{
    pimpl->animationMode=mode;
    pimpl->preview->setAnimationMode(mode);
}

//--------------------------------------------------------------------------

ImageLabel::AnimationMode ChatMessageImageItem::animationMode() const noexcept
{
    return pimpl->animationMode;
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

void ChatMessageImageItem::rebuildMenu()
{
    pimpl->menu->setItems(buildChatFileMenuItems(pimpl->item,true,pimpl->incoming,this));
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::updatePreview()
{
    if (width()<=0 || height()<=0)
    {
        // not laid out yet -- the next resizeEvent() will re-render against a real size
        return;
    }

    auto path=animatableLocalPath(pimpl->item);
    if (!path.isEmpty())
    {
        if (path==pimpl->loadedPath)
        {
            // already loaded -- ImageLabel rescales/re-renders itself from its own resizeEvent(),
            // re-loading here would re-decode the content and restart the animation
            return;
        }

        pimpl->preview->setSvgIcon(nullptr);
        if (pimpl->preview->setImageFile(path))
        {
            pimpl->loadedPath=path;
            setPlaceholderMode(false);
            return;
        }

        // decode failed (e.g. animated WebP without the qtimageformats plugin) -- fall through
        // to the static preview below
        pimpl->loadedPath.clear();
    }
    else if (!pimpl->loadedPath.isEmpty())
    {
        pimpl->preview->clearImage();
        pimpl->loadedPath.clear();
    }

    auto preview=pimpl->item.preview();
    if (!preview.isNull())
    {
        pimpl->preview->setSvgIcon(nullptr);
        pimpl->preview->setPixmap(scaledAndCropped(QPixmap::fromImage(preview),size()));
        setPlaceholderMode(false);
    }
    else
    {
        // Nothing to show: deliberately no fallback glyph. The tile already carries a centered
        // load control and a floating menu button, and an icon behind those read as noise --
        // the tile itself becomes the placeholder instead, drawn as an empty rounded outline
        // (see chatmessagefiles.qss's [placeholder="true"] rule).
        pimpl->preview->setSvgIcon(nullptr);
        pimpl->preview->setPixmap(QPixmap());
        setPlaceholderMode(true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::setPlaceholderMode(bool enable)
{
    if (property("placeholder").toBool()==enable)
    {
        return;
    }

    // Dynamic properties drive QSS selectors only after an explicit repolish -- Qt does not
    // re-evaluate stylesheets on a bare setProperty().
    setProperty("placeholder",enable);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::repositionOverlays()
{
    // Inset of the floating menu button from the tile's top-right corner. Comfortably clear of
    // the 3px placeholder outline (chatmessagefiles.qss's [placeholder="true"] rule) rather
    // than sitting right on it -- the button reads as floating over the tile, not attached to
    // its edge, and the same inset looks right over real photo content too.
    constexpr int margin=6;

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
