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

#include <iostream>
#include <stdexcept>

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QResizeEvent>
#include <QPainter>
#include <QColor>
#include <QEnterEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/spinner.hpp>

#include <uise/desktop/spinnersection.hpp>
#include <uise/desktop/detail/spinnersection_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Spinner_p
{
    public:

        Spinner_p() :
            styleSample(nullptr),
            singleScrollStep(Spinner::DefaultSingleScrollStep),
            pageScrollStep(Spinner::DefaultPageScrollStep),
            mousePressed(false),
            keyPressed(false),
            selectionHeight(0),
            itemHeight(0)
        {}

        QWidget* styleSample;
        std::vector<std::shared_ptr<SpinnerSection>> sections;

        int singleScrollStep;
        int pageScrollStep;

        QPoint lastMousePos;
        bool mousePressed;
        bool keyPressed;
        std::shared_ptr<SpinnerSection> sectionUnderCursor;
        int selectionHeight;
        int itemHeight;
};

//--------------------------------------------------------------------------
Spinner::Spinner(
        QWidget* parent
    ) : QFrame(parent),
        WheelEventHandler(WheelScrollStep),
        pimpl(std::make_unique<Spinner_p>())
{
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    setMouseTracking(true);
}

//--------------------------------------------------------------------------
Spinner::~Spinner()
{}

//--------------------------------------------------------------------------
void Spinner::setStyleSample(QWidget *widget)
{
    pimpl->styleSample=widget;
    pimpl->styleSample->setParent(this);
    pimpl->styleSample->setVisible(false);
    pimpl->styleSample->setProperty("style-sample",true);
}

//--------------------------------------------------------------------------
void Spinner::paintEvent(QPaintEvent *event)
{
    auto w = width();
    auto widgetHeight=height();
    auto h=widgetHeight;
    auto sel = selectionRect();

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    // draw background
    painter.setBrush(pimpl->styleSample->palette().color(QPalette::Base));
    painter.drawRect(geometry());

    // draw highliting
    painter.setBrush(pimpl->styleSample->palette().color(QPalette::Highlight));
    painter.drawRoundedRect(sel,3,3);

    // draw sections
    auto x=sel.left();
    for (auto&& section:pimpl->sections)
    {
        // render left bar label
        if (section->pimpl->leftBarLabel!=nullptr)
        {
            auto labelY = (widgetHeight-section->pimpl->leftBarLabel->height())/2;
            section->pimpl->leftBarLabel->render(&painter,QPoint(x,labelY));
        }
        x+=section->pimpl->leftBarWidth;

        // calculate items positions
        auto [topItemIndex,offset,y,h] = calcTopItem(section.get());
        auto sel = selectionRect(h,offset);

        // render items
        auto renderItems=[this,h,section,&x,&y,&painter,&sel,&offset](int from, int to)
        {
            for (int i=from;i<to;i++)
            {
                section->pimpl->items[i]->render(&painter,QPoint(x,y));
                y+=pimpl->itemHeight;

                if (y>(offset+h))
                {
                    break;
                }
            }
        };
        renderItems(topItemIndex,section->pimpl->items.size());
        if (section->pimpl->circular)
        {
            while (y<h)
            {
                renderItems(0,section->pimpl->items.size());
            }
        }
        x+=section->pimpl->itemsWidth;

        // render right bar label
        if (section->pimpl->rightBarLabel!=nullptr)
        {
            auto labelY = (widgetHeight-section->pimpl->rightBarLabel->height())/2;
            section->pimpl->rightBarLabel->render(&painter,QPoint(x,labelY));
        }
        x+=section->pimpl->rightBarWidth;
    }

    //  construct gradient mask with highlighter hole
    auto maskPixmap = QPixmap(w,h);
    maskPixmap.fill(Qt::transparent);
    QPainter imagePainter;
    imagePainter.begin(&maskPixmap);
    imagePainter.setPen(Qt::NoPen);
    // gradient
    QLinearGradient gr(w, 0, w, h);
    auto c = pimpl->styleSample->palette().color(QPalette::Base);
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
    return selectionRect(height(),0);
}

//--------------------------------------------------------------------------
QRect Spinner::selectionRect(SpinnerSection* section) const
{
    auto height=sectionHeight(section);
    auto offset=sectionOffset(section);
    return selectionRect(height,offset);
}

//--------------------------------------------------------------------------
QRect Spinner::selectionRect(int height, int offset) const
{
    return QRect(5,offset+height/2-pimpl->selectionHeight/2,width()-10,pimpl->selectionHeight);
}

//--------------------------------------------------------------------------
std::shared_ptr<SpinnerSection> Spinner::sectionUnderCursor() const
{
    auto pos = QCursor::pos();
    int x=mapToGlobal(QPoint(0,0)).x();

    for (auto&& sec:pimpl->sections)
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
    pimpl->keyPressed=true;
    auto section=pimpl->sectionUnderCursor;
    if (!section)
    {
        return;
    }

    if (event->key()==Qt::Key_Up)
    {
        scroll(section.get(),-pimpl->singleScrollStep);
    }
    else if (event->key()==Qt::Key_Down)
    {
        scroll(section.get(),pimpl->singleScrollStep);
    }
    else if (event->key()==Qt::Key_PageUp)
    {
        scroll(section.get(),-pimpl->pageScrollStep);
    }
    else if (event->key()==Qt::Key_PageDown)
    {
        scroll(section.get(),pimpl->pageScrollStep);
    }

    QFrame::keyPressEvent(event);
}

//--------------------------------------------------------------------------
void Spinner::keyReleaseEvent(QKeyEvent *)
{
    pimpl->keyPressed=false;
}

//--------------------------------------------------------------------------
void Spinner::wheelEvent(QWheelEvent *event)
{
    auto section=pimpl->sectionUnderCursor;
    if (!section)
    {
        return;
    }

   scroll(section.get(),-handleWheelEvent(event));
   event->accept();
}

//--------------------------------------------------------------------------
void Spinner::scroll(SpinnerSection* section, int delta)
{
    auto pos=section->pimpl->currentOffset-delta;
    scrollTo(section,pos);
}

//--------------------------------------------------------------------------
void Spinner::scrollTo(SpinnerSection* section, int pos)
{
    auto h=sectionHeight(section);
    if (!section->pimpl->circular)
    {
        auto itemsHeight=section->pimpl->items.size()*pimpl->itemHeight;
        if (pos<0)
        {
            // down

            auto edge=itemsHeight-(h+pimpl->itemHeight)/2;
            if (qAbs(pos)>edge)
            {
                pos=-edge;
            }
        }
        else
        {
            // up

            auto edge=h/2 - pimpl->itemHeight/2;
            if (pos>edge)
            {
                pos=edge;
            }
        }
    }

    if (section->pimpl->currentOffset==pos)
    {
        return;
    }

    section->pimpl->currentOffset=pos;

    updateCurrentIndex(section);
    repaint();

    adjustPosition(section);
}

//--------------------------------------------------------------------------
void Spinner::leaveEvent(QEvent *)
{
    pimpl->sectionUnderCursor.reset();
    pimpl->mousePressed=false;
}

//--------------------------------------------------------------------------
void Spinner::enterEvent(QEnterEvent *event)
{
    pimpl->sectionUnderCursor=sectionUnderCursor();
    resetWheel();
}

//--------------------------------------------------------------------------
void Spinner::mousePressEvent(QMouseEvent *event)
{
    if (!pimpl->sectionUnderCursor)
    {
        return;
    }

    pimpl->mousePressed=true;
    pimpl->lastMousePos.setY(event->position().y());
    QCursor cursor;
    cursor.setShape(Qt::OpenHandCursor);
    setCursor(cursor);
}

//--------------------------------------------------------------------------
void Spinner::mouseReleaseEvent(QMouseEvent *event)
{
    pimpl->mousePressed=false;

    QCursor cursor;
    cursor.setShape(Qt::ArrowCursor);
    setCursor(cursor);
}

//--------------------------------------------------------------------------
void Spinner::mouseMoveEvent(QMouseEvent *event)
{
    auto section=sectionUnderCursor();
    if (pimpl->sectionUnderCursor!=section)
    {
        resetWheel();
    }
    pimpl->sectionUnderCursor=section;

    if (!section)
    {
        return;
    }

    if (!pimpl->mousePressed)
    {
        return;
    }

    auto y=event->position().y();
    scroll(section.get(),pimpl->lastMousePos.y()-y);
    pimpl->lastMousePos.setY(y);
}

//--------------------------------------------------------------------------
void Spinner::setSections(std::vector<std::shared_ptr<SpinnerSection>> sections)
{
    pimpl->sections=std::move(sections);
    int i=0;
    for (auto&& section:pimpl->sections)
    {
        section->pimpl->index=i;

        section->pimpl->adjustTimer=new SingleShotTimer(this);
        section->pimpl->selectionTimer=new SingleShotTimer(this);
        section->pimpl->notifyTimer=new SingleShotTimer(this);
        section->pimpl->animation=new QVariantAnimation(section->pimpl->adjustTimer);
        section->pimpl->animation->setDuration(300);
        section->pimpl->animation->setEasingCurve(QEasingCurve::OutQuad);

        for (auto&& item:section->pimpl->items)
        {
            item->setParent(this);
            item->setVisible(false);
            if (section->pimpl->leftBarLabel)
            {
                section->pimpl->leftBarLabel->setParent(this);
                section->pimpl->leftBarLabel->setVisible(false);
            }
            if (section->pimpl->rightBarLabel)
            {
                section->pimpl->rightBarLabel->setParent(this);
                section->pimpl->rightBarLabel->setVisible(false);
            }
        }

        selectItem(section.get(),0);

        ++i;
    }
}

//--------------------------------------------------------------------------
std::shared_ptr<SpinnerSection> Spinner::section(int index) const
{
    return pimpl->sections.at(index);
}

//--------------------------------------------------------------------------
size_t Spinner::sectionCount() const noexcept
{
    return pimpl->sections.size();
}

//--------------------------------------------------------------------------
void Spinner::selectItem(SpinnerSection *section, int index)
{
    if (index>=section->pimpl->items.size())
    {
        throw std::out_of_range("Index is out of range");
    }

    auto idx=index;
    if (idx>=section->pimpl->items.size())
    {
        idx=section->pimpl->items.size()-1;
    }

    if (section->pimpl->firstIndexUpdating)
    {
        section->pimpl->currentItemIndex=idx;
        section->pimpl->selectionTimer->shot(
            0,
            [section,this]()
            {
                updateCurrentIndex(section);
            }
        );
        return;
    }

    auto delta=idx-section->pimpl->currentItemIndex;
    auto offset=delta*pimpl->itemHeight;
    auto pos=section->pimpl->currentOffset-offset;
    scrollTo(section,pos);
}

//--------------------------------------------------------------------------
void Spinner::adjustPosition(SpinnerSection *section, bool animate, bool noDelay)
{
    if (section->pimpl->currentItemPosition<0)
    {
        return;
    }

    auto h=height();
    int delay = noDelay ? 0 : 100;
    section->pimpl->animation->stop();
    section->pimpl->adjustTimer->clear();

    auto handler=[section,animate,h,this](){

        if (section->pimpl->currentItemPosition<0)
        {
            return;
        }

        section->pimpl->animation->stop();
        if (pimpl->mousePressed || pimpl->keyPressed)
        {
            adjustPosition(section);
            return;
        }

        auto pos=section->pimpl->currentItemPosition%h;
        auto sel=selectionRect();
        auto offset=pos-sel.top();
        if (offset==0)
        {
            return;
        }

        // jump to next item if scroll is above/below half height of the item
        if (section->pimpl->currentItemIndex>=0)
        {
            if (qAbs(offset)>pimpl->itemHeight/2)
            {
                if (offset<0)
                {
                    // down

                    // enable for non circtular section or not last item
                    if (!section->pimpl->circular || section->pimpl->currentItemIndex<(section->pimpl->items.size()-1))
                    {
                        offset+=pimpl->itemHeight;
                    }
                }
                else
                {
                    // up

                    // enable for non circtular section or not first item
                    if (!section->pimpl->circular || section->pimpl->currentItemIndex>0)
                    {
                        offset-=pimpl->itemHeight;
                    }
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
            section->pimpl->animation->disconnect(this);
            section->pimpl->animationVal=0;
            section->pimpl->animation->setStartValue(0);
            section->pimpl->animation->setEndValue(endVal);
            connect(section->pimpl->animation,&QVariantAnimation::valueChanged,this,[this,section,asc](const QVariant& val){

                if (pimpl->mousePressed || pimpl->keyPressed)
                {
                    adjustPosition(section);
                    return;
                }

                int rm = asc?val.toInt():-val.toInt();
                auto offs = section->pimpl->animationVal - rm;
                section->pimpl->animationVal=rm;
                section->pimpl->currentOffset+=offs;

                updateCurrentIndex(section);
                repaint();
            });
            section->pimpl->animation->start();
        }
        else
        {
            section->pimpl->currentOffset-=offset;

            updateCurrentIndex(section);
            repaint();
        }
    };

    if (delay==0)
    {
        handler();
    }
    else
    {
        section->pimpl->adjustTimer->shot(delay,handler);
    }
}

//--------------------------------------------------------------------------
int Spinner::selectedItemIndex(SpinnerSection *section) const
{
    return section->pimpl->currentItemIndex;
}

//--------------------------------------------------------------------------
int Spinner::itemsHeight(SpinnerSection *section) const
{
    return section->pimpl->items.size()*pimpl->itemHeight;
}

//--------------------------------------------------------------------------
int Spinner::sectionHeight(SpinnerSection *section) const
{
    if (section->pimpl->circular)
    {
        return height();
    }
    return qMin(height(),itemsHeight(section));
}

//--------------------------------------------------------------------------
int Spinner::sectionOffset(SpinnerSection *section) const
{
    if (section->pimpl->circular)
    {
        return 0;
    }

    auto itemsH = itemsHeight(section);
    auto h = height();
    if (h<=itemsH)
    {
        return 0;
    }

    return (h-itemsH)/2;
}

//--------------------------------------------------------------------------
void Spinner::appendItems(int sectionIndex, const QList<QWidget *> &items)
{
    auto section=pimpl->sections[sectionIndex];
    section->pimpl->items.append(items);
    for (auto&& item:items)
    {
        item->setParent(this);
        item->setVisible(false);
    }
    selectItem(section.get(),section->pimpl->currentItemIndex);
    update();
}

//--------------------------------------------------------------------------
void Spinner::removeLastItems(int sectionIndex, int count)
{
    auto section=pimpl->sections[sectionIndex];

    if (section->pimpl->currentItemIndex<0)
    {
        return;
    }

    for (auto i=0;i<count;i++)
    {
        section->pimpl->items.removeLast();
    }

    int index=section->pimpl->currentItemIndex;
    if (index>=section->pimpl->items.size())
    {
        index=section->pimpl->items.size()-1;
    }
    selectItem(section.get(),index);
    update();
}

//--------------------------------------------------------------------------
void Spinner::setItemHeight(int val) noexcept
{
    pimpl->itemHeight=val;
    pimpl->selectionHeight=val;
    pimpl->singleScrollStep=val;
    pimpl->pageScrollStep=5*val;
}

//--------------------------------------------------------------------------
int Spinner::itemHeight() const noexcept
{
   return pimpl->itemHeight;
}

//--------------------------------------------------------------------------
std::tuple<int,int,int,int> Spinner::calcTopItem(SpinnerSection *section) const
{
    int topItemIndex=0;
    int offset=sectionOffset(section);
    int y=offset;
    int h=sectionHeight(section);

    if (section->pimpl->circular)
    {
        auto b = qFloor(section->pimpl->currentOffset/pimpl->itemHeight)%section->pimpl->items.size();
        topItemIndex=(section->pimpl->items.size()-b)%section->pimpl->items.size();

        auto delta=section->pimpl->currentOffset%pimpl->itemHeight;
        if (delta>0)
        {
            topItemIndex-=1;
            if (topItemIndex<0)
            {
                topItemIndex=section->pimpl->items.size()-1;
            }
            delta=delta-pimpl->itemHeight;
        }
        y+=delta;
    }
    else
    {
        if (section->pimpl->currentOffset>h)
        {
            topItemIndex = qFloor(section->pimpl->currentOffset/pimpl->itemHeight)%section->pimpl->items.size();

            auto delta=section->pimpl->currentOffset%pimpl->itemHeight;
            if (delta>0)
            {
                topItemIndex-=1;
                if (topItemIndex<0)
                {
                    topItemIndex=0;
                }
                delta=delta-pimpl->itemHeight;
            }
            y+=delta;
        }
        else
        {
            y+=section->pimpl->currentOffset;
        }
    }

    return std::make_tuple(topItemIndex,offset,y,h);
}

//--------------------------------------------------------------------------
void Spinner::updateCurrentIndex(SpinnerSection *section)
{
    auto [topItemIndex,offset,y,h] = calcTopItem(section);
    auto sel = selectionRect(h,offset);

    // set offset for the first run
    if (section->pimpl->firstIndexUpdating)
    {
        section->pimpl->currentOffset=h/2-pimpl->selectionHeight/2-section->pimpl->currentItemIndex*pimpl->itemHeight;
    }

    // render items
    auto renderItems=[this,h,section,&y,&sel,&offset](int from, int to)
    {
        for (int i=from;i<to;i++)
        {
            if (y>=sel.top() && y<=sel.bottom())
            {
                section->pimpl->currentItemPosition=y;

                if (y==sel.top())
                {
                    section->pimpl->currentItemIndex=i;
                }
            }

            y+=pimpl->itemHeight;

            if (y>(offset+h))
            {
                break;
            }
        }
    };
    renderItems(topItemIndex,section->pimpl->items.size());
    if (section->pimpl->circular)
    {
        while (y<h)
        {
            renderItems(0,section->pimpl->items.size());
        }
    }

    // notify that selected item changed
    if (section->pimpl->previousItemIndex!=section->pimpl->currentItemIndex)
    {
        section->pimpl->notifyTimer->clear();
        section->pimpl->notifyTimer->shot(100,[section,this](){
            section->pimpl->previousItemIndex=section->pimpl->currentItemIndex;
            emit itemChanged(section->pimpl->index,section->pimpl->currentItemIndex);
        });
    }

    // uncheck first run
    section->pimpl->firstIndexUpdating=false;
}

//--------------------------------------------------------------------------
void Spinner::notifySelectionChanged(SpinnerSection* section)
{
    if (section->pimpl->previousItemIndex!=section->pimpl->currentItemIndex)
    {
        section->pimpl->notifyTimer->clear();
        section->pimpl->notifyTimer->shot(100,[section,this](){
            section->pimpl->previousItemIndex=section->pimpl->currentItemIndex;
            emit itemChanged(section->pimpl->index,section->pimpl->currentItemIndex);
        });
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
