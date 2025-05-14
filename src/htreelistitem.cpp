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
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/elidedlabel.hpp>

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
        HTreePath residentPath;
        bool selected=false;
};

//--------------------------------------------------------------------------

void HTreeListItem::showMenu(const QPoint&)
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
            emit openInNewTabRequested(pathElement(),residentPath());
        }
    );

    auto openInNewWindow=menu->addAction(tr("Open in new window"));
    connect(openInNewWindow,&QAction::triggered,this,
        [this]()
        {
            emit openInNewTreeRequested(pathElement(),residentPath());
        }
    );

    fillContextMenu(menu);

    menu->setDefaultAction(open);
    menu->exec(QCursor::pos());
}

//--------------------------------------------------------------------------

HTreeListItem::HTreeListItem(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeListItem_p>())
{
    pimpl->self=this;
    pimpl->layout=Layout::vertical(this);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    setContextMenuPolicy(Qt::CustomContextMenu);

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

void HTreeListItem::setSelected(bool enable)
{
    setProperty("selected",enable);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("selected",enable);
    }
    style()->unpolish(this);
    style()->polish(this);
    pimpl->selected=true;

    emit selectionChanged(enable);
}

//--------------------------------------------------------------------------

bool HTreeListItem::isSelected() const
{
    return pimpl->selected;
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
    style()->unpolish(this);
    style()->polish(this);
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
    style()->unpolish(this);
    style()->polish(this);
}

//--------------------------------------------------------------------------

void HTreeListItem::mousePressEvent(QMouseEvent *event)
{        
    QFrame::mousePressEvent(event);
    if (event->buttons()&Qt::RightButton)
    {
        return;
    }

    if (QApplication::keyboardModifiers() & Qt::ShiftModifier
#ifdef Q_OS_MAC
        ||
        (
        QApplication::keyboardModifiers() & Qt::AltModifier
        &&
        QApplication::keyboardModifiers() & Qt::ControlModifier
        )
#endif
        )
    {
        emit openInNewTreeRequested(pathElement(),residentPath());
        return;
    }
    if (QApplication::keyboardModifiers() & Qt::ControlModifier)
    {
        emit openInNewTabRequested(pathElement(),residentPath());
        return;
    }
    setSelected(true);
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

const HTreePathElement& HTreeListItem::pathElement() const noexcept
{
    return pimpl->pathElement;
}

//--------------------------------------------------------------------------

void HTreeListItem::setResidentPath(HTreePath path)
{
    pimpl->residentPath=std::move(path);
}

//--------------------------------------------------------------------------

const HTreePath& HTreeListItem::residentPath() const noexcept
{
    return pimpl->residentPath;
}

/************************* HTreeStandardListItem ******************************/

class HTreeStansardListItem_p
{
    public:

        HTreeStansardListItem* self=nullptr;
        QHBoxLayout* layout=nullptr;
        QLabel* pixmap=nullptr;
        ElidedLabel* text=nullptr;
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
    pimpl->layout->addWidget(pimpl->pixmap);

    pimpl->text=new ElidedLabel(this);
    pimpl->text->setObjectName("hTreeItemText");
    pimpl->layout->addWidget(pimpl->text);
    pimpl->layout->addStretch(1);
    setTextElideMode(Qt::ElideMiddle);

    setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
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
