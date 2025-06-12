/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/widget.cpp
*
*  Defines Widget.
*
*/

/****************************************************************************/

#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/widget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

QWidget* Widget::makeWidget(const char* className, QString name, QWidget* parent) const
{
    const auto* factory=m_factory.get();
    if (factory==nullptr)
    {
        factory=Style::instance().widgetFactory();
    }
    if (factory==nullptr)
    {
        return nullptr;
    }
    return factory->makeWidget(className,std::move(name),parent);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
