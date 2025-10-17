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
#include <uise/desktop/busywaiting.hpp>
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
        QPointer<BusyWaiting> busyWaiting;
        QPointer<QFrame> busyWaitingFrame;

        bool busyWaitingMode=false;
};

//--------------------------------------------------------------------------

LoadingFrame::LoadingFrame(QWidget* parent)
    : FrameWithModalPopup(parent),
      pimpl(std::make_unique<LoadingFrame_p>())
{
}

//--------------------------------------------------------------------------

void LoadingFrame::construct()
{
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

    l->addStretch(1000);
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
    if (pimpl->busyWaitingMode)
    {
        return;
    }

    pimpl->busyWaiting->setVisible(true);
    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    pimpl->busyWaiting->start();
    popup();
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
    pimpl->busyWaiting->stop();
    closePopup();
}

//--------------------------------------------------------------------------

BusyWaiting* LoadingFrame::busyWaitingWidget() const
{
    return pimpl->busyWaiting;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
