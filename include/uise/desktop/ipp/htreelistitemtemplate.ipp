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
#include <QPointer>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/dragsource.hpp>
#include <uise/desktop/utils/listreorderdrop.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/htreelistitemtemplate.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/************************* HTreeListItem ******************************/

template <typename BaseT>
class HTreeListItemT_p
{
    public:

        HTreeListItemT<BaseT>* self=nullptr;
        QVBoxLayout* layout=nullptr;
        QWidget* widget=nullptr;

        bool selected=false;

        bool doubleClickActivation=false;
        bool ignoreMousePress=false;

        SingleShotTimer* mousePressTimer=nullptr;

        bool openInTabEnabled=true;
        bool openInWindowEnabled=true;

        bool dragReorder=false;
        DragGesture dragGesture;

        std::function<void (HTreeListItemMenu&)> menuExtender;
        std::function<void (const QPoint&)> menuHandler;

        // Created on first use, items rebuilt on every popup. currentMenu holds the handlers of
        // the host's entries for the popup that is (or was last) shown.
        QPointer<DropdownMenu> contextMenu;
        HTreeListItemMenu currentMenu;

        QPointer<QObject> navigationObj;
        std::function<void(const HTreePathElement&,const HTreePath&)> openFn;
        std::function<void(const HTreePath&,const HTreePath&)> openSubpathFn;
};

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::showMenu(const QPoint& pos)
{
    if (m_buttonMode)
    {
        return;
    }

    if (pimpl->menuHandler)
    {
        pimpl->menuHandler(this->mapToGlobal(pos));
        return;
    }

    // The standard entries use ids below HTreeListItemMenu::FirstCustomId, the host's entries
    // are numbered from it.
    constexpr const int OpenId=0;
    constexpr const int OpenInTabId=1;
    constexpr const int OpenInWindowId=2;

    if (pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu=new DropdownMenu();
        // a context menu is expected to pop up instantly
        pimpl->contextMenu->setAnimationDurationMs(0);
        pimpl->contextMenu->setTriggerWidget(this);
        QObject::connect(
            pimpl->contextMenu,
            &DropdownMenu::itemTriggered,
            qobject(),
            [this](int id)
            {
                switch (id)
                {
                    case (OpenId):
                        requestOpen();
                        break;
                    case (OpenInTabId):
                        requestOpenInNewTab();
                        break;
                    case (OpenInWindowId):
                        requestOpenInNewWindow();
                        break;
                    default:
                    {
                        // copied out first: the handler may reload the list and destroy this row
                        auto handler=pimpl->currentMenu.handler(id);
                        if (handler)
                        {
                            handler();
                        }
                        break;
                    }
                }
            }
        );
    }

    auto icon=[this](const char* alias)
    {
        return Style::instance().svgIconLocator().icon(QString("HTreeListItemMenu::%1").arg(alias),this);
    };

    std::vector<MenuItem> items;
    items.emplace_back(OpenId,QObject::tr("Open","HTreeListItem"),icon("open"));
    if (pimpl->openInTabEnabled)
    {
        items.emplace_back(OpenInTabId,QObject::tr("Open in new tab","HTreeListItem"),icon("openInTab"));
    }
    if (pimpl->openInWindowEnabled)
    {
        items.emplace_back(OpenInWindowId,QObject::tr("Open in new window","HTreeListItem"),icon("openInWindow"));
    }

    pimpl->currentMenu.clear();
    fillContextMenu(pimpl->currentMenu);
    if (pimpl->menuExtender)
    {
        pimpl->menuExtender(pimpl->currentMenu);
    }
    if (!pimpl->currentMenu.isEmpty())
    {
        items.emplace_back(MenuItem::separator());
        for (const auto& item : pimpl->currentMenu.items())
        {
            items.emplace_back(item);
        }
    }

    pimpl->contextMenu->setItems(std::move(items));
    pimpl->contextMenu->popupAt(this->mapToGlobal(pos));
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
    setPathElement(std::move(el));
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
    if (!pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu->closeDropdown(true);
        destroyWidget(pimpl->contextMenu);
    }

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
        Style::updateWidgetStyle(pimpl->widget);
    }
    Style::updateWidgetStyle(this);
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
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::click()
{
    doClick();
    if (m_buttonMode)
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
        requestOpenInNewWindow();
        return;
    }
    if (QApplication::keyboardModifiers() & Qt::ControlModifier)
    {
        requestOpenInNewTab();
        return;
    }
    requestOpen();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::requestOpenInNewWindow()
{
    if (targetSubpath().isNull())
    {
        emit qobject()->openInNewTreeRequested(pathElement(),residentPath());
    }
    else
    {
        emit qobject()->openSubpathInNewTreeRequested(targetSubpath(),residentPath());
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::requestOpenInNewTab()
{
    if (targetSubpath().isNull())
    {
        emit qobject()->openInNewTabRequested(pathElement(),residentPath());
    }
    else
    {
        emit qobject()->openSubpathInNewTabRequested(targetSubpath(),residentPath());
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::requestOpen()
{
    setSelected(true);
    if (pimpl->navigationObj)
    {
        auto sub=targetSubpath();
        auto el=pathElement();
        auto resident=residentPath();
        // Defer navigation by one short cycle so the selection highlight paints
        // before a potentially heavy node starts loading (keeps GUI responsive).
        // Path data is captured by value now so the lambda survives item
        // destruction caused by a concurrent reload(); the branch node is used
        // as context because it outlives individual list items.
        QTimer::singleShot(
            5,
            pimpl->navigationObj,
            [sub=std::move(sub),el=std::move(el),resident=std::move(resident),
             openFn=pimpl->openFn,openSubpathFn=pimpl->openSubpathFn]() mutable
            {
                if (sub.isNull())
                    openFn(el,resident);
                else
                    openSubpathFn(sub,resident);
            }
        );
    }
    else
    {
        QTimer::singleShot(
            5,
            qobject(),
            [this]()
            {
                if (targetSubpath().isNull())
                {
                    emit qobject()->openRequested(pathElement(),residentPath());
                }
                else
                {
                    emit qobject()->openSubpathRequested(targetSubpath(),residentPath());
                }
            }
        );
    }
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

    if (pimpl->dragReorder && !m_buttonMode && event->button()==Qt::LeftButton)
    {
        // The press may turn into a drag, so the click waits for the release. Accepting the press
        // makes this row get the moves and the release even when a child widget got the press.
        pimpl->dragGesture.press(event->position().toPoint());
        event->accept();
        return;
    }

    click();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::mouseMoveEvent(QMouseEvent *event)
{
    if (!pimpl->dragGesture.isArmed())
    {
        QFrame::mouseMoveEvent(event);
        return;
    }

    if (!(event->buttons()&Qt::LeftButton))
    {
        pimpl->dragGesture.reset();
        return;
    }

    if (!pimpl->dragGesture.movedPastThreshold(event->position().toPoint()))
    {
        return;
    }
    pimpl->dragGesture.reset();

    const auto tag=listReorderTagFor(this);
    if (tag.isEmpty())
    {
        return;
    }

    // The drop is reported after the drag has ended, so this row is not touched afterwards: the
    // owner may reload the list and destroy it.
    startListReorderDrag(this,tag,QString::fromStdString(uniqueId()),this->grab());
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::mouseReleaseEvent(QMouseEvent *event)
{
    if (pimpl->dragGesture.isArmed())
    {
        const bool isClick=pimpl->dragGesture.releaseIsClick();
        pimpl->dragGesture.reset();
        if (isClick && event->button()==Qt::LeftButton)
        {
            event->accept();
            click();
            return;
        }
    }

    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setDragReorderEnabled(bool enable)
{
    pimpl->dragReorder=enable;
    if (!enable)
    {
        pimpl->dragGesture.reset();
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
bool HTreeListItemT<BaseT>::isDragReorderEnabled() const noexcept
{
    return pimpl->dragReorder;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setContextMenuExtender(std::function<void (HTreeListItemMenu&)> extender)
{
    pimpl->menuExtender=std::move(extender);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeListItemT<BaseT>::setContextMenuHandler(std::function<void (const QPoint&)> handler)
{
    pimpl->menuHandler=std::move(handler);
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
void HTreeListItemT<BaseT>::fillContextMenu(HTreeListItemMenu& menu)
{
    std::ignore=menu;
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
void HTreeListItemT<BaseT>::setNavigationHandler(
    QObject* obj,
    std::function<void(const HTreePathElement&,const HTreePath&)> openFn,
    std::function<void(const HTreePath&,const HTreePath&)> openSubpathFn
)
{
    pimpl->navigationObj=obj;
    pimpl->openFn=std::move(openFn);
    pimpl->openSubpathFn=std::move(openSubpathFn);
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

}

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_IPP
