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

/** @file uise/desktop/searchlineedit.hpp
*
*  Declares SearchLineEdit.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SEARCHLINEEDIT_HPP
#define UISE_DESKTOP_SEARCHLINEEDIT_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/lineedit.hpp>

class QShortcut;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT SearchLineEdit : public LineEdit
{
    Q_OBJECT

    public:

        SearchLineEdit(QWidget* parent=nullptr);

        ActionWithSvgIcon addActionWithSvgIcon(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position)=delete;

        PushButton* addPushButton(std::shared_ptr<SvgIcon> icon);

        void addPushButton(PushButton* button);

        void setSearchButtonVisible(bool enable);

        /**
         * @brief Enable/disable the built-in Escape-to-cancel shortcut. Enabled by default.
         *
         * That shortcut uses Qt::WidgetShortcut context, so while this line has focus Qt
         * prefers it over any less specific binding of the same key -- including a
         * Qt::WindowShortcut Escape on an enclosing dialog, which then never fires. Turn it
         * off when the host owns Escape and the line should not intercept it.
         */
        void setCancelShortcutEnabled(bool enable);

        bool isCancelShortcutEnabled() const;

    public slots:

        virtual void cancel() override;
        void edit();

    signals:

        void editingModeChanged(bool enable);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void doCancel(bool keepFocus);

        PushButton* m_searchButton;
        PushButton* m_cancelButton;
        QShortcut* m_cancelShortcut;

        bool m_editing;
        bool m_searchButtonVisible=true;
};

}

#endif // UISE_DESKTOP_SEARCHLINEEDIT_HPP
