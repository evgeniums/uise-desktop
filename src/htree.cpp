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

/** @file uise/desktop/htree.cpp
*
*  Defines HTree.
*
*/

/****************************************************************************/

#include <QTabWidget>
#include <QSplitter>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreesidebar.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htree.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class HTree_p
{
    public:

        HTree* self=nullptr;
        QSplitter* splitter=nullptr;
        HTreeSideBar* sidebar=nullptr;

        QTabWidget* tabs=nullptr;

        const HTreeNodeFactory* factory=nullptr;

        std::pair<HTreeTab*,int> addTab(const HTreePath& path);
};

//--------------------------------------------------------------------------

std::pair<HTreeTab*,int> HTree_p::addTab(const HTreePath& path)
{
    auto tab=new HTreeTab(self,splitter);
    auto index=tabs->addTab(tab,QString::fromStdString(path.elements().back().name()));

    tab->connect(
        tab,
        &HTreeTab::nameUpdated,
        self,
        [self=self,this,index](const QString& val)
        {
           tabs->setTabText(index,val);
        }
    );

    tab->connect(
        tab,
        &HTreeTab::tooltipUpdated,
        self,
        [self=self,this,index](const QString& val)
        {
            tabs->setTabToolTip(index,val);
        }
    );

    tab->connect(
        tab,
        &HTreeTab::iconUpdated,
        self,
        [self=self,this,index](const QIcon& val)
        {
            tabs->setTabIcon(index,val);
        }
    );

    return std::make_pair(tab,index);
}

//--------------------------------------------------------------------------

HTree::HTree(QWidget* parent)
    : QFrame(parent),
    pimpl(std::make_unique<HTree_p>())
{
    pimpl->self=this;

    auto l=Layout::vertical(this);

    pimpl->splitter=new QSplitter(this);
    pimpl->splitter->setOrientation(Qt::Horizontal);
    pimpl->splitter->setObjectName("hTreeSplitter");
    l->addWidget(pimpl->splitter);

    pimpl->sidebar=new HTreeSideBar(this,pimpl->splitter);
    pimpl->splitter->addWidget(pimpl->sidebar);

    pimpl->tabs=new QTabWidget(pimpl->splitter);
    pimpl->tabs->setObjectName("hTreeTabs");
    pimpl->splitter->addWidget(pimpl->tabs);
}

//--------------------------------------------------------------------------

HTree::~HTree()
{}

//--------------------------------------------------------------------------

void HTree::setNodeFactory(const HTreeNodeFactory* factory) noexcept
{
    pimpl->factory=factory;
}

//--------------------------------------------------------------------------

const HTreeNodeFactory* HTree::nodeFactory() const noexcept
{
    return pimpl->factory;
}

//--------------------------------------------------------------------------

int HTree::tabCount() const
{
    return pimpl->tabs->count();
}

//--------------------------------------------------------------------------

int HTree::currentTabIndex() const
{
    return pimpl->tabs->currentIndex();
}

//--------------------------------------------------------------------------

void HTree::setCurrentTab(int tabIndex)
{
    pimpl->tabs->setCurrentIndex(tabIndex);
}

//--------------------------------------------------------------------------

void HTree::closeTab(int tabIndex)
{
    auto w=pimpl->tabs->widget(tabIndex);
    auto tab=qobject_cast<HTreeTab*>(w);
    if (tab!=nullptr)
    {
        //! @todo Update side bar
    }
    pimpl->tabs->removeTab(tabIndex);
    destroyWidget(w);
}

//--------------------------------------------------------------------------

void HTree::closeAllTabs()
{
    while (tabCount()>0)
    {
        closeTab(0);
    }
}

//--------------------------------------------------------------------------

HTreeTab* HTree::tab(int tabIndex) const
{
    if (tabIndex==CurrentTabIndex)
    {
        return currentTab();
    }
    else if (tabIndex<0)
    {
        return nullptr;
    }

    auto w=pimpl->tabs->widget(tabIndex);
    auto t=qobject_cast<HTreeTab*>(w);
    return t;
}

//--------------------------------------------------------------------------

HTreeTab* HTree::currentTab() const
{
    auto w=pimpl->tabs->currentWidget();
    auto t=qobject_cast<HTreeTab*>(w);
    return t;
}

//--------------------------------------------------------------------------

std::vector<HTreePath> HTree::paths() const
{
    std::vector<HTreePath> ps;
    for (int i=0;i<tabCount();i++)
    {
        auto t=tab(i);
        if (t!=nullptr)
        {
            ps.push_back(t->path());
        }
    }
    return ps;
}

//--------------------------------------------------------------------------

void HTree::loadPaths(const std::vector<HTreePath>& paths)
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

void HTree::openPath(HTreePath path, int tabIndex)
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

HTreeNode* HTree::node(const HTreePath& path) const
{
    auto find=[&path](const HTreeTab* tab, bool exact) -> HTreeNode*
    {
        auto n=tab->node(path, exact);
        if (n!=nullptr)
        {
            return n;
        }
        return nullptr;
    };

    auto findInTabs=[&find,this](bool exact) -> HTreeNode*
    {
        // first try current tab
        const auto* t=currentTab();
        if (t!=nullptr)
        {
            auto n=find(t,exact);
            if (n!=nullptr)
            {
                return n;
            }
        }

        // try the rest tabs
        for (int i=0;i<tabCount();i++)
        {
            auto n=find(tab(i),exact);
            if (n!=nullptr)
            {
                return n;
            }
        }
        return nullptr;
    };

    // first try to find the first tab where node's path exactly matches the argument
    auto n=findInTabs(true);
    if (n!=nullptr)
    {
        return n;
    }

    // try to find the first tab that contains intermediate node with path matching the argument
    n=findInTabs(false);
    if (n!=nullptr)
    {
        return n;
    }

    // nothing found
    return nullptr;
}

//--------------------------------------------------------------------------

HTreeNode* HTree::showNode(const HTreePath& path)
{
    auto n=node(path);
    if (n==nullptr)
    {
        return nullptr;
    }

    n->treeTab()->raise();
    n->treeTab()->scrollToNode(n);
    return n;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
