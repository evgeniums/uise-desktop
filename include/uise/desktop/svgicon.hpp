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

/** @file uise/desktop/svgicon.hpp
*
*  Declares SvgIcon class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SVG_ICON_HPP
#define UISE_DESKTOP_SVG_ICON_HPP

#include <memory>
#include <map>
#include <set>

#include <QDebug>
#include <QIcon>
#include <QIconEngine>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

enum class IconMode : int
{
    Normal=QIcon::Normal,
    Disabled=QIcon::Disabled,
    Active=QIcon::Active,
    Checked=QIcon::Selected,

    Hovered,

    User=0x100
};

inline std::map<QString,IconMode> defaultModeMap()
{
    std::map<QString,IconMode> m{
        {"normal",IconMode::Normal},
        {"disabled",IconMode::Disabled},
        {"active",IconMode::Active},
        {"checked",IconMode::Checked},
        {"hovered",IconMode::Hovered}
    };
    return m;
}

struct compareQSize
{
    template <typename T>
    bool operator () (const T& l, const T& r) const noexcept
    {
        if (l.width()<r.width())
        {
            return true;
        }
        if (l.width()>r.width())
        {
            return false;
        }
        if (l.height()<r.height())
        {
            return true;
        }
        if (l.height()>r.height())
        {
            return false;
        }
        return false;
    }
};

using SizeSet=std::set<QSize,compareQSize>;

class IconPixmapSet
{
    public:

        QPixmap pixmap(QIcon::State state=QIcon::On) const
        {
            if (fallback(state)->isNull() && !pixmaps(state)->empty())
            {
                return pixmaps(state)->begin()->second;
            }
            return *fallback(state);
        }

        void setPixmap(QPixmap icon, QIcon::State state=QIcon::On)
        {
            *fallback(state)=std::move(icon);
        }

        void addPixmap(QPixmap px,QIcon::State state=QIcon::On)
        {
            // qDebug() << "IconPixmapSet::addPixmap state=" << state << " size="<<px.size() << " "<<this;

            pixmaps(state)->emplace(px.size(),std::move(px));
        }

        QPixmap pixmap(const QSize& size, QIcon::State state=QIcon::On) const
        {
            // qDebug() << "IconPixmapSet::pixmap size=" << size << " state=" << state << " "<<this;

            if (size.isNull())
            {
                // qDebug()<<"null size";
                return pixmap(state);
            }

            auto it=pixmaps(state)->find(size);
            if (it!=pixmaps(state)->end())
            {
                // qDebug()<<"IconPixmapSet::pixmap found";
                return it->second;
            }
            // qDebug()<<"IconPixmapSet::pixmap not found "<< this;
            return pixmap(state);
        }

        QPixmap pixmap(int size, QIcon::State state=QIcon::On) const
        {
            return pixmap(QSize(size,size),state);
        }

        void clear()
        {
            m_pixmaps.clear();
            m_offPixmaps.clear();
        }

        void reset()
        {
            clear();
            m_fallback=QPixmap{};
            m_fallbackOff=QPixmap{};
        }

        bool isNull() const
        {
            return m_fallback.isNull() && m_pixmaps.empty();
        }

        bool isEmpty() const
        {
            return m_pixmaps.empty();
        }

    private:

        const QPixmap* fallback(QIcon::State state) const
        {
            if (state==QIcon::Off)
            {
                return &m_fallbackOff;
            }
            return &m_fallback;
        }

        const std::map<QSize,QPixmap,compareQSize>* pixmaps(QIcon::State state) const
        {
            if (state==QIcon::Off)
            {
                return &m_pixmaps;
            }
            return &m_offPixmaps;
        }

        QPixmap* fallback(QIcon::State state)
        {
            if (state==QIcon::Off)
            {
                return &m_fallbackOff;
            }
            return &m_fallback;
        }

        std::map<QSize,QPixmap,compareQSize>* pixmaps(QIcon::State state)
        {
            if (state==QIcon::Off)
            {
                return &m_pixmaps;
            }
            return &m_offPixmaps;
        }

        std::map<QSize,QPixmap,compareQSize> m_pixmaps;
        QPixmap m_fallback;

        std::map<QSize,QPixmap,compareQSize> m_offPixmaps;
        QPixmap m_fallbackOff;
};

class IconVariant
{
    public:

        IconVariant(IconMode mode=IconMode::Normal) : m_mode(static_cast<int>(mode))
        {}

        template <typename T>
        IconVariant(T val) : m_mode(static_cast<int>(val))
        {}

        IconMode mode() const noexcept
        {
            return static_cast<IconMode>(m_mode);
        }

        QIcon::Mode qIconMode() const noexcept
        {
            return mode<QIcon::Mode>();
        }

        template <typename T>
        T mode() const noexcept
        {
            return static_cast<T>(m_mode);
        }

        template <typename T>
        void  set(T val) noexcept
        {
            m_mode=static_cast<int>(val);
        }

        template <typename T>
        void  setMode(T val) noexcept
        {
            m_mode=static_cast<int>(val);
        }

        operator int() const noexcept
        {
            return m_mode;
        }

        operator IconMode() const noexcept
        {
            return static_cast<IconMode>(m_mode);
        }

        template <typename T>
        bool operator== (T other) const noexcept
        {
            return m_mode==static_cast<int>(other);
        }

        template <typename T>
        bool operator< (T other) const noexcept
        {
            return m_mode<static_cast<int>(other);
        }

        bool operator== (IconVariant other) const noexcept
        {
            return m_mode==other.m_mode;
        }

        bool operator< (IconVariant other) const noexcept
        {
            return m_mode<other.m_mode;
        }

    private:

        int m_mode;
};

class UISE_DESKTOP_EXPORT SvgIcon : public std::enable_shared_from_this<SvgIcon>
{
    public:

        struct ColorMap
        {
            std::map<QString,QString> off;
            std::map<QString,QString> on;

            ColorMap()=default;

            ColorMap(
                    std::map<QString,QString> off,
                    std::map<QString,QString> on
                ) : off(std::move(off)),
                    on(std::move(on))
            {}

            ColorMap(
                std::map<QString,QString> off
                ) : off(off),
                    on(off)
            {}
        };

        SvgIcon() =default;

        void paint(QPainter *painter, const QRect &rect, IconVariant mode,  QIcon::State state=QIcon::Off, bool cache=true);
        void paint(QPainter *painter, const QRect &rect)
        {
            paint(painter,rect,IconMode::Normal,QIcon::Off,false);
        }

        QPixmap pixmap(const QSize &size, IconVariant mode=IconMode::Normal,  QIcon::State state=QIcon::Off);

        QPixmap pixmap(int size, IconVariant mode, QIcon::State state)
        {
            return pixmap(QSize(size,size),mode,state);
        }        

        bool addFile(
            const QString& filename,
            const std::map<IconVariant,ColorMap>& colorMaps,
            const SizeSet& sizes=SizeSet{}
        );

        bool addFile(
            const QString& filename,
            IconVariant state,
            const SizeSet& sizes=SizeSet{}
        )
        {
            return addFile(filename,{{state,ColorMap{}}},sizes);
        }

        bool addFile(
                const QString& filename,
                const SizeSet& sizes
            )
        {
            return addFile(filename,IconMode::Normal,sizes);
        }

        bool addFile(
                const QString& filename
            )
        {
            return addFile(filename,IconMode::Normal,SizeSet{});
        }

        void reset()
        {
            m_pixmapSets.clear();
            m_initialContent.clear();
            m_onContent.clear();
            m_offContent.clear();
        }

        QIcon icon();
        QIcon hoverIcon();

        void reload(
            const std::map<IconVariant,ColorMap>& colorMaps
        );

        void setName(QString name)
        {            
            auto parts=name.split("::");
            if (parts.size()>1)
            {
                m_context=parts.at(parts.count()-2);
            }
            m_name=std::move(name);
        }

        QString name() const
        {            
            return m_name;
        }

        QString context() const
        {
            return m_context;
        }

        void reload(std::shared_ptr<SvgIcon> other)
        {
            m_context=other->m_context;
            m_initialContent=other->m_initialContent;
            m_onContent=other->m_onContent;
            m_offContent=other->m_offContent;
            m_pixmapSets.clear();

            for (auto&& it : m_refs)
            {
                auto refPtr=it.lock();
                if (refPtr)
                {
                    refPtr->reload(other);
                    other->m_refs.push_back(it);
                }
            }
            m_refs.clear();
        }

        void reloadOtherFromThis(std::shared_ptr<SvgIcon> other)
        {
            other->reload(shared_from_this());
            m_refs.emplace_back(other);
        }

    private:

        QString m_name;
        QString m_context;

        template <typename T>
        const IconPixmapSet* pixmapSet(T mode) const
        {
            IconVariant st(mode);
            auto it=m_pixmapSets.find(st);
            if (it!=m_pixmapSets.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        template <typename T>
        IconPixmapSet* pixmapSet(T mode)
        {
            IconVariant st(mode);
            auto it=m_pixmapSets.find(st);
            if (it!=m_pixmapSets.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        QPixmap makePixmap(const QSize &size, IconVariant mode=IconMode::Normal,  QIcon::State state=QIcon::On, bool cache=true);

        QByteArray offContent(IconVariant mode) const
        {
            // qDebug() << "looking for offContent " << mode << " name="<<m_name;

            auto it=m_offContent.find(mode);
            if (it!=m_offContent.end())
            {
                return it->second;
            }
            if (mode!=IconMode::Normal)
            {
                return offContent(IconMode::Normal);
            }
            return initialContent(mode);
        }

        QByteArray onContent(IconVariant mode) const
        {
            // qDebug() << "looking for onContent " << mode << " name="<<m_name;

            auto it=m_onContent.find(mode);
            if (it!=m_onContent.end())
            {
                // qDebug() << "on content found mode="<<mode;

                return it->second;
            }
            if (mode!=IconMode::Normal)
            {
                // qDebug() << "on content not found mode="<<mode;

                return onContent(IconMode::Normal);
            }

            // qDebug() << "on content fallback to initial mode="<<mode;
            return initialContent(mode);
        }

        QByteArray initialContent(IconVariant mode) const
        {
            auto it=m_initialContent.find(mode);
            if (it!=m_initialContent.end())
            {
                return it->second;
            }
            if (mode!=IconMode::Normal)
            {
                return initialContent(IconMode::Normal);
            }
            return QByteArray{};
        }

        std::map<IconVariant,QByteArray> m_initialContent;
        std::map<IconVariant,QByteArray> m_onContent;
        std::map<IconVariant,QByteArray> m_offContent;

        std::map<IconVariant,IconPixmapSet> m_pixmapSets;

        std::vector<std::weak_ptr<SvgIcon>> m_refs;
};

class SvgIconEngine : public QIconEngine
{
    public:

        SvgIconEngine(std::shared_ptr<SvgIcon> icon, bool hovered=false)
            : m_icon(std::move(icon)),
              m_hovered(hovered)
        {}

        virtual void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
        {
            // qDebug() << "paint mode=" << mode << " state=" << state;

            if (m_hovered  && IconVariant(mode)!=IconMode::Disabled)
            {
                m_icon->paint(painter,rect,IconMode::Hovered,state);
            }
            else
            {
                m_icon->paint(painter,rect,mode,state);
            }
        }

        virtual QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
        {
            // qDebug() << "pixmap mode=" << mode << " state=" << state;

            if (m_hovered && IconVariant(mode)!=IconMode::Disabled)
            {
                return m_icon->pixmap(size,IconMode::Hovered,state);
            }
            else
            {
                return m_icon->pixmap(size,mode,state);
            }
        }

        virtual QIconEngine* clone() const override
        {
            return new SvgIconEngine(m_icon);
        }

    private:

        std::shared_ptr<SvgIcon> m_icon;
        bool m_hovered;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SVG_ICON_HPP
