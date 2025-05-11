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

/** @file uise/desktop/horizontaltree.hpp
*
*  Defines HorizontalTree.
*
*/

/****************************************************************************/

#include <QTabWidget>
#include <QSplitter>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/navigationbar.hpp>

#include <uise/desktop/horizontaltree.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/******************************HorizontalTreeSideBar******************************/

//--------------------------------------------------------------------------

class HorizontalTreeSideBar_p
{
    public:

        HorizontalTree* tree=nullptr;
};

//--------------------------------------------------------------------------

HorizontalTreeSideBar::HorizontalTreeSideBar(HorizontalTree* tree, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HorizontalTreeSideBar_p>())
{
    pimpl->tree=tree;
}

//--------------------------------------------------------------------------

HorizontalTreeSideBar::~HorizontalTreeSideBar()
{}

/******************************HorizontalTreeNode******************************/

//--------------------------------------------------------------------------

class HorizontalTreeNode_p
{
    public:

        HorizontalTree* tree=nullptr;
};

//--------------------------------------------------------------------------

HorizontalTreeNode::HorizontalTreeNode(HorizontalTree* tree, QWidget* parent)
    : FrameWithRefresh(parent),
      pimpl(std::make_unique<HorizontalTreeNode_p>())
{
    pimpl->tree=tree;
}

//--------------------------------------------------------------------------

HorizontalTreeNode::HorizontalTreeNode(QWidget* parent)
    : HorizontalTreeNode(nullptr,parent)
{}

//--------------------------------------------------------------------------

HorizontalTreeNode::~HorizontalTreeNode()
{}

/******************************HorizontalTreeBranch******************************/

//--------------------------------------------------------------------------

class HorizontalTreeBranch_p
{
    public:
};

//--------------------------------------------------------------------------

HorizontalTreeBranch::HorizontalTreeBranch(HorizontalTree* tree, QWidget* parent)
    : HorizontalTreeNode(parent),
      pimpl(std::make_unique<HorizontalTreeBranch_p>())
{
}

//--------------------------------------------------------------------------

HorizontalTreeBranch::HorizontalTreeBranch(QWidget* parent)
    : HorizontalTreeBranch(nullptr,parent)
{
}

//--------------------------------------------------------------------------

HorizontalTreeBranch::~HorizontalTreeBranch()
{}

/******************************HorizontalTree******************************/

//--------------------------------------------------------------------------

class HorizontalTreeTab_p
{
    public:

        HorizontalTreeTab* self;
        HorizontalTree* tree;

        NavigationBar* navbar=nullptr;

        QScrollArea* scArea=nullptr;
        QSplitter* splitter=nullptr;

        std::vector<HorizontalTreeNode*> nodes;

        void appendNode(HorizontalTreeNode* node);
};

//--------------------------------------------------------------------------

void HorizontalTreeTab_p::appendNode(HorizontalTreeNode* node)
{
    HorizontalTreeNode* lastNode=nullptr;
    if (!nodes.empty())
    {
        lastNode=nodes.back();
        node->setParentNode(lastNode);
    }

    // add to nodes
    nodes.push_back(node);

    // add widget to splitter
    splitter->addWidget(node);

    // add item to navigation bar
    navbar->addItem(node->name(),node->tooltip(),node->id());
    self->connect(
        node,
        &HorizontalTreeNode::nameUpdated,
        self,
        [this](const QString& val)
        {
            navbar->setItemName(nodes.size(),val);
        }
        );
    self->connect(
        node,
        &HorizontalTreeNode::tooltipUpdated,
        self,
        [this](const QString& val)
        {
            navbar->setItemTooltip(nodes.size(),val);
        }
    );

    // disconnect signals from previous last node
    if (lastNode!=nullptr)
    {
        lastNode->disconnect(
            lastNode,
            &HorizontalTreeNode::nameUpdated,
            self,
            &HorizontalTreeTab::nameUpdated
        );

        lastNode->disconnect(
            lastNode,
            &HorizontalTreeNode::tooltipUpdated,
            self,
            &HorizontalTreeTab::tooltipUpdated
        );

        lastNode->disconnect(
            lastNode,
            &HorizontalTreeNode::iconUpdated,
            self,
            &HorizontalTreeTab::iconUpdated
        );
    }

    // signal that last node is updated
    emit self->nameUpdated(lastNode->name());
    emit self->tooltipUpdated(lastNode->tooltip());
    emit self->iconUpdated(lastNode->icon());

    // connect last node
    lastNode->connect(
        lastNode,
        &HorizontalTreeNode::nameUpdated,
        self,
        &HorizontalTreeTab::nameUpdated
    );
    lastNode->connect(
        lastNode,
        &HorizontalTreeNode::tooltipUpdated,
        self,
        &HorizontalTreeTab::tooltipUpdated
    );
    lastNode->connect(
        lastNode,
        &HorizontalTreeNode::iconUpdated,
        self,
        &HorizontalTreeTab::iconUpdated
    );
}

//--------------------------------------------------------------------------

HorizontalTreeTab::HorizontalTreeTab(HorizontalTree* tree, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HorizontalTreeTab_p>())
{
    pimpl->self=this;
    pimpl->tree=tree;

    auto l=Layout::vertical(this);

    pimpl->navbar=new NavigationBar(this);
    pimpl->navbar->setCheckable(false);
    pimpl->navbar->setExclusive(false);
    l->addWidget(pimpl->navbar);

    pimpl->scArea=new ScrollArea(this);
    pimpl->scArea->setObjectName("hTreaTabScArea");
    pimpl->scArea->setWidgetResizable(true);
    l->addWidget(pimpl->scArea);

    pimpl->splitter=new QSplitter(pimpl->scArea);
    pimpl->splitter->setOrientation(Qt::Horizontal);
    pimpl->splitter->setObjectName("hTreeTabSplitter");
    pimpl->scArea->setWidget(pimpl->splitter);

    connect(
        pimpl->navbar,
        &NavigationBar::indexClicked,
        this,
        [this](int index)
        {
            auto node=pimpl->nodes[index];
            if (!node->isExpanded())
            {
                node->setExpanded(true);
            }
            pimpl->scArea->ensureWidgetVisible(node);
        }
    );
}

//--------------------------------------------------------------------------

HorizontalTreeTab::~HorizontalTreeTab()
{}

//--------------------------------------------------------------------------

HorizontalTreeNode* HorizontalTreeTab::node() const
{
    if (pimpl->nodes.empty())
    {
        return nullptr;
    }
    return pimpl->nodes.back();
}

//--------------------------------------------------------------------------

void HorizontalTreeTab::openPath(HorizontalTreePath path)
{
    QList<int> splitterSizes;
    HorizontalTreePath partialPath;

    for (size_t i=0;i<path.elements().size();i++)
    {
        const auto& el=path.elements().at(i);
        partialPath.elements().push_back(el);
        auto node=pimpl->tree->nodeFactory()->makeNode(partialPath);
        pimpl->appendNode(node);

        splitterSizes.push_back(el.config().width());

        node->refresh();
    }

    pimpl->splitter->setSizes(splitterSizes);
}

/******************************HorizontalTree******************************/

//--------------------------------------------------------------------------

class HorizontalTree_p
{
    public:

        HorizontalTree* self=nullptr;
        QSplitter* splitter=nullptr;
        HorizontalTreeSideBar* sidebar=nullptr;

        QTabWidget* tabs=nullptr;

        const HorizontalTreeNodeFactory* factory=nullptr;

        std::pair<HorizontalTreeTab*,int> addTab(const HorizontalTreePath& path);
};

//--------------------------------------------------------------------------

std::pair<HorizontalTreeTab*,int> HorizontalTree_p::addTab(const HorizontalTreePath& path)
{
    auto tab=new HorizontalTreeTab(self,splitter);
    auto index=tabs->addTab(tab,QString::fromStdString(path.elements().back().name()));

    tab->connect(
        tab,
        &HorizontalTreeTab::nameUpdated,
        self,
        [self=self,this,index](const QString& val)
        {
           tabs->setTabText(index,val);
        }
    );

    tab->connect(
        tab,
        &HorizontalTreeTab::tooltipUpdated,
        self,
        [self=self,this,index](const QString& val)
        {
            tabs->setTabToolTip(index,val);
        }
    );

    tab->connect(
        tab,
        &HorizontalTreeTab::iconUpdated,
        self,
        [self=self,this,index](const QIcon& val)
        {
            tabs->setTabIcon(index,val);
        }
    );

    return std::make_pair(tab,index);
}

//--------------------------------------------------------------------------

HorizontalTree::HorizontalTree(QWidget* parent)
    : QFrame(parent),
    pimpl(std::make_unique<HorizontalTree_p>())
{
    pimpl->self=this;

    auto l=Layout::vertical(this);

    pimpl->splitter=new QSplitter(this);
    pimpl->splitter->setOrientation(Qt::Horizontal);
    pimpl->splitter->setObjectName("hTreeSplitter");
    l->addWidget(pimpl->splitter);

    pimpl->sidebar=new HorizontalTreeSideBar(this,pimpl->splitter);
    pimpl->splitter->addWidget(pimpl->sidebar);

    pimpl->tabs=new QTabWidget(pimpl->splitter);
    pimpl->tabs->setObjectName("hTreeTabs");
    pimpl->splitter->addWidget(pimpl->tabs);
}

//--------------------------------------------------------------------------

HorizontalTree::~HorizontalTree()
{}

//--------------------------------------------------------------------------

void HorizontalTree::setNodeFactory(const HorizontalTreeNodeFactory* factory) noexcept
{
    pimpl->factory=factory;
}

//--------------------------------------------------------------------------

const HorizontalTreeNodeFactory* HorizontalTree::nodeFactory() const noexcept
{
    return pimpl->factory;
}

//--------------------------------------------------------------------------

int HorizontalTree::tabCount() const
{
    return pimpl->tabs->count();
}

//--------------------------------------------------------------------------

int HorizontalTree::currentTabIndex() const
{
    return pimpl->tabs->currentIndex();
}

//--------------------------------------------------------------------------

void HorizontalTree::setCurrentTab(int tabIndex)
{
    pimpl->tabs->setCurrentIndex(tabIndex);
}

//--------------------------------------------------------------------------

void HorizontalTree::closeTab(int tabIndex)
{
    auto w=pimpl->tabs->widget(tabIndex);
    auto tab=qobject_cast<HorizontalTreeTab*>(w);
    if (tab!=nullptr)
    {
        //! @todo Update side bar
    }
    pimpl->tabs->removeTab(tabIndex);
    destroyWidget(w);
}

//--------------------------------------------------------------------------

void HorizontalTree::closeAllTabs()
{
    while (tabCount()>0)
    {
        closeTab(0);
    }
}

//--------------------------------------------------------------------------

HorizontalTreeTab* HorizontalTree::tab(int tabIndex) const
{
    auto w=pimpl->tabs->widget(tabIndex);
    auto t=qobject_cast<HorizontalTreeTab*>(w);
    return t;
}

//--------------------------------------------------------------------------

std::vector<HorizontalTreePath> HorizontalTree::paths() const
{
    std::vector<HorizontalTreePath> ps;
    for (int i=0;i<tabCount();i++)
    {
        auto t=tab(i);
        if (t!=nullptr)
        {
            auto splitterSizes=t->pimpl->splitter->sizes();
            auto n=t->node();
            if (n!=nullptr)
            {
                auto p=n->path();
                for (int j=0;j<p.elements().size();j++)
                {
                    auto& el=p.elements().at(j);
                    HorizontalTreePathElementConfig cfg{n->isExpanded(),splitterSizes[j]};
                    el.setConfig(std::move(cfg));
                }
                ps.push_back(n->path());
            }
        }
    }
    return ps;
}

//--------------------------------------------------------------------------

void HorizontalTree::loadPaths(const std::vector<HorizontalTreePath>& paths)
{
    closeAllTabs();
    for (auto&& path:paths)
    {
        auto r=pimpl->addTab(path);
        auto index=r.second;
        openPath(path,index);
    }
}

//--------------------------------------------------------------------------

void HorizontalTree::openPath(HorizontalTreePath path, int tabIndex)
{
    auto t=tab(tabIndex);
    if (t==nullptr)
    {
        auto r=pimpl->addTab(path);
        t=r.first;
    }
    t->openPath(std::move(path));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
