/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/dropdownmenu.cpp
*
*  Defines MenuItem and DropdownMenu.
*
*/

/****************************************************************************/

#include <map>

#include <QEvent>
#include <QFrame>
#include <QBoxLayout>
#include <QLayout>
#include <QSignalBlocker>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class DropdownMenu_p
{
    public:

        std::vector<MenuItem> items;

        QFrame* contentFrame=nullptr;
        QBoxLayout* contentLayout=nullptr;

        std::map<int,QPointer<IconTextButton>> buttons;

        bool closeOnCheckableActivation=false;

        std::shared_ptr<SvgIcon> checkIcon;
        std::shared_ptr<SvgIcon> submenuIcon;

        // lazily created, cached for this menu's whole lifetime (see DropdownMenu::
        // ensureSubmenu()) -- NOT torn down per opening the way rows are, since that would
        // churn native top-level windows and re-run the expensive first-open measurement pass
        // on every single opening
        std::map<int,QPointer<DropdownMenu>> submenus;

        SingleShotTimer* hoverTimer=nullptr;
        int openSubmenuId=-1;
        int hoverDelayMs=DropdownMenu::DefaultSubmenuHoverDelayMs;
        int closeDelayMs=DropdownMenu::DefaultSubmenuCloseDelayMs;

        MenuItem* findItem(int id)
        {
            // THIS level only -- used where a submenu's own id must not accidentally match a
            // nested child's id (which cannot happen given the uniqueness contract, but also
            // used for the buttons map, which only ever holds THIS level's rows anyway)
            for (auto& item : items)
            {
                if (!item.isSeparator && item.id==id)
                {
                    return &item;
                }
            }
            return nullptr;
        }

        static MenuItem* findItemRecursive(std::vector<MenuItem>& list, int id)
        {
            for (auto& item : list)
            {
                // separators all carry the default id==-1 -- skip them explicitly so a lookup
                // for id==-1 (never a valid caller-assigned id) cannot spuriously match one
                if (item.isSeparator)
                {
                    continue;
                }
                if (item.id==id)
                {
                    return &item;
                }
                if (!item.children.empty())
                {
                    if (auto* found=findItemRecursive(item.children,id))
                    {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        // the live row widget for an id that belongs to THIS level, or nullptr -- distinct from
        // the public, recursive itemButton() which may return a row from a nested submenu
        IconTextButton* ownRow(int id) const
        {
            auto it=buttons.find(id);
            return (it!=buttons.end() && !it->second.isNull()) ? it->second.data() : nullptr;
        }

        void clearRows()
        {
            QLayoutItem* child=nullptr;
            while ((child=contentLayout->takeAt(0))!=nullptr)
            {
                if (child->widget()!=nullptr)
                {
                    destroyWidget(child->widget());
                }
                delete child;
            }
            buttons.clear();
        }
};

//--------------------------------------------------------------------------

DropdownMenu::DropdownMenu(QWidget* parent)
    : DropdownFrame(parent),
      pimpl(std::make_unique<DropdownMenu_p>())
{
    pimpl->contentFrame=new QFrame(this);
    pimpl->contentFrame->setObjectName("menuContent");
    pimpl->contentLayout=Layout::vertical(pimpl->contentFrame);
    setContent(pimpl->contentFrame);

    pimpl->checkIcon=Style::instance().svgIconLocator().icon("DropdownMenu::check",this);
    pimpl->submenuIcon=Style::instance().svgIconLocator().icon("DropdownMenu::submenu",this);

    pimpl->hoverTimer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

DropdownMenu::~DropdownMenu()
{}

//--------------------------------------------------------------------------

void DropdownMenu::setItems(std::vector<MenuItem> items)
{
    pimpl->items=std::move(items);
}

//--------------------------------------------------------------------------

void DropdownMenu::addItem(MenuItem item)
{
    pimpl->items.push_back(std::move(item));
}

//--------------------------------------------------------------------------

void DropdownMenu::addSeparator()
{
    pimpl->items.push_back(MenuItem::separator());
}

//--------------------------------------------------------------------------

void DropdownMenu::clear()
{
    pimpl->items.clear();

    // unlike setItems()/addItem(), which deliberately leave the submenu cache alone (a stale
    // cached entry for an id that is no longer present is simply never reached again), clear()
    // wipes every id at once -- keeping the cache around here would just leak every submenu
    // ever opened, for a menu whose next setItems() call may reuse the same ids for entirely
    // different content
    pimpl->hoverTimer->clear();
    closeSubmenu(true);
    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            destroyWidget(kv.second.data());
        }
    }
    pimpl->submenus.clear();
}

//--------------------------------------------------------------------------

const std::vector<MenuItem>& DropdownMenu::items() const
{
    return pimpl->items;
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemChecked(int id, bool checked)
{
    // findItemRecursive searches the WHOLE tree (this level's items plus every submenu's, all
    // held as separate copies once a submenu has actually been opened -- see ensureSubmenu()),
    // so the descriptor is updated correctly no matter which level id belongs to
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    if (item==nullptr || !item->isCheckable)
    {
        return;
    }
    item->isChecked=checked;

    if (auto* row=pimpl->ownRow(id))
    {
        QSignalBlocker b(row);
        row->setChecked(checked);
        row->setTrailingSvgIcon(checked ? pimpl->checkIcon : nullptr);
    }

    // a cached child menu holds its own COPY of its items (see openSubmenu()), so updating this
    // level's descriptor above does not reach it on its own -- forward the same call down every
    // live child, each of which repeats this exact same two-step update at its own level, so a
    // single call at the root cascades correctly to any depth
    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            kv.second->setItemChecked(id,checked);
        }
    }
}

//--------------------------------------------------------------------------

bool DropdownMenu::isItemChecked(int id) const
{
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    return item!=nullptr && item->isChecked;
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemEnabled(int id, bool enable)
{
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    if (item==nullptr)
    {
        return;
    }
    item->isEnabled=enable;

    if (auto* row=pimpl->ownRow(id))
    {
        row->setEnabled(enable);
    }

    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            kv.second->setItemEnabled(id,enable);
        }
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemVisible(int id, bool visible)
{
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    if (item==nullptr)
    {
        return;
    }
    item->isVisible=visible;

    if (auto* row=pimpl->ownRow(id))
    {
        row->setVisible(visible);
    }

    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            kv.second->setItemVisible(id,visible);
        }
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemText(int id, const QString& text)
{
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    if (item==nullptr)
    {
        return;
    }
    item->text=text;

    if (auto* row=pimpl->ownRow(id))
    {
        row->setText(text);
    }

    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            kv.second->setItemText(id,text);
        }
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemIcon(int id, std::shared_ptr<SvgIcon> icon)
{
    auto* item=DropdownMenu_p::findItemRecursive(pimpl->items,id);
    if (item==nullptr)
    {
        return;
    }
    item->icon=icon;

    if (auto* row=pimpl->ownRow(id))
    {
        row->setSvgIcon(icon);
    }

    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            kv.second->setItemIcon(id,icon);
        }
    }
}

//--------------------------------------------------------------------------

IconTextButton* DropdownMenu::itemButton(int id) const
{
    if (auto* row=pimpl->ownRow(id))
    {
        return row;
    }

    for (auto& kv : pimpl->submenus)
    {
        if (!kv.second.isNull())
        {
            if (auto* row=kv.second->itemButton(id))
            {
                return row;
            }
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

void DropdownMenu::setCloseOnCheckableActivation(bool enable) noexcept
{
    pimpl->closeOnCheckableActivation=enable;
}

bool DropdownMenu::isCloseOnCheckableActivation() const noexcept
{
    return pimpl->closeOnCheckableActivation;
}

//--------------------------------------------------------------------------

void DropdownMenu::setSubmenuHoverDelayMs(int ms) noexcept
{
    pimpl->hoverDelayMs=ms;
}

int DropdownMenu::submenuHoverDelayMs() const noexcept
{
    return pimpl->hoverDelayMs;
}

//--------------------------------------------------------------------------

void DropdownMenu::setSubmenuCloseDelayMs(int ms) noexcept
{
    pimpl->closeDelayMs=ms;
}

int DropdownMenu::submenuCloseDelayMs() const noexcept
{
    return pimpl->closeDelayMs;
}

//--------------------------------------------------------------------------

DropdownMenu* DropdownMenu::submenuFor(int id) const
{
    auto it=pimpl->submenus.find(id);
    return (it!=pimpl->submenus.end()) ? it->second.data() : nullptr;
}

//--------------------------------------------------------------------------

void DropdownMenu::attachTo(QWidget* trigger)
{
    if (trigger==nullptr)
    {
        return;
    }

    setTriggerWidget(trigger);

    if (auto* btn=qobject_cast<IconTextButton*>(trigger))
    {
        btn->setCheckable(true);

        QPointer<IconTextButton> guarded(btn);
        connect(
            btn,
            &IconTextButton::clicked,
            this,
            [this,guarded]()
            {
                if (!guarded.isNull())
                {
                    toggleBelow(guarded);
                }
            }
        );
        // the trigger's own click() toggles its checked state synchronously (see
        // IconTextButton::click(), invoked from mouseReleaseEvent since the button activates on
        // release) before control returns here, so the very first open is already reflected;
        // these two just keep it correct for every close, including the ones DropdownFrame
        // drives on its own (Escape/outside click/re-click) that the trigger never sees a
        // matching click for
        connect(
            this,
            &DropdownFrame::shown,
            this,
            [guarded]()
            {
                if (!guarded.isNull())
                {
                    QSignalBlocker b(guarded);
                    guarded->setChecked(true);
                }
            }
        );
        connect(
            this,
            &DropdownFrame::hidden,
            this,
            [guarded]()
            {
                if (!guarded.isNull())
                {
                    QSignalBlocker b(guarded);
                    guarded->setChecked(false);
                }
            }
        );
    }
    else if (trigger->metaObject()->indexOfSignal("clicked()")>=0)
    {
        connect(trigger,SIGNAL(clicked()),this,SLOT(onTriggerClicked()));
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::onTriggerClicked()
{
    if (auto* w=qobject_cast<QWidget*>(sender()))
    {
        toggleBelow(w);
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::onItemToggled(int id, bool checked)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr)
    {
        return;
    }
    item->isChecked=checked;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        it->second->setTrailingSvgIcon(checked ? pimpl->checkIcon : nullptr);
    }

    if (checked && item->group>=0)
    {
        for (auto& other : pimpl->items)
        {
            if (other.id!=id && other.group==item->group && other.isChecked)
            {
                other.isChecked=false;

                auto oit=pimpl->buttons.find(other.id);
                if (oit!=pimpl->buttons.end() && !oit->second.isNull())
                {
                    QSignalBlocker b(oit->second);
                    oit->second->setChecked(false);
                    oit->second->setTrailingSvgIcon(nullptr);
                }
                emit itemToggled(other.id,false);
            }
        }
    }

    emit itemToggled(id,checked);

    if (pimpl->closeOnCheckableActivation)
    {
        // ownRow(), not itemButton(): id here always belongs to THIS level (see the comment
        // above), and notifyActivated() ignores its argument anyway -- ownRow() is simply the
        // more honest type to pass now that itemButton() is recursive
        notifyActivated(pimpl->ownRow(id));
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::fillContent()
{
    // the rows a submenu is currently anchored to are about to be destroyed below -- tear any
    // open submenu down first, immediately (no point animating a close no one will see)
    pimpl->hoverTimer->clear();
    closeSubmenu(true);

    pimpl->clearRows();

    // tracks whether a section row has already been emitted, so every ordinary row that
    // follows one (until the next separator) can be marked subsection="true" for QSS
    // indentation, and whether this is the very first emitted row, so a leading section can
    // have its top margin zeroed via firstItem="true"
    bool sawSection=false;
    bool firstRow=true;

    for (auto& item : pimpl->items)
    {
        if (!item.isVisible)
        {
            continue;
        }

        if (item.isSeparator)
        {
            auto* sep=new QFrame(pimpl->contentFrame);
            sep->setObjectName("separator");
            sep->setFrameShape(QFrame::HLine);
            pimpl->contentLayout->addWidget(sep);
            sawSection=false;
            firstRow=false;
            continue;
        }

        auto* btn=new IconTextButton(item.text,item.icon,pimpl->contentFrame,IconTextButton::IconPosition::BeforeText);
        if (!item.name.isEmpty())
        {
            btn->setObjectName(item.name);
        }
        btn->setEnabled(item.isEnabled);

        const auto hasSub=item.hasSubmenu();

        // a submenu item is never checkable -- its trailing chevron occupies the same slot the
        // checkmark uses (see MenuItem::submenu())
        btn->setCheckable(!hasSub && item.isCheckable);

        // dynamic properties must be set before the row is added to the layout below, so that
        // the polish pass DropdownFrame::measureContentSize() runs right after fillContent()
        // sees them
        if (item.isSection)
        {
            btn->setProperty("section",true);
            sawSection=true;
        }
        else if (sawSection)
        {
            btn->setProperty("subsection",true);
        }
        if (firstRow)
        {
            btn->setProperty("firstItem",true);
        }
        if (hasSub)
        {
            btn->setProperty("submenu",true);
        }
        firstRow=false;

        // btn's own polish pass (see the comment above) covers btn itself and its #text child,
        // but not its RippleOverlay: that child was already polished inside IconTextButton's
        // constructor, by RippleOverlay::install(), BEFORE section/firstItem/submenu were set
        // above -- same ordering gap as iconOnly/avatarOnly (see icontextbutton.cpp's
        // constructor comment). Without this, ripple.qss's [section="true"]/[firstItem="true"]
        // rippleInsetTop rules never take effect and the row's ripple bleeds into its top margin.
        Style::updateWidgetStyle(btn->rippleOverlay());

        auto id=item.id;

        // hovering ANY row -- not just submenu rows -- feeds the submenu open/close state
        // machine: hovering an ordinary row is what dismisses a sibling row's open submenu
        connect(
            btn,
            &IconTextButton::hovered,
            this,
            [this,id](bool hovered)
            {
                onRowHovered(id,hovered);
            }
        );

        if (hasSub)
        {
            btn->setTrailingSvgIcon(pimpl->submenuIcon);

            connect(
                btn,
                &IconTextButton::clicked,
                this,
                [this,id]()
                {
                    onSubmenuRowClicked(id);
                }
            );
        }
        else if (item.isCheckable)
        {
            btn->setChecked(item.isChecked);
            btn->setTrailingSvgIcon(item.isChecked ? pimpl->checkIcon : nullptr);

            connect(
                btn,
                &IconTextButton::toggled,
                this,
                [this,id](bool checked)
                {
                    onItemToggled(id,checked);
                }
            );
        }
        else
        {
            connect(
                btn,
                &IconTextButton::clicked,
                this,
                [this,id]()
                {
                    emit itemTriggered(id);
                    notifyActivated(pimpl->ownRow(id));
                }
            );
        }

        pimpl->contentLayout->addWidget(btn);
        pimpl->buttons[id]=btn;
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::clearContent()
{
    pimpl->hoverTimer->clear();
    closeSubmenu(true);
    pimpl->clearRows();
}

//--------------------------------------------------------------------------

void DropdownMenu::enterEvent(QEnterEvent* event)
{
    // the pointer has landed inside this popup -- if this is itself a submenu, cancel whatever
    // close its parent scheduled when the pointer clipped a neighbouring row on the way here.
    // Entering one of THIS frame's own rows does not send Leave to this frame (it stays inside
    // the same ancestor chain), so this fires exactly once per arrival, not once per row.
    if (auto* parent=qobject_cast<DropdownMenu*>(chainParent()))
    {
        parent->pimpl->hoverTimer->clear();
    }

    QFrame::enterEvent(event);
}

//--------------------------------------------------------------------------

void DropdownMenu::onRowHovered(int id, bool hovered)
{
    if (!hovered)
    {
        // deliberately a no-op: the pointer leaving a submenu row is exactly what happens when
        // it travels TOWARDS the submenu that row just opened, so closing here would race that
        // very travel. Dismissal instead happens when a DIFFERENT row is hovered (below), when
        // an item is activated (notifyActivated()), or when the chain closes some other way.
        return;
    }

    if (id==pimpl->openSubmenuId)
    {
        // back on the row whose submenu is already open -- cancel any pending close that a
        // previously hovered sibling row scheduled
        pimpl->hoverTimer->clear();
        return;
    }

    auto* item=pimpl->findItem(id);
    if (item!=nullptr && item->isEnabled && item->hasSubmenu())
    {
        pimpl->hoverTimer->shot(
            static_cast<size_t>(pimpl->hoverDelayMs),
            [this,id]()
            {
                closeSubmenu(true);
                openSubmenu(id);
            },
            true
        );
        return;
    }

    if (pimpl->openSubmenuId>=0)
    {
        // an ordinary row while a submenu is open: close it, but only after a grace delay --
        // pointer travel from the row that opened a submenu into that submenu very often clips
        // the row below on the way there, and closing immediately would make the submenu
        // unreachable (the same reason a native menu delays its own submenu transitions)
        pimpl->hoverTimer->shot(
            static_cast<size_t>(pimpl->closeDelayMs),
            [this]()
            {
                closeSubmenu();
            },
            true
        );
    }
    else
    {
        // nothing open yet -- cancel a still-pending open scheduled by a submenu row the
        // pointer has since moved away from
        pimpl->hoverTimer->clear();
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::onSubmenuRowClicked(int id)
{
    pimpl->hoverTimer->clear();

    if (id==pimpl->openSubmenuId)
    {
        // already open (this click landed on the owning row itself) -- nothing to do. A click
        // meant to CLOSE it instead is consumed earlier, by the submenu frame's own
        // eventFilter(): its triggerWidget() is this very row (see openSubmenu()), so the press
        // never reaches this row's own clicked() handler in the first place.
        return;
    }

    closeSubmenu(true);
    openSubmenu(id);
}

//--------------------------------------------------------------------------

void DropdownMenu::openSubmenu(int id)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr || !item->isEnabled || !item->hasSubmenu())
    {
        return;
    }
    auto* row=pimpl->ownRow(id);
    if (row==nullptr)
    {
        return;
    }

    auto* sub=ensureSubmenu(id);
    sub->setItems(item->children);   // the child holds only a copy -- refresh it every opening
    // rows are destroyed and rebuilt on every fillContent(), so the trigger must be re-pointed
    // at the CURRENT row widget on every opening, not just the first
    sub->setTriggerWidget(row);

    emit submenuAboutToShow(id);

    // anchor = this row's own vertical band, widened to THIS frame's outer edges, so the
    // submenu clears this frame's QSS padding/border and a left-flip lands on this frame's own
    // left edge. fullRect(), not geometry(): geometry() is clipped mid-animation, fullRect() is
    // the settled target rect even while this frame is still animating.
    const auto rowTop=row->mapToGlobal(QPoint(0,0)).y();
    const QRect anchorRect(fullRect().left(),rowTop,fullRect().width(),row->height());

    pimpl->openSubmenuId=id;
    Style::setStyleProperty(row,"submenuOpen",QVariant(true));

    sub->popupBesideRect(anchorRect);
}

//--------------------------------------------------------------------------

void DropdownMenu::closeSubmenu(bool immediate)
{
    if (pimpl->openSubmenuId<0)
    {
        return;
    }

    auto id=pimpl->openSubmenuId;
    pimpl->openSubmenuId=-1;

    if (auto* row=pimpl->ownRow(id))
    {
        Style::setStyleProperty(row,"submenuOpen",QVariant(false));
    }

    auto it=pimpl->submenus.find(id);
    if (it!=pimpl->submenus.end() && !it->second.isNull())
    {
        it->second->closeDropdown(immediate);
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::onSubmenuClosed(int id)
{
    // idempotent, and deliberately does nothing when id is not the one THIS instance currently
    // considers open: closeSubmenu() above already resets openSubmenuId (and the row highlight)
    // BEFORE it asks the child frame to close, so the child's own closeRequested()/hidden(),
    // both connected here (see ensureSubmenu()), land here redundantly in that case. This
    // handler only has real work to do when the submenu closed on its own -- Escape, an outside
    // click, or the whole chain closing via notifyActivated()'s chainRoot() call.
    if (pimpl->openSubmenuId!=id)
    {
        return;
    }
    pimpl->openSubmenuId=-1;

    if (auto* row=pimpl->ownRow(id))
    {
        Style::setStyleProperty(row,"submenuOpen",QVariant(false));
    }
}

//--------------------------------------------------------------------------

DropdownMenu* DropdownMenu::ensureSubmenu(int id)
{
    auto it=pimpl->submenus.find(id);
    if (it!=pimpl->submenus.end() && !it->second.isNull())
    {
        return it->second.data();
    }

    // parented to THIS frame (not to nullptr, not to the host window): QWidget::isActiveWindow()
    // walks parentWidget()->window() for Qt::Tool windows, so this parenting is what makes the
    // existing isActiveWindow() guard in DropdownFrame::eventFilter()'s WindowDeactivate case
    // protect this frame on platforms where WA_ShowWithoutActivating is not fully honoured and
    // the submenu ends up taking key focus anyway
    auto* sub=new DropdownMenu(this);
    sub->setChainParent(this);
    sub->setCloseOnCheckableActivation(pimpl->closeOnCheckableActivation);
    sub->setSubmenuHoverDelayMs(pimpl->hoverDelayMs);
    sub->setSubmenuCloseDelayMs(pimpl->closeDelayMs);

    connect(sub,&DropdownMenu::itemTriggered,this,&DropdownMenu::itemTriggered);
    connect(
        sub,
        &DropdownMenu::itemToggled,
        this,
        [this](int nestedId, bool checked)
        {
            // write the child's decision back into THIS level's own copy of the nested
            // descriptor, then bubble the signal -- every level repeats this same step, so an
            // application only ever has to connect to the outermost (root) menu
            if (auto* found=DropdownMenu_p::findItemRecursive(pimpl->items,nestedId))
            {
                found->isChecked=checked;
            }
            emit itemToggled(nestedId,checked);
        }
    );
    connect(
        sub,
        &DropdownFrame::closeRequested,
        this,
        [this,id](DropdownFrame::CloseReason)
        {
            onSubmenuClosed(id);
        }
    );
    connect(
        sub,
        &DropdownFrame::hidden,
        this,
        [this,id]()
        {
            onSubmenuClosed(id);
        }
    );

    pimpl->submenus[id]=sub;
    return sub;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
