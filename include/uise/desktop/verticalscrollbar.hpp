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

/** @file uise/desktop/verticalscrollbar.hpp
*
*  Declares vertical scrollbar with placeholder when invisible.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_VERTICALSCROLLBAR_HPP
#define UISE_DESKTOP_VERTICALSCROLLBAR_HPP

#include <QScrollBar>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT VerticalScrollBar : public QFrame
{
    Q_OBJECT

    public:

        explicit VerticalScrollBar(QWidget* parent=nullptr);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        QScrollBar* bar() const
        {
            return m_bar;
        }

        void setVisible(bool enable) override;

        bool isVisible() const
        {
            return m_visible;
        }

        void setHoldPlace(bool enable);

        bool isHoldPlace() const
        {
            return m_holdPlace;
        }

    private:

        QScrollBar* m_bar;
        bool m_holdPlace=false;
        bool m_visible=false;
};

}

#endif // UISE_DESKTOP_VERTICALSCROLLBAR_HPP
