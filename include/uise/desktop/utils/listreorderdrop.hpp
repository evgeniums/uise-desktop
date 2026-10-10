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
