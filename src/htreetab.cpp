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

/** @file uise/desktop/htreetab.hpp
*
*  Defines HTreeTab.
*
*/

/****************************************************************************/

#include <QTextBrowser>

#include <QScrollBar>
#include <QSplitter>
#include <QSignalMapper>
#include <QDateTime>
#include <QDebug>

#include <uise/desktop/utils/assert.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/navigationbar.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htreebranch.hpp>
#include <uise/desktop/htreenodelocator.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreesplitter.hpp>
#include <uise/desktop/htreetab.hpp>

#include <uise/desktop/detail/htreesplitter_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

bool htreeTabDebug()
{
    static bool enabled=qEnvironmentVariableIsSet("UISE_HTREE_DEBUG");
    return enabled;
}

QString htreeTabDebugTs()
{
    return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
}

//! Whether \p prefix names a strict ancestor of \p path (element-wise, so type+id only).
bool isPathAncestor(const HTreePath& prefix, const HTreePath& path)
{
    const auto& prefixElements=prefix.elements();
    const auto& pathElements=path.elements();
    if (prefixElements.empty() || prefixElements.size()>=pathElements.size())
    {
        return false;
    }
    for (size_t i=0;i<prefixElements.size();i++)
    {
        if (!(prefixElements.at(i)==pathElements.at(i)))
        {
            return false;
        }
    }
    return true;
}

}

//--------------------------------------------------------------------------

class HTreeTab_p
{
    public:

        HTreeTab* self;
        HTree* tree;

        NavigationBar* navbar=nullptr;

        HTreeSplitter* splitter=nullptr;

        QSignalMapper* nodeDestroyedMapper;

        std::vector<HTreeNode*> nodes;
        SingleShotTimer* scrollTimer;
        SingleShotTimer* reconfigureTimer;

        std::vector<HTreePath> history;
        int historyPos=-1;
        bool historyNavigating=false;

        bool closeWarnDisable=false;
        QFrame* closeWarnFrame;
        SingleShotTimer* closeWarnTimer;
        SingleShotTimer* closeWarnDisableTimer;

        QIcon tabIconOverride;
        bool tabIconOverrideSet=false;
        QString tabTooltipOverride;
        bool tabTooltipOverrideSet=false;

        ~HTreeTab_p()
        {
            for (auto& node : nodes)
            {
                if (node)
                {
                    disconnectNode(node,true);
                    // do not call argument-less node->disconnect() here: it would sever
                    // Qt's internal destroyed->QStyleSheetStyleCaches connection, leaving
                    // a dangling pointer in the stylesheet caches that crashes the next
                    // qApp->setStyleSheet()
                }
            }
        }

        void appendNode(HTreeNode* node);
        void updateLastNode();
        void disconnectNode(HTreeNode* node, bool beforeDestroy);
        void truncate(int index);
        void scrollToNode(HTreeNode* node);
        void scrollToEnd();
        bool reconstructLastNode(int index, HTreePath path);
        bool doOpenPath(HTreePath path);
        void doRecordHistory(HTreeTab::HistoryMode historyMode);
        bool navigateHistory(int index, int direction);
        void expandLandingColumns();
        HTreeNode* collapsedParentHost() const;
        void syncHistoryCursorTo(const HTreePath& path);
};

//--------------------------------------------------------------------------

void HTreeTab_p::scrollToEnd()
{
    splitter->scrollToEnd();
}

//--------------------------------------------------------------------------

void HTreeTab_p::scrollToNode(HTreeNode* node)
{
    splitter->scrollToIndex(node->path().elements().size()-1);
}

//--------------------------------------------------------------------------

void HTreeTab_p::disconnectNode(HTreeNode* node, bool beforeDestroy)
{
    if (node!=nullptr)
    {
        node->disconnect(
            node,
            &HTreeNode::nameUpdated,
            self,
            &HTreeTab::nameUpdated
            );
        node->disconnect(
            node,
            &HTreeNode::tooltipUpdated,
            self,
            &HTreeTab::tooltipUpdated
            );
        node->disconnect(
            node,
            &HTreeNode::iconUpdated,
            self,
            &HTreeTab::iconUpdated
        );
        node->disconnect(
            node,
            &HTreeNode::closeHovered,
            self,
            &HTreeTab::nodeCloseHovered
        );

        if (beforeDestroy)
        {
            node->prepareForDestroy();

            nodeDestroyedMapper->removeMappings(node);
            node->disconnect(
                node,
                SIGNAL(destroyed()),
                nodeDestroyedMapper,
                SLOT(map())
            );
        }
    }
}

//--------------------------------------------------------------------------

void HTreeTab_p::truncate(int index)
{
    if (index<0 || index>=static_cast<int>(nodes.size()))
    {
        return;
    }

    int lastIndex=static_cast<int>(nodes.size())-1;
    for (int i=lastIndex;i>=index;i--)
    {
        auto w=splitter->widget(i);
        auto s=qobject_cast<HTreeSplitterSection*>(w);
        if (s!=nullptr)
        {
            auto n=qobject_cast<HTreeNode*>(s->widget());
            if (n!=nullptr)
            {
                if (i==index)
                {
                    n->informForDestroy();
                }
                disconnectNode(n,true);
            }
        }
    }
    splitter->truncate(index);
    nodes.resize(index);

    navbar->blockSignals(true);
    navbar->truncate(index);
    navbar->blockSignals(false);

    updateLastNode();
}

//--------------------------------------------------------------------------

void HTreeTab_p::appendNode(HTreeNode* node)
{
    if (!nodes.empty())
    {
        auto lastNode=nodes.back();
        node->setParentNode(lastNode);
        disconnectNode(lastNode,false);
    }

    // add to nodes
    nodes.push_back(node);

    // add widget to splitter
    splitter->addWidget(node);
    auto index=splitter->count()-1;

    // add item to navigation bar
    // Pass the node's already-known title icon (if any) at creation time -- without it,
    // NavigationBarItem is built with IconPosition::Invisible (no icon slot at all) and the
    // icon only arrives later over titleIconUpdated, once the connection below is made. That
    // gap is what makes a node's status icon (e.g. character online/offline) flicker in on
    // every navbar item recreation.
    navbar->addItem(node->name(),node->nodeTooltip(),node->id(),node->titleIcon());
    navbar->blockSignals(true);
    navbar->setItemChecked(index,node->isExpanded());

    if (node->path().last()->config().isForceVisibleInNavbar())
    {
        navbar->setForceSingleItemVisible(index,true);
    }

    navbar->blockSignals(false);
    node->connect(
        node,
        &HTreeNode::nameUpdated,
        self,
        [this,index](const QString& val)
        {
            navbar->setItemName(index,val);
            self->emitNodesReconfigured();
        }
    );
    node->connect(
        node,
        &HTreeNode::tooltipUpdated,
        self,
        [this,index](const QString& val)
        {
            navbar->setItemTooltip(index,val);
        }
    );
    node->connect(
        node,
        &HTreeNode::toggleExpanded,
        self,
        [this,index,node](bool enable)
        {
            navbar->blockSignals(true);
            navbar->setItemChecked(index,enable);
            navbar->blockSignals(false);

            auto w=splitter->widget(index);
            if (w!=nullptr)
            {
                w->setMinimumWidth(node->minimumWidth()+splitter->sectionLineWidth());
                w->setMaximumWidth(std::min(QWIDGETSIZE_MAX,(node->maximumWidth()+splitter->sectionLineWidth())));
                splitter->toggleSectionExpanded(index,enable,node->isNodeVisible());
            }

            self->emitNodesReconfigured();
        }
    );
    node->connect(
        node,
        &HTreeNode::closeHovered,
        self,
        [this](UISE_DESKTOP_NAMESPACE::HTreeNode* node, bool enable)
        {
            self->nodeCloseHovered(node,enable);
        }
    );
    node->connect(
        node,
        &HTreeNode::titleIconUpdated,
        self,
        [this,index](std::shared_ptr<SvgIcon> icon)
        {
            navbar->setItemIcon(index,std::move(icon));
        }
    );
    node->connect(
        node,
        &HTreeNode::trailingIconUpdated,
        self,
        [this,index](std::shared_ptr<SvgIcon> icon)
        {
            navbar->setItemTrailingIcon(index,std::move(icon));
        }
    );

    // update last node
    updateLastNode();

    // watch node destroying
    node->connect(
        node,
        SIGNAL(destroyed()),
        nodeDestroyedMapper,
        SLOT(map())
    );
    nodeDestroyedMapper->setMapping(node,static_cast<int>(nodes.size()-1));

    if (tree->expandableLastDepthOnNodeOpen()!=0)
    {
        node->expandExclusive(tree->expandableLastDepthOnNodeOpen());
    }
    else
    {
        scrollTimer->shot(50,
            [this,node=QPointer<HTreeNode>{node}]
            {
                if (node)
                {
                    if (htreeTabDebug())
                    {
                        qDebug().noquote() << htreeTabDebugTs() << "DEFERRED(50ms) appendNode scrollToNode";
                    }
                    scrollToNode(node);
                }
            },
            true
        );
    }

    self->emitNodesReconfigured();
}

//--------------------------------------------------------------------------

void HTreeTab_p::updateLastNode()
{
    if (nodes.empty())
    {
        return;
    }
    HTreeNode* lastNode=nodes.back();

    // signal that last node is updated
    emit self->nodeUpdated(lastNode->path());
    emit self->nameUpdated(lastNode->name());
    emit self->tooltipUpdated(lastNode->nodeTooltip());
    emit self->iconUpdated(lastNode->icon());

    // connect last node
    lastNode->connect(
        lastNode,
        &HTreeNode::nameUpdated,
        self,
        &HTreeTab::nameUpdated
    );
    lastNode->connect(
        lastNode,
        &HTreeNode::tooltipUpdated,
        self,
        &HTreeTab::tooltipUpdated
    );
    lastNode->connect(
        lastNode,
        &HTreeNode::iconUpdated,
        self,
        &HTreeTab::iconUpdated
    );
}

//--------------------------------------------------------------------------

bool HTreeTab_p::reconstructLastNode(int index, HTreePath path)
{
    auto* cand=nodes.at(static_cast<size_t>(index));

    if (htreeTabDebug())
    {
        qDebug().noquote() << htreeTabDebugTs() << "reconstructLastNode index" << index
                            << "from" << QString::fromStdString(cand->path().uniqueId())
                            << "to" << QString::fromStdString(path.uniqueId());
    }

    // close/destroy any nodes that were opened deeper than the reconstructed one; a no-op
    // when there is nothing deeper (see the index guard in HTreeTab_p::truncate())
    self->truncate(static_cast<int>(path.elements().size()));

    auto lastElement=path.elements().back();
    cand->reconstructFromPath(std::move(path));

    if (cand->parentNode()!=nullptr)
    {
        // HTreeBranch::loadNextNode() compares against this on the next navigation, so it
        // must track the reconstructed node's new identity or the branch would believe a
        // stale/wrong node is already open
        cand->parentNode()->setNextNodeId(lastElement.uniqueId());
    }

    navbar->blockSignals(true);
    navbar->setItemId(index,cand->id());
    navbar->setItemIcon(index,cand->titleIcon());
    navbar->blockSignals(false);

    // cand may or may not still carry the tab-level signal wiring from when it was last
    // appended (it does if it was never superseded by a deeper node, it doesn't if it was and
    // that deeper node was just truncated above) - disconnect unconditionally so updateLastNode()
    // below cannot create a duplicate connection
    disconnectNode(cand,false);
    updateLastNode();

    self->emitNodesReconfigured();
    scrollToNode(cand);

    return true;
}

//--------------------------------------------------------------------------

bool HTreeTab::reconstructNode(HTreeNode* node, HTreePath path)
{
    if (node==nullptr)
    {
        return false;
    }

    for (size_t i=0;i<pimpl->nodes.size();i++)
    {
        if (pimpl->nodes.at(i)==node)
        {
            return pimpl->reconstructLastNode(static_cast<int>(i),std::move(path));
        }
    }

    return false;
}

//--------------------------------------------------------------------------

HTreeTab::HTreeTab(HTree* tree, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeTab_p>())
{
    pimpl->self=this;
    pimpl->tree=tree;
    pimpl->scrollTimer=new SingleShotTimer(this);
    pimpl->reconfigureTimer=new SingleShotTimer(this);

    auto l=Layout::vertical(this);

    auto sep=new NavigationBarSeparator(this);
    sep->setHoverCharacterEnabled(pimpl->tree->isInternalNodeExpandable());
    sep->setHoverCharacterClickable(pimpl->tree->isInternalNodeExpandable());
    pimpl->navbar=new NavigationBar(this);
    pimpl->navbar->setExclusive(false);
    pimpl->navbar->setSeparatorSample(sep);
    pimpl->navbar->setSingleVisibleMode(tree->isNavbarSingleVisibleMode());
    l->addWidget(pimpl->navbar);

    pimpl->splitter=new HTreeSplitter(this);
    l->addWidget(pimpl->splitter,1);

    pimpl->nodeDestroyedMapper=new QSignalMapper(this);
    connect(pimpl->nodeDestroyedMapper,&QSignalMapper::mappedInt,this,
        [this](int index)
        {
            if (index>=pimpl->tree->expandableLastDepthOnNodeOpen())
            {
                int i=index-pimpl->tree->expandableLastDepthOnNodeOpen();
                if (i<0)
                {
                    i=0;
                }
                for (;i<index;i++)
                {
                    pimpl->nodes[i]->expandNode();
                }
            }
            pimpl->truncate(index+1);
            emitNodesReconfigured();
        }
    );

    connect(
        pimpl->navbar,
        &NavigationBar::indexToggled,
        this,
        [this](int index, bool checked)
        {
            auto node=pimpl->nodes[index];
            if (node!=nullptr)
            {
                if (checked)
                {
                    auto customAction=node->navbarActivateAction();
                    if (customAction)
                    {
                        customAction();
                    }
                    else
                    {
                        if (pimpl->tree->expandableLastDepthOnNodeOpen()==0)
                        {
                            node->setExpanded(true);
                        }
                        else
                        {
                            auto next=node->nextNode();
                            if (next!=nullptr)
                            {
                                QTimer::singleShot(
                                    10,
                                    this,
                                    [next=QPointer<HTreeNode>{next}]()
                                    {
                                        if (next)
                                        {
                                            next->expandParentNode();
                                        }
                                    });
                            }
                            else
                            {
                                node->setExpanded(true);
                            }
                        }
                    }
                }
                else
                {
                    pimpl->navbar->blockSignals(true);
                    pimpl->navbar->setItemChecked(index,true);
                    pimpl->navbar->blockSignals(false);

                    scrollToNode(node);
                }
            }
        }
    );

    connect(
        pimpl->navbar,
        &NavigationBar::indexSeparatorClicked,
        this,
        [this](int index)
        {
            auto node=pimpl->nodes[index];
            auto branch=qobject_cast<HTreeBranch*>(node);
            if (branch!=nullptr)
            {
                if (node->isCollapsible())
                {
                    branch->setExpanded(!node->isExpanded());
                    pimpl->navbar->blockSignals(true);
                    pimpl->navbar->setItemChecked(index,node->isExpanded());
                    pimpl->navbar->blockSignals(false);
                }
            }
            if (node->isExpanded())
            {
                scrollToNode(node);
            }
        }
    );

    connect(
        pimpl->navbar,
        &NavigationBar::indexSeparatorHovered,
        this,
        [this](int index, bool enable)
        {
            auto node=pimpl->nodes[index];
            auto branch=qobject_cast<HTreeBranch*>(node);
            if (branch!=nullptr)
            {
                if (node->isCollapsible())
                {
                    auto check=enable?!node->isExpanded():node->isExpanded();
                    pimpl->navbar->blockSignals(true);
                    pimpl->navbar->setItemChecked(index,check);

                    if (node->isExpanded())
                    {
                        pimpl->navbar->setSeparatorTooltip(index,tr("Collapse"));
                    }
                    else
                    {
                        pimpl->navbar->setSeparatorTooltip(index,tr("Expand"));
                    }

                    pimpl->navbar->blockSignals(false);
                }
            }
        }
    );

    connect(
        pimpl->navbar,
        &NavigationBar::indexOpenInNewTabRequested,
        this,
        [this](int index)
        {
            if (index<0 || index>=(int)pimpl->nodes.size()) return;
            auto node=pimpl->nodes[index];
            if (node!=nullptr)
            {
                pimpl->tree->openPath(node->path(),HTree::NewTabIndex);
            }
        }
    );

    connect(
        pimpl->navbar,
        &NavigationBar::indexOpenInNewWindowRequested,
        this,
        [this](int index)
        {
            if (index<0 || index>=(int)pimpl->nodes.size()) return;
            auto node=pimpl->nodes[index];
            if (node!=nullptr)
            {
                emit pimpl->tree->newTreeRequested(node->path());
            }
        }
    );

    pimpl->splitter->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    pimpl->closeWarnFrame=new QFrame(this);
    pimpl->closeWarnFrame->setAttribute(Qt::WA_ShowWithoutActivating);
    pimpl->closeWarnFrame->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    pimpl->closeWarnFrame->setObjectName("closeWarnFrame");
    pimpl->closeWarnFrame->setVisible(false);
    auto cwL=Layout::vertical(pimpl->closeWarnFrame);
    auto closeWarnButton=new PushButton(pimpl->closeWarnFrame);
    auto closeWarnIcon=Style::instance().svgIconLocator().icon("HTreeTabCloseWarn::close",this);
    closeWarnButton->setSvgIcon(closeWarnIcon);    
    cwL->addWidget(closeWarnButton,0,Qt::AlignLeft);
    cwL->addStretch(1);
    pimpl->closeWarnTimer=new SingleShotTimer(this);
    pimpl->closeWarnDisableTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

HTreeTab::~HTreeTab()
{
    disconnect(SIGNAL(nodesReconfigured()));
    pimpl->nodeDestroyedMapper->blockSignals(true);
    pimpl->scrollTimer->clear();
}

//--------------------------------------------------------------------------

HTreeNode* HTreeTab::node() const
{
    if (pimpl->nodes.empty())
    {
        return nullptr;
    }
    return pimpl->nodes.back();
}

//--------------------------------------------------------------------------

HTreeNode* HTreeTab::node(const HTreePath& path, bool exact) const
{
    if (pimpl->nodes.empty())
    {
        return nullptr;
    }

    auto n=node();
    if (exact)
    {
        if (n->path()==path)
        {
            return n;
        }
    }

    while(n!=nullptr)
    {
        if (n->path()==path)
        {
            return n;
        }
        n=n->parentNode();
    }

    return nullptr;
}

//--------------------------------------------------------------------------

bool HTreeTab::openPath(HTreePath path, HistoryMode historyMode)
{
    auto ok=pimpl->doOpenPath(std::move(path));
    if (ok)
    {
        recordHistory(historyMode);
    }
    return ok;
}

//--------------------------------------------------------------------------

bool HTreeTab_p::doOpenPath(HTreePath path)
{
#if 0
    auto w=new QFrame();
    w->setObjectName("tf");
    // w->setMinimumHeight(500);
    w->setMinimumWidth(150);
    // w->setMaximumWidth(900);
    w->setStyleSheet("#tf {background-color:green;}");

    auto w1=new QFrame();
    w1->setObjectName("tf1");
    // w1->setMinimumHeight(500);
    w1->setMinimumWidth(50);
    // w1->setMaximumWidth(800);
    w1->setStyleSheet("#tf1 {background-color:blue;}");

    auto s=new HTreeSplitter();
    s->resize(800,600);
    s->addWidget(w,2);
    s->addWidget(w1,4);
    s->show();

    return false;
#endif

    int truncIndex=0;
    auto truncCount=std::min(path.elements().size(),nodes.size());
    for (size_t i=0;i<truncCount;i++)
    {
        const auto& el=path.elements().at(i);
        const auto* node=nodes.at(i);
        if (el.uniqueId()==node->id().toStdString())
        {
            truncIndex=i+1;
        }
        else
        {
            break;
        }
    }

    if (truncIndex==static_cast<int>(path.elements().size())-1
        && nodes.size()>=path.elements().size())
    {
        auto* cand=nodes.at(static_cast<size_t>(truncIndex));
        if (cand->canReconstructFromPath(path.elements().back()))
        {
            return reconstructLastNode(truncIndex,std::move(path));
        }
    }

    if (truncIndex>0)
    {
        self->truncate(truncIndex);
    }
    else
    {
        self->truncate(0);
    }

    HTreeNode* nod=nullptr;
    for (size_t i=static_cast<size_t>(truncIndex);i<path.elements().size();i++)
    {
        auto lastInPath=i==path.elements().size()-1;
        const auto& el=path.elements().at(i);
        auto lastNode=self->node();
        if (lastNode!=nullptr)
        {
            auto branch=qobject_cast<HTreeBranch*>(lastNode);
            UiseAssert(branch!=nullptr,"All nodes in the path except for the last must be branch nodes");
            auto prevNode=branch->nextNode();
            nod=branch->loadNextNode(el,lastInPath);
            if (nod==nullptr)
            {
                return false;
            }
            if (nod==prevNode)
            {
                nod->setExpanded(true);
            }

            if (lastInPath)
            {
                scrollTimer->shot(50,
                    [this,node=QPointer<HTreeNode>{nod}]
                    {
                        if (node)
                        {
                            if (htreeTabDebug())
                            {
                                qDebug().noquote() << htreeTabDebugTs() << "DEFERRED(50ms) openPath scrollToNode";
                            }
                            scrollToNode(node);
                        }
                    },
                    true
                );
            }
        }
        else
        {
            UiseAssert(i==0,"Previous last node must exist for all path elements except for the first");

            auto nodeResult=tree->nodeLocator()->findOrCreateNode(el,nullptr,self);
            if (nodeResult.first==nullptr)
            {
                return false;
            }
            if (!nodeResult.second && nodeResult.first->isUnique())
            {
                nodeResult.first->activate();
                return false;
            }

            nod=nodeResult.first;
            if (nod==nullptr)
            {
                return false;
            }
            if (lastInPath || !tree->isExlusivelyExpandableNode())
            {
                nod->fillContent();
            }
            self->appendNode(nod);
        }
    }

    //! @todo Restore splitter configuration

    if (nod!=nullptr)
    {
        if (htreeTabDebug())
        {
            qDebug().noquote() << htreeTabDebugTs() << "openPath final sync scrollToNode";
        }
        scrollToNode(nod);
    }
    else if (truncIndex==static_cast<int>(path.elements().size()) && !nodes.empty())
    {
        // The requested path is already fully open (the element loop above never ran), so
        // give the currently open last node a chance to redo its action via
        // reopen()/doReopen() -- e.g. re-clicking the Quit item's own navbar breadcrumb.
        // No-op by default for ordinary nodes.
        nodes.back()->reopen();
    }
    return true;
}

//--------------------------------------------------------------------------

HTreePath HTreeTab::path() const
{
    HTreePath p;

    auto n=node();
    if (n!=nullptr)
    {
        p=n->path();

        //! @todo Save width in configuration
#if 0
        for (int j=0;j<p.elements().size();j++)
        {
            auto& el=p.elements().at(j);
            bool expanded=true;
            auto branch=qobject_cast<HTreeBranch*>(n);
            if (branch!=nullptr)
            {
                expanded=branch->isExpanded();
            }

            HTreePathElementConfig cfg{expanded,splitterSizes[j]};
            el.setConfig(std::move(cfg));
        }
#endif
    }

    return p;
}

//--------------------------------------------------------------------------

void HTreeTab::setTabIcon(const QIcon& icon)
{
    pimpl->tabIconOverride=icon;
    pimpl->tabIconOverrideSet=true;
    emit iconUpdated(icon);
}

//--------------------------------------------------------------------------

QIcon HTreeTab::tabIcon() const
{
    return pimpl->tabIconOverride;
}

//--------------------------------------------------------------------------

bool HTreeTab::hasTabIconOverride() const noexcept
{
    return pimpl->tabIconOverrideSet;
}

//--------------------------------------------------------------------------

void HTreeTab::setTabTooltip(const QString& tooltip)
{
    pimpl->tabTooltipOverride=tooltip;
    pimpl->tabTooltipOverrideSet=true;
    emit tooltipUpdated(tooltip);
}

//--------------------------------------------------------------------------

QString HTreeTab::tabTooltip() const
{
    return pimpl->tabTooltipOverride;
}

//--------------------------------------------------------------------------

bool HTreeTab::hasTabTooltipOverride() const noexcept
{
    return pimpl->tabTooltipOverrideSet;
}

//--------------------------------------------------------------------------

void HTreeTab::appendNode(HTreeNode* node)
{
    pimpl->appendNode(node);
    if (!tree()->isNodeHeaderVisible())
    {
        node->setHeaderVisible(false);
    }
}

//--------------------------------------------------------------------------

void HTreeTab::setTree(HTree* tree)
{
    pimpl->tree=tree;
}

//--------------------------------------------------------------------------

HTree* HTreeTab::tree() const
{
    return pimpl->tree;
}

//--------------------------------------------------------------------------

NavigationBar* HTreeTab::navbar() const
{
    return pimpl->navbar;
}

//--------------------------------------------------------------------------

void HTreeTab::closeNode(HTreeNode* node)
{
    pimpl->closeWarnFrame->setVisible(false);
    pimpl->closeWarnDisable=true;

    pimpl->closeWarnDisableTimer->shot(
        1000,
        [this]()
        {
            pimpl->closeWarnDisable=false;
        },
        true
    );

    if(node==nullptr)
    {
        return;
    }

    int index=node->path().elements().size()-1;
    if (index>=pimpl->tree->expandableLastDepthOnNodeOpen())
    {
        int i=index-pimpl->tree->expandableLastDepthOnNodeOpen();
        if (i<0)
        {
            i=0;
        }
        for (;i<index;i++)
        {
            pimpl->nodes[i]->setExpanded(true);
        }
    }

    pimpl->truncate(index);
}

//--------------------------------------------------------------------------

void HTreeTab::truncate(int index)
{
    pimpl->closeWarnFrame->setVisible(false);
    pimpl->closeWarnDisableTimer->shot(
        1000,
        [this]()
        {
            pimpl->closeWarnDisable=false;
        },
        true
    );

    pimpl->truncate(index);
}

//--------------------------------------------------------------------------

void HTreeTab::scrollToNode(HTreeNode* node)
{
    pimpl->scrollToNode(node);
}

//--------------------------------------------------------------------------

void HTreeTab::activate()
{
    tree()->activate();
    tree()->setCurrentTab(this);
}

//--------------------------------------------------------------------------

bool HTreeTab::isSingleCollapsePlaceholder() const noexcept
{
    return tree()->isSingleCollapsePlaceholder();
}

//--------------------------------------------------------------------------

bool HTreeTab::isCollapsePlaceholderHidden() const noexcept
{
    return tree()->isCollapsePlacehodlerHidden();
}

//--------------------------------------------------------------------------

void HTreeTab::adjustWidthsAndPositions()
{
    pimpl->splitter->adjustWidthsAndPositions();
}

//--------------------------------------------------------------------------

void HTreeTab::nodeCloseHovered(HTreeNode* /*node*/, bool /*enable*/)
{
//! @todo Configure close frame display
#if 0

    if (pimpl->closeWarnDisable)
    {
        return;
    }

    pimpl->closeWarnTimer->clear();
    if (node==nullptr)
    {
        pimpl->closeWarnFrame->setVisible(false);
        return;
    }

    if (!enable)
    {
        pimpl->closeWarnTimer->shot(
            100,
            [this]()
            {
                pimpl->closeWarnFrame->setVisible(false);
            }
        );
        return;
    }

    if (enable && pimpl->closeWarnFrame->isVisible())
    {
        pimpl->closeWarnTimer->shot(
            3000,
            [this]()
            {
                pimpl->closeWarnFrame->setVisible(false);
            }
        );
        return;
    }

    QRect nodeRect = node->geometry();
    auto topLeft=nodeRect.topLeft();
    topLeft.setY(topLeft.y() + node->titleBarHeight());
    topLeft=node->mapTo(this,topLeft);

    auto lastNode=pimpl->nodes.back();
    if (lastNode!=node)
    {
        auto lastNodeRect=lastNode->geometry();
        auto bottomRight=lastNode->mapTo(this,lastNodeRect.bottomRight());

        QRect targetRect{topLeft,bottomRight};

        pimpl->closeWarnFrame->setGeometry(targetRect);
        pimpl->closeWarnFrame->setVisible(true);

        pimpl->closeWarnTimer->shot(
            3000,
            [this]()
            {
                pimpl->closeWarnFrame->setVisible(false);
            }
        );
    }
#endif
}

//--------------------------------------------------------------------------

std::vector<HTreeNode*> HTreeTab::nodes() const
{
    return pimpl->nodes;
}

//--------------------------------------------------------------------------

void HTreeTab::recordHistory(HistoryMode historyMode)
{
    pimpl->doRecordHistory(historyMode);
}

//--------------------------------------------------------------------------

void HTreeTab_p::doRecordHistory(HTreeTab::HistoryMode historyMode)
{
    if (historyNavigating)
    {
        // goBack()/goForward() are themselves replaying a history entry via openPath() -- must
        // not re-push what is already there
        return;
    }
    if (nodes.empty())
    {
        return;
    }
    if (!nodes.back()->isHistoryEnabled())
    {
        return;
    }
    auto p=self->path();
    if (p.isNull())
    {
        return;
    }
    if (historyPos>=0 && history.at(static_cast<size_t>(historyPos))==p)
    {
        // same node as the current cursor entry (e.g. a reopen()) -- do not duplicate
        return;
    }

    if (historyMode==HTreeTab::HistoryMode::Redirect
        && historyPos>=0
        && isPathAncestor(history.at(static_cast<size_t>(historyPos)),p))
    {
        // The node we were just on opened this child by itself rather than the user stepping
        // into it -- it is a transient waypoint, so replace its entry instead of stacking on
        // top of it. Back then leaves the subtree altogether, as if the child had been the
        // navigation target all along.
        history.resize(static_cast<size_t>(historyPos+1));
        history[static_cast<size_t>(historyPos)]=std::move(p);
        emit self->historyChanged();
        return;
    }

    if (historyPos>0 && history.at(static_cast<size_t>(historyPos-1))==p)
    {
        // Opening the very node Back points at IS the Back navigation -- just walk the cursor
        // back, exactly as goBack() would, instead of appending a duplicate. Without this,
        // bouncing between two chats records A,B,A,B,... forever and Back only ever undoes one
        // hop of the bounce rather than leaving the pair.
        --historyPos;
        emit self->historyChanged();
        return;
    }

    // A new navigation from a point in history discards whatever was ahead of it, same as a
    // browser: the redo branch is gone once you navigate somewhere new.
    history.resize(static_cast<size_t>(historyPos+1));
    history.push_back(std::move(p));
    if (history.size()>HTreeTab::MaxHistoryDepth)
    {
        history.erase(history.begin());
    }
    historyPos=static_cast<int>(history.size())-1;

    emit self->historyChanged();
}

//--------------------------------------------------------------------------

HTreeNode* HTreeTab_p::collapsedParentHost() const
{
    // A node reached by an "open exclusively" navigation collapses its ancestors rather than
    // closing them (HTreeNode::expandExclusive()), and collapsing never changes the tab's
    // path() -- so a node hidden that way is invisible to the path history and revealing it has
    // to be a step of its own, which Back takes before moving through the history.
    //
    // The catch is telling "hidden away" apart from the ordinary viewport: expandableLastDepth
    // OnNodeOpen() columns staying expanded while everything before them is collapsed is simply
    // how every node is opened (see appendNode()), so with a chat page open Root and Character
    // are always collapsed and a naive "is any ancestor collapsed?" test would make Back reveal
    // forever and never reach the history. Only a stack that shows FEWER than that many columns
    // has actually had something collapsed away -- e.g. CharacterController's Chats item, whose
    // expandExclusive() takes the default depth 0 and so collapses the character column too.
    if (tree==nullptr)
    {
        return nullptr;
    }
    auto depth=tree->expandableLastDepthOnNodeOpen();
    if (depth<=0)
    {
        // the tree never collapses anything on open, so nothing can have been hidden this way
        return nullptr;
    }

    int count=static_cast<int>(nodes.size());
    int visible=0;
    for (int i=count-1;i>=0;i--)
    {
        if (nodes[i]==nullptr || !nodes[i]->isExpanded())
        {
            break;
        }
        ++visible;
    }
    if (visible==0 || visible>=depth)
    {
        return nullptr;
    }

    // the shallowest expanded node -- the leftmost column the user can actually see
    auto* host=nodes[static_cast<size_t>(count-visible)];
    if (host!=nullptr && host->isToParentVisible() && host->parentNode()!=nullptr)
    {
        return host;
    }
    return nullptr;
}

//--------------------------------------------------------------------------

void HTreeTab_p::syncHistoryCursorTo(const HTreePath& path)
{
    // Revealing a hidden column can also close a deeper node (HTreeNode::expandParentNode()
    // closes whatever sits expandableLastDepthOnNodeOpen() steps ahead of the parent), which
    // moves the tab to a shallower path without going through the history. Left alone, the
    // cursor would still point at the now-closed deeper entry and the next Back -- reading that
    // as an off-history detour -- would navigate forward into it again. Walk the cursor back to
    // the entry that matches where we actually ended up.
    for (int i=historyPos;i>=0;i--)
    {
        if (history.at(static_cast<size_t>(i))==path)
        {
            historyPos=i;
            return;
        }
    }
}

//--------------------------------------------------------------------------

void HTreeTab_p::expandLandingColumns()
{
    // Landing on a shallower path goes through truncate(), which -- unlike closeNode() -- never
    // re-expands anything. A node that some descendant collapsed on its way in (see
    // HTreeNode::expandExclusive(), used by appendNode() and by "open exclusively" navigation
    // handlers) would therefore be navigated to but stay collapsed, leaving the user staring at
    // a collapsed strip instead of the page they asked for. Re-expand the trailing columns
    // exactly the way HTreeTab::closeNode() does when a node is closed by hand.
    if (nodes.empty() || tree==nullptr)
    {
        return;
    }
    auto depth=tree->expandableLastDepthOnNodeOpen();
    if (depth<=0)
    {
        return;
    }
    int count=static_cast<int>(nodes.size());
    int first=count-depth;
    if (first<0)
    {
        first=0;
    }
    for (int i=first;i<count;i++)
    {
        nodes[i]->setExpanded(true);
    }
}

//--------------------------------------------------------------------------

bool HTreeTab_p::navigateHistory(int index, int direction)
{
    while (index>=0 && index<static_cast<int>(history.size()))
    {
        auto target=history.at(static_cast<size_t>(index));

        historyNavigating=true;
        self->openPath(target);
        historyNavigating=false;

        if (self->path()==target)
        {
            historyPos=index;
            expandLandingColumns();
            if (!nodes.empty())
            {
                scrollToNode(nodes.back());
            }
            emit self->historyChanged();
            return true;
        }

        // The entry is no longer reachable via openPath() (e.g. a chat page hosted on a node
        // whose own path openPath() cannot rebuild, see ChatPage::noteRecentChat()) -- drop it
        // and keep looking in the same direction instead of getting stuck on it.
        history.erase(history.begin()+index);
        if (historyPos>index)
        {
            --historyPos;
        }
        if (historyPos>=static_cast<int>(history.size()))
        {
            // the erased entry was the last one at/after the cursor -- nothing left to point at
            historyPos=static_cast<int>(history.size())-1;
        }
        if (direction<0)
        {
            --index;
        }
    }

    emit self->historyChanged();
    return false;
}

//--------------------------------------------------------------------------

bool HTreeTab::canGoBack() const noexcept
{
    if (pimpl->collapsedParentHost()!=nullptr)
    {
        // a column is hidden above the visible stack and Back reveals it first
        return true;
    }
    if (pimpl->historyPos<0)
    {
        return false;
    }
    if (!(path()==pimpl->history.at(static_cast<size_t>(pimpl->historyPos))))
    {
        // current path is a detour off the recorded cursor entry (e.g. a node was closed with
        // its own close button) -- one Back returns to the cursor entry itself
        return true;
    }
    return pimpl->historyPos>0;
}

//--------------------------------------------------------------------------

bool HTreeTab::canGoForward() const noexcept
{
    if (pimpl->historyPos<0 || pimpl->historyPos+1>=static_cast<int>(pimpl->history.size()))
    {
        return false;
    }
    // deliberately disabled while off-history (see canGoBack()) so Back and Forward never mean
    // the same thing in that state -- a Back press restores the cursor entry first
    return path()==pimpl->history.at(static_cast<size_t>(pimpl->historyPos));
}

//--------------------------------------------------------------------------

bool HTreeTab::goBack()
{
    if (auto* host=pimpl->collapsedParentHost(); host!=nullptr)
    {
        // Reveal the column hidden above the visible stack before touching the history cursor --
        // this is the step the old navbar Back took unconditionally (HTreeNode::
        // activateToParent()), and the only way to get back to a node that was collapsed away by
        // an "open exclusively" navigation, since collapsing never changed the tab's path.
        // activateToParent() honours a node's own toParentAction()/toParentPath() override, so a
        // node that redefines "up" (e.g. CharacterNode, ShareMeNode) still navigates its own way,
        // recording that as an ordinary history entry.
        auto before=path();
        host->activateToParent();
        auto after=path();
        if (!(after==before))
        {
            pimpl->syncHistoryCursorTo(after);
        }
        emit historyChanged();
        return true;
    }
    if (pimpl->historyPos<0)
    {
        return false;
    }
    if (!(path()==pimpl->history.at(static_cast<size_t>(pimpl->historyPos))))
    {
        return pimpl->navigateHistory(pimpl->historyPos,-1);
    }
    if (pimpl->historyPos==0)
    {
        return false;
    }
    return pimpl->navigateHistory(pimpl->historyPos-1,-1);
}

//--------------------------------------------------------------------------

bool HTreeTab::goForward()
{
    if (!canGoForward())
    {
        return false;
    }
    return pimpl->navigateHistory(pimpl->historyPos+1,1);
}

//--------------------------------------------------------------------------

std::vector<HTreePath> HTreeTab::history() const
{
    return pimpl->history;
}

//--------------------------------------------------------------------------

int HTreeTab::historyPosition() const noexcept
{
    return pimpl->historyPos;
}

//--------------------------------------------------------------------------

void HTreeTab::clearHistory()
{
    pimpl->history.clear();
    pimpl->historyPos=-1;
    emit historyChanged();
}

//--------------------------------------------------------------------------

void HTreeTab::emitNodesReconfigured()
{
    pimpl->reconfigureTimer->shot(55,
                                  [this]()
                                  {
                                      emit nodesReconfigured();
                                  },
                                  true
                            );
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
