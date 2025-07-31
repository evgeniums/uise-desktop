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

/** @file uise/desktop/newpasswordwizard.hpp
*
*  Declares NewPasswordWizard.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_NEW_PASSWORD_WIZARD_HPP
#define UISE_DESKTOP_NEW_PASSWORD_WIZARD_HPP

#include <memory>

#include <QWidget>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/abstractdialog.hpp>
#include <uise/desktop/modaldialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class AbstractWizardDialog;

class UISE_DESKTOP_EXPORT AbstractNewPasswordWizard : public QObject,
                                                      public Widget
{
    Q_OBJECT

    public:

        enum class InformationFormat : int
        {
            Plain,
            Html,
            Markdown
        };

        using ValidatorStatus=QString;
        using ValidatorFn=std::function<ValidatorStatus (const QString&)>;

        static bool validationPassed(const ValidatorStatus& status)
        {
            return status.isEmpty();
        }

        static QString validationError(const ValidatorStatus& status)
        {
            return status;
        }

        AbstractNewPasswordWizard(QObject* parent=nullptr);

        virtual void setIntroInformation(const QString& text, std::shared_ptr<SvgIcon> icon={}, InformationFormat format=InformationFormat::Plain)=0;
        virtual void setPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon={})=0;
        virtual void setRepeatPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon={})=0;

        virtual void setValidator(ValidatorFn validator)
        {
            m_validator=std::move(validator);
        }

        virtual QString password() const=0;

        virtual AbstractDialog* dialogWidget() const = 0;

        virtual void reset() =0;

        void setMismatchError(QString msg)
        {
            m_mismatchError=std::move(msg);
        }

        void setEmptyError(QString msg)
        {
            m_emptyError=std::move(msg);
        }

    signals:

        void completed();
        void cancelled();

    protected:

        ValidatorFn m_validator;
        QString m_mismatchError;
        QString m_emptyError;
};

class NewPasswordWizard_p;

class UISE_DESKTOP_EXPORT NewPasswordWizard : public AbstractNewPasswordWizard
{
    Q_OBJECT

    public:

        /**
             * @brief Constructor.
             * @param parent Parent widget.
             */
        NewPasswordWizard(QWidget* parent=nullptr);

        ~NewPasswordWizard();

        NewPasswordWizard(const NewPasswordWizard&)=delete;
        NewPasswordWizard(NewPasswordWizard&&)=delete;
        NewPasswordWizard& operator=(const NewPasswordWizard&)=delete;
        NewPasswordWizard& operator=(NewPasswordWizard&&)=delete;

        virtual void setIntroInformation(const QString& text, std::shared_ptr<SvgIcon> icon={}, InformationFormat format=InformationFormat::Plain) override;
        virtual void setPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon={}) override;
        virtual void setRepeatPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon={}) override;

        virtual QString password() const override;

        virtual AbstractDialog* dialogWidget() const override;

        virtual void construct() override;

        virtual void reset() override;

    private:

        void complete();

        std::unique_ptr<NewPasswordWizard_p> pimpl;
};

struct NewPasswordWidgetExtractor
{
    template <typename T>
    static auto dialogWidget(T dialog)
    {
        return dialog->dialogWidget();
    }
};

using ModalNewPasswordWizard=ModalDialog<AbstractNewPasswordWizard,NewPasswordWizard,600,ModalDialogDefaultMaxWidthPercent,-1,ModalDialogDefaultMaxHeightPercent,NewPasswordWidgetExtractor>;

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_WIZARD_DIALOG_HPP
