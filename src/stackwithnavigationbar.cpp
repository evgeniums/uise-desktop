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
#include <uise/desktop/framewithrefresh.hpp>
#include <uise/desktop/navigationbar.hpp>
#include <uise/desktop/stackwithnavigationbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
    pimpl->navbar->addItem(name,tooltip);

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
    truncate(index);
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

void StackWithNavigationBar::truncate(int index)
{
    if (index<0 || index>=count())
    {
        return;
    }

    auto wasIndex=currentIndex();

    pimpl->navbar->truncate(index);

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
    truncate(0);
}

//--------------------------------------------------------------------------

size_t StackWithNavigationBar::count() const
{
    return pimpl->stack->count();
}

//--------------------------------------------------------------------------

NavigationBar* StackWithNavigationBar::navigationBar() const
{
    return pimpl->navbar;
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
