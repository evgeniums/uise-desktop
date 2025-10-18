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

/** @file uise/desktop/src/elidedcontainer.cpp
*
*  Defines ElidedContainer.
*
*/

/****************************************************************************/

#include <QResizeEvent>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/elidedcontainer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** ElidedContainer ****************************/

//--------------------------------------------------------------------------

ElidedContainer::ElidedContainer(QWidget* parent)
    : QFrame(parent)
{
    m_updateTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

void ElidedContainer::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    updateSize(event->size());
}

//--------------------------------------------------------------------------

void ElidedContainer::updateSize(const QSize& newSize)
{
    auto margins=contentsMargins();

    int elidedMinWidth=0;
    int nonElidedMinWidth=0;
    int elidedLabelsCount=0;
    int elidedWidthHint=0;
    for (const auto& w : m_widgets)
    {
        if (!w.widget || w.widget->isHidden())
        {
            continue;
        }

        if (w.label)
        {
            elidedLabelsCount++;
            elidedMinWidth+=w.label->minimumWidth();
            elidedWidthHint+=w.label->widthHint();
        }
        else
        {
            nonElidedMinWidth+=w.widget->sizeHint().width();
        }
    }
    auto newWidth=newSize.width();
    auto newHeight=newSize.height();
    auto elidedWidth=0;
    int averageElidedWidth=0;
    if (elidedLabelsCount!=0)
    {
        elidedWidth=newWidth-nonElidedMinWidth;
        if (elidedWidth>elidedWidthHint)
        {
            elidedWidth=elidedWidthHint;
        }
        if (elidedWidth<elidedMinWidth)
        {
            elidedWidth=elidedMinWidth;
        }
        averageElidedWidth=elidedWidth/elidedLabelsCount;
    }
    int usedWidth=0;
    auto usedCount=0;
    for (size_t i=0;i<m_widgets.size();i++)
    {
        auto& w=m_widgets.at(i);
        if (!w.label || w.widget->isHidden())
        {
            continue;
        }

        w.width=0;
        auto widgetWidth=averageElidedWidth;
        if (widgetWidth>w.label->widthHint())
        {
            widgetWidth=w.label->widthHint();
            w.width=widgetWidth;
        }
        if (widgetWidth<w.label->minimumWidth())
        {
            widgetWidth=w.label->minimumWidth();
            w.width=widgetWidth;
        }
        if (w.width!=0)
        {
            usedWidth+=w.width;
            usedCount++;
        }
    }
#if 0
    qDebug() << "newWidth="<<newWidth << " elidedWidthHint="<<elidedWidthHint<<" nonElidedMinWidth="<<nonElidedMinWidth
             << " elidedWidth="<<elidedWidth<<" averageElidedWidth="<<averageElidedWidth << " usedWidth="<<usedWidth
             << " usedCount=" << usedCount;
#endif
    if (usedCount!=0)
    {
        auto leftCount=elidedLabelsCount-usedCount;
        if (leftCount!=0)
        {
            averageElidedWidth=(newWidth-nonElidedMinWidth-usedWidth)/leftCount;
#if 0
            qDebug() << "adjusted averageElidedWidth="<<averageElidedWidth;
#endif
        }
    }

    int accElidedWidth=0;
    int pos=margins.left();
    int y=margins.top();
    for (size_t i=0;i<m_widgets.size();i++)
    {
        auto& w=m_widgets.at(i);
        if (!w.widget || w.widget->isHidden())
        {
            continue;
        }
        w.widget->move(pos,y);

        auto widgetWidth=w.widget->sizeHint().width();
        if (w.label)
        {
            if (w.width!=0)
            {
                widgetWidth=w.width;
            }
            else
            {
                widgetWidth=averageElidedWidth;
                if (widgetWidth>w.label->widthHint())
                {
                    widgetWidth=w.label->widthHint();
                }
            }
        }
#if 0
        UNCOMMENTED_QDEBUG << "Move widget " << i <<  " to pos=" << pos << " y=" << y << " width="<<widgetWidth << " height="<<newHeight
                 << " totalWidth="<<newWidth;
#endif
        pos+=widgetWidth;
        w.widget->resize(widgetWidth,newHeight);
    }
}

//--------------------------------------------------------------------------

void ElidedContainer::addWidget(QWidget* widget)
{
    widget->setParent(this);
    Widget w;
    w.widget=widget;
    onWidgetAdded(w);
}

//--------------------------------------------------------------------------

void ElidedContainer::addElidedLabel(ElidedLabel* label)
{
    label->setParent(this);
    connect(
        label,
        &ElidedLabel::textUpdated,
        this,
        &ElidedContainer::onElidedTextUpdate
    );
    Widget w;
    w.widget=label;
    w.label=label;
    onWidgetAdded(w);
}

//--------------------------------------------------------------------------

QSize ElidedContainer::sizeHint() const
{
    int width=0;
    int height=minimumHeight();
    for (const auto& w : m_widgets)
    {
        if (!w.widget)
        {
            continue;
        }

        auto sz=w.widget->sizeHint();
        if (w.label!=nullptr)
        {
            width+=w.label->widthHint();
        }
        else
        {
            width+=sz.width();
        }
        height=std::max(height,sz.height());
    }

    return QSize{width,height};
}

//--------------------------------------------------------------------------

void ElidedContainer::updateMinimumHeight()
{
    int min=0;
    for (const auto& w : m_widgets)
    {
        if (!w.widget || w.widget->isHidden())
        {
            continue;
        }
        min=std::max(min,w.widget->sizeHint().height());
    }
    setMinimumHeight(min);
}

//--------------------------------------------------------------------------

void ElidedContainer::onWidgetAdded(Widget w)
{
    m_widgets.emplace_back(w);
    w.widget->installEventFilter(this);
    updateMinimumHeight();
    refresh();
}

//--------------------------------------------------------------------------

bool ElidedContainer::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type()==QEvent::Show || event->type()==QEvent::Hide)
    {
        refresh();
    }
    return false;
}

//--------------------------------------------------------------------------

void ElidedContainer::onElidedTextUpdate()
{
    refresh();
}

//--------------------------------------------------------------------------

void ElidedContainer::refresh()
{
    m_updateTimer->shot(10,
        [this]()
        {
            updateSize(size());
        }
    );
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
