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

/** @file uise/desktop/src/loadingframe.cpp
*
*  Defines LoadingFrame.
*
*/

/****************************************************************************/

#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractloadingwidget.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/loadingframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** LoadingFrame ****************************/

//--------------------------------------------------------------------------

class LoadingFrame_p
{
    public:

        bool cancellableBusyWaiting=false;

        QFrame* popupWidget=nullptr;
        QBoxLayout* popupLayout=nullptr;

        QPointer<AbstractLoadingWidget> operationWidget;
        QPointer<AbstractLoadingWidget> panelWidget;
        bool usingPanelWidget=false;

        bool busyWaitingMode=false;
        QBoxLayout* layout=nullptr;
        QPointer<QWidget> content;
};

//--------------------------------------------------------------------------

LoadingFrame::LoadingFrame(QWidget* parent)
    : FrameWithModalPopup(parent),
      pimpl(std::make_unique<LoadingFrame_p>())
{
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* LoadingFrame::activeLoadingWidget() const
{
    return pimpl->usingPanelWidget ? pimpl->panelWidget.data() : pimpl->operationWidget.data();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* LoadingFrame::inactiveLoadingWidget() const
{
    return pimpl->usingPanelWidget ? pimpl->operationWidget.data() : pimpl->panelWidget.data();
}

//--------------------------------------------------------------------------

void LoadingFrame::reconfigurePopupLayout()
{
    auto* w = activeLoadingWidget();
    if (pimpl->popupLayout==nullptr || w==nullptr)
    {
        return;
    }

    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    if (w->isCenterAligned())
    {
        pimpl->popupLayout->addStretch(1000);
        pimpl->popupLayout->addWidget(w,0,Qt::AlignCenter);
        pimpl->popupLayout->addStretch(1000);
        setMaxWidthPercent(FrameWithModalPopup::DefaultMaxWidthPercent);
        setMaxHeightPercent(FrameWithModalPopup::DefaultMaxHeightPercent);
    }
    else
    {
        pimpl->popupLayout->setContentsMargins(0,0,0,0);
        pimpl->popupLayout->setSpacing(0);
        pimpl->popupLayout->addWidget(w,1);
        setMaxWidthPercent(100);
        setMaxHeightPercent(100);
    }
}

//--------------------------------------------------------------------------

void LoadingFrame::construct()
{
    pimpl->popupWidget=new QFrame();
    pimpl->popupWidget->setObjectName("popupFrame");
    pimpl->popupLayout=Layout::vertical(pimpl->popupWidget);

    if (pimpl->operationWidget==nullptr)
    {
        pimpl->operationWidget=makeWidget<AbstractOperationLoadingWidget>(pimpl->popupWidget);
    }
    else
    {
        pimpl->operationWidget->setParent(pimpl->popupWidget);
    }
    Q_ASSERT(pimpl->operationWidget);

    if (pimpl->panelWidget==nullptr)
    {
        pimpl->panelWidget=makeWidget<AbstractPanelLoadingWidget>(pimpl->popupWidget);
    }
    else
    {
        pimpl->panelWidget->setParent(pimpl->popupWidget);
    }
    Q_ASSERT(pimpl->panelWidget);

    pimpl->usingPanelWidget=false;
    pimpl->panelWidget->setVisible(false);

    reconfigurePopupLayout();
    setPopupWidget(pimpl->popupWidget);

    connect(
        this,
        &LoadingFrame::popupHidden,
        this,
        [this]()
        {
            if (pimpl->busyWaitingMode && pimpl->cancellableBusyWaiting)
            {
                cancel();
            }
        }
    );
}

//--------------------------------------------------------------------------

LoadingFrame::~LoadingFrame()
{}

//--------------------------------------------------------------------------

void LoadingFrame::setCancellableBusyWaiting(bool enable)
{
    pimpl->cancellableBusyWaiting=enable;
}

//--------------------------------------------------------------------------

bool LoadingFrame::isCancellableBusyWaiting() const
{
    return pimpl->cancellableBusyWaiting;
}

//--------------------------------------------------------------------------

void LoadingFrame::popupBusyWaiting()
{
    popupBusyWaiting(false);
}

//--------------------------------------------------------------------------

void LoadingFrame::popupBusyWaiting(bool forLoading)
{
    bool needSwitch=(forLoading!=pimpl->usingPanelWidget);
    bool wasShowing=pimpl->busyWaitingMode;

    if (wasShowing && !needSwitch)
    {
        return;
    }

    if (wasShowing)
    {
        activeLoadingWidget()->stop();
        activeLoadingWidget()->setVisible(false);
    }

    pimpl->usingPanelWidget=forLoading;

    auto* inactive=inactiveLoadingWidget();
    if (inactive)
    {
        inactive->setVisible(false);
    }

    reconfigurePopupLayout();

    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    activeLoadingWidget()->setVisible(true);
    activeLoadingWidget()->start();

    if (!wasShowing)
    {
        popup();
    }
}

//--------------------------------------------------------------------------

QSize LoadingFrame::sizeHint() const
{
    if (pimpl->content!=nullptr)
    {
        auto m=pimpl->content->contentsMargins();
        auto sz=pimpl->content->sizeHint();
        sz.setWidth(sz.width()+m.left()+m.right());
        sz.setHeight(sz.height()+m.top()+m.bottom());
        return sz;
    }
    return FrameWithModalPopup::sizeHint();
}

//--------------------------------------------------------------------------

void LoadingFrame::cancel()
{
    finish();
    emit cancelled();
}

//--------------------------------------------------------------------------

void LoadingFrame::finish()
{
    if (!pimpl->busyWaitingMode)
    {
        return;
    }

    pimpl->busyWaitingMode=false;
    activeLoadingWidget()->stop();
    closePopup();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* LoadingFrame::loadingWidget(bool forLoading) const
{
    return forLoading ? pimpl->panelWidget.data() : pimpl->operationWidget.data();
}

//--------------------------------------------------------------------------

void LoadingFrame::setLoadingWidget(AbstractLoadingWidget* widget, bool forLoading)
{
    auto& slot=forLoading ? pimpl->panelWidget : pimpl->operationWidget;

    if (widget==slot)
    {
        return;
    }

    if (slot!=nullptr)
    {
        if (pimpl->popupLayout!=nullptr)
        {
            pimpl->popupLayout->removeWidget(slot);
        }
        slot->deleteLater();
    }

    slot=widget;

    if (slot==nullptr)
    {
        return;
    }

    if (pimpl->popupWidget!=nullptr)
    {
        slot->setParent(pimpl->popupWidget);
    }

    if (forLoading==pimpl->usingPanelWidget)
    {
        reconfigurePopupLayout();
    }
    else
    {
        slot->setVisible(false);
    }
}

//--------------------------------------------------------------------------

void LoadingFrame::setWidget(QWidget* widget)
{
    destroyWidget(pimpl->content);

    pimpl->content=new QFrame(this);
    pimpl->layout=Layout::vertical(pimpl->content.get());
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
