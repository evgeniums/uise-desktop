/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/icontextbutton.hpp
*
*  Declares IconTextButton.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ICONTEXTBUTTON_HPP
#define UISE_DESKTOP_ICONTEXTBUTTON_HPP

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>

class QLabel;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RoundedImage;
class RippleOverlay;
class IconBadge;

class UISE_DESKTOP_EXPORT IconTextButton : public QFrame
{
    Q_OBJECT

    public:

        enum class IconPosition
        {
            Invisible,
            BeforeText,
            AfterText,
            AboveText,
            BelowText
        };

        IconTextButton(std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText);

        IconTextButton(QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::shared_ptr<SvgIcon>{},parent,iconPosition)
        {}

        IconTextButton(const QString text, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::shared_ptr<SvgIcon>{},parent,iconPosition)
        {
            setText(text);
        }

        IconTextButton(const QString text, std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, IconPosition iconPosition=IconPosition::BeforeText) : IconTextButton(std::move(icon),parent,iconPosition)
        {
            setText(text);
        }

        void setSvgIcon(std::shared_ptr<SvgIcon> icon);

        std::shared_ptr<SvgIcon> svgIcon() const;

        void setTrailingSvgIcon(std::shared_ptr<SvgIcon> icon);

        std::shared_ptr<SvgIcon> trailingSvgIcon() const;

        /**
         * @brief Set arbitrary widget placed before everything else in the button (ahead of the
         * leading icon), in every IconPosition. Unlike a widget added to layout() directly, this
         * one survives setIconPosition()'s layout rebuild -- it is re-added on every rebuild the
         * same way the trailing icon already is.
         *
         * The button takes ownership: the widget is reparented into it and destroyed with it (see
         * destroyWidget()). Passing a different widget, or nullptr, destroys the previous one.
         * Passing the same widget again is a no-op. Reparenting hides the widget (Qt), so its
         * visibility as observed just before the call is restored right after reparenting --
         * callers may attach an already-hidden widget without it flashing into view.
         *
         * @param widget Widget to place, or nullptr to remove the current one.
         */
        void setLeadingWidget(QWidget* widget);

        QWidget* leadingWidget() const noexcept
        {
            return m_leadingWidget.get();
        }

        /**
         * @brief Set arbitrary widget placed after everything else in the button (behind the
         * trailing icon). See setLeadingWidget() for ownership/ordering/visibility semantics.
         */
        void setTrailingWidget(QWidget* widget);

        QWidget* trailingWidget() const noexcept
        {
            return m_trailingWidget.get();
        }

        /**
         * @brief Show a counter badge over the top-right corner of the (leading) icon, or hide it
         * with empty \p text. The badge is an IconBadge floating over this button, outside the
         * layout: changing it never changes this button's size.
         *
         * The first call creates the badge and sets the dynamic property iconBadge="true" on this
         * button (repolishing the icon wrapper), which QSS can use to reserve a permanent gap
         * between the icon and the text for the badge to grow into -- see navigationbar.qss. The
         * property then stays set even while the badge is hidden, so that gap never toggles.
         *
         * @param text Pre-formatted badge text, e.g. "42" or "1.2K"; empty hides the badge.
         * @param muted Use the muted badge colour.
         * @param mentioned Use the mentioned badge colour; wins over \p muted.
         */
        void setIconBadge(const QString& text, bool muted=false, bool mentioned=false);

        IconBadge* iconBadge() const noexcept
        {
            return m_iconBadge;
        }

        void setParentHovered(bool enable);

        bool isParentHovered() const noexcept
        {
            return m_parentHovered;
        }

        bool isChecked() const noexcept
        {
            return m_checked;
        }

        void setCheckable(bool enable) noexcept;

        bool isCheckable() const noexcept
        {
            return m_checkable;
        }

        void setText(const QString& text);

        QString text() const;

        /**
         * @brief Keep the button's width independent of which digits the text currently shows.
         *
         * Meant for text that ticks, e.g. a call duration or a timer. With proportional digits
         * ("1" is narrower than "0") the label's width changes from tick to tick, and in a
         * right-aligned container (a navigation bar's trailing group) the whole button then
         * visibly jitters. When enabled, the text label reserves the width of the current text
         * with every digit replaced by the widest digit of its font, re-measured on every
         * setText() and font change (QSS polish, theme switch). Only a change of the text's
         * format (e.g. "59:59" -> "1:00:00") changes the width.
         *
         * Off by default.
         */
        void setStableDigitWidth(bool enable);

        bool isStableDigitWidth() const noexcept
        {
            return m_stableDigitWidth;
        }

        void setIconPosition(IconPosition iconPosition);

        IconPosition iconPosition() const noexcept
        {
            return m_iconPosition;
        }

        void setTextInteractionFlags(Qt::TextInteractionFlags flags);
        Qt::TextInteractionFlags textInteractionFlags() const;

        /**
         * @brief Fire click() on the left-button PRESS instead of on the release.
         *
         * Off by default: the usual act-on-release lets a user cancel by dragging off the button.
         * Meant for toggles whose effect should start the instant the button goes down -- e.g.
         * MessageEditor's formatting-mode expand button, where acting on release meant the whole
         * hold time (while the press ripple plays) passed before anything changed. The release is
         * still claimed (accepted) so it cannot leak to the parent, but it no longer clicks.
         */
        void setClickOnPress(bool enable) noexcept
        {
            m_clickOnPress=enable;
        }

        bool isClickOnPress() const noexcept
        {
            return m_clickOnPress;
        }

        /**
         * @brief Make the press ripple a rounded rectangle instead of a circle/flat ellipse.
         *
         * Use it on a button whose checked (or hovered) state paints a filled rounded-rect
         * background, so the ripple grows into the same shape as the highlight it hands over to
         * instead of a circle followed by a square. Sets the "rectRipple" dynamic property that
         * ripple.qss keys its roundedrect rule on; the corner radius is still the QSS
         * rippleCornerRadius, keep it in step with the button's own border-radius.
         *
         * DropdownMenu::attachTo() turns this on for its trigger.
         */
        void setRectRipple(bool enable);

        bool isRectRipple() const;

        /**
         * @brief The click-ripple overlay of this button, see RippleOverlay.
         *
         * Created on demand: the constructor does not install it (a screen with dozens of buttons
         * would pay a full QSS match and an event filter for each, for ripples nobody asked for), the
         * first hover or press does, and so does the first call of this accessor. A caller that
         * tunes the ripple (setRippleEnabled(), insets) simply gets it created at that moment.
         */
        RippleOverlay* rippleOverlay() const;

    signals:

        void clicked();
        void toggled(bool enable);
        void hovered(bool enable);

    public slots:

        void click();

        void toggle();

        void setChecked(bool enable);

    protected:

        /**
         * @brief Installs the ripple overlay on the first Enter or left-button press.
         *
         * Done in event(), not in enterEvent()/mousePressEvent(): subclasses (NavigationBarItem)
         * override those without calling the base, event() runs before them all.
         */
        bool event(QEvent* event) override;

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        //! Watches m_text for FontChange to re-measure the stable digit width.
        bool eventFilter(QObject* watched, QEvent* event) override;

        /**
         * @brief Whether a left-button press is currently held down inside this button.
         *
         * Set by mousePressEvent(), cleared by mouseReleaseEvent() right before it decides
         * whether to click() -- protected rather than private because a subclass that overrides
         * mouseReleaseEvent() to intercept the release itself (see NavigationBarItem's
         * exclusive-mode re-click) must clear it too, or the next unrelated release would still
         * see it set.
         */
        bool m_pressed=false;

    private:

        void setHovered(bool enable);

        //! Installs the ripple overlay if it is not there yet and returns it.
        RippleOverlay* ensureRippleOverlay() const;

        //! Sets m_text's minimum width for setStableDigitWidth(); no-op reset when disabled.
        void updateStableTextWidth();

        //! Shared by the constructor and setIconPosition(): (re)builds m_layout from scratch for
        //! the current m_iconPosition, then re-adds the leading/trailing widgets (if any) so they
        //! survive the rebuild -- see setLeadingWidget()'s doc comment.
        void rebuildLayout();

        IconPosition m_iconPosition;

        QBoxLayout* m_layout;
        RoundedImage* m_icon;
        RoundedImage* m_trailingIcon;
        QLabel* m_text;

        QPointer<QWidget> m_leadingWidget;
        QPointer<QWidget> m_trailingWidget;

        // Created lazily by setIconBadge(); a child of this button, destroyed with it.
        IconBadge* m_iconBadge=nullptr;

        bool m_parentHovered;
        bool m_checked;
        bool m_checkable;
        bool m_clickOnPress=false;
        bool m_stableDigitWidth=false;

        //! Null until the first hover/press/rippleOverlay() call, see ensureRippleOverlay().
        mutable RippleOverlay* m_ripple;
};

}

#endif // UISE_DESKTOP_ICONTEXTBUTTON_HPP
