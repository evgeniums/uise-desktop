/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/src/alignedstretchingwidget.cpp
*
*  Defines AlignedStretchingWidget.
*
*/

/****************************************************************************/

#include <QVariant>
#include <QResizeEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/alignedstretchingwidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
AlignedStretchingWidget::AlignedStretchingWidget(QWidget *parent)
    : m_widget(parent),
      m_orientation(Qt::Vertical),
      m_alignment(Qt::Alignment())
{
}

//--------------------------------------------------------------------------
QWidget* AlignedStretchingWidget::takeWidget()
{
    if (m_widget)
    {
        m_widget->setProperty("AlignedStretchingWidget",QVariant());
    }

    m_alignment=Qt::Alignment();
    return m_widget.data();
}

//--------------------------------------------------------------------------
void AlignedStretchingWidget::setWidget(
        QWidget *widget,
        Qt::Orientation orientation,
        Qt::Alignment alignment
    )
{
    if (m_widget)
    {
        destroyWidget(m_widget);
    }
    if (!widget)
    {
        return;
    }

    m_orientation=orientation;
    m_alignment=alignment;

    m_widget=widget;
    m_widget->setParent(this);
    m_widget->setProperty("AlignedStretchingWidget",true);

    m_widget->installEventFilter(this);

    updateMinMaxSize();
}

//--------------------------------------------------------------------------
void AlignedStretchingWidget::updateMinMaxSize()
{
    auto margins=contentsMargins();

    auto width=m_widget->minimumWidth() + margins.left() + margins.right();
    auto height=m_widget->minimumHeight() + margins.top() + margins.bottom();

    setMinimumWidth(width);
    setMinimumHeight(height);

    updateSize();
}

//--------------------------------------------------------------------------
void AlignedStretchingWidget::updateSize()
{
    if (!m_widget)
    {
        return;
    }

    auto margins=contentsMargins();

    auto targetSize=oprop(this,OProp::size);
    targetSize-=isHorizontal()?(margins.left() + margins.right()):(margins.top() + margins.bottom());
    auto minSize=oprop(m_widget.data(),OProp::min_size);
    auto maxSize=oprop(m_widget.data(),OProp::max_size);

    targetSize=qBound(minSize,targetSize,maxSize);
    auto targetSizeDiff=oprop(this,OProp::size)-targetSize;

    auto minOtherSize=oprop(m_widget.data(),OProp::min_size,true);
    auto maxOtherSize=oprop(m_widget.data(),OProp::max_size,true);
    auto otherSize=oprop(this,OProp::size,true);
    otherSize-=!isHorizontal()?(margins.left() + margins.right()):(margins.top() + margins.bottom());
    otherSize=qBound(minOtherSize,otherSize,maxOtherSize);
    auto otherSizeDiff=oprop(this,OProp::size,true)-otherSize;

    QSize widgetSize;
    setOProp(widgetSize,OProp::size,targetSize);
    setOProp(widgetSize,OProp::size,otherSize,true);
    m_widget->resize(widgetSize);

    QPoint pos;
    bool move=false;
    if (m_orientation==Qt::Horizontal)
    {
        if (m_alignment & Qt::AlignHCenter)
        {
            setOProp(pos,OProp::pos,margins.left() + (width()-margins.left()-margins.right()-targetSize)/2);
            move=true;
        }
        else if (m_alignment & Qt::AlignLeft)
        {
            setOProp(pos,OProp::pos,margins.left());
            move=true;
        }
        else if (m_alignment & Qt::AlignRight)
        {
            setOProp(pos,OProp::pos,targetSizeDiff - margins.right());
            move=true;
        }

        if (m_alignment & Qt::AlignVCenter || (m_alignment & Qt::AlignVertical_Mask)==0)
        {
            setOProp(pos,OProp::pos,margins.top() + (height()-margins.top()-margins.bottom()-otherSize)/2,true);
            move=true;
        }
        else if (m_alignment & Qt::AlignTop)
        {
            setOProp(pos,OProp::pos,margins.top(),true);
            move=true;
        }
        else if (m_alignment & Qt::AlignBottom)
        {
            setOProp(pos,OProp::pos,otherSizeDiff - margins.bottom(),true);
            move=true;
        }
    }
    else
    {
        if (m_alignment & Qt::AlignVCenter)
        {
            setOProp(pos,OProp::pos,margins.top() + (height()-margins.top()-margins.bottom()-targetSize)/2);
            move=true;
        }
        else if (m_alignment & Qt::AlignTop)
        {
            setOProp(pos,OProp::pos,margins.top());
            move=true;
        }
        else if (m_alignment & Qt::AlignBottom)
        {
            setOProp(pos,OProp::pos,targetSizeDiff - margins.bottom());
            move=true;
        }

        if (m_alignment & Qt::AlignHCenter || (m_alignment & Qt::AlignHorizontal_Mask)==0)
        {
            setOProp(pos,OProp::pos,margins.left() + (width()-margins.left()-margins.right()-otherSize)/2,true);
            move=true;
        }
        else if (m_alignment & Qt::AlignLeft)
        {
            setOProp(pos,OProp::pos,margins.left(),true);
            move=true;
        }
        else if (m_alignment & Qt::AlignRight)
        {
            setOProp(pos,OProp::pos,otherSizeDiff - margins.right(),true);
            move=true;
        }
    }
    if (move)
    {
        m_widget->move(pos);
    }

    // after move there can be artefacts, repaint widget to get rid of them
    update();
}

//--------------------------------------------------------------------------
void AlignedStretchingWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateSize();
}

//--------------------------------------------------------------------------
bool AlignedStretchingWidget::isHorizontal() const noexcept
{
    return m_orientation==Qt::Horizontal;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
