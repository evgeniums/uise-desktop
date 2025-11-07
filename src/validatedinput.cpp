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

/** @file uise/desktop/src/validatedinput.cpp
*
*  Defines PassowrdInput.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/validatedinput.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** ValidatedInput ***********************************/

//--------------------------------------------------------------------------

ValidatedInput::ValidatedInput(QWidget* parent)
    : AbstractValidatedInput(parent)
{
    auto l=Layout::horizontal(this);

    m_input=new QLineEdit(this);
    l->addWidget(m_input,1);

    m_button=new PushButton(tr("Apply"),this);
    l->addWidget(m_button);

    connect(
        m_button,
        &PushButton::clicked,
        this,
        &ValidatedInput::tryApply
        );
    connect(
        m_input,
        &QLineEdit::returnPressed,
        this,
        &ValidatedInput::tryApply
        );
    connect(
        m_input,
        &QLineEdit::textChanged,
        this,
        &ValidatedInput::onTextChanged
    );

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    m_input->setClearButtonEnabled(true);
}

//--------------------------------------------------------------------------

void ValidatedInput::clear()
{
    m_input->clear();
}

//--------------------------------------------------------------------------

void ValidatedInput::onTextChanged(QString text)
{
    int pos=0;
    auto state=m_input->validator()->validate(text,pos);
    switch (state)
    {
    case QValidator::Invalid: m_input->setProperty("state","invalid"); break;
    case QValidator::Intermediate: m_input->setProperty("state","intermediate"); break;
    case QValidator::Acceptable: m_input->setProperty("state","acceptable"); break;
    }
    Style::updateWidgetStyle(m_input);
}

//--------------------------------------------------------------------------

void ValidatedInput::tryApply()
{
    int pos=0;
    auto text=m_input->text();
    auto state=m_input->validator()->validate(text,pos);
    if (state==QValidator::Acceptable)
    {
        emit applyRequested();
    }
}

//--------------------------------------------------------------------------

void ValidatedInput::setText(const QString& text)
{
    m_input->setText(text);
}

//--------------------------------------------------------------------------

std::optional<QString> ValidatedInput::text() const
{
    auto text=m_input->text();
    int pos=0;
    auto state=m_input->validator()->validate(text,pos);
    if (state==QValidator::Acceptable)
    {
        return text;
    }
    return std::optional<QString>{};
}

//--------------------------------------------------------------------------

void ValidatedInput::setApplyButtonText(const QString& text)
{
    m_button->setText(text);
}

//--------------------------------------------------------------------------

QString ValidatedInput::applyButtonText() const
{
    return m_button->text();
}

//--------------------------------------------------------------------------

void ValidatedInput::setPlaceholderText(const QString& text)
{
    m_input->setPlaceholderText(text);
}

//--------------------------------------------------------------------------

QString ValidatedInput::placeholderText() const
{
    return m_input->placeholderText();
}

//--------------------------------------------------------------------------

QWidget* ValidatedInput::editorWidget() const
{
    return m_input;
}

//--------------------------------------------------------------------------

void ValidatedInput::setApplyButtonVisible(bool enable)
{
    m_button->setVisible(enable);
}

//--------------------------------------------------------------------------

bool ValidatedInput::isApplyButtonVisible() const
{
    return m_button->isVisible();
}

//--------------------------------------------------------------------------

void ValidatedInput::updateValidator()
{
    m_input->setValidator(validator());
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
