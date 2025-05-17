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

/********************* HTreeNodeTitleBar *********************************/

//--------------------------------------------------------------------------

class HTreeNodeTitleBar_p
{
    public:

        HTreeNode* node=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodeTitleBar::HTreeNodeTitleBar(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodeTitleBar_p>())
{
    pimpl->node=node;
}

//--------------------------------------------------------------------------

HTreeNodeTitleBar::~HTreeNodeTitleBar()
{}

/********************* HTreeNodePlaceHolder *********************************/

//--------------------------------------------------------------------------

class HTreeNodePlaceHolder_p
{
    public:

        HTreeNode* node=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodePlaceHolder::HTreeNodePlaceHolder(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodePlaceHolder_p>())
{
    pimpl->node=node;
}

//--------------------------------------------------------------------------

HTreeNodePlaceHolder::~HTreeNodePlaceHolder()
{}

/****************************** HTreeNode_p *********************************/

//--------------------------------------------------------------------------

class HTreeNode_p
{
    public:

        HTreePath path;

        HTreeTab* treeTab=nullptr;
        HTreeNode* parentNode=nullptr;

        QIcon icon;
        QString tooltip;

        QFrame* content=nullptr;
        QBoxLayout* layout=nullptr;
        HTreeNodeTitleBar* titleBar=nullptr;
        HTreeNodePlaceHolder* placeHolder=nullptr;
        QWidget* widget=nullptr;

        bool expanded=true;
};

//--------------------------------------------------------------------------

HTreeNode::HTreeNode(HTreeTab* treeTab, QWidget* parent)
    : FrameWithRefresh(parent),
      pimpl(std::make_unique<HTreeNode_p>())
{
    pimpl->treeTab=treeTab;

    auto l=Layout::vertical(this);

    pimpl->titleBar=new HTreeNodeTitleBar(this);
    l->addWidget(pimpl->titleBar);

    pimpl->content=new QFrame(this);
    l->addWidget(pimpl->content);

    pimpl->layout=Layout::horizontal(pimpl->content);

    pimpl->placeHolder=new HTreeNodePlaceHolder(this);
    pimpl->layout->addWidget(pimpl->placeHolder);

    connect(
        pimpl->titleBar,
        &HTreeNodeTitleBar::closeRequested,
        this,
        &HTreeNode::closeNode
    );
    connect(
        pimpl->titleBar,
        &HTreeNodeTitleBar::collapseRequested,
        this,
        &HTreeNode::collapseNode
    );
    connect(
        pimpl->titleBar,
        &HTreeNodeTitleBar::refreshRequested,
        this,
        &HTreeNode::refresh
    );

    connect(
        pimpl->placeHolder,
        &HTreeNodePlaceHolder::expandRequested,
        this,
        &HTreeNode::expandNode
    );
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

QString HTreeNode::nodeTooltip() const
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

void HTreeNode::setNodeName(const QString& val)
{
    pimpl->path.elements().back().setName(val.toStdString());
    emit nameUpdated(val);
}

//--------------------------------------------------------------------------

void HTreeNode::setNodeTooltip(const QString& val)
{
    pimpl->tooltip=val;
    emit tooltipUpdated(val);
}

//--------------------------------------------------------------------------

void HTreeNode::setNodeIcon(const QIcon& val)
{
    pimpl->icon=val;
    emit iconUpdated(val);
}

//--------------------------------------------------------------------------

void HTreeNode::setContentWidget(QWidget* widget)
{
    pimpl->placeHolder->setVisible(false);

    pimpl->widget=widget;
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

QWidget* HTreeNode::contentWidget() const
{
    return pimpl->widget;
}

//--------------------------------------------------------------------------

void HTreeNode::closeNode()
{
    treeTab()->closeNode(this);
}

//--------------------------------------------------------------------------

void HTreeNode::collapseNode()
{
    pimpl->placeHolder->setVisible(true);
    destroyWidget(pimpl->widget);
}

//--------------------------------------------------------------------------

void HTreeNode::expandNode()
{
    setContentWidget(createContentWidget());
    refresh();
}

//--------------------------------------------------------------------------

bool HTreeNode::isExpanded() const
{
    return pimpl->expanded;
}

//--------------------------------------------------------------------------

void HTreeNode::setExpanded(bool enable)
{
    pimpl->expanded=enable;
    if (enable)
    {
        expandNode();
    }
    else
    {
        collapseNode();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
