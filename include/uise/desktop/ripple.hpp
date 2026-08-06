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

/** @file uise/desktop/ripple.hpp
*
*  Declares RippleOverlay, a reusable material-style click-ripple effect for any widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_RIPPLE_HPP
#define UISE_DESKTOP_RIPPLE_HPP

#include <memory>

#include <QWidget>
#include <QColor>
#include <QEasingCurve>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class RippleOverlay_p;

/**
 * @brief Transparent child widget that paints a material-style ripple on top of its host.
 *
 * The clickable widgets in this library (IconTextButton, CalendarDay, PushButton, AvatarButton,
 * ...) have unrelated base classes, so there is no single class to put a shared paintEvent()
 * in. RippleOverlay sidesteps that: it is a plain QWidget any host installs as a transparent
 * child covering its own rect (see install()), and it drives the whole effect from a QVariantAnimation
 * pair without the host needing to know anything about it beyond calling install() once (and,
 * for hosts that must gate the effect on their own state, start()/release()/cancel() -- see
 * CalendarDay, which only ripples on a selectable day).
 *
 * By default (isAutoTrigger()==true) the overlay filters its host's own mouse press/release/
 * leave events and drives itself: press grows a circle from the cursor, held for as long as the
 * button stays pressed, then fades out on release. Every knob is a Q_PROPERTY, settable from QSS
 * via qproperty-* exactly like TypingIndicator/LoadControl/FastSwitchButton -- see
 * resources/style/ripple.qss for the full list and the per-widget-type defaults.
 *
 * rippleEnabled==false makes start() a no-op, so a stylesheet can switch the effect off for a
 * given widget type or objectName without touching C++.
 */
class UISE_DESKTOP_EXPORT RippleOverlay : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(bool    rippleEnabled         READ isRippleEnabled       WRITE setRippleEnabled)
    Q_PROPERTY(QColor  rippleColor           READ rippleColor           WRITE setRippleColor)
    Q_PROPERTY(qreal   rippleOpacity         READ rippleOpacity         WRITE setRippleOpacity)
    Q_PROPERTY(int     rippleDurationMs      READ rippleDurationMs      WRITE setRippleDurationMs)
    Q_PROPERTY(int     rippleFadeDurationMs  READ rippleFadeDurationMs  WRITE setRippleFadeDurationMs)
    Q_PROPERTY(int     rippleEasingCurveType READ rippleEasingCurveType WRITE setRippleEasingCurveType)
    Q_PROPERTY(qreal   rippleRadiusScaleX    READ rippleRadiusScaleX    WRITE setRippleRadiusScaleX)
    Q_PROPERTY(qreal   rippleRadiusScaleY    READ rippleRadiusScaleY    WRITE setRippleRadiusScaleY)
    Q_PROPERTY(bool    rippleHoldOnPress     READ isRippleHoldOnPress   WRITE setRippleHoldOnPress)
    //! QSS: qproperty-rippleOrigin: "cursor" | "center";
    Q_PROPERTY(QString rippleOrigin          READ rippleOriginName      WRITE setRippleOriginName)
    //! QSS: qproperty-rippleClip: "rect" | "rounded" | "ellipse" | "capsule";
    Q_PROPERTY(QString rippleClip            READ rippleClipName        WRITE setRippleClipName)
    Q_PROPERTY(int     rippleCornerRadius    READ rippleCornerRadius    WRITE setRippleCornerRadius)
    Q_PROPERTY(int     rippleInsetLeft       READ rippleInsetLeft       WRITE setRippleInsetLeft)
    Q_PROPERTY(int     rippleInsetTop        READ rippleInsetTop        WRITE setRippleInsetTop)
    Q_PROPERTY(int     rippleInsetRight      READ rippleInsetRight      WRITE setRippleInsetRight)
    Q_PROPERTY(int     rippleInsetBottom     READ rippleInsetBottom     WRITE setRippleInsetBottom)

    public:

        /** @brief Where a ripple grows from. */
        enum class Origin
        {
            Cursor,     //!< the press position passed to start() (the default)
            Center      //!< always the overlay's own centre, ignoring the press position
        };

        /** @brief Region the ripple is clipped to, matching the host's own QSS shape. */
        enum class Clip
        {
            Rect,       //!< the overlay's full rect, square corners
            Rounded,    //!< the overlay's rect with rounded corners, see rippleCornerRadius()
            Ellipse,    //!< inscribed ellipse -- for round hosts, e.g. a CalendarDay label
            /**
             * Rounded rect whose corner radius is always min(width,height)/2, ignoring
             * rippleCornerRadius() -- a true circle when the host's rect is square, a clean
             * straight-edged stadium/pill otherwise. Prefer this over Ellipse for any host
             * that is *intended* to look circular (an icon-only button, an avatar) but isn't
             * guaranteed pixel-perfect square: unlike an inscribed ellipse, whose curved
             * boundary tapers to a point at the left/right (or top/bottom) extremes on a
             * non-square rect, a capsule's straight edges stay uniform-height/width right up
             * to the rounded ends, with no per-host rippleCornerRadius tuning needed.
             */
            Capsule
        };

        constexpr static const bool DefaultRippleEnabled=true;
        constexpr static const bool DefaultAutoTrigger=true;
        constexpr static const bool DefaultHoldOnPress=true;
        constexpr static const int DefaultDurationMs=150;
        constexpr static const int DefaultFadeDurationMs=50;
        constexpr static const QEasingCurve::Type DefaultEasingCurve=QEasingCurve::OutCubic;
        constexpr static const qreal DefaultOpacity=0.02;
        constexpr static const qreal DefaultRadiusScaleX=1.0;
        constexpr static const qreal DefaultRadiusScaleY=1.0;
        constexpr static const int DefaultCornerRadius=0;
        constexpr static const int DefaultInset=0;
        constexpr static const Origin DefaultOrigin=Origin::Cursor;
        constexpr static const Clip DefaultClip=Clip::Rect;

        explicit RippleOverlay(QWidget* host);

        ~RippleOverlay();

        RippleOverlay(const RippleOverlay&)=delete;
        RippleOverlay(RippleOverlay&&)=delete;
        RippleOverlay& operator=(const RippleOverlay&)=delete;
        RippleOverlay& operator=(RippleOverlay&&)=delete;

        /**
         * @brief Install a RippleOverlay as a child of host, or return the one already installed.
         * @param host Widget to paint the ripple on top of. Keeps ownership of the overlay.
         *
         * Call this after the host has created its own children (see IconTextButton's
         * constructor) so the overlay ends up last in the child list and therefore paints on
         * top; raise() is also called on every host QEvent::ChildAdded/Show to keep it there.
         */
        static RippleOverlay* install(QWidget* host);

        /** @brief The overlay previously installed on host, or nullptr. */
        static RippleOverlay* find(QWidget* host);

        /** @brief Host this overlay was installed on. */
        QWidget* host() const noexcept;

        /**
         * @brief Whether the overlay drives itself from the host's own mouse events.
         *
         * True (the default) suits a host whose whole area is always clickable, e.g.
         * IconTextButton. Set to false when the host must gate the effect on extra state the
         * overlay cannot see itself -- e.g. CalendarDay only ripples a selectable day -- and
         * call start()/release()/cancel() from the host's own mouse handlers instead.
         */
        void setAutoTrigger(bool enable) noexcept;
        bool isAutoTrigger() const noexcept;

        void setRippleEnabled(bool enable) noexcept;
        bool isRippleEnabled() const noexcept;

        void setRippleColor(const QColor& color);
        QColor rippleColor() const noexcept;

        /** @brief Peak alpha of the ripple, as a 0..1 fraction of rippleColor()'s own alpha. */
        void setRippleOpacity(qreal opacity) noexcept;
        qreal rippleOpacity() const noexcept;

        /** @brief Duration of the grow animation, in milliseconds. */
        void setRippleDurationMs(int ms) noexcept;
        int rippleDurationMs() const noexcept;

        /** @brief Duration of the fade-out animation, in milliseconds. */
        void setRippleFadeDurationMs(int ms) noexcept;
        int rippleFadeDurationMs() const noexcept;

        void setRippleEasingCurveType(int type);
        int rippleEasingCurveType() const noexcept;

        /**
         * @brief Independent multipliers on the (single, diagonal-to-farthest-corner) radius
         * needed to just cover the overlay from the origin, applied separately to the
         * horizontal and vertical extent of the drawn ellipse.
         *
         * Equal values (the default, both 1) draw a true circle inscribed in/circumscribing
         * the overlay depending on rippleClip() -- see CalendarDay's and IconTextButton's
         * icon-only ripple. Unequal values draw a flattened ellipse -- e.g. IconTextButton's
         * ripple when it has visible text is stretched wide (rippleRadiusScaleX close to 1)
         * and flat (rippleRadiusScaleY well under 1), spreading mostly left-right along the
         * button rather than growing into a circle.
         */
        void setRippleRadiusScaleX(qreal scale) noexcept;
        qreal rippleRadiusScaleX() const noexcept;
        void setRippleRadiusScaleY(qreal scale) noexcept;
        qreal rippleRadiusScaleY() const noexcept;

        /**
         * @brief Whether a held press keeps the ripple at full size until release().
         *
         * True (the default) is the material "press and hold" look. False starts the fade
         * immediately once the grow animation completes, regardless of whether the host is
         * still pressed -- a tap and a long-press then look identical.
         */
        void setRippleHoldOnPress(bool enable) noexcept;
        bool isRippleHoldOnPress() const noexcept;

        void setRippleOrigin(Origin origin) noexcept;
        Origin rippleOrigin() const noexcept;
        //! QSS-friendly string form: "cursor" | "center".
        void setRippleOriginName(const QString& name);
        QString rippleOriginName() const;

        void setRippleClip(Clip clip) noexcept;
        Clip rippleClip() const noexcept;
        //! QSS-friendly string form: "rect" | "rounded" | "ellipse" | "capsule".
        void setRippleClipName(const QString& name);
        QString rippleClipName() const;

        /** @brief Corner radius used when rippleClip()==Clip::Rounded. Keep in step with the
         *  host's own QSS border-radius -- Qt does not expose a way to read that back in C++. */
        void setRippleCornerRadius(int radius) noexcept;
        int rippleCornerRadius() const noexcept;

        /**
         * @brief Per-side inset shrinking the overlay's own geometry within its host, in
         * pixels each. All default to 0 (the overlay exactly covers host->rect()).
         *
         * Qt's box model, unlike CSS, makes a QSS "margin" on the host part of the host's own
         * size (see host->size() / host->rect()) rather than external spacing outside it -- so
         * with no inset, a ripple installed on a host styled with e.g. "margin: 4px;" visibly
         * grows into that transparent margin band. Setting the insets to match (keep in step
         * with the host's own QSS margin -- Qt does not expose a way to read that back in C++,
         * the same reasoning as rippleCornerRadius above) confines the ripple to the host's
         * border+padding+content box instead, i.e. everything except the margin.
         */
        void setRippleInsetLeft(int px) noexcept;
        int rippleInsetLeft() const noexcept;
        void setRippleInsetTop(int px) noexcept;
        int rippleInsetTop() const noexcept;
        void setRippleInsetRight(int px) noexcept;
        int rippleInsetRight() const noexcept;
        void setRippleInsetBottom(int px) noexcept;
        int rippleInsetBottom() const noexcept;

    public slots:

        /**
         * @brief Start a new ripple.
         * @param pos Origin of the ripple in this overlay's own coordinates, used unless
         *  rippleOrigin()==Origin::Center.
         *
         * No-op if !isRippleEnabled(). A ripple already in progress is fast-faded out first --
         * only one ripple is ever shown at a time.
         */
        void start(const QPoint& pos);

        /** @brief End the hold begun by start() and begin the fade-out. No-op if nothing is held. */
        void release();

        /** @brief Stop the current ripple immediately, with no fade. */
        void cancel();

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

    private:

        void updateGeometryFromHost();
        void startFade();

        std::unique_ptr<RippleOverlay_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_RIPPLE_HPP
