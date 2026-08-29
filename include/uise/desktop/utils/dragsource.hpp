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

/** @file uise/desktop/utils/dragsource.hpp
*
*  Declares helpers for starting an OUTGOING file drag (dragging a local file OUT of a widget
*  onto the desktop, a file manager, or another application). This is the only place in the
*  library that constructs a QDrag -- everywhere else (FileUploadWidget, FileDropOverlay) only
*  ever accepts an incoming drop.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DRAGSOURCE_HPP
#define UISE_DESKTOP_DRAGSOURCE_HPP

#include <QPoint>
#include <QPixmap>
#include <QList>
#include <QUrl>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>

class QWidget;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Tracks a press-then-move gesture that may turn into a drag, shared by every
 *  drag-capable widget so the "moved past the threshold" and "was this a plain click"
 *  bookkeeping is written once.
 *
 * Usage: press() on mousePressEvent, movedPastThreshold() on each mouseMoveEvent to decide
 * whether to start a drag, releaseIsClick() on mouseReleaseEvent to decide whether to emit a
 * plain click instead, reset() once the gesture is consumed either way.
 */
class UISE_DESKTOP_EXPORT DragGesture
{
    public:

        DragGesture() noexcept;

        //! Arm the gesture at the given press position (widget-local coordinates).
        void press(const QPoint& pos) noexcept;

        //! Check pos against QApplication::startDragDistance() from the press position and
        //! latch the result -- once this has returned true for a given press, it keeps
        //! returning true (and releaseIsClick() stays false) for the rest of the gesture even
        //! if a later pos happens to fall back within the threshold. Only meaningful while
        //! isArmed(). Not const: this is what makes releaseIsClick() correct, so it must be
        //! called from every mouseMoveEvent while the gesture is armed, not just read lazily
        //! from mouseReleaseEvent.
        bool movedPastThreshold(const QPoint& pos) noexcept;

        //! True if the gesture was armed and never crossed the drag threshold -- i.e. this
        //! release should be treated as a plain click.
        bool releaseIsClick() const noexcept;

        //! Whether press() was called since the last reset().
        bool isArmed() const noexcept;

        //! Clear armed state. Call once the gesture is consumed (drag started, click emitted,
        //! or the widget lost the mouse grab).
        void reset() noexcept;

    private:

        bool m_armed;
        bool m_movedPastThreshold;
        QPoint m_pressPos;
};

/**
 * @brief Start an outgoing local-file drag carrying urls, e.g. text/uri-list for the OS/file
 *  manager/another application to consume.
 * @param source The widget the drag originates from -- becomes QDrag's parent.
 * @param urls Local file URLs to offer (QUrl::fromLocalFile()). Every url should already point
 *  at real, readable bytes -- this function does not resolve/export/decrypt anything, it only
 *  starts the OS-level drag.
 * @param preview Pixmap shown under the cursor while dragging; may be null, in which case Qt's
 *  own default drag cursor is used.
 * @param sourceTag Opaque identity of the widget/context this drag originates from, stamped onto
 *  the mime data as dragSourceMimeFormat() when non-empty. A drop target can compare it back via
 *  mimeDataDragSourceTag() to refuse a payload it itself produced (e.g. a chat page refusing a
 *  file dragged out of one of its own messages). Empty by default -- no restriction.
 * @return false, without starting a drag, when urls is empty or the left mouse button is no
 *  longer down (QApplication::mouseButtons()) -- i.e. the user already released while urls was
 *  still being resolved asynchronously. true once QDrag::exec() has returned.
 */
UISE_DESKTOP_EXPORT bool startFileUrlDrag(QWidget* source, const QList<QUrl>& urls, const QPixmap& preview={}, const QString& sourceTag={});

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DRAGSOURCE_HPP
