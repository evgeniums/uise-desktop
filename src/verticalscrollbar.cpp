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

/** @file uise/desktop/scrollarea.hpp
*
*  Defines ScrollArea.
*
*/

/****************************************************************************/

#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/verticalscrollbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

VerticalScrollBar::VerticalScrollBar(QWidget* parent) : QFrame(parent)
{
    auto l=Layout::horizontal(this);
    m_bar=new QScrollBar(this);
    m_bar->setOrientation(Qt::Vertical);
    l->addWidget(m_bar);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
}

//--------------------------------------------------------------------------

QSize VerticalScrollBar::minimumSizeHint() const
{
    return m_bar->minimumSizeHint();
}

//--------------------------------------------------------------------------

QSize VerticalScrollBar::sizeHint() const
{
    return m_bar->sizeHint();
}

//--------------------------------------------------------------------------

void VerticalScrollBar::setHoldPlace(bool enable)
{
    m_holdPlace=enable;
    if (!m_visible)
    {
        QFrame::setVisible(m_holdPlace);
    }
}

//--------------------------------------------------------------------------

void VerticalScrollBar::setVisible(bool enable)
{
    m_visible=enable;
    if (!m_visible)
    {
        QFrame::setVisible(m_holdPlace);
    }
    else
    {
        QFrame::setVisible(true);
    }
    m_bar->setVisible(m_visible);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
