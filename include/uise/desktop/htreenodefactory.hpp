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

/** @file uise/desktop/htreenodefactory.hpp
*
*  Declares horizontal tree node factory.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_NODE_FACTORY_HPP
#define UISE_DESKTOP_HTREE_NODE_FACTORY_HPP

#include <map>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeTab;
class HTreeNode;
class HTreeNodeBuilder;

class UISE_DESKTOP_EXPORT HTreeNodeFactory
{
    public:

        HTreeNodeFactory()=default;
        virtual ~HTreeNodeFactory();

        HTreeNodeFactory(const HTreeNodeFactory&)=default;
        HTreeNodeFactory(HTreeNodeFactory&&)=default;
        HTreeNodeFactory& operator= (const HTreeNodeFactory&)=default;
        HTreeNodeFactory& operator= (HTreeNodeFactory&&)=default;

        HTreeNode* makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const;

        void registerBuilder(std::string type, std::shared_ptr<HTreeNodeBuilder> builder)
        {
            m_builders.emplace(std::move(type),std::move(builder));
        }

        const HTreeNodeBuilder* builder(const std::string& type) const
        {
            auto it=m_builders.find(type);
            if (it!=m_builders.end())
            {
                return it->second.get();
            }
            return nullptr;
        }

    protected:

        virtual HTreeNode* doMakeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const;

    private:

        std::map<std::string,std::shared_ptr<HTreeNodeBuilder>> m_builders;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_NODE_FACTORY_HPP
