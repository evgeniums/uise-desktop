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
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** PixmapProducer **********************************/

//--------------------------------------------------------------------------

PixmapProducer::PixmapProducer(QString name, const QSize& size)
    : WithNameAndSize(std::move(name),size),
      m_destroyingTimer(new SingleShotTimer(this))
{
}

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
        px=px.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    }
    if (state==QIcon::State::On)
    {
        m_onPixmaps.emplace(mode,std::move(px));
    }
    else
    {
        m_offPixmaps.emplace(mode,std::move(px));
    }

    emit pixmapUpdated();
}

//--------------------------------------------------------------------------

void PixmapProducer::setDefaultPixmap(const QPixmap& pixmap)
{
    m_defaultPixmap=pixmap;
    if (!m_defaultPixmap.isNull() && size().isValid() && m_defaultPixmap.size()!=size())
    {
        m_defaultPixmap=m_defaultPixmap.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    }
    emit pixmapUpdated();
}

/************************** PixmapConsumer *********************************/

//--------------------------------------------------------------------------

PixmapConsumer::PixmapConsumer(QObject* parent)
    : QObject(parent)
{
}

//--------------------------------------------------------------------------

PixmapConsumer::PixmapConsumer(QString name, const QSize& size, QObject* parent)
    : QObject(parent),
      WithNameAndSize(std::move(name),size)
{
}

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
        disconnect(
            m_producer.get(),
            &PixmapProducer::pixmapUpdated,
            this,
            &PixmapConsumer::pixmapUpdated
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
    connect(
        m_producer.get(),
        &PixmapProducer::pixmapUpdated,
        this,
        &PixmapConsumer::pixmapUpdated
    );
}

//--------------------------------------------------------------------------

void PixmapConsumer::setPixmapSource(std::shared_ptr<PixmapSource> source)
{
    m_source=std::move(source);
    if (m_source)
    {
        auto p=m_source->acquireProducer(this);
        setPixmapProducer(std::move(p));
    }
}

/************************** PixmapSource **********************************/

//--------------------------------------------------------------------------

PixmapSource::PixmapSource()
    : m_producerDestroyingDelayMs(DefaultProducerDestroyingDelayMs)
{
}

//--------------------------------------------------------------------------

PixmapSource::~PixmapSource()
{
}

//--------------------------------------------------------------------------

std::shared_ptr<PixmapProducer> PixmapSource::acquireProducer(PixmapConsumer* consumer)
{
    PixmapKey key{consumer->name(),consumer->size()};
    auto& kIdx=keyIdx();

    auto it=kIdx.find(key);
    if (it!=kIdx.end())
    {
        auto destroyingTimer=it->value()->destroyingTimer();
        destroyingTimer->clear();

        it->value()->registerConsumer(consumer);

        return it->sharedValue();
    }

    auto producer=std::make_shared<PixmapProducer>(consumer->name(),consumer->size());
    producer->setDefaultPixmap(m_defaultPixmap);
    producer->registerConsumer(consumer);
    m_producers.insert(producer);

    doLoadPixmap(key);

    return producer;
}

//--------------------------------------------------------------------------

void PixmapSource::releaseProducer(PixmapConsumer* consumer)
{
    PixmapKey key{consumer->name(),consumer->size()};
    auto& kIdx=keyIdx();

    auto it=kIdx.find(key);
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

    removeProducer(key,producer);
}

//--------------------------------------------------------------------------

void PixmapSource::removeProducer(PixmapKey key, PixmapProducer* producer)
{
    auto self=shared_from_this();

    auto remove=[key=std::move(key),self]()
    {
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

UISE_DESKTOP_NAMESPACE_END
