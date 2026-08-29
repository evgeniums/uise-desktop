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
#include <uise/desktop/utils/dragsource.hpp>
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

        QFrame* textColumn=nullptr;
        ElidedLabel* nameLabel=nullptr;
        QLabel* infoLabel=nullptr;
        QVBoxLayout* textColumnLayout=nullptr;
        Qt::Alignment textVerticalAlignment=Qt::AlignVCenter;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;

        //! Set by rebuildMenu(), consumed (and cleared) the next time the menu button is
        //! clicked -- see the constructor's clicked() handler. Defers buildChatFileMenuItems()
        //! (one svg-icon lookup per entry) from every refresh() call to only rows whose menu is
        //! actually opened.
        bool menuDirty=true;

        bool dragEnabled=true;
        DragGesture dragGesture;

        //! Set by the current handleDragPress() call -- whether THIS press originated on
        //! fileIcon/nameLabel/imagePreview (via eventFilter()) rather than on the row's own
        //! background/other children (via mousePressEvent()). Read back by handleDragRelease()
        //! to gate clicked(); see handleDragPress()'s own doc comment (chatmessagefileitem.hpp).
        bool pressedOnClickTarget=false;
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
    // Not clickable itself -- its clicks are now routed through this row's own event filter
    // below, alongside fileIcon/nameLabel, so a press on it can also arm the drag gesture. A
    // pure gating flag, no visual effect (avatar.hpp).
    pimpl->imagePreview->setClickable(false);
    pimpl->imagePreview->setCursor(Qt::PointingHandCursor);
    pimpl->imagePreview->installEventFilter(this);

    // The load control overlay is created lazily on first use (see ensureLoadControl()) -- an
    // already-transferred row never needs it, and this row lives in a chat message list where
    // per-row widget count is on the scroll hot path. Unlike ChatMessageImageItem's menu button
    // (a hover-only overlay), this row's own menuButton below has no such gating -- it is a
    // permanent, always-visible part of every row's layout -- so it stays eager; only its
    // drop-down's item list is deferred (see the clicked() handler below).
    pimpl->textColumn=new QFrame(this);
    pimpl->textColumn->setObjectName("textColumn");
    pimpl->textColumnLayout=Layout::vertical(pimpl->textColumn);
    layout->addWidget(pimpl->textColumn,1);

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

    pimpl->nameLabel=new ElidedLabel(pimpl->textColumn);
    pimpl->nameLabel->setObjectName("nameLabel");
    pimpl->nameLabel->setElideMode(Qt::ElideMiddle);
    pimpl->nameLabel->setCursor(Qt::PointingHandCursor);
    pimpl->nameLabel->installEventFilter(this);
    pimpl->textColumnLayout->addWidget(pimpl->nameLabel);

    pimpl->textColumnLayout->addSpacing(TextLineSpacing);

    pimpl->infoLabel=new QLabel(pimpl->textColumn);
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

    // Menu items are only ever built the moment the menu button is actually clicked, not on
    // every refresh() -- see rebuildMenu()'s doc comment -- via the button's own clicked()
    // rather than DropdownFrame::aboutToShow(): DropdownFrame::popupBelow()/popupAt() already
    // run fillContent()+measure() BEFORE beginOpen() emits aboutToShow() (despite that signal's
    // "right before content is filled and measured" doc comment), so rebuilding on aboutToShow()
    // is one step too late and measures an empty, tiny popup -- see
    // ChatMessageImageItem::ensureMenuButton() for the full account of this bug. Connected here,
    // BEFORE attachTo() below wires its own clicked handler that actually opens the dropdown, so
    // this slot runs first -- Qt invokes same-signal slots in connection order.
    connect(pimpl->menuButton,&IconTextButton::clicked,this,
        [this]()
        {
            if (pimpl->menuDirty)
            {
                pimpl->menu->setItems(buildChatFileMenuItems(pimpl->item,false,pimpl->incoming,this));
                pimpl->menuDirty=false;
            }
        }
    );

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
    // todo-file-descriptor-content-missing-recovery.md: NotLoaded is unreachable for a
    // genuinely not-yet-sent outgoing item (it always starts Ready or Pending/Uploading, never
    // NotLoaded) - so a NotLoaded item, regardless of message direction, is always asking to be
    // DOWNLOADED (recovered), never uploaded. Override here so every consumer of pimpl->incoming
    // (load control state icon via chatFileLoadControlState(), file-description text, this
    // item's own context menu) agrees and shows Download rather than Upload.
    pimpl->incoming=incoming || item.state()==ChatFileTransferState::NotLoaded;
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
    return ensureLoadControl()->loadControl();
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

void ChatMessageFileItem::limitWidth(int totalWidth)
{
    if (totalWidth<=0)
    {
        pimpl->nameLabel->setMaximumWidth(QWIDGETSIZE_MAX);
        return;
    }

    // Measured directly from widgets whose width is independent of the file name -- icon slot,
    // menu button, textColumn's own QSS margins -- rather than derived by subtracting nameLabel's
    // own sizeHint() from this row's sizeHint(). That subtraction raced with the very
    // setMaximumWidth() call below (an earlier call's cap was still part of "this row's
    // sizeHint()" whenever the layout hadn't re-settled yet), so the computed overhead drifted
    // across repeated negotiation passes and the bubble visibly flickered between the full
    // available width and the correctly capped one. None of the widgets read here are affected by
    // nameLabel's own maximumWidth, so this is stable no matter how many times it is called.
    auto textColumnMargins=pimpl->textColumn->contentsMargins();
    auto overhead=pimpl->iconSlot->sizeHint().width()
                 +pimpl->menuButton->sizeHint().width()
                 +textColumnMargins.left()+textColumnMargins.right();

    auto nameWidth=totalWidth-overhead;
    if (nameWidth<0)
    {
        nameWidth=0;
    }
    pimpl->nameLabel->setMaximumWidth(nameWidth);
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
    if (obj==pimpl->fileIcon || obj==pimpl->nameLabel || obj==pimpl->imagePreview)
    {
        auto* child=qobject_cast<QWidget*>(obj);
        switch (event->type())
        {
            case (QEvent::MouseButtonPress):
            {
                auto me=static_cast<QMouseEvent*>(event);
                if (me->button()==Qt::LeftButton)
                {
                    // isClickTarget=true -- this filter only ever runs for fileIcon/nameLabel/
                    // imagePreview (see the obj== check above), the three widgets a plain click
                    // is actually meant to open.
                    handleDragPress(child->mapTo(this,me->pos()),true);
                    return true;
                }
                break;
            }

            case (QEvent::MouseMove):
            {
                auto me=static_cast<QMouseEvent*>(event);
                if (me->buttons() & Qt::LeftButton)
                {
                    handleDragMove(child->mapTo(this,me->pos()));
                    return true;
                }
                break;
            }

            case (QEvent::MouseButtonRelease):
            {
                auto me=static_cast<QMouseEvent*>(event);
                if (me->button()==Qt::LeftButton)
                {
                    handleDragRelease();
                    return true;
                }
                break;
            }

            default:
                break;
        }
    }
    return QFrame::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::handleDragPress(const QPoint& pos, bool isClickTarget)
{
    pimpl->pressedOnClickTarget=isClickTarget;

    if (!pimpl->dragEnabled)
    {
        if (isClickTarget)
        {
            emit clicked();
        }
        return;
    }

    pimpl->dragGesture.press(pos);
    emit dragPrepareRequested();
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::handleDragMove(const QPoint& pos)
{
    if (!pimpl->dragEnabled || !pimpl->dragGesture.isArmed())
    {
        return;
    }

    if (pimpl->dragGesture.movedPastThreshold(pos))
    {
        emit dragStartRequested();
    }
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::handleDragRelease()
{
    if (!pimpl->dragEnabled || !pimpl->dragGesture.isArmed())
    {
        return;
    }

    // isClickTarget-gated: per AbstractChatMessageFiles::itemClicked's own contract
    // ("icon/preview or file name"), a plain click landing anywhere ELSE on the row (its own
    // background, or a leaked/unaccepted event from a child like menuButton -- see
    // handleDragPress()'s own doc comment, chatmessagefileitem.hpp) must not open the file.
    // The drag gesture itself is still tracked from anywhere on the row regardless (see
    // dragGesture.reset() below, unconditional) -- only the click emission is scoped.
    if (pimpl->dragGesture.releaseIsClick() && pimpl->pressedOnClickTarget)
    {
        emit clicked();
    }
    pimpl->dragGesture.reset();
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        // isClickTarget=false -- a press reaching the frame's own handler landed on the row's
        // background or on a child that doesn't route through eventFilter() above (e.g.
        // menuButton, if its own accept() were ever missing) -- still a valid drag-gesture
        // start (see handleDragPress()'s own doc comment), but never a click that opens the
        // file.
        handleDragPress(event->pos(),false);
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::mouseMoveEvent(QMouseEvent* event)
{
    if (pimpl->dragEnabled && pimpl->dragGesture.isArmed() && (event->buttons() & Qt::LeftButton))
    {
        handleDragMove(event->pos());
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (pimpl->dragEnabled && pimpl->dragGesture.isArmed() && event->button()==Qt::LeftButton)
    {
        handleDragRelease();
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::setDragEnabled(bool enable)
{
    pimpl->dragEnabled=enable;
}

//--------------------------------------------------------------------------

bool ChatMessageFileItem::isDragEnabled() const noexcept
{
    return pimpl->dragEnabled;
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::startDrag(const QList<QUrl>& urls)
{
    startFileUrlDrag(this,urls,grab().scaled(160,160,Qt::KeepAspectRatio,Qt::SmoothTransformation));
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::rebuildMenu()
{
    // Deferred: buildChatFileMenuItems() (one svg-icon lookup per entry) only actually runs the
    // next time the menu button is clicked (see the constructor's clicked() handler), not on
    // every refresh() -- most refresh() calls are just a progress tick and the menu is never
    // opened for most rows at all.
    pimpl->menuDirty=true;
}

//--------------------------------------------------------------------------

void ChatMessageFileItem::updateIconSlot()
{
    const auto& it=pimpl->item;
    // Unresolved is treated the same as Ready here -- neither has a load control to show, they
    // differ only in what state a later refresh() may resolve to. Showing a download affordance
    // before the caller even knows one is needed is exactly the wrong-glyph flicker this state
    // exists to avoid; while unresolved this row renders its plain identity (type icon / image
    // preview) like any already-available item.
    const auto ready=(it.state()==ChatFileTransferState::Ready
                       || it.state()==ChatFileTransferState::Unresolved);

    pimpl->fileIcon->setVisible(false);
    pimpl->imagePreview->setVisible(false);

    if (!ready)
    {
        auto loadControl=ensureLoadControl();
        loadControl->setVisible(true);
        loadControl->setState(chatFileLoadControlState(it.state(),pimpl->incoming));
        loadControl->loadControl()->setClickable(isChatFileLoadControlClickable(it.state()));
        // m_progress is a sticky member on the reused per-row LoadControl (setState() never
        // touches it) and paintEvent() draws the progress arc unconditionally from it, in
        // every state -- so every non-transferring state must explicitly zero it, not just
        // skip the setProgress() call, or a stale ring (a full one for Complete, a partial
        // one for e.g. Running->Failed) paints behind that state's own glyph. Pending is kept
        // alongside Running/Paused so a re-queued item with real seeded partial bytes doesn't
        // lose that partial ring.
        if (it.state()==ChatFileTransferState::Running
            || it.state()==ChatFileTransferState::Paused
            || it.state()==ChatFileTransferState::Pending)
        {
            loadControl->setProgress(it.transferred(),it.size());
        }
        else
        {
            loadControl->setProgress(0.0);
        }
        // A Running item with no measurable progress yet is exactly what Indeterminate exists
        // for ("fixed-span arc circulating around the circle; progress() ignored") -- without
        // this, such an item draws a zero-length Static arc, i.e. a Running control visually
        // indistinguishable from a stalled one. Reachable whenever a transfer is underway but
        // no bytes have moved yet: for an upload that covers the whole local prepare/create
        // phase, which a host may legitimately report as Running (a sender's content can be
        // fully prepared locally long before the first byte leaves). Once real bytes are
        // moving, AnimatedProgress is the mode that actually means "working, and here's how
        // far" -- arc length still reflects progress() like Static, but it also circulates,
        // so the control doesn't go visually still the moment a real number is available.
        // Paused/Pending stay Static deliberately -- a paused or queued item is NOT working,
        // so a circulating arc would misrepresent it; they keep whatever partial ring they
        // already earned.
        auto progressMode=AbstractLoadControl::ProgressMode::Static;
        if (it.state()==ChatFileTransferState::Running)
        {
            progressMode=(it.transferred()<=0)
                ? AbstractLoadControl::ProgressMode::Indeterminate
                : AbstractLoadControl::ProgressMode::AnimatedProgress;
        }
        loadControl->loadControl()->setProgressMode(progressMode);
        // Only used to build the Pause/Cancel menu text (see LoadControlMenu::
        // setFileDescription()'s doc comment), so only needed while the control is shown.
        loadControl->setFileDescription(it.fileName(),it.isImage(),pimpl->incoming);
        return;
    }

    if (pimpl->loadControl!=nullptr)
    {
        // Never create a load control just to hide it -- an already-Ready row never needed one
        // in the first place.
        pimpl->loadControl->setVisible(false);
    }

    if (it.isImage())
    {
        pimpl->imagePreview->setVisible(true);
        auto preview=it.preview();
        if (!preview.isNull())
        {
            // Scale to PHYSICAL pixels and tag with devicePixelRatio -- same rule as
            // ChatMessageImageItem::updatePreview(), see that function's comment for why both
            // halves (physical-size source AND the tag) are required for a sharp brush-texture
            // paint on HiDPI/Retina displays.
            const qreal dpr=devicePixelRatioF();
            QSize physicalSize(qRound(IconSlotSize.width()*dpr),qRound(IconSlotSize.height()*dpr));
            auto px=scaledAndCropped(QPixmap::fromImage(preview),physicalSize);
            px.setDevicePixelRatio(dpr);
            pimpl->imagePreview->setPixmap(px);
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

LoadControlMenu* ChatMessageFileItem::ensureLoadControl() const
{
    if (pimpl->loadControl==nullptr)
    {
        auto self=const_cast<ChatMessageFileItem*>(this);

        pimpl->loadControl=new LoadControlMenu(pimpl->iconSlot);
        pimpl->loadControl->setObjectName("loadControl");
        // iconSlot is a fixed-size frame (see IconSlotSize) -- unlike ChatMessageImageItem's
        // overlays, this geometry is a one-time constant, not something a resizeEvent() needs
        // to keep re-applying.
        pimpl->loadControl->setGeometry(QRect(QPoint(0,0),IconSlotSize));
        connect(pimpl->loadControl,&LoadControlMenu::clicked,self,&ChatMessageFileItem::loadControlClicked);
        connect(pimpl->loadControl,&LoadControlMenu::pauseRequested,self,&ChatMessageFileItem::pauseRequested);
        connect(pimpl->loadControl,&LoadControlMenu::cancelRequested,self,&ChatMessageFileItem::cancelRequested);

        // See ChatMessageImages::rebuildGrid()'s identical comment on freshly created tiles:
        // QSS-driven content must be polished before its first paint.
        pimpl->loadControl->ensurePolished();
        pimpl->loadControl->show();
    }
    return pimpl->loadControl;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
