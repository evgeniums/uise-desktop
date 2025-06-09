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

/** @file uise/desktop/editablepanel.hpp
*
*  Declares EditablePanel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITABLEPANEL_HPP
#define UISE_DESKTOP_EDITABLEPANEL_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

class EditablePanel_p;

class UISE_DESKTOP_EXPORT EditablePanel : public QFrame
{
    Q_OBJECT

    public:

        enum class ButtonsMode : int
        {
            TopAlwaysVisible,
            TopOnHoverVisible,
            BottomAlwaysVisible
        };

        EditablePanel(QWidget* parent=nullptr);

        ~EditablePanel();

        EditablePanel(const EditablePanel&)=delete;
        EditablePanel(EditablePanel&&)=delete;
        EditablePanel& operator=(const EditablePanel&)=delete;
        EditablePanel& operator=(EditablePanel&&)=delete;

        void setEditable(bool enable);
        bool isEditable() const noexcept;

        bool isEditingMode() const noexcept;

        void setTitle(const QString& title);
        QString title() const;

        void setTitleVisible(bool enable);
        bool isTitleVisible() const noexcept;

        void setButtonsMode(ButtonsMode mode);
        ButtonsMode buttonsMode() const noexcept;

        void setBottomApplyText(const QString& text);
        QString bottomApplyText() const;

        void setBottomApplyIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> bottomApplyIcon() const;

        void setWidget(QWidget* widget);

    signals:

        void editRequested();
        void cancelRequested();
        void applyRequested();

    public slots:

        void edit();
        void apply();
        void cancel();

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void setEditingMode(bool enable);
        void updateState();

        std::unique_ptr<EditablePanel_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANEL_HPP
