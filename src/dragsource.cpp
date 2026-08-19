/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/dragsource.cpp
*
*  Defines helpers for starting an outgoing file drag.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QWidget>

#include <uise/desktop/utils/dragsource.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

DragGesture::DragGesture() noexcept
    : m_armed(false),
      m_movedPastThreshold(false)
{
}

//--------------------------------------------------------------------------

void DragGesture::press(const QPoint& pos) noexcept
{
    m_armed=true;
    m_movedPastThreshold=false;
    m_pressPos=pos;
}

//--------------------------------------------------------------------------

bool DragGesture::movedPastThreshold(const QPoint& pos) noexcept
{
    if (!m_armed)
    {
        return false;
    }

    if (!m_movedPastThreshold
        && (pos-m_pressPos).manhattanLength()>=QApplication::startDragDistance())
    {
        m_movedPastThreshold=true;
    }

    return m_movedPastThreshold;
}

//--------------------------------------------------------------------------

bool DragGesture::releaseIsClick() const noexcept
{
    return m_armed && !m_movedPastThreshold;
}

//--------------------------------------------------------------------------

bool DragGesture::isArmed() const noexcept
{
    return m_armed;
}

//--------------------------------------------------------------------------

void DragGesture::reset() noexcept
{
    m_armed=false;
    m_movedPastThreshold=false;
}

//--------------------------------------------------------------------------

bool startFileUrlDrag(QWidget* source, const QList<QUrl>& urls, const QPixmap& preview)
{
    if (urls.isEmpty())
    {
        return false;
    }

    // The export/staging that produced urls is asynchronous -- by the time it lands the user
    // may already have released the button. Starting QDrag::exec() then would pop a drag that
    // never terminates from a real button-up, so bail out silently rather than start it.
    if (!(QApplication::mouseButtons() & Qt::LeftButton))
    {
        return false;
    }

    auto* drag=new QDrag(source);
    auto* mimeData=new QMimeData();
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);
    if (!preview.isNull())
    {
        drag->setPixmap(preview);
    }

    drag->exec(Qt::CopyAction);
    return true;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
