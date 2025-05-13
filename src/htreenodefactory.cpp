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

/** @file uise/desktop/htreenodefactory.cpp
*
*  Defines HTreeNodeFactory.
*
*/

/****************************************************************************/

#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htreenodefactory.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

HTreeNodeFactory::~HTreeNodeFactory()
{}

//--------------------------------------------------------------------------

HTreeNodeBuilder::~HTreeNodeBuilder()
{}

//--------------------------------------------------------------------------

HTreeNode* HTreeNodeFactory::makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode, HTreeTab* treeTab) const
{
    auto node=doMakeNode(pathElement,parentNode,treeTab);
    if (node!=nullptr)
    {
        node->setParentNode(parentNode);
        node->setTreeTab(treeTab);
        HTreePath path;
        if (parentNode!=nullptr)
        {
            path=HTreePath{parentNode->path(),pathElement};
        }
        else
        {
            path=HTreePath{pathElement};
        }
        node->setPath(path);
        return node;
    }

    return nullptr;
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNodeFactory::doMakeNode(const HTreePathElement& pathElement, HTreeNode* parentNode, HTreeTab* treeTab) const
{
    auto b=builder(pathElement.type());
    if (b!=nullptr)
    {
        return b->makeNode(pathElement,parentNode,treeTab);
    }

    return nullptr;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
