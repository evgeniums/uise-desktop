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

/** @file uise/desktop/icon.hpp
*
*  Declares icon class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ICON_HPP
#define UISE_DESKTOP_ICON_HPP

#include <map>
#include <set>

#include <QIcon>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

enum class IconMode : int
{
    Normal=QIcon::Normal,
    Disabled=QIcon::Disabled,
    Hovered=QIcon::Active,
    Checked=QIcon::Selected,

    User=0x100
};

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

class IconSet
{
    public:

        QIcon icon() const
        {
            if (m_fallbackIcon.isNull() && !m_icons.empty())
            {
                return m_icons.begin()->second;
            }
            return m_fallbackIcon;
        }

        void setIcon(QIcon icon)
        {
            m_fallbackIcon=std::move(icon);
        }

        void addIcon(QIcon icon, const QSize& size)
        {
            if (size.isNull())
            {
                setIcon(std::move(icon));
            }
            else
            {
                m_icons.emplace(size,std::move(icon));
            }
        }

        QIcon icon(const QSize& size) const
        {
            if (size.isNull())
            {
                return icon();
            }

            auto it=m_icons.find(size);
            if (it!=m_icons.end())
            {
                return it->second;
            }
            return m_fallbackIcon;
        }

        QIcon icon(int size) const
        {
            return icon(QSize(size,size));
        }

        void clear()
        {
            m_icons.clear();
        }

        void reset()
        {
            clear();
            m_fallbackIcon=QIcon();
        }

        bool isNull() const
        {
            return m_fallbackIcon.isNull() && m_icons.empty();
        }

        bool isEmpty() const
        {
            return m_icons.empty();
        }

    private:

        std::map<QSize,QIcon,compareQSize> m_icons;
        QIcon m_fallbackIcon;
};

class IconState
{
    public:

        IconState(IconMode mode=IconMode::Normal) : m_mode(static_cast<int>(mode))
        {}

        template <typename T>
        IconState(T val) : m_mode(static_cast<int>(val))
        {}

        IconMode get() const noexcept
        {
            return static_cast<IconMode>(m_mode);
        }

        QIcon::Mode qIconMode() const noexcept
        {
            return static_cast<QIcon::Mode>(m_mode);
        }

        template <typename T>
        T get() const noexcept
        {
            return static_cast<T>(m_mode);
        }

        template <typename T>
        void  set(T val) noexcept
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

    private:

        int m_mode;
};

class UISE_DESKTOP_EXPORT Icon
{
    public:

        Icon() =default;

        Icon(QIcon icon, IconState state=IconMode::Normal, const QSize& size=QSize())
        {
            add(std::move(icon),state,size);
        }

        void add(QIcon icon, IconState state=IconMode::Normal, const QSize& size=QSize())
        {
            auto* set=iconSet(state);
            if (set==nullptr)
            {
                auto it=m_iconSets.emplace(state,IconSet{});
                set=&it.first->second;
            }

            set->addIcon(std::move(icon),size);
        }

        void add(const QPixmap& px, IconState state=IconMode::Normal)
        {
            add(QIcon{px},state,px.size());
        }

        void add(const QString& fileName, IconState state=IconMode::Normal, const QSize& size=QSize())
        {
            add(QIcon{fileName},state,size);
        }

        QIcon get(IconState state=IconMode::Normal, const QSize& size=QSize()) const
        {
            auto* set=iconSet(state);
            if (set==nullptr)
            {
                return m_fallbackIcon;
            }

            if (set->isNull())
            {
                return m_fallbackIcon;
            }

            return set->icon(size);
        }

        QIcon get(int size, IconState state=IconMode::Normal) const
        {
            auto* set=iconSet(state);
            if (set==nullptr)
            {
                return m_fallbackIcon;
            }

            if (set->isNull())
            {
                return m_fallbackIcon;
            }

            return set->icon(size);
        }

        void addMultistate(QIcon icon, const QSize& size=QSize())
        {
            m_multistate.addIcon(std::move(icon),size);
        }

        void addMultistate(const QPixmap& px, IconState state=IconMode::Normal, const QSize& size=QSize())
        {
            auto sz=size;
            if (sz.isNull())
            {
                sz=px.size();
            }

            auto icon=m_multistate.icon(sz);
            icon.addPixmap(px,state.qIconMode());
            m_multistate.addIcon(icon,sz);
        }

        void addMultistate(const QString& filename, const QSize& size, IconState state=IconMode::Normal)
        {
            auto icon=m_multistate.icon(size);
            icon.addFile(filename,size,state.qIconMode());
            m_multistate.addIcon(icon,size);
        }

        void addMultistate(const std::map<IconState,QPixmap>& px, const QSize& size)
        {
            for (const auto& it: px)
            {
                addMultistate(it.second,it.first,size);
            }
        }

        void addMultistate(const std::map<IconState,QPixmap>& px)
        {
            for (const auto& it: px)
            {
                addMultistate(px,it.second.size());
            }
        }

        void addMultistate(const std::map<IconState,QString>& files, const QSize& size=QSize())
        {
            for (const auto& it: files)
            {
                addMultistate(it.second,size,it.first.qIconMode());
            }
        }

        QIcon multistate(const QSize& size=QSize()) const
        {
            if (m_multistate.isNull())
            {
                return m_fallbackIcon;
            }
            return m_multistate.icon(size);
        }

        QIcon multistate(int size) const
        {
            if (m_multistate.isNull())
            {
                return m_fallbackIcon;
            }
            return m_multistate.icon(size);
        }

        void setMultistate(IconSet iconSet)
        {
            m_multistate=std::move(iconSet);
        }

        void addIconSet(IconSet iconSet, IconState state=IconMode::Normal)
        {
            m_iconSets.emplace(state,std::move(iconSet));
        }

        template <typename T>
        const IconSet* iconSet(T state) const
        {
            IconState st(state);
            auto it=m_iconSets.find(st);
            if (it!=m_iconSets.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        template <typename T>
        IconSet* iconSet(T state)
        {
            IconState st(state);
            auto it=m_iconSets.find(st);
            if (it!=m_iconSets.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        bool loadSvg(
            const QString& filename,
            const SizeSet& sizes=SizeSet{},
            const std::map<IconState,std::map<QString,QString>>& colorMaps={}
        );

        bool loadSvg(
            const QString& filename,
            const std::map<QString,QString>& colorMap,
            const SizeSet& sizes=SizeSet{},
            IconState state=IconMode::Normal
            )
        {
            return loadSvg(filename,sizes,{{state,colorMap}});
        }

        bool loadMultistateSvg(
            const QString& filename,
            const std::map<IconState,std::map<QString,QString>>& colorMaps,
            const SizeSet& sizes=SizeSet{}
            );

        void reset()
        {
            m_iconSets.clear();
            m_multistate.reset();
            m_fallbackIcon=QIcon();
        }

    private:

        std::map<IconState,IconSet> m_iconSets;
        IconSet m_multistate;
        QIcon m_fallbackIcon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ICON_HPP
