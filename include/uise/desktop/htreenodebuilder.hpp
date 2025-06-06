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

/** @file uise/desktop/htreenodebuilder.hpp
*
*  Declares horizontal tree node builder.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_NODE_BUILDER_HPP
#define UISE_DESKTOP_HTREE_NODE_BUILDER_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeTab;
class HTreeNode;

class UISE_DESKTOP_EXPORT HTreeNodeBuilder
{
    public:

        HTreeNodeBuilder()=default;
        virtual ~HTreeNodeBuilder();

        HTreeNodeBuilder(const HTreeNodeBuilder&)=default;
        HTreeNodeBuilder(HTreeNodeBuilder&&)=default;
        HTreeNodeBuilder& operator= (const HTreeNodeBuilder&)=default;
        HTreeNodeBuilder& operator= (HTreeNodeBuilder&&)=default;

        virtual HTreeNode* makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const=0;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_NODE_BUILDER_HPP
