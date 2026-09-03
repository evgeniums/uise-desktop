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

/** @file uise/desktop/fastswitchbutton.hpp
*
*  Declares FastSwitchButton and FastSwitchButtonDropdown.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FASTSWITCHBUTTON_HPP
#define UISE_DESKTOP_FASTSWITCHBUTTON_HPP

#include <memory>

#include <QFrame>
#include <QEasingCurve>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/dropdownframe.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;

/**
 * @brief Overlay frame used by FastSwitchButton to present its drop-down content.
 *
 * A plain, unstyled-beyond-its-own-QSS-class subclass of DropdownFrame: FastSwitchButton keeps
 * driving content creation/fill/clear itself (see ensureDropdownContent()/fillDropdownContent()/
 * clearDropdownContent()) rather than through DropdownFrame's fillContent()/clearContent()
 * hooks, so this class has nothing of its own to add. It exists so QSS type selectors can keep
 * targeting `uise--FastSwitchButtonDropdown` specifically (see resources/style/
 * fastswitchbutton.qss) without those rules also matching every other DropdownFrame consumer.
 */
class UISE_DESKTOP_EXPORT FastSwitchButtonDropdown : public DropdownFrame
{
    Q_OBJECT

    public:

        using DropdownFrame::DropdownFrame;
};

class FastSwitchButton_p;

/**
 * @brief Composite navigation-bar control combining a checkable icon button, a widget that
 *  slides out on hover, and an animated drop-down.
 *
 * The control has three states (see State): in Normal state only the main button (mainButton())
 * is shown, looking like an ordinary icon button. In Hovered state an additional widget
 * (extraWidget()) slides out to the right of the main button, revealed left-to-right from
 * behind it. Clicking the main button (or calling openDropdown()) opens a drop-down
 * (dropdown()) below the control; the extra widget stays visible for as long as the drop-down
 * is open, even if the mouse leaves the control. Escape, an outside click, or activating any
 * sub-control (see notifyActivated()) animates the control back to Normal state.
 *
 * FastSwitchButton itself has no notion of what the extra widget or the drop-down content
 * show: subclasses override createExtraWidget()/fillExtraWidget()/clearExtraWidget() and
 * createDropdownContent()/fillDropdownContent()/clearDropdownContent() to supply and populate
 * them. See the protected section for the full hook set and the rationale for the
 * create/connect/fill/clear split.
 */
class UISE_DESKTOP_EXPORT FastSwitchButton : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(int extraSlideDurationMs READ extraSlideDurationMs WRITE setExtraSlideDurationMs)
    Q_PROPERTY(int dropdownAnimationDurationMs READ dropdownAnimationDurationMs WRITE setDropdownAnimationDurationMs)
    Q_PROPERTY(int extraEasingCurveType READ extraEasingCurveType WRITE setExtraEasingCurveType)
    Q_PROPERTY(int dropdownEasingCurveType READ dropdownEasingCurveType WRITE setDropdownEasingCurveType)
    Q_PROPERTY(int extraSpacing READ extraSpacing WRITE setExtraSpacing)
    Q_PROPERTY(int dropdownOffsetX READ dropdownOffsetX WRITE setDropdownOffsetX)
    Q_PROPERTY(int dropdownOffsetY READ dropdownOffsetY WRITE setDropdownOffsetY)
    Q_PROPERTY(int hoverEnterDelayMs READ hoverEnterDelayMs WRITE setHoverEnterDelayMs)
    Q_PROPERTY(int hoverLeaveDelayMs READ hoverLeaveDelayMs WRITE setHoverLeaveDelayMs)

    public:

        /**
         * @brief State of the control.
         */
        enum class State : uint8_t
        {
            Normal,     //!< Only the main button is shown.
            Hovered,    //!< The extra widget is shown/animating next to the main button.
            Dropdown    //!< The drop-down is open; the extra widget stays shown.
        };
        Q_ENUM(State)

        constexpr static const int DefaultSlideDurationMs=150;
        constexpr static const int DefaultHoverEnterDelayMs=0;
        constexpr static const int DefaultHoverLeaveDelayMs=120;
        constexpr static const int DefaultExtraSpacing=0;
        constexpr static const int DefaultDropdownOffsetX=0;
        constexpr static const int DefaultDropdownOffsetY=4;
        constexpr static const QEasingCurve::Type DefaultEasingCurve=QEasingCurve::OutCubic;

        /**
         * @brief Constructor.
         * @param icon Icon of the main button.
         * @param parent Parent widget.
         */
        FastSwitchButton(std::shared_ptr<SvgIcon> icon={}, QWidget* parent=nullptr);

        explicit FastSwitchButton(QWidget* parent) : FastSwitchButton(std::shared_ptr<SvgIcon>{},parent)
        {}

        ~FastSwitchButton();

        FastSwitchButton(const FastSwitchButton&)=delete;
        FastSwitchButton(FastSwitchButton&&)=delete;
        FastSwitchButton& operator=(const FastSwitchButton&)=delete;
        FastSwitchButton& operator=(FastSwitchButton&&)=delete;

        /**
         * @brief Get the main icon button.
         * @return Operation result.
         */
        IconTextButton* mainButton() const;

        /**
         * @brief Get the extra ("fast switch") widget.
         * @return Operation result, nullptr until the control has been hovered at least once.
         */
        QWidget* extraWidget() const;

        /**
         * @brief Get the drop-down frame.
         * @return Operation result, nullptr until the drop-down has been opened at least once.
         */
        FastSwitchButtonDropdown* dropdown() const;

        /**
         * @brief Get the drop-down content widget.
         * @return Operation result, nullptr until the drop-down has been opened at least once.
         */
        QWidget* dropdownContent() const;

        void setSvgIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> svgIcon() const;

        State state() const noexcept;
        bool isDropdownOpen() const noexcept;
        bool isExtraWidgetVisible() const noexcept;

        void setExtraSlideDurationMs(int val) noexcept;
        int extraSlideDurationMs() const noexcept;

        void setDropdownAnimationDurationMs(int val) noexcept;
        int dropdownAnimationDurationMs() const noexcept;

        void setExtraEasingCurveType(int val) noexcept;
        int extraEasingCurveType() const noexcept;

        void setDropdownEasingCurveType(int val) noexcept;
        int dropdownEasingCurveType() const noexcept;

        void setExtraSpacing(int val) noexcept;
        int extraSpacing() const noexcept;

        void setDropdownOffsetX(int val) noexcept;
        int dropdownOffsetX() const noexcept;

        void setDropdownOffsetY(int val) noexcept;
        int dropdownOffsetY() const noexcept;

        void setHoverEnterDelayMs(int val) noexcept;
        int hoverEnterDelayMs() const noexcept;

        void setHoverLeaveDelayMs(int val) noexcept;
        int hoverLeaveDelayMs() const noexcept;

    public slots:

        /**
         * @brief Animate the extra widget into view (Normal -> Hovered).
         */
        void showExtraWidget();

        /**
         * @brief Animate the extra widget out of view (Hovered -> Normal). No-op in Dropdown state.
         */
        void hideExtraWidget();

        /**
         * @brief Open the drop-down (-> Dropdown).
         */
        void openDropdown();

        /**
         * @brief Close the drop-down (Dropdown -> Hovered/Normal).
         */
        void closeDropdown();

        /**
         * @brief Return the control to Normal state: close the drop-down, retract and clear
         *  the extra widget, uncheck the main button, and clear the drop-down content.
         * @param immediate If true then skip the animations.
         */
        void returnToNormal(bool immediate=false);

    signals:

        void stateChanged(State state);

        /**
         * @brief Emitted when the extra widget's default click handling activates it.
         */
        void extraWidgetActivated();

        void dropdownAboutToShow();
        void dropdownShown();
        void dropdownHidden();

        /**
         * @brief Emitted whenever any sub-control is activated, see notifyActivated().
         * @param source The widget that was activated, may be nullptr.
         */
        void activated(QWidget* source);

    protected:

        /**
         * @brief Create the main button.
         *
         * Called from the FastSwitchButton constructor, unlike every other create* hook,
         * because the main button must exist before the constructor returns. This makes it
         * NOT safely overridable: per ordinary C++ construction rules, a virtual call made
         * from a base class constructor always resolves to the base class's own
         * implementation, never to a derived override, since the derived part of the object
         * does not exist yet. It is therefore a plain (non-virtual) method rather than a
         * hook; use setSvgIcon() and mainButton() to customize the main button after
         * construction instead. Default implementation creates an icon-only checkable
         * IconTextButton (IconPosition::BeforeText with an empty text, because
         * IconPosition::Invisible hides the icon rather than the text).
         */
        IconTextButton* createMainButton();

        /**
         * @brief Create the extra ("fast switch") widget.
         * @param parent Parent to create the widget with (an internal clip container).
         * @return Newly created widget.
         *
         * Called once, lazily, on first hover. This is the only hook that decides the
         * *class* of the extra widget (e.g. a subclass may return an AvatarButton instead of
         * the default IconTextButton); per-hover data goes into fillExtraWidget() instead, so
         * that the (potentially expensive) construction and style polish happen only once.
         */
        virtual QWidget* createExtraWidget(QWidget* parent);

        /**
         * @brief Wire up the extra widget's signals.
         * @param widget Widget returned by createExtraWidget().
         *
         * Called once, right after createExtraWidget(). Default implementation connects
         * widget's clicked() signal, if it has one, so that clicking the extra widget calls
         * notifyActivated(). Subclasses whose extra widget exposes a differently-named
         * activation signal should override this instead of duplicating creation.
         */
        virtual void connectExtraWidget(QWidget* widget);

        /**
         * @brief Fill the extra widget with data for the current hover.
         * @param widget Widget returned by createExtraWidget().
         *
         * Called every time the control enters Hovered state. Acquire whatever resources are
         * needed to present the extra widget's content here (e.g. subscribe an observer);
         * release them symmetrically in clearExtraWidget().
         */
        virtual void fillExtraWidget(QWidget* widget);

        /**
         * @brief Release resources acquired in fillExtraWidget().
         * @param widget Widget returned by createExtraWidget().
         *
         * Called after the retract animation finishes, never at the start of it, so the
         * widget's content stays intact for the whole collapse.
         */
        virtual void clearExtraWidget(QWidget* widget);

        /**
         * @brief Reflect composite hover state on the extra widget.
         * @param widget Widget returned by createExtraWidget().
         * @param hovered New hovered state.
         *
         * Default implementation calls setParentHovered() if widget is an IconTextButton or
         * AvatarButton, otherwise sets the "hovered" dynamic property and repolishes.
         */
        virtual void setExtraWidgetHovered(QWidget* widget, bool hovered);

        /**
         * @brief Create the drop-down content widget.
         * @param parent Parent to create the widget with (the dropdown() frame).
         * @return Newly created widget.
         *
         * Called once, lazily, on first open. Default implementation creates an empty QFrame
         * with a vertical layout. If the content can grow taller than the available space
         * below the control, wrap it in a ScrollArea: the dropdown clamps to the available
         * height and does not scroll on its own.
         */
        virtual QWidget* createDropdownContent(QWidget* parent);

        /**
         * @brief Fill the drop-down content for the current opening.
         * @param content Widget returned by createDropdownContent().
         *
         * Called every time the drop-down opens, before its size is measured, so the natural
         * size reflects however many rows this fill produces.
         */
        virtual void fillDropdownContent(QWidget* content);

        /**
         * @brief Release resources acquired in fillDropdownContent().
         * @param content Widget returned by createDropdownContent().
         *
         * Called after the close animation finishes, never at the start of it.
         */
        virtual void clearDropdownContent(QWidget* content);

        /**
         * @brief Notification that the control's state changed.
         * @param state New state.
         */
        virtual void onStateChanged(State state);

        /**
         * @brief Notification that a sub-control was activated, see notifyActivated().
         * @param source The widget that was activated, may be nullptr.
         *
         * Default implementation does nothing; returnToNormal() and the activated() signal
         * are already handled by notifyActivated() itself.
         */
        virtual void onActivated(QWidget* source);

        /**
         * @brief Notify the control that a sub-control (e.g. a drop-down row) was activated.
         * @param source The widget that was activated, may be nullptr.
         *
         * Calls onActivated(source), emits activated(source), then returnToNormal(). Intended
         * to be called by subclass-owned widgets inside the drop-down content or the extra
         * widget, e.g. from a row's clicked() handler. Not virtual: override onActivated()
         * instead of this method.
         */
        void notifyActivated(QWidget* source=nullptr);

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void ensureExtraWidget();
        void ensureDropdown();
        void ensureDropdownContent();
        void measureExtra();

        void setState(State state);

        void animateExtraWidget(bool forward, bool immediate);
        void finishExtraAnimation(bool forward);

    private slots:

        void onExtraWidgetClicked();

        /**
         * @brief React to the drop-down closing itself (Escape, outside click, trigger
         *  click, or the host window being deactivated/moved/resized).
         *
         * DropdownFrame owns the actual close animation and its own escape/outside-click
         * detection; this control still needs to keep its own state (mainButton's checked
         * state, the extra widget) in sync whenever that happens, without re-entering the
         * frame's own close -- see the definition for why this must NOT call
         * dropdown()->closeDropdown() itself.
         */
        void onDropdownSelfClosed();

    private:

        std::unique_ptr<FastSwitchButton_p> pimpl;
};

}

#endif // UISE_DESKTOP_FASTSWITCHBUTTON_HPP
