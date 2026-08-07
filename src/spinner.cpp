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

/** @file uise/desktop/src/spinner.cpp
*
*  Defines Spinner.
*
*/

/****************************************************************************/

#include <iostream>
#include <stdexcept>

#include <QApplication>
#include <QtMath>
#include <QPalette>
#include <QStyle>
#include <QResizeEvent>
#include <QPainter>
#include <QColor>
#include <QEnterEvent>
#include <QLineEdit>

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
            itemHeight(0),
            dragged(false),
            clickPending(false),
            clickTargetOffset(0)
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

        // click-to-scroll press-tracking (see Spinner::mousePressEvent/mouseMoveEvent/
        // mouseReleaseEvent and Spinner::animateScrollTo())
        QPoint pressPos;
        bool dragged;
        bool clickPending;
        int clickTargetOffset;
        std::shared_ptr<SpinnerSection> pressSection;
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

    setStyleSample(new QLineEdit());
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
void Spinner::paintEvent(QPaintEvent* /*event*/)
{
    auto w = width();
    auto widgetHeight=height();
    auto h=widgetHeight;
    auto sel = selectionRect();

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    // draw background
    // NOTE: must be rect() (local, always (0,0,width,height)), not geometry() (this widget's
    // position+size IN ITS PARENT's coordinate system) -- QPainter here already operates in
    // local widget coordinates, so drawing geometry() shifts the background fill by however far
    // this Spinner sits from its parent's origin, which grows with any surrounding layout
    // (titles, sibling widgets, margins) and reproduces as the whole "columns/rows shifted"
    // family of bugs.
    painter.setBrush(pimpl->styleSample->palette().color(QPalette::Base));
    painter.drawRect(rect());

    // draw highliting
    painter.setBrush(pimpl->styleSample->palette().color(QPalette::Highlight));
    painter.drawRoundedRect(sel,3,3);

    // NOTE: child widgets must not be drawn with widget->render(&painter,QPoint(x,y)) here.
    // That call applies the offset in the coordinate system of the painter's paint DEVICE --
    // inside a paintEvent that is the whole window's backing store, not this widget (Qt only
    // compensates for the widget->backing-store redirection when the device is itself a
    // QWidget, see QWidgetPrivate::render() in qwidget.cpp), so everything lands shifted by
    // this spinner's position within the window; the shift is invisible only when the spinner
    // sits at the window origin. Rendering each child into a pixmap and drawing it with
    // painter.drawPixmap() routes the offset through the painter's full transform stack
    // (redirection AND high-dpi scaling), which is correct on every path: on-screen paint,
    // grab(), any devicePixelRatio.
    const auto dpr=painter.device()->devicePixelRatioF();
    auto renderWidget=[&painter,dpr](QWidget* widget, const QPoint& pos)
    {
        QPixmap pix(qCeil(widget->width()*dpr),qCeil(widget->height()*dpr));
        pix.setDevicePixelRatio(dpr);
        pix.fill(Qt::transparent);
        widget->render(&pix);
        painter.drawPixmap(pos,pix);
    };

    // draw sections
    auto x=sel.left();
    for (auto&& section:pimpl->sections)
    {
        // render left bar label
        if (section->pimpl->leftBarLabel!=nullptr)
        {
            auto labelY = (widgetHeight-section->pimpl->leftBarLabel->height())/2;
            renderWidget(section->pimpl->leftBarLabel,QPoint(x,labelY));
        }
        x+=section->pimpl->leftBarWidth;

        // calculate items positions
        int topItemIndex,offset,y,h;
        std::tie(topItemIndex,offset,y,h) = calcTopItem(section.get());
        auto sel = selectionRect(h,offset);

        // render items
        auto renderItems=[this,h,section,&x,&y,&sel,&offset,&renderWidget](int from, int to)
        {
            for (int i=from;i<to;i++)
            {
                if (y+pimpl->itemHeight>offset)
                {
                    renderWidget(section->pimpl->items[i],QPoint(x,y));
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
        x+=section->pimpl->itemsWidth;

        // render right bar label
        if (section->pimpl->rightBarLabel!=nullptr)
        {
            auto labelY = (widgetHeight-section->pimpl->rightBarLabel->height())/2;
            renderWidget(section->pimpl->rightBarLabel,QPoint(x,labelY));
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
    // sections are painted starting at selectionRect().left(), not at the widget origin (see
    // paintEvent()) -- walking from 0 hit-tests every column 5px left of where it is actually
    // drawn, misresolving clicks/drags near a column boundary in a multi-section spinner
    int x=mapToGlobal(QPoint(selectionRect().left(),0)).x();

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
int Spinner::offsetForIndex(SpinnerSection* section, int index) const
{
    return sectionHeight(section)/2 - pimpl->selectionHeight/2 - index*pimpl->itemHeight;
}

//--------------------------------------------------------------------------
int Spinner::firstEnabledIndex(SpinnerSection* section) const
{
    return section->pimpl->firstEnabled;
}

//--------------------------------------------------------------------------
int Spinner::lastEnabledIndex(SpinnerSection* section) const
{
    return section->pimpl->lastEnabled;
}

//--------------------------------------------------------------------------
int Spinner::clampIndex(SpinnerSection* section, int index) const
{
    return qBound(firstEnabledIndex(section),index,lastEnabledIndex(section));
}

//--------------------------------------------------------------------------
int Spinner::clampOffset(SpinnerSection* section, int pos) const
{
    auto n=section->pimpl->items.size();
    if (n<=0 || pimpl->itemHeight<=0)
    {
        return pos;
    }

    if (section->pimpl->circular && !section->pimpl->masked)
    {
        // unmasked circular section: wrap freely, as before
        return pos;
    }

    // Both the non-circular case and a masked circular one clamp to the offsets at which the
    // first/last enabled item sit exactly in the selection band. For an unmasked non-circular
    // section this is exactly the pre-mask behaviour (firstEnabledIndex()==0,
    // lastEnabledIndex()==n-1) -- see offsetForIndex().
    auto hi=offsetForIndex(section,firstEnabledIndex(section));
    auto lo=offsetForIndex(section,lastEnabledIndex(section));
    if (lo>hi)
    {
        // degenerate (e.g. a single-item enabled range with weird geometry) -- do not clamp
        return pos;
    }
    return qBound(lo,pos,hi);
}

//--------------------------------------------------------------------------
void Spinner::scrollTo(SpinnerSection* section, int pos)
{
    pos=clampOffset(section,pos);

    if (section->pimpl->currentOffset==pos)
    {
        return;
    }

    // an explicit scroll (wheel/drag/selectItem) supersedes an in-flight click-scroll rather
    // than fighting it -- see animateScrollTo()
    if (section->pimpl->clickScrolling)
    {
        section->pimpl->clickScrolling=false;
        section->pimpl->clickAnimation->stop();
    }

    section->pimpl->currentOffset=pos;

    updateCurrentIndex(section);
    repaint();

    adjustPosition(section);
}

//--------------------------------------------------------------------------
void Spinner::animateScrollTo(SpinnerSection* section, int targetOffset)
{
    auto target=clampOffset(section,targetOffset);
    if (section->pimpl->currentOffset==target)
    {
        return;
    }

    // take the snap machinery out of play: adjustPosition()'s 100 ms timer would stop() us
    // mid-flight (see its own clickScrolling guard), and this animation lands exactly on the
    // grid by construction anyway
    section->pimpl->animation->stop();
    section->pimpl->adjustTimer->clear();

    auto anim=section->pimpl->clickAnimation;
    anim->stop();
    anim->disconnect(this);
    anim->setDuration(ClickScrollDurationMs);
    anim->setEasingCurve(QEasingCurve::OutQuad);
    anim->setStartValue(section->pimpl->currentOffset);
    anim->setEndValue(target);

    connect(anim,&QVariantAnimation::valueChanged,this,[this,section](const QVariant& val)
    {
        // only currentOffset drives painting (see paintEvent()/calcTopItem()), so deliberately
        // do NOT call updateCurrentIndex() here -- that would emit intermediate itemChanged()
        // signals and let consumers like DateTimePicker call selectItem() back into us
        // mid-animation, fighting this very scroll
        section->pimpl->currentOffset=val.toInt();
        repaint();
    });
    connect(anim,&QVariantAnimation::finished,this,[this,section]()
    {
        section->pimpl->clickScrolling=false;
        updateCurrentIndex(section);
        repaint();
        // land exactly on the grid: updateCurrentIndex() only assigns currentItemIndex when an
        // item's top is exactly at sel.top()
        adjustPosition(section,false,true);
    });

    section->pimpl->clickScrolling=true;
    anim->start();
}

//--------------------------------------------------------------------------
namespace {

// Drives QSS greying of masked-out items (uise--Spinner QLabel[itemDisabled="true"]). Guards on
// "property already has this value" to avoid unpolish()/polish() repaint storms when a mask is
// re-pushed with the same range (see Spinner::setEnabledRange()).
void applyItemDisabledProperty(QWidget* widget, bool disabled)
{
    if (widget==nullptr)
    {
        return;
    }
    auto current=widget->property("itemDisabled");
    if (current.isValid() && current.toBool()==disabled)
    {
        return;
    }
    widget->setProperty("itemDisabled",disabled);
    if (widget->style()!=nullptr)
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

}

//--------------------------------------------------------------------------
void Spinner::updateItemsDisabledState(SpinnerSection* section)
{
    for (int i=0;i<section->pimpl->items.size();++i)
    {
        applyItemDisabledProperty(section->pimpl->items.at(i),!section->itemEnabled(i));
    }
}

//--------------------------------------------------------------------------
void Spinner::enforceEnabledItems(SpinnerSection* section)
{
    auto n=section->pimpl->items.size();
    if (n<=0 || pimpl->itemHeight<=0)
    {
        return;
    }

    if (!section->pimpl->masked)
    {
        return;
    }

    auto hi=offsetForIndex(section,firstEnabledIndex(section));
    auto lo=offsetForIndex(section,lastEnabledIndex(section));
    if (lo>hi)
    {
        return;
    }

    auto pos=section->pimpl->currentOffset;

    if (section->pimpl->circular)
    {
        // A circular section's currentOffset can have drifted an arbitrary number of whole
        // periods while it was unmasked (or under a wider mask). Rendering -- and therefore
        // currentItemIndex/currentItemPosition -- is exactly P-periodic in currentOffset (see
        // calcTopItem(): adding P raises qFloor(currentOffset/itemHeight) by exactly n, leaving
        // both q%n and the remainder unchanged, including for negative offsets), so folding it
        // back by whole periods is a VISUAL NO-OP and can be assigned directly -- BEFORE any
        // gap clamping, and only the fold, or scrollTo() below (which moves only when its target
        // differs from currentOffset) would see no difference and skip re-deriving
        // currentItemIndex/currentItemPosition for the folded position entirely. This is the
        // only place such folding happens -- see clampOffset()'s comment for why per-call
        // modular arithmetic there would be wrong.
        auto p=static_cast<int>(n)*pimpl->itemHeight;
        auto d=pos-lo;
        auto k=(d>=0) ? d/p : -(((-d)+p-1)/p); // floor division, negative-safe
        pos-=k*p;                              // pos is now in [lo,lo+p)
        section->pimpl->currentOffset=pos;

        // if pos fell in the forbidden gap, the nearer boundary is the actual scroll target;
        // scrollTo() below performs that move (and only that move) through the normal path
        if (pos>hi)
        {
            pos=((pos-hi)<=(lo+p-pos)) ? hi : lo;
        }
    }

    // route the actual (possibly clamping) move through the normal path: cancels an in-flight
    // click animation, updates the index, repaints, and re-arms the post-scroll snap. A no-op
    // (scrollTo() early-returns) when pos is already within [lo,hi].
    scrollTo(section,qBound(lo,pos,hi));
}

//--------------------------------------------------------------------------
void Spinner::setEnabledRange(SpinnerSection* section, int first, int last)
{
    auto prevFirst=section->firstEnabledIndex();
    auto prevLast=section->lastEnabledIndex();
    auto prevMasked=section->pimpl->masked;

    section->setEnabledRange(first,last);

    // cheap regardless -- also catches an interior mask (set item-by-item via setItemEnabled())
    // collapsing into the same [first,last] bounds, which the bounds-only comparison below would
    // otherwise miss
    updateItemsDisabledState(section);

    if (prevMasked==section->pimpl->masked
        && prevFirst==section->firstEnabledIndex() && prevLast==section->lastEnabledIndex())
    {
        return;
    }

    enforceEnabledItems(section);
    update();
}

//--------------------------------------------------------------------------
void Spinner::resetEnabledItems(SpinnerSection* section)
{
    if (!section->pimpl->masked)
    {
        return;
    }

    section->resetEnabledItems();
    updateItemsDisabledState(section);
    update();
}

//--------------------------------------------------------------------------
void Spinner::setItemEnabled(SpinnerSection* section, int index, bool enable)
{
    if (section->itemEnabled(index)==enable)
    {
        return;
    }

    section->setItemEnabled(index,enable);
    updateItemsDisabledState(section);
    enforceEnabledItems(section);
    update();
}

//--------------------------------------------------------------------------
void Spinner::leaveEvent(QEvent *)
{
    pimpl->sectionUnderCursor.reset();
    pimpl->mousePressed=false;
}

//--------------------------------------------------------------------------
void Spinner::enterEvent(QEnterEvent* /*event*/)
{
    pimpl->sectionUnderCursor=sectionUnderCursor();
    resetWheel();
}

//--------------------------------------------------------------------------
void Spinner::mousePressEvent(QMouseEvent *event)
{
    auto section=sectionUnderCursor();
    pimpl->sectionUnderCursor=section;
    if (!section)
    {
        return;
    }

    pimpl->mousePressed=true;
    pimpl->dragged=false;
    pimpl->pressPos=event->position().toPoint();
    pimpl->lastMousePos.setY(event->position().y());
    pimpl->pressSection=section;
    pimpl->clickPending=false;

    // Capture the aim point at PRESS time and store it as an ABSOLUTE target offset, so a few
    // pixels of jitter-scroll between press and release cannot shift where the click lands.
    // Items sit on a grid of pitch itemHeight whose phase is pinned by the selection band, so
    // the item distance is just the banded-relative row index. firstIndexUpdating means the
    // section has not settled its first index yet, so a click is ignored rather than acting on
    // a not-yet-meaningful offset.
    if (!section->pimpl->firstIndexUpdating && pimpl->itemHeight>0)
    {
        auto sel=selectionRect(section.get());
        auto k=qFloor(double(pimpl->pressPos.y()-sel.top())/pimpl->itemHeight);
        if (k!=0)
        {
            pimpl->clickTargetOffset=section->pimpl->currentOffset-k*pimpl->itemHeight;
            pimpl->clickPending=true;
        }
    }

    QCursor cursor;
    cursor.setShape(Qt::OpenHandCursor);
    setCursor(cursor);
}

//--------------------------------------------------------------------------
void Spinner::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    pimpl->mousePressed=false;

    QCursor cursor;
    cursor.setShape(Qt::ArrowCursor);
    setCursor(cursor);

    auto section=pimpl->pressSection;
    const bool click=pimpl->clickPending && !pimpl->dragged && section;
    pimpl->clickPending=false;
    pimpl->pressSection.reset();

    if (click)
    {
        animateScrollTo(section.get(),pimpl->clickTargetOffset);
    }
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

    // promote to a real drag once movement passes Qt's standard threshold, which cancels the
    // pending click -- see mousePressEvent()/mouseReleaseEvent()
    if (!pimpl->dragged
        && (event->position().toPoint()-pimpl->pressPos).manhattanLength()>=QApplication::startDragDistance())
    {
        pimpl->dragged=true;
        pimpl->clickPending=false;
    }

    auto y=event->position().y();
    scroll(section.get(),pimpl->lastMousePos.y()-y);
    pimpl->lastMousePos.setY(y);
}

//--------------------------------------------------------------------------
void Spinner::setSections(std::vector<std::shared_ptr<SpinnerSection>> sections)
{
    for (auto&& section:pimpl->sections)
    {
        // animation/clickAnimation are children of adjustTimer, so delete them first
        delete section->pimpl->animation;
        delete section->pimpl->clickAnimation;
        delete section->pimpl->adjustTimer;
        delete section->pimpl->selectionTimer;
        delete section->pimpl->notifyTimer;
    }

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
        section->pimpl->clickAnimation=new QVariantAnimation(section->pimpl->adjustTimer);
        section->pimpl->clickScrolling=false;

        for (auto&& item:section->pimpl->items)
        {
            item->setParent(this);
            item->setVisible(false);
        }
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

        // a section may arrive with a mask already set (SpinnerSection::setEnabledRange()
        // called before setSections()) -- make sure its greying is applied from the start, and
        // that item 0 (not necessarily selectable) is not what gets selected. An empty section
        // has nothing to select at all -- selectItem() would throw std::out_of_range.
        updateItemsDisabledState(section.get());
        if (!section->pimpl->items.isEmpty())
        {
            selectItem(section.get(),firstEnabledIndex(section.get()));
        }

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

    // a masked-out index is not a valid target -- land on the nearest enabled item instead of
    // throwing (DateTimePicker never actually hits this: its own value is always clamped before
    // it calls selectItem(), see DateTimePicker_p::applyValueToColumns())
    auto idx=clampIndex(section,index);

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
    if (section->pimpl->clickScrolling)
    {
        // a click-to-scroll animation is in flight (see animateScrollTo()) -- its own finish
        // handler calls adjustPosition(section,false,true) once currentOffset is already exactly
        // on the grid, so there is nothing for the snap to do here, and letting it run mid-flight
        // would stop() the click animation (QAbstractAnimation::stop() emits finished(), so this
        // guard also protects against that re-entrancy)
        return;
    }

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

                    // enable for non circular section or not last enabled item
                    if (!section->pimpl->circular || section->pimpl->currentItemIndex<lastEnabledIndex(section))
                    {
                        offset+=pimpl->itemHeight;
                    }
                }
                else
                {
                    // up

                    // enable for non circular section or not first enabled item
                    if (!section->pimpl->circular || section->pimpl->currentItemIndex>firstEnabledIndex(section))
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
                // defensive: the geometric proof that this snap cannot cross a mask boundary
                // relies on selectionHeight==itemHeight (the only case that can arise today,
                // see setItemHeight()) -- route through clampOffset() as insurance regardless
                section->pimpl->currentOffset=clampOffset(section,section->pimpl->currentOffset+offs);

                updateCurrentIndex(section);
                repaint();
            });
            section->pimpl->animation->start();
        }
        else
        {
            section->pimpl->currentOffset=clampOffset(section,section->pimpl->currentOffset-offset);

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

    // keep the enabled mask parallel to items -- appended items default to enabled, mirroring
    // SpinnerSection::setItems() (QList<bool>::resize() alone would default-construct them to
    // false, the wrong default)
    if (!section->pimpl->itemsEnabled.isEmpty())
    {
        while (section->pimpl->itemsEnabled.size()<section->pimpl->items.size())
        {
            section->pimpl->itemsEnabled.append(true);
        }
        section->pimpl->updateEnabledBounds();
    }

    // reset stale "itemDisabled" state on recycled widgets: DateTimePicker's day-column pool
    // reuses labels that may have been marked disabled the last time they were loaded (see
    // DateTimePicker_p::poolLabel())
    updateItemsDisabledState(section.get());
    enforceEnabledItems(section.get());

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
        if (!section->pimpl->itemsEnabled.isEmpty())
        {
            section->pimpl->itemsEnabled.removeLast();
        }
    }
    if (!section->pimpl->itemsEnabled.isEmpty())
    {
        // must happen before the mask is used below (clampIndex()/lastEnabledIndex() inside
        // selectItem()/enforceEnabledItems()), or a stale enabledLast could still point past
        // the shrunk item list
        section->pimpl->updateEnabledBounds();
    }
    enforceEnabledItems(section.get());

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
    // set offset for the first run -- must happen BEFORE calcTopItem() below: calcTopItem()
    // reads section->pimpl->currentOffset, which for a freshly created section is still its
    // default (0), not yet centered on the index selectItem() set. Correcting the offset only
    // after computing topItemIndex/y from the stale value would make the render/tracking loop
    // below walk from item 0 regardless of which index was actually selected, clobbering
    // currentItemIndex back to whichever row lands in the middle of the (wrongly offset) view.
    if (section->pimpl->firstIndexUpdating)
    {
        // clamp into the enabled range first: selectItem()'s firstIndexUpdating branch writes
        // currentItemIndex directly, bypassing clampOffset(), so without this a mask set before
        // the section settles its first index could leave currentOffset (computed below, which
        // is exactly offsetForIndex(currentItemIndex)) outside [lo,hi]
        section->pimpl->currentItemIndex=clampIndex(section,section->pimpl->currentItemIndex);
        auto h=sectionHeight(section);
        section->pimpl->currentOffset=h/2-pimpl->selectionHeight/2-section->pimpl->currentItemIndex*pimpl->itemHeight;
    }

    int topItemIndex,offset,y,h;
    std::tie(topItemIndex,offset,y,h) = calcTopItem(section);
    auto sel = selectionRect(h,offset);

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
