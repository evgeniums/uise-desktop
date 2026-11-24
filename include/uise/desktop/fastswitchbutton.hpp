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
#include <QString>
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
 * @brief Composite navigation-bar control: a checkable icon button with a hover preview and a
 *  drop-down.
 *
 * The control has three states (see State). In Normal state only the main button
 * (mainButton()) is shown, looking like an ordinary icon button. How the hover preview and the
 * drop-down are presented depends on previewMode():
 *
 * - PreviewMode::Popup (the default). Hovering the main button opens a compact popup (Hovered
 *   state) as a separate top-level window whose top-left corner sits at the main button's
 *   top-right corner, so the host layout never changes size. The compact popup has up to three
 *   rows: a header button showing the main button's icon and headerText(); the extra widget
 *   (extraWidget()), e.g. the most recently used item; and an icon-only chevron button that
 *   expands the popup. Expanding (the chevron, the header button, a press on the main button, or
 *   openDropdown()) switches the SAME popup in place to the full content (Dropdown state): the
 *   header stays on top, the extra widget and the chevron are hidden, and the content created by
 *   createDropdownContent() is shown below. The popup keeps its anchor corner, so it only grows
 *   downwards. The main button is checked in that state. Pressing the header or the main button
 *   again, Escape, an outside click, or activating any sub-control (see notifyActivated())
 *   returns the control to Normal state.
 *
 * - PreviewMode::SlideOut (the original behaviour). In Hovered state the extra widget slides
 *   out to the right of the main button, revealed left-to-right from behind it. Clicking the
 *   main button (or calling openDropdown()) opens a drop-down below the control; the extra
 *   widget stays visible for as long as the drop-down is open, even if the mouse leaves the
 *   control. Escape, an outside click, or activating any sub-control animates the control back
 *   to Normal state. Note that the control changes its own width while sliding, which some
 *   users perceive as flicker -- that is what Popup mode avoids.
 *
 * FastSwitchButton itself has no notion of what the extra widget or the drop-down content
 * show: subclasses override createExtraWidget()/fillExtraWidget()/clearExtraWidget() and
 * createDropdownContent()/fillDropdownContent()/clearDropdownContent() to supply and populate
 * them, in either mode. See the protected section for the full hook set and the rationale for
 * the create/connect/fill/clear split.
 */
class UISE_DESKTOP_EXPORT FastSwitchButton : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(PreviewMode previewMode READ previewMode WRITE setPreviewMode)
    Q_PROPERTY(int extraSlideDurationMs READ extraSlideDurationMs WRITE setExtraSlideDurationMs)
    Q_PROPERTY(int dropdownAnimationDurationMs READ dropdownAnimationDurationMs WRITE setDropdownAnimationDurationMs)
    Q_PROPERTY(int extraEasingCurveType READ extraEasingCurveType WRITE setExtraEasingCurveType)
    Q_PROPERTY(int dropdownEasingCurveType READ dropdownEasingCurveType WRITE setDropdownEasingCurveType)
    Q_PROPERTY(int extraSpacing READ extraSpacing WRITE setExtraSpacing)
    Q_PROPERTY(int dropdownOffsetX READ dropdownOffsetX WRITE setDropdownOffsetX)
    Q_PROPERTY(int dropdownOffsetY READ dropdownOffsetY WRITE setDropdownOffsetY)
    Q_PROPERTY(int popupOffsetX READ popupOffsetX WRITE setPopupOffsetX)
    Q_PROPERTY(int popupOffsetY READ popupOffsetY WRITE setPopupOffsetY)
    Q_PROPERTY(int hoverEnterDelayMs READ hoverEnterDelayMs WRITE setHoverEnterDelayMs)
    Q_PROPERTY(int popupHoverEnterDelayMs READ popupHoverEnterDelayMs WRITE setPopupHoverEnterDelayMs)
    Q_PROPERTY(int hoverLeaveDelayMs READ hoverLeaveDelayMs WRITE setHoverLeaveDelayMs)
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText)

    public:

        /**
         * @brief How the hover preview and the drop-down are presented, see the class comment.
         */
        enum class PreviewMode : uint8_t
        {
            Popup,      //!< Compact popup beside the main button, expandable in place (default).
            SlideOut    //!< Extra widget slides out to the right of the main button.
        };
        Q_ENUM(PreviewMode)

        /**
         * @brief State of the control.
         *
         * Popup mode: Normal - no popup; Hovered - the compact popup is open; Dropdown - the
         * popup is expanded. SlideOut mode: Normal - only the main button; Hovered - the extra
         * widget is shown/animating; Dropdown - the drop-down is open and the extra widget stays
         * shown.
         */
        enum class State : uint8_t
        {
            Normal,
            Hovered,
            Dropdown
        };
        Q_ENUM(State)

        constexpr static const int DefaultSlideDurationMs=150;
        constexpr static const int DefaultHoverEnterDelayMs=0;
        constexpr static const int DefaultPopupHoverEnterDelayMs=200;
        constexpr static const int DefaultHoverLeaveDelayMs=120;
        constexpr static const int DefaultExtraSpacing=0;
        constexpr static const int DefaultDropdownOffsetX=0;
        constexpr static const int DefaultDropdownOffsetY=4;
        constexpr static const int DefaultPopupOffsetX=4;
        constexpr static const int DefaultPopupOffsetY=0;
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
         * @return Operation result, nullptr until the preview has been shown at least once.
         */
        QWidget* extraWidget() const;

        /**
         * @brief Get the drop-down frame.
         * @return Operation result, nullptr until the popup/drop-down has been opened at least once.
         */
        FastSwitchButtonDropdown* dropdown() const;

        /**
         * @brief Get the drop-down content widget created by createDropdownContent().
         * @return Operation result, nullptr until the popup/drop-down has been opened at least once.
         */
        QWidget* dropdownContent() const;

        void setSvgIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> svgIcon() const;

        /**
         * @brief Set the presentation mode.
         * @param mode New mode.
         *
         * Returns the control to Normal state first and discards everything built so far (the
         * popup/drop-down frame and the extra widget), so the create* hooks run again, lazily,
         * for the new mode. Normally set once, by QSS ("qproperty-previewMode: SlideOut;") or
         * right after construction.
         */
        void setPreviewMode(PreviewMode mode);
        PreviewMode previewMode() const noexcept;

        /**
         * @brief Set text of the popup's header button, the first row of both compact and
         *  expanded popup. Empty by default, which leaves an icon-only header. Popup mode only.
         */
        void setHeaderText(const QString& text);
        QString headerText() const;

        State state() const noexcept;

        //! True while the popup is expanded / the drop-down is open.
        bool isDropdownOpen() const noexcept;

        //! True while the extra widget is shown (compact popup, or slide-out and drop-down).
        bool isExtraWidgetVisible() const noexcept;

        //! SlideOut mode.
        void setExtraSlideDurationMs(int val) noexcept;
        int extraSlideDurationMs() const noexcept;

        void setDropdownAnimationDurationMs(int val) noexcept;
        int dropdownAnimationDurationMs() const noexcept;

        //! SlideOut mode.
        void setExtraEasingCurveType(int val) noexcept;
        int extraEasingCurveType() const noexcept;

        void setDropdownEasingCurveType(int val) noexcept;
        int dropdownEasingCurveType() const noexcept;

        //! SlideOut mode.
        void setExtraSpacing(int val) noexcept;
        int extraSpacing() const noexcept;

        //! SlideOut mode: offset of the drop-down from the main button's bottom-left corner.
        void setDropdownOffsetX(int val) noexcept;
        int dropdownOffsetX() const noexcept;

        void setDropdownOffsetY(int val) noexcept;
        int dropdownOffsetY() const noexcept;

        //! Popup mode: gap between the main button's right edge and the popup's left edge.
        void setPopupOffsetX(int val) noexcept;
        int popupOffsetX() const noexcept;

        //! Popup mode: shift of the popup's top edge relative to the main button's top edge.
        void setPopupOffsetY(int val) noexcept;
        int popupOffsetY() const noexcept;

        //! SlideOut mode: delay between the mouse entering the control and the extra widget sliding out.
        void setHoverEnterDelayMs(int val) noexcept;
        int hoverEnterDelayMs() const noexcept;

        //! Popup mode: delay between the mouse entering the control and the compact popup opening.
        void setPopupHoverEnterDelayMs(int val) noexcept;
        int popupHoverEnterDelayMs() const noexcept;

        /**
         * @brief Delay between the mouse leaving the control (and, in Popup mode, its popup)
         *  and the preview closing.
         */
        void setHoverLeaveDelayMs(int val) noexcept;
        int hoverLeaveDelayMs() const noexcept;

    public slots:

        /**
         * @brief Show the hover preview (Normal -> Hovered): slide the extra widget out, or open
         *  the compact popup. No-op unless in Normal state and isExtraWidgetHoverEnabled().
         */
        void showExtraWidget();

        /**
         * @brief Hide the hover preview (Hovered -> Normal). No-op in Dropdown state.
         */
        void hideExtraWidget();

        /**
         * @brief Expand the compact popup in place (Hovered -> Dropdown). In SlideOut mode this
         *  is the same as openDropdown().
         */
        void expandDropdown();

        /**
         * @brief Open the drop-down (-> Dropdown).
         */
        void openDropdown();

        /**
         * @brief Close the drop-down. In SlideOut mode Dropdown -> Hovered/Normal, in Popup mode
         *  the whole popup closes (-> Normal).
         */
        void closeDropdown();

        /**
         * @brief Return the control to Normal state: close the drop-down/popup, retract and clear
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
         * @param parent Parent to create the widget with: an internal clip container in
         *  SlideOut mode, the popup's root widget in Popup mode.
         * @return Newly created widget.
         *
         * Called once per mode, lazily, on first hover. This is the only hook that decides the
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
         * release them symmetrically in clearExtraWidget(). Not called when Popup mode opens
         * directly expanded.
         */
        virtual void fillExtraWidget(QWidget* widget);

        /**
         * @brief Release resources acquired in fillExtraWidget().
         * @param widget Widget returned by createExtraWidget().
         *
         * SlideOut mode: called after the retract animation finishes, never at the start of it,
         * so the widget's content stays intact for the whole collapse. Popup mode: called when
         * the popup is expanded (the extra widget goes away) or hidden, whichever comes first.
         */
        virtual void clearExtraWidget(QWidget* widget);

        /**
         * @brief Reflect composite hover state on the extra widget. SlideOut mode only.
         * @param widget Widget returned by createExtraWidget().
         * @param hovered New hovered state.
         *
         * Default implementation sets the "hovered" dynamic property and repolishes.
         */
        virtual void setExtraWidgetHovered(QWidget* widget, bool hovered);

        /**
         * @brief Whether hovering the control alone (without clicking it open) may reveal the
         *  extra widget / open the compact popup.
         * @return true by default.
         *
         * Consulted by showExtraWidget() (and so, indirectly, by enterEvent()'s deferred/
         * immediate paths) every time the mouse enters the control while in Normal state. Does
         * NOT gate openDropdown() -- a subclass that returns false here still gets the
         * drop-down once the control is actually clicked open, this hook only suppresses the
         * passive hover preview.
         */
        virtual bool isExtraWidgetHoverEnabled() const;

        /**
         * @brief Create the drop-down content widget.
         * @param parent Parent to create the widget with (the dropdown() frame in SlideOut mode,
         *  the popup's root widget in Popup mode).
         * @return Newly created widget.
         *
         * Called once per mode, lazily, on first open. Default implementation creates an empty
         * QFrame with a vertical layout. If the content can grow taller than the available space,
         * wrap it in a ScrollArea: the frame clamps to the available height and does not scroll
         * on its own.
         */
        virtual QWidget* createDropdownContent(QWidget* parent);

        /**
         * @brief Fill the drop-down content for the current opening.
         * @param content Widget returned by createDropdownContent().
         *
         * Called every time the drop-down opens (Popup mode: the popup expands), before its size
         * is measured, so the natural size reflects however many rows this fill produces.
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
         * @brief Whether the compact popup offers the chevron button that expands it. Popup mode only.
         * @return true by default.
         *
         * Consulted every time the compact popup opens. A subclass whose expanded content would
         * add nothing over the compact one returns false to hide the chevron.
         */
        virtual bool isExpandAvailable() const;

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
        bool event(QEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        bool isPopupMode() const noexcept;

        // shared
        void ensureDropdown();
        void ensureDropdownContent();
        void setState(State state);
        void discardBuiltWidgets();

        // SlideOut mode
        void ensureExtraWidget();
        void measureExtra();
        void animateExtraWidget(bool forward, bool immediate);
        void finishExtraAnimation(bool forward);

        // Popup mode
        void ensurePopupContent();
        void applyPopupMode(bool expanded);
        void releaseExtraWidget();
        void popupShow();
        void popupExpand();
        void popupOpenExpanded();
        void popupReturnToNormal(bool immediate);
        void scheduleLeaveCheck();
        void checkHoverLeave();

    private slots:

        void onExtraWidgetClicked();

        /**
         * @brief React to the drop-down/popup closing itself (Escape, outside click, trigger
         *  click, or the host window being deactivated/moved/resized).
         *
         * DropdownFrame owns the actual close animation and its own escape/outside-click
         * detection; this control still needs to keep its own state (mainButton's checked
         * state, the extra widget) in sync whenever that happens, without re-entering the
         * frame's own close -- see the definition for why this must NOT call
         * dropdown()->closeDropdown() itself.
         */
        void onDropdownSelfClosed();

        /**
         * @brief Popup mode: react to a press on the main button while the popup is open --
         *  expand the compact popup, or close the expanded one.
         */
        void onTriggerPressed();

        void onHeaderClicked();

    private:

        std::unique_ptr<FastSwitchButton_p> pimpl;
};

}

#endif // UISE_DESKTOP_FASTSWITCHBUTTON_HPP
