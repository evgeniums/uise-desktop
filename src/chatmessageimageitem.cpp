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
#include <uise/desktop/utils/dragsource.hpp>
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

//! Content to feed ImageLabel for animated playback: either a local path (setImageFile()) or
//! in-memory bytes (setImageData()), empty when neither applies.
struct AnimatableContent
{
    QString    path;
    QByteArray data;
    QByteArray format;

    bool isEmpty() const noexcept
    {
        return path.isEmpty() && data.isEmpty();
    }
};

//! Resolves animatableContent() for item, preferring bytes over a local path -- see
//! ChatFileItem::animatedData()'s own doc comment for why the bytes branch is not gated on
//! state()/mime the way the path branch is.
AnimatableContent animatableContent(const ChatFileItem& item)
{
    if (!item.animatedData().isEmpty())
    {
        return {{},item.animatedData(),item.animatedFormat()};
    }

    if (item.state()!=ChatFileTransferState::Ready || item.localPath().isEmpty())
    {
        return {};
    }

    // Restricted to formats that can actually animate: a plain JPEG/PNG tile keeps rendering
    // item.preview() (a small pre-decoded thumbnail) rather than decoding the full-size original.
    // A static image of one of these MIME types is harmless too -- ImageLabel demotes single-frame
    // content to its still path on its own.
    const auto mime=item.mimeType();
    if (mime==QLatin1String("image/gif")
        || mime==QLatin1String("image/webp")
        || mime==QLatin1String("image/apng")
        || mime==QLatin1String("image/avif"))
    {
        return {item.localPath(),{},{}};
    }

    return {};
}

//! Size the real content (a rung sharing `natural`'s aspect ratio, up to rounding) would end up
//! at when fitted into `box` via scaledToFit(box,natural,maxUpscale) -- computed from `natural`
//! alone, without needing an actual QPixmap, so it can be reused as the placeholder's target
//! crop size too (see updatePreview()) and keep the visible content box identical across the
//! placeholder-to-real-content swap. Reproduces QPixmap::scaled(target,Qt::KeepAspectRatio)'s own
//! size formula for a source whose size is `natural`.
QSize fittedContentSize(const QSize& natural, const QSize& box, qreal maxUpscale)
{
    if (natural.width()<=0 || natural.height()<=0 || box.width()<=0 || box.height()<=0)
    {
        // no natural size to fit against -- fill the box, matching scaledAndCropped()'s own
        // cover behaviour for the "pixelSize is unknown" case this falls back to
        return box;
    }
    QSize limit=(maxUpscale>1.0)
        ? QSize(qRound(natural.width()*maxUpscale),qRound(natural.height()*maxUpscale))
        : natural;
    QSize target(qMin(limit.width(),box.width()),qMin(limit.height(),box.height()));
    auto factor=qMin(
        static_cast<qreal>(target.width())/natural.width(),
        static_cast<qreal>(target.height())/natural.height()
    );
    return QSize(qMax(1,qRound(natural.width()*factor)),qMax(1,qRound(natural.height()*factor)));
}

//! Compose `content` centred onto a transparent canvas of exactly `canvasSize` -- the same
//! centring scaledToFitPadded() does for real content, reused for the placeholder crop so both
//! states paint into the same content box (see updatePreview()).
QPixmap composePadded(const QPixmap& content, const QSize& canvasSize)
{
    QPixmap canvas(canvasSize);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QRect target(QPoint(0,0),content.size());
    target.moveCenter(QRect(QPoint(0,0),canvasSize).center());
    painter.drawPixmap(target,content);
    return canvas;
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
        bool menuButtonVisibleOnHover=true;

        //! Set by rebuildMenu(), consumed (and cleared) the next time the drop-down is about
        //! to open -- see ensureMenuButton(). Defers buildChatFileMenuItems() (one svg-icon
        //! lookup per entry) from every refresh() call to only the tiles that are actually
        //! opened.
        bool menuDirty=true;

        //! Path currently loaded into preview via setImageFile(), empty when the still
        //! (scaledToFitPadded(item.preview())/svg-fallback) path is in use instead.
        QString loadedPath;

        //! Bytes currently loaded into preview via setImageData(), empty when loadedPath or the
        //! still path is in use instead. Kept alive here (QByteArray is implicitly shared) purely
        //! so updatePreview()'s identity check has something to compare against -- see its own
        //! doc comment on why this is a (size,constData()) identity test, not a memcmp.
        QByteArray loadedData;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;

        //! See ChatMessageImageItem::setMaxUpscale()'s own doc comment. Matches
        //! ChatMessageImages::TileMaxUpscale, restated here as a plain default rather than
        //! shared via a header the way DefaultMaxWidth/PlaceholderTileExtent are not either --
        //! this tile has no dependency on albumlayout.hpp at all, only on whatever concrete
        //! value its owner pushes through setMaxUpscale().
        qreal maxUpscale=2.0;

        bool dragEnabled=true;
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
    // ImageLabel defaults to KeepAspectRatioByExpanding, i.e. it CROPS -- which would silently
    // undo updatePreview()'s never-crop rule for the one branch that bypasses it: animatable
    // content (gif/webp/apng/avif), which ImageLabel renders itself from setImageFile() via
    // renderTile() rather than from the pixmap updatePreview() composes. Without this, the same
    // image could render uncropped or cropped depending purely on whether its mime happened to be
    // animatable and its local file had resolved yet -- exactly the intermittent "sometimes looks
    // cropped after reopening the page" symptom.
    pimpl->preview->setAspectRatioMode(Qt::KeepAspectRatio);
    pimpl->preview->setAnimationMode(pimpl->animationMode);
    // Defensive restatement of ImageLabel's own defaults, in the same spirit as
    // setAspectRatioMode() above -- this tile lives in a flyweight scrolling list where several
    // multi-MB animated GIFs can be bound at once, so CacheAll (all decoded frames resident) and
    // playback while the window is in the background are both explicitly ruled out here rather
    // than left to whatever ImageLabel's default happens to be.
    pimpl->preview->setCacheFrames(false);
    pimpl->preview->setPauseWhenWindowInactive(true);
    pimpl->preview->setDragEnabled(pimpl->dragEnabled);
    connect(pimpl->preview,&ImageLabel::clicked,this,&ChatMessageImageItem::clicked);
    connect(pimpl->preview,&ImageLabel::dragPrepareRequested,this,&ChatMessageImageItem::dragPrepareRequested);
    connect(pimpl->preview,&ImageLabel::dragStartRequested,this,&ChatMessageImageItem::dragStartRequested);

    // The load control overlay, the menu button and its drop-down are all created lazily on
    // demand (see ensureLoadControl()/ensureMenuButton()) -- a transferred, never-hovered tile
    // never needs either, and this tile lives in a flyweight chat list where per-item widget
    // count is on the scroll hot path.
    updateMenuButtonVisibility();
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
    // todo-file-descriptor-content-missing-recovery.md: see ChatMessageFileItem::setItem()'s
    // identical override - NotLoaded is unreachable for a genuinely not-yet-sent outgoing item,
    // so it always means DOWNLOADED (recovered), never uploaded, regardless of direction.
    pimpl->incoming=incoming || item.state()==ChatFileTransferState::NotLoaded;
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
    if (!ready)
    {
        auto loadControl=ensureLoadControl();
        loadControl->setVisible(true);
        loadControl->setState(chatFileLoadControlState(pimpl->item.state(),pimpl->incoming));
        loadControl->loadControl()->setClickable(isChatFileLoadControlClickable(pimpl->item.state()));
        // See ChatMessageFileItem::updateIconSlot()'s identical fix for why every non-
        // transferring state must explicitly zero progress, not just skip setProgress(): it's
        // a sticky member on the reused per-row LoadControl, and paintEvent() draws the arc
        // unconditionally from it.
        if (pimpl->item.state()==ChatFileTransferState::Running
            || pimpl->item.state()==ChatFileTransferState::Paused
            || pimpl->item.state()==ChatFileTransferState::Pending)
        {
            loadControl->setProgress(pimpl->item.transferred(),pimpl->item.size());
        }
        else
        {
            loadControl->setProgress(0.0);
        }
        // See ChatMessageFileItem::updateIconSlot()'s identical block for the rationale: a
        // Running item with no measurable progress yet draws a zero-length Static arc,
        // indistinguishable from stalled -- Indeterminate is the mode for exactly that.
        // Once real bytes are moving, AnimatedProgress keeps it circulating while still
        // reflecting progress() like Static would, so it doesn't go visually still the
        // moment a real number is available.
        auto progressMode=AbstractLoadControl::ProgressMode::Static;
        if (pimpl->item.state()==ChatFileTransferState::Running)
        {
            progressMode=(pimpl->item.transferred()<=0)
                ? AbstractLoadControl::ProgressMode::Indeterminate
                : AbstractLoadControl::ProgressMode::AnimatedProgress;
        }
        loadControl->loadControl()->setProgressMode(progressMode);
        // Only used to build the Pause/Cancel menu text (see LoadControlMenu::
        // setFileDescription()'s doc comment), so only needed while the control is shown.
        loadControl->setFileDescription(pimpl->item.fileName(),pimpl->item.isImage(),pimpl->incoming);
        loadControl->raise();
    }
    else if (pimpl->loadControl!=nullptr)
    {
        // Never create a load control just to hide it -- a transferred, never-not-ready tile
        // never needed one in the first place.
        pimpl->loadControl->setVisible(false);
    }

    rebuildMenu();
}

//--------------------------------------------------------------------------

AbstractLoadControl* ChatMessageImageItem::loadControl() const
{
    return ensureLoadControl()->loadControl();
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageImageItem::menuButton() const
{
    return ensureMenuButton();
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

void ChatMessageImageItem::setMaxUpscale(qreal maxUpscale)
{
    if (pimpl->maxUpscale==maxUpscale)
    {
        return;
    }
    pimpl->maxUpscale=maxUpscale;
    updatePreview();
}

//--------------------------------------------------------------------------

qreal ChatMessageImageItem::maxUpscale() const noexcept
{
    return pimpl->maxUpscale;
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

void ChatMessageImageItem::setMenuButtonVisibleOnHover(bool enable)
{
    if (pimpl->menuButtonVisibleOnHover==enable)
    {
        return;
    }

    pimpl->menuButtonVisibleOnHover=enable;
    updateMenuButtonVisibility();
}

//--------------------------------------------------------------------------

bool ChatMessageImageItem::menuButtonVisibleOnHover() const noexcept
{
    return pimpl->menuButtonVisibleOnHover;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::setDragEnabled(bool enable)
{
    pimpl->dragEnabled=enable;
    pimpl->preview->setDragEnabled(enable);
}

//--------------------------------------------------------------------------

bool ChatMessageImageItem::isDragEnabled() const noexcept
{
    return pimpl->dragEnabled;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::startDrag(const QList<QUrl>& urls)
{
    // A placeholder is a stand-in for content that hasn't resolved yet -- not worth using as a
    // drag pixmap; Qt's own default drag cursor is a better signal than a blurry placeholder.
    QPixmap preview;
    if (!pimpl->item.isPreviewPlaceholder() && !pimpl->item.preview().isNull())
    {
        preview=scaledToFit(QPixmap::fromImage(pimpl->item.preview()),QSize(160,160));
    }
    startFileUrlDrag(this,urls,preview);
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

void ChatMessageImageItem::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    updateMenuButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    updateMenuButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::rebuildMenu()
{
    // Deferred: buildChatFileMenuItems() (one svg-icon lookup per entry) only actually runs the
    // next time the menu button is clicked (see ensureMenuButton()'s clicked() handler), not on
    // every refresh() -- most refresh() calls are just a progress tick and the menu is never
    // opened for most tiles at all.
    pimpl->menuDirty=true;
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::updatePreview()
{
    if (width()<=0 || height()<=0)
    {
        // not laid out yet -- the next resizeEvent() will re-render against a real size
        return;
    }

    auto content=animatableContent(pimpl->item);
    if (!content.isEmpty())
    {
        // Already loaded -- ImageLabel rescales/re-renders itself from its own resizeEvent();
        // re-loading here would re-decode the content and restart the animation.
        //
        // Bytes are compared by (size, constData()) rather than by value: QByteArray is
        // implicitly shared, so the host's stored buffer and pimpl->loadedData's copy of it share
        // one d-pointer, making this an O(1) identity test rather than an O(n) memcmp on a
        // multi-megabyte GIF, on every single refresh(). It cannot false-positive either:
        // pimpl->loadedData holds a reference that keeps the old buffer alive, so a genuinely new
        // allocation can never reuse its address while we are still comparing.
        bool same=content.path.isEmpty()
            ? (!pimpl->loadedData.isEmpty()
               && content.data.size()==pimpl->loadedData.size()
               && content.data.constData()==pimpl->loadedData.constData())
            : (content.path==pimpl->loadedPath);
        if (same)
        {
            return;
        }

        pimpl->preview->setSvgIcon(nullptr);
        bool ok=content.path.isEmpty()
            ? pimpl->preview->setImageData(content.data,content.format)
            : pimpl->preview->setImageFile(content.path);
        if (ok)
        {
            pimpl->loadedPath=content.path;
            pimpl->loadedData=content.data;
            setPlaceholderMode(false);
            return;
        }

        // decode failed (e.g. animated WebP without the qtimageformats plugin) -- fall through
        // to the static preview below
        pimpl->loadedPath.clear();
        pimpl->loadedData.clear();
    }
    else if (!pimpl->loadedPath.isEmpty() || !pimpl->loadedData.isEmpty())
    {
        pimpl->preview->clearImage();
        pimpl->loadedPath.clear();
        pimpl->loadedData.clear();
    }

    auto preview=pimpl->item.preview();
    if (!preview.isNull())
    {
        pimpl->preview->setSvgIcon(nullptr);

        // Fit the image inside the tile, preserving its own aspect ratio and never cropping it --
        // confirmed requirement: a chat image tile must show the full original image, only ever
        // scaled down, bounded-upscaled at most maxUpscale() times its own natural resolution
        // (see setMaxUpscale()'s doc comment) -- purely a paint-time allowance on THIS tile's own
        // already-decided rect, never on album layout geometry (see albumLayout()'s own doc
        // comment for why a whole-album resolution clamp was tried and reverted instead).
        // scaledToFitPadded() (not scaledAndCropped()) composes the fitted image centred onto an
        // exactly-tile-sized canvas, since RoundedImage::paintEvent()'s QBrush texture fill needs
        // an exact-size pixmap to render correctly at all (a smaller one tiles instead of
        // centering).
        //
        // ...but a PLACEHOLDER preview -- the embedded thumbnail files2 generates, a SQUARE
        // centre-crop (ScaleMode::FillCrop) -- is cropped to fill CONTENT BOX SIZE below instead
        // of the whole tile: scale to COVER that box, preserving the thumbnail's own aspect
        // ratio, and centre-crop the overflow, same policy every other thumbnail chip in this
        // library already uses (see FileUploadListItem::updatePreviews(), ChatMessageFileItem,
        // ImagePreviewStrip) -- then padded onto the tile canvas exactly like real content is.
        // Using the SAME content box for both states (fittedContentSize(), computed from
        // item.pixelSize() alone, before any real content is local) is what keeps the visible
        // box from jumping in size when the placeholder is later replaced by a real rung: without
        // it, a small original's placeholder used to fill the whole tile and the real rung would
        // then appear at a much smaller, padded size once it arrived.
        //
        // Scale to PHYSICAL pixels and tag the result with the screen's devicePixelRatio --
        // the brush fill DOES honor the tag (see FileUploadListItem::updatePreviews() and
        // pixmapscale.hpp's own doc comments for the same rule). Without both halves --
        // physical-size canvas AND the tag -- the tile rasterises at 1x and reads as soft/blurry
        // on any HiDPI/Retina display.
        // item.pixelSize() is the ORIGINAL image's own pixel size, known from the attachment
        // metadata (chat_file_item::width/height) long before any content is local. Passing it
        // is what stops a reduced-resolution RUNG from being letterboxed: the preview handed
        // over here is whichever rung the image source resolved (for a chat tile, normally the
        // 1080px `chat` rung -- see whitemdesktop's ChatImageSource), which on a HiDPI display
        // is routinely SMALLER than this tile's physical box, and scaledToFit()'s never-upscale
        // rule would then centre it at native size and pad the remainder. The rule is about the
        // ORIGINAL's resolution, not the delivered rung's -- see scaledToFit()'s own doc
        // comment. A genuinely small image is unaffected up to maxUpscale(): there pixelSize()
        // equals the delivered pixmap's size, so the clamp still refuses to enlarge it further.
        const qreal dpr=devicePixelRatioF();
        QSize physicalSize(qRound(size().width()*dpr),qRound(size().height()*dpr));
        auto contentBox=fittedContentSize(pimpl->item.pixelSize(),physicalSize,pimpl->maxUpscale);
        auto srcPx=QPixmap::fromImage(preview);
        auto px=pimpl->item.isPreviewPlaceholder()
            ? composePadded(scaledAndCropped(srcPx,contentBox),physicalSize)
            : scaledToFitPadded(srcPx,physicalSize,pimpl->item.pixelSize(),pimpl->maxUpscale);
        px.setDevicePixelRatio(dpr);
        pimpl->preview->setPixmap(px);
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
    // Dynamic properties drive QSS selectors only after an explicit repolish -- Qt does not
    // re-evaluate stylesheets on a bare setProperty(). Style::setStyleProperty() guards on "value
    // actually changed" to avoid a repaint storm when this is called repeatedly with the same mode.
    if (Style::setStyleProperty(this,"placeholder",enable))
    {
        update();
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::repositionOverlays()
{
    // Inset of the floating menu button from the tile's top-right corner. Comfortably clear of
    // the 3px placeholder outline (chatmessagefiles.qss's [placeholder="true"] rule) rather
    // than sitting right on it -- the button reads as floating over the tile, not attached to
    // its edge, and the same inset looks right over real photo content too.
    constexpr int margin=6;

    // Both overlays are created lazily -- guarded independently since a tile can have either,
    // both, or neither at any given moment.
    if (pimpl->menuButton!=nullptr)
    {
        auto menuSize=pimpl->menuButton->sizeHint();
        pimpl->menuButton->setGeometry(width()-menuSize.width()-margin,margin,menuSize.width(),menuSize.height());
        pimpl->menuButton->raise();
    }

    if (pimpl->loadControl!=nullptr)
    {
        pimpl->loadControl->setGeometry(
            (width()-LoadControlSize.width())/2,
            (height()-LoadControlSize.height())/2,
            LoadControlSize.width(),
            LoadControlSize.height()
        );
        pimpl->loadControl->raise();
    }
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::updateMenuButtonVisibility()
{
    bool visible=!pimpl->menuButtonVisibleOnHover
        || underMouse()
        || (!pimpl->menu.isNull() && pimpl->menu->isOpen());

    if (!visible && pimpl->menuButton==nullptr)
    {
        // Never create the menu button just to leave it hidden -- most tiles are never
        // hovered.
        return;
    }

    ensureMenuButton()->setVisible(visible);
}

//--------------------------------------------------------------------------

void ChatMessageImageItem::onMenuItemTriggered(int id)
{
    emit menuTriggered(id);
}

//--------------------------------------------------------------------------

LoadControlMenu* ChatMessageImageItem::ensureLoadControl() const
{
    if (pimpl->loadControl==nullptr)
    {
        pimpl->loadControl=new LoadControlMenu(const_cast<ChatMessageImageItem*>(this));
        pimpl->loadControl->setObjectName("loadControl");
        connect(pimpl->loadControl,&LoadControlMenu::clicked,this,&ChatMessageImageItem::loadControlClicked);
        connect(pimpl->loadControl,&LoadControlMenu::pauseRequested,this,&ChatMessageImageItem::pauseRequested);
        connect(pimpl->loadControl,&LoadControlMenu::cancelRequested,this,&ChatMessageImageItem::cancelRequested);

        // See rebuildGrid()'s identical comment on freshly created tiles: QSS-driven content
        // must be polished before its first paint.
        pimpl->loadControl->ensurePolished();
        const_cast<ChatMessageImageItem*>(this)->repositionOverlays();
        pimpl->loadControl->show();
    }
    return pimpl->loadControl;
}

//--------------------------------------------------------------------------

IconTextButton* ChatMessageImageItem::ensureMenuButton() const
{
    if (pimpl->menuButton==nullptr)
    {
        auto self=const_cast<ChatMessageImageItem*>(this);

        // floats over the tile's top-right corner, positioned by repositionOverlays() -- not
        // added to any layout of `this` (this widget has none)
        pimpl->menuButton=new IconTextButton(
            menuIcon(QStringLiteral("menu"),self),
            self,
            IconTextButton::IconPosition::BeforeText
        );
        pimpl->menuButton->setObjectName("menuButton");
        pimpl->menuButton->setText(QString());
        pimpl->menuButton->setCursor(Qt::PointingHandCursor);

        // DropdownMenu is constructed parentless, like FileUploadListItem's own per-item menu --
        // see that class's constructor for why (DropdownFrame reparents itself lazily to the
        // trigger's actual window() on first opening)
        pimpl->menu=new DropdownMenu();

        // Menu items are only ever built the moment the drop-down is actually about to open --
        // see rebuildMenu()'s doc comment -- via the trigger button's own clicked() rather than
        // DropdownFrame::aboutToShow(): DropdownFrame::popupBelow()/popupAt() already run
        // fillContent()+measure() BEFORE beginOpen() emits aboutToShow() (despite that signal's
        // "right before content is filled and measured" doc comment), so rebuilding on
        // aboutToShow() is one step too late and measures an empty, tiny popup. Connected here,
        // BEFORE menu->attachTo() below wires its own clicked handler that actually opens the
        // dropdown, so this slot runs first -- Qt invokes same-signal slots in connection order.
        connect(pimpl->menuButton,&IconTextButton::clicked,self,
            [self]()
            {
                if (self->pimpl->menuDirty)
                {
                    self->pimpl->menu->setItems(buildChatFileMenuItems(self->pimpl->item,true,self->pimpl->incoming,self));
                    self->pimpl->menuDirty=false;
                }
            }
        );

        pimpl->menu->attachTo(pimpl->menuButton);
        connect(pimpl->menu,&DropdownMenu::itemTriggered,self,&ChatMessageImageItem::onMenuItemTriggered);
        // the dropdown is a separate top-level popup, not a child of this tile -- moving the
        // mouse onto it while it is open fires this tile's leaveEvent, so
        // updateMenuButtonVisibility() must re-run once it closes too, to hide the button again
        // if the mouse never came back
        connect(pimpl->menu,&DropdownMenu::hidden,self,[self](){ self->updateMenuButtonVisibility(); });

        // See rebuildGrid()'s identical comment on freshly created tiles: QSS-driven content
        // (this button's 18px icon size) must be polished before its first paint, and before
        // repositionOverlays() reads its sizeHint().
        pimpl->menuButton->ensurePolished();
        self->repositionOverlays();
        pimpl->menuButton->show();
    }
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
