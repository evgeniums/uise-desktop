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

/** @file uise/desktop/radiobox.cpp
*
*  Defines RadioBox.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/radiobox.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

RadioBox::RadioBox(QWidget* parent)
    : AbstractCheckBox(parent)
{
    setAutoExclusive(true);
    setIndicatorShape(IndicatorShape::Circle);
    // Alias declared in resources/style/checkbox.json; per-theme mark colours in
    // light|dark/checkbox.json. A stylesheet can switch to the pure-QSS mark instead with
    // qproperty-indicatorMode: "qss" (see #mark in light|dark/checkbox.qss).
    setMarkIcon(Style::instance().svgIconLocator().icon(QStringLiteral("RadioBox::mark"),this));
}

//--------------------------------------------------------------------------

RadioBox::RadioBox(const QString& text, QWidget* parent)
    : RadioBox(parent)
{
    setText(text);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
