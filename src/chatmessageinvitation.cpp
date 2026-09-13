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

#include <algorithm>

#include <QPixmap>
#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessageinvitation.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/elidedlabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*****************************ChatMessageInvitation***************************/

namespace {

//! Gap between the caption and description lines -- same value ChatMessageFileItem uses between
//! its own nameLabel and infoLabel, so an invitation card and a file row space their two lines
//! identically.
constexpr int TextLineSpacing=4;

//! Hard cap on the bubble width of an invitation message, mirroring
//! AbstractChatMessageFiles::DefaultMaxBubbleWidth (and chat.qss's own cap on a text bubble): the
//! card's width otherwise tracks its title's full unelided length. See bubbleWidthHint().
constexpr int MaxBubbleWidth=600;

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

        // ElidedLabel, NOT a word-wrapped QLabel: a QLabel with setWordWrap(true) reports a
        // deliberately compact sizeHint() (Qt picks a readable block width rather than the text's
        // full single-line width), and the default bubbleWidthHint() just forwards that -- so the
        // bubble asked for far less width than one line needs and the caption wrapped even with
        // room to spare beside it. ElidedLabel reports the full single-line width instead, and
        // elides rather than wrapping when the bubble genuinely cannot grow that far. Same widget
        // and same reasoning as ChatMessageFileItem's own nameLabel.
        ElidedLabel* caption=nullptr;
        ElidedLabel* description=nullptr;

        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;

        //! Set by mousePressEvent() so mouseReleaseEvent() only treats a release as a click when
        //! the press that started it landed on this card too.
        bool pressed=false;
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

    // Stretches bracketing the two labels, and an explicit gap between them -- copied from
    // ChatMessageFileItem's own text column for the reason its constructor spells out: this
    // column's height follows the row's (driven by the icon, well beyond what two lines of text
    // need), and a plain QVBoxLayout with neither stretch hands that leftover space to the
    // labels themselves (both default to a Preferred vertical size policy). Each label then
    // centers its text inside its own inflated cell, so the caption drifts up and the
    // description down, leaving a visibly bigger gap between them than the file row has.
    textLayout->addStretch(1);

    pimpl->caption=new ElidedLabel(this);
    pimpl->caption->setObjectName("caption");
    pimpl->caption->setElideMode(Qt::ElideRight);
    textLayout->addWidget(pimpl->caption);

    textLayout->addSpacing(TextLineSpacing);

    pimpl->description=new ElidedLabel(this);
    pimpl->description->setObjectName("description");
    // ElideMiddle, like the file row's own name line: this line carries a title, a username or a
    // "#code@domain", all of which have a meaningful tail worth keeping visible.
    pimpl->description->setElideMode(Qt::ElideMiddle);
    textLayout->addWidget(pimpl->description);

    textLayout->addStretch(1);

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

    // Connected BEFORE attachTo() below, which wires its own clicked handler that actually opens
    // the dropdown -- Qt invokes same-signal slots in connection order, so this one must run
    // first to have the items in place by the time the popup is filled and measured. With the
    // two connects the other way round the first click popped up an empty, tiny square (the
    // items only landed afterwards, so it took a second click to show a real menu) -- the same
    // trap ChatMessageFileItem's own constructor documents at length.
    connect(pimpl->menuButton,&IconTextButton::clicked,this,&ChatMessageInvitation::onMenuButtonClicked);

    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatMessageInvitation::onMenuItemTriggered);

    // Matches the default State::Available -- kept in sync from updateState() on every change
    // after that, so a card that is never given a state still looks clickable, which it is.
    setCursor(Qt::PointingHandCursor);

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

    // Only a card that can actually be acted on advertises itself as clickable -- the same gate
    // mouseReleaseEvent() applies before emitting.
    setCursor(isInvitationActionable(state()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
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

int ChatMessageInvitation::bubbleWidthHint(int forMaxWidth)
{
    // sizeHint() is the icon slot + the wider of the two ElidedLabels' FULL single-line text +
    // the menu button. Clamped by this card's own cap first and the negotiation's budget second;
    // whichever bites, the labels elide into whatever width the bubble ends up with.
    auto capped=std::min(forMaxWidth,MaxBubbleWidth);
    return std::min(sizeHint().width(),capped);
}

//--------------------------------------------------------------------------

int ChatMessageInvitation::ownWidthCeiling() const
{
    return MaxBubbleWidth;
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && isInvitationActionable(state()))
    {
        pimpl->pressed=true;
        event->accept();
        return;
    }
    AbstractChatMessageInvitation::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageInvitation::mouseReleaseEvent(QMouseEvent* event)
{
    if (pimpl->pressed && event->button()==Qt::LeftButton)
    {
        pimpl->pressed=false;

        // Release must land back on the card -- dragging off it and letting go is a cancelled
        // click, the same way any push button behaves.
        if (rect().contains(event->pos()) && isInvitationActionable(state()))
        {
            emit menuActionTriggered(static_cast<int>(MenuAction::AddContact));
        }
        event->accept();
        return;
    }
    AbstractChatMessageInvitation::mouseReleaseEvent(event);
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
