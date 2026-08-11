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

/** @file uise/desktop/src/loadcontrolmenu.cpp
*
*  Defines LoadControlMenu.
*
*/

/****************************************************************************/

#include <QCoreApplication>
#include <QFontMetrics>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/loadcontrol.hpp>
#include <uise/desktop/loadcontrolmenu.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Local, private to this widget -- not ChatFileMenuAction, which LoadControlMenu (a generic,
//! non-chat-specific widget) must not depend on.
enum class MenuAction : int
{
    Pause=1,
    Cancel=2
};

// Conservative budget for the filename portion of the Cancel entry's text -- DropdownMenu rows
// have no max-width of their own (see dropdownmenu.qss), so an unelided long filename would
// otherwise stretch the popup arbitrarily wide.
constexpr int MaxFileNameTextWidth=160;

std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("LoadControl::%1").arg(alias),context);
}

}

//--------------------------------------------------------------------------

class LoadControlMenu_p
{
    public:

        LoadControl* loadControl=nullptr;
        QPointer<DropdownMenu> menu;

        QString fileName;
        bool isImage=false;
        bool incoming=false;
};

//--------------------------------------------------------------------------

LoadControlMenu::LoadControlMenu(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<LoadControlMenu_p>())
{
    auto layout=Layout::vertical(this);

    pimpl->loadControl=new LoadControl(this);
    pimpl->loadControl->setObjectName("loadControl");
    layout->addWidget(pimpl->loadControl);
    connect(pimpl->loadControl,&LoadControl::clicked,this,&LoadControlMenu::onLoadControlClicked);

    // DropdownMenu is constructed parentless, like every other per-item menu in this library --
    // DropdownFrame reparents itself lazily to the trigger's actual window() on first opening,
    // so constructing it with a parent this early would just capture the wrong window. Not
    // attachTo()'d: that would auto-toggle the menu on every click regardless of state, but this
    // menu should only ever open from onLoadControlClicked()'s own Running-state check --
    // setTriggerWidget() alone still gives correct anchoring and outside-click/re-click handling.
    pimpl->menu=new DropdownMenu();
    pimpl->menu->setTriggerWidget(pimpl->loadControl);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&LoadControlMenu::onMenuItemTriggered);
    connect(pimpl->menu,&DropdownFrame::closeRequested,this,&LoadControlMenu::onMenuCloseRequested);
}

//--------------------------------------------------------------------------

LoadControlMenu::~LoadControlMenu()
{
    if (!pimpl->menu.isNull())
    {
        destroyWidget(pimpl->menu);
    }
}

//--------------------------------------------------------------------------

LoadControl* LoadControlMenu::loadControl() const
{
    return pimpl->loadControl;
}

//--------------------------------------------------------------------------

void LoadControlMenu::setState(AbstractLoadControl::State state)
{
    pimpl->loadControl->setState(state);
}

//--------------------------------------------------------------------------

AbstractLoadControl::State LoadControlMenu::state() const
{
    return pimpl->loadControl->state();
}

//--------------------------------------------------------------------------

void LoadControlMenu::setProgress(qreal value)
{
    pimpl->loadControl->setProgress(value);
}

//--------------------------------------------------------------------------

void LoadControlMenu::setProgressImpl(qreal currentValue, qreal total)
{
    // both already qreal, so this just instantiates AbstractLoadControl::setProgress<qreal,qreal>
    // -- reuses its existing zero-total/overflow clamping rather than duplicating it here
    pimpl->loadControl->setProgress(currentValue,total);
}

//--------------------------------------------------------------------------

void LoadControlMenu::setFileDescription(const QString& fileName, bool isImage, bool incoming)
{
    pimpl->fileName=fileName;
    pimpl->isImage=isImage;
    pimpl->incoming=incoming;
}

//--------------------------------------------------------------------------

void LoadControlMenu::rebuildMenuItems()
{
    // "Pause"/"Cancel" alone, paired with their icons, could later be misread as media-playback
    // controls once the app grows an inline player for audio/video messages -- naming the
    // transfer explicitly (and, for Cancel, the file too) keeps both entries unambiguous
    // regardless of what else might be clickable nearby.
    auto pauseText=pimpl->incoming
        ? QCoreApplication::translate("LoadControlMenu","Pause downloading")
        : QCoreApplication::translate("LoadControlMenu","Pause sending");

    auto cancelVerb=pimpl->incoming
        ? QCoreApplication::translate("LoadControlMenu","Cancel downloading")
        : QCoreApplication::translate("LoadControlMenu","Cancel sending");

    QString subject;
    if (pimpl->fileName.isEmpty())
    {
        subject=pimpl->isImage
            ? QCoreApplication::translate("LoadControlMenu","this image")
            : QCoreApplication::translate("LoadControlMenu","this file");
    }
    else
    {
        QFontMetrics fm(font());
        subject=fm.elidedText(pimpl->fileName,Qt::ElideMiddle,MaxFileNameTextWidth);
    }

    std::vector<MenuItem> items;
    items.push_back(MenuItem(
        static_cast<int>(MenuAction::Pause),
        pauseText,
        menuIcon(QStringLiteral("pause"),this)
    ));
    items.push_back(MenuItem(
        static_cast<int>(MenuAction::Cancel),
        QStringLiteral("%1 %2").arg(cancelVerb,subject),
        menuIcon(QStringLiteral("cancel"),this)
    ));
    pimpl->menu->setItems(std::move(items));
}

//--------------------------------------------------------------------------

void LoadControlMenu::onLoadControlClicked()
{
    // Waiting is treated exactly like Running: a queued-but-not-yet-started transfer is just
    // as pausable (pausing keeps the scheduler from picking it up) and just as cancellable as
    // one already in flight -- isChatFileCancellable()/buildChatFileMenuItems() already offer
    // both for it in the overflow menu, so without this the control would be the one place
    // that silently ignores a click in that state.
    auto state=pimpl->loadControl->state();
    if (state!=AbstractLoadControl::State::Running && state!=AbstractLoadControl::State::Waiting)
    {
        emit clicked();
        return;
    }

    emit pauseRequested();

    rebuildMenuItems();
    pimpl->menu->popupBelow(pimpl->loadControl);
}

//--------------------------------------------------------------------------

void LoadControlMenu::onMenuItemTriggered(int id)
{
    if (static_cast<MenuAction>(id)==MenuAction::Cancel)
    {
        emit cancelRequested();
    }
    // Pause: nothing further -- the transfer was already paused by the click that opened this
    // menu, so picking this item just confirms staying paused. Escape/an outside click leave it
    // paused too -- see the class doc for why there's no silent-resume path for those -- so this
    // function has nothing to do for either case.
}

//--------------------------------------------------------------------------

void LoadControlMenu::onMenuCloseRequested(DropdownFrame::CloseReason reason)
{
    // Every other close reason is a dismissal the user didn't direct at the control itself (see
    // the class doc for why those never resume). A second click on the control while its own
    // menu is open is different -- DropdownFrame consumes that press to close the menu rather
    // than letting it reach LoadControl::mousePressEvent(), so LoadControl::clicked() never
    // fires for it on its own; surfacing it here as an ordinary clicked() is what makes "click
    // the control again" behave the same whether the menu happened to still be open or not.
    if (reason==DropdownFrame::CloseReason::TriggerClick)
    {
        emit clicked();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
