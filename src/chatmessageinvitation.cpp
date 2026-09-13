/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chatmessageinvitation.cpp
*
*  Defines ChatMessageInvitation.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QPixmap>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessageinvitation.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*****************************ChatMessageInvitation***************************/

namespace {

std::shared_ptr<SvgIcon> invitationMenuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageInvitationMenu::%1").arg(alias),context);
}

}

//--------------------------------------------------------------------------

class ChatMessageInvitation_p
{
    public:

        QBoxLayout* layout=nullptr;
        WithRoundedImage* invitationIcon=nullptr;
        QLabel* caption=nullptr;
        QLabel* description=nullptr;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;
};

//--------------------------------------------------------------------------

ChatMessageInvitation::ChatMessageInvitation(QWidget* parent)
    : AbstractChatMessageInvitation(parent),
      pimpl(std::make_unique<ChatMessageInvitation_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->invitationIcon=new WithRoundedImage(this);
    pimpl->invitationIcon->setObjectName("invitationIcon");
    pimpl->layout->addWidget(pimpl->invitationIcon);

    auto textLayout=new QVBoxLayout();
    Layout::clear(textLayout);
    pimpl->layout->addLayout(textLayout,1);

    pimpl->caption=new QLabel(this);
    pimpl->caption->setObjectName("caption");
    pimpl->caption->setWordWrap(true);
    textLayout->addWidget(pimpl->caption);

    pimpl->description=new QLabel(this);
    pimpl->description->setObjectName("description");
    pimpl->description->setWordWrap(true);
    textLayout->addWidget(pimpl->description);

    pimpl->menuButton=new IconTextButton(
        invitationMenuIcon(QStringLiteral("menu"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    pimpl->menuButton->setCursor(Qt::PointingHandCursor);
    pimpl->layout->addWidget(pimpl->menuButton);

    // DropdownMenu is constructed parentless -- same ChatMessageFileItem precedent
    // (thirdparty/uise-desktop/src/chatmessagefileitem.cpp): DropdownFrame reparents itself
    // lazily to the trigger's actual window() on first opening.
    pimpl->menu=new DropdownMenu();
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menuButton,&IconTextButton::clicked,this,&ChatMessageInvitation::onMenuButtonClicked);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatMessageInvitation::onMenuItemTriggered);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageInvitation::~ChatMessageInvitation()
{
    if (!pimpl->menu.isNull())
    {
        destroyWidget(pimpl->menu);
    }
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::onMenuButtonClicked()
{
    // Deferred build -- same ChatMessageFileItem::rebuildMenu()/menuDirty precedent: menuItems()
    // does an svg-icon lookup per entry, only worth paying the moment the menu is actually opened.
    if (menuDirty())
    {
        pimpl->menu->setItems(menuItems(this));
        clearMenuDirty();
    }
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::onMenuItemTriggered(int id)
{
    emit menuActionTriggered(id);
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateCaption()
{
    pimpl->caption->setText(formatCaption());
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateKind()
{
    updateIcon();
    updateCaption();
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateState()
{
    // State text takes priority on the description line when it says something (Protected/
    // Expired/Accepted/Unsupported); Available (empty state text) falls through to the identity
    // line instead -- see updateIdentityText()'s own body, called here too so a state change
    // alone (identityText() unchanged) still re-evaluates which of the two should show.
    updateIdentityText();
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateIdentityText()
{
    auto state=formatStateText();
    pimpl->description->setText(state.isEmpty() ? identityText() : state);
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateAvatar()
{
    updateIcon();
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateChatMessage()
{
    // Mirrors ChatMessageCall/ChatMessageError's own updateChatMessage(): a setter may have run
    // before setWidgets() attached this body to its chat message, so re-render everything once it
    // has.
    updateIcon();
    updateCaption();
    updateIdentityText();
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::updateIcon()
{
    auto img=avatarImage();
    if (!img.isNull())
    {
        pimpl->invitationIcon->image()->setPixmap(QPixmap::fromImage(img));
        return;
    }

    QString icon;
    switch (kind())
    {
        case (Kind::Contact):
            icon="InvitationMsgIcon::contact";
            break;
        case (Kind::GroupChat):
            icon="InvitationMsgIcon::group";
            break;
        case (Kind::Unknown):
            icon="InvitationMsgIcon::unknown";
            break;
    }
    pimpl->invitationIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon(icon,this));
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::presetIdentityText(const QString& text)
{
    pimpl->description->setText(text);
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::presetIcon(const QString& icon)
{
    pimpl->invitationIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon(icon,this));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
