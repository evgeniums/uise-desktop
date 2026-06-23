/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/busywaitingframe.cpp
*
*  Defines BusyWaitingFrame.
*
*/

/****************************************************************************/

#include <uise/desktop/busywaiting.hpp>
#include <uise/desktop/busywaitingframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

BusyWaitingFrame::BusyWaitingFrame(QWidget* parent,
                                   bool centerOnParent,
                                   bool disableParentWhenSpinning)
    : AbstractLoadingWidget(parent)
{
    setObjectName("busyWaitingFrame");
    m_busyWaiting=new BusyWaiting(this,centerOnParent,disableParentWhenSpinning);
    connect(
        m_busyWaiting,
        &BusyWaiting::sizeUpdated,
        this,
        [this](QSize size)
        {
            setFixedSize(size);
        }
        );
    connect(
        m_busyWaiting,
        &BusyWaiting::cancelled,
        this,
        &AbstractLoadingWidget::cancelled
        );
    setFixedSize(m_busyWaiting->size());
}

//--------------------------------------------------------------------------

BusyWaitingFrame::~BusyWaitingFrame() = default;

//--------------------------------------------------------------------------

BusyWaiting* BusyWaitingFrame::busyWaitingWidget() const
{
    return m_busyWaiting;
}

//--------------------------------------------------------------------------

bool BusyWaitingFrame::isRunning() const noexcept
{
    if (m_busyWaiting==nullptr)
    {
        return false;
    }
    return m_busyWaiting->isRunning();
}

//--------------------------------------------------------------------------

void BusyWaitingFrame::start()
{
    if (m_busyWaiting!=nullptr)
    {
        m_busyWaiting->start();
    }
}

//--------------------------------------------------------------------------

void BusyWaitingFrame::stop()
{
    if (m_busyWaiting!=nullptr)
    {
        m_busyWaiting->stop();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
