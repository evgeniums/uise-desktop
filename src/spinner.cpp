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
        m_selectionHeight(0),
        m_itemHeight(0)
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
    auto sel = selectionRect();

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    // draw background
    painter.setBrush(m_styleSample->palette().color(QPalette::Base));
    painter.drawRect(geometry());

    // draw highliting
    painter.setBrush(m_styleSample->palette().color(QPalette::Highlight));
    painter.drawRoundedRect(sel,3,3);

    // draw section items
    auto x=0;
    for (auto&& section:m_sections)
    {
        int topItemIndex=0;
        int y=0;
        int lastItemOffset=0;

        if (section->circular)
        {
            auto b = qFloor(section->currentOffset/m_itemHeight)%section->items.size();
            topItemIndex=(section->items.size()-b)%section->items.size();

            auto delta=section->currentOffset%m_itemHeight;
            if (delta>0)
            {
                topItemIndex-=1;
                if (topItemIndex<0)
                {
                    topItemIndex=section->items.size()-1;
                }
                delta=delta-m_itemHeight;
            }
            y+=delta;
        }
        else
        {
            y=section->currentOffset%h;
        }

        section->currentItemIndex=-1;
        section->currentItemPosition=-1;
        auto renderItems=[this,h,section,&x,&y,&painter,&lastItemOffset](int from, int to)
        {
            for (int i=from;i<to;i++)
            {
                if (y>(h/2-m_selectionHeight/2) && y<(h/2+m_selectionHeight/2))
                {
                    section->currentItemIndex=i;
                    section->currentItemPosition=y;
                }

                lastItemOffset=y;
                section->items[i]->render(&painter,QPoint(x,y));
                y+=m_itemHeight;

                if (y>h)
                {
                    break;
                }
            }
        };

        renderItems(topItemIndex,section->items.size());
        renderItems(0,topItemIndex-1);

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
    imagePainter.drawRoundedRect(sel,3,3);

    // draw mask
    painter.drawPixmap(0,0,maskPixmap);
}

//--------------------------------------------------------------------------
QSize Spinner::sizeHint() const
{
    return size();
}

//--------------------------------------------------------------------------
QRect Spinner::selectionRect() const
{
    return QRect(5,height()/2-m_selectionHeight/2,width()-10,m_selectionHeight);
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
    m_keyPressed=true;
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
void Spinner::keyReleaseEvent(QKeyEvent *)
{
    m_keyPressed=false;
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
    if (!section->circular)
    {
        auto itemsHeight=section->items.size()*m_itemHeight;
        if (pos<0)
        {
            // down

            auto edge=itemsHeight-(height()+m_selectionHeight)/2;
            if (qAbs(pos)>edge)
            {
                pos=-edge;
            }
        }
        else
        {
            // up

            auto edge=height()/2 - m_itemHeight/2;
            if (pos>edge)
            {
                pos=edge;
            }
        }
    }

    if (section->currentOffset==pos)
    {
        return;
    }

    section->currentOffset=pos;
    repaint();

    adjustPosition(section);
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

//--------------------------------------------------------------------------
void Spinner::setSections(std::vector<std::shared_ptr<SpinnerSection>> sections)
{
    m_sections=std::move(sections);
    for (auto&& section:m_sections)
    {
        section->adjustTimer=new SingleShotTimer(this);
        section->animation=new QVariantAnimation(section->adjustTimer);
        section->animation->setDuration(300);
        section->animation->setEasingCurve(QEasingCurve::OutQuad);

        adjustPosition(section.get(),false,true);
    }    
}

//--------------------------------------------------------------------------
int Spinner::itemOffset(SpinnerSection* section, int index) const
{
    auto offs=index * m_itemHeight;
    return offs;
}

//--------------------------------------------------------------------------
void Spinner::scrollToItem(SpinnerSection *section, int index)
{
    auto offs = itemOffset(section,index);
    scrollTo(section,offs);
}

//--------------------------------------------------------------------------
void Spinner::adjustPosition(SpinnerSection *section, bool animate, bool noDelay)
{
    if (section->currentItemPosition<0)
    {
        return;
    }

    int delay = noDelay ? 50 : 100;
    section->animation->stop();
    section->adjustTimer->clear();
    section->adjustTimer->shot(delay,[section,animate,this](){

        if (section->currentItemPosition<0)
        {
            return;
        }

        section->animation->stop();
        if (m_mousePressed || m_keyPressed)
        {
            adjustPosition(section);
            return;
        }

        auto pos=section->currentItemPosition%height();
        auto sel=selectionRect();
        auto offset=pos-sel.top();
        if (offset==0)
        {
            return;
        }

        // jump to next item if scroll is above/below half height of the item
        if (qAbs(offset)>m_itemHeight/2)
        {
            if (offset<0)
            {
                // down

                // enable for non circtular section or not last item
                if (!section->circular || section->currentItemIndex<(section->items.size()-1))
                {
                    offset+=m_itemHeight;
                }
            }
            else
            {
                // up

                // enable for non circtular section or not first item
                if (!section->circular || section->currentItemIndex>0)
                {
                    offset-=m_itemHeight;
                }
            }
        }

        int endVal=0;
        bool asc=offset>0;
        if (asc)
        {
            endVal=offset;
        }
        else
        {
            endVal=-offset;
        }

        if (animate)
        {
            section->animationVal=0;
            section->animation->setStartValue(0);
            section->animation->setEndValue(endVal);
            section->animation->disconnect(this);
            connect(section->animation,&QVariantAnimation::valueChanged,this,[this,section,asc](const QVariant& val){

                if (m_mousePressed || m_keyPressed)
                {
                    adjustPosition(section);
                    return;
                }

                int rm = asc?val.toInt():-val.toInt();
                auto offs = section->animationVal - rm;
                section->animationVal=rm;

                section->currentOffset+=offs;
                repaint();
            });
            section->animation->start();
        }
        else
        {
            section->currentOffset-=offset;
            repaint();
        }
    });
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
