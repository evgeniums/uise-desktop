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
            PixmapConsumer consumer;

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
    pimpl->previews=std::move(previews);
    pimpl->visualIndexAnimation->stop();

    rebuildItems();

    auto count=static_cast<int>(pimpl->items.size());
    pimpl->currentIndex=count>0 ? qBound(0,currentIndex,count-1) : 0;
    pimpl->visualIndex=static_cast<qreal>(pimpl->currentIndex);

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

void ImagePreviewStrip::setImageSource(std::shared_ptr<PixmapSource> source)
{
    pimpl->imageSource=std::move(source);

    // Only affects items that never got a source at all yet (Preview::content null and no prior
    // setImageSource() call) -- applyItemContent() itself no-ops for anything already wired to a
    // producer, so calling this again is safe rather than accumulating duplicate connections.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        applyItemContent(i);
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

void ImagePreviewStrip::rebuildItems()
{
    for (auto& entry : pimpl->items)
    {
        destroyWidget(entry->widget);
    }
    pimpl->items.clear();

    for (auto& preview : pimpl->previews)
    {
        auto entry=std::make_shared<ImagePreviewStrip_p::ItemEntry>();
        entry->key=preview.key;
        entry->content=preview.content;

        entry->widget=new ImageLabel(this);
        entry->widget->setObjectName("previewItem");
        entry->widget->setClickable(true);
        entry->widget->setAutoSize(false);
        entry->widget->setImageSize(pimpl->itemSize);
        entry->widget->resize(pimpl->itemSize);

        auto index=pimpl->items.size();
        connect(
            entry->widget,
            &ImageLabel::clicked,
            this,
            [this,index]()
            {
                emit previewClicked(static_cast<int>(index));
            }
        );

        pimpl->items.push_back(entry);
        entry->widget->show();

        applyItemContent(index);
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

    if (entry.consumer.pixmapProducer()!=nullptr)
    {
        // Already wired to a source by an earlier call (rebuildItems() or a previous
        // setImageSource()) -- PixmapConsumer::setPixmapProducer() never disconnects a prior
        // pixmapUpdated connection, so re-wiring here would accumulate a duplicate connection
        // and fire setItemPixmap() multiple times per update. The existing connection already
        // resolves this item (or is still waiting to), nothing to do.
        return;
    }

    entry.consumer.setPathAndSize(entry.key);
    entry.consumer.setPixmapSource(pimpl->imageSource);
    connect(
        &entry.consumer,
        &PixmapConsumer::pixmapUpdated,
        this,
        [this,index]()
        {
            if (index>=pimpl->items.size())
            {
                return;
            }
            auto* producer=pimpl->items[index]->consumer.pixmapProducer();
            if (producer!=nullptr)
            {
                setItemPixmap(index,producer->pixmap());
            }
        }
    );
    entry.consumer.acquireProducer();

    auto* producer=entry.consumer.pixmapProducer();
    if (producer!=nullptr)
    {
        setItemPixmap(index,producer->pixmap());
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

void ImagePreviewStrip::applyItemOpacity(size_t index)
{
    if (index>=pimpl->items.size())
    {
        return;
    }
    auto& entry=*pimpl->items[index];

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
