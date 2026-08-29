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

/** @file uise/desktop/imagepreviewstrip.cpp
*
*  Defines ImagePreviewStrip.
*
*/

/****************************************************************************/

#include <algorithm>
#include <map>

#include <QResizeEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPainter>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/pixmapscale.hpp>
#include <uise/desktop/imagelabel.hpp>
#include <uise/desktop/imagepreviewstrip.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ImagePreviewStrip_p
{
    public:

        struct ItemEntry
        {
            ImageLabel* widget=nullptr;
            PixmapKey key;
            QPixmap content;

            //! Seed animation content for this item (Preview::animation) -- what applyItemAnimation()
            //! applies when non-null. Distinct from appliedAnimation below.
            AnimationContent animation;

            //! Animation content actually last handed to widget->setImageFile()/setImageData() (or
            //! a null AnimationContent if none/cleared) -- lets setItemAnimation() skip redundant
            //! reloads (which would restart playback) when called again with the same content.
            AnimationContent appliedAnimation;

            PixmapConsumer consumer;
            bool consumerWired=false;
            QMetaObject::Connection clickedConnection;

            // Opacity is baked directly into the pixmap handed to widget->setPixmap() (see
            // ImagePreviewStrip::applyItemOpacity()) rather than via a per-item
            // QGraphicsOpacityEffect -- this widget already sits inside ChatImageViewerControls'
            // own single QGraphicsOpacityEffect (the overlay auto-hide fade), and NESTED
            // QGraphicsEffects are a known-broken Qt combination: the inner effect's cached
            // rendering does not reliably repaint on ordinary content/opacity changes once an
            // ancestor also has an effect installed, which showed up as previews staying blank
            // until an unrelated repaint (the outer fade, or a hover) forced the whole subtree
            // to redraw.
            QPixmap scaledContent;
            qreal opacity=1.0;
        };

        std::vector<ImagePreviewStrip::Preview> previews;
        std::vector<std::shared_ptr<ItemEntry>> items;

        int currentIndex=0;
        qreal visualIndex=0.0;

        std::shared_ptr<PixmapSource> imageSource;

        QVariantAnimation* visualIndexAnimation=nullptr;

        QSize itemSize{ImagePreviewStrip::DefaultItemSize,ImagePreviewStrip::DefaultItemSize};
        int spacing=ImagePreviewStrip::DefaultSpacing;
        qreal minOpacity=ImagePreviewStrip::DefaultMinOpacity;
        int visibleNeighbours=ImagePreviewStrip::DefaultVisibleNeighbours;
        int scrollAnimationDurationMs=ImagePreviewStrip::DefaultScrollAnimationDurationMs;

        ImageAnimator::AnimationMode animationMode=ImageAnimator::AnimationMode::Never;
};

//--------------------------------------------------------------------------

ImagePreviewStrip::ImagePreviewStrip(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ImagePreviewStrip_p>())
{
    setObjectName("imagePreviewStrip");

    pimpl->visualIndexAnimation=new QVariantAnimation(this);
    pimpl->visualIndexAnimation->setEasingCurve(QEasingCurve::InOutSine);
    connect(
        pimpl->visualIndexAnimation,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            setVisualIndex(value.toReal());
        }
    );
}

//--------------------------------------------------------------------------

ImagePreviewStrip::~ImagePreviewStrip()
{}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setPreviews(std::vector<Preview> previews, int currentIndex)
{
    // Captured before pimpl->items is touched by diffItems() below -- used only to decide
    // jump-vs-animate afterwards (see the class doc on setPreviews()).
    PixmapKey oldCurrentKey;
    bool hadCurrent=false;
    if (!pimpl->items.empty()
        && pimpl->currentIndex>=0
        && static_cast<size_t>(pimpl->currentIndex)<pimpl->items.size())
    {
        oldCurrentKey=pimpl->items[static_cast<size_t>(pimpl->currentIndex)]->key;
        hadCurrent=true;
    }

    pimpl->previews=std::move(previews);
    pimpl->visualIndexAnimation->stop();

    diffItems();

    auto count=static_cast<int>(pimpl->items.size());
    auto newIndex=count>0 ? qBound(0,currentIndex,count-1) : 0;
    bool animate=hadCurrent && indexOf(oldCurrentKey)>=0;

    pimpl->currentIndex=newIndex;
    if (animate)
    {
        pimpl->visualIndexAnimation->setDuration(pimpl->scrollAnimationDurationMs);
        pimpl->visualIndexAnimation->setStartValue(pimpl->visualIndex);
        pimpl->visualIndexAnimation->setEndValue(static_cast<qreal>(newIndex));
        pimpl->visualIndexAnimation->start();
    }
    else
    {
        pimpl->visualIndex=static_cast<qreal>(newIndex);
    }

    relayout();
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::clear()
{
    setPreviews({});
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::count() const noexcept
{
    return static_cast<int>(pimpl->items.size());
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::currentIndex() const noexcept
{
    return pimpl->currentIndex;
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::indexOf(const PixmapKey& key) const
{
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i]->key==key)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setCurrentPreview(const PixmapKey& key)
{
    auto idx=indexOf(key);
    if (idx>=0)
    {
        setCurrentIndex(idx);
    }
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setImageSource(std::shared_ptr<PixmapSource> source)
{
    if (pimpl->imageSource==source)
    {
        return;
    }
    pimpl->imageSource=std::move(source);

    // Re-applies every item's content: an item with non-null Preview::content is a no-op
    // (applyItemContent() returns immediately), an item never resolved through any source starts
    // resolving through this one, and -- unlike before this fix -- an item already resolved
    // through a DIFFERENT prior source is now correctly re-wired onto this one instead of being
    // silently left on the old one (see wireItemConsumer()/applyItemContent()'s own source check).
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        applyItemContent(i);
        applyItemAnimation(i);
    }
}

//--------------------------------------------------------------------------

std::shared_ptr<PixmapSource> ImagePreviewStrip::imageSource() const
{
    return pimpl->imageSource;
}

//--------------------------------------------------------------------------

QSize ImagePreviewStrip::itemSize() const
{
    return pimpl->itemSize;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setItemSize(QSize size)
{
    pimpl->itemSize=size;

    for (size_t i=0;i<pimpl->items.size();++i)
    {
        auto& entry=*pimpl->items[i];
        entry.widget->resize(size);
        entry.widget->setImageSize(size);

        QPixmap px=entry.content;
        if (px.isNull() && entry.consumer.pixmapProducer()!=nullptr)
        {
            px=entry.consumer.pixmapProducer()->pixmap();
        }
        setItemPixmap(i,px);
    }

    updateGeometry();
    relayout();
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::spacing() const
{
    return pimpl->spacing;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setSpacing(int px)
{
    pimpl->spacing=px;
    relayout();
}

//--------------------------------------------------------------------------

qreal ImagePreviewStrip::minOpacity() const
{
    return pimpl->minOpacity;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setMinOpacity(qreal value)
{
    pimpl->minOpacity=value;
    relayout();
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::visibleNeighbours() const
{
    return pimpl->visibleNeighbours;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setVisibleNeighbours(int n)
{
    pimpl->visibleNeighbours=n;
    relayout();
}

//--------------------------------------------------------------------------

int ImagePreviewStrip::scrollAnimationDurationMs() const
{
    return pimpl->scrollAnimationDurationMs;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setScrollAnimationDurationMs(int ms)
{
    pimpl->scrollAnimationDurationMs=ms;
}

//--------------------------------------------------------------------------

qreal ImagePreviewStrip::visualIndex() const
{
    return pimpl->visualIndex;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setAnimationMode(ImageAnimator::AnimationMode mode)
{
    if (pimpl->animationMode==mode)
    {
        return;
    }
    pimpl->animationMode=mode;

    for (auto& entry : pimpl->items)
    {
        entry->widget->setAnimationMode(mode);
    }
}

//--------------------------------------------------------------------------

ImageAnimator::AnimationMode ImagePreviewStrip::animationMode() const
{
    return pimpl->animationMode;
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setVisualIndex(qreal value)
{
    pimpl->visualIndex=value;
    relayout();
}

//--------------------------------------------------------------------------

QSize ImagePreviewStrip::sizeHint() const
{
    auto span=2*pimpl->visibleNeighbours+1;
    auto w=span*pimpl->itemSize.width()+(span-1)*pimpl->spacing;
    return QSize(w,pimpl->itemSize.height());
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setCurrentIndex(int index)
{
    if (pimpl->items.empty())
    {
        pimpl->currentIndex=0;
        return;
    }

    index=qBound(0,index,static_cast<int>(pimpl->items.size())-1);
    if (index==pimpl->currentIndex)
    {
        return;
    }
    pimpl->currentIndex=index;

    pimpl->visualIndexAnimation->stop();
    pimpl->visualIndexAnimation->setDuration(pimpl->scrollAnimationDurationMs);
    pimpl->visualIndexAnimation->setStartValue(pimpl->visualIndex);
    pimpl->visualIndexAnimation->setEndValue(static_cast<qreal>(index));
    pimpl->visualIndexAnimation->start();
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    relayout();
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::diffItems()
{
    // Key -> previous entry, so a key that reappears (mostly-overlapping continuous slide) or
    // simply moves position (mostly-disjoint album swap that happens to share one boundary image)
    // keeps its widget and, if already resolved, its live PixmapConsumer -- see the class doc on
    // setPreviews().
    std::map<PixmapKey,std::shared_ptr<ImagePreviewStrip_p::ItemEntry>> oldByKey;
    for (auto& entry : pimpl->items)
    {
        oldByKey[entry->key]=entry;
    }

    std::vector<std::shared_ptr<ImagePreviewStrip_p::ItemEntry>> newItems;
    newItems.reserve(pimpl->previews.size());

    for (auto& preview : pimpl->previews)
    {
        auto it=oldByKey.find(preview.key);
        if (it!=oldByKey.end())
        {
            auto entry=it->second;
            oldByKey.erase(it);

            // Refresh the stored key too, not just content -- preview.key can carry an updated
            // data() payload (e.g. a host-attached priority/cursor hint) even when path+size
            // (hence PixmapKey equality) are unchanged.
            entry->key=preview.key;
            if (entry->content.cacheKey()!=preview.content.cacheKey())
            {
                entry->content=preview.content;
            }
            entry->animation=preview.animation;

            newItems.push_back(std::move(entry));
            continue;
        }

        auto entry=std::make_shared<ImagePreviewStrip_p::ItemEntry>();
        entry->key=preview.key;
        entry->content=preview.content;
        entry->animation=preview.animation;

        entry->widget=new ImageLabel(this);
        entry->widget->setObjectName("previewItem");
        entry->widget->setClickable(true);
        entry->widget->setAutoSize(false);
        entry->widget->setAnimationMode(pimpl->animationMode);
        entry->widget->setImageSize(pimpl->itemSize);
        entry->widget->resize(pimpl->itemSize);
        entry->widget->show();

        newItems.push_back(std::move(entry));
    }

    // Whatever is left in oldByKey fell out of the new set entirely.
    for (auto& [key,entry] : oldByKey)
    {
        std::ignore=key;
        destroyWidget(entry->widget);
    }

    pimpl->items=std::move(newItems);

    // Re-wire click handlers now that indices are final. Capturing key (not the index itself) and
    // re-resolving via indexOf() at click/emit time keeps this correct even across a LATER
    // setPreviews() call that reorders items without touching this particular connection again.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        auto& entry=*pimpl->items[i];
        auto key=entry.key;

        if (entry.clickedConnection)
        {
            disconnect(entry.clickedConnection);
        }
        entry.clickedConnection=connect(
            entry.widget,
            &ImageLabel::clicked,
            this,
            [this,key]()
            {
                emit previewClicked(indexOf(key));
                emit previewClickedKey(key);
            }
        );

        applyItemContent(i);
        applyItemAnimation(i);
    }

    // Deliberately NOT setVisible(items.size()>1) -- hiding the whole widget would exclude it
    // (and its stretch factor) from the host's own layout entirely once Qt lays out siblings,
    // making the host's overall bar width/height depend on whether an album happens to be
    // showing (see ChatImageViewerControls, which relies on this strip always reserving its
    // constant-size layout slot). Zero items already means nothing is drawn or clickable here --
    // that alone satisfies "not visible for a single image" from the user's point of view.
    updateGeometry();
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::applyItemContent(size_t index)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

    if (!entry.content.isNull())
    {
        setItemPixmap(index,entry.content);
        return;
    }

    if (!pimpl->imageSource)
    {
        setItemPixmap(index,QPixmap());
        return;
    }

    if (entry.consumer.pixmapSource()==pimpl->imageSource)
    {
        // Already wired to the current source -- wireItemConsumer() only ever connects
        // pixmapUpdated once per entry (see its own consumerWired guard), nothing more to do; the
        // existing connection already resolves this item or is still waiting to.
        return;
    }

    wireItemConsumer(index);
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::applyItemAnimation(size_t index)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

    if (!entry.animation.isNull())
    {
        setItemAnimation(index,entry.animation);
        return;
    }

    if (!pimpl->imageSource)
    {
        setItemAnimation(index,AnimationContent{});
        return;
    }

    // Resolved (if at all) through the consumer wireItemConsumer() wires for the pixmap channel --
    // it connects PixmapConsumer::animationUpdated alongside pixmapUpdated, so there is nothing
    // further to do here; applyItemContent(), always called alongside this one, already triggered
    // wireItemConsumer() if needed.
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::wireItemConsumer(size_t index)
{
    auto& entry=*pimpl->items[index];
    auto key=entry.key;

    if (!entry.consumerWired)
    {
        entry.consumerWired=true;
        connect(
            &entry.consumer,
            &PixmapConsumer::pixmapUpdated,
            this,
            [this,key]()
            {
                auto idx=indexOf(key);
                if (idx<0)
                {
                    return;
                }
                auto* producer=pimpl->items[static_cast<size_t>(idx)]->consumer.pixmapProducer();
                if (producer!=nullptr)
                {
                    setItemPixmap(static_cast<size_t>(idx),producer->pixmap());
                }
            }
        );
        connect(
            &entry.consumer,
            &PixmapConsumer::animationUpdated,
            this,
            [this,key]()
            {
                auto idx=indexOf(key);
                if (idx<0)
                {
                    return;
                }
                auto* producer=pimpl->items[static_cast<size_t>(idx)]->consumer.pixmapProducer();
                if (producer!=nullptr)
                {
                    setItemAnimation(static_cast<size_t>(idx),producer->animation());
                }
            }
        );
    }

    entry.consumer.setPathAndSize(entry.key);
    // Releases any previous source's producer first (see PixmapConsumer::setPixmapSource()) --
    // this is what fixes the "second setImageSource() call is silently ignored" bug: the guard
    // above only skips entries already on the CURRENT source, not merely already-wired ones.
    entry.consumer.setPixmapSource(pimpl->imageSource);

    auto* producer=entry.consumer.pixmapProducer();
    if (producer!=nullptr)
    {
        setItemPixmap(index,producer->pixmap());
        setItemAnimation(index,producer->animation());
    }
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setItemPixmap(size_t index, const QPixmap& px)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

    entry.scaledContent=px.isNull() ? QPixmap() : scaledAndCropped(px,entry.widget->size());
    applyItemOpacity(index);
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::setItemAnimation(size_t index, const AnimationContent& animation)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

    if (entry.appliedAnimation==animation)
    {
        return;
    }
    entry.appliedAnimation=animation;

    if (animation.isNull())
    {
        if (entry.widget->isAnimated())
        {
            entry.widget->clearImage();
            applyItemOpacity(index);
        }
        return;
    }

    if (!animation.file.isEmpty())
    {
        entry.widget->setImageFile(animation.file);
    }
    else
    {
        entry.widget->setImageData(animation.data,animation.format);
    }
    applyItemOpacity(index);
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::applyItemOpacity(size_t index)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

    if (entry.widget->isAnimated())
    {
        // Animated content paints its own decoded frames (see ImageLabel::paintEvent()), a path
        // that never chains through RoundedImage's pixmap()-based paint -- so opacity has
        // to be applied via ImageLabel::setContentOpacity() instead of being baked into a pixmap
        // handed to setPixmap(), as the static branch below does.
        entry.widget->setContentOpacity(qMax(0.0,entry.opacity));
        return;
    }
    entry.widget->setContentOpacity(1.0);

    if (entry.scaledContent.isNull())
    {
        entry.widget->setPixmap(QPixmap());
        return;
    }
    if (entry.opacity>=1.0)
    {
        entry.widget->setPixmap(entry.scaledContent);
        return;
    }

    // Bakes entry.opacity directly into a fresh ARGB32 pixmap -- see ItemEntry::scaledContent's
    // doc comment for why this replaces a per-item QGraphicsOpacityEffect.
    QPixmap blended(entry.scaledContent.size());
    blended.setDevicePixelRatio(entry.scaledContent.devicePixelRatio());
    blended.fill(Qt::transparent);
    QPainter p(&blended);
    p.setOpacity(qMax(0.0,entry.opacity));
    p.drawPixmap(0,0,entry.scaledContent);
    p.end();
    entry.widget->setPixmap(blended);
}

//--------------------------------------------------------------------------

void ImagePreviewStrip::relayout()
{
    auto n=pimpl->items.size();
    if (n==0)
    {
        return;
    }

    const auto& sz=pimpl->itemSize;
    auto step=sz.width()+pimpl->spacing;
    auto cx=width()/2;
    auto cy=height()/2;

    auto denom=qMax(1,pimpl->visibleNeighbours);

    for (size_t i=0;i<n;++i)
    {
        auto& entry=*pimpl->items[i];

        qreal offset=static_cast<qreal>(i)-pimpl->visualIndex;
        int x=cx+qRound(offset*step)-sz.width()/2;
        int y=cy-sz.height()/2;
        entry.widget->setGeometry(x,y,sz.width(),sz.height());

        auto distance=qAbs(offset);
        qreal opacity=1.0;
        if (distance>0.0)
        {
            opacity=qMax(pimpl->minOpacity,1.0-(distance/static_cast<qreal>(denom))*(1.0-pimpl->minOpacity));
        }
        entry.opacity=opacity;
        applyItemOpacity(i);
    }

    // Raise items nearest the centre last, so dimmer neighbours sit visually below them where
    // adjacent items overlap during the scroll animation.
    std::vector<size_t> order(n);
    for (size_t i=0;i<n;++i)
    {
        order[i]=i;
    }
    std::sort(
        order.begin(),
        order.end(),
        [this](size_t a, size_t b)
        {
            return qAbs(static_cast<qreal>(a)-pimpl->visualIndex) > qAbs(static_cast<qreal>(b)-pimpl->visualIndex);
        }
    );
    for (auto i : order)
    {
        pimpl->items[i]->widget->raise();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
