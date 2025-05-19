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

#include <QPushButton>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT PushButton : public QPushButton
{
    public:

        PushButton(std::shared_ptr<SvgIcon> icon, QWidget* parent=nullptr)
            : QPushButton(parent),
              m_icon(std::move(icon))
        {}

        PushButton() = default;

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        std::shared_ptr<SvgIcon> m_icon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PUSHBUTTON_HPP
