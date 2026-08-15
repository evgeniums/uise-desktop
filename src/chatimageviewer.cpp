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

/** @file uise/desktop/chatimageviewer.cpp
*
*  Defines ChatImageViewer.
*
*/

/****************************************************************************/

#include <map>
#include <algorithm>

#include <uise/desktop/chatimageviewercontrols.hpp>
#include <uise/desktop/chatimageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ChatImageViewer_p
{
    public:

        struct ChatMeta
        {
            QString sender;
            QDateTime dateTime;
            QString messageId;
            PixmapKey previewKey;   //!< Valid only when ChatImage::previewKey was set explicitly.
        };

        ChatImageViewerControls* controls=nullptr;

        std::map<PixmapKey,ChatMeta> meta;

        ChatImageViewer::StripScope stripScope=ChatImageViewer::StripScope::Album;
        size_t stripRadius=ChatImageViewer::DefaultStripRadius;
        bool counterVisible=true;

        //! Split a caller-supplied ChatImage list into the base's own Image list plus this
        //! class's metadata, and record the metadata under each image's key -- shared by every
        //! load/insert entry point below.
        std::vector<AbstractImageViewer::Image> splitAndRecordMeta(std::vector<ChatImageViewer::ChatImage>& images)
        {
            std::vector<AbstractImageViewer::Image> baseImages;
            baseImages.reserve(images.size());
            for (auto& img : images)
            {
                baseImages.emplace_back(img.key,img.content,img.animation);

                ChatMeta m;
                m.sender=std::move(img.sender);
                m.dateTime=std::move(img.dateTime);
                m.messageId=std::move(img.messageId);
                m.previewKey=std::move(img.previewKey);
                meta[img.key]=std::move(m);
            }
            return baseImages;
        }
};

//--------------------------------------------------------------------------

ChatImageViewer::ChatImageViewer(QObject* parent)
    : ImageViewer(parent),
      pimpl(std::make_unique<ChatImageViewer_p>())
{
    // Overlay is this class's whole point (a ChatImageViewer without it is just an ImageViewer),
    // so default to it here rather than forcing it unconditionally in doCreateActualWidget() --
    // m_widget is still null at this point so this only records the mode (see
    // ImageViewer::setControlsMode()'s own null-guard), letting a caller's own
    // setControlsMode(Static) made before the widget exists stick instead of being silently
    // overridden once the widget is created.
    setControlsMode(ControlsMode::Overlay);
}

//--------------------------------------------------------------------------

ChatImageViewer::~ChatImageViewer()
{}

//--------------------------------------------------------------------------

Widget* ChatImageViewer::doCreateActualWidget(QWidget* parent)
{
    auto* w=ImageViewer::doCreateActualWidget(parent);

    // controlsMode() is already Overlay by now -- either the constructor's own default, or a
    // caller's explicit setControlsMode() made before the widget existed -- and
    // ImageViewer::doCreateActualWidget() above already applied it to m_widget, so
    // setBottomWidget() below reparents straight into the overlay position with no transient
    // Static-mode detour.
    pimpl->controls=new ChatImageViewerControls(m_widget);
    setBottomWidget(pimpl->controls);

    // Picks up a source set via setImageSource() before the widget existed -- setImageSource()'s
    // own override only reaches controls() when it is non-null, so replay it here too.
    if (imageSource())
    {
        pimpl->controls->setPreviewSource(imageSource());
    }

    // Same replay rationale for a setCounterVisible(false) made before the widget existed.
    pimpl->controls->setCounterVisible(pimpl->counterVisible);

    connect(
        pimpl->controls,
        &ChatImageViewerControls::rotateRequested,
        this,
        &AbstractImageViewer::rotateCounterclockwise
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::zoomInRequested,
        this,
        &AbstractImageViewer::zoomIn
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::zoomOutRequested,
        this,
        &AbstractImageViewer::zoomOut
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::playPauseRequested,
        this,
        &ImageViewer::togglePlay
    );
    connect(
        this,
        &ImageViewer::currentImageAnimationStateChanged,
        this,
        &ChatImageViewer::onAnimationStateChanged
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::previewClickedKey,
        this,
        [this](const PixmapKey& key)
        {
            selectImage(key);
        }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::saveAsRequested,
        this,
        [this](){ emit saveAsRequested(currentImageKey()); }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::copyRequested,
        this,
        [this](){ emit copyRequested(currentImageKey()); }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::forwardRequested,
        this,
        [this](){ emit forwardRequested(currentImageKey()); }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::goToMessageRequested,
        this,
        [this]()
        {
            auto it=pimpl->meta.find(currentImageKey());
            emit goToMessageRequested(it!=pimpl->meta.end() ? it->second.messageId : QString());
        }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::deleteMessageRequested,
        this,
        [this]()
        {
            auto it=pimpl->meta.find(currentImageKey());
            emit deleteMessageRequested(it!=pimpl->meta.end() ? it->second.messageId : QString());
        }
    );

    connect(
        this,
        &AbstractImageViewer::currentImageIndexChanged,
        this,
        &ChatImageViewer::onCurrentImageIndexChanged
    );
    // Catches everything a pure selection change misses: an album/neighbourhood completing at a
    // window edge once a fetch reply lands, hasMoreBefore()/hasMoreAfter() flipping, an
    // updateChatImage()/removeImagesForMessage() call, etc. -- see updateControls()'s own doc.
    connect(
        this,
        &AbstractImageViewer::windowChanged,
        this,
        &ChatImageViewer::onWindowChangedSlot
    );

    updateControls();
    onAnimationStateChanged();

    return w;
}

//--------------------------------------------------------------------------

void ChatImageViewer::loadChatImages(std::vector<ChatImage> images)
{
    loadChatImages(std::move(images),false,false,0,-1);
}

//--------------------------------------------------------------------------

void ChatImageViewer::loadChatImages(
    std::vector<ChatImage> images,
    bool hasMoreBefore,
    bool hasMoreAfter,
    qint64 firstPosition,
    qint64 totalCount)
{
    pimpl->meta.clear();
    auto baseImages=pimpl->splitAndRecordMeta(images);
    loadImages(std::move(baseImages),hasMoreBefore,hasMoreAfter,firstPosition,totalCount);
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::insertFetchedChatImages(std::vector<ChatImage> images, Direction direction, size_t requestedCount)
{
    auto baseImages=pimpl->splitAndRecordMeta(images);
    insertFetchedImages(std::move(baseImages),direction,requestedCount);
    updateControls();
}

//--------------------------------------------------------------------------

bool ChatImageViewer::updateChatImage(const ChatImage& image)
{
    auto it=pimpl->meta.find(image.key);
    if (it==pimpl->meta.end())
    {
        return false;
    }

    it->second.sender=image.sender;
    it->second.dateTime=image.dateTime;
    it->second.messageId=image.messageId;
    it->second.previewKey=image.previewKey;

    bool ok=updateImage(Image{image.key,image.content,image.animation});
    updateControls();
    return ok;
}

//--------------------------------------------------------------------------

size_t ChatImageViewer::removeImagesForMessage(const QString& messageId)
{
    std::vector<PixmapKey> toRemove;
    for (const auto& [key,m] : pimpl->meta)
    {
        if (m.messageId==messageId)
        {
            toRemove.push_back(key);
        }
    }

    size_t removed=0;
    for (const auto& key : toRemove)
    {
        if (removeImage(key))
        {
            ++removed;
        }
    }
    return removed;
}

//--------------------------------------------------------------------------

ChatImageViewerControls* ChatImageViewer::controls() const
{
    return pimpl->controls;
}

//--------------------------------------------------------------------------

void ChatImageViewer::setImageNumbering(size_t firstImageNumber, size_t totalCount)
{
    // firstImageNumber is 1-based over the whole chat; windowFirstPosition() is 0-based.
    setWindowFirstPosition(static_cast<qint64>(firstImageNumber)-1);
    setTotalCountHint(static_cast<qint64>(totalCount));
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::resetImageNumbering()
{
    setTotalCountHint(-1);
    if (!hasMoreBefore() && !hasMoreAfter())
    {
        setWindowFirstPosition(0);
    }
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::setStripScope(StripScope scope)
{
    if (pimpl->stripScope==scope)
    {
        return;
    }
    pimpl->stripScope=scope;
    updateControls();
}

//--------------------------------------------------------------------------

ChatImageViewer::StripScope ChatImageViewer::stripScope() const noexcept
{
    return pimpl->stripScope;
}

//--------------------------------------------------------------------------

void ChatImageViewer::setStripRadius(size_t n)
{
    if (pimpl->stripRadius==n)
    {
        return;
    }
    pimpl->stripRadius=n;
    if (pimpl->stripScope==StripScope::Continuous)
    {
        updateControls();
    }
}

//--------------------------------------------------------------------------

size_t ChatImageViewer::stripRadius() const noexcept
{
    return pimpl->stripRadius;
}

//--------------------------------------------------------------------------

void ChatImageViewer::setCounterVisible(bool visible)
{
    pimpl->counterVisible=visible;
    if (pimpl->controls!=nullptr)
    {
        pimpl->controls->setCounterVisible(visible);
    }
}

//--------------------------------------------------------------------------

bool ChatImageViewer::isCounterVisible() const
{
    return pimpl->counterVisible;
}

//--------------------------------------------------------------------------

void ChatImageViewer::setImageSource(std::shared_ptr<PixmapSource> imageSource)
{
    AbstractImageViewer::setImageSource(imageSource);
    if (pimpl->controls!=nullptr)
    {
        pimpl->controls->setPreviewSource(imageSource);
        // Preview::content precedence follows the base's own image content (see makePreview()):
        // with a source now available, previews should stop relying on any seed content they
        // were built with and resolve live instead -- force a rebuild rather than relying on
        // setPreviewSource() alone, which only re-wires previews that never had ANY source before
        // (see ImagePreviewStrip::setImageSource()'s own doc on what it does and does not do).
        updateControls();
    }
}

//--------------------------------------------------------------------------

void ChatImageViewer::onCurrentImageIndexChanged(size_t index)
{
    std::ignore=index;
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::onWindowChangedSlot()
{
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::onAnimationStateChanged()
{
    if (pimpl->controls==nullptr)
    {
        return;
    }
    pimpl->controls->setPlayPauseVisible(isCurrentImageAnimated());
    pimpl->controls->setPlaying(isCurrentImagePlaying());
}

//--------------------------------------------------------------------------

void ChatImageViewer::onImageEvicted(const PixmapKey& key)
{
    pimpl->meta.erase(key);
}

//--------------------------------------------------------------------------

void ChatImageViewer::onImageRemoved(const PixmapKey& key)
{
    pimpl->meta.erase(key);
}

//--------------------------------------------------------------------------

PixmapKey ChatImageViewer::previewKeyFor(const PixmapKey& imageKey) const
{
    PixmapKey pk;
    pk.setPath(imageKey.toWithPath());
    pk.setData(imageKey.data());

    auto size=(pimpl->controls!=nullptr)
        ? pimpl->controls->previewStrip()->itemSize()
        : QSize{ImagePreviewStrip::DefaultItemSize,ImagePreviewStrip::DefaultItemSize};
    pk.setSize(size);
    pk.setAnySize(false);

    return pk;
}

//--------------------------------------------------------------------------

ImagePreviewStrip::Preview ChatImageViewer::makePreview(size_t index) const
{
    auto key=imageKey(index);

    auto it=pimpl->meta.find(key);
    PixmapKey previewKey;
    if (it!=pimpl->meta.end() && it->second.previewKey.isValid())
    {
        previewKey=it->second.previewKey;
    }
    else
    {
        previewKey=previewKeyFor(key);
    }

    // With a source configured, always pass null content so the strip resolves (and can upgrade)
    // the preview live through that source -- passing a seed here would recreate the same
    // content-shadows-the-producer trap D4 fixes on the base viewer, one level down (see
    // ImagePreviewStrip::applyItemContent(), which returns immediately for non-null content).
    // Only fall back to whatever seed pixmap the base holds for this image when there is no
    // source at all -- the same situation Image::content exists for in the first place, which is
    // exactly what keeps the source-less chatimageviewer-demo/chatimageviewerwindow-demo working.
    QPixmap seed=imageSource() ? QPixmap{} : imagePixmap(index);
    AnimationContent animSeed=imageSource() ? AnimationContent{} : imageAnimation(index);

    return ImagePreviewStrip::Preview{previewKey,seed,animSeed};
}

//--------------------------------------------------------------------------

void ChatImageViewer::updateControls()
{
    if (pimpl->controls==nullptr)
    {
        return;
    }

    auto idx=currentImageIndex();

    if (pimpl->counterVisible)
    {
        auto number=static_cast<size_t>(windowFirstPosition()+static_cast<qint64>(idx))+1;
        auto total=totalCountHint()>=0 ? static_cast<size_t>(totalCountHint()) : imageCount();
        pimpl->controls->setCounter(number,total);
    }

    auto key=currentImageKey();
    auto metaIt=pimpl->meta.find(key);
    if (metaIt==pimpl->meta.end())
    {
        pimpl->controls->setSender(QString());
        pimpl->controls->setDateTime(QDateTime());
        pimpl->controls->setPreviews({});
        refreshOverlayGeometry();
        return;
    }

    pimpl->controls->setSender(metaIt->second.sender);
    pimpl->controls->setDateTime(metaIt->second.dateTime);

    auto count=imageCount();
    std::vector<ImagePreviewStrip::Preview> previews;
    int currentPos=0;

    if (pimpl->stripScope==StripScope::Continuous)
    {
        auto radius=pimpl->stripRadius;
        auto lo=(idx>=radius) ? (idx-radius) : size_t{0};
        auto hi=(count==0) ? size_t{0} : std::min(count-1,idx+radius);
        for (auto i=lo; count>0 && i<=hi; ++i)
        {
            if (i==idx)
            {
                currentPos=static_cast<int>(previews.size());
            }
            previews.push_back(makePreview(i));
        }
    }
    else
    {
        // StripScope::Album -- the contiguous run sharing the current image's messageId. Runs
        // only over the loaded window: if it reaches either edge while that end's hasMoreBefore()/
        // hasMoreAfter() is still true, the album may be truncated here and will simply be
        // recomputed (via the windowChanged() connection) once AbstractImageViewer's own
        // prefetching -- driven by how close currentImageIndex() is to that edge, see
        // setPrefetchThreshold() -- extends the window far enough to complete it. Never blocks.
        const auto& messageId=metaIt->second.messageId;
        auto start=idx;
        auto end=idx;
        if (!messageId.isEmpty())
        {
            while (start>0)
            {
                auto it=pimpl->meta.find(imageKey(start-1));
                if (it==pimpl->meta.end() || it->second.messageId!=messageId)
                {
                    break;
                }
                --start;
            }
            while ((end+1)<count)
            {
                auto it=pimpl->meta.find(imageKey(end+1));
                if (it==pimpl->meta.end() || it->second.messageId!=messageId)
                {
                    break;
                }
                ++end;
            }
        }

        for (auto i=start; i<=end; ++i)
        {
            if (i==idx)
            {
                currentPos=static_cast<int>(previews.size());
            }
            previews.push_back(makePreview(i));
        }
    }

    // ImagePreviewStrip::setPreviews() diffs internally (see its own class doc) -- reusing
    // widgets/consumers for keys already shown and animating rather than jumping when the
    // previously-current key survives -- so calling it unconditionally on every updateControls()
    // is cheap and correct, unlike the old index-based "did the range change" fast path this
    // replaces (which broke across a window prepend/eviction shifting every index).
    pimpl->controls->setPreviews(std::move(previews),currentPos);

    // setPreviews() above can change the album strip's, and hence the whole bottom widget's,
    // sizeHint (e.g. an album appearing/disappearing) -- force the overlay geometry to catch up
    // immediately rather than waiting for the queued QEvent::LayoutRequest that
    // ImageViewerWidget's own eventFilter() reacts to on the next event loop iteration.
    refreshOverlayGeometry();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
