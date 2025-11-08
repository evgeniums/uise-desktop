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

/** @file uise/desktop/src/drawer.cpp
*
*  Defines FrameWithDrawer.
*
*/

/****************************************************************************/

#include <QResizeEvent>
#include <QShortcut>
#include <QPalette>
#include <QVariantAnimation>
#include <QBoxLayout>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/drawer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**********************************Drawer********************************/

//--------------------------------------------------------------------------

namespace {

enum class State : uint8_t
{
    Hidden,
    Visible,
    SlidingToShow,
    SlidingToHide
};

}

class Drawer_p
{
    public:

        QWidget* self=nullptr;

        QWidget* widget=nullptr;
        FrameWithDrawer* parent=nullptr;
        QShortcut* shortcut=nullptr;
        QVariantAnimation* animation=nullptr;

        State state=State::Hidden;

        void updateDrawerPosition(const QVariant& val);

        std::pair<int,int> coordinates();
        void setAnimationBoundaries();
};

//--------------------------------------------------------------------------

std::pair<int,int> Drawer_p::coordinates()
{
    auto margins=self->contentsMargins();
    switch (parent->drawerEdge())
    {
        case (Qt::LeftEdge) :
        {
            auto leftX=-widget->width()+margins.left();
            auto rightX=margins.left();

            if (state==State::Hidden)
            {
                widget->move(leftX,widget->y());
                return std::make_pair(leftX,rightX);
            }
            else
            {
                return std::make_pair(widget->x(),leftX);
            }
        }
        break;

        case (Qt::RightEdge) :
        {
            auto leftX=self->width()-widget->width()-margins.right();
            auto rightX=self->width()-margins.right();

            if (state==State::Hidden)
            {
                widget->move(rightX,widget->y());
                return std::make_pair(rightX,leftX);
            }
            else
            {
                return std::make_pair(widget->x(),rightX);
            }
        }
        break;

        case (Qt::TopEdge) :
        {
            auto topY=-widget->height()+margins.top();
            auto bottomY=margins.top();

            if (state==State::Hidden)
            {
                widget->move(widget->x(),topY);
                return std::make_pair(topY,bottomY);
            }
            else
            {
                return std::make_pair(widget->y(),topY);
            }
        }
        break;

        case (Qt::BottomEdge) :
        {
            auto topY=self->height()-widget->height()-margins.bottom();
            auto bottomY=self->height()-margins.bottom();

            if (state==State::Hidden)
            {
                widget->move(widget->x(),bottomY);
                return std::make_pair(bottomY,topY);
            }
            else
            {
                return std::make_pair(widget->y(),bottomY);
            }
        }
        break;
    }

    return std::make_pair(0,0);
}

//--------------------------------------------------------------------------

void Drawer_p::setAnimationBoundaries()
{
    auto coord=coordinates();
    animation->setStartValue(coord.first);
    animation->setEndValue(coord.second);
}

//--------------------------------------------------------------------------

void Drawer_p::updateDrawerPosition(const QVariant& val)
{
    switch (parent->drawerEdge())
    {
        case (Qt::LeftEdge) : [[fallthrough]];
        case (Qt::RightEdge) :
        {
            widget->move(val.toInt(),widget->y());
        }
        break;

        case (Qt::TopEdge) : [[fallthrough]];
        case (Qt::BottomEdge) :
        {
            widget->move(widget->x(),val.toInt());
        }
        break;
    }
}

//--------------------------------------------------------------------------

Drawer::Drawer(FrameWithDrawer* parent)
    : QFrame(parent),
      pimpl(std::make_unique<Drawer_p>())
{
    pimpl->self=this;
    pimpl->parent=parent;
    pimpl->shortcut=new QShortcut(Qt::Key_Escape, this);
    pimpl->shortcut->setContext(Qt::WindowShortcut);
    connect(
        pimpl->shortcut,
        &QShortcut::activated,
        this,
        [this]()
        {
            closeDrawer();
        }
    );

    pimpl->animation=new QVariantAnimation(this);
    connect(
        pimpl->animation,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& val)
        {
            pimpl->updateDrawerPosition(val);
        }
    );
    connect(
        pimpl->animation,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            switch (pimpl->state)
            {
                case (State::SlidingToShow):
                {
                    pimpl->state=State::Visible;
                    pimpl->parent->afterDrawerVisible();
                }
                break;

                case (State::SlidingToHide):
                {
                    pimpl->state=State::Hidden;
                    hide();
                    pimpl->parent->afterDrawerHidden();
                }
                break;

                default:
                    break;
            }
        }
    );
}

//--------------------------------------------------------------------------

Drawer::~Drawer()
{}

//--------------------------------------------------------------------------

void Drawer::setWidget(QWidget* widget)
{
    destroyWidget(pimpl->widget);

    pimpl->widget=widget;
    pimpl->widget->setParent(this);
    updateDrawerGeometry();
}

//--------------------------------------------------------------------------

QWidget* Drawer::takeWidget()
{
    auto widget=pimpl->widget;
    if (pimpl->widget!=nullptr)
    {
        pimpl->widget->setParent(nullptr);
        pimpl->widget=nullptr;
    }
    return widget;
}

//--------------------------------------------------------------------------

void Drawer::openDrawer()
{
    if (pimpl->state==State::Visible || pimpl->state==State::SlidingToHide)
    {
        return;
    }

    QPalette pal = pimpl->parent->palette();
    auto background=pal.color(QPalette::Window);
    QString css("uise--Drawer {background-color: rgba(%1,%2,%3,%4);}");
    css=css.arg(255-background.red()).arg(255-background.green()).arg(255-background.blue()).arg(pimpl->parent->drawerAlpha());
    setStyleSheet(css);

    show();
    raise();
    updateDrawerGeometry();
    pimpl->shortcut->setEnabled(true);

    // start sliding
    pimpl->setAnimationBoundaries();
    pimpl->animation->setDuration(pimpl->parent->slideDurationMs());
    pimpl->state=State::SlidingToShow;
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void Drawer::closeDrawer(bool immediate)
{
    if (pimpl->state==State::Hidden || pimpl->state==State::SlidingToHide)
    {
        return;
    }


    pimpl->shortcut->setEnabled(false);
    if (immediate)
    {
        hide();
        pimpl->state=State::Hidden;
        pimpl->parent->afterDrawerHidden();
        return;
    }

    // start sliding
    pimpl->setAnimationBoundaries();
    pimpl->animation->setDuration(pimpl->parent->slideDurationMs());
    pimpl->state=State::SlidingToHide;
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void Drawer::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);

    if (pimpl->widget==nullptr)
    {
        closeDrawer(true);
        return;
    }

    auto underMouse=pimpl->widget->rect().contains(mapFromGlobal(QCursor::pos()));
    if (!underMouse)
    {
        closeDrawer();
    }
}

//--------------------------------------------------------------------------

void Drawer::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateDrawerGeometry();
}

//--------------------------------------------------------------------------

void Drawer::updateDrawerGeometry()
{
    if (pimpl->widget==nullptr)
    {
        return;
    }

    auto margins=contentsMargins();
    auto w=width();
    auto h=height();

    int newW=0;
    int newH=0;
    if (pimpl->parent->isVertical())
    {
        newW=w-(margins.left()+margins.right());
        newH=(h*pimpl->parent->sizePercent()/100)-(margins.top()+margins.bottom());
        newH=std::max(pimpl->widget->minimumHeight(),newH);
        newH=std::max(pimpl->widget->sizeHint().height(),newH);
    }
    else
    {
        newW=(w*pimpl->parent->sizePercent()/100)-(margins.left()+margins.right());
        newH=h-(margins.top()+margins.bottom());
        newW=std::max(pimpl->widget->minimumWidth(),newW);
        newW=std::max(pimpl->widget->sizeHint().width(),newW);
    }
    pimpl->widget->resize(newW,newH);

    switch (pimpl->parent->drawerEdge())
    {
        case (Qt::LeftEdge) : [[fallthrough]];
        case (Qt::TopEdge) :
        {
            pimpl->widget->move(margins.left(),margins.top());
        }
        break;

        case (Qt::BottomEdge) :
        {
            pimpl->widget->move(margins.left(),h-margins.top()-newH);
        }
        break;

        case (Qt::RightEdge) :
        {
            pimpl->widget->move(w-margins.right()-newW,margins.top());
        }
        break;
    }
}

/****************************FrameWithDrawer******************************/

//--------------------------------------------------------------------------

class FrameWithDrawer_p
{
    public:

        Drawer* drawer;
        bool drawerVisible=false;

        int sizePercent=FrameWithDrawer::DefaultSizePercent;
        int drawerAlpha=FrameWithDrawer::DefaultAlpha;
        int slideDurationMs=FrameWithDrawer::DefaultSlideDurationMs;

        Qt::Edge edge=Qt::LeftEdge;

        QBoxLayout* layout=nullptr;
        QWidget* contentWidget=nullptr;
};

//--------------------------------------------------------------------------

FrameWithDrawer::FrameWithDrawer(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FrameWithDrawer_p>())
{
    pimpl->drawer=new Drawer(this);
    pimpl->drawer->setVisible(false);
}

//--------------------------------------------------------------------------

FrameWithDrawer::~FrameWithDrawer()
{}

//--------------------------------------------------------------------------

void FrameWithDrawer::setDrawerWidget(QWidget* widget)
{
    pimpl->drawer->setWidget(widget);
}

//--------------------------------------------------------------------------

QWidget* FrameWithDrawer::takeDrawerWidget()
{
    return pimpl->drawer->takeWidget();
}

//--------------------------------------------------------------------------

void FrameWithDrawer::openDrawer()
{
    pimpl->drawer->openDrawer();
    pimpl->drawerVisible=true;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::closeDrawer(bool immediate)
{
    pimpl->drawer->closeDrawer(immediate);
}

//--------------------------------------------------------------------------

bool FrameWithDrawer::isDrawerVisible() const
{
    return pimpl->drawerVisible;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setDrawerVisible(bool enable)
{
    if (enable)
    {
        openDrawer();
    }
    else
    {
        closeDrawer();
    }
}

//--------------------------------------------------------------------------

void FrameWithDrawer::afterDrawerHidden()
{
    bool was=pimpl->drawerVisible;
    pimpl->drawerVisible=false;
    emit drawerHidden();
    if (was)
    {
        emit drawerVisibilityChanged(false);
    }
}

//--------------------------------------------------------------------------

void FrameWithDrawer::afterDrawerVisible()
{
    bool was=pimpl->drawerVisible;
    pimpl->drawerVisible=true;
    emit drawerShown();
    if (!was)
    {
        emit drawerVisibilityChanged(true);
    }
}

//--------------------------------------------------------------------------

void FrameWithDrawer::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    auto margins=contentsMargins();
    pimpl->drawer->resize(width()-margins.left()-margins.right(),height()-margins.top()-margins.bottom());
    pimpl->drawer->move(margins.left(),margins.top());
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setSizePercent(int val)
{
    pimpl->sizePercent=val;
}

//--------------------------------------------------------------------------

int FrameWithDrawer::sizePercent() const
{
    return pimpl->sizePercent;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setDrawerAlpha(int val)
{
    pimpl->drawerAlpha=val;
}

//--------------------------------------------------------------------------

int FrameWithDrawer::drawerAlpha() const
{
    return pimpl->drawerAlpha;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setSlideDurationMs(int val)
{
    pimpl->slideDurationMs=val;
}

//--------------------------------------------------------------------------

int FrameWithDrawer::slideDurationMs() const
{
    return pimpl->slideDurationMs;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setDrawerEdge(Qt::Edge val)
{
    pimpl->edge=val;
}

//--------------------------------------------------------------------------

Qt::Edge FrameWithDrawer::drawerEdge() const noexcept
{
    return pimpl->edge;
}

//--------------------------------------------------------------------------

bool FrameWithDrawer::isHorizontal() const noexcept
{
    return pimpl->edge==Qt::LeftEdge || pimpl->edge==Qt::RightEdge;
}

//--------------------------------------------------------------------------

void FrameWithDrawer::setContentWidget(QWidget* widget)
{
    destroyWidget(pimpl->contentWidget);
    if (pimpl->layout==nullptr)
    {
        pimpl->layout=Layout::vertical(this);
    }
    pimpl->contentWidget=widget;
    pimpl->layout->addWidget(widget,1);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
