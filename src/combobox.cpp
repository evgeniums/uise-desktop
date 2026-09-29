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

/** @file uise/desktop/combobox.cpp
*
*  Defines ComboBox.
*
*/

/****************************************************************************/

#include <QStyledItemDelegate>

#include <uise/desktop/combobox.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

ComboBox::ComboBox(QWidget* parent) : QComboBox(parent)
{
    // Replace the platform delegate (e.g. the native one on macOS) with Qt's own styled delegate
    // so that the popup items honour QSS on every platform. setItemDelegate() does not take
    // ownership of the delegate, so parent it to this combo box to avoid a leak.
    setItemDelegate(new QStyledItemDelegate(this));
}

//--------------------------------------------------------------------------

}
