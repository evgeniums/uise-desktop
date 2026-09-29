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

/** @file uise/desktop/combobox.hpp
*
*  Declares ComboBox.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_COMBOBOX_HPP
#define UISE_DESKTOP_COMBOBOX_HPP

#include <QComboBox>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//! QComboBox with a QStyledItemDelegate forced on its popup view, so that the popup's items are
//! painted by Qt's own style rather than a platform-native delegate (e.g. macOS), which ignores
//! "QComboBox QAbstractItemView" rules in QSS. Use this everywhere instead of QComboBox so combo
//! popups stay stylable with QSS on every platform.
class UISE_DESKTOP_EXPORT ComboBox : public QComboBox
{
    Q_OBJECT

    public:

        explicit ComboBox(QWidget* parent=nullptr);
};

}

#endif // UISE_DESKTOP_COMBOBOX_HPP
