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

/** @file uise/desktop/newpassworddialog.hpp
*
*  Declares NewPasswordDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_NEW_PASSWORD_DIALOG_HPP
#define UISE_DESKTOP_NEW_PASSWORD_DIALOG_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/modaldialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class AbstractPasswordInput;
class AbstractNewPasswordPanel;
class NewPasswordDialog_p;

class UISE_DESKTOP_EXPORT AbstractNewPasswordDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual void setInformation(const QString& message, const QString& title={}, std::shared_ptr<SvgIcon> icon={})=0;

        virtual QString currentPassword() const=0;

        virtual QString password() const=0;

        virtual void setError(const QString& message)=0;

        virtual void reset()=0;

        virtual void setCurrentPasswordTitle(const QString& title)=0;

        virtual QString currentPasswordTitle() const=0;

        virtual void setPasswordTitle(const QString& title)=0;

        virtual void setRepeatPasswordTitle(const QString& title)=0;

        virtual AbstractPasswordInput* currentPasswordInput() const=0;

        virtual AbstractNewPasswordPanel* newPasswordPanel() const=0;

    signals:

        void passwordEntered();
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

class UISE_DESKTOP_EXPORT NewPasswordDialog : public Dialog<AbstractNewPasswordDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractNewPasswordDialog>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        NewPasswordDialog(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~NewPasswordDialog();

        NewPasswordDialog(const NewPasswordDialog&)=delete;
        NewPasswordDialog(NewPasswordDialog&&)=delete;
        NewPasswordDialog& operator=(const NewPasswordDialog&)=delete;
        NewPasswordDialog& operator=(NewPasswordDialog&&)=delete;

        virtual void setInformation(const QString& message, const QString& title={}, std::shared_ptr<SvgIcon> icon={}) override;
        virtual QString currentPassword() const override;
        virtual QString password() const override;

        virtual void setError(const QString& message) override;

        void setDialogFocus() override
        {
            setPasswordFocus();
        }

        void setPasswordFocus();

        virtual void reset() override;

        virtual void construct() override;

        virtual void setCurrentPasswordTitle(const QString& title) override;

        virtual QString currentPasswordTitle() const override;

        virtual void setPasswordTitle(const QString& title) override;

        virtual void setRepeatPasswordTitle(const QString& title) override;

        virtual AbstractPasswordInput* currentPasswordInput() const override;

        virtual AbstractNewPasswordPanel* newPasswordPanel() const override;

    private:

        void updateMinimumHeight();

        void setInformationImpl(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon={});

        std::unique_ptr<NewPasswordDialog_p> pimpl;
};

using ModalNewPasswordDialogType=ModalDialog<AbstractNewPasswordDialog,NewPasswordDialog,ModalDialogDefaultPopupMaxWidth,ModalDialogDefaultMaxWidthPercent,-1,30>;

class UISE_DESKTOP_EXPORT ModalNewPasswordDialog : public ModalNewPasswordDialogType
{
    Q_OBJECT

    public:

        using ModalNewPasswordDialogType::ModalNewPasswordDialogType;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_NEW_PASSWORD_DIALOG_HPP
