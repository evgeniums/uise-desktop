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
    setMinimumWidth(4);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    setFrameShape(QFrame::HLine);
    setFrameShadow(QFrame::Sunken);
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
    // m_content=new QFrame();
    // m_layout=Layout::horizontal(m_content);

    // auto l=Layout::vertical(this);
    // l->addWidget(m_content);

    m_layout=Layout::horizontal(this);

    // AlignedStretchingWidget::setWidget(m_content,Qt::Horizontal,Qt::AlignLeft);
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
      m_splitter(splitter)
{
    m_blockResizeTimer=new SingleShotTimer(this);

    // m_layout=Layout::horizontal(this);
    // resize(800,600);

    // setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
}

//--------------------------------------------------------------------------

HTreeSplitterInternal::~HTreeSplitterInternal()
{
    for (auto& section: m_sections)
    {
        disconnect(
            section->obj,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(onSectionDestroyed(QObject*))
        );
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);

    qDebug() << " resize event size="<<event->size();

    if (!m_blockResizeEvent)
    {
        recalculateWidths(event->size().width());
    }
    updatePositions();
    updateWidths();
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
    // qDebug() << " HTreeSplitterInternal::sizeHint()  " << QSize(w,h);
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
            auto& it=m_sections.at(i);
            auto* section=qobject_cast<HTreeSplitterSection*>(it->obj);
            if (section==nullptr || it->destroyed)
            {
                continue;
            }

            if (section->isLineUnderMouse())
            {
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
                    if (last.get()!=it.get())
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
                                m_blockResizeEvent=true;

                                qDebug() << " resizing from " << width() << " to " << width()+dW;

                                auto newW=width()+dW;
                                setMinimumWidth(newW);
                                emit minMaxSizeUpdated();
                                m_blockResizeEvent=false;
                            }
                        }
                        else
                        {
#if 1
                            int totalWidth=0;
                            for (const auto& section : m_sections)
                            {
                                if (!section->destroyed)
                                {
                                    totalWidth+=section->width;
                                }
                            }
                            auto splitterWidth=m_splitter->viewPort()->width();

                            qDebug() << " totalWidth " << totalWidth
                                     << " splitterWidth " << splitterWidth;

                            if (/*(QApplication::keyboardModifiers() & Qt::ControlModifier) &&*/
                                totalWidth > splitterWidth
                                )
                            {
                                // auto newWidth=minimumWidth()-qAbs(sectionDx);
                                // if (newWidth==totalWidth)
                                {
                                    qDebug() << "resizing from " << width() << " to " << totalWidth;
                                    setMinimumWidth(totalWidth);
                                    resize(totalWidth,height());
                                    emit minMaxSizeUpdated();
                                }
                            }
                            else
                            {
                                qDebug() << "resizing last section";

                                last->width+=qAbs(sectionDx);
                                lastSection->resize(last->width,lastSection->height());
                            }
#endif
#if 0
                            if (dW!=0)
                            {
                                auto ww=width();
                                qDebug() << "resizing panel to " << ww-dW << " from " << ww;
                                m_blockResizeEvent=true;
                                setMinimumWidth(width()-dW);
                                emit minMaxSizeUpdated();
                                m_blockResizeTimer->shot(300,[this](){m_blockResizeEvent=false;},true);
                            }
#endif
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
    auto it=std::find_if(m_sections.begin(),m_sections.end(),
    [obj](const auto& s)
    {
        return s->obj==obj;
    });
    if (it==m_sections.end())
    {
        return;
    }
    const auto& section=*it;

    auto minW=minimumWidth();
    minW-=section->minWidth;
    if (minW<0)
    {
        minW=0;
    }

    setMinimumWidth(minW);
    m_sections.erase(it);
    auto w=width();
    w-=section->width;
    if (w<0)
    {
        w=0;
    }

    if (!m_sections.empty())
    {
        auto s=qobject_cast<HTreeSplitterSection*>(m_sections.back()->obj);
        if (s)
        {
            s->setLineVisible(false);
        }
    }

    w=recalculateWidths(w);
    resize(w,height());
    updatePositions();
    updateWidths();

    emit minMaxSizeUpdated();
}

//--------------------------------------------------------------------------

int HTreeSplitterInternal::recalculateWidths(int totalWidth)
{
    int minTotalWidth=0;
    int hintTotalWidth=0;
    int totalStretch=0;
    for (auto& section: m_sections)
    {
        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr)
        {
            minTotalWidth+=w->minimumWidth();
            hintTotalWidth+=w->sizeHint().width();
            totalStretch+=section->stretch;
        }
    }

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

            precalculatedWidths.push_back(width);
            precalculateWidth+=width;


        }
        else
        {
            precalculatedWidths.push_back(0);
        }
    }
    int precalculatedExtraWidth=precalculateWidth-minTotalWidth;
    double wRatio=0.0;
    if (precalculatedExtraWidth!=0)
    {
        wRatio=static_cast<double>(totalExtraWidth)/static_cast<double>(precalculatedExtraWidth);
    }

    qDebug() << "minTotalWidth="<<minTotalWidth
             << ", hintTotalWidth="<<hintTotalWidth
             << ", extraWidth="<<totalExtraWidth
             << ", totalStretch="<<totalStretch
             << ", precalculateWidth="<<precalculateWidth
             << ", precalculatedExtraWidth="<<precalculatedExtraWidth
             << " ratio="<<wRatio
        ;

#if 0
    int accumulatedWidth=0;
    for (auto& section: m_sections)
    {
        auto w=qobject_cast<QWidget*>(section->obj);
        if (w!=nullptr)
        {
            auto width=w->sizeHint().width();
            auto minWidth=w->minimumWidth();
            if (width<minWidth)
            {
                width=minWidth;
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
                        auto wExtra=qFloor(static_cast<double>(totalExtraWidth) * static_cast<double>(section->stretch)/static_cast<double>(totalStretch));

                        // qDebug() << "wExtra=extraWidth*section->stretch/totalStretch"<<wExtra;

                        width=wExtra+minWidth;
                        // if (width<minWidth)
                        // {
                        //     width=minWidth;
                        // }
                    }
                }
                else
                {
                    width=qFloor(static_cast<double>(hintTotalWidth)/static_cast<double>(m_sections.size()));
                    if (width<minWidth)
                    {
                        width=minWidth;
                    }
                }
            }

            section->width=width;
            section->minWidth=minWidth;

            // qDebug() << "section->width="<<section->width
            //          << ", section->minWidth="<<section->minWidth
            //          << ", section->stretch=" << section->stretch
            //     ;

            accumulatedWidth+=width;
        }
    }
#else
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

            qDebug() << "i="<<i
                     << "section->precalculated="<<precalculatedWidths[i]
                     << "section->width"<<section->width
                     << "section->stretch"<<section->stretch
                ;
        }
    }
#endif
    qDebug() << "accumulatedWidth="<<accumulatedWidth;

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

            // qDebug() << "Move to " << currentX << " width=" << section->width;
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

            // qDebug() << "Visible " << w->isVisible();
            // qDebug() << "Geometry " << w->geometry();
            // qDebug() << "W Geometry " << w->widget()->geometry();
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
        qDebug() << "new stretch for i="<<i<<" stretch="<<section->stretch;
    }
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::addWidget(QWidget* widget, int stretch)
{
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

    // update stretches for the last section
    for (size_t i=0;i<m_sections.size();i++)
    {
        if (m_sections[i]->stretch<=1)
        {
            m_sections[i]->stretch=0;
        }
    }
    s->stretch=std::max(stretch,1);
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

    // qDebug() << "minwidth=" << minimumWidth();
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
    destroyWidget(qobject_cast<QWidget*>(s->obj));
}

//--------------------------------------------------------------------------

int HTreeSplitterInternal::count() const
{
    return static_cast<int>(m_sections.size());
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
    l->addWidget(pimpl->scArea);

    pimpl->wrapper=new AlignedStretchingWidget(pimpl->scArea);
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

void HTreeSplitter::addWidget(QWidget* widget, int stretch)
{
    pimpl->content->addWidget(widget,stretch);

    //! @todo Implement smooth scrolling
    scrollToEnd();
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
    pimpl->scArea->horizontalScrollBar()->setValue(pimpl->scArea->horizontalScrollBar()->maximum());
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToHome()
{
    pimpl->scArea->horizontalScrollBar()->setValue(0);
}

//--------------------------------------------------------------------------

void HTreeSplitter::scrollToWidget(QWidget* widget, int xmargin)
{
    pimpl->scArea->ensureWidgetVisible(widget,xmargin,0);
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitter::viewPort() const
{
    return pimpl->scArea->viewport();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
