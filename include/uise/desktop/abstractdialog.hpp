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

/** @file uise/desktop/abstractdialog.hpp
*
*  Declares AbstractDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_DIALOG_HPP
#define UISE_DESKTOP_ABSTRACT_DIALOG_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

class UISE_DESKTOP_EXPORT AbstractDialog : public QFrame,
                                           public Widget
{
    Q_OBJECT

    public:

        enum class StandardButton : int
        {
            Close=0,
            Accept=1,
            OK=2,
            Apply=3,
            Yes=4,
            Ignore=5,
            Cancel=6,
            No=7,
            Skip=8,
            Retry=9,
            Next=10,
            Back=11,
            Start=12,
            Finish=13,
            Complete=14
        };

        struct ButtonConfig
        {
            int id=0;
            QString text;
            std::shared_ptr<SvgIcon> icon;
            QString name;

            ButtonConfig(int id, QString text, std::shared_ptr<SvgIcon> icon={})
                : id(id),
                  text(std::move(text)),
                  icon(std::move(icon))
            {
                name=this->text;
            }

            ButtonConfig(StandardButton button, QWidget* parent=nullptr) : ButtonConfig(standardButton(button,parent))
            {}
        };

        using QFrame::QFrame;

        virtual void setButtons(std::vector<ButtonConfig> buttons)=0;

        static ButtonConfig standardButton(StandardButton button, QWidget* parent=nullptr);

        static bool isButton(int id, StandardButton button)
        {
            return id==static_cast<int>(button);
        }

        template <typename T>
        void setButtonVisible(T id, bool enable)
        {
            setButtonVisible(static_cast<int>(id),enable);
        }

        template <typename T>
        void setButtonEnabled(T id, bool enable)
        {
            setButtonEnabled(static_cast<int>(id),enable);
        }

        template <typename T>
        void activateButton(T id)
        {
            activateButton(static_cast<int>(id));
        }

        virtual void setTitle(const QString& title)=0;

        virtual void setDialogFocus()
        {}

        virtual void setClosable(bool enable)
        {
            std::ignore=enable;
        }

        virtual void setButtonsStyle(ButtonsStyle style) =0;
        virtual void resetButtonsStyle() =0;

    signals:

        void buttonClicked(int id);

        void closeRequested();

    public slots:

        void activateButton(int id);
        void setButtonEnabled(int id, bool enable);
        void setButtonVisible(int id, bool enable);
        void closeDialog();

    protected:

        virtual void doActivateButton(int id)=0;
        virtual void doSetButtonEnabled(int id, bool enable)=0;
        virtual void doSetButtonVisible(int id, bool enable)=0;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_DIALOG_HPP
