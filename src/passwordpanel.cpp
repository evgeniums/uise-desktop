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

/** @file uise/desktop/src/passwordpanel.cpp
*
*  Defines PasswordPanel.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QLabel>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/passwordinput.hpp>
#include <uise/desktop/passwordpanel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** PasswordPanel ***********************************/

//--------------------------------------------------------------------------

class PasswordPanel_p
{
    public:

        QLabel* description;
        QLabel* error;

        PasswordInput* password;
};

//--------------------------------------------------------------------------

PasswordPanel::PasswordPanel(QWidget* parent)
    : AbstractPasswordPanel(parent),
      pimpl(std::make_unique<PasswordPanel_p>())
{
}

//--------------------------------------------------------------------------

void PasswordPanel::construct()
{
    auto l=Layout::vertical(this);
    l->addStretch(1);

    pimpl->description=new QLabel(this);
    l->addWidget(pimpl->description);
    pimpl->description->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->description->setWordWrap(true);

    pimpl->password=new PasswordInput(this);
    l->addWidget(pimpl->password);

    pimpl->error=new QLabel(this);
    l->addWidget(pimpl->error);
    pimpl->error->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->error->setWordWrap(true);
    pimpl->error->setObjectName("error");

    l->addStretch(1);

    connect(
        pimpl->password,
        &PasswordInput::passwordEntered,
        this,
        [this]()
        {
            setError(QString{});
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

    setDescriptionImpl(tr("Enter password"));
}

//--------------------------------------------------------------------------

PasswordPanel::~PasswordPanel()
{
}

//--------------------------------------------------------------------------

QString PasswordPanel::password() const
{
    return pimpl->password->editor()->text();
}

//--------------------------------------------------------------------------

void PasswordPanel::setDescription(const QString& message)
{
    setDescriptionImpl(message);
}

//--------------------------------------------------------------------------

void PasswordPanel::setDescriptionImpl(const QString& message)
{
    pimpl->description->setText(message);
}

//--------------------------------------------------------------------------

void PasswordPanel::setPasswordFocus()
{
    pimpl->password->editor()->setFocus(Qt::MouseFocusReason);
}

//--------------------------------------------------------------------------

void PasswordPanel::reset()
{
    pimpl->password->reset();
    pimpl->error->setText(QString{});
}

//--------------------------------------------------------------------------

void PasswordPanel::setError(const QString& message)
{
    pimpl->error->setText(message);
}

//--------------------------------------------------------------------------

void PasswordPanel::setApplyButtonVisible(bool enable)
{
    pimpl->password->setApplyButtonVisible(enable);
}

//--------------------------------------------------------------------------

bool PasswordPanel::isApplyButtonVisible() const
{
    return pimpl->password->isApplyButtonVisible();
}

//--------------------------------------------------------------------------

void PasswordPanel::setApplyButtonContent(const QString& text, std::shared_ptr<SvgIcon> icon)
{
    pimpl->password->setApplyButtonContent(text,std::move(icon));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
