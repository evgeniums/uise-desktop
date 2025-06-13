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
#include <uise/desktop/dialog.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class StatusDialog_p;

class UISE_DESKTOP_EXPORT AbstractStatusDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        enum class Type : int
        {
            Error,
            Warning,
            Info,
            Question,

            User=0x100
        };

        virtual QLabel* textWidget() const=0;

        virtual void setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={})=0;

        virtual void setStatus(const QString& message, Type type, const QString& title={})=0;
};

class UISE_DESKTOP_EXPORT StatusDialog : public Dialog<AbstractStatusDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractStatusDialog>;

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

        QLabel* textWidget() const override;

        void setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={}) override;

        void setStatus(const QString& message, Type type, const QString& title={}) override;

    private:

        std::unique_ptr<StatusDialog_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STATUS_DIALOG_HPP
