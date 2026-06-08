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

/** @file uise/desktop/src/newpassworddialog.cpp
*
*  Defines NewPasswordDialog.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QLabel>
#include <QFrame>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/passwordinput.hpp>
#include <uise/desktop/newpasswordpanel.hpp>
#include <uise/desktop/newpassworddialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** NewPasswordDialog ***********************************/

//--------------------------------------------------------------------------

class NewPasswordDialog_p
{
    public:

        PasswordInput* currentInput;
        AbstractNewPasswordPanel* panel;
};

//--------------------------------------------------------------------------

NewPasswordDialog::NewPasswordDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<NewPasswordDialog_p>())
{
}

//--------------------------------------------------------------------------

void NewPasswordDialog::construct()
{
    auto* container=new QFrame(this);
    auto l=Layout::vertical(container);

    pimpl->currentInput=new PasswordInput(container);
    pimpl->currentInput->setObjectName("currentPasswordInput");
    pimpl->currentInput->setApplyButtonVisible(false);
    pimpl->currentInput->setTitleVisible(true);
    pimpl->currentInput->setTitle(tr("Current password"));
    l->addWidget(pimpl->currentInput);

    pimpl->panel=makeWidget<AbstractNewPasswordPanel,NewPasswordPanel>();
    pimpl->panel->setPasswordGenerator(pimpl->panel->defaultPasswordGenerator());
    l->addWidget(pimpl->panel);

    setWidget(container);

    setButtons(
        {
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Apply,this),
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
        }
    );

    connect(
        this,
        &Base::closeRequested,
        this,
        [this]()
        {
            pimpl->currentInput->reset();
            pimpl->panel->reset();
        }
    );

    connect(
        this,
        &Base::buttonClicked,
        this,
        [this](int id)
        {
            if (isButton(id,StandardButton::Apply) || isButton(id,StandardButton::Accept) || isButton(id,StandardButton::OK))
            {
                if (pimpl->panel->checkPassword())
                {
                    emit passwordEntered();
                }
                else
                {
                    updateMinimumHeight();
                }
            }
        }
    );

    connect(
        pimpl->currentInput,
        &AbstractPasswordInput::passwordEntered,
        this,
        [this]()
        {
            pimpl->panel->setPasswordFocus();
        }
    );

    connect(
        pimpl->panel,
        &AbstractNewPasswordPanel::passwordEntered,
        this,
        [this]()
        {
            emit passwordEntered();
        }
    );

    setInformationImpl(tr("Set new password"),tr("Change password"));
}

//--------------------------------------------------------------------------

NewPasswordDialog::~NewPasswordDialog()
{
}

//--------------------------------------------------------------------------

QString NewPasswordDialog::currentPassword() const
{
    return pimpl->currentInput->password();
}

//--------------------------------------------------------------------------

QString NewPasswordDialog::password() const
{
    return pimpl->panel->password();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setInformation(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    setInformationImpl(message,title,std::move(icon));
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setInformationImpl(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    pimpl->panel->setPasswordTitle(message);
    if (!title.isEmpty())
    {
        setTitle(title);
    }

    if (!icon)
    {
        icon=Style::instance().svgIconLocator().icon("PasswordDialog::key",this);
    }
    setSvgIcon(std::move(icon));

    setMinimumWidth(450);
    updateMinimumHeight();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setPasswordFocus()
{
    raise();
    setFocus();
    pimpl->currentInput->editor()->setFocus();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::reset()
{
    pimpl->currentInput->reset();
    pimpl->panel->reset();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setError(const QString& message)
{
    pimpl->panel->setError(message);
    updateMinimumHeight();
    setPasswordFocus();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setCurrentPasswordTitle(const QString& title)
{
    pimpl->currentInput->setTitle(title);
}

//--------------------------------------------------------------------------

QString NewPasswordDialog::currentPasswordTitle() const
{
    return pimpl->currentInput->title();
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setPasswordTitle(const QString& title)
{
    pimpl->panel->setPasswordTitle(title);
}

//--------------------------------------------------------------------------

void NewPasswordDialog::setRepeatPasswordTitle(const QString& title)
{
    pimpl->panel->setRepeatPasswordTitle(title);
}

//--------------------------------------------------------------------------

AbstractPasswordInput* NewPasswordDialog::currentPasswordInput() const
{
    return pimpl->currentInput;
}

//--------------------------------------------------------------------------

AbstractNewPasswordPanel* NewPasswordDialog::newPasswordPanel() const
{
    return pimpl->panel;
}

//--------------------------------------------------------------------------

void NewPasswordDialog::updateMinimumHeight()
{
    auto newHeight=std::max(sizeHint().height(),150);
    if (newHeight>minimumHeight())
    {
        setMinimumHeight(newHeight);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
