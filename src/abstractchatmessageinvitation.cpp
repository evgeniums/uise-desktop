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

/** @file uise/desktop/abstractchatmessageinvitation.cpp
*
*  Defines AbstractChatMessageInvitation.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractchatmessageinvitation.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

std::shared_ptr<SvgIcon> invitationMenuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageInvitationMenu::%1").arg(alias),context);
}

}

//--------------------------------------------------------------------------

QString AbstractChatMessageInvitation::selectedText() const
{
    QString text=formatCaption();
    auto identity=identityText();
    if (!identity.isEmpty())
    {
        text+=QLatin1Char('\n')+identity;
    }
    auto state=formatStateText();
    if (!state.isEmpty())
    {
        text+=QLatin1Char('\n')+state;
    }
    return text;
}

//--------------------------------------------------------------------------

QString AbstractChatMessageInvitation::formatCaption() const
{
    // Same fixed wording ChatInvitationFileItem::refresh() already uses for the legacy .inv-file
    // row (ui/chatinvitationfileitem.cpp) -- a native invitation message's name-line states what
    // the attachment IS, same as that row's setNameText(tr("Contact invitation")), rather than
    // repeating the identity line (that lives on the description line below it).
    return formatInvitationHeadline(kind());
}

//--------------------------------------------------------------------------

QString AbstractChatMessageInvitation::formatStateText() const
{
    switch (state())
    {
        case (State::Available):
            return QString{};
        case (State::Protected):
            return tr("Password required");
        case (State::Expired):
            return tr("Expired");
        case (State::Accepted):
            return tr("Already in contacts");
        case (State::Unsupported):
            return tr("Update the application to open this invitation");
    }
    return QString{};
}

//--------------------------------------------------------------------------

QString AbstractChatMessageInvitation::formatActionText() const
{
    switch (kind())
    {
        case (Kind::Contact):
            return tr("Add contact");
        case (Kind::GroupChat):
            return tr("Join chat");
        case (Kind::Unknown):
            break;
    }
    return tr("Open");
}

//--------------------------------------------------------------------------

std::vector<MenuItem> AbstractChatMessageInvitation::menuItems(QWidget* iconContext) const
{
    std::vector<MenuItem> items;

    // AddContact/ShowQrCode need a usable invitation to act on -- hidden (not just disabled) for
    // Expired/Unsupported, same gate the previous single-action-button design applied.
    bool actionable=state()!=State::Expired && state()!=State::Unsupported;
    if (actionable)
    {
        items.push_back(MenuItem(static_cast<int>(MenuAction::AddContact),formatActionText(),
                                 invitationMenuIcon(QStringLiteral("addContact"),iconContext)));
        items.push_back(MenuItem(static_cast<int>(MenuAction::ShowQrCode),tr("Show QR code"),
                                 invitationMenuIcon(QStringLiteral("qrcode"),iconContext)));
    }

    // CopyUsername/CopyTemporaryCode/SaveAsFile stay available regardless of state -- even an
    // expired or unsupported invitation's raw bytes/identity strings are still worth keeping.
    if (hasUsername())
    {
        items.push_back(MenuItem(static_cast<int>(MenuAction::CopyUsername),tr("Copy username"),
                                 invitationMenuIcon(QStringLiteral("copy"),iconContext)));
    }
    if (hasTemporaryCode())
    {
        items.push_back(MenuItem(static_cast<int>(MenuAction::CopyTemporaryCode),tr("Copy temporary code"),
                                 invitationMenuIcon(QStringLiteral("copy"),iconContext)));
    }
    items.push_back(MenuItem(static_cast<int>(MenuAction::SaveAsFile),tr("Save as file"),
                             invitationMenuIcon(QStringLiteral("save"),iconContext)));

    return items;
}

//--------------------------------------------------------------------------

// Free-function twin of the kind-based fallback wording used for formatCaption()/
// updateIdentityText()'s own fallback (see the doc comment on the declaration in
// abstractchatmessageinvitation.hpp): same tr() calls, same AbstractChatMessageInvitation
// context, so existing translations keep resolving, but usable without a widget instance.

QString formatInvitationHeadline(AbstractChatMessageInvitation::Kind kind)
{
    using Kind=AbstractChatMessageInvitation::Kind;

    switch (kind)
    {
        case (Kind::Contact):
            return AbstractChatMessageInvitation::tr("Contact invitation");
        case (Kind::GroupChat):
            return AbstractChatMessageInvitation::tr("Group chat invitation");
        case (Kind::Unknown):
            break;
    }

    return AbstractChatMessageInvitation::tr("Invitation");
}

//--------------------------------------------------------------------------

}
