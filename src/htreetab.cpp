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

#include <QPushButton>
#include <QTextBrowser>

#include <QScrollBar>
#include <QSplitter>
#include <QSignalMapper>

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
        SingleShotTimer* scrolTimer;

        QFrame* closeWarnFrame;
        SingleShotTimer* closeWarnTimer;

        ~HTreeTab_p()
        {
            for (auto& node : nodes)
            {
                if (node)
                {
                    disconnectNode(node,true);
                    node->disconnect();
                }
            }
        }

        void appendNode(HTreeNode* node);
        void updateLastNode();
        void disconnectNode(HTreeNode* node, bool beforeDestroy);
        void truncate(int index);
        void scrollToNode(HTreeNode* node);
        void scrollToEnd();
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
    if (index<0 || index>=nodes.size())
    {
        return;
    }

    auto lastIndex=nodes.size()-1;
    for (auto i=lastIndex;i>=index;i--)
    {
        auto w=splitter->widget(i);
        auto s=qobject_cast<HTreeSplitterSection*>(w);
        if (s!=nullptr)
        {
            auto n=qobject_cast<HTreeNode*>(s->widget());
            if (n!=nullptr)
            {
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
    navbar->addItem(node->name(),node->nodeTooltip(),node->id());
    navbar->blockSignals(true);
    navbar->setItemChecked(index,node->isExpanded());
    navbar->blockSignals(false);
    node->connect(
        node,
        &HTreeNode::nameUpdated,
        self,
        [this,index](const QString& val)
        {
            navbar->setItemName(index,val);
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
        [this,index](bool enable)
        {
            navbar->blockSignals(true);
            navbar->setItemChecked(index,enable);
            navbar->blockSignals(false);
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

    // watch if node is expanded
    node->connect(
        node,
        &HTreeNode::toggleExpanded,
        self,
        [this,index,node](bool enable)
        {
            auto w=splitter->widget(index);
            if (w!=nullptr)
            {
                w->setMinimumWidth(node->minimumWidth()+splitter->sectionLineWidth());
                w->setMaximumWidth(std::min(QWIDGETSIZE_MAX,(node->maximumWidth()+splitter->sectionLineWidth())));
                splitter->toggleSectionExpanded(index,enable,node->isNodeVisible());
            }
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
        scrolTimer->shot(50,
            [this,node=QPointer<HTreeNode>{node}]
            {
                if (node)
                {
                    scrollToNode(node);
                }
            },
            true
        );
    }
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

HTreeTab::HTreeTab(HTree* tree, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeTab_p>())
{
    pimpl->self=this;
    pimpl->tree=tree;
    pimpl->scrolTimer=new SingleShotTimer(this);

    auto l=Layout::vertical(this);

    auto sep=new NavigationBarSeparator(this);
    sep->setHoverCharacterEnabled(true);
    pimpl->navbar=new NavigationBar(this);
    pimpl->navbar->setExclusive(false);
    pimpl->navbar->setSeparatorSample(sep);
    l->addWidget(pimpl->navbar);

    pimpl->splitter=new HTreeSplitter(this);
    l->addWidget(pimpl->splitter,1);

    pimpl->nodeDestroyedMapper=new QSignalMapper(this);
    connect(pimpl->nodeDestroyedMapper,&QSignalMapper::mappedInt,this,
        [this](int index)
        {
            pimpl->truncate(index+1);
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
}

//--------------------------------------------------------------------------

HTreeTab::~HTreeTab()
{
    pimpl->nodeDestroyedMapper->blockSignals(true);
    pimpl->scrolTimer->clear();
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

bool HTreeTab::openPath(HTreePath path)
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
    auto truncCount=std::min(path.elements().size(),pimpl->nodes.size());
    for (size_t i=0;i<truncCount;i++)
    {
        const auto& el=path.elements().at(i);
        const auto* node=pimpl->nodes.at(i);
        if (el.uniqueId()==node->id())
        {
            truncIndex=i+1;
        }
        else
        {
            break;
        }
    }

    if (truncIndex>0)
    {
        truncate(truncIndex);
    }
    else
    {
        truncate(0);
    }

    HTreeNode* nod=nullptr;
    for (size_t i=static_cast<size_t>(truncIndex);i<path.elements().size();i++)
    {
        auto lastInPath=i==path.elements().size()-1;
        const auto& el=path.elements().at(i);
        auto lastNode=node();
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
                pimpl->scrolTimer->shot(50,
                    [this,node=QPointer<HTreeNode>{nod}]
                    {
                        if (node)
                        {
                            pimpl->scrollToNode(node);
                        }
                    },
                    true
                );
            }
        }
        else
        {
            UiseAssert(i==0,"Previous last node must exist for all path elements except for the first");

            auto nodeResult=pimpl->tree->nodeLocator()->findOrCreateNode(el,nullptr,this);
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
            if (lastInPath || !tree()->isExlusivelyExpandableNode())
            {
                nod->fillContent();
            }
            appendNode(nod);
        }
    }

    //! @todo Restore splitter configuration

    if (nod!=nullptr)
    {
        pimpl->scrollToNode(nod);
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

void HTreeTab::appendNode(HTreeNode* node)
{
    pimpl->appendNode(node);
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

    if(node==nullptr)
    {
        return;
    }

    int index=node->path().elements().size()-1;
    pimpl->truncate(index);
}

//--------------------------------------------------------------------------

void HTreeTab::truncate(int index)
{
    pimpl->closeWarnFrame->setVisible(false);
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

void HTreeTab::adjustWidthsAndPositions()
{
    pimpl->splitter->adjustWidthsAndPositions();
}

//--------------------------------------------------------------------------

void HTreeTab::nodeCloseHovered(HTreeNode* node, bool enable)
{
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
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
