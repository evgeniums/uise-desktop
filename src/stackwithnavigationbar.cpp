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

#include <QScrollArea>
#include <QButtonGroup>
#include <QStackedWidget>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include <uise/desktop/stackwithnavigationbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************NavigationBarButton*******************************/

//--------------------------------------------------------------------------

NavigationBarButton::NavigationBarButton(QWidget* parent) : QToolButton(parent)
{
    setCheckable(true);
}

/********************************NavigationBar*******************************/

//--------------------------------------------------------------------------

class NavigationBar_p
{
    public:

        StackWithNavigationBar* parent=nullptr;

        QScrollArea* scArea=nullptr;
        NavigationBarPanel* panel=nullptr;
        QHBoxLayout* layout=nullptr;

        QButtonGroup* buttons;
};

//--------------------------------------------------------------------------

NavigationBar::NavigationBar(StackWithNavigationBar* parent)
    : QFrame(parent),
      pimpl(std::make_unique<NavigationBar_p>())
{
    pimpl->parent=parent;
    auto vl=Layout::vertical(this);

    pimpl->scArea=new QScrollArea(this);
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
}

//--------------------------------------------------------------------------

NavigationBar::~NavigationBar()
{}

//--------------------------------------------------------------------------

void NavigationBar::addWidget(const QString& name, int index, const QString& tooltip)
{
    auto button=new NavigationBarButton(this);
    if (!tooltip.isEmpty())
    {
        button->setToolTip(tooltip);
    }
    button->setText(name);
    pimpl->buttons->addButton(button,index);

    //! @todo Add separator if index >0

    pimpl->layout->addWidget(button,0,Qt::AlignLeft);
}

//--------------------------------------------------------------------------

void NavigationBar::setWidgetName(int index, const QString& name)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setText(name);
    }
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
}

//--------------------------------------------------------------------------

void NavigationBar::clear()
{
    clearFromIndex(0);
}

//--------------------------------------------------------------------------

void NavigationBar::clearFromIndex(int index)
{
    pimpl->buttons->blockSignals(true);

    auto buttons=pimpl->buttons->buttons();
    for (size_t i=index;i<buttons.count();i++)
    {
        auto button=buttons[i];
        pimpl->buttons->removeButton(button);
        destroyWidget(button);

        //! @todo Remove separators
    }

    pimpl->buttons->blockSignals(false);
}

/****************************StackWithNavigationBar******************************/

//--------------------------------------------------------------------------

class StackWithNavigationBar_p
{
    public:

        NavigationBar* navbar;
        QStackedWidget* stack;
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
    pimpl->navbar->addWidget(name,widgetCount()-1,tooltip);
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::replaceWidget(FrameWithRefresh* widget, const QString& name, int index, const QString& tooltip)
{
    closeWidget(index);
    addWidget(widget,name,tooltip);
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::closeWidget(int index)
{
    clearFromIndex(index);
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::setCurrentIndex(int index)
{
    pimpl->navbar->setCurrentIndex(index);
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
    if (index<0 || index>=widgetCount())
    {
        return;
    }

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
}

//--------------------------------------------------------------------------

void StackWithNavigationBar::clear()
{
    clearFromIndex(0);
}

//--------------------------------------------------------------------------

size_t StackWithNavigationBar::widgetCount() const
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

UISE_DESKTOP_NAMESPACE_END
