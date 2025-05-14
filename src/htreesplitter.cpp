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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/alignedstretchingwidget.hpp>

#include <uise/desktop/detail/htreesplitter_p.hpp>
#include <uise/desktop/htreesplitter.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

HTreeSplitterLine::HTreeSplitterLine(QWidget* parent)
    : QFrame(parent)
{
    setMinimumWidth(4);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    // setFrameShape(QFrame::HLine);
    // setFrameShadow(QFrame::Sunken);
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
    m_layout->addWidget(m_line);
    connect(
        m_widget,
        SIGNAL(destroyed()),
        this,
        SLOT(onWidgetDestroyed())
    );

    //! @todo Update minimum width when destroying widget
    setMinimumWidth(minimumWidth()+widget->minimumWidth()+m_line->minimumWidth());
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

HTreeSplitterInternal::HTreeSplitterInternal(QWidget* parent)
    : QFrame(parent)
{
    m_layout=Layout::horizontal(this);
}

//--------------------------------------------------------------------------

HTreeSplitterInternal::~HTreeSplitterInternal()
{
    for (auto& section: m_sections)
    {
        disconnect(
            section.obj,
            SIGNAL(destroyed(QObject*)),
            this,
            SLOT(onSectionDestroyed(QObject*))
        );
    }
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

void HTreeSplitterInternal::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons()&Qt::LeftButton)
    {
        auto newPos=event->pos();

        for (auto& it: m_sections)
        {
            auto section=qobject_cast<HTreeSplitterSection*>(it.obj);
            if (section==nullptr)
            {
                continue;
            }

            if (section->isLineUnderMouse())
            {
#if 0
                auto cursorDelta=newPos.x()-m_prevMousePos.x();
                auto w=section->width();
                auto newW=cursorDelta+w;

                qDebug() << " prevMousePos.x()="<<m_prevMousePos.x()
                         << " newPos.x()=" << newPos.x()
                         << " cursorDelta=" << cursorDelta
                         << " w="<<w
                         << " newW="<<newW
                         << " section->maximumWidth()="<<section->maximumWidth()
                         << " section->maximumWidth()="<<section->maximumWidth()
                    ;

                if (newW>section->maximumWidth())
                {
                    newW=section->maximumWidth();
                    qDebug() << " newW=maximumWidth="<<newW;
                }
                else if (newW<section->minimumWidth())
                {
                    newW=section->minimumWidth();
                    qDebug() << " newW=minimumWidth="<<newW;
                }
                if (newW!=w)
                {
                    // section->resize(newW,section->height());
                    auto delta=newW-w;

                    qDebug() << " delta="<<delta;

                    if (delta!=cursorDelta)
                    {
                        auto newX=m_prevMousePos.x()+delta;

                        qDebug() << " newX"<<newX;

                        // QCursor::setPos(newX,newPos.y());
                    }
                }
#else
                auto sectionX=section->pos().x();
                auto sectionDx=newPos.x()-sectionX;
                if (sectionDx>0)
                {
                    auto expected=sectionDx;
                    // if (sectionDx>section->maximumWidth())
                    // {
                    //     sectionDx=section->maximumWidth();
                    // }
                    // else if (sectionDx<section->minimumWidth())
                    // {
                    //     sectionDx=section->minimumWidth();
                    // }
                    section->resize(sectionDx,section->height());
                    if (expected!=section->width())
                    {
                        qDebug() << " newPos.x()="<<newPos.x() << " actual x="<<(section->width()+sectionX);
                        QCursor::setPos(section->width()+sectionX,newPos.y());
                    }

                    qDebug() << " section->width()="<<section->width() << " section->minimumWidth()="<<section->minimumWidth();
                }
#endif
                break;
            }
        }
    }

    QFrame::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::onSectionDestroyed(QObject* obj)
{
    auto it=std::find_if(m_sections.begin(),m_sections.end(),
    [obj](const Section& s)
    {
        return s.obj==obj;
    });
    if (it==m_sections.end())
    {
        return;
    }
    const auto& section=*it;

    auto minW=minimumWidth();
    minW-=section.minWidth;
    if (minW<0)
    {
        minW=0;
    }

    setMinimumWidth(minW);
    m_sections.erase(it);
    emit minMaxSizeUpdated();
}

//--------------------------------------------------------------------------

void HTreeSplitterInternal::addWidget(QWidget* widget)
{
    auto section=new HTreeSplitterSection(this);
    section->setWidget(widget);
    Section s{section->minimumWidth(),section};
    m_sections.push_back(s);
    m_layout->addWidget(section);

    connect(
        section,
        SIGNAL(destroyed(QObject*)),
        this,
        SLOT(onSectionDestroyed(QObject*))
    );

    setMinimumWidth(minimumWidth()+section->minimumWidth());
    emit minMaxSizeUpdated();
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

    return qobject_cast<QWidget*>(m_sections.at(idx).obj);
}

//--------------------------------------------------------------------------

int HTreeSplitterInternal::count() const
{
    return static_cast<int>(m_sections.size());
}

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
    pimpl->scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    pimpl->scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    l->addWidget(pimpl->scArea);

    pimpl->wrapper=new AlignedStretchingWidget(pimpl->scArea);
    pimpl->scArea->setWidget(pimpl->wrapper);

    pimpl->content=new HTreeSplitterInternal();
    pimpl->wrapper->setWidget(pimpl->content,Qt::Horizontal,Qt::AlignLeft);

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

void HTreeSplitter::addWidget(QWidget* widget)
{
    pimpl->content->addWidget(widget);

    //! @todo Implement smooth scrolling
    scrollToEnd();
}

//--------------------------------------------------------------------------

QWidget* HTreeSplitter::widget(int index) const
{
    return pimpl->content->widget(index);
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

UISE_DESKTOP_NAMESPACE_END
