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

/** @file uise/desktop/checkbox.cpp
*
*  Defines CheckBox.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/checkbox.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

CheckBox::CheckBox(QWidget* parent)
    : AbstractCheckBox(parent)
{
    setAutoExclusive(false);
    setIndicatorShape(IndicatorShape::Box);
    // Alias declared in resources/style/checkbox.json; per-theme mark colours in
    // light|dark/checkbox.json. A stylesheet can switch to the pure-QSS mark instead with
    // qproperty-indicatorMode: "qss" (see #mark in light|dark/checkbox.qss).
    setMarkIcon(Style::instance().svgIconLocator().icon(QStringLiteral("CheckBox::mark"),this));
}

//--------------------------------------------------------------------------

CheckBox::CheckBox(const QString& text, QWidget* parent)
    : CheckBox(parent)
{
    setText(text);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
