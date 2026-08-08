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

/** @file uise/desktop/filedropoverlay.hpp
*
*  Declares FileDropOverlay, a Telegram-style drag-and-drop overlay for accepting files/images
*  dropped onto a host widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILEDROPOVERLAY_HPP
#define UISE_DESKTOP_FILEDROPOVERLAY_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QMimeData;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;

UISE_DESKTOP_NAMESPACE_BEGIN

class FileDropOverlay_p;

/**
 * @brief Telegram-style drag-and-drop overlay covering a host widget, offering one or two drop
 *  targets depending on whether the dragged payload contains images.
 *
 * A consumer page (a chat page, say) calls setAcceptDrops(true) once and install() once. As soon
 * as a drag carrying files or image data enters the page, the overlay covers it: a single
 * full-area panel ("Drop files here to send as documents") when the payload has no images, or two
 * panels ("Send as documents" / "Send as images") when it does -- side by side by default, or
 * stacked top-to-bottom, see setPanelOrientation(). Whichever panel the pointer is over
 * highlights via QSS; dropping on one emits dropped() with that panel, which is what a caller
 * uses to preset FileUploadWidget::setSendAsDocuments() before opening the upload dialog.
 *
 * Unlike RippleOverlay this widget must NOT be transparent for mouse events: Qt resolves a drag
 * target with QWidget::childAt(), which skips both hidden children and children with
 * Qt::WA_TransparentForMouseEvents, so a transparent overlay would never receive a single drag
 * event and per-panel hover would be impossible. It is therefore a real, opaque-to-input child
 * that is only ever visible for the duration of a drag -- see dismiss() and
 * setLeaveWatchdogIntervalMs() for the guarantees that it always goes away again.
 *
 * The host must keep acceptDrops(true) of its own: the overlay starts hidden, and a hidden child
 * is invisible to childAt(), so without a drop-accepting host there would be no drag target in
 * the window at drag-enter time and hence no moment at which to show the overlay. The overlay
 * takes over as the drag target from the first drag-move onwards; the host gets a
 * QDragLeaveEvent at that instant purely because Qt's drag target changed. That event is
 * swallowed by this overlay's event filter on the host while isActive(), so a consumer's own
 * dragLeaveEvent() never sees it -- acting on it would hide the overlay, which would hand the
 * target straight back to the host, which would show it again, at pointer-move frequency.
 */
class UISE_DESKTOP_EXPORT FileDropOverlay : public QFrame
{
    Q_OBJECT

    //! QSS: qproperty-leaveWatchdogIntervalMs: 200; -- 0 disables, see setLeaveWatchdogIntervalMs()
    Q_PROPERTY(int leaveWatchdogIntervalMs READ leaveWatchdogIntervalMs WRITE setLeaveWatchdogIntervalMs)

    //! QSS: qproperty-panelOrientation: "horizontal" | "vertical";
    Q_PROPERTY(QString panelOrientation READ panelOrientationName WRITE setPanelOrientationName)

    public:

        /** @brief Which drop panel an interaction refers to. */
        enum class Panel
        {
            None,       //!< the overlay is not active; never emitted by dropped()
            Documents,  //!< "send as documents" -- original files, no compression
            Images      //!< "send as images" -- quick sending, compressed
        };
        Q_ENUM(Panel)

        constexpr static const bool DefaultAutoShow=true;
        constexpr static const bool DefaultImagesPanelAllowed=true;
        constexpr static const int DefaultLeaveWatchdogIntervalMs=200;
        constexpr static const Qt::Orientation DefaultPanelOrientation=Qt::Horizontal;

        explicit FileDropOverlay(QWidget* host);

        ~FileDropOverlay();

        FileDropOverlay(const FileDropOverlay&)=delete;
        FileDropOverlay(FileDropOverlay&&)=delete;
        FileDropOverlay& operator=(const FileDropOverlay&)=delete;
        FileDropOverlay& operator=(FileDropOverlay&&)=delete;

        /**
         * @brief Install an overlay as a child of host, or return the one already installed.
         * @param host Widget to cover. Keeps ownership. Must have acceptDrops(true).
         *
         * Pushes QSS-supplied values into effect immediately (Style::updateWidgetStyle() plus
         * ensurePolished()) so a widget that is never shown until the first drag still gets its
         * qproperty-* and its icon sizes, same reasoning as RippleOverlay::install().
         */
        static FileDropOverlay* install(QWidget* host);

        /** @brief The overlay previously installed on host, or nullptr. */
        static FileDropOverlay* find(QWidget* host);

        /** @brief Host this overlay was installed on. */
        QWidget* host() const noexcept;

        /**
         * @brief Whether the overlay shows and hides itself from the host's own drag events.
         *
         * True (the default) means the consumer writes no drag handlers at all: the overlay's
         * event filter on the host accepts the drag, shows itself, and routes an early drop
         * that still landed on the host. Set to false when the consumer wants its own
         * dragEnterEvent()/dropEvent() and will call showForMimeData() itself; the filter then
         * still swallows the host's spurious DragLeave while isActive() and still keeps the
         * overlay's geometry and stacking in step with the host.
         */
        void setAutoShow(bool enable) noexcept;
        bool isAutoShow() const noexcept;

        /**
         * @brief Whether the "send as images" panel may appear at all.
         *
         * False forces the single full-area documents panel regardless of payload -- for a
         * consumer that never compresses.
         */
        void setImagesPanelAllowed(bool enable);
        bool isImagesPanelAllowed() const noexcept;

        /**
         * @brief How the two panels are arranged in the two-panel (has-images) layout.
         *
         * Qt::Horizontal (the default) is the brief's own "two panels side by side"; Qt::Vertical
         * stacks documents above images instead -- useful for a host that is taller than it is
         * wide (a narrow chat sidebar, say). Only affects the two-panel layout; the single
         * full-area panel shown when there are no images is unaffected either way.
         */
        void setPanelOrientation(Qt::Orientation orientation);
        Qt::Orientation panelOrientation() const noexcept;

        //! QSS-friendly string form: "horizontal" | "vertical".
        void setPanelOrientationName(const QString& name);
        QString panelOrientationName() const;

        /** @brief Whether the overlay is currently shown for a drag. */
        bool isActive() const noexcept;

        /** @brief Whether the payload passed to the last showForMimeData() contained images. */
        bool hasImages() const noexcept;

        /** @brief Panel currently under the pointer, or Panel::None when !isActive(). */
        Panel hoveredPanel() const noexcept;

        /** @brief Panel frame, for a consumer that wants to add its own children or QSS hooks. */
        QFrame* panelFrame(Panel panel) const;

        //! Caption of the single full-area panel shown when the payload has no images.
        void setSingleCaption(const QString& text);
        QString singleCaption() const;
        void setSingleSubtitle(const QString& text);
        QString singleSubtitle() const;

        //! Caption of the documents panel in the two-panel layout (first/left/top, depending on
        //! panelOrientation()).
        void setDocumentsCaption(const QString& text);
        QString documentsCaption() const;
        void setDocumentsSubtitle(const QString& text);
        QString documentsSubtitle() const;

        //! Caption of the images panel in the two-panel layout (second/right/bottom).
        void setImagesCaption(const QString& text);
        QString imagesCaption() const;
        void setImagesSubtitle(const QString& text);
        QString imagesSubtitle() const;

        /**
         * @brief Interval, in ms, at which an active overlay re-checks the real pointer position
         *  against its own global geometry and dismisses itself when the pointer is no longer
         *  over it. 0 disables the check.
         *
         * A safety net, not the primary path: dragLeaveEvent() normally dismisses the overlay,
         * and is trustworthy here because nothing inside the overlay accepts drops, so a leave
         * delivered to it can only mean the pointer really left. The watchdog covers the cases
         * that event cannot: a drag that entered at the very edge and left again before the
         * overlay ever became Qt's drag target, and any platform that drops the notification.
         *
         * Deliberately NOT a "no dragMoveEvent for N ms" staleness check: Qt's Cocoa plugin
         * answers NO to wantsPeriodicDraggingUpdates, so a pointer held still over a panel
         * produces no drag events at all and such a check would hide the overlay under the user.
         */
        void setLeaveWatchdogIntervalMs(int ms);
        int leaveWatchdogIntervalMs() const noexcept;

        /**
         * @brief Whether this payload is worth showing the overlay for at all: any local-file
         *  URL, raw image data, or one of acceptedImageMimeFormats().
         */
        static bool acceptsMimeData(const QMimeData* mimeData);

    public slots:

        /**
         * @brief Show the overlay over the host, laid out for this payload.
         * @param mimeData Payload being dragged; not retained.
         *
         * Two panels when mimeDataHasImages(mimeData) && isImagesPanelAllowed(), one otherwise.
         * Raises itself, refreshes its icons for the current theme, and starts the leave
         * watchdog. Safe to call while already active (re-lays out in place, no flicker).
         */
        void showForMimeData(const QMimeData* mimeData);

        /** @brief Hide the overlay, clear the hovered panel and stop the watchdog. */
        void dismiss();

    signals:

        /**
         * @brief A drop landed on one of the panels.
         * @param panel Panel::Documents or Panel::Images, never Panel::None.
         * @param mimeData Dropped payload.
         *
         * Emitted synchronously from inside dropEvent(), so mimeData is only valid for the
         * duration of the slot -- consume it there (AbstractFileUploadWidget::addFromMimeData()
         * copies everything it needs) and defer anything that spins the event loop, such as
         * actually opening a dialog, with QTimer::singleShot(0,...): showing a modal dialog from
         * inside a drop handler blocks the platform's drag session.
         */
        void dropped(Panel panel, const QMimeData* mimeData);

        /** @brief Panel under the pointer changed while the overlay is active. */
        void panelHovered(Panel panel);

        /** @brief The overlay was shown or hidden. */
        void activeChanged(bool active);

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dragLeaveEvent(QDragLeaveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    private:

        void updateGeometryFromHost();
        void rebuildPanelsLayout();
        void updatePanels();
        void updateIcons();
        void setHoveredPanel(Panel panel);
        Panel panelAt(const QPoint& pos) const;
        void handleDrop(const QMimeData* mimeData, const QPoint& pos);
        void restartLeaveWatchdog();
        void stopLeaveWatchdog();
        void checkPointerLeft();

        std::unique_ptr<FileDropOverlay_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILEDROPOVERLAY_HPP
