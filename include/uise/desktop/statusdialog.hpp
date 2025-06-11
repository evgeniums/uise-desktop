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

/** @file uise/desktop/statusdialog.hpp
*
*  Declares StatusDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STATUS_DIALOG_HPP
#define UISE_DESKTOP_STATUS_DIALOG_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class StatusDialog_p;

class UISE_DESKTOP_EXPORT StatusDialog : public QFrame
{
    Q_OBJECT

    public:

        enum class Type : int
        {
            Error,
            Warning,
            Info,
            Question,

            User=0x100
        };

        enum class StandardButton : int
        {
            Close=0,
            Accept=1,
            OK=2,
            Yes=3,
            Ignore=4,
            Cancel=5,
            No=6,
            Skip=7,
            Retry=8
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

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        StatusDialog(QWidget* parent=nullptr);

        /**
             * @brief Destructor.
             */
        ~StatusDialog();

        StatusDialog(const StatusDialog&)=delete;
        StatusDialog(StatusDialog&&)=delete;
        StatusDialog& operator=(const StatusDialog&)=delete;
        StatusDialog& operator=(StatusDialog&&)=delete;

        QLabel* textWidget() const;

        void setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={});

        void setStatus(const QString& message, Type type, const QString& title={});

        void setButtons(std::vector<ButtonConfig> buttons);

        static ButtonConfig standardButton(StandardButton button, QWidget* parent=nullptr);

    signals:

        void buttonClicked(int id);

        void closeRequested();

    public slots:

        void activateButton(int id);

    private:

        std::unique_ptr<StatusDialog_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STATUS_DIALOG_HPP
