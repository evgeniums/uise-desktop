/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/pixmapproducer.hpp
*
*  Declares pixmap producer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PIXMAP_PRODUCER_HPP
#define UISE_DESKTOP_PIXMAP_PRODUCER_HPP

#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <QObject>
#include <QPixmap>
#include <QImage>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class PixmapProducer;
class PixmapSource;

class WithNameAndSize
{
    public:

        WithNameAndSize(QString name={}, const QSize& size={})
            : m_name(std::move(name)),
              m_size(size)
        {}

        void setSize(const QSize& size)
        {
            m_size=size;
        }

        QSize size() const noexcept
        {
            return m_size;
        }

        void setName(QString name)
        {
            m_name=std::move(name);
        }

        QString name() const noexcept
        {
            return m_name;
        }

    private:

        QString m_name;
        QSize m_size;
};

struct PixmapKey
{
    QString name;
    QSize size;

    PixmapKey(QString name={}, QSize size={})
        : name(std::move(name)),
        size(size)
    {}

    PixmapKey(const WithNameAndSize& nmsz)
        : PixmapKey(nmsz.name(),nmsz.size())
    {}

    bool operator <(const PixmapKey& other) const noexcept
    {
        if (name<other.name)
        {
            return true;
        }
        if (name>other.name)
        {
            return false;
        }
        if (size.width()<other.size.width())
        {
            return true;
        }
        if (size.width()>other.size.width())
        {
            return false;
        }
        if (size.height()<other.size.height())
        {
            return true;
        }
        if (size.height()>other.size.height())
        {
            return false;
        }
        return false;
    }
};

class UISE_DESKTOP_EXPORT PixmapConsumer : public QObject,
                                           public WithNameAndSize
{
    Q_OBJECT

    public:

        PixmapConsumer(QString name, const QSize& size={}, QObject* parent=nullptr);
        PixmapConsumer(QObject* parent=nullptr);

        virtual ~PixmapConsumer();
        PixmapConsumer(const PixmapConsumer&)=delete;
        PixmapConsumer(PixmapConsumer&&)=delete;
        PixmapConsumer& operator=(const PixmapConsumer&)=delete;
        PixmapConsumer& operator=(PixmapConsumer&&)=delete;

        void setPixmapSource(std::shared_ptr<PixmapSource> source);

        std::shared_ptr<PixmapSource> pixmapSource() const
        {
            return m_source;
        }

        PixmapProducer* pixmapProducer() const
        {
            return m_producer.get();
        }

        std::shared_ptr<PixmapProducer> pixmapProducerShared() const
        {
            return m_producer;
        }

        void resetPixmapProducer();

        PixmapKey pixmapKey() const
        {
            return PixmapKey{name(),size()};
        }

    signals:

        void pixmapUpdated();

    private:

        void setPixmapProducer(std::shared_ptr<PixmapProducer> producer);

        std::shared_ptr<PixmapSource> m_source;
        std::shared_ptr<PixmapProducer> m_producer;
};

class UISE_DESKTOP_EXPORT PixmapProducer : public QObject,
                                           public WithNameAndSize
{
    Q_OBJECT

    public:

        constexpr static const QIcon::State DefaultIconState=QIcon::State::Off;
        constexpr static const Qt::AspectRatioMode DefaultAspectRatioMode=Qt::KeepAspectRatioByExpanding;

        PixmapProducer(QString name={}, const QSize& size={});

        size_t consumerCount() const noexcept
        {
            return m_consumers.size();
        }

        PixmapKey pixmapKey() const
        {
            return PixmapKey{name(),size()};
        }

        QPixmap pixmap(IconVariant mode={}, QIcon::State state=DefaultIconState);

        std::shared_ptr<SvgIcon> svgIcon() const
        {
            return m_svgIcon;
        }

        void setDefaultPixmap(const QPixmap& pixmap);
        QPixmap defaultPixmap() const
        {
            return m_defaultPixmap;
        }

        void emitPixmapUpdated()
        {
            emit pixmapUpdated();
        }

        void setAspectRatioMode(Qt::AspectRatioMode mode) noexcept
        {
            m_aspectRatioMode=mode;
        }

        Qt::AspectRatioMode aspectRatioMode() const noexcept
        {
            return m_aspectRatioMode;
        }

    signals:

        void pixmapUpdated();

    public slots:

        void setImage(const QImage& img, UISE_DESKTOP_NAMESPACE::IconVariant mode={}, QIcon::State state=DefaultIconState);
        void setPixmap(const QPixmap& pixmap, UISE_DESKTOP_NAMESPACE::IconVariant mode={}, QIcon::State state=DefaultIconState);

        void setSvgIcon(std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> svgIcon);

    private:

        SingleShotTimer* destroyingTimer()
        {
            return m_destroyingTimer;
        }

        void registerConsumer(PixmapConsumer* consumer)
        {
            m_consumers.insert(consumer);
        }

        void unregisterConsumer(PixmapConsumer* consumer)
        {
            m_consumers.erase(consumer);
        }

        void clearConsumers()
        {
            m_consumers.clear();
        }

        QPixmap m_defaultPixmap;

        std::map<IconMode,QPixmap> m_onPixmaps;
        std::map<IconMode,QPixmap> m_offPixmaps;

        std::shared_ptr<SvgIcon> m_svgIcon;

        std::set<PixmapConsumer*> m_consumers;

        SingleShotTimer* m_destroyingTimer;

        Qt::AspectRatioMode m_aspectRatioMode;

        friend class PixmapSource;
};

class PixmapProducerWrapper
{
    public:

        PixmapProducerWrapper(std::shared_ptr<PixmapProducer> value) : m_value(std::move(value))
        {}

        std::shared_ptr<PixmapProducer> sharedValue() const
        {
            return m_value;
        }

        PixmapProducer* value() const
        {
            return m_value.get();
        }

        auto* operator -> () const noexcept
        {
            return m_value.get();
        }

        auto* operator * () const noexcept
        {
            return m_value.get();
        }

        PixmapKey key() const
        {
            return m_value->pixmapKey();
        }

        QString name() const
        {
            return m_value->name();
        }

    private:

        std::shared_ptr<PixmapProducer> m_value;
};

class UISE_DESKTOP_EXPORT PixmapSource : public std::enable_shared_from_this<PixmapSource>
{
    public:

        constexpr static const size_t DefaultProducerDestroyingDelayMs=15000;
        constexpr static const Qt::AspectRatioMode DefaultAspectRatioMode=Qt::KeepAspectRatioByExpanding;

        PixmapSource();

        virtual ~PixmapSource();

        PixmapSource(const PixmapSource&)=delete;
        PixmapSource(PixmapSource&&)=default;
        PixmapSource& operator=(const PixmapSource&)=delete;
        PixmapSource& operator=(PixmapSource&&)=default;

        void setDefaultPixmap(const QPixmap& pixmap)
        {
            m_defaultPixmap=pixmap;
        }

        QPixmap defaultPixmap() const
        {
            return m_defaultPixmap;
        }

        std::shared_ptr<PixmapProducer> acquireProducer(PixmapConsumer* consumer);
        void releaseProducer(PixmapConsumer* consumer);

        void setProducerDestroyingDelay(size_t value) noexcept
        {
            m_producerDestroyingDelayMs=value;
        }

        size_t producerDestroyingDelay() const noexcept
        {
            return m_producerDestroyingDelayMs;
        }

        void updatePixmap(const PixmapKey& key, const QPixmap& pixmap);

        void updateScaledPixmaps(const QString& name, const QPixmap& originalPixmap);

        void setAspectRatioMode(Qt::AspectRatioMode mode) noexcept
        {
            m_aspectRatioMode=mode;
        }

        Qt::AspectRatioMode aspectRatioMode() const noexcept
        {
            return m_aspectRatioMode;
        }

        std::vector<std::shared_ptr<PixmapProducer>> producers(const QString& name) const;

    protected:

        virtual void doLoadProducer(const PixmapKey& key)
        {
            std::ignore=key;
        }

        virtual void doUnloadProducer(const PixmapKey& key)
        {
            std::ignore=key;
        }

        virtual void doLoadPixmap(const PixmapKey& key) =0;

        void removeProducer(PixmapKey key, PixmapProducer* producer);

        QPixmap m_defaultPixmap;

        using PixmapKeyIdxFn=boost::multi_index::const_mem_fun<
            PixmapProducerWrapper,
            PixmapKey,
            &PixmapProducerWrapper::key
        >;
        using PixmapNameIdxFn=boost::multi_index::const_mem_fun<
            PixmapProducerWrapper,
            QString,
            &PixmapProducerWrapper::name
        >;

        using ProducersContainer=boost::multi_index::multi_index_container
            <
                PixmapProducerWrapper,
                boost::multi_index::indexed_by<
                    boost::multi_index::ordered_non_unique<
                        PixmapNameIdxFn
                        >,
                    boost::multi_index::ordered_unique<
                        PixmapKeyIdxFn
                        >
                    >
            >;

        ProducersContainer m_producers;

        auto& keyIdx() noexcept
        {
            return m_producers.get<1>();
        }

        auto& nameIdx() noexcept
        {
            return m_producers.get<0>();
        }

        const auto& nameIdx() const noexcept
        {
            return m_producers.get<0>();
        }

        size_t m_producerDestroyingDelayMs;

        Qt::AspectRatioMode m_aspectRatioMode;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PIXMAP_PRODUCER_HPP
