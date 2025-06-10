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

/** @file uise/desktop/htreesplitter.cpp
*
*  Defines HTreeSplitter.
*
*/

/****************************************************************************/

#include <QTimer>
#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QScrollBar>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/alignedstretchingwidget.hpp>

#include <uise/desktop/detail/htreesplitter_p.hpp>
#include <uise/desktop/htreesplitter.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************ HTreeSplitterLine *******************************/

//--------------------------------------------------------------------------

HTreeSplitterLine::HTreeSplitterLine(QWidget* parent)
    : QFrame(parent)
{
    setFixedWidth(HTreeSplitter::SectionLineWidth);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
}

//--------------------------------------------------------------------------

void HTreeSplitterLine::enterEvent(QEnterEvent* event)
{
    setCursor(Qt::CursorShape::SplitHCursor);
    QFrame::enterEvent(event);
}

//--------------------------------------------------------------------------

void HTreeSplitterLine::leaveEvent(QEvent* event)
{
    setCursor(Qt::CursorShape::ArrowCursor);
    QFrame::leaveEvent(event);
}

/************************ HTreeSplitterSection ****************************/

//--------------------------------------------------------------------------

HTreeSplitterSection::HTreeSplitterSection(QWidget* parent)
    : QFrame(parent)
{
    m_layout=Layout::horizontal(this);
}

//--------------------------------------------------------------------------

HTreeSplitterSection::~HTreeSplitterSection()
{
    if (m_widget!=nullptr)
    {
        disconnect(
            m_widget,
            SIGNAL(destroyed()),
            this,
            SLOT(onWidgetDestroyed())
        );
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterSection::setWidget(QWidget* widget)
{
    m_widget=widget;
    m_layout->addWidget(m_widget);
    m_line=new HTreeSplitterLine(m_content);
    m_stubLine=new QFrame(this);
    m_stubLine->setMinimumWidth(4);
    m_stubLine->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    m_stubLine->setVisible(false);
    m_layout->addWidget(m_line);
    m_layout->addWidget(m_stubLine);
    connect(
        m_widget,
        SIGNAL(destroyed()),
        this,
        SLOT(onWidgetDestroyed())
    );

    setMinimumWidth(minimumWidth()+widget->minimumWidth()+m_line->minimumWidth());
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitterSection::widget() const
{
    return m_widget;
}

//--------------------------------------------------------------------------

void HTreeSplitterSection::onWidgetDestroyed()
{
    destroyWidget(this);
}

//--------------------------------------------------------------------------

bool HTreeSplitterSection::isLineUnderMouse() const
{
    if (m_line==nullptr)
    {
        return false;
    }

    return m_line->underMouse();
}

//--------------------------------------------------------------------------

void HTreeSplitterSection::setLineVisible(bool enable)
{
    m_line->setVisible(enable);
    m_stubLine->setVisible(!enable);
}

/************************ HTreeSplitterInternal ***************************/

//--------------------------------------------------------------------------

HTreeSplitterInternal::HTreeSplitterInternal(HTreeSplitter* splitter, QWidget* parent)
    : QFrame(parent),
      m_blockResizeEvent(false),
      m_resizingIndex(-1),
      m_prevViewportWidth(0),
      m_splitter(splitter),
      m_stretchLastSection(false)
{
    m_blockResizeTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

HTreeSplitterInternal::~HTreeSplitterInternal()
{
    for (auto& section: m_sections)
    {
        if (section->obj!=nullptr)
        {
            disconnect(
                section->obj,
                SIGNAL(destroyed(QObject*)),
                this,
                SLOT(onSectionDestroyed(QObject*))
            );
        }
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);

    // qDebug() << " resize event size="<<event->size() << " m_blockResizeEvent="<<m_blockResizeEvent;

    auto vw=m_splitter->viewPort()->width();
    if (!m_blockResizeEvent)
    {
        auto w=event->size().width();

        // if viewport was decreased more than by 20% than adjust to new viewport width
        bool needResize=false;
        if (m_prevViewportWidth!=0)
        {
            auto prevW=m_prevViewportWidth*0.8;
            if (prevW>vw)
            {
                w=vw;
                m_prevViewportWidth=vw;
                needResize=true;
            }
        }

        auto newW=recalculateWidths(w);
        if (needResize)
        {
            // qDebug() << "needResize to " << newW;
            QTimer::singleShot(10,this,[this,newW](){setMinimumWidth(newW);resize(newW,height());emit minMaxSizeUpdated();});
        }
    }
    updatePositions();
    updateWidths();

    m_prevViewportWidth=vw;
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons()&Qt::LeftButton)
    {
        m_prevMousePos=event->pos();
    }

    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_resizingIndex>=0)
    {
        recalculateSectionStretch();
        m_resizingIndex=-1;
    }

    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

QSize HTreeSplitterInternal::sizeHint() const
{
    int h=0;
    int w=0;
    for (auto& it: m_sections)
    {
        if (!it->destroyed)
        {
            h=std::max(qobject_cast<QWidget*>(it->obj)->sizeHint().height(),h);
            w+=it->width;
        }
    }
    // // qDebug() << " HTreeSplitterInternal::sizeHint()  " << QSize(w,h);
    return QSize(w,h);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons()&Qt::LeftButton)
    {
        auto newPos=event->pos();

        for (size_t i=0;i<m_sections.size()-1;i++)
        {
            auto* it=m_sections.at(i).get();
            auto* section=qobject_cast<HTreeSplitterSection*>(it->obj);
            if (section==nullptr || it->destroyed)
            {
                continue;
            }

            if (section->isLineUnderMouse())
            {
                while (!section->isExpanded())
                {
                    if (i==0)
                    {
                        QFrame::mouseMoveEvent(event);
                        return;
                    }

                    i--;
                    it=m_sections.at(i).get();
                    section=qobject_cast<HTreeSplitterSection*>(it->obj);
                    if (section==nullptr || it->destroyed)
                    {
                        continue;
                    }
                }

                auto sectionDx=newPos.x()-m_prevMousePos.x();
                if (sectionDx!=0)
                {
                    m_blockResizeEvent=true;
                    m_blockResizeTimer->shot(300,[this](){m_blockResizeEvent=false;},true);

                    // update section before cursor
                    auto prevW=it->width;
                    it->width=prevW+sectionDx;
                    if (it->width<section->minimumWidth())
                    {
                        it->width=section->minimumWidth();
                    }
                    if (it->width>section->maximumWidth())
                    {
                        it->width=section->maximumWidth();
                    }
                    sectionDx=it->width-prevW;
                    if (sectionDx==0)
                    {
                        m_resizingIndex=-1;
                        return;
                    }
                    section->resize(it->width,section->height());
                    m_resizingIndex=i;
                    if (it->stretch==0)
                    {
                        it->stretch=100;
                    }

                    // update last section
                    auto& last=m_sections.back();
                    auto lastSection=qobject_cast<HTreeSplitterSection*>(last->obj);
                    if (last.get()!=it)
                    {
                        auto prevLastW=last->width;
                        if (sectionDx>0)
                        {
                            int dW=sectionDx;
                            auto lastWidthHint=lastSection->sizeHint().width();
                            if (last->width > lastWidthHint)
                            {
                                auto prevLastW=last->width;
                                last->width-=sectionDx;
                                if (last->width<last->minWidth)
                                {
                                    last->width=last->minWidth;
                                }
                                dW=sectionDx-(prevLastW-last->width);
                                lastSection->resize(last->width,lastSection->height());

                                // qDebug() << " resize lastWidthHint="<<lastWidthHint
                                //          << " last->width="<<last->width
                                //          << " dW="<<dW
                                //          << " sectionDx="<<sectionDx
                                //          << " width()="<<width();

                            }
                            else
                            {
                                // qDebug() << " no resize lastWidthHint="<<lastWidthHint
                                //          << " last->width="<<last->width
                                //          << " dW="<<dW
                                //          << " sectionDx="<<sectionDx
                                //          << " width()="<<width();

                            }
                            if (dW!=0)
                            {
                                // qDebug() << " resizing from " << width() << " to " << width()+dW;
                                auto newW=width()+dW;
                                setMinimumWidth(newW);
                                emit minMaxSizeUpdated();
                            }
                        }
                        else
                        {
                            int totalWidth=0;
                            for (const auto& section : m_sections)
                            {
                                if (!section->destroyed)
                                {
                                    totalWidth+=section->width;
                                }
                            }
                            auto splitterWidth=m_splitter->viewPort()->width();

                            // qDebug() << " totalWidth " << totalWidth << " splitterWidth " << splitterWidth;

                            if (/*(QApplication::keyboardModifiers() & Qt::ControlModifier) &&*/
                                totalWidth > splitterWidth
                                )
                            {
                                // auto newWidth=minimumWidth()-qAbs(sectionDx);
                                // if (newWidth==totalWidth)
                                {
                                    // qDebug() << "resizing from " << width() << " to " << totalWidth;
                                    setMinimumWidth(totalWidth);
                                    resize(totalWidth,height());
                                    emit minMaxSizeUpdated();
                                }
                            }
                            else
                            {
                                // qDebug() << "resizing last section";

                                last->width+=qAbs(sectionDx);
                                lastSection->resize(last->width,lastSection->height());
                            }
                        }

                        if (prevLastW!=last->width && last->stretch==0)
                        {
                            last->stretch=100;
                        }
                        updatePositions();
                    }
                }

                break;
            }
        }
        m_prevMousePos=newPos;
    }

    QFrame::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::onSectionDestroyed(QObject* obj)
{
    // qDebug() << "onSectionDestroyed begin count=" << m_sections.size();

    for (size_t i=0;i<m_sections.size();i++)
    {
        auto& s=m_sections[i];
        if (s->obj==obj)
        {
            m_splitter->truncate(i);
            break;
        }
    }
}

//--------------------------------------------------------------------------

int HTreeSplitterInternal::recalculateWidths(int totalWidth)
{
    // qDebug() << "HTreeSplitterInternal::recalculateWidths";

    int minTotalWidth=0;
    int hintTotalWidth=0;
    int totalStretch=0;
    int i=0;
    for (auto& section: m_sections)
    {
        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr && !section->destroyed)
        {
            minTotalWidth+=w->minimumWidth();
            hintTotalWidth+=w->sizeHint().width();

            // qDebug() << "i=" << i++ << "section hint w=" << w->sizeHint().width() << " section minWidth="<<w->minimumWidth();

            totalStretch+=section->stretch;
        }
    }
    bool stretchLastSection=m_stretchLastSection && (totalStretch<=1);

    if (hintTotalWidth<minTotalWidth)
    {
        hintTotalWidth=minTotalWidth;
    }
    if (hintTotalWidth<totalWidth)
    {
        hintTotalWidth=totalWidth;
    }
    auto totalExtraWidth=hintTotalWidth-minTotalWidth;

    std::vector<int> precalculatedWidths;
    int precalculateWidth=0;

    auto notDestroyedCount=0;
    for (auto& section: m_sections)
    {
        if (!section->destroyed)
        {
            notDestroyedCount++;
        }
    }

    for (auto& section: m_sections)
    {
        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr && !section->destroyed)
        {
            auto width=w->sizeHint().width();
            auto minWidth=w->minimumWidth();
            if (width<minWidth)
            {
                width=minWidth;
            }
            else
            {
                if (stretchLastSection)
                {
                    width=section->width;
                }
                else
                {
                    if (totalStretch!=0)
                    {
                        if (section->stretch==0)
                        {
                            width=minWidth;
                        }
                        else
                        {
                            width=qFloor(static_cast<double>(hintTotalWidth) * static_cast<double>(section->stretch)/static_cast<double>(totalStretch));
                        }
                    }
                    else
                    {
                        width=qFloor(static_cast<double>(hintTotalWidth)/static_cast<double>(notDestroyedCount));
                        if (width<minWidth)
                        {
                            width=minWidth;
                        }
                    }
                }
            }
            if (width > w->maximumWidth())
            {
                width=w->maximumWidth();
            }

            // qDebug() << "i=" << i << " maximumWidth=" << w->maximumWidth();

            precalculatedWidths.push_back(width);
            precalculateWidth+=width;
        }
        else
        {
            precalculatedWidths.push_back(0);
        }
    }

    if (stretchLastSection)
    {
        if (precalculateWidth<hintTotalWidth)
        {
            precalculatedWidths.back()+=hintTotalWidth-precalculateWidth;
        }
    }

    int precalculatedExtraWidth=precalculateWidth-minTotalWidth;
    double wRatio=0.0;
    if (precalculatedExtraWidth!=0)
    {
        wRatio=static_cast<double>(totalExtraWidth)/static_cast<double>(precalculatedExtraWidth);
    }

    // qDebug() << ", totalWidth="<<totalWidth
    //          << ", minTotalWidth="<<minTotalWidth
    //          << ", hintTotalWidth="<<hintTotalWidth
    //          << ", extraWidth="<<totalExtraWidth
    //          << ", totalStretch="<<totalStretch
    //          << ", precalculateWidth="<<precalculateWidth
    //          << ", precalculatedExtraWidth="<<precalculatedExtraWidth
    //          << " ratio="<<wRatio
        ;

    int accumulatedStretch=0;
    int accumulatedWidth=0;
    for (size_t i=0;i<m_sections.size();i++)
    {
        auto& section=m_sections[i];
        if (section->destroyed)
        {
            continue;
        }

        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr)
        {
            auto minW=w->minimumWidth();
            if (section->stretch==0 || precalculateWidth==hintTotalWidth)
            {
                section->width=precalculatedWidths[i];
            }
            else
            {
                section->width=qFloor(wRatio*static_cast<double>(precalculatedWidths[i] - minW)) + minW;
            }

            section->minWidth=minW;
            if (section->width < section->minWidth)
            {
                section->width=section->minWidth;
            }

            accumulatedWidth+=section->width;

            if (stretchLastSection)
            {
                if (i!=(m_sections.size()-1))
                {
                    section->stretch=0;
                }
                else
                {
                    section->stretch=1;
                }
            }

            // qDebug() << "i="<<i
            //          << "section->precalculated="<<precalculatedWidths[i]
            //          << "section->width"<<section->width
            //          << "section->stretch"<<section->stretch
            //     ;
        }
    }

    // qDebug() << "accumulatedWidth="<<accumulatedWidth;

    return accumulatedWidth;
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::updatePositions()
{
    auto margins=contentsMargins();
    int currentX=margins.left();
    int y=margins.top();
    for (auto& section: m_sections)
    {
        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr)
        {
            w->move(currentX,y);
            currentX+=section->width;
        }
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::updateWidths()
{
    auto margins=contentsMargins();
    auto h=height()-margins.top()-margins.bottom();
    for (auto& section: m_sections)
    {
        if (section->destroyed)
        {
            continue;
        }
        auto w=qobject_cast<HTreeSplitterSection*>(section->obj);
        if (w!=nullptr)
        {
            w->show();
            w->raise();
            w->resize(section->width,h);

            // // qDebug() << "Visible " << w->isVisible();
            // // qDebug() << "Geometry " << w->geometry();
            // // qDebug() << "W Geometry " << w->widget()->geometry();
        }
    }
}

//--------------------------------------------------------------------------

HTreeSplitterInternal::Section* HTreeSplitterInternal::section(int index) const
{
    if (index<0)
    {
        return nullptr;
    }

    auto idx=static_cast<size_t>(index);
    if (idx>=m_sections.size())
    {
        return nullptr;
    }
    return m_sections.at(idx).get();
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::recalculateSectionStretch()
{
    int totalStretchedWidth=0;
    for (size_t i=0;i<m_sections.size();i++)
    {
        const auto& section=m_sections[i];
        if (section->destroyed || section->stretch==0)
        {
            continue;
        }
        totalStretchedWidth+=section->width;
    }
    if (totalStretchedWidth==0)
    {
        return;
    }

    for (size_t i=0;i<m_sections.size();i++)
    {
        const auto& section=m_sections[i];
        if (section->stretch!=0 && !section->destroyed)
        {
            section->stretch=static_cast<double>(section->width) * static_cast<double>(100) / static_cast<double>(totalStretchedWidth);
        }
        // qDebug() << "new stretch for i="<<i<<" stretch="<<section->stretch;
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::updateStretches(Section* section, int stretch)
{
    // update stretches for the last section
    int totalStretch=0;
    int maxStretch=0;
    for (size_t i=0;i<m_sections.size();i++)
    {
        if (qAbs(m_sections[i]->width - m_sections[i]->minWidth)<2 || m_sections[i]->stretch<=1)
        {
            m_sections[i]->stretch=0;
        }

        totalStretch+=m_sections[i]->stretch;
        maxStretch=std::max(maxStretch,m_sections[i]->stretch);
    }
    if (stretch==0 && totalStretch!=0)
    {
        stretch=maxStretch;

        int newTotalStretch=totalStretch+stretch;
        if (newTotalStretch>width())
        {
            newTotalStretch=width();
        }

        double ratio=static_cast<double>(newTotalStretch)/static_cast<double>(totalStretch);

        for (size_t i=0;i<m_sections.size();i++)
        {
            if (m_sections[i]->stretch!=0)
            {
                m_sections[i]->stretch=qRound(m_sections[i]->stretch * ratio);
            }
        }
        stretch=qRound(ratio * stretch);
    }

    section->stretch=std::max(stretch,1);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::addWidget(QWidget* widget, int stretch)
{
    // qDebug() << "Add widget " << m_sections.size();

    if (!m_sections.empty())
    {
        auto s=qobject_cast<HTreeSplitterSection*>(m_sections.back()->obj);
        if (s)
        {
            s->setLineVisible(true);
        }
    }

    auto section=new HTreeSplitterSection(this);
    section->setWidget(widget);
    auto s=std::make_unique<Section>(section,widget->width(),section->minimumWidth(),stretch);
    section->setLineVisible(false);
    section->installEventFilter(this);

    updateStretches(s.get(),stretch);
    m_sections.push_back(std::move(s));

    auto newWidth=recalculateWidths(width());
    setMinimumWidth(newWidth);
    emit minMaxSizeUpdated();
    resize(newWidth,height());
    updatePositions();
    updateWidths();

    connect(
        section,
        SIGNAL(destroyed(QObject*)),
        this,
        SLOT(onSectionDestroyed(QObject*))
    );

    // setMinimumWidth(minimumWidth()+section->minimumWidth());

    // // qDebug() << "minwidth=" << minimumWidth();
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitterInternal::widget(int index) const
{
    if (index<0)
    {
        return nullptr;
    }

    auto idx=static_cast<size_t>(index);
    if (idx>=m_sections.size())
    {
        return nullptr;
    }

    return qobject_cast<QWidget*>(m_sections.at(idx)->obj);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::removeWidget(int index)
{
    // qDebug() << "remove widget " << index << " sections size " << m_sections.size();

    if (index<0)
    {
        return;
    }

    auto idx=static_cast<size_t>(index);
    if (idx>=m_sections.size())
    {
        return;
    }
    auto& s=m_sections.at(idx);
    s->destroyed=true;
    auto obj=s->obj;
    if (obj!=nullptr)
    {
        disconnect(
            obj,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(onSectionDestroyed(QObject*))
        );
    }
    onSectionDestroyed(obj);

    // qDebug() << "destroying widget " << index << " w="<<obj;
    destroyWidget(qobject_cast<QWidget*>(obj));
}

//--------------------------------------------------------------------------

int HTreeSplitterInternal::count() const
{
    return static_cast<int>(m_sections.size());
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::toggleSectionExpanded(int index, bool expanded)
{
    // qDebug() << "HTreeSplitterInternal::toggleSectionExpanded " << index << " " << expanded;

    auto s=section(index);
    if (s==nullptr)
    {
        return;
    }
    auto w=qobject_cast<HTreeSplitterSection*>(s->obj);
    if (w==nullptr)
    {
        return;
    }
    w->setExpanded(expanded);

    auto prevMinWidth=s->minWidth;
    auto prevWidth=s->width;
    s->minWidth=w->minimumWidth();
    if (!expanded)
    {
        s->stretch=0;
        s->width=s->minWidth;        
    }
    else
    {
        s->width=w->width();
    }

    if (index==m_sections.size()-1)
    {
        if (expanded)
        {
            updateStretches(s);
        }
        else
        {
            auto s=section(index-1);
            if (s!=nullptr)
            {
                updateStretches(s);
            }
        }
    }

    // qDebug() << "prev minwidth=" << prevMinWidth << " minwidth="<<s->minWidth << " prev width=" << width();

    auto minWd=minimumWidth() - prevMinWidth + s->minWidth;
    setMinimumWidth(minWd);

    auto wd=recalculateWidths(width() - prevWidth + s->width);

    // qDebug() << "recalculated width="<<wd<<" maxwidth " << maximumWidth();

    resize(wd,height());
    updatePositions();
    updateWidths();

    emit minMaxSizeUpdated();
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::truncate(int index)
{
    // qDebug() << "HTreeSplitterInternal::truncate";

    auto minW=minimumWidth();
    auto w=width();

    for (size_t i=m_sections.size()-1;i>=static_cast<size_t>(index);i--)
    {
        const auto& section=m_sections[i];

        minW-=section->minWidth;
        if (minW<0)
        {
            minW=0;
        }
        w-=section->width;
        if (w<0)
        {
            w=0;
        }
        section->destroyed=true;

        auto obj=section->obj;
        if (obj!=nullptr)
        {
            disconnect(
                obj,
                SIGNAL(destroyed(QObject*)),
                this,
                SLOT(onSectionDestroyed(QObject*))
            );
            destroyWidget(qobject_cast<QWidget*>(obj));
        }
    }

    m_sections.resize(index);
    if (!m_sections.empty())
    {
        auto s=qobject_cast<HTreeSplitterSection*>(m_sections.back()->obj);
        if (s)
        {
            s->setLineVisible(false);
        }
    }

    // qDebug() << "truncate minW="<<minW <<" w="<<w;

    setMinimumWidth(minW);    
    w=recalculateWidths(w);
    m_stretchLastSection=true;
    resize(w,height());
    updatePositions();
    updateWidths();
    emit adjustViewPortRequested(m_splitter->viewPort()->width());
    emit minMaxSizeUpdated();
    m_stretchLastSection=false;
}

/**************************** HTreeSplitter *******************************/

//--------------------------------------------------------------------------

class HTreeSplitter_p
{
    public:

        QScrollArea* scArea=nullptr;
        AlignedStretchingWidget* wrapper=nullptr;

        HTreeSplitterInternal* content=nullptr;
        QHBoxLayout* layout=nullptr;
};

//--------------------------------------------------------------------------

HTreeSplitter::HTreeSplitter(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeSplitter_p>())
{
    auto* l=Layout::horizontal(this);

    pimpl->scArea=new QScrollArea(this);
    pimpl->scArea->setWidgetResizable(true);
    pimpl->scArea->setObjectName("hTreeSplitterScArea");
    l->addWidget(pimpl->scArea);

    pimpl->wrapper=new AlignedStretchingWidget(pimpl->scArea);
    pimpl->wrapper->setObjectName("hTreeSplitterWrapper");
    pimpl->scArea->setWidget(pimpl->wrapper);

    pimpl->content=new HTreeSplitterInternal(this);
    // QFrame* cf=new QFrame();
    // cf->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    // auto* cl=Layout::vertical(cf);
    // cl->addWidget(pimpl->content);
    pimpl->wrapper->setWidget(pimpl->content,Qt::Horizontal,Qt::AlignLeft);
    // pimpl->scArea->setWidget(cf);

    connect(
        pimpl->content,
        &HTreeSplitterInternal::minMaxSizeUpdated,
        pimpl->wrapper,
        &AlignedStretchingWidget::updateMinMaxSize
    );

    connect(
        pimpl->content,
        &HTreeSplitterInternal::adjustViewPortRequested,
        pimpl->wrapper,
        [this](int width)
        {
            pimpl->wrapper->setMinimumWidth(0);
            pimpl->wrapper->resize(width,pimpl->wrapper->height());
        }
    );
}

//--------------------------------------------------------------------------

HTreeSplitter::~HTreeSplitter()
{}

//--------------------------------------------------------------------------

int HTreeSplitter::count() const
{
    return pimpl->content->count();
}

//--------------------------------------------------------------------------

void HTreeSplitter::addWidget(QWidget* widget, int stretch, bool scrollTo)
{
    pimpl->content->addWidget(widget,stretch);

    if (scrollTo)
    {
        scrollToIndex(count()-1);
    }
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitter::widget(int index) const
{
    return pimpl->content->widget(index);
}

//--------------------------------------------------------------------------

void HTreeSplitter::removeWidget(int index)
{
    pimpl->content->removeWidget(index);
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToEnd()
{
    // qDebug() << "HTreeSplitter::scrollToEnd()";
    pimpl->scArea->horizontalScrollBar()->setValue(pimpl->scArea->horizontalScrollBar()->maximum());
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToHome()
{
    // qDebug() << "HTreeSplitter::scrollToHome()";
    pimpl->scArea->horizontalScrollBar()->setValue(0);
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToWidget(QWidget* widget, int xmargin)
{
    // qDebug() << "HTreeSplitter::scrollToWidget()";
    pimpl->scArea->ensureWidgetVisible(widget,xmargin,0);
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitter::viewPort() const
{
    return pimpl->scArea->viewport();
}

//--------------------------------------------------------------------------

void HTreeSplitter::toggleSectionExpanded(int index, bool expanded)
{
    pimpl->content->toggleSectionExpanded(index,expanded);
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToIndex(int index, int xmargin)
{
    // qDebug() << "HTreeSplitter::scrollToIndex() index=" << index << " count="<<pimpl->content->count();

    if (index>=pimpl->content->count())
    {
        scrollToEnd();
        return;
    }

    int pos=0;
    for (int i=0;i<index;i++)
    {
        auto s=pimpl->content->section(i);
        if (s==nullptr || s->destroyed)
        {
            continue;
        }
        pos+=s->width;
    }

    // qDebug() << "Scroll to index="<<index<<" margin="<<xmargin << " pos="<<pos;

    pos-=xmargin;
    if (pos<0)
    {
        pos=0;
    }

    pimpl->scArea->horizontalScrollBar()->setValue(pos);
}

//--------------------------------------------------------------------------

void HTreeSplitter::truncate(int index)
{
    pimpl->content->truncate(index);
    if (count()>0)
    {
        scrollToIndex(count()-1);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
