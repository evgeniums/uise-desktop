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

/** @file uise/desktop/abstractcheckbox.hpp
*
*  Declares AbstractCheckBox, base class of CheckBox and RadioBox.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHECKBOX_HPP
#define UISE_DESKTOP_ABSTRACTCHECKBOX_HPP

#include <memory>

#include <QAbstractButton>
#include <QEasingCurve>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>

class QLabel;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RippleOverlay;
class RoundedImage;
class AbstractCheckBox_p;

/**
 * @brief Base class of the checkbox/radiobox family: a QAbstractButton whose entire
 *  appearance is built from QSS-styled child widgets rather than painted by a QStyle.
 *
 * Deriving from QAbstractButton (rather than from QFrame like IconTextButton) is what makes
 * QButtonGroup, auto-exclusivity, Space/Return activation, the shortcut mnemonic and the
 * accessibility bridge work natively -- see RadioBox, which is CheckBox plus
 * setAutoExclusive(true) plus a different indicator shape and mark icon.
 *
 * The indicator is a transparent container (#indicator) holding two full-overlapping,
 * independently styled layers -- #indicatorOff and #indicatorOn -- placed in the same
 * QGridLayout cell with Qt::AlignCenter. Switching checked state cross-fades #indicatorOn's
 * QGraphicsOpacityEffect from 0 to 1, which is the only way to animate an appearance that is
 * defined by a stylesheet: a QSS state change is instantaneous by construction, so the two
 * states have to exist as two real, simultaneously styled widgets. #indicatorOn carries the
 * mark: either a RoundedImage rendering an SvgIcon (#markIcon, "svg" mode, the default -- see
 * checkbox.json) or a plain QSS-styled QFrame (#mark, "qss" mode).
 *
 * #indicator is deliberately larger than the layers it centres (see checkbox.qss): the
 * surplus is the halo band the RippleOverlay grows into. The ripple is driven from
 * QAbstractButton::pressed()/released() with RippleOverlay::setAutoTrigger(false), not from
 * the overlay's own mouse filtering, so a press on the text label and a Space-key activation
 * both produce exactly the same halo on the indicator.
 *
 * Every part carries Qt::WA_TransparentForMouseEvents, so all input lands on this widget.
 * The consequence for stylesheets: use ":hover" on uise--CheckBox itself, but
 * [hovered="true"] on the parts -- they never receive Enter/Leave and ":hover" can never
 * match there. Same contract as IconTextButton.
 */
class UISE_DESKTOP_EXPORT AbstractCheckBox : public QAbstractButton
{
    Q_OBJECT

    Q_PROPERTY(bool    checkAnimationEnabled         READ isCheckAnimationEnabled       WRITE setCheckAnimationEnabled)
    Q_PROPERTY(int     checkAnimationDurationMs      READ checkAnimationDurationMs      WRITE setCheckAnimationDurationMs)
    Q_PROPERTY(int     checkAnimationEasingCurveType READ checkAnimationEasingCurveType WRITE setCheckAnimationEasingCurveType)
    //! QSS: qproperty-cursorShape: "pointer" | "arrow" | "text" | ... ; see style.hpp's
    //! cursorShapeFromString() for the full vocabulary. "default"/"inherit"/"" clears the
    //! override back to DefaultCursorShape.
    Q_PROPERTY(QString cursorShape         READ cursorShapeName         WRITE setCursorShapeName)
    //! QSS: qproperty-disabledCursorShape: same vocabulary, applied while !isEnabled().
    Q_PROPERTY(QString disabledCursorShape READ disabledCursorShapeName WRITE setDisabledCursorShapeName)
    //! QSS: qproperty-indicatorMode: "auto" (svg when a mark icon is set, qss otherwise) | "svg" | "qss";
    Q_PROPERTY(QString indicatorMode       READ indicatorModeName       WRITE setIndicatorModeName)
    //! QSS: qproperty-indicatorShape: "box" | "circle"; mirrored onto every part as [shape].
    Q_PROPERTY(QString indicatorShape      READ indicatorShapeName      WRITE setIndicatorShapeName)
    //! QSS: qproperty-textPosition: "after" | "before" the indicator.
    Q_PROPERTY(QString textPosition        READ textPositionName        WRITE setTextPositionName)

    public:

        enum class IndicatorMode
        {
            Auto,
            Svg,
            Qss
        };

        enum class IndicatorShape
        {
            Box,
            Circle
        };

        enum class TextPosition
        {
            After,
            Before
        };

        constexpr static const bool DefaultCheckAnimationEnabled=true;
        constexpr static const int  DefaultCheckAnimationDurationMs=120;
        constexpr static const QEasingCurve::Type DefaultCheckAnimationEasingCurve=QEasingCurve::OutCubic;
        constexpr static const Qt::CursorShape DefaultCursorShape=Qt::PointingHandCursor;
        constexpr static const Qt::CursorShape DefaultDisabledCursorShape=Qt::ArrowCursor;
        constexpr static const IndicatorMode   DefaultIndicatorMode=IndicatorMode::Auto;
        constexpr static const IndicatorShape  DefaultIndicatorShape=IndicatorShape::Box;
        constexpr static const TextPosition    DefaultTextPosition=TextPosition::After;

        explicit AbstractCheckBox(QWidget* parent=nullptr);

        ~AbstractCheckBox();

        AbstractCheckBox(const AbstractCheckBox&)=delete;
        AbstractCheckBox(AbstractCheckBox&&)=delete;
        AbstractCheckBox& operator=(const AbstractCheckBox&)=delete;
        AbstractCheckBox& operator=(AbstractCheckBox&&)=delete;

        /**
         * @brief Set the button's text.
         *
         * QAbstractButton::setText() is not virtual, so this SHADOWS rather than overrides
         * it. The base version is called first and keeps the authoritative copy -- it feeds
         * QAbstractButton::text(), the '&' mnemonic shortcut and the accessibility bridge,
         * all of which a QAbstractButton* holder (QButtonGroup, QAccessible, ...) reaches
         * without going through this class. The child #text label then gets the
         * de-mnemonicised form, since QLabel renders '&' literally.
         */
        void setText(const QString& text);
        QString text() const;

        /** @brief The child QLabel that actually paints the text (objectName "text"). */
        QLabel* textWidget() const noexcept;

        /** @brief The transparent halo container the ripple installs on (objectName "indicator"). */
        QWidget* indicatorWidget() const noexcept;
        QWidget* indicatorOffWidget() const noexcept;
        QWidget* indicatorOnWidget() const noexcept;

        /** @brief The click-ripple overlay installed on indicatorWidget(), see RippleOverlay. */
        RippleOverlay* rippleOverlay() const noexcept;

        /**
         * @brief Icon rendered as the checked mark in "svg" indicator mode.
         *
         * Setting a non-null icon is what makes IndicatorMode::Auto resolve to Svg. The icon
         * is rendered through a RoundedImage, so it picks up IconMode::Normal/Hovered/
         * Checked/CheckedHovered/Disabled automatically from this widget's own state -- see
         * light|dark/checkbox.json for the per-mode colours.
         */
        void setMarkIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> markIcon() const;

        void setCheckAnimationEnabled(bool enable) noexcept;
        bool isCheckAnimationEnabled() const noexcept;

        void setCheckAnimationDurationMs(int ms) noexcept;
        int  checkAnimationDurationMs() const noexcept;

        void setCheckAnimationEasingCurveType(int type);
        int  checkAnimationEasingCurveType() const noexcept;

        void setCursorShape(Qt::CursorShape shape);
        Qt::CursorShape cursorShape() const noexcept;
        void setCursorShapeName(const QString& name);
        QString cursorShapeName() const;

        void setDisabledCursorShape(Qt::CursorShape shape);
        Qt::CursorShape disabledCursorShape() const noexcept;
        void setDisabledCursorShapeName(const QString& name);
        QString disabledCursorShapeName() const;

        void setIndicatorMode(IndicatorMode mode);
        IndicatorMode indicatorMode() const noexcept;
        void setIndicatorModeName(const QString& name);
        QString indicatorModeName() const;

        void setIndicatorShape(IndicatorShape shape);
        IndicatorShape indicatorShape() const noexcept;
        void setIndicatorShapeName(const QString& name);
        QString indicatorShapeName() const;

        void setTextPosition(TextPosition position);
        TextPosition textPosition() const noexcept;
        void setTextPositionName(const QString& name);
        QString textPositionName() const;

        /**
         * @brief Two-state checked/unchecked mapping of Qt::CheckState, for source
         *  compatibility with call sites migrated from QCheckBox. There is no tristate mode
         *  -- Qt::PartiallyChecked maps to Checked on the way in and is never returned.
         */
        Qt::CheckState checkState() const noexcept;
        void setCheckState(Qt::CheckState state);

        /** @brief Join the setParentHovered() propagation contract, see IconTextButton. */
        void setParentHovered(bool enable);
        bool isParentHovered() const noexcept;

    signals:

        void hovered(bool enable);

    protected:

        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void changeEvent(QEvent* event) override;

        //! Both QAbstractButton state hooks are overridden instead of listening to this
        //! widget's OWN toggled() signal: the appearance here is not painted by a QStyle but
        //! driven by updateCheckedState()/applyPartState(), and a caller wrapping the widget
        //! in a QSignalBlocker -- the standard way to push state in without echoing it back
        //! out as a user edit -- would suppress an internal toggled() listener too, flipping
        //! the logical state while the widget kept painting the old one. checkStateSet()
        //! covers every programmatic setChecked(); nextCheckState() covers the one path that
        //! skips it, since QAbstractButtonPrivate::click() sets blockRefresh around its
        //! nextCheckState() call. Between them every path that can change the checked state
        //! is covered, including a QButtonGroup unchecking a sibling.
        void checkStateSet() override;
        void nextCheckState() override;

    private:

        void setHovered(bool enable);
        void applyPartState();
        void applyCursor();
        void updateCheckedState(bool animate);
        void syncCheckedState();
        void endRipple();
        void applyTextPosition();
        void applyIndicatorMode();

        /**
         * @brief Request a deferred rebuild of the indicator mode / text position.
         *
         * Deferral is mandatory: setIndicatorModeName()/setTextPositionName() are Q_PROPERTY
         * writers, and Qt's style engine calls property writers DURING polish -- doing the
         * widget visibility/layout work synchronously would re-enter the style engine on a
         * widget that is already mid-polish. Same guard and rationale as
         * AbstractEditablePanel::scheduleAlignmentsUpdate() (editablepanel.cpp) and
         * FileUploadWidget's deferred list-area height update (fileuploadwidget.cpp).
         */
        void scheduleStructureUpdate();
        void doUpdateStructure();

        std::unique_ptr<AbstractCheckBox_p> pimpl;
};

}

#endif // UISE_DESKTOP_ABSTRACTCHECKBOX_HPP
