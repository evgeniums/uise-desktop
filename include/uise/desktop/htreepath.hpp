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
        {}

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

    private:

        std::string m_type;
        std::string m_id;
        std::string m_name;

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

        std::string type() const noexcept
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->type();
        }

        std::string id() const noexcept
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->id();
        }

        std::string name() const noexcept
        {
            auto l=last();
            if (l==nullptr)
            {
                return std::string{};
            }
            return l->name();
        }

        bool operator== (const HTreePath& other) const noexcept
        {
            return m_elements==other.m_elements;
        }

    private:

        std::vector<HTreePathElement> m_elements;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP
