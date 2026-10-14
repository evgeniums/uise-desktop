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
#include <QFrame>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/passwordpanel.hpp>
#include <uise/desktop/passworddialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**************************** PasswordDialog ***********************************/

//--------------------------------------------------------------------------

class PasswordDialog_p
{
    public:

        AbstractPasswordPanel* passwordPanel;
        CheckBox* checkBox;
        QFrame* checkBoxRow;
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

    // The optional checkbox lives inside the panel, right under the input. Placed after the panel
    // it would sit below the error line's reserved (blank) height.
    pimpl->checkBoxRow=new QFrame(pimpl->passwordPanel);
    pimpl->checkBoxRow->setObjectName("checkBoxRow");
    pimpl->checkBox=new CheckBox(pimpl->checkBoxRow);
    pimpl->checkBox->setObjectName("optionCheckBox");
    auto* rl=Layout::horizontal(pimpl->checkBoxRow);
    rl->addWidget(pimpl->checkBox);
    pimpl->checkBoxRow->setVisible(false);
    pimpl->passwordPanel->insertBelowInput(pimpl->checkBoxRow);

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
            reset();
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
                setError(QString{});
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
    pimpl->checkBox->setChecked(false);
}

//--------------------------------------------------------------------------

void PasswordDialog::setOptionCheckBox(const QString& text, bool checked)
{
    pimpl->checkBox->setText(text);
    pimpl->checkBox->setChecked(checked);
    pimpl->checkBoxRow->setVisible(true);
    updateMinimumHeight();
}

//--------------------------------------------------------------------------

bool PasswordDialog::isOptionChecked() const
{
    // isHidden() rather than isVisible(): the answer must not depend on the dialog being on screen
    return !pimpl->checkBoxRow->isHidden() && pimpl->checkBox->isChecked();
}

//--------------------------------------------------------------------------

void PasswordDialog::clearOptionCheckBox()
{
    pimpl->checkBoxRow->setVisible(false);
    pimpl->checkBox->setChecked(false);
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
    return;
    auto newHeight=std::max(sizeHint().height(),150);
    if (newHeight>minimumHeight())
    {
        setMinimumHeight(newHeight);
    }
}

//--------------------------------------------------------------------------

}
