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

#include <QFile>
#include <QPointer>
#include <QMenu>
#include <QCursor>
#include <QTimer>

#include <uise/desktop/utils/assert.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* HTreeNodeTitleBar *********************************/

namespace{

auto* iconButton(const QString& iconName, QWidget* parent=nullptr)
{
    auto icon=Style::instance().svgIconLocator().icon(iconName,parent);
    if (icon)
    {
        UiseAssert(icon,"SVG icon must be set in icon theme");
        auto* bt=new PushButton(icon,parent);
        bt->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        return bt;
    }
    return new PushButton(parent);
}

auto* placeholderButton(QWidget* parent=nullptr)
{
    return new PushButton(parent);
}

}

//--------------------------------------------------------------------------

class HTreeNodeTitleBar_p
{
    public:

        HTreeNode* node=nullptr;

        QBoxLayout* layout=nullptr;

        PushButton* close=nullptr;
        PushButton* collapse=nullptr;
        PushButton* refresh=nullptr;
        PushButton* toParentNode=nullptr;
        PushButton* expandExclusive=nullptr;

        ElidedLabel* title=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodeTitleBar::HTreeNodeTitleBar(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodeTitleBar_p>())
{
    pimpl->node=node;

    pimpl->toParentNode=iconButton("HTreeNodeTitleBar::arrow-left",this);

    pimpl->close=iconButton("HTreeNodeTitleBar::close",this);
    pimpl->close->setToolTip(tr("Close this section with all subsequent sections"));

    pimpl->collapse=iconButton("HTreeNodeTitleBar::collapse",this);
    pimpl->collapse->setToolTip(tr("Collapse section"));

    pimpl->expandExclusive=iconButton("HTreeNodeTitleBar::expand",this);
    pimpl->expandExclusive->setToolTip(tr("Maximize section"));

    pimpl->refresh=iconButton("HTreeNodeTitleBar::refresh",this);
    pimpl->refresh->setToolTip(tr("Refresh"));

    pimpl->title=new ElidedLabel(this);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->title->setElideMode(Qt::ElideMiddle);
    pimpl->title->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);

    pimpl->layout=Layout::horizontal(this);

#ifdef Q_OS_MACOS

    pimpl->layout->addWidget(pimpl->toParentNode);
    pimpl->layout->addWidget(pimpl->close);
    pimpl->layout->addWidget(pimpl->collapse);
    pimpl->layout->addWidget(pimpl->expandExclusive);
    pimpl->layout->addWidget(pimpl->refresh);
    pimpl->layout->addWidget(placeholderButton(this));
    pimpl->layout->addWidget(pimpl->title,1);
    pimpl->layout->addWidget(placeholderButton(this));

#else

    pimpl->layout->addWidget(pimpl->toParentNode);
    pimpl->layout->addWidget(placeholderButton(this));
    pimpl->layout->addWidget(pimpl->title,1);
    pimpl->layout->addWidget(placeholderButton(this));
    pimpl->layout->addWidget(pimpl->refresh);
    pimpl->layout->addWidget(pimpl->collapse);
    pimpl->layout->addWidget(pimpl->expandExclusive);
    pimpl->layout->addWidget(pimpl->close);

#endif

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    connect(
        pimpl->close,
        &PushButton::clicked,
        this,
        &HTreeNodeTitleBar::closeRequested
    );
    connect(
        pimpl->collapse,
        &PushButton::clicked,
        this,
        &HTreeNodeTitleBar::collapseRequested
    );
    connect(
        pimpl->refresh,
        &PushButton::clicked,
        this,
        &HTreeNodeTitleBar::refreshRequested
    );
    connect(
        pimpl->toParentNode,
        &PushButton::clicked,
        this,
        &HTreeNodeTitleBar::toParentRequested
    );
    connect(
        pimpl->expandExclusive,
        &PushButton::clicked,
        this,
        &HTreeNodeTitleBar::exclusiveRequested
    );

    pimpl->toParentNode->setVisible(false);
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

        QBoxLayout* layout=nullptr;
        PushButton* expand=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodePlaceHolder::HTreeNodePlaceHolder(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodePlaceHolder_p>())
{
    pimpl->node=node;

    pimpl->layout=Layout::vertical(this);
    pimpl->expand=iconButton("HTreeNodePlaceHolder::dots",this);
    pimpl->expand->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    pimpl->layout->addWidget(pimpl->expand,1);

    connect(
        pimpl->expand,
        &PushButton::clicked,
        this,
        &HTreeNodePlaceHolder::expandRequested
    );
}

//--------------------------------------------------------------------------

HTreeNodePlaceHolder::~HTreeNodePlaceHolder()
{}

/****************************** HTreeNode_p *********************************/

//--------------------------------------------------------------------------

class HTreeNode_p
{
    public:

        HTreeNode* node;

        HTreePath path;

        HTreeTab* treeTab=nullptr;
        HTreeNode* parentNode=nullptr;

        QIcon icon;
        QString tooltip;

        QFrame* mainFrame=nullptr;
        QFrame* content=nullptr;
        QBoxLayout* layout=nullptr;
        HTreeNodeTitleBar* titleBar=nullptr;
        HTreeNodePlaceHolder* placeHolder=nullptr;
        QPointer<QWidget> widget;

        bool expanded=true;
        bool collapsible=true;
        bool refreshable=true;
        bool closable=true;
        bool closeEnabled=true;

        bool unique=false;

        int prevMinWidth=0;
        int prevMaxWidth=0;

        QPointer<HTreeNode> nextNode;
        bool prepareForDestroy=false;

        HTreeNodeLocator* nextNodeLocator=nullptr;

        bool collapsiblePlaceholderVisible=true;
        bool loaded=false;

        void setCollapsePlaceholderVisible(bool enable)
        {
            collapsiblePlaceholderVisible=enable;
            if (enable)
            {
                node->setVisible(true);
                placeHolder->setVisible(true);
                node->setFixedWidth(placeHolder->maximumWidth());
            }
            else
            {
                node->setFixedWidth(0);
                node->setVisible(false);
            }
            updatePlaceholderTooltip();
        }

        void updatePlaceholderTooltip()
        {
            if (node->treeTab()->isSingleCollapsePlaceholder() && node->nextNode() && !node->nextNode()->isExpanded())
            {
                placeHolder->pimpl->expand->setToolTip(QObject::tr("Expand","HTreeNode"));
            }
            else
            {
                placeHolder->pimpl->expand->setToolTip(tooltip);
            }
        }
};

//--------------------------------------------------------------------------

HTreeNode::HTreeNode(HTreeTab* treeTab, QWidget* parent)
    : FrameWithRefresh(parent),
      pimpl(std::make_unique<HTreeNode_p>())
{
    pimpl->node=this;
    pimpl->treeTab=treeTab;

    auto l=Layout::horizontal(this);

    pimpl->placeHolder=new HTreeNodePlaceHolder(this);
    l->addWidget(pimpl->placeHolder);

    pimpl->mainFrame=new QFrame(this);
    auto l1=Layout::vertical(pimpl->mainFrame);
    l->addWidget(pimpl->mainFrame);

    pimpl->titleBar=new HTreeNodeTitleBar(this);
    l1->addWidget(pimpl->titleBar);

    pimpl->content=new QFrame(this);
    l1->addWidget(pimpl->content);
    pimpl->layout=Layout::horizontal(pimpl->content);

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
        pimpl->titleBar,
        &HTreeNodeTitleBar::toParentRequested,
        this,
        &HTreeNode::expandParentNode
    );
    connect(
        pimpl->titleBar,
        &HTreeNodeTitleBar::exclusiveRequested,
        this,
        [this]()
        {
            expandExclusive();
        }
    );

    connect(
        pimpl->placeHolder,
        &HTreeNodePlaceHolder::expandRequested,
        this,
        &HTreeNode::onPlaceHolderExpandRequest
    );

    setCollapsible(false);
    setClosable(false);
    setRefreshable(false);
    pimpl->titleBar->pimpl->expandExclusive->setVisible(false);
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
    pimpl->titleBar->pimpl->title->setText(QString::fromStdString(path.name()));
    pimpl->path=std::move(path);

    setClosable(pimpl->path.elements().size()>1);
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
    if (pimpl->parentNode)
    {
        setParentNodeTitle(node->name());
    }

    updateExclusivelyExpandable();
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNode::parentNode() const
{
    return pimpl->parentNode;
}

//--------------------------------------------------------------------------

QString HTreeNode::id() const
{
    return QString::fromStdString(pimpl->path.uniqueId());
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
    pimpl->titleBar->pimpl->title->setText(val);
    pimpl->path.elements().back().setName(val.toStdString());
    if (nextNode())
    {
        nextNode()->setParentNodeTitle(val);
    }
    emit nameUpdated(val);
}

//--------------------------------------------------------------------------

void HTreeNode::setNodeTooltip(const QString& val)
{
    pimpl->tooltip=val;
    pimpl->titleBar->pimpl->title->setToolTip(val);
    pimpl->updatePlaceholderTooltip();
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
    pimpl->widget=widget;
    pimpl->layout->addWidget(widget);

    setMinimumWidth(widget->minimumWidth());
    setMaximumWidth(widget->maximumWidth());
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
    if (nextNode()==nullptr)
    {
        return;
    }

    if (!isExclusivelyExpandable())
    {
        pimpl->prevMinWidth=minimumWidth();
        destroyWidget(pimpl->widget);
        pimpl->loaded=false;
    }

    pimpl->expanded=false;    
    pimpl->mainFrame->setVisible(false);

    auto emitExpanded=updateCollapsePlaceholder();
    if (emitExpanded)
    {
        emit toggleExpanded(false);
    }

    if (nextNode())
    {
        nextNode()->updateCollapsePlaceholder();
    }
    if (parentNode())
    {
        parentNode()->updateCollapsePlaceholderTooltip();
    }
}

//--------------------------------------------------------------------------

void HTreeNode::expandNode()
{
    pimpl->expanded=true;
    fillContent();

    if (nextNode())
    {
        nextNode()->updateCollapsePlaceholder();
    }
    if (parentNode())
    {
        parentNode()->updateCollapsePlaceholderTooltip();
    }

    emit toggleExpanded(true);

    if (isExclusivelyExpandable())
    {
        expandExclusive();
    }
}

//--------------------------------------------------------------------------

void HTreeNode::fillContent()
{
    setVisible(true);
    pimpl->placeHolder->setVisible(false);

    if (!pimpl->widget)
    {
        setContentWidget(createContentWidget());
        pimpl->mainFrame->setVisible(true);
    }
    else
    {
        pimpl->mainFrame->setVisible(true);
        setMinimumWidth(pimpl->widget->minimumWidth());
        setMaximumWidth(pimpl->widget->maximumWidth());
    }

    if (isExclusivelyExpandable())
    {
        expandExclusive();
        QTimer::singleShot(0,this,[this](){treeTab()->adjustWidthsAndPositions();});
    }

    if (!pimpl->loaded)
    {
        pimpl->loaded=true;
        refresh();
    }
}

//--------------------------------------------------------------------------

bool HTreeNode::isExpanded() const
{
    return pimpl->expanded;
}

//--------------------------------------------------------------------------

void HTreeNode::setExpanded(bool enable)
{
    if (pimpl->expanded==enable)
    {
        return;
    }

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

void HTreeNode::setNextNode(HTreeNode* node)
{
    if (pimpl->nextNode!=nullptr)
    {
        disconnect(
            pimpl->nextNode,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(nextNodeDestroyed(QObject*))
        );
    }

    pimpl->nextNode=node;
    if (node!=nullptr)
    {
        setCollapsible(true);
        connect(
            node,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(nextNodeDestroyed(QObject*))
        );

        auto n=this;
        while (n!=nullptr)
        {
            connect(
                node,
                SIGNAL(toggleExpanded(bool)),
                n,
                SLOT(otherNodeExpanded(bool))
            );
            connect(
                n,
                SIGNAL(toggleExpanded(bool)),
                node,
                SLOT(otherNodeExpanded(bool))
            );
            n=n->parentNode();
        }
    }
    else
    {
        setCollapsible(false);
    }

    updateExclusivelyExpandable();
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNode::nextNode() const
{
    return pimpl->nextNode;
}

//--------------------------------------------------------------------------

void HTreeNode::prepareForDestroy()
{
    pimpl->prepareForDestroy=true;
    if (pimpl->nextNode!=nullptr)
    {
        disconnect(
            pimpl->nextNode,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(nextNodeDestroyed(QObject*))
            );
    }
}

//--------------------------------------------------------------------------

void HTreeNode::nextNodeDestroyed(QObject* obj)
{
    if (pimpl->prepareForDestroy)
    {
        return;
    }

    if (!pimpl->nextNode || obj==pimpl->nextNode)
    {
        pimpl->nextNode =nullptr;
        setExpanded(true);
        setCollapsible(false);
    }

    updateExclusivelyExpandable();
    setNextNodeId(std::string{});
}

//--------------------------------------------------------------------------

void HTreeNode::otherNodeExpanded(bool enable)
{
    if (pimpl->prepareForDestroy)
    {
        return;
    }

    auto p=parentNode();
    if (p)
    {
        pimpl->titleBar->pimpl->toParentNode->setVisible(!p->isExpanded());
    }

    setClosable(p!=nullptr);
    setCollapsible(nextNode()!=nullptr && isAtListOneNodeExpanded());
    updateExclusivelyExpandable();
}

//--------------------------------------------------------------------------

void HTreeNode::setCollapsible(bool enable)
{
    pimpl->collapsible=enable && !isExclusivelyExpandable();
    pimpl->titleBar->pimpl->collapse->setVisible(pimpl->collapsible);
}

//--------------------------------------------------------------------------

bool HTreeNode::isCollapsible() const
{
    return pimpl->collapsible;
}

//--------------------------------------------------------------------------

void HTreeNode::setClosable(bool enable)
{
    pimpl->closable=enable && !isExclusivelyExpandable();
    pimpl->titleBar->pimpl->close->setVisible(pimpl->closable && pimpl->closeEnabled);
}

//--------------------------------------------------------------------------

bool HTreeNode::isClosable() const
{
    return pimpl->closable;
}

//--------------------------------------------------------------------------

void HTreeNode::setRefreshable(bool enable)
{
    pimpl->refreshable=enable;
    pimpl->titleBar->pimpl->refresh->setVisible(enable);
}

//--------------------------------------------------------------------------

bool HTreeNode::isRefreshable() const
{
    return pimpl->refreshable;
}

//--------------------------------------------------------------------------

void HTreeNode::setTitleBarVisible(bool enable)
{
    pimpl->titleBar->setVisible(enable);
}

//--------------------------------------------------------------------------

bool HTreeNode::isTitleBarVisible() const
{
    return pimpl->titleBar->isVisible();
}

//--------------------------------------------------------------------------

void HTreeNode::setNextNodeId(const std::string&)
{
    // intentionally empty
}

//--------------------------------------------------------------------------

void HTreeNode::activate()
{
    treeTab()->activate();
    setExpanded(true);
    treeTab()->scrollToNode(this);
}

//--------------------------------------------------------------------------

void HTreeNode::setUnique(bool enable)
{
    pimpl->unique=enable;
}

//--------------------------------------------------------------------------

bool HTreeNode::isUnique() const
{
    return pimpl->unique;
}

//--------------------------------------------------------------------------

void HTreeNode::setNextNodeLocator(HTreeNodeLocator* locator)
{
    pimpl->nextNodeLocator=locator;
}

//--------------------------------------------------------------------------

HTreeNodeLocator* HTreeNode::nextNodeLocator() const
{
    if (pimpl->nextNodeLocator!=nullptr)
    {
        return pimpl->nextNodeLocator;
    }

    auto prevNode=parentNode();
    if (prevNode!=nullptr)
    {
        return prevNode->nextNodeLocator();
    }

    return pimpl->treeTab->tree()->nodeLocator();
}

//--------------------------------------------------------------------------

void HTreeNode::setCloseEnabled(bool enable)
{
    pimpl->closeEnabled=enable;
    setClosable(isClosable());
}

//--------------------------------------------------------------------------

bool HTreeNode::isCloseEnabled() const
{
    return pimpl->closeEnabled;
}

//--------------------------------------------------------------------------

bool HTreeNode::updateCollapsePlaceholder()
{
    if (!isExpanded())
    {
        if (treeTab()->isSingleCollapsePlaceholder())
        {
            bool visiblePlaceholder=true;
            auto pNode=parentNode();
            if (pNode)
            {
                if (!pNode->isExpanded())
                {
                    visiblePlaceholder=false;
                }
            }
            pimpl->setCollapsePlaceholderVisible(visiblePlaceholder);
            emit toggleExpanded(false);
            return false;
        }

        pimpl->setCollapsePlaceholderVisible(true);
    }
    return true;
}

//--------------------------------------------------------------------------

bool HTreeNode::isNodeVisible() const
{
    if (treeTab()->isSingleCollapsePlaceholder() && !isExpanded())
    {
        return pimpl->collapsiblePlaceholderVisible;
    }

    return true;
}

//--------------------------------------------------------------------------

void HTreeNode::onPlaceHolderExpandRequest()
{
    // expand this node
    if (!treeTab()->isSingleCollapsePlaceholder() || !nextNode() || nextNode()->isExpanded())
    {
        expandNode();
        return;
    }

    // show menu with list of collapsed nodes
    auto menu=new QMenu(this);
    auto n=this;
    while (n && !n->isExpanded())
    {
        auto action=menu->addAction(n->name());
        action->setData(reinterpret_cast<quint64>(n));
        n=n->nextNode();
    }
    auto action=menu->exec(QCursor::pos());

    // expand selected node
    if (action!=nullptr)
    {
        auto selectedNode=reinterpret_cast<HTreeNode*>(action->data().toULongLong());
        selectedNode->setExpanded(true);
    }
}

//--------------------------------------------------------------------------

void HTreeNode::updateCollapsePlaceholderTooltip()
{
    pimpl->updatePlaceholderTooltip();
}

//--------------------------------------------------------------------------

void HTreeNode::init()
{
    doInit();
    emit initRequested();
}

//--------------------------------------------------------------------------

void HTreeNode::expandParentNode()
{
    if (parentNode())
    {
        parentNode()->setExpanded(true);
    }
}

//--------------------------------------------------------------------------

void HTreeNode::setParentNodeTitle(const QString& title)
{
    pimpl->titleBar->pimpl->toParentNode->setToolTip(title);
}

//--------------------------------------------------------------------------

void HTreeNode::expandExclusive(int depth)
{    
    auto p=parentNode();
    while (p!=nullptr)
    {
        auto collapseParent=depth<=0;
        p->setExpanded(!collapseParent);
        p=p->parentNode();
        --depth;
    }
    auto n=nextNode();
    if (n!=nullptr)
    {
        n->closeNode();
    }
}

//--------------------------------------------------------------------------

bool HTreeNode::isAtListOneNodeExpanded() const
{
    bool atLeastOneExpanded=false;
    auto p=parentNode();
    while (p!=nullptr)
    {
        if (p->isExpanded())
        {
            atLeastOneExpanded=true;
            break;
        }
        p=p->parentNode();
    }
    if (!atLeastOneExpanded)
    {
        auto n=pimpl->nextNode.get();
        while(n!=nullptr)
        {
            if (n->isExpanded())
            {
                atLeastOneExpanded=true;
                break;
            }
            n=n->nextNode();
        }
    }
    return atLeastOneExpanded;
}

//--------------------------------------------------------------------------

void HTreeNode::updateExclusivelyExpandable()
{
    pimpl->titleBar->pimpl->expandExclusive->setVisible(
        parentNode() != nullptr && isAtListOneNodeExpanded() && !isExclusivelyExpandable()
    );
}

//--------------------------------------------------------------------------

bool HTreeNode::isExclusivelyExpandable() const
{
    return treeTab()->tree()->isExlusivelyExpandableNode();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
