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
#include <uise/desktop/busywaiting.hpp>
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
        QPointer<BusyWaiting> busyWaiting;
        QPointer<QFrame> busyWaitingFrame;
        QPointer<AbstractStatusDialog> statusDialog;

        PushButton* cancelButton=nullptr;

        bool busyWaitingMode=false;
};

//--------------------------------------------------------------------------

FrameWithModalStatus::FrameWithModalStatus(QWidget* parent)
    : FrameWithModalPopup(parent),
      pimpl(std::make_unique<FrameWithModalStatus_p>())
{
    //! @todo Move it to construct

    pimpl->popupWidget=new QFrame();
    pimpl->popupWidget->setObjectName("popupFrame");
    auto l=Layout::vertical(pimpl->popupWidget);
    l->addStretch(1000);

    pimpl->busyWaitingFrame=new QFrame(pimpl->popupWidget);
    pimpl->busyWaitingFrame->setObjectName("busyWaitingFrame");
    pimpl->busyWaiting=new BusyWaiting(pimpl->busyWaitingFrame,true,false);
    l->addWidget(pimpl->busyWaitingFrame,0,Qt::AlignCenter);
    connect(
        pimpl->busyWaiting,
        &BusyWaiting::sizeUpdated,
        this,
        [this](QSize size)
        {
            pimpl->busyWaitingFrame->setFixedSize(size);
        }
    );
    pimpl->busyWaitingFrame->setFixedSize(pimpl->busyWaiting->size());

    pimpl->statusDialog=makeWidget<AbstractStatusDialog>(pimpl->popupWidget);
    Q_ASSERT(pimpl->statusDialog);
    l->addWidget(pimpl->statusDialog);

    auto btFrame=new QFrame(pimpl->popupWidget);
    btFrame->setObjectName("buttonsFrame");
    auto btL=Layout::horizontal(btFrame);
    pimpl->cancelButton=new PushButton(tr("Cancel"),btFrame);
    pimpl->cancelButton->setObjectName("cancelBusyButton");
    btL->addWidget(pimpl->cancelButton,0,Qt::AlignHCenter);
    pimpl->cancelButton->setVisible(pimpl->cancellableBusyWaiting);
    l->addWidget(btFrame);

    l->addStretch(1000);

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

    setMaxWidthPercent(DefaultMaxWidthPercent);
    pimpl->popupWidget->setMaximumWidth(DefaultPopupMaxWidth);
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

    pimpl->busyWaiting->setVisible(true);
    pimpl->statusDialog->setVisible(false);
    if (pimpl->cancellableBusyWaiting)
    {
        pimpl->cancelButton->setVisible(true);
    }
    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    pimpl->cancelButton->setVisible(pimpl->cancellableBusyWaiting);
    pimpl->busyWaiting->start();
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
    pimpl->busyWaiting->stop();
    pimpl->busyWaiting->setVisible(false);
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
    pimpl->busyWaiting->stop();
    closePopup();
}

//--------------------------------------------------------------------------

BusyWaiting* FrameWithModalStatus::busyWaitingWidget() const
{
    return pimpl->busyWaiting;
}

//--------------------------------------------------------------------------

AbstractStatusDialog* FrameWithModalStatus::statusDialog() const
{
    return pimpl->statusDialog;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
