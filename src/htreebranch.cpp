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

#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreenodelocator.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreebranch.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*****************************HTreeBranch************************************/

class HTreeBranch_p
{
    public:

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

HTreeNode* HTreeBranch::loadNextNode(const HTreePathElement& pathElement)
{
    auto next=nextNode();
    if (next!=nullptr)
    {
        if (next->path().uniqueId()==pathElement.uniqueId())
        {
            next->setExpanded(true);
            return next;
        }
        else
        {
            closeNextNode();
        }
    }

    HTreeNode* nextNode=nullptr;
    auto locator=nextNodeLocator();
    auto nodeResult=locator->findOrCreateNode(pathElement,this,treeTab());
    if (nodeResult.first==nullptr)
    {
        return nullptr;
    }
    if (!nodeResult.second && nodeResult.first->isUnique())
    {
        nodeResult.first->activate();
        return nullptr;
    }

    nextNode=nodeResult.first;
    setNextNodeId(pathElement.uniqueId());
    setNextNode(nextNode);
    nextNode->fillContent();
    treeTab()->appendNode(nextNode);    

    return nextNode;
}

//--------------------------------------------------------------------------

void HTreeBranch::closeNextNode()
{
    treeTab()->closeNode(nextNode());
    setNextNode(nullptr);
    setNextNodeId(std::string{});
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNode(const HTreePathElement& pathElement, bool exclusive)
{
    loadNextNode(pathElement);
    if (exclusive)
    {
        if (nextNode())
        {
            nextNode()->expandExclusive();
        }
    }
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

void HTreeBranch::openNextNodeInNewTree(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        emit treeTab()->tree()->newTreeRequested(residentPath);
    }
    else
    {
        auto p=path();
        p.elements().push_back(pathElement);
        emit treeTab()->tree()->newTreeRequested(p);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
