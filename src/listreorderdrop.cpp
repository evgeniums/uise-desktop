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

/** @file uise/desktop/src/listreorderdrop.cpp
*
*  Defines ListReorderDrop.
*
*/

/****************************************************************************/

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QTimer>
#include <QWidget>

#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/utils/listreorderdrop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

constexpr int LineThickness=2;

}

//--------------------------------------------------------------------------

QString listReorderTagFor(const QWidget* widget)
{
    for (const QWidget* w=widget;w!=nullptr;w=w->parentWidget())
    {
        const auto tag=w->property(ListReorderTagProperty);
        if (tag.isValid())
        {
            return tag.toString();
        }
    }
    return QString();
}

//--------------------------------------------------------------------------

ListReorderDrop::ListReorderDrop(QWidget* target, RowsFn rowsFn, DroppedFn droppedFn)
    : QObject(target),
      m_target(target),
      m_rowsFn(std::move(rowsFn)),
      m_droppedFn(std::move(droppedFn)),
      m_tag(QStringLiteral("uise-list-reorder-%1").arg(reinterpret_cast<quintptr>(target)))
{
    m_target->setProperty(ListReorderTagProperty,m_tag);
    m_target->setAcceptDrops(true);
    m_target->installEventFilter(this);

    m_line=new QFrame(m_target);
    m_line->setObjectName(QStringLiteral("listReorderIndicator"));
    m_line->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_line->hide();
}

//--------------------------------------------------------------------------

bool ListReorderDrop::placeAt(const QPoint& pos, Place& place) const
{
    const auto rows=m_rowsFn();

    struct Span
    {
        int top;
        int bottom;
    };
    std::vector<Span> spans;
    int draggedHeight=0;
    size_t draggedIndex=0;
    bool draggedFound=false;
    int lineX=0;
    int lineWidth=0;
    for (const auto& row : rows)
    {
        if (row.widget==nullptr || !row.widget->isVisible())
        {
            continue;
        }
        if (row.id==m_draggedId)
        {
            // the row stays where it is while it is dragged, only the others are counted
            draggedHeight=row.widget->height();
            draggedIndex=spans.size();
            draggedFound=true;
            continue;
        }
        const auto topLeft=row.widget->mapTo(m_target,QPoint(0,0));
        if (spans.empty())
        {
            lineX=topLeft.x();
            lineWidth=row.widget->width();
        }
        spans.push_back({topLeft.y(),topLeft.y()+row.widget->height()});
    }
    if (spans.empty())
    {
        return false;
    }
    if (draggedHeight==0)
    {
        draggedHeight=spans.front().bottom-spans.front().top;
    }

    // refuse a drop far from the rows, e.g. over the fixed rows around them
    const int y=pos.y();
    if (y<spans.front().top-draggedHeight || y>spans.back().bottom+draggedHeight)
    {
        return false;
    }

    size_t slot=0;
    for (const auto& span : spans)
    {
        if (y<(span.top+span.bottom)/2)
        {
            break;
        }
        slot++;
    }

    place.slot=slot;
    place.noop=draggedFound && slot==draggedIndex;
    place.lineY=slot==0 ? spans.front().top : spans[slot-1].bottom;
    place.lineX=lineX;
    place.lineWidth=lineWidth;
    return true;
}

//--------------------------------------------------------------------------

void ListReorderDrop::showLine(const Place& place)
{
    if (m_line.isNull())
    {
        return;
    }
    m_line->setGeometry(place.lineX,place.lineY-LineThickness/2,place.lineWidth,LineThickness);
    m_line->show();
    m_line->raise();
}

//--------------------------------------------------------------------------

void ListReorderDrop::hideLine()
{
    if (!m_line.isNull())
    {
        m_line->hide();
    }
}

//--------------------------------------------------------------------------

bool ListReorderDrop::eventFilter(QObject* watched, QEvent* event)
{
    if (watched!=m_target)
    {
        return QObject::eventFilter(watched,event);
    }

    switch (event->type())
    {
        case QEvent::DragEnter:
        case QEvent::DragMove:
        {
            auto* dragEvent=static_cast<QDragMoveEvent*>(event);

            QString tag;
            QString id;
            if (!mimeDataListReorder(dragEvent->mimeData(),tag,id) || tag!=m_tag)
            {
                // someone else's drag, let the target's other handlers see it
                return false;
            }
            m_draggedId=id.toStdString();

            Place place;
            if (!placeAt(dragEvent->position().toPoint(),place))
            {
                hideLine();
                dragEvent->ignore();
                return true;
            }

            dragEvent->setDropAction(Qt::MoveAction);
            dragEvent->accept();
            if (place.noop)
            {
                hideLine();
            }
            else
            {
                showLine(place);
            }
            return true;
        }

        case QEvent::DragLeave:
        {
            hideLine();
            m_draggedId.clear();
            return false;
        }

        case QEvent::Drop:
        {
            auto* dropEvent=static_cast<QDropEvent*>(event);
            hideLine();

            QString tag;
            QString id;
            if (!mimeDataListReorder(dropEvent->mimeData(),tag,id) || tag!=m_tag)
            {
                return false;
            }
            m_draggedId=id.toStdString();

            Place place;
            const bool placed=placeAt(dropEvent->position().toPoint(),place);
            const auto draggedId=m_draggedId;
            m_draggedId.clear();
            if (!placed)
            {
                dropEvent->ignore();
                return true;
            }

            dropEvent->setDropAction(Qt::MoveAction);
            dropEvent->accept();
            if (!place.noop && m_droppedFn)
            {
                // leave the drag's own event loop before the owner starts changing the list
                QTimer::singleShot(0,this,
                    [fn=m_droppedFn,draggedId,slot=place.slot]()
                    {
                        fn(draggedId,slot);
                    }
                );
            }
            return true;
        }

        default:
            break;
    }

    return QObject::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
