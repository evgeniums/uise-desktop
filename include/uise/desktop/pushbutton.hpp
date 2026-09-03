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

/** @file uise/desktop/pushbutton.hpp
*
*  Declares PushButton.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PUSHBUTTON_HPP
#define UISE_DESKTOP_PUSHBUTTON_HPP

#include <QFrame>
#include <QPushButton>
#include <QToolButton>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RippleOverlay;

/**
 * @brief A QFrame wrapper embedding a real QPushButton/QToolButton, styled via QSS.
 *
 * Styling contract between the outer PushButton frame and its inner QPushButton/QToolButton
 * (see qPushButton()/qToolButton()):
 *
 * - The inner button owns `padding`, `border`, `border-radius` and `background` in QSS -- it is
 *   the whole visible button, and RippleOverlay::install() puts the click-ripple on it (see
 *   rippleOverlay()), so whatever the inner button's border box covers is exactly what the
 *   ripple fills.
 * - The outer frame supports only `margin` in QSS, for spacing between adjacent PushButtons.
 *   `padding` on the outer frame is inert: the constructor resets this frame's own QVBoxLayout
 *   contents margins to 0 (via Layout::vertical(), see Layout::clear()), and an explicit
 *   QLayout::setContentsMargins() call overrides whatever QSS padding would otherwise feed into
 *   it. `border`/`background` on the outer frame would not line up with the ripple or the inner
 *   button's own border, since the inner button is pinned to its sizeHint and centered inside
 *   the frame (see setContentAlignment()) -- the frame can be larger than the button.
 * - `margin` on the *inner* button is unsupported: use `padding` on the inner button, or
 *   `margin` on the outer frame, instead.
 * - A `margin` on the outer frame does not need RippleOverlay's rippleInset* compensation --
 *   because the inner button is pinned to its sizeHint, a wrapper margin never enlarges the
 *   button's own rect(), so it can never reach the ripple host and never lets the ripple bleed
 *   into it. rippleInset* is only for QSS rules that grow the ripple *host's own* rect (a
 *   `margin` on a widget whose ripple installs on itself, e.g. IconTextButton/AvatarButton) or
 *   for hosts that need trimming for other reasons (e.g. shape correction).
 */
class UISE_DESKTOP_EXPORT PushButton : public QFrame
{
    Q_OBJECT

    public:

        PushButton(std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, bool toolButton=false);

        PushButton(QWidget* parent=nullptr, bool toolButton=false) : PushButton(std::shared_ptr<SvgIcon>{},parent,toolButton)
        {}

        PushButton(const QString text, QWidget* parent=nullptr, bool toolButton=false) : PushButton(std::shared_ptr<SvgIcon>{},parent,toolButton)
        {
            setText(text);
        }

        PushButton(const QString text, std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr, bool toolButton=false) : PushButton(std::move(icon),parent,toolButton)
        {
            setText(text);
        }

        void setSvgIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_icon=std::move(icon);
            if (m_icon)
            {
                setIcon(m_icon->icon());
            }
            else
            {
                setIcon(QIcon{});
            }
        }

        std::shared_ptr<SvgIcon> svgIcon() const
        {
            return m_icon;
        }

        void setParentHovered(bool enable);

        bool isParentHovered() const
        {
            return m_parentHovered;
        }

        void setChecked(bool enable);

        bool isChecked() const
        {
            return m_button->isChecked();
        }

        void setCheckable(bool enable);

        bool isCheckable() const
        {
            return m_button->isCheckable();
        }

        void setIcon(const QIcon& icon)
        {
            m_button->setIcon(icon);
        }

        QIcon icon() const
        {
            return m_button->icon();
        }

        void setText(const QString& text)
        {
            m_button->setText(text);
        }

        QString text() const
        {
            return m_button->text();
        }

        QPushButton* qPushButton() const
        {
            return m_pushButton;
        }

        QToolButton* qToolButton() const
        {
            return m_toolButton;
        }

        void resetHover();

        /**
         * @brief Set alignment of the inner button within this frame.
         *
         * Default Qt::AlignCenter pins the inner button to its sizeHint, centered -- the
         * historical behaviour. Dropping the horizontal flag (e.g. Qt::AlignVCenter) lets
         * the inner button fill the frame's width, which is what a stretched vertical
         * dialog-buttons column needs (see Dialog<>::applyButtonsLayout()).
         */
        void setContentAlignment(Qt::Alignment alignment);

        Qt::Alignment contentAlignment() const noexcept
        {
            return m_contentAlignment;
        }

        /** @brief The click-ripple overlay installed on this button's inner QPushButton/
         *  QToolButton, see RippleOverlay. */
        RippleOverlay* rippleOverlay() const noexcept
        {
            return m_ripple;
        }

    signals:

        void clicked();
        void toggled(bool enable);
        void hovered(bool enable);

    public slots:

        void click()
        {
            m_button->click();
        }

        void toggle()
        {
            m_button->toggle();
        }

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        QAbstractButton* m_button;

        QPushButton* m_pushButton;
        QToolButton* m_toolButton;

        std::shared_ptr<SvgIcon> m_icon;
        bool m_parentHovered;
        Qt::Alignment m_contentAlignment=Qt::AlignCenter;

        RippleOverlay* m_ripple=nullptr;
};

}

#endif // UISE_DESKTOP_PUSHBUTTON_HPP
