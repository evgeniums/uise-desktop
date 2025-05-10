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

/** @file uise/desktop/stackwithnavigationbar.hpp
*
*  Defines StackWithNavigationBar.
*
*/

/****************************************************************************/

#include <QScrollBar>
#include <QScrollArea>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QEnterEvent>
#include <QCursor>
#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/stackwithnavigationbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************NavigationBarButton*******************************/

//--------------------------------------------------------------------------

NavigationBarButton::NavigationBarButton(QWidget* parent) : QToolButton(parent),m_firstShow(true)
{
    setCheckable(true);

    connect(this,&QToolButton::toggled,this,
        [this](bool checked)
        {
            if (!checked)
            {
                setCursor(Qt::PointingHandCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
        }
    );
}

//--------------------------------------------------------------------------

void NavigationBarButton::enterEvent(QEnterEvent * event)
{
    QToolButton::enterEvent(event);
    if (!isChecked())
    {
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
}

//--------------------------------------------------------------------------

void NavigationBarButton::showEvent(QShowEvent* event)
{
    QToolButton::showEvent(event);
    if (m_firstShow)
    {
        emit shown();
        m_firstShow=false;
    }
}

/********************************NavigationBarSeparator**********************/

NavigationBarSeparator::NavigationBarSeparator(QWidget* parent)
{
    setText("–");
}

/********************************NavigationBar*******************************/

//--------------------------------------------------------------------------

class NavigationBar_p
{
    public:

        NavigationBar* self=nullptr;
        StackWithNavigationBar* parent=nullptr;

        ScrollArea* scArea=nullptr;
        NavigationBarPanel* panel=nullptr;
        QHBoxLayout* layout=nullptr;

        QButtonGroup* buttons;

        std::vector<NavigationBarSeparator*> separators;

        void updateScrollArea(int addWidth=0);

        int prevWidth=0;
};

//--------------------------------------------------------------------------

void NavigationBar_p::updateScrollArea(int addWidth)
{
    auto newWidth=prevWidth+addWidth;
    auto w=panel->width();
    if (addWidth!=0)
    {
        w=newWidth;
    }
    if (w>self->width())
    {
        scArea->setMinimumHeight(panel->height()+scArea->horizontalScrollBar()->height());
    }
    else
    {
        scArea->setMinimumHeight(panel->height());
    }
    prevWidth=w;
}

//--------------------------------------------------------------------------

NavigationBar::NavigationBar(StackWithNavigationBar* parent)
    : QFrame(parent),
      pimpl(std::make_unique<NavigationBar_p>())
{
    pimpl->self=this;
    pimpl->parent=parent;
    auto vl=Layout::vertical(this);

    pimpl->scArea=new ScrollArea(this);
    vl->addWidget(pimpl->scArea);
    pimpl->scArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    pimpl->scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl->scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl->scArea->setWidgetResizable(true);

    pimpl->panel=new NavigationBarPanel(pimpl->scArea);
    pimpl->layout=Layout::horizontal(pimpl->panel);
    pimpl->scArea->setWidget(pimpl->panel);

    pimpl->buttons=new QButtonGroup(pimpl->panel);
    pimpl->buttons->setExclusive(true);
    connect(pimpl->buttons,&QButtonGroup::idToggled,this,
        [this](int index, bool checked)
        {
            if (checked)
            {
                emit indexSelected(index);
            }
        }
    );

    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->panel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->scArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    if (pimpl->scArea->viewport())
    {
        pimpl->scArea->viewport()->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    }
}

//--------------------------------------------------------------------------

NavigationBar::~NavigationBar()
{}

//--------------------------------------------------------------------------

void  NavigationBar::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::addWidget(const QString& name, int index, const QString& tooltip)
{
    if (pimpl->layout->count()!=0)
    {
        delete pimpl->layout->takeAt(pimpl->layout->count()-1);
    }

    auto button=new NavigationBarButton(this);
    if (!tooltip.isEmpty())
    {
        button->setToolTip(tooltip);
    }
    button->setText(name);
    pimpl->buttons->addButton(button,index);

    int w=0;
    if (index>0)
    {
        auto sep=new NavigationBarSeparator(pimpl->panel);
        pimpl->layout->addWidget(sep);
        pimpl->separators.emplace_back(sep);
        w+=sep->sizeHint().width();
    }

    pimpl->layout->addWidget(button,0,Qt::AlignLeft);
    pimpl->layout->addStretch(1);
    w+=button->sizeHint().width();

    pimpl->updateScrollArea(w);
}

//--------------------------------------------------------------------------

void NavigationBar::setWidgetName(int index, const QString& name)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setText(name);
    }

    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::setWidgetTooltip(int index, const QString& tooltip)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setToolTip(tooltip);
    }
}

//--------------------------------------------------------------------------

void NavigationBar::setCurrentIndex(int index)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setChecked(true);
    }

    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::clear()
{
    clearFromIndex(0);
}

//--------------------------------------------------------------------------

void NavigationBar::clearFromIndex(int index)
{
    int w=0;

    pimpl->buttons->blockSignals(true);

    auto buttons=pimpl->buttons->buttons();
    for (size_t i=index;i<buttons.count();i++)
    {
        auto button=buttons[i];
        w-=button->sizeHint().width();
        pimpl->buttons->removeButton(button);
        destroyWidget(button);

        if (i>0)
        {
            auto sep=pimpl->separators[i-1];
            w-=sep->sizeHint().width();
            destroyWidget(sep);
        }
    }
    if (index>0)
    {
        pimpl->separators.resize(index-1);
    }
    else
    {
        pimpl->separators.clear();
    }
    pimpl->buttons->blockSignals(false);

    pimpl->updateScrollArea(w);
}

/****************************StackWithNavigationBar******************************/

//--------------------------------------------------------------------------

class StackWithNavigationBar_p
{
    public:

        NavigationBar* navbar=nullptr;
        QStackedWidget* stack=nullptr;

        bool refreshOnSelect=false;
};

//--------------------------------------------------------------------------

StackWithNavigationBar::StackWithNavigationBar(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<StackWithNavigationBar_p>())
{
    pimpl->navbar=new NavigationBar(this);
    pimpl->stack=new QStackedWidget(this);
    pimpl->stack->setObjectName("stack");

    auto vl=Layout::vertical(this);
    vl->addWidget(pimpl->navbar);
    vl->addWidget(pimpl->stack,1);

    connect(pimpl->navbar,&NavigationBar::indexSelected,this,
        [this](int index)
        {
            pimpl->stack->setCurrentIndex(index);            

            auto w=widget(index);
            if (w!=nullptr && pimpl->refreshOnSelect)
            {
                w->refresh();
            }

            emit currentChanged(index);
            emit widgetSelected(w);
        }
    );
}

//--------------------------------------------------------------------------

StackWithNavigationBar::~StackWithNavigationBar()
{}

//--------------------------------------------------------------------------

void StackWithNavigationBar::addWidget(FrameWithRefresh* widget, const QString& name, const QString& tooltip)
{
    pimpl->stack->addWidget(widget);
    pimpl->navbar->addWidget(name,count()-1,tooltip);

    if (count()==1)
    {
        setCurrentIndex(0);
    }
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::replaceWidget(FrameWithRefresh* widget, const QString& name, int index, const QString& tooltip)
{
    auto wasIndex=currentIndex();

    closeWidget(index);
    addWidget(widget,name,tooltip);

    if (wasIndex>=index)
    {
        setCurrentIndex(index);
    }
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::closeWidget(int index)
{
    clearFromIndex(index);
}

//--------------------------------------------------------------------------

int StackWithNavigationBar::currentIndex() const
{
    return pimpl->stack->currentIndex();
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setCurrentIndex(int index)
{
    pimpl->navbar->setCurrentIndex(index);
}

//--------------------------------------------------------------------------

FrameWithRefresh* StackWithNavigationBar::currentWidget() const
{
    return qobject_cast<FrameWithRefresh*>(pimpl->stack->currentWidget());
}

//--------------------------------------------------------------------------

FrameWithRefresh* StackWithNavigationBar::widget(int index) const
{
    return qobject_cast<FrameWithRefresh*>(pimpl->stack->widget(index));
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setCurrentWidget(UISE_DESKTOP_NAMESPACE::FrameWithRefresh* widget)
{
    auto index=pimpl->stack->indexOf(widget);
    if (index>=0)
    {
        setCurrentIndex(index);
    }
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::clearFromIndex(int index)
{
    if (index<0 || index>=count())
    {
        return;
    }

    auto wasIndex=currentIndex();

    pimpl->navbar->clearFromIndex(index);

    pimpl->stack->blockSignals(true);
    auto widget=pimpl->stack->widget(index);
    while (widget!=nullptr)
    {
        pimpl->stack->removeWidget(widget);
        destroyWidget(widget);
        widget=pimpl->stack->widget(index);
    }
    pimpl->stack->blockSignals(false);

    if (index>0 && wasIndex>=index)
    {
        setCurrentIndex(index-1);
    }
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::clear()
{
    clearFromIndex(0);
}

//--------------------------------------------------------------------------

size_t StackWithNavigationBar::count() const
{
    return pimpl->stack->count();
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setWidgetName(int index, const QString& name)
{
    pimpl->navbar->setWidgetName(index,name);
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setWidgetTooltip(int index, const QString& tooltip)
{
    pimpl->navbar->setWidgetTooltip(index,tooltip);
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setRefreshOnSelect(bool enable) noexcept
{
    pimpl->refreshOnSelect=enable;
}

//--------------------------------------------------------------------------

bool StackWithNavigationBar::isRefreshOnSelect() const noexcept
{
    return pimpl->refreshOnSelect;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
