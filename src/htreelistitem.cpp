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

/** @file uise/desktop/htreelistitem.cpp
*
*  Defines HTreeListItem and HTreeStansardListItem.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QMenu>

#include <uise/desktop/utils/layout.hpp>

#include <uise/thirdparty/elidedlabel.h>
#include <uise/desktop/htreelistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeListItem ******************************/

class HTreeListItem_p
{
    public:

        HTreeListItem* self=nullptr;
        QVBoxLayout* layout=nullptr;
        QWidget* widget=nullptr;

        HTreePathElement pathElement;
};

//--------------------------------------------------------------------------

void HTreeListItem::showMenu(const QPoint& pos)
{
    auto menu=new QMenu(this);

    auto open=menu->addAction(tr("Open"));
    connect(open,&QAction::triggered,this,
        [this]()
        {
            emit openRequested(pathElement());
        }
    );

    auto openInTab=menu->addAction(tr("Open in new tab"));
    connect(openInTab,&QAction::triggered,this,
        [this]()
        {
            emit openInNewTabRequested(pathElement());
        }
    );

    fillContextMenu(menu);

    menu->setDefaultAction(open);
    menu->exec(pos);
}

//--------------------------------------------------------------------------

HTreeListItem::HTreeListItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeListItem_p>())
{
    pimpl->self=this;
    pimpl->layout=Layout::vertical(this);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    connect(
        this,
        &HTreeListItem::customContextMenuRequested,
        this,
        &HTreeListItem::showMenu
    );
}

//--------------------------------------------------------------------------

HTreeListItem::~HTreeListItem()
{}

//--------------------------------------------------------------------------

void HTreeListItem::setWidget(QWidget* widget)
{
    widget->setProperty("htree_item",true);
    pimpl->widget=widget;
    pimpl->layout->addWidget(widget);
    if (underMouse())
    {
        pimpl->widget->setProperty("hover",true);
    }
}

//--------------------------------------------------------------------------

void HTreeListItem::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    setProperty("hover",true);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("hover",true);
    }
}

//--------------------------------------------------------------------------

void HTreeListItem::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    setProperty("hover",false);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("hover",false);
    }
}

//--------------------------------------------------------------------------

void HTreeListItem::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);
    emit openRequested(pathElement());
}

//--------------------------------------------------------------------------

void HTreeListItem::fillContextMenu(QMenu* menu)
{
    std::ignore=menu;
}

//--------------------------------------------------------------------------

void HTreeListItem::setPathElement(HTreePathElement pathElement)
{
    pimpl->pathElement=std::move(pathElement);
}

//--------------------------------------------------------------------------

const HTreePathElement& HTreeListItem::pathElement() const
{
    return pimpl->pathElement;
}

/************************* HTreeStandardListItem ******************************/

class HTreeStansardListItem_p
{
    public:

        HTreeStansardListItem* self=nullptr;
        QHBoxLayout* layout=nullptr;
        QLabel* pixmap=nullptr;
        UISE_THIRDPARTY_NAMESPACE::ElidedLabel* text=nullptr;
};

//--------------------------------------------------------------------------

HTreeStansardListItem::HTreeStansardListItem(const QString& type, QWidget* parent)
    : HTreeListItem(parent),
        pimpl(std::make_unique<HTreeStansardListItem_p>())
{
    pimpl->self=this;
    pimpl->layout=Layout::horizontal(this);

    pimpl->pixmap=new QLabel(this);
    pimpl->pixmap->setObjectName("hTreeItemPixmap");
    pimpl->layout->addWidget(pimpl->pixmap,0,Qt::AlignLeft);

    pimpl->text=new UISE_THIRDPARTY_NAMESPACE::ElidedLabel(this);
    pimpl->text->setObjectName("hTreeItemText");
    pimpl->layout->addWidget(pimpl->text,0,Qt::AlignLeft);
    setTextElideMode(Qt::ElideMiddle);

    pimpl->layout->addSpacing(1);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

HTreeStansardListItem::~HTreeStansardListItem()
{}

//--------------------------------------------------------------------------

void HTreeStansardListItem::setText(const QString& text)
{
    pimpl->text->setText(text);
}

//--------------------------------------------------------------------------

void HTreeStansardListItem::setPixmap(const QPixmap& pixmap)
{
    pimpl->pixmap->setPixmap(pixmap);
}

//--------------------------------------------------------------------------

QString HTreeStansardListItem::text() const
{
    return pimpl->text->text();
}

//--------------------------------------------------------------------------

QPixmap HTreeStansardListItem::pixmap() const
{
    return pimpl->pixmap->pixmap();
}

//--------------------------------------------------------------------------

void HTreeStansardListItem::setTextElideMode(Qt::TextElideMode mode)
{
    pimpl->text->setElideMode(mode);
}

//--------------------------------------------------------------------------

Qt::TextElideMode HTreeStansardListItem::textElideMode() const
{
    return pimpl->text->elideMode();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
