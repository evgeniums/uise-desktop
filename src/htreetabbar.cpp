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

/** @file uise/desktop/htreetabbar.cpp
*
*  Defines HTreeTabBarItem, HTreeTabBarBuilder and HTreeTabBar.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QStyleOptionTab>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/htreetab.hpp>

#include <uise/desktop/htreetabbar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Cut \p text to \p maxLength characters, suffixed with "..." when it was actually cut --
//! see DefaultMaxTitleLength's own doc comment for why a character cap rather than pixel
//! -accurate elision.
QString elideTitle(QString text, int maxLength)
{
    if (maxLength>0 && text.length()>maxLength)
    {
        text=text.left(maxLength)+QStringLiteral("...");
    }
    return text;
}

}

//--------------------------------------------------------------------------

class HTreeTabBarItem_p
{
    public:

        HTreeTabBarItem* self=nullptr;
        QPointer<HTreeTab> tab;

        QBoxLayout* layout=nullptr;
        PushButton* closeButton=nullptr;
        QLabel* iconLabel=nullptr;
        QLabel* textLabel=nullptr;

        bool current=false;
        bool pressed=false;
        bool baseLabelsEnabled=true;
        bool hasIcon=false;

        int maxTitleLength=HTreeTabBarItem::DefaultMaxTitleLength;
        //! The untruncated text last passed to setTabText(), kept so setMaxTitleLength() can
        //! re-elide without the caller having to re-supply it.
        QString fullTabText;
};

//--------------------------------------------------------------------------

HTreeTabBarItem::HTreeTabBarItem(HTreeTab* tab, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeTabBarItem_p>())
{
    pimpl->self=this;
    pimpl->tab=tab;

    pimpl->layout=Layout::horizontal(this);

    // Base content: an icon label + a text label, usable standalone (a plain title/icon tab)
    // without any application-specific subclass. A subclass (see whitemdesktop's
    // ChatTabBarItem) adds its own widgets via addContentWidget(), positioned around this
    // pair according to closeButtonLeading().
    pimpl->iconLabel=new QLabel(this);
    pimpl->iconLabel->setObjectName("icon");
    pimpl->iconLabel->setVisible(false);
    pimpl->textLabel=new QLabel(this);
    pimpl->textLabel->setObjectName("text");

    auto icon=Style::instance().svgIconLocator().icon(QStringLiteral("HTreeTabBarItem::close"),this);
    pimpl->closeButton=new PushButton(icon,this);
    pimpl->closeButton->setObjectName("closeButton");
    pimpl->closeButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    pimpl->closeButton->setCursor(Qt::PointingHandCursor);
    connect(pimpl->closeButton,&PushButton::clicked,this,
            [this]()
            {
                emit closeRequested();
            }
    );

    // The whole item is clickable (see mousePressEvent()/mouseReleaseEvent()), so the cursor
    // matches the close/pin sub-buttons rather than falling back to the default arrow over
    // whatever empty space is left between them.
    setCursor(Qt::PointingHandCursor);

    pimpl->layout->addWidget(pimpl->closeButton);
    pimpl->layout->addWidget(pimpl->iconLabel);
    pimpl->layout->addWidget(pimpl->textLabel);

    // On macOS the close button leads; the two lines above already put it first. Everywhere
    // else it trails -- move it to the end instead of re-deriving the whole layout, so a
    // later addContentWidget() (which prepends on non-macOS) still lands between the base
    // content and the close button, exactly as if it had been built trailing from the start.
    if (!closeButtonLeading())
    {
        pimpl->layout->removeWidget(pimpl->closeButton);
        pimpl->layout->addWidget(pimpl->closeButton);
    }
}

//--------------------------------------------------------------------------

HTreeTabBarItem::~HTreeTabBarItem()
{}

//--------------------------------------------------------------------------

HTreeTab* HTreeTabBarItem::treeTab() const noexcept
{
    return pimpl->tab;
}

//--------------------------------------------------------------------------

bool HTreeTabBarItem::isCurrent() const noexcept
{
    return pimpl->current;
}

//--------------------------------------------------------------------------

bool HTreeTabBarItem::closeButtonLeading() noexcept
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setTabText(const QString& text)
{
    pimpl->fullTabText=text;
    pimpl->textLabel->setText(elideTitle(text,pimpl->maxTitleLength));
    emit sizeHintChanged();
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setMaxTitleLength(int maxLength)
{
    if (pimpl->maxTitleLength==maxLength)
    {
        return;
    }
    pimpl->maxTitleLength=maxLength;
    pimpl->textLabel->setText(elideTitle(pimpl->fullTabText,pimpl->maxTitleLength));
    emit sizeHintChanged();
}

//--------------------------------------------------------------------------

int HTreeTabBarItem::maxTitleLength() const noexcept
{
    return pimpl->maxTitleLength;
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setTabIcon(const QIcon& icon)
{
    pimpl->hasIcon=!icon.isNull();
    if (pimpl->hasIcon)
    {
        // A fixed size rather than pimpl->iconLabel->sizeHint(): before any pixmap is set
        // that sizeHint() is the QLabel's near-zero empty-content size, not a usable icon
        // size -- there is nothing else in a bare QFrame+QLabel to derive one from. Matches
        // the common native QTabBar/QPushButton default icon size.
        static const QSize IconSize(16,16);
        pimpl->iconLabel->setPixmap(icon.pixmap(IconSize));
    }
    else
    {
        pimpl->iconLabel->clear();
    }
    pimpl->iconLabel->setVisible(pimpl->baseLabelsEnabled && pimpl->hasIcon);
    emit sizeHintChanged();
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setTabTooltip(const QString& tooltip)
{
    setToolTip(tooltip);
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setCurrent(bool enable)
{
    if (pimpl->current==enable)
    {
        return;
    }
    pimpl->current=enable;
    setProperty("current",enable);
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setCloseEnabled(bool enable)
{
    pimpl->closeButton->setEnabled(enable);
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::refresh()
{
    // Base implementation displays only whatever setTabText()/setTabIcon() were last called
    // with -- nothing else to recompute. A subclass overrides this to pull application-level
    // state (see ChatTabBarItem::refresh()).
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::setBaseLabelsVisible(bool enable)
{
    if (pimpl->baseLabelsEnabled==enable)
    {
        return;
    }
    pimpl->baseLabelsEnabled=enable;
    pimpl->textLabel->setVisible(enable);
    pimpl->iconLabel->setVisible(enable && pimpl->hasIcon);
    emit sizeHintChanged();
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::addContentWidget(QWidget* w, int stretch )
{
    if (closeButtonLeading())
    {
        pimpl->layout->addWidget(w,stretch);
    }
    else
    {
        pimpl->layout->insertWidget(0,w,stretch);
    }
}

//--------------------------------------------------------------------------

PushButton* HTreeTabBarItem::closeButton() const
{
    return pimpl->closeButton;
}

//--------------------------------------------------------------------------

QBoxLayout* HTreeTabBarItem::contentLayout() const
{
    return pimpl->layout;
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::mousePressEvent(QMouseEvent* event)
{
    // Matches AvatarButton/PushButton's own press/release contract: a press only marks the
    // item down, so a press dragged out before release cancels the select. Sub-buttons
    // (close/pin) are real QAbstractButton-backed widgets underneath, so their own presses
    // are consumed before ever reaching here.
    if (event->button()==Qt::LeftButton)
    {
        pimpl->pressed=true;
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void HTreeTabBarItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && pimpl->pressed)
    {
        pimpl->pressed=false;
        if (rect().contains(event->pos()))
        {
            emit selectRequested();
        }
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

HTreeTabBarBuilder::~HTreeTabBarBuilder()
{}

//--------------------------------------------------------------------------

class HTreeTabBar_p
{
    public:

        HTreeTabBar* self=nullptr;
        QMargins itemMargins;
        SingleShotTimer* relayoutTimer=nullptr;
};

//--------------------------------------------------------------------------

HTreeTabBar::HTreeTabBar(QWidget* parent)
    : QTabBar(parent),
      pimpl(std::make_unique<HTreeTabBar_p>())
{
    pimpl->self=this;
    setObjectName("hTreeTabBar");

    pimpl->relayoutTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

HTreeTabBar::~HTreeTabBar()
{}

//--------------------------------------------------------------------------

HTreeTabBarItem* HTreeTabBar::item(int index) const
{
    return qobject_cast<HTreeTabBarItem*>(tabButton(index,QTabBar::LeftSide));
}

//--------------------------------------------------------------------------

void HTreeTabBar::setItem(int index, HTreeTabBarItem* item)
{
    auto* old=this->item(index);

    // QTabBar::setTabButton() replacing a widget hide()s the previous one but never deletes
    // it (verified in qtabbar.cpp) -- destroyWidget() it ourselves, unless the caller passed
    // the very same pointer back in, which setTabButton() would otherwise just hide.
    setTabButton(index,QTabBar::LeftSide,item);
    if (old!=nullptr && old!=item)
    {
        destroyWidget(old);
    }

    if (item!=nullptr)
    {
        connect(item,&HTreeTabBarItem::sizeHintChanged,this,&HTreeTabBar::scheduleRelayout);
        item->setCurrent(index==currentIndex());
    }
}

//--------------------------------------------------------------------------

void HTreeTabBar::setItemMargins(const QMargins& margins)
{
    pimpl->itemMargins=margins;
    scheduleRelayout();
}

//--------------------------------------------------------------------------

QMargins HTreeTabBar::itemMargins() const noexcept
{
    return pimpl->itemMargins;
}

//--------------------------------------------------------------------------

void HTreeTabBar::scheduleRelayout()
{
    pimpl->relayoutTimer->shot(
        0,
        [this]()
        {
            // QTabBar exposes no public "my content changed size, please relayout" call.
            // QEvent::FontChange is the narrowest event QTabBar::changeEvent() turns into a
            // full QTabBarPrivate::refresh() (verified against qtabbar.cpp) -- refresh()
            // re-runs layoutTabs(), which ends in tabLayoutChange() below. StyleChange also
            // works but additionally re-evaluates elide-mode/scroll-button style hints this
            // class has no reason to touch.
            QEvent event(QEvent::FontChange);
            QCoreApplication::sendEvent(this,&event);
        }
    );
}

//--------------------------------------------------------------------------

QSize HTreeTabBar::tabSizeHint(int index) const
{
    if (auto* it=item(index))
    {
        auto sz=it->sizeHint();
        const auto& m=pimpl->itemMargins;
        return QSize(sz.width()+m.left()+m.right(),sz.height()+m.top()+m.bottom());
    }
    return QTabBar::tabSizeHint(index);
}

//--------------------------------------------------------------------------

QSize HTreeTabBar::minimumTabSizeHint(int index) const
{
    if (auto* it=item(index))
    {
        auto sz=it->minimumSizeHint();
        const auto& m=pimpl->itemMargins;
        return QSize(sz.width()+m.left()+m.right(),sz.height()+m.top()+m.bottom());
    }
    return QTabBar::minimumTabSizeHint(index);
}

//--------------------------------------------------------------------------

void HTreeTabBar::tabLayoutChange()
{
    // QTabBar's own pass (already run by the time this virtual fires) only ever *moves* a
    // left/right tab button to its top-left corner -- it never resizes one (verified in
    // qtabbar.cpp's QTabBarPrivate::layoutTab()). Give every item its full tabRect() here.
    QTabBar::tabLayoutChange();

    for (int i=0;i<count();++i)
    {
        if (auto* it=item(i))
        {
            it->setGeometry(tabRect(i).marginsRemoved(pimpl->itemMargins));
        }
    }
}

//--------------------------------------------------------------------------

void HTreeTabBar::initStyleOption(QStyleOptionTab* option, int tabIndex) const
{
    QTabBar::initStyleOption(option,tabIndex);

    // The item widget only covers SE_TabBarTabLeftButton's sub-rect, not the whole tab, so
    // without this the native label would still be painted underneath/beside it. Shape,
    // selection state and hover/pressed painting are untouched -- only text/icon are blanked.
    if (item(tabIndex)!=nullptr)
    {
        option->text.clear();
        option->icon=QIcon();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
