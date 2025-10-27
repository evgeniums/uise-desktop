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

/** @file uise/desktop/htreepath.hpp
*
*  Declares horizontal tree path.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_PATH_HPP
#define UISE_DESKTOP_HTREE_PATH_HPP

#include <QVariant>

#include <string>
#include <vector>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreePathElementConfig
{
    public:

        HTreePathElementConfig(bool expanded=true, int width=-1)
            : m_expanded(expanded),
              m_width(width)
        {}

        bool expanded() const noexcept
        {
            return m_expanded;
        }

        int width() const noexcept
        {
            return m_width;
        }

        void setExpanded(bool m_expanded) noexcept
        {
            m_expanded=m_expanded;
        }

        void setWidth(int width) noexcept
        {
            m_width=width;
        }

    private:

        bool m_expanded;
        int m_width;
};

class HTreePathElement
{
    public:

        HTreePathElement()=default;

        HTreePathElement(
            std::string type,
            std::string id,
            std::string name={}
        ) : m_type(std::move(type)),
            m_id(std::move(id)),
            m_name(std::move(name))
        {
            updateUniqueId();
        }

        std::string type() const noexcept
        {
            return m_type;
        }

        void setType(std::string type)
        {
            m_type=std::move(type);
        }

        std::string id() const noexcept
        {
            return m_id;
        }

        void setId(std::string id)
        {
            m_type=std::move(id);
            updateUniqueId();
        }

        std::string name() const noexcept
        {
            return m_name;
        }

        void setName(std::string name)
        {
            m_type=std::move(name);
        }

        const HTreePathElementConfig& config() const noexcept
        {
            return m_config;
        }

        void setConfig(HTreePathElementConfig config) noexcept
        {
            m_config=std::move(config);
        }

        bool operator== (const HTreePathElement& other) const noexcept
        {
            return m_type==other.m_type && m_id==other.m_id;
        }

        bool isNull() const noexcept
        {
            return m_id.empty();
        }

        void setData(QVariant data)
        {
            m_data=std::move(data);
        }

        QVariant data() const
        {
            return m_data;
        }

        QVariantMap dataMap() const
        {
            if (!m_data.isNull() && m_data.metaType().id()==QMetaType::QVariantMap)
            {
                return m_data.toMap();
            }
            return QVariantMap{};
        }

        void setData(const QString& key, const QVariant& value)
        {
            auto m=dataMap();
            m.insert(key,value);
            setData(m);
        }

        QVariant data(const QString& key,const QVariant& defaultValue={}) const
        {
            return dataMap().value(key);
        }

        std::string uniqueId() const
        {
            return m_uniqueId;
        }

        static std::string uniqueId(std::string type,std::string id)
        {
            return id.append(type);
        }

    private:

        void updateUniqueId()
        {
            m_uniqueId.clear();
            m_uniqueId.append(m_id).append(m_type);
        }

        std::string m_type;
        std::string m_id;
        std::string m_name;
        std::string m_uniqueId;

        QVariant m_data;

        HTreePathElementConfig m_config;
};

class HTreePathConfig
{
    public:

        HTreePathConfig(int tabIndex=-1)
            : m_tabIndex(tabIndex)
        {}


        int tabIndex() const noexcept
        {
            return m_tabIndex;
        }

        void setTabIndex(int tabIndex) noexcept
        {
            m_tabIndex=tabIndex;
        }

    private:

        int m_tabIndex;
};

class HTreePath
{
    public:

        HTreePath(std::vector<HTreePathElement> elements={}) : m_elements(std::move(elements))
        {}

        HTreePath(HTreePathElement element) : m_elements({std::move(element)})
        {}

        HTreePath(const HTreePath& parent, HTreePathElement element) : m_elements(parent.elements())
        {
            m_elements.emplace_back(std::move(element));
        }

        const std::vector<HTreePathElement>& elements() const noexcept
        {
            return m_elements;
        }

        std::vector<HTreePathElement>& elements() noexcept
        {
            return m_elements;
        }

        void setElements(std::vector<HTreePathElement> elements)
        {
            m_elements=std::move(elements);
        }

        const HTreePathElement* last() const
        {
            if (m_elements.empty())
            {
                return nullptr;
            }
            return &m_elements.back();
        }

        HTreePathElement* last()
        {
            if (m_elements.empty())
            {
                return nullptr;
            }
            return &m_elements.back();
        }

        std::string type() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->type();
        }

        std::string id() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->id();
        }

        std::string uniqueId() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->uniqueId();
        }

        std::string name() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->name();
        }

        QVariant data() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return QVariant{};
            }
            return l->data();
        }

        QVariantMap dataMap() const
        {
            auto l=last();
            if (l==nullptr)
            {
                return QVariantMap{};
            }
            return l->dataMap();
        }

        void setData(const QString& key, const QVariant& value)
        {
            auto l=last();
            if (l==nullptr)
            {
                return;
            }
            l->setData(key,value);
        }

        QVariant data(const QString& key,const QVariant& defaultValue={}) const
        {
            auto l=last();
            if (l==nullptr)
            {
                return QVariant{};
            }
            return l->data(key,defaultValue);
        }

        bool operator== (const HTreePath& other) const noexcept
        {
            return m_elements==other.m_elements;
        }

        bool isNull() const noexcept
        {
            return m_elements.empty();
        }

        void append(HTreePathElement el)
        {
            m_elements.emplace_back(std::move(el));
        }

        HTreePath copyAppend(HTreePathElement el)
        {
            auto p=*this;
            p.m_elements.emplace_back(std::move(el));
            return p;
        }

        bool hasType(std::string_view type) const
        {
            for (const auto& el : m_elements)
            {
                if (el.type()==type)
                {
                    return true;
                }
            }
            return false;
        }

        const HTreePathElement* findByType(std::string_view type) const
        {
            for (const auto& el : m_elements)
            {
                if (el.type()==type)
                {
                    return &el;
                }
            }
            return nullptr;
        }

    private:

        std::vector<HTreePathElement> m_elements;
};

enum class NodeOpenMode
{
    SameTab,
    NewTab,
    NewWindow
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP
