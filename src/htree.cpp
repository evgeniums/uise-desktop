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
#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>

#include <uise/desktop/htreesidebar.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htreetabbar.hpp>
#include <uise/desktop/htree.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! QTabWidget::setTabBar() is protected -- this exists solely to expose it to HTree_p. No
//! Q_OBJECT/new behaviour of its own, so it needs no moc entry.
class HTreeTabWidget : public QTabWidget
{
    public:

        using QTabWidget::QTabWidget;
        using QTabWidget::setTabBar;
};

}

//--------------------------------------------------------------------------

class HTree_p
{
    public:

        HTree* self=nullptr;
        QSplitter* splitter=nullptr;
        HTreeSideBar* sidebar=nullptr;

        HTreeTabWidget* tabs=nullptr;

        // Always installed, from construction, as pimpl->tabs's tab bar -- QTabWidget::
        // setTabBar() is documented undefined behaviour once tabs already exist, so this bar
        // is created exactly once, before any addTab() call, rather than swapped in lazily
        // when setTabBarBuilder() is first called. With no builder set, every tab's item()
        // stays null and HTreeTabBar falls back to plain QTabBar rendering/sizing for that
        // index (see htreetabbar.cpp), so behaviour is unaffected until a builder is set.
        HTreeTabBar* bar=nullptr;
        std::shared_ptr<HTreeTabBarBuilder> tabBarBuilder;

        HTreeNodeLocator* locator=nullptr;

        std::pair<HTreeTab*,int> addTab(const HTreePath& path);

        //! The tab-bar item for \p tab, resolved fresh via indexOf() every time -- never
        //! cache the result, same reasoning as addTab()'s own index-capture warning below.
        HTreeTabBarItem* itemFor(HTreeTab* tab) const;

        //! Builds (if a builder is set) and installs the tab-bar item for \p tab at \p index,
        //! seeding it from whatever the QTabWidget already holds for that index and wiring
        //! its closeRequested()/selectRequested() signals. A no-op if no builder is set.
        void installItem(HTreeTab* tab, int index);

        bool singleCollapsePlaceholder=true;
        bool exlusivelyExpandableNode=false;
        bool collapsePlaceholderHidden=true;
        bool internalNodeExpandable=false;
        bool internalNodeClosable=false;
        bool nodesHeaderVisible=true;

        int expandLastDepthOnOpen=0;

        bool navbarSingleVisibleMode=false;
};

//--------------------------------------------------------------------------

HTreeTabBarItem* HTree_p::itemFor(HTreeTab* tab) const
{
    if (tab==nullptr)
    {
        return nullptr;
    }
    auto idx=tabs->indexOf(tab);
    if (idx<0)
    {
        return nullptr;
    }
    return bar->item(idx);
}

//--------------------------------------------------------------------------

void HTree_p::installItem(HTreeTab* tab, int index)
{
    if (tabBarBuilder==nullptr)
    {
        return;
    }

    auto* item=tabBarBuilder->makeItem(tab,bar);
    bar->setItem(index,item);
    if (item==nullptr)
    {
        return;
    }

    item->connect(
        item,
        &HTreeTabBarItem::closeRequested,
        self,
        [this,tab]()
        {
            // Hide the whole tab -- shape and item alike -- synchronously, the instant the
            // user clicks close. HTree::closeTab() below does the real removal, and closing
            // the underlying HTreeTab is not meant to be instantaneous (destroyWidget()'s
            // setParent(nullptr) on a fully-built page, e.g. a live ChatPage, can itself take
            // a perceptible moment even though the actual content teardown is correctly
            // deferred -- see ChatPageNode's own drain queue in chatpagenode.h); calling both
            // in the same synchronous stretch left the tab visibly lingering until that work
            // finished, since nothing forced a repaint in between. Deferring closeTab() by a
            // few milliseconds gives Qt a chance to actually paint the hidden (and, below,
            // relaid-out) tab first -- same idiom as ChatSwitchButton's own
            // DeferredActionDelayMs (chatswitchbutton.cpp).
            // Resolved fresh via indexOf() rather than cached -- same reasoning as every other
            // index lookup in this file, see addTab()'s own comment.
            auto idx=tabs->indexOf(tab);
            if (idx>=0)
            {
                bar->setTabVisible(idx,false);

                if (tabs->count()==2)
                {
                    // Exactly one other tab is left, and removeTab() hasn't run yet (it's
                    // deferred below) -- so for the next few ms that other tab is still the
                    // bar's only *visible* tab, and QTabBar::expanding immediately stretches
                    // it to fill the whole bar width. setTabBarAutoHide() only hides the bar
                    // once count() actually drops to 1, which is just as deferred -- so
                    // without this, closing one of two tabs flashes a single tab stretched
                    // across the full bar right before the bar disappears for good.
                    //
                    // Hide the bar widget itself here, NOT the survivor's tab via
                    // setTabVisible(). QTabBar::visible is persistent per-tab state that
                    // removeTab() of a *different* index never resets (verified in
                    // qtabbar.cpp) -- so setTabVisible(i,false) on the surviving tab would
                    // leave it permanently flagged invisible after the close completes,
                    // reappearing as a missing/blank tab the next time the bar is shown
                    // again (setTabBarAutoHide() only toggles the whole bar's widget
                    // visibility, it never restores any individual tab's flag). Hiding the
                    // bar widget is purely transient view state Qt recomputes from count()
                    // on every insert/remove, so it can't leak like that.
                    bar->hide();
                }

                // setTabVisible() only hides a tab (and its item) and calls update() --
                // unlike removeTab(), it never repositions the tabs after it, since it has no
                // reason to assume the caller wants that (see its own doc comment: it's meant
                // for toggling visibility, not shrinking the bar). Without this, closing a tab
                // in the middle of 3+ open tabs left a visible gap where it used to be, with
                // the tabs after it sitting at their old x-positions, until the deferred
                // closeTab() below did a real removeTab() and finally triggered one.
                bar->scheduleRelayout();
            }
            QTimer::singleShot(10,self,[this,tab](){ self->closeTab(tab); });
        }
    );
    item->connect(
        item,
        &HTreeTabBarItem::selectRequested,
        self,
        [this,tab]()
        {
            self->setCurrentTab(tab);
        }
    );

    // Seed from whatever the QTabWidget already holds at this index -- set moments ago by
    // addTab() below for a brand new tab, or long ago for a pre-existing tab that is only
    // now getting its first item because a builder was registered after tabs were already
    // open. The nameUpdated()/iconUpdated()/tooltipUpdated() forwarders in addTab() keep it
    // live from here on.
    item->setTabText(tabs->tabText(index));
    item->setTabIcon(tabs->tabIcon(index));
    item->setTabTooltip(tabs->tabToolTip(index));
    item->refresh();
}

//--------------------------------------------------------------------------

std::pair<HTreeTab*,int> HTree_p::addTab(const HTreePath& path)
{
    auto tab=new HTreeTab(self,splitter);
    auto index=tabs->addTab(tab,QString::fromStdString(path.name()));

    installItem(tab,index);

    // The tab's index is deliberately NOT captured: QTabWidget re-indexes every tab on
    // removeTab(), so a captured index would, after any tab close, write one tab's
    // title/tooltip/icon onto another tab (or fall silently out of range for the highest tab).
    // tabs->indexOf(tab) is resolved fresh on every emission instead; the tab itself is stable
    // for as long as these connections live, since it is their sender. The tab-bar item is
    // resolved just as freshly via bar->item(idx) -- it is a different widget at a different
    // index the moment another tab closes, exactly like the QTabWidget's own state below.
    tab->connect(
        tab,
        &HTreeTab::nameUpdated,
        self,
        [this,tab](const QString& val)
        {
            auto idx=tabs->indexOf(tab);
            if (idx>=0)
            {
                tabs->setTabText(idx,val);
                if (auto* it=bar->item(idx))
                {
                    it->setTabText(val);
                }
            }
        }
    );

    tab->connect(
        tab,
        &HTreeTab::tooltipUpdated,
        self,
        [this,tab](const QString& val)
        {
            auto idx=tabs->indexOf(tab);
            if (idx>=0)
            {
                // A tab-level tooltip override (see HTreeTab::setTabTooltip()) wins over the
                // last node's own tooltip, which is what this signal argument otherwise carries.
                auto effective=tab->hasTabTooltipOverride() ? tab->tabTooltip() : val;
                tabs->setTabToolTip(idx,effective);
                if (auto* it=bar->item(idx))
                {
                    it->setTabTooltip(effective);
                }
            }
        }
    );

    tab->connect(
        tab,
        &HTreeTab::iconUpdated,
        self,
        [this,tab](const QIcon& val)
        {
            auto idx=tabs->indexOf(tab);
            if (idx>=0)
            {
                // Same override precedence as the tooltip above -- see HTreeTab::setTabIcon().
                auto effective=tab->hasTabIconOverride() ? tab->tabIcon() : val;
                tabs->setTabIcon(idx,effective);
                if (auto* it=bar->item(idx))
                {
                    it->setTabIcon(effective);
                }
            }
        }
    );

    return std::make_pair(tab,index);
}

//--------------------------------------------------------------------------

HTree::HTree(HTreeNodeLocator* locator, QWidget* parent)
    : QFrame(parent),
    pimpl(std::make_unique<HTree_p>())
{
    pimpl->self=this;
    pimpl->locator=locator;

    auto l=Layout::vertical(this);

    pimpl->splitter=new QSplitter(this);
    pimpl->splitter->setOrientation(Qt::Horizontal);
    pimpl->splitter->setObjectName("hTreeSplitter");
    l->addWidget(pimpl->splitter);

    pimpl->sidebar=new HTreeSideBar(this,pimpl->splitter);
    pimpl->splitter->addWidget(pimpl->sidebar);

    pimpl->tabs=new HTreeTabWidget(pimpl->splitter);
    pimpl->tabs->setObjectName("hTreeTabs");

    // Installed unconditionally, before setTabBarAutoHide()/setTabsClosable()/addWidget() and
    // long before any tab exists -- QTabWidget::setTabBar()'s own doc warns the behaviour is
    // undefined once tabs have already been added. See HTree_p::bar's own comment for why the
    // bar stays installed (with every item() null) even when no HTreeTabBarBuilder is ever
    // registered.
    pimpl->bar=new HTreeTabBar(pimpl->tabs);
    pimpl->tabs->setTabBar(pimpl->bar);

    // Both must run after setTabBar(): QTabWidget::setTabBarAutoHide()/setTabsClosable() set
    // properties on -- and (for setTabsClosable) wire tabCloseRequested forwarding against --
    // whichever QTabBar is installed *at the time of the call*. Calling either before
    // setTabBar() would touch the default bar Qt discards a moment later, leaving the real one
    // with autoHide off and tabCloseRequested unwired -- which is exactly what happened here
    // the first time around: the tab bar stayed visible with a single tab open.
    pimpl->tabs->setTabBarAutoHide(true);
    pimpl->tabs->setTabsClosable(true);
    pimpl->splitter->addWidget(pimpl->tabs);

    connect(
        pimpl->tabs,
        &QTabWidget::tabCloseRequested,
        this,
        QOverload<int>::of(&HTree::closeTab)
    );

    connect(
        pimpl->tabs,
        &QTabWidget::currentChanged,
        this,
        [this](int index)
        {
            if (pimpl->tabBarBuilder!=nullptr)
            {
                for (int i=0;i<pimpl->tabs->count();++i)
                {
                    if (auto* it=pimpl->bar->item(i))
                    {
                        it->setCurrent(i==index);
                    }
                }
            }

            // index==-1 is QTabWidget's own "no current tab" -- NOT HTree::CurrentTabIndex, so
            // it must not be forwarded into tab(), which would recurse into currentTab().
            emit currentTabChanged(index<0 ? nullptr : tab(index));
        }
    );
}

//--------------------------------------------------------------------------

HTree::~HTree()
{}

//--------------------------------------------------------------------------

void HTree::setNodeLocator(HTreeNodeLocator* locator) noexcept
{
    pimpl->locator=locator;
}

//--------------------------------------------------------------------------

HTreeNodeLocator* HTree::nodeLocator() const noexcept
{
    return pimpl->locator;
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

void HTree::setCurrentTab(HTreeTab* tab)
{
    pimpl->tabs->setCurrentWidget(tab);
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

    // Self-correcting sweep, independent of whatever hid these tabs: QTabBar::visible is
    // per-tab state that removeTab() only ever clears for the removed index itself (verified
    // in qtabbar.cpp), so any tab left flagged invisible by an earlier close-in-progress (see
    // the setTabVisible()/bar->hide() handling in HTree_p::installItem()) would otherwise
    // survive this removal and render as a missing/blank tab forever after.
    bool restored=false;
    for (int i=0;i<pimpl->tabs->count();++i)
    {
        // Guarded: QTabBar::setTabVisible() assigns d->layoutDirty=(visible!=tab->visible)
        // before its early-return, so an unconditional call would clobber a genuinely
        // pending relayout for tabs that were already visible.
        if (!pimpl->bar->isTabVisible(i))
        {
            pimpl->bar->setTabVisible(i,true);
            restored=true;
        }
    }
    if (restored)
    {
        // setTabVisible(true) only calls update() -- it does not resize the item widget
        // back up from the empty tabRect() an invisible tab reports (see
        // HTreeTabBar::tabLayoutChange()), so a relayout pass is required, not cosmetic.
        pimpl->bar->scheduleRelayout();
    }

    // Belt and braces: nothing currently drives currentIndex() to -1 while tabs remain (the
    // fix above removes the only known producer of that state), but QTabWidget ignores a
    // negative index when syncing its page stack (verified in qtabwidget.cpp), so guard
    // against the tab bar's current-tab highlighting silently drifting from whatever page is
    // actually on screen.
    if (pimpl->tabs->count()>0 && pimpl->tabs->currentIndex()<0)
    {
        auto* shown=pimpl->tabs->currentWidget();
        auto idx=shown!=nullptr ? pimpl->tabs->indexOf(shown) : -1;
        pimpl->tabs->setCurrentIndex(idx>=0 ? idx : 0);
    }
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

void HTree::closeTab(HTreeTab* tab)
{
    if (tab==nullptr)
    {
        return;
    }

    // Resolved fresh rather than accepted as an index from the caller -- same reasoning as
    // every other index lookup in this file, see HTree_p::addTab()'s own comment.
    auto idx=pimpl->tabs->indexOf(tab);
    if (idx>=0)
    {
        closeTab(idx);
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

HTreeTab* HTree::openPath(HTreePath path, int tabIndex)
{
    auto t=tab(tabIndex);
    if (t==nullptr)
    {
        auto r=pimpl->addTab(path);
        t=r.first;
        tabIndex=r.second;
    }
    setCurrentTab(tabIndex);

    // Give every ancestor's layout (this HTree, the surrounding QTabWidget/QSplitter, the host
    // window's own layout, ...) a real, synchronous geometry pass before t->openPath() below
    // builds and inserts this tab's nodes. Without this, the very first openPath() call on a
    // window that has been constructed but never shown/laid out yet (e.g. the initial
    // locked-state path opened from MainWindow's constructor, before MainWindow::show()) leaves
    // HTreeTab's navbar and HTreeSplitterInternal to compute their child geometry from Qt's
    // un-laid-out default widget size -- producing a one-frame flash of stale/short panels that
    // only self-corrects once the deferred layout timers fire on the next event loop turn.
    Layout::activateUpward(t);

    t->openPath(std::move(path));
    return t;
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

HTreeSideBar* HTree::sidebar() const
{
    return pimpl->sidebar;
}

//--------------------------------------------------------------------------

void HTree::activate()
{
    show();
    auto w=window();
    if (w!=nullptr)
    {
        w->show();
        w->setWindowState(w->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
        w->raise();
    }
}

//--------------------------------------------------------------------------

void HTree::setSingleCollapsePlaceholder(bool enable) noexcept
{
    pimpl->singleCollapsePlaceholder=enable;
}

//--------------------------------------------------------------------------

bool HTree::isSingleCollapsePlaceholder() const noexcept
{
    return pimpl->singleCollapsePlaceholder;
}

//--------------------------------------------------------------------------

void HTree::setCollapsePlaceholderHidden(bool enable) noexcept
{
    pimpl->collapsePlaceholderHidden=enable;
}

//--------------------------------------------------------------------------

bool HTree::isCollapsePlacehodlerHidden() const noexcept
{
    return pimpl->collapsePlaceholderHidden;
}

//--------------------------------------------------------------------------

void HTree::setExlusivelyExpandableNode(bool enable) noexcept
{
    pimpl->exlusivelyExpandableNode=enable;
}

//--------------------------------------------------------------------------

bool HTree::isExlusivelyExpandableNode() const noexcept
{
    return pimpl->exlusivelyExpandableNode;
}

//--------------------------------------------------------------------------

void HTree::setExpandableLastDepthOnNodeOpen(int value) noexcept
{
    pimpl->expandLastDepthOnOpen=value;
}

//--------------------------------------------------------------------------

int HTree::expandableLastDepthOnNodeOpen() const noexcept
{
    return pimpl->expandLastDepthOnOpen;
}

//--------------------------------------------------------------------------

void HTree::setInternalNodeExpandable(bool enable) noexcept
{
    pimpl->internalNodeExpandable=enable;
}

//--------------------------------------------------------------------------

bool HTree::isInternalNodeExpandable() const noexcept
{
    return pimpl->internalNodeExpandable;
}

//--------------------------------------------------------------------------

void HTree::setInternalNodeClosable(bool enable) noexcept
{
    pimpl->internalNodeClosable=enable;
}

//--------------------------------------------------------------------------

bool HTree::isInternalNodeClosable() const noexcept
{
    return pimpl->internalNodeClosable;
}

//--------------------------------------------------------------------------

void HTree::setNodeHeaderVisible(bool enable)
{
    pimpl->nodesHeaderVisible=enable;
}

//--------------------------------------------------------------------------

bool HTree::isNodeHeaderVisible() const noexcept
{
    return pimpl->nodesHeaderVisible;
}

//--------------------------------------------------------------------------

void HTree::setNavbarSingleVisibleMode(bool enable)
{
    pimpl->navbarSingleVisibleMode=enable;
}

//--------------------------------------------------------------------------

bool HTree::isNavbarSingleVisibleMode() const noexcept
{
    return pimpl->navbarSingleVisibleMode;
}

//--------------------------------------------------------------------------

void HTree::setTabBarBuilder(std::shared_ptr<HTreeTabBarBuilder> builder)
{
    pimpl->tabBarBuilder=std::move(builder);

    // Native QTabWidget::setTabsClosable() close-x only when nothing else is managing close
    // (see HTreeTabBarItem's own close button once a builder is set).
    pimpl->tabs->setTabsClosable(pimpl->tabBarBuilder==nullptr);

    for (int i=0;i<pimpl->tabs->count();++i)
    {
        auto* t=tab(i);
        if (t==nullptr)
        {
            continue;
        }

        if (pimpl->tabBarBuilder!=nullptr)
        {
            // Only fit tabs that don't already have an item -- calling this again with the
            // same builder must not rebuild every open tab's item from scratch.
            if (pimpl->bar->item(i)==nullptr)
            {
                pimpl->installItem(t,i);
            }
        }
        else
        {
            pimpl->bar->setItem(i,nullptr);
        }
    }
}

//--------------------------------------------------------------------------

std::shared_ptr<HTreeTabBarBuilder> HTree::tabBarBuilder() const
{
    return pimpl->tabBarBuilder;
}

//--------------------------------------------------------------------------

HTreeTabBar* HTree::htreeTabBar() const
{
    return pimpl->tabBarBuilder!=nullptr ? pimpl->bar : nullptr;
}

//--------------------------------------------------------------------------

HTreeTabBarItem* HTree::tabBarItem(HTreeTab* tab) const
{
    if (pimpl->tabBarBuilder==nullptr)
    {
        return nullptr;
    }
    return pimpl->itemFor(tab);
}

//--------------------------------------------------------------------------

void HTree::setTabBarAutoHide(bool enable)
{
    pimpl->tabs->setTabBarAutoHide(enable);
}

//--------------------------------------------------------------------------

bool HTree::isTabBarAutoHide() const noexcept
{
    return pimpl->tabs->tabBarAutoHide();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
