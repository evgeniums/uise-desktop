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

/** @file uise/desktop/dropdownframe.hpp
*
*  Declares DropdownFrame.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DROPDOWNFRAME_HPP
#define UISE_DESKTOP_DROPDOWNFRAME_HPP

#include <memory>

#include <QFrame>
#include <QEasingCurve>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class DropdownFrame_p;

/**
 * @brief Overlay frame that shows/hides its content() widget with an animated grow/shrink,
 *  anchored below (or, when there is not enough room, above) a trigger widget.
 *
 * This is the machinery originally built for FastSwitchButtonDropdown, extracted so it can be
 * reused by any anchored, animated popup (see DropdownMenu). The frame is a genuine top-level
 * window (Qt::Tool | Qt::FramelessWindowHint, translucent background), not embedded into the
 * anchor/trigger's own window, so it can grow past the edges of the host window exactly like a
 * native menu -- it is positioned in global (screen) coordinates and never clipped by the host
 * window's bounds, only by the screen's available geometry. It deliberately does not use
 * Qt::Popup: a native popup cannot animate its own dismissal (ignoring its QCloseEvent to run a
 * shrink would leave it holding the application-wide popup mouse grab), so this frame drives its
 * own dismissal instead (see eventFilter()). Qt::WA_ShowWithoutActivating keeps the host window
 * active while the frame is open, so opening it never triggers the frame's own
 * CloseReason::WindowChanged dismissal.
 *
 * Self-dismissal also covers a press on a window's native frame (title bar, its minimize/zoom/
 * close buttons, a resize border) and the host window being hidden, minimized, closed, or the
 * whole application losing activation (see eventFilter()) -- without this a dropdown could
 * otherwise be left floating on screen with no parent window still visible. On X11 the window
 * manager owns the decorations rather than Qt, so a bare title-bar click there is not observed;
 * dragging/resizing the host, switching applications, and hiding/minimizing it still dismiss the
 * dropdown correctly on that platform.
 *
 * The frame intentionally has NO QLayout. Its content() widget is sized once per opening and
 * never touched again; growing/shrinking the frame with setGeometry() therefore clips the
 * content instead of relaying it out, which is what makes the width+height animation a clip
 * rather than a squeeze. Do not add a layout to this class.
 *
 * Subclasses that own their content internally (e.g. DropdownMenu) override fillContent()/
 * clearContent() to build/tear down their rows on demand. Subclasses that merely reuse this as
 * an animated container for externally-owned content (e.g. FastSwitchButtonDropdown) can leave
 * both hooks at their empty default and drive setContent() themselves.
 */
class UISE_DESKTOP_EXPORT DropdownFrame : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(int animationDurationMs READ animationDurationMs WRITE setAnimationDurationMs)
    Q_PROPERTY(int easingCurveType READ easingCurveType WRITE setEasingCurveType)
    Q_PROPERTY(int offsetX READ offsetX WRITE setOffsetX)
    Q_PROPERTY(int offsetY READ offsetY WRITE setOffsetY)

    public:

        /**
         * @brief Reason a dropdown closed itself without an explicit closeDropdown() call from
         *  the owner.
         */
        enum class CloseReason : uint8_t
        {
            Programmatic,   //!< closeDropdown() was called directly by the owner.
            Escape,         //!< the Escape key was pressed while the frame was open.
            OutsideClick,   //!< a mouse press landed outside both the frame and the trigger widget,
                            //!< including a press on any window's native frame (title bar, its
                            //!< minimize/zoom/close buttons, a resize border).
            TriggerClick,   //!< a mouse press landed on the trigger widget while open (a toggle-close).
            WindowChanged   //!< the host window was deactivated, moved, resized, hidden, minimized
                            //!< or closed, or the application as a whole lost activation.
        };
        Q_ENUM(CloseReason)

        constexpr static const int DefaultAnimationDurationMs=150;
        constexpr static const int DefaultOffsetX=0;
        constexpr static const int DefaultOffsetY=4;
        constexpr static const QEasingCurve::Type DefaultEasingCurve=QEasingCurve::OutCubic;

        /**
         * @brief Constructor.
         * @param parent Owning widget, purely for Qt object-tree bookkeeping -- the frame is
         *  always a top-level window regardless, and never relies on parent for positioning.
         *  Normally left null; callers manage the frame's lifetime explicitly (see
         *  utils/destroywidget.hpp) rather than relying on Qt parent-child deletion.
         */
        DropdownFrame(QWidget* parent=nullptr);

        ~DropdownFrame();

        DropdownFrame(const DropdownFrame&)=delete;
        DropdownFrame(DropdownFrame&&)=delete;
        DropdownFrame& operator=(const DropdownFrame&)=delete;
        DropdownFrame& operator=(DropdownFrame&&)=delete;

        /**
         * @brief Set content widget.
         * @param content Widget to show in the frame, reparented to this frame.
         *
         * The previous content widget, if any, is destroyed with destroyWidget().
         */
        void setContent(QWidget* content);

        /**
         * @brief Get content widget.
         * @return Operation result, may be nullptr.
         */
        QWidget* content() const;

        /**
         * @brief Detach content widget without destroying it.
         * @return Detached widget, may be nullptr.
         */
        QWidget* takeContent();

        /**
         * @brief Set corner the frame grows from/is anchored to.
         * @param corner One of the four Qt::Corner values.
         *
         * Set automatically by measure()/measureAt() based on available space; exposed as a
         * setter mainly for FastSwitchButtonDropdown's pre-existing public API and for tests.
         */
        void setAnchorCorner(Qt::Corner corner);

        Qt::Corner anchorCorner() const noexcept;

        /**
         * @brief Set full (unclipped, natural) size of the frame.
         * @param size Full size to animate towards/from.
         */
        void setFullSize(const QSize& size);

        QSize fullSize() const noexcept;

        /**
         * @brief Set the widget that opens/toggles this frame.
         * @param widget Trigger widget, or nullptr to clear it.
         *
         * A press on the trigger widget while the frame is open is consumed (so the trigger's
         * own click handling cannot re-open what this press is meant to close) and reported via
         * closeRequested(CloseReason::TriggerClick) instead of OutsideClick.
         */
        void setTriggerWidget(QWidget* widget);

        QWidget* triggerWidget() const;

        /**
         * @brief Enable/disable Escape and outside-click dismissal.
         * @param enable If true (the default) Escape, an outside click, or the host window
         *  being deactivated/moved/resized closes the frame on its own.
         */
        void setSelfDismissEnabled(bool enable) noexcept;

        bool isSelfDismissEnabled() const noexcept;

        /**
         * @brief Enable/disable flipping upward when there is not enough room below the anchor.
         * @param enable Default true.
         */
        void setVerticalFlipEnabled(bool enable) noexcept;

        bool isVerticalFlipEnabled() const noexcept;

        /**
         * @brief Enable/disable restoring focus to the previously focused widget on close.
         * @param enable Default true.
         */
        void setRestoreFocus(bool enable) noexcept;

        bool isRestoreFocus() const noexcept;

        /**
         * @brief Check if the frame is currently open (shown or animating open/closed).
         */
        bool isOpen() const noexcept;

        void setAnimationDurationMs(int val) noexcept;
        int animationDurationMs() const noexcept;

        void setEasingCurveType(int val) noexcept;
        int easingCurveType() const noexcept;

        void setOffsetX(int val) noexcept;
        int offsetX() const noexcept;

        void setOffsetY(int val) noexcept;
        int offsetY() const noexcept;

    public slots:

        /**
         * @brief Open the frame anchored below (or, if flipped, above) the given widget.
         * @param anchor Widget to anchor to.
         *
         * Anchored at the anchor's bottom-left corner by default, like a native menu; flips to
         * the anchor's bottom-right corner only if the frame would not fit within the anchor's
         * screen otherwise (see isVerticalFlipEnabled() for the analogous vertical flip). The
         * frame is free to extend past the edges of anchor->window() -- only the screen's
         * available geometry bounds it.
         *
         * If the frame is already open (e.g. mid-close-animation from a very quick toggle),
         * content is not re-filled and geometry is not re-measured -- the open animation simply
         * reverses from wherever the close animation currently is.
         */
        void popupBelow(QWidget* anchor);

        /**
         * @brief Open the frame anchored at a global position, e.g. the cursor position.
         * @param globalPos Global position of the frame's top-left corner (before any flip).
         *
         * Anchored with its top-left corner at globalPos by default, like a native context menu;
         * flips to top-right only if the frame would not fit within globalPos's screen otherwise.
         * The frame is free to extend past the edges of the host window -- only the screen's
         * available geometry bounds it.
         *
         * Uses triggerWidget()->window() as the host if a trigger widget is set, otherwise the
         * application's active window -- the host is only used for WindowChanged dismissal and
         * for restoring focus on close, not for clipping or positioning.
         */
        void popupAt(const QPoint& globalPos);

        /**
         * @brief Close the frame if open, otherwise open it anchored below the given widget.
         */
        void toggleBelow(QWidget* anchor);

        /**
         * @brief Close the frame.
         * @param immediate If true, skip the close animation.
         */
        void closeDropdown(bool immediate=false);

    signals:

        /**
         * @brief Emitted right before content is filled and measured for a fresh opening.
         */
        void aboutToShow();

        /**
         * @brief Emitted once the open animation finishes.
         */
        void shown();

        /**
         * @brief Emitted right before the frame becomes hidden, for any reason.
         *
         * Owners that drive dismissal themselves normally react to closeRequested() instead;
         * this signal is a defensive recovery path for cases nothing else initiated, e.g. the
         * host window being hidden outright.
         */
        void aboutToHide();

        /**
         * @brief Emitted once the close animation finishes, content has been cleared via
         *  clearContent(), and the frame is actually hidden.
         */
        void hidden();

        /**
         * @brief Emitted when the frame decides to close itself, before closeDropdown() runs.
         * @param reason Why the frame is closing.
         *
         * Owners that keep their own state in sync with open/closed (e.g. a checkable trigger
         * button) should react here rather than assuming they are always the ones initiating
         * the close.
         */
        void closeRequested(CloseReason reason);

    protected:

        /**
         * @brief Build/refresh content for the current opening.
         *
         * Called before measurement, every time a fresh opening begins (not on a reopen that
         * interrupts a still-visible close animation). Default implementation does nothing;
         * subclasses that own their content internally (e.g. DropdownMenu) override this to
         * rebuild rows from their current descriptor state.
         */
        virtual void fillContent() {}

        /**
         * @brief Release resources acquired in fillContent().
         *
         * Called after the close animation finishes, never at the start of it.
         */
        virtual void clearContent() {}

        /**
         * @brief Request a programmatic close in reaction to a content row being activated.
         * @param source The widget that was activated, may be nullptr.
         *
         * Intended to be called by a subclass's own row widgets (via a lambda capturing `this`,
         * since this method is protected) when a clickable item is triggered.
         */
        void notifyActivated(QWidget* source=nullptr);

        void resizeEvent(QResizeEvent* event) override;
        void hideEvent(QHideEvent* event) override;

        /**
         * @brief Explicitly paint the QSS "background-color"/"border"/"border-radius" box for
         *  this frame (see uise--DropdownMenu in dropdownmenu.qss).
         *
         * QFrame's own default paintEvent()/CE_ShapedFrame drawing is not reliably composited
         * on a Qt::WA_TranslucentBackground top-level window (the same reason Toast, BusyWaiting
         * and TypingIndicator each paint their own background manually rather than depending on
         * the default) -- explicitly invoking PE_Widget here is the standard Qt technique for
         * making a subclassed widget's stylesheet box model apply reliably, and keeps the
         * background/border/radius QSS-driven (and thus per-theme) rather than hardcoded.
         */
        void paintEvent(QPaintEvent* event) override;

        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        void beginOpen(QWidget* host);
        void trackHost(QWidget* host);
        QSize measureContentSize(QMargins& outMargins);
        void measure(QWidget* anchor);
        void measureAt(const QPoint& globalPos);
        void animateFrame(bool forward, bool immediate);
        void finishAnimation(bool forward);
        void applyFrame(qreal t);

        std::unique_ptr<DropdownFrame_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DROPDOWNFRAME_HPP
