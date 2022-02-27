/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/src/spinner.cpp
*
*  Defines Spinner.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QResizeEvent>
#include <QPainter>
#include <QColor>
#include <QEnterEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/spinner.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
Spinner::Spinner(
        QWidget* parent
    ) : QFrame(parent),
        m_styleSample(nullptr),
        m_singleScrollStep(DefaultSingleScrollStep),
        m_pageScrollStep(DefaultPageScrollStep),
        m_wheelOffsetAccumulated(0.0),
        m_mousePressed(false),
        m_selectionHeight(0)
{
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    setMouseTracking(true);
}

//--------------------------------------------------------------------------
void Spinner::setStyleSample(QWidget *widget)
{
    m_styleSample=widget;
}

//--------------------------------------------------------------------------
void Spinner::paintEvent(QPaintEvent *event)
{
    auto h = height();
    auto w = width();

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    // draw background
    painter.setBrush(m_styleSample->palette().color(QPalette::Base));
    painter.drawRect(geometry());

    // draw highliting
    painter.setBrush(m_styleSample->palette().color(QPalette::Highlight));
    painter.drawRoundedRect(5,h/2-m_selectionHeight/2,w-10,m_selectionHeight,3,3);

    // draw section items
    auto x=0;
    for (auto&& section:m_sections)
    {
        int y=section->currentOffset%h;

        if (section->circular && y>0)
        {
            if (y>0)
            {
                auto itemsCount=qCeil(double(y)/double(section->itemHeight));
                if (itemsCount<section->items.size())
                {
                    int yy=y-itemsCount*section->itemHeight;
                    for (auto i=section->items.size()-itemsCount;i<section->items.size();i++)
                    {
                        section->items[i]->render(&painter,QPoint(x,yy));
                        yy+=section->itemHeight;

                        if (yy>=h)
                        {
                            break;
                        }
                    }
                }
            }
        }

        for (auto&& item:section->items)
        {
            item->render(&painter,QPoint(x,y));
            y+=section->itemHeight;
            if (y>h)
            {
                break;
            }
        }

        if (section->circular &&  y<h)
        {
            auto hy = h-y;
            auto itemsCount=qCeil(double(hy)/double(section->itemHeight));
            if (itemsCount<section->items.size())
            {
                int yy=y;
                for (auto i=0;i<itemsCount;i++)
                {
                    section->items[i]->render(&painter,QPoint(x,yy));
                    yy+=section->itemHeight;

                    if (yy>h)
                    {
                        break;
                    }
                }
            }
        }

        x+=section->width();
    }

    //  construct gradient mask with highliter hole
    auto maskPixmap = QPixmap(w,h);
    maskPixmap.fill(Qt::transparent);
    QPainter imagePainter;
    imagePainter.begin(&maskPixmap);
    imagePainter.setPen(Qt::NoPen);
    // gradient
    QLinearGradient gr(w, 0, w, h);
    auto c = m_styleSample->palette().color(QPalette::Base);
    auto mc =c ;
    mc.setAlpha(64);
    gr.setColorAt(0.0, c);
    gr.setColorAt(0.5, mc);
    gr.setColorAt(1.0, c);
    imagePainter.setRenderHint(QPainter::Antialiasing);
    imagePainter.setBrush(gr);
    imagePainter.drawRect(0, 0, w, h);
    // hole
    imagePainter.setBrush(Qt::transparent);
    imagePainter.setOpacity(1.0);
    imagePainter.setCompositionMode(QPainter::CompositionMode_Source);
    imagePainter.drawRoundedRect(5,h/2-m_selectionHeight/2,w-10,m_selectionHeight,3,3);

    // draw mask
    painter.drawPixmap(0,0,maskPixmap);
}

//--------------------------------------------------------------------------
QSize Spinner::sizeHint() const
{
    return size();
}

//--------------------------------------------------------------------------
std::shared_ptr<SpinnerSection> Spinner::sectionUnderCursor() const
{
    auto pos = QCursor::pos();
    int x=mapToGlobal(QPoint(0,0)).x();

    for (auto&& sec:m_sections)
    {
        auto nextX = x + sec->width();
        if (pos.x()>=x && pos.x()<nextX)
        {
            return sec;
        }
        x=nextX;
    }

    return std::shared_ptr<SpinnerSection>();
}

//--------------------------------------------------------------------------
void Spinner::keyPressEvent(QKeyEvent *event)
{
    auto section=m_sectionUnderCursor;
    if (!section)
    {
        return;
    }

    if (event->key()==Qt::Key_Up)
    {
        scroll(section.get(),m_singleScrollStep);
    }
    else if (event->key()==Qt::Key_Down)
    {
        scroll(section.get(),-m_singleScrollStep);
    }
    else if (event->key()==Qt::Key_PageUp)
    {
        scroll(section.get(),m_pageScrollStep);
    }
    else if (event->key()==Qt::Key_PageDown)
    {
        scroll(section.get(),-m_pageScrollStep);
    }

    QFrame::keyPressEvent(event);
}

//--------------------------------------------------------------------------
void Spinner::wheelEvent(QWheelEvent *event)
{
    auto section=m_sectionUnderCursor;
    if (!section)
    {
        return;
    }

    auto numPixels = event->pixelDelta();
    auto angleDelta = event->angleDelta();

    int scrollDelta=0;

#ifndef Q_WS_X11 // Qt documentation says that on X11 pixelDelta() is unreliable and should not be used
   if (!numPixels.isNull())
   {
       scrollDelta=numPixels.y();
   }
   else if (!angleDelta.isNull())
#endif
   {
       auto evalOffset=[this,&angleDelta](float& accumulated)
       {
           auto deltaPos=static_cast<qreal>(angleDelta.y());
           auto scrollLines=QApplication::wheelScrollLines();
           auto numStepsU = m_singleScrollStep * scrollLines * deltaPos / 120;
           if (qAbs(accumulated)>std::numeric_limits<float>::epsilon()
               &&
               (numStepsU/accumulated)<0
               )
           {
               accumulated=0.0f;
           }
           accumulated+=numStepsU;
           auto numSteps=static_cast<int>(accumulated);
           accumulated-=numSteps;

           return numSteps;
       };

       scrollDelta=evalOffset(m_wheelOffsetAccumulated);
   }

   scroll(section.get(),-scrollDelta);
   event->accept();
}

//--------------------------------------------------------------------------
void Spinner::scroll(SpinnerSection* section, int delta)
{
    auto pos=section->currentOffset-delta;
    scrollTo(section,pos);
}

//--------------------------------------------------------------------------
void Spinner::scrollTo(SpinnerSection* section, int pos)
{
    section->currentOffset=pos;
    repaint();

    //! @todo process current index
}

//--------------------------------------------------------------------------
void Spinner::leaveEvent(QEvent *)
{
    m_sectionUnderCursor.reset();
    m_mousePressed=false;
}

//--------------------------------------------------------------------------
void Spinner::enterEvent(QEnterEvent *event)
{
    m_sectionUnderCursor=sectionUnderCursor();
    m_wheelOffsetAccumulated=0;
}

//--------------------------------------------------------------------------
void Spinner::mousePressEvent(QMouseEvent *event)
{
    if (!m_sectionUnderCursor)
    {
        return;
    }

    m_mousePressed=true;
    m_lastMousePos.setY(event->position().y());
    QCursor cursor;
    cursor.setShape(Qt::OpenHandCursor);
    setCursor(cursor);
}

//--------------------------------------------------------------------------
void Spinner::mouseReleaseEvent(QMouseEvent *event)
{
    m_mousePressed=false;

    QCursor cursor;
    cursor.setShape(Qt::ArrowCursor);
    setCursor(cursor);
}

//--------------------------------------------------------------------------
void Spinner::mouseMoveEvent(QMouseEvent *event)
{
    auto section=sectionUnderCursor();
    if (m_sectionUnderCursor!=section)
    {
        m_wheelOffsetAccumulated=0;
    }
    m_sectionUnderCursor=section;

    if (!section)
    {
        return;
    }

    if (!m_mousePressed)
    {
        return;
    }

    auto y=event->position().y();
    scroll(section.get(),m_lastMousePos.y()-y);
    m_lastMousePos.setY(y);
}

void Spinner::setSections(std::vector<std::shared_ptr<SpinnerSection>> sections)
{
    m_sections=std::move(sections);
    auto h = 0;
    for (auto&& section:m_sections)
    {
        for (auto&& item:section->items)
        {
            section->itemHeight=item->sizeHint().height()*2;
            if (h<section->itemHeight)
            {
                h=section->itemHeight;
            }
        }
    }
    m_selectionHeight=h;
}


//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
