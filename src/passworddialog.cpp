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
#include <uise/desktop/passwordinput.hpp>
#include <uise/desktop/passworddialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** PasswordDialog ***********************************/

//--------------------------------------------------------------------------

class PasswordDialog_p
{
    public:

        QFrame* passwordDialogFrame;
        QFrame* passwordFrame;

        PushButton* icon;
        QLabel* text;
        QLabel* error;

        PasswordInput* password;
};

//--------------------------------------------------------------------------

PasswordDialog::PasswordDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<PasswordDialog_p>())
{
    pimpl->passwordDialogFrame=new QFrame(this);
    pimpl->passwordDialogFrame->setObjectName("dialogFrame");
    auto sl=Layout::horizontal(pimpl->passwordDialogFrame);

    pimpl->icon=new PushButton(this);
    sl->addWidget(pimpl->icon);
    pimpl->icon->setObjectName("dialogIcon");

    pimpl->passwordFrame=new QFrame(this);
    pimpl->passwordFrame->setObjectName("dialogInternal");
    sl->addWidget(pimpl->passwordFrame,1);
    auto l=Layout::vertical(pimpl->passwordFrame);
    l->addStretch(1);

    pimpl->text=new QLabel(pimpl->passwordFrame);
    l->addWidget(pimpl->text);
    pimpl->text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->text->setWordWrap(true);

    pimpl->password=new PasswordInput(pimpl->passwordFrame);
    l->addWidget(pimpl->password);

    pimpl->error=new QLabel(pimpl->passwordFrame);
    l->addWidget(pimpl->error);
    pimpl->error->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->error->setWordWrap(true);
    pimpl->error->setObjectName("error");

    l->addStretch(1);

    setWidget(pimpl->passwordDialogFrame);

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
            pimpl->password->editor()->clear();
            setError(QString{});
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
        pimpl->password,
        &PasswordInput::returnPressed,
        this,
        [this]()
        {
            emit passwordEntered();
        }
    );

    connect(
        pimpl->password->editor(),
        &QLineEdit::textChanged,
        this,
        [this](const QString&)
        {
            setError(QString{});
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
    return pimpl->password->editor()->text();
}

//--------------------------------------------------------------------------

void PasswordDialog::setInformation(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    setInformationImpl(message,title,std::move(icon));
}

//--------------------------------------------------------------------------

void PasswordDialog::setInformationImpl(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    pimpl->text->setText(message);
    if (icon)
    {
        pimpl->icon->setVisible(true);
    }    
    setTitle(title);

    if (!icon)
    {
        icon=Style::instance().svgIconLocator().icon("PasswordDialog::key",this);
    }
    pimpl->icon->setSvgIcon(std::move(icon));

    setMinimumWidth(400);
    setFixedHeight(sizeHint().height());
}

//--------------------------------------------------------------------------

void PasswordDialog::setPasswordFocus()
{
    pimpl->password->editor()->setFocus(Qt::MouseFocusReason);
}

//--------------------------------------------------------------------------

void PasswordDialog::reset()
{
    pimpl->password->editor()->clear();
    pimpl->error->setText(QString{});
}

//--------------------------------------------------------------------------

void PasswordDialog::setError(const QString& message)
{
    pimpl->error->setText(message);
}

/************************* FrameWithModalPasswordDialog ***********************/

//--------------------------------------------------------------------------

FrameWithModalPasswordDialog::FrameWithModalPasswordDialog(QWidget* parent) : FrameWithModalPopup(parent)
{
    setShortcutEnabled(false);
}

//--------------------------------------------------------------------------

bool FrameWithModalPasswordDialog::openDialog(bool destroyOnCancel)
{
    if (m_dialog)
    {
        popup();
        return false;
    }

    m_dialog=makeWidget<AbstractPasswordDialog,PasswordDialog>();

    setPopupWidget(m_dialog,destroyOnCancel);
    connect(
        m_dialog,
        &PasswordDialog::closeRequested,
        this,
        &FrameWithModalPopup::closePopup
    );

    popup();
    m_dialog->setFocus();
    m_dialog->setPasswordFocus();
    return true;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
