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

/** @file uise/desktop/src/framewithmodalstatus.cpp
*
*  Defines FrameWithModalStatus.
*
*/

/****************************************************************************/

#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractloadingwidget.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/framewithmodalstatus.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** FrameWithModalStatus ****************************/

//--------------------------------------------------------------------------

class FrameWithModalStatus_p
{
    public:

        bool cancellableBusyWaiting=false;

        QFrame* popupWidget=nullptr;
        QBoxLayout* popupLayout=nullptr;

        QPointer<AbstractLoadingWidget> operationWidget;
        QPointer<AbstractLoadingWidget> panelWidget;
        bool usingPanelWidget=false;

        QPointer<AbstractStatusDialog> statusDialog;

        QFrame* btFrame=nullptr;
        PushButton* cancelButton=nullptr;

        bool busyWaitingMode=false;

        QMargins originalMargins;
        int originalSpacing=-1;
};

//--------------------------------------------------------------------------

FrameWithModalStatus::FrameWithModalStatus(QWidget* parent)
    : FrameWithModalPopup(parent),
      pimpl(std::make_unique<FrameWithModalStatus_p>())
{
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* FrameWithModalStatus::activeLoadingWidget() const
{
    return pimpl->usingPanelWidget ? pimpl->panelWidget.data() : pimpl->operationWidget.data();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* FrameWithModalStatus::inactiveLoadingWidget() const
{
    return pimpl->usingPanelWidget ? pimpl->operationWidget.data() : pimpl->panelWidget.data();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::reconfigureForLoading()
{
    auto* active=activeLoadingWidget();
    if (pimpl->popupLayout==nullptr || active==nullptr)
    {
        return;
    }

    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    if (active->isCenterAligned())
    {
        pimpl->popupLayout->setContentsMargins(pimpl->originalMargins);
        pimpl->popupLayout->setSpacing(pimpl->originalSpacing);
        pimpl->popupLayout->addStretch(1000);
        pimpl->popupLayout->addWidget(active,0,Qt::AlignCenter);
        pimpl->popupLayout->addWidget(pimpl->statusDialog);
        pimpl->popupLayout->addWidget(pimpl->btFrame);
        pimpl->popupLayout->addStretch(1000);
        setMaxWidthPercent(DefaultMaxWidthPercent);
        setMaxHeightPercent(FrameWithModalPopup::DefaultMaxHeightPercent);
        pimpl->popupWidget->setMaximumWidth(DefaultPopupMaxWidth);
    }
    else
    {
        pimpl->popupLayout->setContentsMargins(0,0,0,0);
        pimpl->popupLayout->setSpacing(0);
        pimpl->popupLayout->addWidget(active,1);
        setMaxWidthPercent(100);
        setMaxHeightPercent(100);
        pimpl->popupWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    }
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::reconfigureForStatus()
{
    if (pimpl->popupLayout==nullptr)
    {
        return;
    }

    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    pimpl->popupLayout->setContentsMargins(pimpl->originalMargins);
    pimpl->popupLayout->setSpacing(pimpl->originalSpacing);
    pimpl->popupLayout->addStretch(1000);
    pimpl->popupLayout->addWidget(pimpl->statusDialog);
    pimpl->popupLayout->addWidget(pimpl->btFrame);
    pimpl->popupLayout->addStretch(1000);
    setMaxWidthPercent(DefaultMaxWidthPercent);
    setMaxHeightPercent(FrameWithModalPopup::DefaultMaxHeightPercent);
    pimpl->popupWidget->setMaximumWidth(DefaultPopupMaxWidth);
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::construct()
{
    pimpl->popupWidget=new QFrame();
    pimpl->popupWidget->setObjectName("popupFrame");
    pimpl->popupLayout=Layout::vertical(pimpl->popupWidget);

    pimpl->originalMargins=pimpl->popupLayout->contentsMargins();
    pimpl->originalSpacing=pimpl->popupLayout->spacing();

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

    pimpl->statusDialog=makeWidget<AbstractStatusDialog>(pimpl->popupWidget);
    Q_ASSERT(pimpl->statusDialog);

    pimpl->btFrame=new QFrame(pimpl->popupWidget);
    pimpl->btFrame->setObjectName("buttonsFrame");
    auto btL=Layout::horizontal(pimpl->btFrame);
    pimpl->cancelButton=new PushButton(tr("Cancel"),pimpl->btFrame);
    pimpl->cancelButton->setObjectName("cancelBusyButton");
    btL->addWidget(pimpl->cancelButton,0,Qt::AlignHCenter);
    pimpl->cancelButton->setVisible(pimpl->cancellableBusyWaiting);

    reconfigureForLoading();

    setPopupWidget(pimpl->popupWidget);

    connect(
        pimpl->cancelButton,
        &PushButton::clicked,
        this,
        [this]()
        {
            if (pimpl->cancellableBusyWaiting)
            {
                cancel();
            }
            else
            {
                closePopup();
            }
        }
        );

    connect(
        this,
        &FrameWithModalStatus::popupHidden,
        this,
        [this]()
        {
            if (pimpl->busyWaitingMode && pimpl->cancellableBusyWaiting)
            {
                cancel();
            }
        }
        );

    connect(
        pimpl->statusDialog,
        &StatusDialog::closeRequested,
        this,
        [this]()
        {
            closePopup();
        }
        );
}

//--------------------------------------------------------------------------

FrameWithModalStatus::~FrameWithModalStatus()
{}

//--------------------------------------------------------------------------

void FrameWithModalStatus::setCancellableBusyWaiting(bool enable)
{
    pimpl->cancellableBusyWaiting=enable;
}

//--------------------------------------------------------------------------

bool FrameWithModalStatus::isCancellableBusyWaiting() const
{
    return pimpl->cancellableBusyWaiting;
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::popupBusyWaiting()
{
    popupBusyWaiting(false);
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::popupBusyWaiting(bool forLoading)
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

    auto* active=activeLoadingWidget();
    bool centered=active->isCenterAligned();

    if (!centered)
    {
        pimpl->statusDialog->setVisible(false);
        pimpl->btFrame->setVisible(false);
    }
    else
    {
        pimpl->statusDialog->setVisible(false);
    }

    reconfigureForLoading();
    active->setVisible(true);
    pimpl->cancelButton->setVisible(centered && pimpl->cancellableBusyWaiting);

    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    active->start();

    if (!wasShowing)
    {
        popup();
    }
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::popupStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    pimpl->statusDialog->setStatus(message,title,std::move(icon));
    showStatus();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::popupStatus(const QString& message, StatusDialog::Type type, const QString& title)
{
    pimpl->statusDialog->setStatus(message,type,title);
    showStatus();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::showStatus()
{
    activeLoadingWidget()->stop();

    pimpl->operationWidget->setVisible(false);
    pimpl->panelWidget->setVisible(false);

    reconfigureForStatus();
    pimpl->statusDialog->setVisible(true);

    pimpl->cancelButton->setVisible(false);
    pimpl->busyWaitingMode=false;
    setShortcutEnabled(true);
    popup();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::cancel()
{
    finish();
    emit cancelled();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::finish()
{
    pimpl->busyWaitingMode=false;
    activeLoadingWidget()->stop();
    closePopup();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* FrameWithModalStatus::loadingWidget(bool forLoading) const
{
    return forLoading ? pimpl->panelWidget.data() : pimpl->operationWidget.data();
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::setLoadingWidget(AbstractLoadingWidget* widget, bool forLoading)
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
        reconfigureForLoading();
    }
    else
    {
        slot->setVisible(false);
    }
}

//--------------------------------------------------------------------------

AbstractStatusDialog* FrameWithModalStatus::statusDialog() const
{
    return pimpl->statusDialog;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
