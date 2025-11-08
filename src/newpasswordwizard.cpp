/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/newpasswordwizard.cpp
*
*  Defines NewPasswordWizard.
*
*/

/****************************************************************************/

#include <QTextBrowser>

#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/style.hpp>

#include <uise/desktop/wizarddialog.hpp>
#include <uise/desktop/passwordpanel.hpp>
#include <uise/desktop/newpasswordwizard.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** AbstractNewPasswordWizard *****************************/

AbstractNewPasswordWizard::AbstractNewPasswordWizard(QObject* parent) : QObject(parent)
{
    m_mismatchError=tr("Entered passwords do not match");
    m_emptyError=tr("Password cannot be empty");
}

/**************************** NewPasswordWizard ***********************************/

class NewPasswordWizard_p
{
    public:

        QPointer<AbstractWizardDialog> wizard;
        QWidget* parent=nullptr;

        QTextBrowser* page0=nullptr;
        AbstractPasswordPanel* page1=nullptr;
        AbstractPasswordPanel* page2=nullptr;
};

//--------------------------------------------------------------------------

NewPasswordWizard::NewPasswordWizard(QWidget* parent) :
    AbstractNewPasswordWizard(parent),
    pimpl(std::make_unique<NewPasswordWizard_p>())
{
    pimpl->parent=parent;
}

//--------------------------------------------------------------------------

NewPasswordWizard::~NewPasswordWizard()
{}

//--------------------------------------------------------------------------

AbstractDialog* NewPasswordWizard::dialogWidget() const
{
    return pimpl->wizard;
}

//--------------------------------------------------------------------------

void NewPasswordWizard::construct()
{
    pimpl->wizard=makeWidget<AbstractWizardDialog,WizardDialog>("NewPasswordWizard",pimpl->parent);
    connect(
        pimpl->wizard,
        SIGNAL(destroyed()),
        this,
        SLOT(deleteLater())
    );

    pimpl->wizard->setTitle(tr("Create a password"));

    pimpl->page0=new QTextBrowser();
    pimpl->wizard->addPage(pimpl->page0,Style::instance().svgIconLocator().icon("NewPasswordWizard::intro"));
    pimpl->page0->setMarkdown(tr(
        "### Choose a strong password\n"
        "Your password should be hard to guess. It should not contain personal information like your birth date or phone number.  \n"
        "Long passwords are stronger, so make your password at least 12 characters long.  \n"
        "These tips can help you create longer passwords that are easier to remember. Try to use: \n "
        "* A lyric from a song or poem\n"
        "* A meaningful quote from a movie or speech\n"
        "* A passage from a book\n"
        "* A series of words that are meaningful to you\n"
        "* An abbreviation: Make a password from the first letter of each word in a sentence\n"
        )
    );

    pimpl->page1=makeWidget<AbstractPasswordPanel,PasswordPanel>();
    pimpl->page1->setDescription(tr("Enter new password"));
    pimpl->page1->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    pimpl->wizard->addPage(pimpl->page1,Style::instance().svgIconLocator().icon("PasswordDialog::key"));

    pimpl->page2=makeWidget<AbstractPasswordPanel,PasswordPanel>();
    pimpl->page2->setDescription(tr("Repeat password"));
    pimpl->page2->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    pimpl->wizard->addPage(pimpl->page2,Style::instance().svgIconLocator().icon("PasswordDialog::password"));

    connect(
        pimpl->wizard,
        &AbstractWizardDialog::buttonClicked,
        this,
        [this](int id)
        {
            if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Complete))
            {
                complete();
            }
            else if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Back))
            {
                pimpl->wizard->setCurrentIndex(pimpl->wizard->currentIndex()-1);
                pimpl->page1->setError({});
                pimpl->page2->setError({});

                if (pimpl->wizard->currentIndex()==1)
                {
                    pimpl->page1->setPasswordFocus();
                }
            }
            else if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Next))
            {
                if (pimpl->wizard->currentIndex()==1)
                {
                    if (m_validator)
                    {
                        auto status=m_validator(pimpl->page1->password());
                        if (!validationPassed(status))
                        {
                            pimpl->page1->setError(validationError(status));
                            return;
                        }
                    }
                    if (pimpl->page1->password().isEmpty())
                    {
                        pimpl->page1->setError(m_emptyError);
                        return;
                    }
                }

                pimpl->wizard->setCurrentIndex(pimpl->wizard->currentIndex()+1);
                if (pimpl->wizard->currentIndex()==1)
                {
                    pimpl->page1->setPasswordFocus();
                }
                else if (pimpl->wizard->currentIndex()==2)
                {
                    pimpl->page2->setPasswordFocus();
                }
            }
        }
    );

    connect(
        pimpl->page1,
        &AbstractPasswordPanel::passwordEntered,
        this,
        [this]()
        {
            pimpl->wizard->activateButton(AbstractDialog::StandardButton::Next);
        }
    );

    connect(
        pimpl->page2,
        &AbstractPasswordPanel::passwordEntered,
        this,
        [this]()
        {
            pimpl->wizard->activateButton(AbstractDialog::StandardButton::Complete);
        }
    );

    connect(
        pimpl->wizard,
        &AbstractWizardDialog::closeRequested,
        this,
        [this]()
        {
            reset();
            emit cancelled();
        }
    );

    pimpl->wizard->setCurrentIndex(0);
}

//--------------------------------------------------------------------------

void NewPasswordWizard::setIntroInformation(const QString& text, std::shared_ptr<SvgIcon> icon, InformationFormat format)
{
    if (!text.isEmpty())
    {
        switch (format)
        {
            case(InformationFormat::Plain): pimpl->page0->setText(text); break;
            case(InformationFormat::Html): pimpl->page0->setHtml(text); break;
            case(InformationFormat::Markdown): pimpl->page0->setMarkdown(text); break;
        }
    }
    if (icon)
    {
        pimpl->wizard->setPageSvgIcon(0,std::move(icon));
    }
}

//--------------------------------------------------------------------------

void NewPasswordWizard::setPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon)
{
    if (!text.isEmpty())
    {
        pimpl->page1->setDescription(text);
    }
    if (icon)
    {
        pimpl->wizard->setPageSvgIcon(1,std::move(icon));
    }
}

//--------------------------------------------------------------------------

void NewPasswordWizard::setRepeatPasswordInformation(const QString& text, std::shared_ptr<SvgIcon> icon)
{
    if (!text.isEmpty())
    {
        pimpl->page2->setDescription(text);
    }
    if (icon)
    {
        pimpl->wizard->setPageSvgIcon(2,std::move(icon));
    }
}

//--------------------------------------------------------------------------

QString NewPasswordWizard::password() const
{
    return pimpl->page1->password();
}

//--------------------------------------------------------------------------

void NewPasswordWizard::complete()
{
    if (pimpl->page1->password()!=pimpl->page2->password())
    {
        pimpl->page2->setError(m_mismatchError);
    }
    else
    {
        emit completed();
    }
}

//--------------------------------------------------------------------------

void NewPasswordWizard::reset()
{
    pimpl->page1->reset();
    pimpl->page2->reset();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
