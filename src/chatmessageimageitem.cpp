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

        //! What the still preview currently on `preview` was rendered from -- see updatePreview()'s
        //! still path. Only meaningful while stillValid; cleared whenever another path takes over.
        bool stillValid=false;
        qint64 stillCacheKey=0;
        QSize stillPhysicalSize;
        qreal stillDpr=0.0;
        qreal stillMaxUpscale=0.0;
        QSize stillPixelSize;
        bool stillPlaceholder=false;
        bool stillCover=false;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;

        //! See ChatMessageImageItem::setMaxUpscale()'s own doc comment. Matches
        //! ChatMessageImages_p::tileMaxUpscale's own default, restated here as a plain default
        //! rather than shared via a header the way DefaultMaxWidth/DefaultMinTileSize are not
        //! either -- this tile has no dependency on albumlayout.hpp at all, only on whatever
        //! concrete value its owner pushes through setMaxUpscale().
        qreal maxUpscale=2.0;

        //! See ChatMessageImageItem::setCoverContent().
        bool coverContent=false;

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

void ChatMessageImageItem::setCoverContent(bool enable)
{
    if (pimpl->coverContent==enable)
    {
        return;
    }
    pimpl->coverContent=enable;
    updatePreview();
}

//--------------------------------------------------------------------------

bool ChatMessageImageItem::coverContent() const noexcept
{
    return pimpl->coverContent;
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

void ChatMessageImageItem::startDrag(const QList<QUrl>& urls, const QString& sourceTag)
{
    // A placeholder is a stand-in for content that hasn't resolved yet -- not worth using as a
    // drag pixmap; Qt's own default drag cursor is a better signal than a blurry placeholder.
    QPixmap preview;
    if (!pimpl->item.isPreviewPlaceholder() && !pimpl->item.preview().isNull())
    {
        preview=scaledToFit(QPixmap::fromImage(pimpl->item.preview()),QSize(160,160));
    }
    startFileUrlDrag(this,urls,preview,sourceTag);
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
            pimpl->stillValid=false;
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
        pimpl->stillValid=false;
    }

    auto preview=pimpl->item.preview();
    if (!preview.isNull())
    {
        const qreal dpr=devicePixelRatioF();
        QSize physicalSize(qRound(size().width()*dpr),qRound(size().height()*dpr));

        // Already showing exactly this render? Then keep it. Everything the still render below
        // depends on is in this key: the source image (QImage::cacheKey() is shared by the
        // implicitly shared copies item().preview() hands out, and changes with its data), the
        // tile's physical box, the upscale allowance, the original's natural size and the
        // placeholder flag (which together decide the crop-vs-fit framing).
        //
        // Without this every refresh() re-converted, smooth-scaled and re-composed the preview --
        // and ChatMessageImages::rebuildGrid() refreshes every tile on every bubble-width pass,
        // twice per pass (bubbleWidthHint() and updateMaximumBubbleWidth()), even when its
        // layoutUnchanged memo keeps the geometry. Every chat-view resize, including the
        // height-only one of the composer's formatting-mode toggle, re-rendered every image
        // preview on screen at full HiDPI resolution: ~27% of GUI-thread CPU in a Windows profile.
        if (pimpl->stillValid
            && pimpl->stillCacheKey==preview.cacheKey()
            && pimpl->stillPhysicalSize==physicalSize
            && pimpl->stillDpr==dpr
            && pimpl->stillMaxUpscale==pimpl->maxUpscale
            && pimpl->stillPixelSize==pimpl->item.pixelSize()
            && pimpl->stillPlaceholder==pimpl->item.isPreviewPlaceholder()
            && pimpl->stillCover==pimpl->coverContent)
        {
            setPlaceholderMode(false);
            return;
        }

        pimpl->preview->setSvgIcon(nullptr);

        // albumLayout() hands this tile exactly its image's aspect ratio, to within one logical
        // pixel per dimension (integer rounding of the tile's edges). So the normal case is to
        // COVER the tile -- scale the rung to the tile's physical box and centre-crop the
        // sub-pixel residual -- which is what keeps a multi-image album visually gapless: a
        // fit-and-pad would leave a hairline of transparent canvas along one edge of every tile
        // whose rounding went the other way. The crop is at most that one logical pixel for real
        // content, never a visible part of the image. Resolution does not enter into it: a small
        // original is upscaled to fill the tile the layout gave it (see albumLayout()'s own doc
        // comment for why a smaller tile would break the packing, and for the uniform shrink that
        // keeps an ALL-thumbnail album small), and the thumbnail-to-rung swap is pixel-stable by
        // construction because both states cover the same box.
        //
        // Scale to PHYSICAL pixels and tag the result with the screen's devicePixelRatio -- the
        // brush fill DOES honor the tag (see FileUploadListItem::updatePreviews() and
        // pixmapscale.hpp's own doc comments for the same rule). Without both halves --
        // physical-size canvas AND the tag -- the tile rasterises at 1x and reads as soft/blurry
        // on any HiDPI/Retina display.
        //
        // sameAspect() against the TILE is checked rather than assumed, because two framings can
        // still genuinely disagree with it and keep the older paths:
        //  - a PLACEHOLDER preview whose own framing disagrees with the original's -- a legacy
        //    square centre-crop of a non-square original (an already-sent message's pre-change
        //    thumbnail, or one supplied by a mobile client; see todo-aspect-preserving-embedded-
        //    thumbnails.md) -- on a tile that does not match the original either (stale geometry
        //    between two layouts): crop it to the content box the ORIGINAL's aspect would occupy
        //    (fittedContentSize(), from item.pixelSize() alone) and pad, so it does not
        //    misrepresent the image. On a tile that DOES match the original it simply covers the
        //    tile like everything else, which centre-crops it to the original's shape;
        //  - an item with no known pixel size at all: fit inside and pad, bounded by
        //    maxUpscale(), never crop -- there is no aspect to trust.
        // Tolerance 0.06 rather than sameAspect()'s 0.04 default so that one pixel of rounding
        // on a tile near albumLayout()'s 60px soft floor still counts as agreeing.
        //
        // coverContent() (the PresetTemplates layout mode) short-circuits all of this: that
        // layout sizes cells from clamped ratios and the centre-crop IS its design, so the tile
        // always covers, whatever the image's ratio and whether or not it is known.
        const auto& pixelSize=pimpl->item.pixelSize();
        const bool haveNaturalSize=pixelSize.isValid() && !pixelSize.isEmpty();
        const bool coverTile=pimpl->coverContent || (haveNaturalSize && sameAspect(pixelSize,size(),0.06));
        const bool cropFraming=pimpl->item.isPreviewPlaceholder() && !sameAspect(preview.size(),pixelSize);

        auto srcPx=QPixmap::fromImage(preview);
        QPixmap px;
        if (coverTile)
        {
            px=scaledAndCropped(srcPx,physicalSize);
        }
        else if (cropFraming)
        {
            auto contentBox=fittedContentSize(pixelSize,physicalSize,pimpl->maxUpscale);
            px=composePadded(scaledAndCropped(srcPx,contentBox),physicalSize);
        }
        else
        {
            px=scaledToFitPadded(srcPx,physicalSize,pixelSize,pimpl->maxUpscale);
        }
        px.setDevicePixelRatio(dpr);
        pimpl->preview->setPixmap(px);

        pimpl->stillValid=true;
        pimpl->stillCacheKey=preview.cacheKey();
        pimpl->stillPhysicalSize=physicalSize;
        pimpl->stillDpr=dpr;
        pimpl->stillMaxUpscale=pimpl->maxUpscale;
        pimpl->stillPixelSize=pimpl->item.pixelSize();
        pimpl->stillPlaceholder=pimpl->item.isPreviewPlaceholder();
        pimpl->stillCover=pimpl->coverContent;

        setPlaceholderMode(false);
    }
    else
    {
        pimpl->stillValid=false;
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
        // todo-load-control-overflows-small-image-tiles.md: LoadControlSize is a fixed constant
        // (its own comment explains why -- LoadControl's stylesheet min/max-width/height is not
        // necessarily what an unmeasured sizeHint() would report), so it does not shrink with a
        // genuinely small tile on its own. albumLayout()'s cost function defends a 60px soft
        // floor on every tile's short side (AlbumLayoutOptions::minTile) and the all-thumbnail
        // shrink stops at qproperty-minTileSize, so ordinary album tiles are comfortably larger
        // than this control -- but neither is a hard guarantee (a budget too small for the image
        // count, an extreme aspect ratio, or a theme setting minTileSize below 56 can all still
        // produce one). boundedTo() is the unconditional backstop: a no-op on the common
        // comfortably-large tile, and never lets the control overhang its own tile's edge on a
        // tiny one.
        auto controlSize=LoadControlSize.boundedTo(size());
        pimpl->loadControl->setGeometry(
            (width()-controlSize.width())/2,
            (height()-controlSize.height())/2,
            controlSize.width(),
            controlSize.height()
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
