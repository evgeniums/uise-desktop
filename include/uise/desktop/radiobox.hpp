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

/** @file uise/desktop/radiobox.hpp
*
*  Declares RadioBox.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_RADIOBOX_HPP
#define UISE_DESKTOP_RADIOBOX_HPP

#include <uise/desktop/abstractcheckbox.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Exclusive-by-default radio button: an AbstractCheckBox with a round indicator and a
 *  dot mark.
 *
 * Differs from CheckBox only in the mark icon it resolves from the icon locator
 * ("RadioBox::mark", see checkbox.json), its default indicator shape (IndicatorShape::Circle)
 * and setAutoExclusive(true). No nextCheckState() override is needed here to enforce
 * exclusivity: QAbstractButtonPrivate::click() already refuses to uncheck the checked member
 * of an auto-exclusive group before calling nextCheckState(), which is exactly the
 * mutual-exclusion behaviour a radio button needs. (AbstractCheckBox itself does override
 * nextCheckState(), but only to keep the painted state in sync with isChecked() -- see its
 * checkStateSet()/nextCheckState() overrides -- which is orthogonal to exclusivity.) A
 * RadioBox can be grouped either by sharing a parent (autoExclusive() alone) or by explicit
 * QButtonGroup membership -- both work unchanged, since AbstractCheckBox derives from
 * QAbstractButton.
 */
class UISE_DESKTOP_EXPORT RadioBox : public AbstractCheckBox
{
    Q_OBJECT

    public:

        explicit RadioBox(QWidget* parent=nullptr);
        explicit RadioBox(const QString& text, QWidget* parent=nullptr);
};

}

#endif // UISE_DESKTOP_RADIOBOX_HPP
