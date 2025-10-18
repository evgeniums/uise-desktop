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
#include <QResizeEvent>

#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/elidedlabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QFrame(parent),
      m_mode(Qt::ElideRight),
      m_ignoreSizeHint(false),
      m_maxLines(1)
{
    auto l=Layout::horizontal(this);

    m_hiddenLabel=new QLabel(this);
    l->addWidget(m_hiddenLabel);
    m_hiddenLabel->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
    m_hiddenLabel->hide();

    m_label=new QLabel(this);
    l->addWidget(m_label,1);

    m_label->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Fixed);

    setText(text);
}

//--------------------------------------------------------------------------

void ElidedLabel::setText(const QString& text)
{
    m_hiddenLabel->setText(text);
    m_label->setText(text);

    updateText(width());

    emit textUpdated();
}

//--------------------------------------------------------------------------

void ElidedLabel::setAlignment(Qt::Alignment alignment)
{
    m_hiddenLabel->setAlignment(alignment);
    m_label->setAlignment(alignment);
}

//--------------------------------------------------------------------------

Qt::Alignment ElidedLabel::alignment() const
{
    return m_label->alignment();
}

//--------------------------------------------------------------------------

QString ElidedLabel::text() const
{
    return m_hiddenLabel->text();
}

//--------------------------------------------------------------------------

QSize ElidedLabel::sizeHint() const
{
    if (m_ignoreSizeHint)
    {
        return QSize{};
    }
    auto sz=m_hiddenLabel->sizeHint();
    if (sz.isValid())
    {
        QSize{sz.width()+contentsMargins().left()+contentsMargins().right(),sz.height()};
    }
    return QSize{};
}

//--------------------------------------------------------------------------

int ElidedLabel::widthHint() const
{
    auto sz=m_hiddenLabel->sizeHint();
    if (sz.isValid())
    {
        return sz.width()+contentsMargins().left()+contentsMargins().right();
    }
    return 0;
}

//--------------------------------------------------------------------------

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    updateText(event->size().width());
    QFrame::resizeEvent(event);
}

//--------------------------------------------------------------------------

void ElidedLabel::updateText(int width)
{
    auto w=width-contentsMargins().left()-contentsMargins().right();
    if (m_maxLines>1)
    {
        w-=m_label->fontMetrics().maxWidth()*3;
    }
    auto w1=w*m_maxLines;
    QString elidedText = m_label->fontMetrics().elidedText(m_hiddenLabel->text(), m_mode, w1);
    m_label->setText(elidedText);
}

//--------------------------------------------------------------------------

void ElidedLabel::setIgnoreSizeHint(bool enable)
{
    m_ignoreSizeHint=enable;
    auto sp=sizePolicy();
    if (enable)
    {
        m_label->setSizePolicy(QSizePolicy::Ignored,sp.verticalPolicy());
    }
    else
    {
        m_label->setSizePolicy(QSizePolicy::MinimumExpanding,sp.verticalPolicy());
    }
}

//--------------------------------------------------------------------------

void ElidedLabel::setMaxLines(int maxLines)
{
    if (maxLines<=0)
    {
        maxLines=1;
    }
    m_maxLines=maxLines;
    auto wordWrap=m_maxLines>1;
    m_label->setWordWrap(wordWrap);
    m_hiddenLabel->setWordWrap(wordWrap);
    auto sp=sizePolicy();
    if (wordWrap)
    {
        m_label->setSizePolicy(sp.horizontalPolicy(),QSizePolicy::Preferred);
    }
    else
    {
        m_label->setSizePolicy(sp.horizontalPolicy(),QSizePolicy::Fixed);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
