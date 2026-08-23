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

        // Exclusive-mode re-click on the already-checked item is handled on release, see
        // mouseReleaseEvent() below -- fall through to the base class here so the press is
        // recorded (m_pressed) like any other left click.
    }

    IconTextButton::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void NavigationBarItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && m_pressed && m_noToggleOff && isChecked()
        && rect().contains(event->pos()))
    {
        // In exclusive mode, re-clicking the already-checked item fires clicked() without
        // unchecking it -- deliberately not delegating to IconTextButton::mouseReleaseEvent(),
        // whose click() would toggle() the item off.
        m_pressed=false;
        emit clicked();
        event->accept();
        return;
    }

    IconTextButton::mouseReleaseEvent(event);
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

        void updateScrollArea();

        NavigationBarSeparator* sepSample=nullptr;
        bool sepsVisible=true;
        Qt::CursorShape hoveringCursor=NavigationBar::DefaultHoveringCursor;

        bool checkable=true;

        SingleShotTimer* scrollTimer=nullptr;

        QString checkedSepTooltip;
        QString uncheckedSepTooltip;

        QFrame* leftFrame=nullptr;
        QBoxLayout* leftFrameLayout=nullptr;

        QFrame* rightFrame=nullptr;
        QBoxLayout* rightFrameLayout=nullptr;

        bool openInTabEnabled=true;
        bool openInWindowEnabled=true;

        bool singleItemVisibleMode=false;
};

//--------------------------------------------------------------------------

void NavigationBar_p::updateScrollArea()
{
    // panel is QSizePolicy::Fixed vertically, so sizeHint() already equals its laid-out height --
    // but unlike height(), it is ALSO correct before the panel has ever been laid out (on the
    // calls that run during construction, height() is still Qt's pre-layout default).
    auto h=panel->sizeHint().height();

    // Reserve room for the horizontal scrollbar only when the bar is ACTUALLY there, never by
    // predicting it from panel-width-vs-viewport-width. That prediction cannot work before the
    // first real layout: viewport()->width() is then a fresh QScrollArea's pre-layout default
    // (~98px), which any populated panel "overflows", so every call made while the tree is
    // being built latches 8px of reserved height that the first real resize then has to take
    // back. That mismatch is what painted as two bad startup frames -- one with a full-width
    // 8px scrollbar under the navbar, then one with the panel sitting centred in a bar still
    // 8px too tall -- before the navbar finally settled at the panel's own height.
    //
    // QAbstractScrollArea decides the bar's visibility itself, from geometry that is real by
    // the time it decides; eventFilter() below calls back here on the bar's Show/Hide so the
    // reservation simply follows that decision. Pre-layout the bar is never visible, so nothing
    // is reserved and the navbar keeps the panel's own height from the very first frame.
    //
    // sizeHint(), not height(): an as-yet-unlaid-out bar reports Qt's default 30px rather than
    // the 8px the style sheet gives it (see QScrollBar:horizontal in reset.qss).
    if (scArea->horizontalScrollBar()->isVisible())
    {
        h+=scArea->horizontalScrollBar()->sizeHint().height();
    }

    if (h>0 && h!=scArea->minimumHeight())
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

    auto vl=Layout::vertical(this);

    pimpl->scArea=new ScrollArea(this);
    vl->addWidget(pimpl->scArea);
    pimpl->scArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pimpl->scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl->scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl->scArea->setWidgetResizable(true);

    // updateScrollArea() reserves height for the horizontal scrollbar based on whether the bar
    // is actually visible, so it has to be re-run whenever that changes -- QAbstractScrollArea
    // shows/hides the bar on its own, without any signal to connect to.
    pimpl->scArea->horizontalScrollBar()->installEventFilter(this);

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
    pimpl->layout->addStretch(1);

    pimpl->rightFrame=new QFrame(pimpl->panel);
    pimpl->rightFrame->setObjectName("rightFrame");
    pimpl->layout->addWidget(pimpl->rightFrame,0,Qt::AlignRight | Qt::AlignVCenter);
    pimpl->rightFrameLayout=Layout::horizontal(pimpl->rightFrame);
    pimpl->rightFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

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

bool NavigationBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->scArea->horizontalScrollBar()
        &&
        (event->type()==QEvent::Show || event->type()==QEvent::Hide))
    {
        // The bar just appeared or disappeared, so the height that must be reserved for it
        // changed -- see updateScrollArea(), which reads the bar's visibility rather than
        // predicting it. Never filters the event out: QAbstractScrollArea's own handling of
        // its scrollbars must still run.
        pimpl->updateScrollArea();
    }

    return QFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void NavigationBar::addItem(const QString& name, const QString& tooltip, const QString& id, std::shared_ptr<SvgIcon> icon)
{
    auto index=pimpl->layout->count()-2;

    auto button=new NavigationBarItem(std::move(icon),this,pimpl->checkable);
    button->setNoToggleOff(pimpl->exclusive);
    if (!pimpl->singleItemVisibleMode)
    {
        button->setHoveringCursor(pimpl->hoveringCursor);
    }
    else
    {
        button->setHoveringCursor(Qt::ArrowCursor);
    }
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

        pimpl->layout->insertWidget(index,sep);
        index++;

        pimpl->separators.emplace_back(sep);
        if (!pimpl->sepsVisible || pimpl->singleItemVisibleMode)
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

    pimpl->layout->insertWidget(index,button,0,Qt::AlignCenter);

    // Settles which items single-visible-mode hides, then calls updateScrollArea() itself
    // (see its own end) -- moved ahead of that scrollTimer call so the scroll area is sized
    // from the panel's real, post-hide content, not from a still-visible earlier breadcrumb
    // that is about to be hidden by this very call.
    updateSingleItemVisibleMode();

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
    for (int i=static_cast<int>(pimpl->items.size())-1;i>=index;i--)
    {
        auto button=pimpl->items[i];
        button->disconnect(this);
        destroyWidget(button);

        if (i>0)
        {
            auto sep=pimpl->separators[static_cast<size_t>(i)-1];
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

    pimpl->updateScrollArea();

    updateSingleItemVisibleMode();
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

void NavigationBar::addTrailingWidget(QWidget* widget)
{
    pimpl->rightFrameLayout->addWidget(widget,0,Qt::AlignRight);
}

//--------------------------------------------------------------------------

bool NavigationBar::isSingleVisibleMode() const
{
    return pimpl->singleItemVisibleMode;
}

//--------------------------------------------------------------------------

void NavigationBar::setSingleVisibleMode(bool enable)
{
    pimpl->singleItemVisibleMode=enable;
    updateSingleItemVisibleMode();
}

//--------------------------------------------------------------------------

void NavigationBar::updateSingleItemVisibleMode()
{
    bool forceVisibleFound=false;
    for (auto& item: pimpl->items)
    {
        if (pimpl->singleItemVisibleMode)
        {
            if (item->isForceVisible())
            {
                forceVisibleFound=true;
                item->setVisible(true);
            }
            else
            {
                item->setVisible(false);
            }
        }
        else
        {
            item->setVisible(true);
        }
    }

    if (pimpl->singleItemVisibleMode && !forceVisibleFound)
    {
        if (!pimpl->items.empty())
        {
            auto last=pimpl->items.back();
            last->setVisible(true);
        }
    }

    pimpl->updateScrollArea();
}

//--------------------------------------------------------------------------

void NavigationBar::setForceSingleItemVisible(int index, bool enable)
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return;
    pimpl->items[index]->setForceVisible(enable);
    updateSingleItemVisibleMode();
}

//--------------------------------------------------------------------------

bool NavigationBar::isForceSingleItemVisible(int index) const
{
    if (index<0 || index>=static_cast<int>(pimpl->items.size())) return false;
    return pimpl->items[index]->isForceVisible();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
