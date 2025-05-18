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

/** @file uise/desktop/navigationbar.hpp
*
*  Defines NavigationBar.
*
*/

/****************************************************************************/

#include <QScrollBar>
#include <QScrollArea>
#include <QButtonGroup>
#include <QEnterEvent>
#include <QCursor>
#include <QTimer>
#include <QMouseEvent>
#include <QStyle>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/navigationbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************NavigationBarItem*******************************/

//--------------------------------------------------------------------------

NavigationBarItem::NavigationBarItem(QWidget* parent, bool chackable)
    : QToolButton(parent),
      m_hoveringCursor(NavigationBar::DefaultHoveringCursor)
{
    setCheckable(chackable);

    connect(this,&QToolButton::toggled,this,
        [this](bool checked)
        {
            if (!checked)
            {
                setCursor(m_hoveringCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
        }
    );
}

//--------------------------------------------------------------------------

void NavigationBarItem::enterEvent(QEnterEvent * event)
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

/********************************NavigationBarSeparator**********************/

NavigationBarSeparator::NavigationBarSeparator(QWidget* parent)
    : m_hoverCharacterEnabled(false),
      m_hoverCharacter(DefaultHoverCharacter)
{
    setText(DefaultCharacter);
}

//--------------------------------------------------------------------------

NavigationBarSeparator* NavigationBarSeparator::clone() const
{
    auto sep=new NavigationBarSeparator();
    sep->setHoverCharacterEnabled(isHoverCharacterEnabled());
    sep->setHoverCharacter(hoverCharacter());

    auto txt=text();
    if (!txt.isEmpty())
    {
        sep->setText(txt);
    }
    else
    {
        auto px=pixmap();
        if (!px.isNull())
        {
            sep->setPixmap(px);
        }
        else
        {
            auto pct=picture();
            if (!pct.isNull())
            {
                sep->setPicture(pct);
            }
        }
    }
    return sep;
}

//--------------------------------------------------------------------------

void NavigationBarSeparator::mousePressEvent(QMouseEvent* event)
{
    QLabel::mousePressEvent(event);

    if (event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
}

//--------------------------------------------------------------------------

void NavigationBarSeparator::enterEvent(QEnterEvent* event)
{
    setProperty("hover",true);
    style()->unpolish(this);
    style()->polish(this);

    if (m_hoverCharacterEnabled)
    {
        QLabel::setText(m_hoverCharacter);
    }

    emit hovered(true);

    QLabel::enterEvent(event);
}

//--------------------------------------------------------------------------

void NavigationBarSeparator::leaveEvent(QEvent* event)
{
    setProperty("hover",false);
    style()->unpolish(this);
    style()->polish(this);

    if (m_hoverCharacterEnabled)
    {
        QLabel::setText(m_fallbackCharacter);
    }

    emit hovered(false);

    QLabel::leaveEvent(event);
}

/********************************NavigationBar*******************************/

//--------------------------------------------------------------------------

class NavigationBar_p
{
    public:

        NavigationBar* self=nullptr;

        ScrollArea* scArea=nullptr;
        NavigationBarPanel* panel=nullptr;
        QHBoxLayout* layout=nullptr;

        QButtonGroup* buttons;

        std::vector<NavigationBarSeparator*> separators;

        void updateScrollArea(int addWidth=0);

        int prevWidth=0;

        NavigationBarSeparator* sepSample=nullptr;
        bool sepsVisible=true;
        Qt::CursorShape hoveringCursor=NavigationBar::DefaultHoveringCursor;

        bool checkable=true;

        SingleShotTimer* scrollTimer=nullptr;

        QString checkedSepTooltip;
        QString uncheckedSepTooltip;
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
    auto h=panel->height();
    if (w>self->width())
    {
        h+=scArea->horizontalScrollBar()->height();
    }
    prevWidth=w;

    if (h>0)
    {
        scArea->setMinimumHeight(h);
    }
}

//--------------------------------------------------------------------------

NavigationBar::NavigationBar(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<NavigationBar_p>())
{
    pimpl->self=this;
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
            auto id=itemId(index);
            if (checked)
            {
                emit indexSelected(index);                
                if (!id.isEmpty())
                {
                    emit idSelected(id);
                }
            }
            emit indexToggled(index,checked);
            if (!id.isEmpty())
            {
                emit idToggled(id,checked);
            }
        }
    );
    connect(pimpl->buttons,&QButtonGroup::idClicked,this,
        [this](int index)
        {
            emit indexClicked(index);
            auto id=itemId(index);
            if (!id.isEmpty())
            {
                emit idClicked(id);
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

    pimpl->layout->addStretch(1);

    pimpl->scrollTimer=new SingleShotTimer(this);
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

void  NavigationBar::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::addItem(const QString& name, const QString& tooltip, const QString& id)
{
    if (pimpl->layout->count()>1)
    {
        delete pimpl->layout->takeAt(pimpl->layout->count()-1);
    }

    auto button=new NavigationBarItem(this,pimpl->checkable);
    button->setHoveringCursor(pimpl->hoveringCursor);
    if (!tooltip.isEmpty())
    {
        button->setToolTip(tooltip);
    }
    if (!id.isEmpty())
    {
        button->setProperty("id",id);
    }
    button->setText(name);

    auto prevButtons=pimpl->buttons->buttons();
    pimpl->buttons->addButton(button,prevButtons.count());

    int w=0;
    if (!prevButtons.isEmpty())
    {
        NavigationBarSeparator* sep=nullptr;
        if (pimpl->sepSample!=nullptr)
        {
            sep=pimpl->sepSample->clone();
        }
        else
        {
            sep=new NavigationBarSeparator(pimpl->panel);            
        }
        sep->setBuddy(button);
        pimpl->layout->addWidget(sep);
        pimpl->separators.emplace_back(sep);
        if (pimpl->sepsVisible)
        {
            w+=sep->sizeHint().width();
        }
        else
        {
            sep->hide();
        }
        int index=static_cast<int>(pimpl->separators.size())-1;
        connect(
            sep,
            &NavigationBarSeparator::clicked,
            this,
            [this,index,id]()
            {
                emit indexSeparatorClicked(index);
                emit idSeparatorClicked(id);
            }
        );
        connect(
            sep,
            &NavigationBarSeparator::hovered,
            this,
            [this,index,id](bool enable)
            {
                emit indexSeparatorHovered(index,enable);
                emit idSeparatorHovered(id,enable);
            }
        );

        setSeparatorTooltip(index,pimpl->checkedSepTooltip);
    }

    pimpl->layout->addWidget(button,0,Qt::AlignCenter);
    pimpl->layout->addStretch(1);
    w+=button->sizeHint().width();

    pimpl->updateScrollArea(w);

    pimpl->scrollTimer->shot(70,
         [this]()
         {
            pimpl->scArea->horizontalScrollBar()->setValue(pimpl->scArea->horizontalScrollBar()->maximum());
         },
         true
    );        
}

//--------------------------------------------------------------------------

void NavigationBar::setItemName(int index, const QString& name)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setText(name);
    }

    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::setItemTooltip(int index, const QString& tooltip)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setToolTip(tooltip);
    }
}

//--------------------------------------------------------------------------

void NavigationBar::setItemId(int index, const QString& id)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setProperty("id",id);
    }
}

//--------------------------------------------------------------------------

void NavigationBar::setItemData(int index, QVariant data)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setProperty("data",std::move(data));
    }
}

//--------------------------------------------------------------------------

QString NavigationBar::itemName(int index) const
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        return button->text();
    }
    return QString{};
}

//--------------------------------------------------------------------------

QString NavigationBar::itemTooltip(int index) const
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        return button->toolTip();
    }
    return QString{};
}

//--------------------------------------------------------------------------

QString NavigationBar::itemId(int index) const
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        return button->property("id").toString();
    }
    return QString{};
}

//--------------------------------------------------------------------------

QVariant NavigationBar::itemData(int index) const
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        return button->property("data");
    }
    return QVariant{};
}

//--------------------------------------------------------------------------

void NavigationBar::setCurrentIndex(int index)
{
    setItemChecked(index);
}

//--------------------------------------------------------------------------

void NavigationBar::setItemChecked(int index, bool checked)
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        button->setChecked(checked);
    }

    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

bool NavigationBar::isItemChecked(int index) const
{
    auto button=pimpl->buttons->button(index);
    if (button!=nullptr)
    {
        return button->isChecked();
    }

    return false;
}

//--------------------------------------------------------------------------

void NavigationBar::setSeparatorTooltip(int index, const QString& val)
{
    if (index < pimpl->separators.size() && index>=0)
    {
        auto sep=pimpl->separators[index];
        sep->setToolTip(val);
    }
}

//--------------------------------------------------------------------------

QString NavigationBar::separatorTooltip(int index) const
{
    if (index < pimpl->separators.size() && index>=0)
    {
        auto sep=pimpl->separators[index];
        return sep->toolTip();
    }
    return QString{};
}

//--------------------------------------------------------------------------

void NavigationBar::clear()
{
    truncate(0);
}

//--------------------------------------------------------------------------

void NavigationBar::truncate(int index)
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

//--------------------------------------------------------------------------

void NavigationBar::setSeparatorSample(NavigationBarSeparator* sep)
{
    pimpl->sepSample=sep;
}

//--------------------------------------------------------------------------

NavigationBarSeparator* NavigationBar::separatorSample() const noexcept
{
    return pimpl->sepSample;
}

//--------------------------------------------------------------------------

void NavigationBar::setSeparatorsVisible(bool enable) noexcept
{
    pimpl->sepsVisible=enable;
}

//--------------------------------------------------------------------------

bool NavigationBar::isSeparatorsVisible() const noexcept
{
    return pimpl->sepsVisible;
}

//--------------------------------------------------------------------------

void NavigationBar::setExclusive(bool enable)
{
    pimpl->buttons->setExclusive(enable);
}

//--------------------------------------------------------------------------

bool NavigationBar::isExclusive() const
{
    return pimpl->buttons->exclusive();
}

//--------------------------------------------------------------------------

void NavigationBar::setCheckable(bool enable)
{
    pimpl->checkable=enable;
}

//--------------------------------------------------------------------------

bool NavigationBar::isCheckable() const
{
    return pimpl->checkable;
}

//--------------------------------------------------------------------------

int NavigationBar::indexForId(const QString& id) const
{
    auto buttons=pimpl->buttons->buttons();
    for (int i=0;i<buttons.count();i++)
    {
        if (id==itemId(i))
        {
            return i;
        }
    }
    return -1;
}

//--------------------------------------------------------------------------

void NavigationBar::setItemHoveringCursor(const Qt::CursorShape& cursor) noexcept
{
    pimpl->hoveringCursor=cursor;
}

//--------------------------------------------------------------------------

Qt::CursorShape NavigationBar::itemHoveringCursor() const noexcept
{
    return pimpl->hoveringCursor;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
