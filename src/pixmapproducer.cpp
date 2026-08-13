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

/** @file uise/desktop/pixmapproducer.cpp
*
*  Defines pixmap producer and consumer classes.
*
*/

/****************************************************************************/

#include <uise/desktop/stylecontext.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** PixmapProducer **********************************/

//--------------------------------------------------------------------------

QPixmap PixmapProducer::pixmap(IconVariant mode, QIcon::State state)
{
    if (m_svgIcon)
    {
        auto px=m_svgIcon->pixmap(size(),mode,state);
        if (!px.isNull())
        {
            return px;
        }
    }

    if (state==QIcon::State::On)
    {
        auto it=m_onPixmaps.find(mode);
        if (it!=m_onPixmaps.end())
        {
            return it->second;
        }
    }
    else
    {
        auto it=m_offPixmaps.find(mode);
        if (it!=m_offPixmaps.end())
        {
            return it->second;
        }
    }

    return m_defaultPixmap;
}

//--------------------------------------------------------------------------

void PixmapProducer::setSvgIcon(std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> svgIcon)
{
    m_svgIcon=std::move(svgIcon);
    emit pixmapUpdated();
}

//--------------------------------------------------------------------------

void PixmapProducer::setData(QVariant data)
{
    m_data=std::move(data);
    emit dataUpdated();
}

//--------------------------------------------------------------------------

void PixmapProducer::setImage(const QImage& img, UISE_DESKTOP_NAMESPACE::IconVariant mode, QIcon::State state)
{
    auto px=QPixmap::fromImage(img);
    setPixmap(px,mode,state);
}

//--------------------------------------------------------------------------

void PixmapProducer::setPixmap(const QPixmap& pixmap, UISE_DESKTOP_NAMESPACE::IconVariant mode, QIcon::State state)
{
    m_svgIcon.reset();
    auto px=pixmap;
    if (!px.isNull() && size().isValid() && px.size()!=size())
    {
        px=px.scaled(size(),m_aspectRatioMode,Qt::SmoothTransformation);
    }
    if (state==QIcon::State::On)
    {
        m_onPixmaps[mode]=std::move(px);
    }
    else
    {
        m_offPixmaps[mode]=std::move(px);
    }

    emit pixmapUpdated();
}

//--------------------------------------------------------------------------

void PixmapProducer::setDefaultPixmap(const QPixmap& pixmap)
{
    m_defaultPixmap=pixmap;
    if (!m_defaultPixmap.isNull() && size().isValid() && m_defaultPixmap.size()!=size())
    {
        m_defaultPixmap=m_defaultPixmap.scaled(size(),m_aspectRatioMode,Qt::SmoothTransformation);
    }
    emit pixmapUpdated();
}

//--------------------------------------------------------------------------

void PixmapProducer::setLoading(bool enable)
{
    if (m_loading==enable)
    {
        return;
    }
    m_loading=enable;
    emit loadingChanged(m_loading);
}

/************************** PixmapConsumer *********************************/

//--------------------------------------------------------------------------

PixmapConsumer::~PixmapConsumer()
{
    resetPixmapProducer();
}

//--------------------------------------------------------------------------

void PixmapConsumer::resetPixmapProducer()
{
    if (m_producer)
    {
        // Disconnect every signal wired in setPixmapProducer() below -- leaving dataUpdated/
        // loadingChanged connected would accumulate duplicate connections to the same slot once a
        // consumer cycles acquire/reset repeatedly against different producers (e.g. a flyweight
        // image viewer releasing producers outside its active window).
        disconnect(
            m_producer.get(),
            &PixmapProducer::pixmapUpdated,
            this,
            &PixmapConsumer::pixmapUpdated
        );
        disconnect(
            m_producer.get(),
            &PixmapProducer::dataUpdated,
            this,
            &PixmapConsumer::dataUpdated
        );
        disconnect(
            m_producer.get(),
            &PixmapProducer::loadingChanged,
            this,
            &PixmapConsumer::loadingChanged
        );
    }

    if (m_source)
    {
        m_source->releaseProducer(this);
    }
    m_producer.reset();
}

//--------------------------------------------------------------------------

void PixmapConsumer::setPixmapProducer(std::shared_ptr<PixmapProducer> producer)
{
    m_producer=std::move(producer);
    if (!m_producer)
    {
        return;
    }
    connect(
        m_producer.get(),
        &PixmapProducer::pixmapUpdated,
        this,
        &PixmapConsumer::pixmapUpdated
    );
    connect(
        m_producer.get(),
        &PixmapProducer::dataUpdated,
        this,
        &PixmapConsumer::dataUpdated
    );
    connect(
        m_producer.get(),
        &PixmapProducer::loadingChanged,
        this,
        &PixmapConsumer::loadingChanged
    );

    // Converge on the producer's CURRENT state, because any update it already published is gone:
    // PixmapSource::acquireProducer() calls doLoadPixmap() synchronously and only then returns the
    // producer for this function to connect to, so a source that answers synchronously (a cache
    // hit) emits pixmapUpdated() with nothing attached yet. The same applies to acquireProducer()'s
    // reuse branch, which hands back an already-populated producer and emits nothing at all.
    //
    // Real bug this fixes: a chat image tile looked correct the first time its chat page opened
    // (cache miss -> async fetch -> signal arrives after this connect) and reverted to its
    // low-resolution placeholder on every reopen within the source's cache TTL (cache hit ->
    // signal lost -> the consumer never learned real content was already available).
    //
    // Emitted directly rather than queued so a consumer that reads state right after acquiring is
    // already correct. Callers that ALSO poll the producer themselves (see
    // RoundedImage::createPixmapConsumer(), which pre-dates this fix) just get a harmless
    // second update.
    if (!m_producer->pixmap().isNull())
    {
        emit pixmapUpdated();
    }
    if (m_producer->data().isValid())
    {
        emit dataUpdated();
    }
    if (m_producer->isLoading())
    {
        emit loadingChanged(true);
    }
}

//--------------------------------------------------------------------------

void PixmapConsumer::acquireProducer()
{
    if (m_source && PixmapKey::isValid())
    {
        auto p=m_source->acquireProducer(this);
        setPixmapProducer(std::move(p));
    }
}

//--------------------------------------------------------------------------

bool PixmapConsumer::isLoading() const
{
    return m_producer && m_producer->isLoading();
}

//--------------------------------------------------------------------------

void PixmapConsumer::setPixmapSource(std::shared_ptr<PixmapSource> source)
{
    if (m_source==source)
    {
        return;
    }

    // Release any producer held in the previous source before switching -- otherwise the old
    // producer leaks there (it is never unregistered) and this consumer's connections to it are
    // never torn down. See resetPixmapProducer() for what "release" tears down.
    resetPixmapProducer();

    m_source=std::move(source);
    acquireProducer();
}

/************************** PixmapSource **********************************/

//--------------------------------------------------------------------------

PixmapSource::PixmapSource()
    : m_producerDestroyingDelayMs(DefaultProducerDestroyingDelayMs),
      m_aspectRatioMode(DefaultAspectRatioMode)
{
}

//--------------------------------------------------------------------------

PixmapSource::~PixmapSource()
{
}

//--------------------------------------------------------------------------

std::shared_ptr<PixmapProducer> PixmapSource::acquireProducer(PixmapConsumer* consumer)
{
    // qDebug() << "PixmapSource::acquireProducer consumer="<<consumer
    //                    <<" path="<<consumer->toString() << " size="<<consumer->size();

    auto& kIdx=keyIdx();

    auto it=kIdx.find(consumer->pixmapKey());
    if (it!=kIdx.end())
    {
        auto destroyingTimer=it->value()->destroyingTimer();
        destroyingTimer->clear();

        it->value()->registerConsumer(consumer);

        return it->sharedValue();
    }

    auto producer=std::make_shared<PixmapProducer>(consumer->pixmapKey());
    producer->setDefaultPixmap(m_defaultPixmap);
    producer->registerConsumer(consumer);
    m_producers.insert(producer);

    doLoadProducer(consumer->pixmapKey());
    doLoadPixmap(consumer->pixmapKey());

    return producer;
}

//--------------------------------------------------------------------------

void PixmapSource::releaseProducer(PixmapConsumer* consumer)
{
    auto& kIdx=keyIdx();

    auto it=kIdx.find(consumer->pixmapKey());
    if (it==kIdx.end())
    {
        return;
    }

    auto* producer=it->value();
    producer->unregisterConsumer(consumer);
    if (producer->consumerCount()>0)
    {
        return;
    }

    removeProducer(consumer->pixmapKey(),producer);
}

//--------------------------------------------------------------------------

void PixmapSource::removeProducer(PixmapKey key, PixmapProducer* producer)
{
    auto self=shared_from_this();

    auto remove=[key=std::move(key),self]()
    {
        self->doUnloadProducer(key);
        auto& kIdx=self->keyIdx();
        kIdx.erase(key);
    };

    if (m_producerDestroyingDelayMs==0)
    {
        remove();
    }
    else
    {
        producer->destroyingTimer()->shot(
            m_producerDestroyingDelayMs,
            std::move(remove)
        );
    }
}

//--------------------------------------------------------------------------

void PixmapSource::updatePixmap(const PixmapKey& key, const QPixmap& pixmap)
{
    auto& kIdx=keyIdx();
    auto it=kIdx.find(key);
    if (it==kIdx.end())
    {
        return;
    }

    auto* producer=it->value();
    // Mirror PixmapProducer::setPixmap()'s own guard: a producer with no fixed size (anySize keys,
    // e.g. DirectoryImagesViewer, or any flyweight image-viewer key requesting the original
    // resolution) reports an invalid QSize(-1,-1) from size(). Scaling to that would hand
    // QPixmap::scaled() an empty target size, which returns a null pixmap and silently destroys
    // the image being delivered.
    if (!pixmap.isNull() && producer->size().isValid() && pixmap.size()!=producer->size())
    {
        auto px=pixmap.scaled(producer->size(),m_aspectRatioMode,Qt::SmoothTransformation);
        producer->setPixmap(px);
    }
    else
    {
        producer->setPixmap(pixmap);
    }
}

//--------------------------------------------------------------------------

void PixmapSource::updateScaledPixmaps(const WithPath& path, const QPixmap& originalPixmap)
{
    auto& pIdx=pathIdx();
    auto [from,to]=pIdx.equal_range(path);
    for (auto it=from; it!=to; ++it)
    {
        auto* producer=it->value();
        if (!originalPixmap.isNull() && producer->size().isValid() && originalPixmap.size()!=producer->size())
        {
            auto px=originalPixmap.scaled(producer->size(),m_aspectRatioMode,Qt::SmoothTransformation);
            producer->setPixmap(px);
        }
        else
        {
            // Either no pixmap to give, or originalPixmap is already the right size (including the
            // anySize/original-resolution case where producer->size() is invalid and no scaling is
            // possible or needed) -- hand it over as-is rather than blanking a perfectly good pixmap.
            producer->setPixmap(originalPixmap);
        }
    }
}

//--------------------------------------------------------------------------

void PixmapSource::setPixmapLoading(const PixmapKey& key, bool enable)
{
    auto& kIdx=keyIdx();
    auto it=kIdx.find(key);
    if (it==kIdx.end())
    {
        return;
    }
    it->value()->setLoading(enable);
}

//--------------------------------------------------------------------------

void PixmapSource::setPathLoading(const WithPath& path, bool enable)
{
    auto& pIdx=pathIdx();
    auto [from,to]=pIdx.equal_range(path);
    for (auto it=from; it!=to; ++it)
    {
        it->value()->setLoading(enable);
    }
}

//--------------------------------------------------------------------------

std::vector<std::shared_ptr<PixmapProducer>> PixmapSource::producers(const WithPath& path) const
{
    std::vector<std::shared_ptr<PixmapProducer>> result;

    const auto& nIdx=pathIdx();
    auto [from,to]=nIdx.equal_range(path);
    for (auto it=from; it!=to; ++it)
    {
        result.emplace_back(it->sharedValue());
    }

    return result;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
