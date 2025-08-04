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

/** @file uise/desktop/src/passworddialog.cpp
*
*  Defines PasswordDialog.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QLabel>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/passwordpanel.hpp>
#include <uise/desktop/passworddialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** PasswordDialog ***********************************/

//--------------------------------------------------------------------------

class PasswordDialog_p
{
    public:

        AbstractPasswordPanel* passwordPanel;
};

//--------------------------------------------------------------------------

PasswordDialog::PasswordDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<PasswordDialog_p>())
{
}

//--------------------------------------------------------------------------

void PasswordDialog::construct()
{
    pimpl->passwordPanel=makeWidget<AbstractPasswordPanel,PasswordPanel>();

    setWidget(pimpl->passwordPanel);

    setButtons(
        {
            AbstractDialog::standardButton(AbstractDialog::StandardButton::OK,this),
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
        }
    );

    connect(
        this,
        &Base::closeRequested,
        this,
        [this]()
        {
            pimpl->passwordPanel->reset();
        }
    );

    connect(
        this,
        &Base::buttonClicked,
        this,
        [this](int id)
        {
            if (isButton(id,StandardButton::OK) || isButton(id,StandardButton::Accept) || isButton(id,StandardButton::Apply))
            {
                emit passwordEntered();
            }
        }
        );

    connect(
        pimpl->passwordPanel,
        &AbstractPasswordPanel::passwordEntered,
        this,
        [this]()
        {
            emit passwordEntered();
        }
    );

    setInformationImpl(tr("Enter password"),tr("Password required"));
}

//--------------------------------------------------------------------------

PasswordDialog::~PasswordDialog()
{
}

//--------------------------------------------------------------------------

QString PasswordDialog::password() const
{
    return pimpl->passwordPanel->password();
}

//--------------------------------------------------------------------------

void PasswordDialog::setInformation(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    setInformationImpl(message,title,std::move(icon));
}

//--------------------------------------------------------------------------

void PasswordDialog::setInformationImpl(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    pimpl->passwordPanel->setDescription(message);
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

void PasswordDialog::setPasswordFocus()
{
    raise();
    setFocus();
    pimpl->passwordPanel->setPasswordFocus();
}

//--------------------------------------------------------------------------

void PasswordDialog::reset()
{
    pimpl->passwordPanel->reset();
}

//--------------------------------------------------------------------------

void PasswordDialog::setError(const QString& message)
{
    pimpl->passwordPanel->setError(message);
    updateMinimumHeight();
    setPasswordFocus();
}

//--------------------------------------------------------------------------

void PasswordDialog::updateMinimumHeight()
{
    auto newHeight=std::max(sizeHint().height(),150);
    if (newHeight>height())
    {
        setFixedHeight(newHeight);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
