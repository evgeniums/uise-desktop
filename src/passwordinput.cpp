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

/** @file uise/desktop/src/passwordinput.cpp
*
*  Defines PassowrdInput.
*
*/

/****************************************************************************/

#include <QLabel>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/passwordinput.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** PasswordInput ***********************************/

class PasswordInput_p
{
    public:

        QLabel* title;
        QLineEdit* editor;
        PushButton* unmaskButton;
};

//--------------------------------------------------------------------------

PasswordInput::PasswordInput(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<PasswordInput_p>())
{
    auto mainL=Layout::vertical(this);
    pimpl->title=new QLabel(this);
    pimpl->title->setObjectName("title");
    mainL->addWidget(pimpl->title);

    auto hFrame=new QFrame(this);
    hFrame->setObjectName("hFrame");
    auto l=Layout::horizontal(hFrame);
    mainL->addWidget(hFrame);

    pimpl->editor=new QLineEdit(this);
    l->addWidget(pimpl->editor,1);

    pimpl->unmaskButton=new PushButton(Style::instance().svgIconLocator().icon("PasswordInput::unmask",this),this);
    pimpl->unmaskButton->setCheckable(true);
    l->addWidget(pimpl->unmaskButton);

    auto setPasswordMode=[this]()
    {
        pimpl->unmaskButton->setToolTip(tr("Show symbols"));
        pimpl->editor->setEchoMode(QLineEdit::EchoMode::Password);
    };
    setPasswordMode();

    connect(
        pimpl->unmaskButton,
        &PushButton::toggled,
        this,
        [this,setPasswordMode](bool checked)
        {
            if (checked)
            {
                pimpl->editor->setEchoMode(QLineEdit::EchoMode::Normal);
                pimpl->unmaskButton->setToolTip(tr("Hide symbols"));
            }
            else
            {
                setPasswordMode();
            }
            emit unmaskButtonToggled(checked);
        }
    );

    connect(pimpl->editor,&QLineEdit::returnPressed,this,&PasswordInput::returnPressed);
    pimpl->editor->setClearButtonEnabled(true);

    pimpl->title->setVisible(true);
}

//--------------------------------------------------------------------------

PasswordInput::~PasswordInput()
{}

//--------------------------------------------------------------------------

void PasswordInput::setUnmaskButtonChecked(bool enable)
{
    pimpl->unmaskButton->setChecked(enable);
}

//--------------------------------------------------------------------------

bool PasswordInput::isUnmaskButtonChecked() const
{
    return pimpl->unmaskButton->isChecked();
}

//--------------------------------------------------------------------------

QLineEdit* PasswordInput::editor() const
{
    return pimpl->editor;
}

//--------------------------------------------------------------------------

void PasswordInput::setUnmaskButtonVisible(bool enable)
{
    pimpl->unmaskButton->setVisible(enable);
}

//--------------------------------------------------------------------------

bool PasswordInput::isUnmaskButtonVisible() const
{
    return pimpl->unmaskButton->isVisible();
}

//--------------------------------------------------------------------------

void PasswordInput::setTitle(const QString& title)
{
    pimpl->title->setText(title);
}

//--------------------------------------------------------------------------

QString PasswordInput::title() const
{
    return pimpl->title->text();
}

//--------------------------------------------------------------------------

void PasswordInput::setTitleVisible(bool enable)
{
    pimpl->title->setVisible(enable);
}

//--------------------------------------------------------------------------

bool PasswordInput::isTitleVisible() const
{
    return pimpl->title->isVisible();
}

//--------------------------------------------------------------------------

void PasswordInput::setClearButtonEnabled(bool enable)
{
    pimpl->editor->setClearButtonEnabled(enable);
}

//--------------------------------------------------------------------------

bool PasswordInput::isClearButtonEnabled() const
{
    return pimpl->editor->isClearButtonEnabled();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
