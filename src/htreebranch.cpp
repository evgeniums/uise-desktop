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
#include <QDebug>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreenodelocator.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreebranch.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {
//! Same UISE_HTREE_DEBUG gate as htreetab.cpp's own htreeTabDebug() (anonymous-namespaced
//! there, so not reusable from this translation unit) -- temporary, added to trace which
//! branch of loadNextNode() a node-identity change actually takes.
bool htreeBranchDebug()
{
    static bool enabled=qEnvironmentVariableIsSet("UISE_HTREE_DEBUG");
    return enabled;
}
}

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

HTreeNode* HTreeBranch::loadNextNode(const HTreePathElement& pathElement, bool last)
{
    auto next=nextNode();
    if (next!=nullptr)
    {
        if (next->path().uniqueId()==pathElement.uniqueId())
        {
            next->setExpanded(true);
            // The requested node is already the open next node -- setExpanded() above is a
            // no-op in that case, so give action-style nodes (e.g. Quit) a chance to redo
            // their action via reopen()/doReopen(). No-op by default for ordinary nodes.
            next->reopen();
            return next;
        }

        if (next->canReconstructFromPath(pathElement))
        {
            // Reuse next in place instead of destroying it and creating a fresh node (see
            // HTreeNode::reconstructFromPath()'s own doc comment). This is the path taken by a
            // branch's own openNextNode()/openNextNodeInNewTab() slots -- e.g. a list item
            // clicked directly -- which never goes through HTreeTab::openPath() at all, so it
            // needs this same opportunity independently of openPath()'s own reconstruction
            // shortcut. HTreeTab::reconstructNode() closes anything opened deeper than next and
            // updates setNextNodeId()/the navbar/tab-level signals itself.
            bool reconstructed=treeTab()->reconstructNode(next,path().copyAppend(pathElement));
            if (htreeBranchDebug())
            {
                qDebug().noquote() << "loadNextNode reconstruct" << (reconstructed?"OK":"FAILED, falling back to destroy+recreate")
                                    << "for" << QString::fromStdString(pathElement.uniqueId());
            }
            if (reconstructed)
            {
                return next;
            }
        }
        else if (htreeBranchDebug())
        {
            qDebug().noquote() << "loadNextNode: next node not reconstructable, destroying+recreating for"
                                << QString::fromStdString(pathElement.uniqueId());
        }

        closeNextNode();
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

    // Append BEFORE filling. appendNode() hands the node to HTreeSplitterSection::setWidget(),
    // which reparents it -- and with an app-wide stylesheet in effect QWidget::setParent()
    // triggers QWidgetPrivate::inheritStyle(), walking the whole descendant subtree and
    // re-running unpolish()+polish() (a full QSS rule re-match) on every already-polished
    // widget in it. Doing that to an empty node instead of a fully built one is what makes the
    // difference: profiling a Character Info open (Instruments, Qt 6.8) charged ~94 ms of
    // ~348 ms to this single reparent. fillContent() then builds the content directly in its
    // final place.
    //
    // Safe with respect to what appendNode() seeds from the node (name, tooltip, title icon,
    // leading/trailing widgets): those are set by doInit()/doInitNode(), which HTreeNode::init()
    // runs inside findOrCreateNode() above, i.e. before either order reaches this point. Values
    // that do change later still arrive over the signals appendNode() connects, and those
    // connections now exist before fillContent() runs rather than after it. The one thing
    // appendNode() reads that fillContent() genuinely changes is the node's minimum width, which
    // HTreeNode::setContentWidget() reports back via HTreeTab::refreshNodeMinWidth().
    // Note that this makes the node VISIBLE (appendNode() -> ... -> HTreeSplitterInternal::
    // updateWidths() shows the section) before it has any content, so fillContent() below builds
    // into a visible parent. HTreeNode::fillContent() compensates by laying the new subtree out
    // synchronously; without that, Qt only posts a LayoutRequest and a paint can beat it to the
    // screen, showing one frame of half-sized rows. Measured and confirmed, see the comment there.
    treeTab()->appendNode(nextNode);

    if (last || !isExclusivelyExpandable())
    {
        nextNode->fillContent();
    }

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

void HTreeBranch::openNextNode(const HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath, bool exclusive)
{
    if (!residentPath.isNull())
    {
        // Lands via HTree::openPath()/HTreeTab::openPath(), which records history itself.
        treeTab()->tree()->openPath(residentPath);
        return;
    }

    auto* next=loadNextNode(pathElement);
    if (next==nullptr)
    {
        // failure, or a unique node that was already open elsewhere and just got activate()d
        return;
    }
    if (exclusive)
    {
        next->expandExclusive();
    }

    // loadNextNode() never goes through HTreeTab::openPath() -- e.g. a list item clicked
    // directly -- so record the landing here. Unlike openPath()'s own recursive descent, this
    // call always lands on the tab's new, settled last node, never an intermediate prefix.
    treeTab()->recordHistory();
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNodes(const HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        treeTab()->tree()->openPath(residentPath);
    }
    else
    {
        auto p=path().copyAppend(subPath);
        treeTab()->openPath(p);
    }
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNodeInNewTab(const HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        treeTab()->tree()->openPathInNewTab(residentPath,treeTab());
    }
    else
    {
        auto p=path();
        p.elements().push_back(pathElement);
        treeTab()->tree()->openPathInNewTab(p,treeTab());
    }
}

//--------------------------------------------------------------------------

void HTreeBranch::openNextNodesInNewTab(const UISE_DESKTOP_NAMESPACE::HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        treeTab()->tree()->openPathInNewTab(residentPath,treeTab());
    }
    else
    {
        auto p=path().copyAppend(subPath);
        treeTab()->tree()->openPathInNewTab(p,treeTab());
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

void HTreeBranch::openNextNodesInNewTree(const UISE_DESKTOP_NAMESPACE::HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath)
{
    if (!residentPath.isNull())
    {
        emit treeTab()->tree()->newTreeRequested(residentPath);
    }
    else
    {
        auto p=path().copyAppend(subPath);
        emit treeTab()->tree()->newTreeRequested(p);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
