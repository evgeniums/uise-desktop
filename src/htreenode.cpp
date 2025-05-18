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

#include <QPushButton>
#include <QFile>
#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/style.hpp>

#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* HTreeNodeTitleBar *********************************/

namespace{

//! @todo Implemet buttons
QPushButton* iconButton(const QString& iconName, QWidget* parent=nullptr, const QString color="#CCCCCC", int sizeX=12, int sizeY=12)
{
    QString name;
    if (iconName=="close.svg")
    {
        name=":/uise/tabler-icons/outline/x.svg";
    }
    else if (iconName=="collapse.svg")
    {
        name=":/uise/tabler-icons/outline/minus.svg";
    }
    else if (iconName=="refresh.svg")
    {
        name=":/uise/tabler-icons/outline/refresh.svg";
    }
    else if (iconName=="dots-vertical.svg")
    {
        name=":/uise/tabler-icons/outline/dots-vertical.svg";
    }

    QFile file(name);
    file.open(QIODevice::ReadOnly);
    QByteArray baData = file.readAll();
    if (!color.isEmpty())
    {
        auto clr=color.toStdString();
        baData.replace("currentColor",clr.c_str());
    }

    QPixmap px{sizeX,sizeY};
    px.loadFromData(baData,"svg");

    QIcon icon{px};

    QPushButton* bt=new QPushButton(parent);
    bt->setIconSize(QSize(sizeX,sizeY));
    bt->setIcon(icon);
    bt->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    return bt;
}

}
//--------------------------------------------------------------------------

class HTreeNodeTitleBar_p
{
    public:

        HTreeNode* node=nullptr;

        QBoxLayout* layout=nullptr;

        QPushButton* close=nullptr;
        QPushButton* collapse=nullptr;
        QPushButton* refresh=nullptr;

        ElidedLabel* title=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodeTitleBar::HTreeNodeTitleBar(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodeTitleBar_p>())
{
    pimpl->node=node;

    pimpl->close=iconButton("close.svg",this);
    pimpl->close->setToolTip(tr("Close this section with all subsequent sections"));

    pimpl->collapse=iconButton("collapse.svg",this);
    pimpl->collapse->setToolTip(tr("Collapse section"));

    pimpl->refresh=iconButton("refresh.svg",this);
    pimpl->refresh->setToolTip(tr("Refresh"));

    pimpl->title=new ElidedLabel(this);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->title->setElideMode(Qt::ElideMiddle);
    pimpl->title->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);

    pimpl->layout=Layout::horizontal(this);

#ifdef Q_OS_MACOS

    pimpl->layout->addWidget(pimpl->close,0,Qt::AlignLeft);
    pimpl->layout->addWidget(pimpl->collapse,0,Qt::AlignLeft);
    pimpl->layout->addWidget(pimpl->refresh,0,Qt::AlignLeft);
    pimpl->layout->addWidget(pimpl->title,1,Qt::AlignLeft);

#else

    pimpl->layout->addWidget(pimpl->title,1);
    pimpl->layout->addWidget(pimpl->refresh);
    pimpl->layout->addWidget(pimpl->collapse);
    pimpl->layout->addWidget(pimpl->close);

#endif

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    connect(
        pimpl->close,
        &QPushButton::clicked,
        this,
        &HTreeNodeTitleBar::closeRequested
    );
    connect(
        pimpl->collapse,
        &QPushButton::clicked,
        this,
        &HTreeNodeTitleBar::collapseRequested
    );
    connect(
        pimpl->refresh,
        &QPushButton::clicked,
        this,
        &HTreeNodeTitleBar::refreshRequested
    );
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
        QPushButton* expand=nullptr;
};

//--------------------------------------------------------------------------

HTreeNodePlaceHolder::HTreeNodePlaceHolder(HTreeNode* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeNodePlaceHolder_p>())
{
    pimpl->node=node;

    pimpl->layout=Layout::vertical(this);
    pimpl->expand=iconButton("dots-vertical.svg",this,"#777777",16,50);
    pimpl->expand->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    pimpl->layout->addWidget(pimpl->expand,1);

    connect(
        pimpl->expand,
        &QPushButton::clicked,
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
        bool Collapsible=true;
        bool closable=true;

        QPointer<HTreeNode> nextNode;
};

//--------------------------------------------------------------------------

HTreeNode::HTreeNode(HTreeTab* treeTab, QWidget* parent)
    : FrameWithRefresh(parent),
      pimpl(std::make_unique<HTreeNode_p>())
{
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
        pimpl->placeHolder,
        &HTreeNodePlaceHolder::expandRequested,
        this,
        &HTreeNode::expandNode
    );

    setCollapsible(false);
    setClosable(false);
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
    pimpl->titleBar->pimpl->title->setText(val);
    pimpl->path.elements().back().setName(val.toStdString());
    emit nameUpdated(val);
}

//--------------------------------------------------------------------------

void HTreeNode::setNodeTooltip(const QString& val)
{
    pimpl->tooltip=val;
    pimpl->titleBar->pimpl->title->setToolTip(val);
    pimpl->placeHolder->pimpl->expand->setToolTip(val);
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

    pimpl->expanded=false;
    pimpl->placeHolder->setVisible(true);
    pimpl->mainFrame->setVisible(false);
    destroyWidget(pimpl->widget);

    setFixedWidth(pimpl->placeHolder->maximumWidth());
    qDebug() << "collapseNode() minimumWidth=" << minimumWidth() << " minimumWidth="<<minimumWidth();

    emit toggleExpanded(false);
}

//--------------------------------------------------------------------------

void HTreeNode::expandNode()
{
    pimpl->expanded=true;
    pimpl->mainFrame->setVisible(true);
    setContentWidget(createContentWidget());
    refresh();

    emit toggleExpanded(true);
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
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNode::nextNode() const
{
    return pimpl->nextNode;
}

//--------------------------------------------------------------------------

void HTreeNode::nextNodeDestroyed(QObject* obj)
{
    if (!pimpl->nextNode || obj==pimpl->nextNode)
    {
        pimpl->nextNode =nullptr;

        setExpanded(true);
        setCollapsible(false);
    }
    setNextNodeId(std::string{});
}

//--------------------------------------------------------------------------

void HTreeNode::otherNodeExpanded(bool enable)
{
    bool atLeastOneVisible=false;
    bool atLeastOneParentVisible=false;

    auto p=parentNode();
    while (p!=nullptr)
    {
        if (p->isExpanded())
        {
            atLeastOneParentVisible=true;
            atLeastOneVisible=true;
            break;
        }
        p=p->parentNode();
    }
    if (!atLeastOneVisible)
    {
        auto n=pimpl->nextNode.get();
        while(n!=nullptr)
        {
            if (n->isExpanded())
            {
                atLeastOneVisible=true;
                break;
            }
            n=n->nextNode();
        }
    }

    if (nextNode()!=nullptr)
    {
        setCollapsible(atLeastOneVisible);
    }
    else
    {
        setCollapsible(false);
    }
    if (parentNode()!=nullptr)
    {
        setClosable(atLeastOneParentVisible);
    }
    else
    {
        setClosable(false);
    }
}

//--------------------------------------------------------------------------

void HTreeNode::setCollapsible(bool enable)
{
    pimpl->Collapsible=enable;
    pimpl->titleBar->pimpl->collapse->setVisible(enable);
}

//--------------------------------------------------------------------------

bool HTreeNode::isCollapsible() const
{
    return pimpl->Collapsible;
}

//--------------------------------------------------------------------------

void HTreeNode::setClosable(bool enable)
{
    pimpl->closable=enable;
    pimpl->titleBar->pimpl->close->setVisible(enable);
}

//--------------------------------------------------------------------------

bool HTreeNode::isClosable() const
{
    return pimpl->closable;
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

UISE_DESKTOP_NAMESPACE_END
