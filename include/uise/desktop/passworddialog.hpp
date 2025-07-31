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

/** @file uise/desktop/passworddialog.hpp
*
*  Declares PasswordDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PASSWORD_DIALOG_HPP
#define UISE_DESKTOP_PASSWORD_DIALOG_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/widget.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class PasswordDialog_p;

class UISE_DESKTOP_EXPORT AbstractPasswordDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual void setInformation(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={})=0;

        virtual QString password() const=0;

        virtual void setError(const QString& message)=0;

        virtual void setPasswordFocus()=0;

        virtual void reset()=0;

    signals:

        void passwordEntered();
};

class UISE_DESKTOP_EXPORT PasswordDialog : public Dialog<AbstractPasswordDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractPasswordDialog>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        PasswordDialog(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~PasswordDialog();

        PasswordDialog(const PasswordDialog&)=delete;
        PasswordDialog(PasswordDialog&&)=delete;
        PasswordDialog& operator=(const PasswordDialog&)=delete;
        PasswordDialog& operator=(PasswordDialog&&)=delete;

        virtual void setInformation(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={}) override;
        virtual QString password() const override;

        virtual void setError(const QString& message) override;

        virtual void setPasswordFocus() override;

        virtual void reset() override;

    private:

        void setInformationImpl(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={});

        std::unique_ptr<PasswordDialog_p> pimpl;
};

class UISE_DESKTOP_EXPORT FrameWithModalPasswordDialog : public FrameWithModalPopup,
                                                         public Widget
{
    Q_OBJECT

    public:

        FrameWithModalPasswordDialog(QWidget* parent=nullptr);

        /**
         * @brief Open password dialog.
         * @parame destroyOnCancel Destroy dialog if cancelled
         * @return Returns true if new dialog is created, false if dialog already existed.
         */
        bool openDialog(bool destroyOnCancel=true);

        QPointer<AbstractPasswordDialog> dialog() const
        {
            return m_dialog;
        }

    private:

        QPointer<AbstractPasswordDialog> m_dialog;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PASSWORD_DIALOG_HPP
