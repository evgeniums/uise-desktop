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

/** @file uise/desktop/wizarddialog.hpp
*
*  Declares WizardDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_WIZARD_DIALOG_HPP
#define UISE_DESKTOP_WIZARD_DIALOG_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class SvgIcon;
class WizardDialog_p;

class UISE_DESKTOP_EXPORT AbstractWizardDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        void setDefaultSvgIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_defaultSvgIcon=std::move(icon);
            updateDefaultSvgIcon();
        }

        std::shared_ptr<SvgIcon> defaultSvgIcon() const
        {
            return m_defaultSvgIcon;
        }

        virtual void addPage(QWidget* page, std::shared_ptr<SvgIcon> icon={})=0;

        virtual void setCurrentIndex(int index)=0;

        virtual int currentIndex() const=0;

        virtual void setPageSvgIcon(int index, std::shared_ptr<SvgIcon> icon)=0;

    protected:

        virtual void updateDefaultSvgIcon()
        {}

    private:

        std::shared_ptr<SvgIcon> m_defaultSvgIcon;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

class UISE_DESKTOP_EXPORT WizardDialog : public Dialog<AbstractWizardDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractWizardDialog>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        WizardDialog(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~WizardDialog();

        WizardDialog(const WizardDialog&)=delete;
        WizardDialog(WizardDialog&&)=delete;
        WizardDialog& operator=(const WizardDialog&)=delete;
        WizardDialog& operator=(WizardDialog&&)=delete;

        virtual void addPage(QWidget* page, std::shared_ptr<SvgIcon> icon={}) override;

        virtual void setCurrentIndex(int index) override;

        virtual int currentIndex() const override;

        virtual void setPageSvgIcon(int index, std::shared_ptr<SvgIcon> icon) override;

    protected:

        virtual void updateDefaultSvgIcon() override;

    private:

        std::unique_ptr<WizardDialog_p> pimpl;
};

using ModalWizard=ModalDialog<AbstractWizardDialog,WizardDialog,-1>;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_WIZARD_DIALOG_HPP
