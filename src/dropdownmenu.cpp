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

#include <QFrame>
#include <QBoxLayout>
#include <QLayout>
#include <QSignalBlocker>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
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

        MenuItem* findItem(int id)
        {
            for (auto& item : items)
            {
                if (item.id==id)
                {
                    return &item;
                }
            }
            return nullptr;
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
}

//--------------------------------------------------------------------------

const std::vector<MenuItem>& DropdownMenu::items() const
{
    return pimpl->items;
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemChecked(int id, bool checked)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr || !item->isCheckable)
    {
        return;
    }
    item->isChecked=checked;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        QSignalBlocker b(it->second);
        it->second->setChecked(checked);
        it->second->setTrailingSvgIcon(checked ? pimpl->checkIcon : nullptr);
    }
}

//--------------------------------------------------------------------------

bool DropdownMenu::isItemChecked(int id) const
{
    auto* item=pimpl->findItem(id);
    return item!=nullptr && item->isChecked;
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemEnabled(int id, bool enable)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr)
    {
        return;
    }
    item->isEnabled=enable;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        it->second->setEnabled(enable);
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemVisible(int id, bool visible)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr)
    {
        return;
    }
    item->isVisible=visible;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        it->second->setVisible(visible);
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemText(int id, const QString& text)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr)
    {
        return;
    }
    item->text=text;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        it->second->setText(text);
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::setItemIcon(int id, std::shared_ptr<SvgIcon> icon)
{
    auto* item=pimpl->findItem(id);
    if (item==nullptr)
    {
        return;
    }
    item->icon=icon;

    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end() && !it->second.isNull())
    {
        it->second->setSvgIcon(icon);
    }
}

//--------------------------------------------------------------------------

IconTextButton* DropdownMenu::itemButton(int id) const
{
    auto it=pimpl->buttons.find(id);
    return (it!=pimpl->buttons.end()) ? it->second.data() : nullptr;
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
        notifyActivated(itemButton(id));
    }
}

//--------------------------------------------------------------------------

void DropdownMenu::fillContent()
{
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
        btn->setCheckable(item.isCheckable);

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
        firstRow=false;

        // btn's own polish pass (see the comment above) covers btn itself and its #text child,
        // but not its RippleOverlay: that child was already polished inside IconTextButton's
        // constructor, by RippleOverlay::install(), BEFORE section/firstItem were set above --
        // same ordering gap as iconOnly/avatarOnly (see icontextbutton.cpp's constructor
        // comment). Without this, ripple.qss's [section="true"]/[firstItem="true"]
        // rippleInsetTop rules never take effect and the row's ripple bleeds into its top margin.
        Style::updateWidgetStyle(btn->rippleOverlay());

        auto id=item.id;
        if (item.isCheckable)
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
                    notifyActivated(itemButton(id));
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
    pimpl->clearRows();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
