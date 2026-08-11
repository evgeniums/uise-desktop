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

#include <limits>

#include <uise/desktop/chatimageviewercontrols.hpp>
#include <uise/desktop/chatimageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

constexpr size_t InvalidAlbumIndex=std::numeric_limits<size_t>::max();

} // anonymous namespace

//--------------------------------------------------------------------------

class ChatImageViewer_p
{
    public:

        ChatImageViewerControls* controls=nullptr;

        std::vector<ChatImageViewer::ChatImage> metadata;

        size_t albumStart=InvalidAlbumIndex;
        size_t albumEnd=InvalidAlbumIndex;

        bool numberingSet=false;
        size_t firstImageNumber=1;
        size_t totalCount=0;
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
        &ChatImageViewerControls::previewClicked,
        this,
        [this](int index)
        {
            if (index<0 || pimpl->albumStart==InvalidAlbumIndex)
            {
                return;
            }
            selectImage(pimpl->albumStart+static_cast<size_t>(index));
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
            auto idx=currentImageIndex();
            auto messageId=idx<pimpl->metadata.size() ? pimpl->metadata[idx].messageId : QString();
            emit goToMessageRequested(messageId);
        }
    );
    connect(
        pimpl->controls,
        &ChatImageViewerControls::deleteMessageRequested,
        this,
        [this]()
        {
            auto idx=currentImageIndex();
            auto messageId=idx<pimpl->metadata.size() ? pimpl->metadata[idx].messageId : QString();
            emit deleteMessageRequested(messageId);
        }
    );

    connect(
        this,
        &AbstractImageViewer::currentImageIndexChanged,
        this,
        &ChatImageViewer::onCurrentImageIndexChanged
    );

    updateControls();

    return w;
}

//--------------------------------------------------------------------------

void ChatImageViewer::loadChatImages(std::vector<ChatImage> images)
{
    pimpl->metadata.clear();
    pimpl->albumStart=InvalidAlbumIndex;
    pimpl->albumEnd=InvalidAlbumIndex;

    std::vector<Image> baseImages;
    baseImages.reserve(images.size());
    pimpl->metadata.reserve(images.size());
    for (auto& img : images)
    {
        baseImages.push_back(Image{img.key,img.content});
        pimpl->metadata.push_back(std::move(img));
    }

    loadImages(std::move(baseImages));
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::insertChatImages(size_t index, std::vector<ChatImage> images)
{
    std::vector<Image> baseImages;
    baseImages.reserve(images.size());
    std::vector<ChatImage> metaToInsert;
    metaToInsert.reserve(images.size());
    for (auto& img : images)
    {
        baseImages.push_back(Image{img.key,img.content});
        metaToInsert.push_back(std::move(img));
    }

    auto boundedIndex=qMin(index,pimpl->metadata.size());
    auto pos=pimpl->metadata.begin()+static_cast<std::vector<ChatImage>::difference_type>(boundedIndex);
    pimpl->metadata.insert(pos,std::make_move_iterator(metaToInsert.begin()),std::make_move_iterator(metaToInsert.end()));

    insertImages(boundedIndex,std::move(baseImages));

    // An insertion shifts every index at/after boundedIndex -- the album window cached by the
    // last updateControls() call may now point at the wrong images, so force a full recompute
    // rather than risk showing a stale album range.
    pimpl->albumStart=InvalidAlbumIndex;
    pimpl->albumEnd=InvalidAlbumIndex;
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::appendChatImages(std::vector<ChatImage> images)
{
    insertChatImages(pimpl->metadata.size(),std::move(images));
}

//--------------------------------------------------------------------------

void ChatImageViewer::prependChatImages(std::vector<ChatImage> images)
{
    insertChatImages(0,std::move(images));
}

//--------------------------------------------------------------------------

ChatImageViewerControls* ChatImageViewer::controls() const
{
    return pimpl->controls;
}

//--------------------------------------------------------------------------

void ChatImageViewer::setImageNumbering(size_t firstImageNumber, size_t totalCount)
{
    pimpl->numberingSet=true;
    pimpl->firstImageNumber=firstImageNumber;
    pimpl->totalCount=totalCount;
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::resetImageNumbering()
{
    pimpl->numberingSet=false;
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::setImageSource(std::shared_ptr<PixmapSource> imageSource)
{
    AbstractImageViewer::setImageSource(imageSource);
    if (pimpl->controls!=nullptr)
    {
        pimpl->controls->setPreviewSource(imageSource);
    }
}

//--------------------------------------------------------------------------

void ChatImageViewer::onCurrentImageIndexChanged(size_t index)
{
    std::ignore=index;
    updateControls();
}

//--------------------------------------------------------------------------

void ChatImageViewer::updateControls()
{
    if (pimpl->controls==nullptr)
    {
        return;
    }

    auto idx=currentImageIndex();
    auto loadedCount=imageCount();

    auto number=pimpl->numberingSet ? (pimpl->firstImageNumber+idx) : (idx+1);
    auto total=pimpl->numberingSet ? pimpl->totalCount : loadedCount;
    pimpl->controls->setCounter(number,total);

    if (idx>=pimpl->metadata.size())
    {
        pimpl->controls->setSender(QString());
        pimpl->controls->setDateTime(QDateTime());
        pimpl->albumStart=idx;
        pimpl->albumEnd=idx;
        pimpl->controls->setPreviews({});
        refreshOverlayGeometry();
        return;
    }

    const auto& meta=pimpl->metadata[idx];
    pimpl->controls->setSender(meta.sender);
    pimpl->controls->setDateTime(meta.dateTime);

    auto start=idx;
    auto end=idx;
    if (!meta.messageId.isEmpty())
    {
        while (start>0 && pimpl->metadata[start-1].messageId==meta.messageId)
        {
            --start;
        }
        while ((end+1)<pimpl->metadata.size() && pimpl->metadata[end+1].messageId==meta.messageId)
        {
            ++end;
        }
    }

    if (start!=pimpl->albumStart || end!=pimpl->albumEnd)
    {
        pimpl->albumStart=start;
        pimpl->albumEnd=end;

        std::vector<ImagePreviewStrip::Preview> previews;
        previews.reserve(end-start+1);
        for (auto i=start;i<=end;++i)
        {
            previews.push_back(ImagePreviewStrip::Preview{imageKey(i),pimpl->metadata[i].content});
        }
        pimpl->controls->setPreviews(std::move(previews),static_cast<int>(idx-start));
    }
    else
    {
        pimpl->controls->setCurrentPreview(static_cast<int>(idx-start));
    }

    // setPreviews() above can change the album strip's, and hence the whole bottom widget's,
    // sizeHint (e.g. an album appearing/disappearing) -- force the overlay geometry to catch up
    // immediately rather than waiting for the queued QEvent::LayoutRequest that
    // ImageViewerWidget's own eventFilter() reacts to on the next event loop iteration.
    refreshOverlayGeometry();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
