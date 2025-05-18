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

#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htreebranch.hpp>
#include <uise/desktop/htreenodefactory.hpp>
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

        void appendNode(HTreeNode* node);
        bool loadingPath=false;

        void updateLastNode();
        void disconnectNode(HTreeNode* node, bool beforeDestroy=true);
        void truncate(int index);
        void scrollToNode(HTreeNode* node);
        void scrollToEnd();
};

//--------------------------------------------------------------------------

void HTreeTab_p::scrollToEnd()
{
    auto timer=new SingleShotTimer(self);
    timer->shot(50,[this,timer]()
    {
        if (!nodes.empty())
        {
            splitter->scrollToIndex(nodes.size()-1);
        }

        timer->deleteLater();
    });
}

//--------------------------------------------------------------------------

void HTreeTab_p::scrollToNode(HTreeNode* node)
{
    // if (node->path().elements().size()==nodes.size())
    // {
    //     scrollToEnd();
    // }
    // else
    // {
        // splitter->scrollToWidget(node);
    // }

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

        if (beforeDestroy)
        {
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
    if (index<1 || index>=nodes.size())
    {
        return;
    }

#if 0
    auto lastIndex=nodes.size()-1;
    for (auto i=lastIndex;i>=index;i--)
    {
        auto w=splitter->widget(i);
        auto n=qobject_cast<HTreeNode*>(w);
        if (n!=nullptr)
        {
            disconnectNode(n,true);
        }
        splitter->removeWidget(i);
    }
#else
    auto lastIndex=nodes.size()-1;
    for (auto i=lastIndex;i>=index;i--)
    {
        auto w=splitter->widget(i);
        auto n=qobject_cast<HTreeNode*>(w);
        if (n!=nullptr)
        {
            disconnectNode(n,true);
        }
    }
    splitter->truncate(index);
#endif
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
        disconnectNode(lastNode);
    }

    // add to nodes
    nodes.push_back(node);

    // add widget to splitter
    splitter->addWidget(node);
    auto index=splitter->count()-1;
    splitter->scrollToIndex(index);

    // add item to navigation bar
    navbar->addItem(node->name(),node->nodeTooltip(),node->id());
    navbar->blockSignals(true);
    navbar->setItemChecked(index,node->isExpanded());
    navbar->blockSignals(false);
    self->connect(
        node,
        &HTreeNode::nameUpdated,
        self,
        [this](const QString& val)
        {
            navbar->setItemName(nodes.size(),val);
        }
    );
    self->connect(
        node,
        &HTreeNode::tooltipUpdated,
        self,
        [this](const QString& val)
        {
            navbar->setItemTooltip(nodes.size(),val);
        }
    );
    self->connect(
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

    // watch if node is expanded
    qDebug() << "added nodw index=" <<index;
    node->connect(
        node,
        &HTreeNode::toggleExpanded,
        self,
        [this,index,node](bool enable)
        {
            qDebug() << "on HTreeNode::toggleExpanded index=" <<index << " enable="<<enable;
            auto w=splitter->widget(index);
            if (w!=nullptr)
            {
                w->setMinimumWidth(node->minimumWidth()+splitter->sectionLineWidth());
                w->setMaximumWidth(node->maximumWidth()+splitter->sectionLineWidth());

                qDebug() << "minimum width=" << w->minimumWidth() << " maximum width=" << w->maximumWidth();

                splitter->toggleSectionExpanded(index,enable);
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

    auto l=Layout::vertical(this);

    pimpl->navbar=new NavigationBar(this);
    // pimpl->navbar->setCheckable(false);
    pimpl->navbar->setExclusive(false);
    l->addWidget(pimpl->navbar);

    pimpl->splitter=new HTreeSplitter(this);
    l->addWidget(pimpl->splitter);

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
            auto branch=qobject_cast<HTreeBranch*>(node);
            if (branch!=nullptr)
            {
                if (!checked && !node->isCollapsible())
                {
                    pimpl->navbar->blockSignals(true);
                    pimpl->navbar->setItemChecked(index,true);
                    pimpl->navbar->blockSignals(false);
                }
                else
                {
                    branch->setExpanded(checked);
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
        &NavigationBar::indexSeparatorClicked,
        this,
        [this](int index)
        {
            auto node=pimpl->nodes[index];
            scrollToNode(node);
        }
    );

    pimpl->splitter->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
}

//--------------------------------------------------------------------------

HTreeTab::~HTreeTab()
{
    pimpl->nodeDestroyedMapper->blockSignals(true);
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
    truncate(0);

    pimpl->loadingPath=true;
    QList<int> splitterSizes;

    HTreeNode* nod=nullptr;
    for (size_t i=0;i<path.elements().size();i++)
    {
        const auto& el=path.elements().at(i);
        auto lastNode=node();
        if (lastNode!=nullptr)
        {
            auto branch=qobject_cast<HTreeBranch*>(lastNode);
            UiseAssert(branch!=nullptr,"All nodes in the path except for the last must be branch nodes");
            auto prevNode=branch->nextNode();
            nod=branch->loadNextNode(el);
            if (nod==nullptr)
            {
                return false;
            }
            if (nod==prevNode)
            {
                nod->setExpanded(true);
            }
        }
        else
        {
            UiseAssert(i==0,"Previous last node must exist for all path elements except for the first");
            nod=pimpl->tree->nodeFactory()->makeNode(el,nullptr,this);
            if (nod==nullptr)
            {
                return false;
            }
            appendNode(nod);
            nod->refresh();
        }

        splitterSizes.push_back(el.config().width());
    }

    //! @todo Set sizes only if they were saved
    // pimpl->splitter->setSizes(splitterSizes);

    pimpl->loadingPath=false;
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
    pimpl->truncate(index);
}

//--------------------------------------------------------------------------

void HTreeTab::scrollToNode(HTreeNode* node)
{
    pimpl->scrollToNode(node);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
