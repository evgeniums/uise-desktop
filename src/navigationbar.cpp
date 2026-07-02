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
#include <QEnterEvent>
#include <QCursor>
#include <QTimer>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QStyle>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/navigationbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************NavigationBarItem*******************************/

//--------------------------------------------------------------------------

NavigationBarItem::NavigationBarItem(std::shared_ptr<SvgIcon> icon, QWidget* parent, bool checkable)
    : IconTextButton(icon, parent, icon ? IconTextButton::IconPosition::BeforeText : IconTextButton::IconPosition::Invisible),
      m_hoveringCursor(NavigationBar::DefaultHoveringCursor)
{
    setCheckable(checkable);

    connect(this,&IconTextButton::toggled,this,
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
    IconTextButton::enterEvent(event);
    if (!isChecked())
    {
        setCursor(m_hoveringCursor);
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
}

//--------------------------------------------------------------------------

void NavigationBarItem::mousePressEvent(QMouseEvent* event)
{
    // Middle click → open in new tab
    if (event->button()==Qt::MiddleButton)
    {
        if (m_openInTabEnabled)
        {
            emit openInNewTabRequested();
        }
        event->accept();
        return;
    }

    if (event->button()==Qt::LeftButton)
    {
        // Shift (or on macOS Alt+Ctrl) → open in new window
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier
#ifdef Q_OS_MAC
            ||
            (
                QApplication::keyboardModifiers() & Qt::AltModifier
                &&
                QApplication::keyboardModifiers() & Qt::ControlModifier
            )
#endif
            )
        {
            if (m_openInWindowEnabled)
            {
                emit openInNewWindowRequested();
            }
            event->accept();
            return;
        }

        // Ctrl → open in new tab
        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
        {
            if (m_openInTabEnabled)
            {
                emit openInNewTabRequested();
            }
            event->accept();
            return;
        }

        // In exclusive mode: clicking the already-checked item fires clicked() but does not uncheck it
        if (m_noToggleOff && isChecked())
        {
            emit clicked();
            event->accept();
            return;
        }
    }

    IconTextButton::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void NavigationBarItem::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_openInTabEnabled && !m_openInWindowEnabled)
    {
        event->ignore();
        return;
    }

    auto menu=new QMenu(this);

    if (m_openInTabEnabled)
    {
        auto openInTab=menu->addAction(tr("Open in new tab","NavigationBarItem"));
        connect(openInTab,&QAction::triggered,this,&NavigationBarItem::openInNewTabRequested);
    }
    if (m_openInWindowEnabled)
    {
        auto openInWindow=menu->addAction(tr("Open in new window","NavigationBarItem"));
        connect(openInWindow,&QAction::triggered,this,&NavigationBarItem::openInNewWindowRequested);
    }

    menu->exec(event->globalPos());
    event->accept();
}

/********************************NavigationBarSeparator**********************/

NavigationBarSeparator::NavigationBarSeparator(QWidget* /*parent*/)
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
    sep->setHoverCharacterClickable(isHoverCharacterClickable());

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

    if (m_hoverCharacterClickable && event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
}

//--------------------------------------------------------------------------

void NavigationBarSeparator::enterEvent(QEnterEvent* event)
{
    if (m_hoverCharacterEnabled)
    {
        setProperty("hover",true);
        style()->unpolish(this);
        style()->polish(this);

        QLabel::setText(m_hoverCharacter);
        emit hovered(true);
    }

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

        std::vector<NavigationBarItem*> items;
        std::vector<NavigationBarSeparator*> separators;

        bool exclusive=true;
        bool updating=false;

        int indexOf(NavigationBarItem* btn) const
        {
            for (int i=0;i<static_cast<int>(items.size());i++)
            {
                if (items[i]==btn) return i;
            }
            return -1;
        }

        void updateScrollArea(int addWidth=0);

        int prevWidth=0;

        NavigationBarSeparator* sepSample=nullptr;
        bool sepsVisible=true;
        Qt::CursorShape hoveringCursor=NavigationBar::DefaultHoveringCursor;

        bool checkable=true;

        SingleShotTimer* scrollTimer=nullptr;

        QString checkedSepTooltip;
        QString uncheckedSepTooltip;

        QFrame* leftFrame=nullptr;
        QBoxLayout* leftFrameLayout=nullptr;

        bool openInTabEnabled=true;
        bool openInWindowEnabled=true;
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

NavigationBarPanel::NavigationBarPanel(QWidget* parent)
    : QFrame(parent)
{
    m_layout=Layout::horizontal(this);
    setObjectName("NavigationBarPanel");
}

//--------------------------------------------------------------------------

NavigationBar::NavigationBar(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<NavigationBar_p>())
{
    setObjectName("NavigationBar");

    pimpl->self=this;
    auto vl=Layout::vertical(this);

    pimpl->scArea=new ScrollArea(this);
    vl->addWidget(pimpl->scArea);
    pimpl->scArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl->scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl->scArea->setWidgetResizable(true);

    pimpl->panel=new NavigationBarPanel(pimpl->scArea);
    pimpl->layout=pimpl->panel->hLayout();
    pimpl->scArea->setWidget(pimpl->panel);

    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->panel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->scArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    if (pimpl->scArea->viewport())
    {
        pimpl->scArea->viewport()->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    }

    pimpl->leftFrame=new QFrame(pimpl->panel);
    pimpl->leftFrame->setObjectName("leftFrame");
    pimpl->layout->addWidget(pimpl->leftFrame,0,Qt::AlignLeft | Qt::AlignVCenter);
    pimpl->leftFrameLayout=Layout::horizontal(pimpl->leftFrame);
    pimpl->leftFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    pimpl->layout->addStretch(1);

    pimpl->scrollTimer=new SingleShotTimer(this);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
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

void NavigationBar::addItem(const QString& name, const QString& tooltip, const QString& id, std::shared_ptr<SvgIcon> icon)
{
    if (pimpl->layout->count()>2)
    {
        delete pimpl->layout->takeAt(pimpl->layout->count()-1);
    }

    auto button=new NavigationBarItem(std::move(icon),this,pimpl->checkable);
    button->setNoToggleOff(pimpl->exclusive);
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
    button->setOpenInTabEnabled(pimpl->openInTabEnabled);
    button->setOpenInWindowEnabled(pimpl->openInWindowEnabled);

    auto prevCount=static_cast<int>(pimpl->items.size());
    pimpl->items.push_back(button);

    connect(button,&IconTextButton::clicked,this,
        [this,button]()
        {
            int idx=pimpl->indexOf(button);
            if (idx<0) return;
            emit indexClicked(idx);
            auto btnId=itemId(idx);
            if (!btnId.isEmpty()) emit idClicked(btnId);
        }
    );
    connect(button,&IconTextButton::toggled,this,
        [this,button](bool checked)
        {
            if (pimpl->updating) return;
            int idx=pimpl->indexOf(button);
            if (idx<0) return;
            auto btnId=itemId(idx);

            if (pimpl->exclusive && checked)
            {
                pimpl->updating=true;
                for (auto* other : pimpl->items)
                {
                    if (other!=button && other->isChecked())
                    {
                        int otherIdx=pimpl->indexOf(other);
                        auto otherId=itemId(otherIdx);
                        other->setChecked(false);
                        emit indexToggled(otherIdx,false);
                        if (!otherId.isEmpty()) emit idToggled(otherId,false);
                    }
                }
                pimpl->updating=false;
            }

            emit indexToggled(idx,checked);
            if (!btnId.isEmpty()) emit idToggled(btnId,checked);
            if (checked)
            {
                emit indexSelected(idx);
                if (!btnId.isEmpty()) emit idSelected(btnId);
            }
        }
    );
    connect(button,&NavigationBarItem::openInNewTabRequested,this,
        [this,button]()
        {
            int idx=pimpl->indexOf(button);
            if (idx<0) return;
            auto btnId=itemId(idx);
            emit indexOpenInNewTabRequested(idx);
            if (!btnId.isEmpty()) emit idOpenInNewTabRequested(btnId);
        }
    );
    connect(button,&NavigationBarItem::openInNewWindowRequested,this,
        [this,button]()
        {
            int idx=pimpl->indexOf(button);
            if (idx<0) return;
            auto btnId=itemId(idx);
            emit indexOpenInNewWindowRequested(idx);
            if (!btnId.isEmpty()) emit idOpenInNewWindowRequested(btnId);
        }
    );

    int w=0;
    if (prevCount>0)
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
        int sepIndex=static_cast<int>(pimpl->separators.size())-1;
        connect(
            sep,
            &NavigationBarSeparator::clicked,
            this,
            [this,sepIndex,id]()
            {
                emit indexSeparatorClicked(sepIndex);
                emit idSeparatorClicked(id);
            }
        );
        connect(
            sep,
            &NavigationBarSeparator::hovered,
            this,
            [this,sepIndex,id](bool enable)
            {
                emit indexSeparatorHovered(sepIndex,enable);
                emit idSeparatorHovered(id,enable);
            }
        );

        setSeparatorTooltip(sepIndex,pimpl->checkedSepTooltip);
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
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setText(name);
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::setItemTooltip(int index, const QString& tooltip)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setToolTip(tooltip);
}

//--------------------------------------------------------------------------

void NavigationBar::setItemId(int index, const QString& id)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setProperty("id",id);
}

//--------------------------------------------------------------------------

void NavigationBar::setItemData(int index, QVariant data)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setProperty("data",std::move(data));
}

//--------------------------------------------------------------------------

void NavigationBar::setItemIcon(int index, std::shared_ptr<SvgIcon> icon)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    auto button=pimpl->items[index];
    button->setSvgIcon(icon);
    button->setIconPosition(icon ? IconTextButton::IconPosition::BeforeText : IconTextButton::IconPosition::Invisible);
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::setItemTrailingIcon(int index, std::shared_ptr<SvgIcon> icon)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setTrailingSvgIcon(std::move(icon));
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

QString NavigationBar::itemName(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return QString{};
    return pimpl->items[index]->text();
}

//--------------------------------------------------------------------------

QString NavigationBar::itemTooltip(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return QString{};
    return pimpl->items[index]->toolTip();
}

//--------------------------------------------------------------------------

QString NavigationBar::itemId(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return QString{};
    return pimpl->items[index]->property("id").toString();
}

//--------------------------------------------------------------------------

QVariant NavigationBar::itemData(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return QVariant{};
    return pimpl->items[index]->property("data");
}

//--------------------------------------------------------------------------

void NavigationBar::setCurrentIndex(int index)
{
    setItemChecked(index);
}

//--------------------------------------------------------------------------

void NavigationBar::setItemChecked(int index, bool checked)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setChecked(checked);
    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

bool NavigationBar::isItemChecked(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return false;
    return pimpl->items[index]->isChecked();
}

//--------------------------------------------------------------------------

void NavigationBar::setSeparatorTooltip(int index, const QString& val)
{
    if (index>=0 && index<static_cast<int>(pimpl->separators.size()))
    {
        auto sep=pimpl->separators[index];
        sep->setToolTip(val);
    }
}

//--------------------------------------------------------------------------

QString NavigationBar::separatorTooltip(int index) const
{
    if (index>=0 && index<static_cast<int>(pimpl->separators.size()))
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

    for (int i=static_cast<int>(pimpl->items.size())-1;i>=index;i--)
    {
        auto button=pimpl->items[i];
        w-=button->sizeHint().width();
        button->disconnect(this);
        destroyWidget(button);

        if (i>0)
        {
            auto sep=pimpl->separators[static_cast<size_t>(i)-1];
            w-=sep->sizeHint().width();
            destroyWidget(sep);
        }
    }

    pimpl->items.erase(pimpl->items.begin()+index,pimpl->items.end());

    if (index>0)
    {
        pimpl->separators.resize(static_cast<size_t>(index)-1);
    }
    else
    {
        pimpl->separators.clear();
    }

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
    pimpl->exclusive=enable;
    for (auto* item : pimpl->items)
    {
        item->setNoToggleOff(enable);
    }
}

//--------------------------------------------------------------------------

bool NavigationBar::isExclusive() const
{
    return pimpl->exclusive;
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
    for (int i=0;i<static_cast<int>(pimpl->items.size());i++)
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

void NavigationBar::setItemsOpenInTabEnabled(bool enable) noexcept
{
    pimpl->openInTabEnabled=enable;
}

//--------------------------------------------------------------------------

void NavigationBar::setItemsOpenInWindowEnabled(bool enable) noexcept
{
    pimpl->openInWindowEnabled=enable;
}

//--------------------------------------------------------------------------

void NavigationBar::addLeadingWidget(QWidget* widget)
{
    pimpl->leftFrameLayout->addWidget(widget,0,Qt::AlignLeft);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
