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

/** @file uise/desktop/htreenode.cpp
*
*  Defines HTreeNode.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class HTreeNode_p
{
    public:

        HTreePath path;

        HTreeTab* treeTab=nullptr;
        HTreeNode* parentNode=nullptr;

        QIcon icon;
        QString tooltip;
};

//--------------------------------------------------------------------------

HTreeNode::HTreeNode(HTreeTab* treeTab, QWidget* parent)
    : FrameWithRefresh(parent),
      pimpl(std::make_unique<HTreeNode_p>())
{
    pimpl->treeTab=treeTab;
}

//--------------------------------------------------------------------------

HTreeNode::HTreeNode(QWidget* parent)
    : HTreeNode(nullptr,parent)
{}

//--------------------------------------------------------------------------

HTreeNode::~HTreeNode()
{}

//--------------------------------------------------------------------------

void HTreeNode::setTreeTab(HTreeTab* treeTab)
{
    pimpl->treeTab=treeTab;
}

//--------------------------------------------------------------------------

HTreeTab* HTreeNode::treeTab() const
{
    return pimpl->treeTab;
}

//--------------------------------------------------------------------------

void HTreeNode::setPath(HTreePath path)
{
    pimpl->path=std::move(path);
}

//--------------------------------------------------------------------------

const HTreePath& HTreeNode::path() const
{
    return pimpl->path;
}

//--------------------------------------------------------------------------

void HTreeNode::setParentNode(HTreeNode* node)
{
    pimpl->parentNode=node;
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNode::parentNode() const
{
    return pimpl->parentNode;
}

//--------------------------------------------------------------------------

QString HTreeNode::id() const
{
    return QString::fromStdString(pimpl->path.id());
}

//--------------------------------------------------------------------------

QString HTreeNode::name() const
{
    return QString::fromStdString(pimpl->path.name());
}

//--------------------------------------------------------------------------

QString HTreeNode::tooltip() const
{
    if (pimpl->tooltip.isEmpty())
    {
        return name();
    }
    return pimpl->tooltip;
}

//--------------------------------------------------------------------------

QIcon HTreeNode::icon() const
{
    return pimpl->icon;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
