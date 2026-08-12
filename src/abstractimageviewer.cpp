/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractimageditor.cpp
*
*  Defines AbstractImageViewer.
*
*/

/****************************************************************************/

#include <deque>
#include <map>
#include <algorithm>

#include <QString>

#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/abstractimageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class AbstractImageViewer_p
{
    public:

        struct Entry
        {
            PixmapKey key;
            QPixmap content;
            PixmapConsumer consumer;
            bool wired=false;
            bool active=false;

            Entry(PixmapKey k) : key(k), content(), consumer(std::move(k))
            {}
        };

        enum class PendingNav : uint8_t
        {
            None,
            Next,
            Prev
        };

        std::deque<std::shared_ptr<Entry>> window;
        std::map<PixmapKey,Entry*> byKey;

        PixmapKey currentKey;
        size_t currentIndex=0;

        qint64 windowFirstPosition=0;
        qint64 totalCountHint=-1;

        bool hasMoreBefore=false;
        bool hasMoreAfter=false;

        bool requestInFlightHome=false;
        bool requestInFlightEnd=false;
        bool inMaybeRequest=false;

        PendingNav pendingNav=PendingNav::None;
        SingleShotTimer* pendingNavTimer=nullptr;

        std::shared_ptr<PixmapSource> imageSource;

        size_t windowSize=AbstractImageViewer::DefaultWindowSize;
        size_t fetchCount=AbstractImageViewer::DefaultFetchCount;
        size_t prefetchThreshold=AbstractImageViewer::DefaultPrefetchThreshold;
        size_t activeWindowRadius=AbstractImageViewer::DefaultActiveWindowRadius;
        size_t pendingNavTimeoutMs=AbstractImageViewer::DefaultPendingNavTimeoutMs;

        Entry* find(const PixmapKey& key) const
        {
            auto it=byKey.find(key);
            return it!=byKey.end() ? it->second : nullptr;
        }
};

//--------------------------------------------------------------------------

AbstractImageViewer::AbstractImageViewer(QObject* parent)
    : WidgetController(parent),
      pimpl(std::make_unique<AbstractImageViewer_p>())
{
    pimpl->pendingNavTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

AbstractImageViewer::~AbstractImageViewer()
{}

//--------------------------------------------------------------------------

void AbstractImageViewer::wireEntry(const PixmapKey& key)
{
    auto* entry=pimpl->find(key);
    if (entry==nullptr || entry->wired)
    {
        return;
    }
    entry->wired=true;

    connect(
        &entry->consumer,
        &PixmapConsumer::pixmapUpdated,
        this,
        [this,key]()
        {
            onPixmapUpdated(key);
        }
    );
    connect(
        &entry->consumer,
        &PixmapConsumer::loadingChanged,
        this,
        [this,key](bool loading)
        {
            onPixmapLoadingChanged(key,loading);
            if (key==currentImageKey())
            {
                emit currentImageLoadingChanged(loading);
            }
        }
    );
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::mergeImages(std::vector<Image>& images, Direction direction)
{
    std::vector<std::shared_ptr<AbstractImageViewer_p::Entry>> newEntries;
    newEntries.reserve(images.size());

    for (auto& image : images)
    {
        auto it=pimpl->byKey.find(image.key);
        if (it!=pimpl->byKey.end())
        {
            // Already windowed -- an insert of a key we already hold is an in-place seed update,
            // never a duplicate (see updateImage(), which this mirrors).
            it->second->content=image.content;
            continue;
        }

        auto entry=std::make_shared<AbstractImageViewer_p::Entry>(image.key);
        entry->content=image.content;
        pimpl->byKey[entry->key]=entry.get();
        wireEntry(entry->key);
        newEntries.push_back(std::move(entry));
    }

    if (direction==Direction::HOME)
    {
        // Insert as one ordered block at the front: images arrive in display order, so pushing
        // them front-wise in REVERSE lands the block in its original order ahead of whatever was
        // already there.
        for (auto rit=newEntries.rbegin(); rit!=newEntries.rend(); ++rit)
        {
            pimpl->window.push_front(*rit);
        }
    }
    else
    {
        for (auto& entry : newEntries)
        {
            pimpl->window.push_back(entry);
        }
    }

    return newEntries.size();
}

//--------------------------------------------------------------------------

PixmapKey AbstractImageViewer::edgeAnchor(Direction direction) const
{
    if (pimpl->window.empty())
    {
        return PixmapKey{};
    }
    return (direction==Direction::HOME) ? pimpl->window.front()->key : pimpl->window.back()->key;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::reindexCurrent()
{
    if (pimpl->window.empty())
    {
        pimpl->currentIndex=0;
        pimpl->currentKey=PixmapKey{};
        return;
    }

    auto* target=pimpl->find(pimpl->currentKey);
    if (target!=nullptr)
    {
        for (size_t i=0; i<pimpl->window.size(); ++i)
        {
            if (pimpl->window[i].get()==target)
            {
                pimpl->currentIndex=i;
                return;
            }
        }
    }

    // currentKey is no longer windowed (removed, or never set) -- clamp to the nearest valid
    // index rather than losing position entirely.
    if (pimpl->currentIndex>=pimpl->window.size())
    {
        pimpl->currentIndex=pimpl->window.size()-1;
    }
    pimpl->currentKey=pimpl->window[pimpl->currentIndex]->key;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::refreshActiveProducers()
{
    if (pimpl->window.empty())
    {
        return;
    }

    auto count=pimpl->window.size();
    auto current=std::min(pimpl->currentIndex,count-1);
    auto radius=pimpl->activeWindowRadius;

    auto lo=(current>=radius) ? (current-radius) : size_t{0};
    auto hi=std::min(count-1,current+radius);

    for (size_t i=0; i<count; ++i)
    {
        auto& entry=*pimpl->window[i];
        bool shouldBeActive=(i>=lo && i<=hi);

        if (!shouldBeActive)
        {
            if (entry.active)
            {
                entry.consumer.resetPixmapProducer();
                entry.active=false;
            }
            continue;
        }

        // A first-acquire-only priority hint for the source -- see the class doc's note on the
        // PixmapKey data() side channel and setImageSource()'s re-wiring caveat below.
        entry.consumer.setData(
            QStringLiteral("uise.imageviewer.priority"),
            i==current ? QStringLiteral("visible") : QStringLiteral("prefetch")
        );

        if (entry.consumer.pixmapSource()!=pimpl->imageSource)
        {
            // Either first activation ever, or setImageSource() switched sources under an entry
            // that was already active -- setPixmapSource() releases the old producer (if any) and
            // acquires under the new source.
            entry.consumer.setPixmapSource(pimpl->imageSource);
        }
        else if (!entry.active)
        {
            // Same source as before but this entry had been deactivated (producer released) --
            // setPixmapSource() would no-op here since the source pointer is unchanged, so
            // re-acquire directly.
            entry.consumer.acquireProducer();
        }
        entry.active=true;
    }
}

//--------------------------------------------------------------------------

void AbstractImageViewer::evictEntry(bool fromFront)
{
    if (pimpl->window.empty())
    {
        return;
    }

    auto entry=fromFront ? pimpl->window.front() : pimpl->window.back();

    if (fromFront)
    {
        pimpl->window.pop_front();
        pimpl->windowFirstPosition+=1;
        pimpl->hasMoreBefore=true;
    }
    else
    {
        pimpl->window.pop_back();
        pimpl->hasMoreAfter=true;
    }

    if (entry->active)
    {
        entry->consumer.resetPixmapProducer();
        entry->active=false;
    }
    pimpl->byKey.erase(entry->key);

    onImageEvicted(entry->key);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::trimWindow()
{
    if (pimpl->windowSize==0)
    {
        return;
    }

    while (pimpl->window.size()>pimpl->windowSize)
    {
        auto count=pimpl->window.size();
        auto current=std::min(pimpl->currentIndex,count-1);
        auto distFront=current;
        auto distBack=count-1-current;

        bool frontProtected=distFront<=pimpl->activeWindowRadius;
        bool backProtected=distBack<=pimpl->activeWindowRadius;

        if (!frontProtected && (backProtected || distFront>=distBack))
        {
            evictEntry(true);
            if (pimpl->currentIndex>0)
            {
                --pimpl->currentIndex;
            }
        }
        else if (!backProtected)
        {
            evictEntry(false);
        }
        else
        {
            // windowSize() is smaller than the protected active range around the current image --
            // nothing left that is safe to evict without dropping one of its neighbours.
            break;
        }
    }

    reindexCurrent();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::maybeRequestMore()
{
    if (pimpl->inMaybeRequest)
    {
        return;
    }
    pimpl->inMaybeRequest=true;

    if (pimpl->hasMoreAfter && !pimpl->requestInFlightEnd)
    {
        auto count=pimpl->window.size();
        auto distance=(count==0) ? size_t{0} : (count-1-pimpl->currentIndex);
        if (count==0 || distance<pimpl->prefetchThreshold)
        {
            pimpl->requestInFlightEnd=true;
            emit imagesRequested(edgeAnchor(Direction::END),pimpl->fetchCount,Direction::END);
        }
    }

    if (pimpl->hasMoreBefore && !pimpl->requestInFlightHome)
    {
        auto count=pimpl->window.size();
        if (count==0 || pimpl->currentIndex<pimpl->prefetchThreshold)
        {
            pimpl->requestInFlightHome=true;
            emit imagesRequested(edgeAnchor(Direction::HOME),pimpl->fetchCount,Direction::HOME);
        }
    }

    pimpl->inMaybeRequest=false;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::updateCurrent(bool imageChanged)
{
    auto oldIndex=pimpl->currentIndex;
    reindexCurrent();
    refreshActiveProducers();

    if (imageChanged)
    {
        doSelectImage();
    }

    onWindowChanged();
    emit windowChanged();

    if (imageChanged || pimpl->currentIndex!=oldIndex)
    {
        emit currentImageIndexChanged(pimpl->currentIndex);
    }
    if (imageChanged)
    {
        emit currentImagePositionChanged(currentImagePosition());
    }

    maybeRequestMore();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::resolvePendingNav()
{
    using PendingNav=AbstractImageViewer_p::PendingNav;

    reindexCurrent();

    if (pimpl->pendingNav==PendingNav::Next)
    {
        if (!pimpl->window.empty() && (pimpl->currentIndex+1)<pimpl->window.size())
        {
            pimpl->pendingNav=PendingNav::None;
            pimpl->pendingNavTimer->clear();
            selectImage(pimpl->currentIndex+1);
            return;
        }
        if (!pimpl->hasMoreAfter)
        {
            pimpl->pendingNav=PendingNav::None;
            pimpl->pendingNavTimer->clear();
        }
    }
    else if (pimpl->pendingNav==PendingNav::Prev)
    {
        if (!pimpl->window.empty() && pimpl->currentIndex>0)
        {
            pimpl->pendingNav=PendingNav::None;
            pimpl->pendingNavTimer->clear();
            selectImage(pimpl->currentIndex-1);
            return;
        }
        if (!pimpl->hasMoreBefore)
        {
            pimpl->pendingNav=PendingNav::None;
            pimpl->pendingNavTimer->clear();
        }
    }

    updateCurrent(false);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::loadImages(std::vector<Image> images)
{
    loadImages(std::move(images),false,false,0,-1);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::loadImages(
    std::vector<Image> images,
    bool hasMoreBefore,
    bool hasMoreAfter,
    qint64 firstPosition,
    qint64 totalCount)
{
    pimpl->window.clear();
    pimpl->byKey.clear();
    pimpl->currentKey=PixmapKey{};
    pimpl->currentIndex=0;
    pimpl->windowFirstPosition=firstPosition;
    pimpl->totalCountHint=totalCount;
    pimpl->hasMoreBefore=hasMoreBefore;
    pimpl->hasMoreAfter=hasMoreAfter;
    pimpl->requestInFlightHome=false;
    pimpl->requestInFlightEnd=false;
    pimpl->pendingNav=AbstractImageViewer_p::PendingNav::None;
    pimpl->pendingNavTimer->clear();

    mergeImages(images,Direction::END);

    if (!pimpl->window.empty())
    {
        pimpl->currentKey=pimpl->window.front()->key;
    }

    updateCurrent(true);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::insertFetchedImages(std::vector<Image> images, Direction direction, size_t requestedCount)
{
    if (direction==Direction::END)
    {
        pimpl->requestInFlightEnd=false;
    }
    else if (direction==Direction::HOME)
    {
        pimpl->requestInFlightHome=false;
    }

    auto imageCountReceived=images.size();
    mergeImages(images,direction);
    reindexCurrent();

    if (requestedCount>0 && imageCountReceived<requestedCount)
    {
        if (direction==Direction::END)
        {
            pimpl->hasMoreAfter=false;
        }
        else if (direction==Direction::HOME)
        {
            pimpl->hasMoreBefore=false;
        }
    }

    trimWindow();
    resolvePendingNav();
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::updateImage(const Image& image)
{
    auto* entry=pimpl->find(image.key);
    if (entry==nullptr)
    {
        return false;
    }
    entry->content=image.content;

    if (entry->key==pimpl->currentKey)
    {
        doSelectImage();
    }
    onWindowChanged();
    emit windowChanged();
    return true;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::containsImage(const PixmapKey& key) const
{
    return pimpl->find(key)!=nullptr;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::removeImage(const PixmapKey& key)
{
    auto* target=pimpl->find(key);
    if (target==nullptr)
    {
        return false;
    }

    size_t removedIndex=0;
    bool found=false;
    for (size_t i=0; i<pimpl->window.size(); ++i)
    {
        if (pimpl->window[i].get()==target)
        {
            removedIndex=i;
            found=true;
            break;
        }
    }
    if (!found)
    {
        return false;
    }

    bool wasCurrent=(key==pimpl->currentKey);
    auto entry=pimpl->window[removedIndex];

    pimpl->window.erase(pimpl->window.begin()+static_cast<std::deque<std::shared_ptr<AbstractImageViewer_p::Entry>>::difference_type>(removedIndex));
    pimpl->byKey.erase(entry->key);

    if (entry->active)
    {
        entry->consumer.resetPixmapProducer();
        entry->active=false;
    }

    if (pimpl->totalCountHint>=0)
    {
        pimpl->totalCountHint=std::max<qint64>(0,pimpl->totalCountHint-1);
    }

    onImageRemoved(key);

    bool imageChanged=false;
    if (wasCurrent)
    {
        if (!pimpl->window.empty())
        {
            auto nextIndex=std::min(removedIndex,pimpl->window.size()-1);
            pimpl->currentIndex=nextIndex;
            pimpl->currentKey=pimpl->window[nextIndex]->key;
        }
        else
        {
            pimpl->currentIndex=0;
            pimpl->currentKey=PixmapKey{};
        }
        imageChanged=true;
    }

    updateCurrent(imageChanged);
    return true;
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::imageCount() const noexcept
{
    return pimpl->window.size();
}

//--------------------------------------------------------------------------

QPixmap AbstractImageViewer::currentImage() const
{
    return imagePixmap(pimpl->currentIndex);
}

//--------------------------------------------------------------------------

QPixmap AbstractImageViewer::imagePixmap(size_t index) const
{
    if (index>=pimpl->window.size())
    {
        return QPixmap{};
    }

    const auto& entry=*pimpl->window[index];

    // D4: prefer a resolved producer pixmap over the caller-seeded placeholder, so a version-
    // ladder source's later PixmapSource::updatePixmap() calls actually take effect instead of
    // being shadowed by the seed forever.
    if (entry.consumer.pixmapProducer()!=nullptr)
    {
        auto px=entry.consumer.pixmapProducer()->pixmap();
        if (!px.isNull())
        {
            return px;
        }
    }
    return entry.content;
}

//--------------------------------------------------------------------------

PixmapKey AbstractImageViewer::currentImageKey() const
{
    if (pimpl->window.empty() || pimpl->currentIndex>=pimpl->window.size())
    {
        return PixmapKey{};
    }
    return pimpl->window[pimpl->currentIndex]->key;
}

//--------------------------------------------------------------------------

PixmapKey AbstractImageViewer::imageKey(size_t index) const
{
    if (index>=pimpl->window.size())
    {
        return PixmapKey{};
    }
    return pimpl->window[index]->key;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setImageSource(std::shared_ptr<PixmapSource> imageSource)
{
    if (pimpl->imageSource==imageSource)
    {
        return;
    }

    pimpl->imageSource=std::move(imageSource);

    // Only entries currently within the active window re-acquire immediately below (via
    // refreshActiveProducers()); an inactive entry's stale source is detected and corrected lazily,
    // the moment it becomes active -- see refreshActiveProducers()'s own comment. Eagerly touching
    // every windowed entry here would force every one of them to acquire a producer right now,
    // defeating the whole point of the active-window bound (see class doc / DefaultActiveWindowRadius).
    refreshActiveProducers();

    onWindowChanged();
    emit windowChanged();
}

//--------------------------------------------------------------------------

std::shared_ptr<PixmapSource> AbstractImageViewer::imageSource() const
{
    return pimpl->imageSource;
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::currentImageIndex() const noexcept
{
    return pimpl->currentIndex;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::hasPrevImage() const noexcept
{
    return pimpl->currentIndex>0 || pimpl->hasMoreBefore;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::hasNextImage() const noexcept
{
    if (pimpl->window.empty())
    {
        return pimpl->hasMoreAfter;
    }
    return (pimpl->currentIndex+1)<pimpl->window.size() || pimpl->hasMoreAfter;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::hasMoreBefore() const noexcept
{
    return pimpl->hasMoreBefore;
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::hasMoreAfter() const noexcept
{
    return pimpl->hasMoreAfter;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setHasMoreBefore(bool enable)
{
    if (pimpl->hasMoreBefore==enable)
    {
        return;
    }
    pimpl->hasMoreBefore=enable;
    updateCurrent(false);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setHasMoreAfter(bool enable)
{
    if (pimpl->hasMoreAfter==enable)
    {
        return;
    }
    pimpl->hasMoreAfter=enable;
    updateCurrent(false);
}

//--------------------------------------------------------------------------

qint64 AbstractImageViewer::windowFirstPosition() const noexcept
{
    return pimpl->windowFirstPosition;
}

//--------------------------------------------------------------------------

qint64 AbstractImageViewer::currentImagePosition() const noexcept
{
    if (pimpl->window.empty())
    {
        return -1;
    }
    return pimpl->windowFirstPosition+static_cast<qint64>(pimpl->currentIndex);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::selectImagePosition(qint64 position)
{
    if (pimpl->window.empty())
    {
        return;
    }
    auto offset=position-pimpl->windowFirstPosition;
    if (offset<0 || offset>=static_cast<qint64>(pimpl->window.size()))
    {
        return;
    }
    selectImage(static_cast<size_t>(offset));
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setWindowFirstPosition(qint64 position)
{
    if (pimpl->windowFirstPosition==position)
    {
        return;
    }
    pimpl->windowFirstPosition=position;
    onWindowChanged();
    emit windowChanged();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setTotalCountHint(qint64 count)
{
    if (pimpl->totalCountHint==count)
    {
        return;
    }
    pimpl->totalCountHint=count;
    onWindowChanged();
    emit windowChanged();
}

//--------------------------------------------------------------------------

qint64 AbstractImageViewer::totalCountHint() const noexcept
{
    return pimpl->totalCountHint;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setWindowSize(size_t n)
{
    pimpl->windowSize=n;
    trimWindow();
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::windowSize() const noexcept
{
    return pimpl->windowSize;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setFetchCount(size_t n)
{
    pimpl->fetchCount=n;
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::fetchCount() const noexcept
{
    return pimpl->fetchCount;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setPrefetchThreshold(size_t n)
{
    pimpl->prefetchThreshold=n;
    maybeRequestMore();
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::prefetchThreshold() const noexcept
{
    return pimpl->prefetchThreshold;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setActiveWindowRadius(size_t n)
{
    pimpl->activeWindowRadius=n;
    refreshActiveProducers();
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::activeWindowRadius() const noexcept
{
    return pimpl->activeWindowRadius;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setPendingNavTimeoutMs(size_t ms)
{
    pimpl->pendingNavTimeoutMs=ms;
}

//--------------------------------------------------------------------------

size_t AbstractImageViewer::pendingNavTimeoutMs() const noexcept
{
    return pimpl->pendingNavTimeoutMs;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::cancelPendingRequests()
{
    pimpl->requestInFlightHome=false;
    pimpl->requestInFlightEnd=false;
    if (pimpl->pendingNav!=AbstractImageViewer_p::PendingNav::None)
    {
        pimpl->pendingNav=AbstractImageViewer_p::PendingNav::None;
        pimpl->pendingNavTimer->clear();
        onWindowChanged();
        emit windowChanged();
    }
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::isCurrentImageLoading() const
{
    if (pimpl->window.empty() || pimpl->currentIndex>=pimpl->window.size())
    {
        return false;
    }
    return pimpl->window[pimpl->currentIndex]->consumer.isLoading();
}

//--------------------------------------------------------------------------

bool AbstractImageViewer::isNavigationPending() const noexcept
{
    return pimpl->pendingNav!=AbstractImageViewer_p::PendingNav::None;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::setControlsMode(ControlsMode mode)
{
    m_controlsMode=mode;
}

//--------------------------------------------------------------------------

AbstractImageViewer::ControlsMode AbstractImageViewer::controlsMode() const noexcept
{
    return m_controlsMode;
}

//--------------------------------------------------------------------------

void AbstractImageViewer::showNextImage()
{
    using PendingNav=AbstractImageViewer_p::PendingNav;

    if (!pimpl->window.empty() && (pimpl->currentIndex+1)<pimpl->window.size())
    {
        selectImage(pimpl->currentIndex+1);
        return;
    }

    if (!pimpl->hasMoreAfter)
    {
        return;
    }

    pimpl->pendingNav=PendingNav::Next;
    if (!pimpl->requestInFlightEnd)
    {
        pimpl->requestInFlightEnd=true;
        emit imagesRequested(edgeAnchor(Direction::END),pimpl->fetchCount,Direction::END);
    }
    pimpl->pendingNavTimer->shot(
        pimpl->pendingNavTimeoutMs,
        [this]()
        {
            onPendingNavTimeout();
        }
    );

    onWindowChanged();
    emit windowChanged();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::showPrevImage()
{
    using PendingNav=AbstractImageViewer_p::PendingNav;

    if (!pimpl->window.empty() && pimpl->currentIndex>0)
    {
        selectImage(pimpl->currentIndex-1);
        return;
    }

    if (!pimpl->hasMoreBefore)
    {
        return;
    }

    pimpl->pendingNav=PendingNav::Prev;
    if (!pimpl->requestInFlightHome)
    {
        pimpl->requestInFlightHome=true;
        emit imagesRequested(edgeAnchor(Direction::HOME),pimpl->fetchCount,Direction::HOME);
    }
    pimpl->pendingNavTimer->shot(
        pimpl->pendingNavTimeoutMs,
        [this]()
        {
            onPendingNavTimeout();
        }
    );

    onWindowChanged();
    emit windowChanged();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::onPendingNavTimeout()
{
    // A reply that never arrived (or arrived so late that it doesn't matter) must not leave
    // isNavigationPending() -- and hence the overlay spinner -- pinned forever, and must not
    // permanently block that end's in-flight flag from ever requesting again.
    cancelPendingRequests();
}

//--------------------------------------------------------------------------

void AbstractImageViewer::selectImage(size_t index)
{
    if (pimpl->window.empty())
    {
        return;
    }
    if (index>=pimpl->window.size())
    {
        index=pimpl->window.size()-1;
    }

    auto newKey=pimpl->window[index]->key;
    bool changed=!(newKey==pimpl->currentKey);

    pimpl->currentIndex=index;
    pimpl->currentKey=newKey;

    updateCurrent(changed);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::selectImage(const PixmapKey& key)
{
    auto* entry=pimpl->find(key);
    if (entry!=nullptr)
    {
        for (size_t i=0; i<pimpl->window.size(); ++i)
        {
            if (pimpl->window[i].get()==entry)
            {
                selectImage(i);
                return;
            }
        }
    }
    selectImage(size_t{0});
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
