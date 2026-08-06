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

/** @file uise/desktop/checkbox.hpp
*
*  Declares CheckBox.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHECKBOX_HPP
#define UISE_DESKTOP_CHECKBOX_HPP

#include <uise/desktop/abstractcheckbox.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Two-state checkbox: an AbstractCheckBox with a square indicator and a check-glyph mark.
 *
 * Differs from RadioBox only in the mark icon it resolves from the icon locator
 * ("CheckBox::mark", see checkbox.json), its default indicator shape (IndicatorShape::Box --
 * a square with a rounded corner, see checkbox.qss) and autoExclusive staying false --
 * everything else, including QButtonGroup membership, is inherited unchanged from
 * AbstractCheckBox.
 */
class UISE_DESKTOP_EXPORT CheckBox : public AbstractCheckBox
{
    Q_OBJECT

    public:

        explicit CheckBox(QWidget* parent=nullptr);
        explicit CheckBox(const QString& text, QWidget* parent=nullptr);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHECKBOX_HPP
