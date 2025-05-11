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

#include <QSplitter>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

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

class HorizontalTree_p
{
    public:

        QSplitter* navSplitter;

        HorizontalTreeSideBar* nav;
        QSplitter* splitter;
};

//--------------------------------------------------------------------------

HorizontalTree::HorizontalTree(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HorizontalTree_p>())
{
    auto l=Layout::vertical(this);

    pimpl->navSplitter=new QSplitter(this);
    pimpl->navSplitter->setOrientation(Qt::Horizontal);
    pimpl->navSplitter->setObjectName("navSplitter");
    l->addWidget(pimpl->navSplitter);

    pimpl->nav=new HorizontalTreeSideBar(this,pimpl->navSplitter);
    pimpl->navSplitter->addWidget(pimpl->nav);

    pimpl->splitter=new QSplitter(pimpl->navSplitter);
    pimpl->splitter->setOrientation(Qt::Horizontal);
    pimpl->splitter->setObjectName("hTreeSplitter");
    pimpl->navSplitter->addWidget(pimpl->navSplitter);
}

//--------------------------------------------------------------------------

HorizontalTree::~HorizontalTree()
{}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
