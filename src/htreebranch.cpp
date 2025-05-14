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

/** @file uise/desktop/htreebranch.cpp
*
*  Defines HTreeBranch.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreenodefactory.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreebranch.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*****************************HTreeBranch************************************/

class HTreeBranch_p
{
    public:

        bool expanded=true;

        HTreeNode* nextNode=nullptr;
};

//--------------------------------------------------------------------------

HTreeBranch::HTreeBranch(HTreeTab* treeTab, QWidget* parent)
    : HTreeNode(treeTab,parent),
      pimpl(std::make_unique<HTreeBranch_p>())
{
}

//--------------------------------------------------------------------------

HTreeBranch::HTreeBranch(QWidget* parent)
    : HTreeBranch(nullptr,parent)
{
}

//--------------------------------------------------------------------------

HTreeBranch::~HTreeBranch()
{}

//--------------------------------------------------------------------------

bool HTreeBranch::isExpanded() const
{
    return pimpl->expanded;
}

//--------------------------------------------------------------------------

void HTreeBranch::setExpanded(bool enable)
{
    pimpl->expanded=enable;
    //! @todo Implement expand/collapse tree branch
}

//--------------------------------------------------------------------------

HTreeNode* HTreeBranch::loadNextNode(const HTreePathElement& pathElement)
{
    auto next=nextNode();
    if (next!=nullptr)
    {
        if (next->path().id()==pathElement.id())
        {
            return next;
        }
    }

    closeNextNode();

    setNextNodeId(pathElement.id());
    pimpl->nextNode=treeTab()->tree()->nodeFactory()->makeNode(pathElement,this,treeTab());
    if (pimpl->nextNode!=nullptr)
    {
        treeTab()->appendNode(pimpl->nextNode);
        pimpl->nextNode->refresh();
    }

    return pimpl->nextNode;
}

//--------------------------------------------------------------------------

HTreeNode* HTreeBranch::nextNode() const
{
    return pimpl->nextNode;
}

//--------------------------------------------------------------------------

void HTreeBranch::closeNextNode()
{
    treeTab()->closeNode(nextNode());
    pimpl->nextNode=nullptr;
    setNextNodeId(std::string{});
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNode(const HTreePathElement& pathElement)
{
    loadNextNode(pathElement);
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNodeInNewTab(const HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        treeTab()->tree()->openPath(residentPath,HTree::NewTabIndex);
    }
    else
    {
        auto p=path();
        p.elements().push_back(pathElement);
        treeTab()->tree()->openPath(p,HTree::NewTabIndex);
    }
}

//--------------------------------------------------------------------------

void HTreeBranch::setNextNodeId(const std::string&)
{
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
