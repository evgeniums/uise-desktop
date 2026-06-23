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
        QPointer<AbstractLoadingWidget> loadingWidget;
        QPointer<AbstractStatusDialog> statusDialog;

        QFrame* btFrame=nullptr;
        PushButton* cancelButton=nullptr;

        bool busyWaitingMode=false;

        // saved on first construct() to restore after full-frame loading mode
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

void FrameWithModalStatus::reconfigureForLoading()
{
    if (pimpl->popupLayout==nullptr || pimpl->loadingWidget==nullptr)
    {
        return;
    }

    // Clear every item (spacers deleted, widgets only removed from layout).
    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    if (pimpl->loadingWidget->isCenterAligned())
    {
        // Centered spinner: all items in one layout, visibility toggled in popupBusyWaiting/showStatus.
        pimpl->popupLayout->setContentsMargins(pimpl->originalMargins);
        pimpl->popupLayout->setSpacing(pimpl->originalSpacing);
        pimpl->popupLayout->addStretch(1000);
        pimpl->popupLayout->addWidget(pimpl->loadingWidget,0,Qt::AlignCenter);
        pimpl->popupLayout->addWidget(pimpl->statusDialog);
        pimpl->popupLayout->addWidget(pimpl->btFrame);
        pimpl->popupLayout->addStretch(1000);
        setMaxWidthPercent(DefaultMaxWidthPercent);
        setMaxHeightPercent(FrameWithModalPopup::DefaultMaxHeightPercent);
        pimpl->popupWidget->setMaximumWidth(DefaultPopupMaxWidth);
    }
    else
    {
        // Full-frame skeleton: loading widget expands to fill the entire LoadingFrame.
        pimpl->popupLayout->setContentsMargins(0,0,0,0);
        pimpl->popupLayout->setSpacing(0);
        pimpl->popupLayout->addWidget(pimpl->loadingWidget,1);
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

    // Clear every item.
    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    // Status dialog always appears in a constrained, centered popup.
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

    // Save the layout's default margins/spacing so we can restore them after
    // switching back from full-frame loading mode.
    pimpl->originalMargins=pimpl->popupLayout->contentsMargins();
    pimpl->originalSpacing=pimpl->popupLayout->spacing();

    if (pimpl->loadingWidget==nullptr)
    {
        pimpl->loadingWidget=makeWidget<AbstractLoadingWidget>(pimpl->popupWidget);
    }
    else
    {
        pimpl->loadingWidget->setParent(pimpl->popupWidget);
    }
    Q_ASSERT(pimpl->loadingWidget);

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
    if (pimpl->busyWaitingMode)
    {
        return;
    }

    if (pimpl->loadingWidget->isCenterAligned())
    {
        // Centered mode: show/hide within the existing layout.
        pimpl->loadingWidget->setVisible(true);
        pimpl->statusDialog->setVisible(false);
        pimpl->cancelButton->setVisible(pimpl->cancellableBusyWaiting);
    }
    else
    {
        // Full-frame mode: layout may have been reconfigured for status; rebuild for loading.
        pimpl->statusDialog->setVisible(false);
        pimpl->btFrame->setVisible(false);
        reconfigureForLoading();
        pimpl->loadingWidget->setVisible(true);
        pimpl->cancelButton->setVisible(false);
    }

    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    pimpl->loadingWidget->start();
    popup();
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
    pimpl->loadingWidget->stop();

    if (pimpl->loadingWidget->isCenterAligned())
    {
        // Centered mode: show/hide within the existing layout.
        pimpl->loadingWidget->setVisible(false);
        pimpl->statusDialog->setVisible(true);
    }
    else
    {
        // Full-frame mode: switch popup to a constrained status layout.
        pimpl->loadingWidget->setVisible(false);
        reconfigureForStatus();
        pimpl->statusDialog->setVisible(true);
    }

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
    pimpl->loadingWidget->stop();
    closePopup();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* FrameWithModalStatus::loadingWidget() const
{
    return pimpl->loadingWidget;
}

//--------------------------------------------------------------------------

void FrameWithModalStatus::setLoadingWidget(AbstractLoadingWidget* widget)
{
    if (widget==pimpl->loadingWidget)
    {
        return;
    }

    if (pimpl->loadingWidget!=nullptr)
    {
        if (pimpl->popupLayout!=nullptr)
        {
            pimpl->popupLayout->removeWidget(pimpl->loadingWidget);
        }
        pimpl->loadingWidget->deleteLater();
    }

    pimpl->loadingWidget=widget;

    if (pimpl->loadingWidget==nullptr)
    {
        return;
    }

    if (pimpl->popupWidget!=nullptr)
    {
        pimpl->loadingWidget->setParent(pimpl->popupWidget);
    }

    reconfigureForLoading();
}

//--------------------------------------------------------------------------

AbstractStatusDialog* FrameWithModalStatus::statusDialog() const
{
    return pimpl->statusDialog;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
