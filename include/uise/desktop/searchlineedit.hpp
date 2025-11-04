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

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT SearchLineEdit : public LineEdit
{
    Q_OBJECT

    public:

        SearchLineEdit(QWidget* parent=nullptr);

        ActionWithSvgIcon addActionWithSvgIcon(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position)=delete;

        PushButton* addPushButton(std::shared_ptr<SvgIcon> icon);

        void addPushButton(PushButton* button);

    public slots:

        void cancel();
        void edit();

    signals:

        void cancelled();
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
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SEARCHLINEEDIT_HPP
