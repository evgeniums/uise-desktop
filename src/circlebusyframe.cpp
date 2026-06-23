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

/** @file uise/desktop/src/circlebusyframe.cpp
*
*  Defines CircleBusyFrame.
*
*/

/****************************************************************************/

#include <QVBoxLayout>

#include <uise/desktop/circlebusy.hpp>
#include <uise/desktop/circlebusyframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

CircleBusyFrame::CircleBusyFrame(QWidget* parent,
                                 bool /*centerOnParent*/,
                                 bool disableParentWhenSpinning)
    : AbstractOperationLoadingWidget(parent)
{
    setObjectName("circleBusyFrame");

    m_circleBusy = new CircleBusy(this, false, disableParentWhenSpinning);

    auto l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(m_circleBusy);

    // CircleBusy auto-starts in its constructor; keep the frame idle until start() is called.
    m_circleBusy->stop();
}

//--------------------------------------------------------------------------

CircleBusyFrame::~CircleBusyFrame() = default;

//--------------------------------------------------------------------------

CircleBusy* CircleBusyFrame::circleBusyWidget() const
{
    return m_circleBusy;
}

//--------------------------------------------------------------------------

bool CircleBusyFrame::isRunning() const noexcept
{
    if (m_circleBusy == nullptr)
    {
        return false;
    }
    return m_circleBusy->isRunning();
}

//--------------------------------------------------------------------------

void CircleBusyFrame::start()
{
    if (m_circleBusy != nullptr)
    {
        m_circleBusy->start();
    }
}

//--------------------------------------------------------------------------

void CircleBusyFrame::stop()
{
    if (m_circleBusy != nullptr)
    {
        m_circleBusy->stop();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
