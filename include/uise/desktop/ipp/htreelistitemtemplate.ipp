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

/** @file uise/desktop/ipp/htreelistitemtemplate.ipp
*
*  Defines HTreeListItemT.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_IPP
#define UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_IPP

#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/htreelistitemtemplate.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeListItem ******************************/

template <typename BaseT>
class HTreeListItemT_p
{
    public:

        HTreeListItemT<BaseT>* self=nullptr;
        QVBoxLayout* layout=nullptr;
        QWidget* widget=nullptr;

        HTreePathElement pathElement;
        HTreePath residentPath;
        bool selected=false;

        bool doubleClickActivation=false;
        bool ignoreMousePress=false;

        SingleShotTimer* mousePressTimer=nullptr;

        bool openInTabEnabled=true;
        bool openInWindowEnabled=true;
};

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::showMenu(const QPoint&)
{
    auto menu=new QMenu(this);

    auto open=menu->addAction(QObject::tr("Open","HTreeListItem"));
    QObject::connect(open,&QAction::triggered,qobject(),
        [this]()
        {
            emit qobject()->openRequested(pathElement());
        }
    );

    if (pimpl->openInTabEnabled)
    {
        auto openInTab=menu->addAction(QObject::tr("Open in new tab","HTreeListItem"));
        QObject::connect(openInTab,&QAction::triggered,qobject(),
            [this]()
            {
                emit qobject()->openInNewTabRequested(pathElement(),residentPath());
            }
        );
    }

    if (pimpl->openInWindowEnabled)
    {
        auto openInNewWindow=menu->addAction(QObject::tr("Open in new window","HTreeListItem"));
        QObject::connect(openInNewWindow,&QAction::triggered,qobject(),
                [this]()
                {
                    emit qobject()->openInNewTreeRequested(pathElement(),residentPath());
                }
        );
    }

    fillContextMenu(menu);

    menu->setDefaultAction(open);
    menu->exec(QCursor::pos());
}

//--------------------------------------------------------------------------

template <typename BaseT>
HTreeListItemT<BaseT>::HTreeListItemT(HTreePathElement el, QWidget* parent)
    : BaseT(parent),
      pimpl(std::make_unique<HTreeListItemT_p<BaseT>>())
{
    auto self=this;
    m_qobject=new HTreeListItemTQ(this);
    m_qobject->setOnDestroy(
        [self]()
        {
            delete self;
        }
    );

    pimpl->self=this;
    pimpl->pathElement=std::move(el);
    pimpl->layout=Layout::vertical(this);
    this->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    pimpl->mousePressTimer=new SingleShotTimer(this);

    QObject::connect(
        this,
        &Base::customContextMenuRequested,
        this,
        &HTreeListItemT<BaseT>::showMenu
    );
}

//--------------------------------------------------------------------------

template <typename BaseT>
HTreeListItemT<BaseT>::~HTreeListItemT()
{
    m_qobject->resetOnDestroy();
    m_qobject->deleteLater();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setWidget(QWidget* widget)
{
    if (pimpl->widget!=nullptr)
    {
        destroyWidget(pimpl->widget);
    }

    widget->setProperty("htree_item",true);
    pimpl->widget=widget;
    pimpl->layout->addWidget(widget);
    if (this->underMouse())
    {
        pimpl->widget->setProperty("hover",true);
        doSetHovered(true);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
QWidget* HTreeListItemT<BaseT>::widget() const
{
    return pimpl->widget;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setSelected(bool enable)
{
    this->setProperty("selected",enable);
    doSetSelected(enable);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("selected",enable);
        pimpl->widget->style()->unpolish(pimpl->widget);
        pimpl->widget->style()->polish(pimpl->widget);
    }
    this->style()->unpolish(this);
    this->style()->polish(this);
    pimpl->selected=enable;

    emit qobject()->selectionChanged(enable);
}

//--------------------------------------------------------------------------

template <typename BaseT>
bool HTreeListItemT<BaseT>::isSelected() const
{
    return pimpl->selected;
}

//--------------------------------------------------------------------------

template <typename BaseT>
std::string HTreeListItemT<BaseT>::uniqueId() const
{
    return pimpl->pathElement.uniqueId();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    this->setProperty("hover",true);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("hover",true);
        Style::updateWidgetStyle(pimpl->widget);
    }
    doSetHovered(true);
    this->style()->unpolish(this);
    this->style()->polish(this);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    this->setProperty("hover",false);
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setProperty("hover",false);
        Style::updateWidgetStyle(pimpl->widget);
    }
    doSetHovered(false);
    this->style()->unpolish(this);
    this->style()->polish(this);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::click()
{
    if (isSelected())
    {
        setSelected(false);
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
        emit qobject()->openInNewTreeRequested(pathElement(),residentPath());
        return;
    }
    if (QApplication::keyboardModifiers() & Qt::ControlModifier)
    {
        emit qobject()->openInNewTabRequested(pathElement(),residentPath());
        return;
    }
    setSelected(true);
    emit qobject()->openRequested(pathElement());
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);
    if (pimpl->ignoreMousePress)
    {
        return;
    }

    if (event->buttons()&Qt::RightButton)
    {
        return;
    }

    click();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::mouseDoubleClickEvent(QMouseEvent *event)
{
    pimpl->ignoreMousePress=true;
    pimpl->mousePressTimer->shot(
        QApplication::doubleClickInterval(),
        [this]()
        {
            pimpl->ignoreMousePress=false;
        }
    );

    QFrame::mouseDoubleClickEvent(event);
    emit qobject()->activateRequested(pathElement());
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::fillContextMenu(QMenu* menu)
{
    std::ignore=menu;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setPathElement(HTreePathElement pathElement)
{
    pimpl->pathElement=std::move(pathElement);
}

//--------------------------------------------------------------------------

template <typename BaseT>
const HTreePathElement& HTreeListItemT<BaseT>::pathElement() const noexcept
{
    return pimpl->pathElement;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setResidentPath(HTreePath path)
{
    pimpl->residentPath=std::move(path);
}

//--------------------------------------------------------------------------

template <typename BaseT>
const HTreePath& HTreeListItemT<BaseT>::residentPath() const noexcept
{
    return pimpl->residentPath;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setOpenInTabEnabled(bool val)
{
    pimpl->openInTabEnabled=val;
}

//--------------------------------------------------------------------------

template <typename BaseT>
bool HTreeListItemT<BaseT>::isOpenInTabEnabled() const noexcept
{
    return pimpl->openInTabEnabled;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setOpenInWindowEnabled(bool val)
{
    pimpl->openInWindowEnabled=val;
}

//--------------------------------------------------------------------------

template <typename BaseT>
bool HTreeListItemT<BaseT>::isOpenInWindowEnabled() const noexcept
{
    return pimpl->openInWindowEnabled;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::doSetHovered(bool)
{
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::doSetSelected(bool)
{
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_IPP
