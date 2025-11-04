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

/** @file uise/desktop/lineedit.hpp
*
*  Declares LineEdit.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LINEEDIT_HPP
#define UISE_DESKTOP_LINEEDIT_HPP

#include <QLineEdit>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;
class SingleShotTimer;

struct ActionWithSvgIcon
{
    std::shared_ptr<SvgIcon> icon;
    QAction* action=nullptr;

    ActionWithSvgIcon(std::shared_ptr<SvgIcon> icon={}, QAction* action=nullptr)
        : icon(std::move(icon)),
          action(action)
    {}
};

class UISE_DESKTOP_EXPORT LineEdit : public QLineEdit
{
    Q_OBJECT

    public:

        using QLineEdit::QLineEdit;

        ActionWithSvgIcon addActionWithSvgIcon(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position);

        PushButton* addPushButton(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position);

        void addPushButton(PushButton* button, QLineEdit::ActionPosition position);

        void setParentHovered(bool enable);

        bool isParentHovered() const
        {
            return m_parentHovered;
        }

        void updateButtonPositions();

    signals:

        void hovered(bool enable);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

    private:

        void updateIcons(bool hovered);        

        std::vector<ActionWithSvgIcon> m_actions;
        bool m_parentHovered=false;

        std::vector<PushButton*> m_leadingButtons;
        std::vector<PushButton*> m_trailingButtons;

        SingleShotTimer* m_updatePositionsTimer=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LINEEDIT_HPP
