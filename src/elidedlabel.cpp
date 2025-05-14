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

/** @file uise/desktop/elidedlabel.cpp
*
*  Defines ElidedLabel
*
*/

/****************************************************************************/

#include <QLabel>

#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/elidedlabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QFrame(parent),
      m_mode(Qt::ElideRight)
{
    auto l=Layout::horizontal(this);

    m_hiddenLabel=new QLabel(this);
    l->addWidget(m_hiddenLabel);
    m_hiddenLabel->hide();

    m_label=new QLabel(this);
    l->addWidget(m_label);

    m_label->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Fixed);

    setText(text);
}

void ElidedLabel::setText(const QString& text)
{
    m_hiddenLabel->setText(text);
    m_label->setText(text);

    updateText();
}

QString ElidedLabel::text() const
{
    return m_hiddenLabel->text();
}

QSize ElidedLabel::sizeHint() const
{
    return m_hiddenLabel->sizeHint();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    updateText();
    QFrame::resizeEvent(event);
}

void ElidedLabel::updateText()
{
    QString elidedText = fontMetrics().elidedText(m_hiddenLabel->text(), m_mode, width());
    m_label->setText(elidedText);
}

UISE_DESKTOP_NAMESPACE_END
