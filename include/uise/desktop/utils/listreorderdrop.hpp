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

/** @file uise/desktop/utils/listreorderdrop.hpp
*
*  Declares ListReorderDrop, the drop side of dragging a row to a new place in a list.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LISTREORDERDROP_HPP
#define UISE_DESKTOP_LISTREORDERDROP_HPP

#include <functional>
#include <string>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>

class QEvent;
class QFrame;
class QWidget;

UISE_DESKTOP_NAMESPACE_BEGIN

//! Name of the dynamic property that marks a widget as the target of a ListReorderDrop and holds
//! the list tag.
constexpr const char* ListReorderTagProperty="uiseListReorderTag";

/**
 * @brief Get the tag of the list a row belongs to.
 * @param widget Row widget, or any widget inside a list.
 * @return Tag of the nearest ancestor (or the widget itself) that is a ListReorderDrop target;
 *  empty if there is none, meaning a row dragged from here has nowhere to be dropped.
 */
UISE_DESKTOP_EXPORT QString listReorderTagFor(const QWidget* widget);

/**
 * @brief Get the widget that is the target of a ListReorderDrop and holds the given widget.
 * @param widget Row widget, or any widget inside a list.
 * @return The nearest ancestor (or the widget itself) that is a ListReorderDrop target, null if there is none.
 */
UISE_DESKTOP_EXPORT QWidget* listReorderTargetFor(const QWidget* widget);

/**
 * @brief Hand a drag event that a row received to the list the row belongs to.
 *
 * A ListReorderDrop sees only the drag events that are sent to its target widget. Qt sends a drag event to the first widget
 * under the cursor that accepts drops and does not pass it on when that widget ignores it. A row that accepts drops for its own
 * reasons (a chat row takes files) therefore keeps every reorder drag away from the list. Such a row calls this first in its
 * dragEnterEvent(), dragMoveEvent(), dragLeaveEvent() and dropEvent(), and goes on with its own handling when it returns false:
 *
 * @code
 * void MyRow::dragEnterEvent(QDragEnterEvent* event)
 * {
 *     if (forwardListReorderDrag(this,event))
 *     {
 *         return;
 *     }
 *     // the row's own drags
 * }
 * @endcode
 *
 * Enter, move and drop events of a reorder drag started in the row's own list are handed to the list's ListReorderDrop with the
 * position translated to the list's coordinates (ListReorderDrop::handleDragEvent(), not a sent event: see there), and the
 * answer (accepted or ignored, the drop action) is copied back to the event.
 * A leave event is passed on whatever the drag is, which only hides the indicator line, and false is returned for it.
 *
 * @param row The widget that received the event.
 * @param event Event of type DragEnter, DragMove, DragLeave or Drop.
 * @return true if the event was a reorder drag of the row's list and is handled; false for any other event, drag, or a row that
 *  is in no list with a ListReorderDrop.
 */
UISE_DESKTOP_EXPORT bool forwardListReorderDrag(QWidget* row, QEvent* event);

/**
 * @brief Drop side of moving a row to a new place in a list by dragging it.
 *
 * Makes target accept drags started with startListReorderDrag() from rows of its own list, shows
 * a thin line between rows where the dragged row would land, and reports the place on drop. It
 * changes nothing in the list itself: what the new place means (a stored position, a pin) and
 * the reload that follows are up to the owner.
 *
 * The place is a slot: the index the dragged row takes among the rows of the list once it has
 * been taken out of it. Slot 0 is above the first row and slot N is below the last of the N
 * other rows. A drop that would leave the row where it is is not reported.
 *
 * Only rows returned by the rows function take part, so fixed rows of the same list (headers,
 * commands) are neither dragged nor counted. A drop is accepted when the cursor is over the rows
 * or within one row height above or below them; further away it is refused.
 *
 * The line is a QFrame named "listReorderIndicator", coloured by listreorder.qss.
 *
 * The object is a child of target and lives as long as it does.
 */
class UISE_DESKTOP_EXPORT ListReorderDrop : public QObject
{
    public:

        struct Row
        {
            std::string id;
            QWidget* widget=nullptr;
        };

        //! Rows that can be moved, in the order they are shown. Called on every drag move.
        using RowsFn=std::function<std::vector<Row>()>;

        //! Called after the drop, outside of the drag's own event loop.
        using DroppedFn=std::function<void (const std::string& id, size_t slot)>;

        ListReorderDrop(QWidget* target, RowsFn rowsFn, DroppedFn droppedFn);

        QString tag() const
        {
            return m_tag;
        }

        void setDroppedFn(DroppedFn droppedFn)
        {
            m_droppedFn=std::move(droppedFn);
        }

        /**
         * @brief Handle a drag event of the target directly, the way the event filter does.
         * @param event Event of type DragEnter, DragMove, DragLeave or Drop, with the position in the target's coordinates.
         * @return true if the event was a reorder drag of this list and is handled.
         *
         * For forwardListReorderDrag(): a drag event cannot be sent to the target with QCoreApplication::sendEvent(), because
         * QApplication::notify() redirects DragMove and Drop to the widget that accepted the DragEnter, and that is the row
         * that forwards them.
         */
        bool handleDragEvent(QEvent* event);

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        struct Place
        {
            size_t slot=0;
            int lineY=0;
            int lineX=0;
            int lineWidth=0;
            bool noop=false;
        };

        bool placeAt(const QPoint& pos, Place& place) const;
        void showLine(const Place& place);
        void hideLine();

        QWidget* m_target;
        RowsFn m_rowsFn;
        DroppedFn m_droppedFn;
        QString m_tag;
        std::string m_draggedId;
        QPointer<QFrame> m_line;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LISTREORDERDROP_HPP
