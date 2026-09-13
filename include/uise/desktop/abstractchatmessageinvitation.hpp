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

/** @file uise/desktop/abstractchatmessageinvitation.hpp
*
*  Declares AbstractChatMessageInvitation.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGEINVITATION_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGEINVITATION_HPP

#include <vector>

#include <QImage>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/dropdownmenu.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//! Body of a chat message carrying a shared invitation (task-invitation-message.md). Styled after
//! ChatMessageFileItem's row (icon slot + two-line text column + a drop-down menu button), not
//! AbstractChatMessageCall's single line -- an invitation is closer in spirit to an attachment
//! than to a system record like a call.
class UISE_DESKTOP_EXPORT AbstractChatMessageInvitation : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        //! Presentation category only -- this library must not know "character", only that some
        //! invitations are for a 1:1 contact and others (reserved, task-basic-group-chats.md) are
        //! for a group chat. Unknown is the safe fallback for any future kind this build doesn't
        //! recognize yet (still renders a card, just with generic wording/icon).
        enum class Kind : uint8_t
        {
            Contact,
            GroupChat,
            Unknown
        };

        enum class State : uint8_t
        {
            Available,
            Protected,
            Expired,
            Accepted,
            Unsupported
        };

        //! Ids of the drop-down menu entries -- same "shared enum, host dispatches uniformly"
        //! precedent as ChatFileMenuAction (chatfileitem.hpp). Starts at 1, not 0 (DropdownMenu's
        //! own MenuItem::id default is -1, but 0 is reserved by convention across this library's
        //! other per-item menus).
        enum class MenuAction
        {
            AddContact=1,
            ShowQrCode=2,
            CopyUsername=3,
            CopyTemporaryCode=4,
            SaveAsFile=5
        };

        using AbstractChatMessageBody::AbstractChatMessageBody;

        template <typename KindT>
        void setKind(KindT kind)
        {
            setKind(static_cast<Kind>(kind));
        }

        void setKind(Kind kind)
        {
            m_kind=kind;
            updateKind();
        }

        Kind kind() const noexcept
        {
            return m_kind;
        }

        template <typename StateT>
        void setState(StateT state)
        {
            setState(static_cast<State>(state));
        }

        void setState(State state)
        {
            m_state=state;
            updateState();
        }

        State state() const noexcept
        {
            return m_state;
        }

        //! The single resolved identity/description line, already fully formatted by the HOST --
        //! a title, a username formatted+domain-trimmed against the VIEWER's own domain, or a
        //! temporary code. An empty string is the host's signal "I have nothing to show" -- this
        //! class then falls back to formatInvitationHeadline(kind()) itself, so a chat-list
        //! preview built via msgPreviewText() (no widget instance) reproduces the identical
        //! wording without duplicating this fallback logic.
        void setIdentityText(const QString& text)
        {
            m_identityText=text;
            updateIdentityText();
        }

        QString identityText() const
        {
            return m_identityText;
        }

        //! Whether the drop-down menu offers "Copy username"/"Copy temporary code" -- the actual
        //! values are never given to this library (it only emits the action id; the host owns
        //! the clipboard write, same split as ChatMessageFileItem's menu leaving Open/SaveAs
        //! execution to its host).
        void setHasUsername(bool has)
        {
            m_hasUsername=has;
            m_menuDirty=true;
        }

        bool hasUsername() const noexcept
        {
            return m_hasUsername;
        }

        void setHasTemporaryCode(bool has)
        {
            m_hasTemporaryCode=has;
            m_menuDirty=true;
        }

        bool hasTemporaryCode() const noexcept
        {
            return m_hasTemporaryCode;
        }

        //! Decoded avatar pixmap, or a null QImage -- the HOST decodes chat_msg_invitation_data::
        //! avatar_thumbnail itself (its own mime field says how); this class stays
        //! image-format-agnostic and falls back to a per-kind icon when null.
        void setAvatarImage(const QImage& image)
        {
            m_avatarImage=image;
            updateAvatar();
        }

        QImage avatarImage() const
        {
            return m_avatarImage;
        }

        QString selectedText() const override;

        //! Fixed caption -- "Contact invitation" / "Group chat invitation" -- shown on the
        //! name-line regardless of identityText(), same role ChatInvitationFileItem's own
        //! setNameText(tr("Contact invitation")) already plays for the legacy .inv-file row.
        QString formatCaption() const;

        QString formatStateText() const;

        //! "Add contact" / "Join chat" -- the MenuAction::AddContact entry's own label.
        QString formatActionText() const;

        //! Sets the identity text verbatim, bypassing setIdentityText()'s own kind-fallback --
        //! only appropriate for a caller that has already resolved/localized the text itself.
        virtual void presetIdentityText(const QString& text) =0;
        virtual void presetIcon(const QString& icon) =0;

    protected:

        virtual void updateKind() =0;
        virtual void updateState() =0;
        virtual void updateIdentityText() =0;
        virtual void updateAvatar() =0;

        //! Menu items for the CURRENT kind/state/hasUsername/hasTemporaryCode -- built once per
        //! menuDirty()-guarded open by the concrete widget's own menu button handler, same
        //! deferred-build precedent as ChatMessageFileItem::rebuildMenu()'s own doc comment.
        std::vector<MenuItem> menuItems(QWidget* iconContext) const;

        bool menuDirty() const noexcept
        {
            return m_menuDirty;
        }

        void clearMenuDirty() noexcept
        {
            m_menuDirty=false;
        }

    signals:

        //! Emitted when a drop-down menu entry is triggered -- \p action is one of MenuAction.
        //! The host (whitemdesktop) owns everything past this point: see
        //! ui::ChatMessage::onInvitationMenuAction() and
        //! TreeController::openAddContactWithInvitation() /
        //! InvitationMsgDataHelper's own username()/content() accessors for CopyUsername/
        //! CopyTemporaryCode/SaveAsFile.
        void menuActionTriggered(int action);

    private:

        Kind m_kind=Kind::Contact;
        State m_state=State::Available;
        QString m_identityText;
        QImage m_avatarImage;
        bool m_hasUsername=false;
        bool m_hasTemporaryCode=false;
        mutable bool m_menuDirty=true;
};

//! Kind-based fallback wording ("Contact invitation" / "Group chat invitation" / a generic
//! "Invitation" for Kind::Unknown), factored out so a client that only has the kind (no widget)
//! can render the identical, identically-localized sentence -- e.g. whitemdesktop's chat-list row
//! and notification popup, which build a plain QString via msgPreviewText() rather than
//! instantiating a chat message body. Also this class's own formatCaption()/updateIdentityText()
//! fallback. The tr() strings stay attached to the AbstractChatMessageInvitation context (Q_OBJECT
//! above), so translations (see translations/uise_ru.ts) resolve consistently.
UISE_DESKTOP_EXPORT QString formatInvitationHeadline(AbstractChatMessageInvitation::Kind kind);

//! "Add contact" / "Join chat" -- widget-free twin of
//! AbstractChatMessageInvitation::formatActionText(), which delegates to it. Same reason
//! formatInvitationHeadline() above is a free function.
UISE_DESKTOP_EXPORT QString formatInvitationActionText(AbstractChatMessageInvitation::Kind kind);

//! Whether the card's primary action ("Add contact"/"Join chat") can be performed at all -- false
//! for an Expired or Unsupported invitation, which has nothing usable to act on. Single definition
//! shared by the menu builder below (which hides AddContact/ShowQrCode when this is false) and by
//! the concrete card's own click handling (ChatMessageInvitation::mouseReleaseEvent(), which must
//! not open a card whose menu would not have offered the action either).
UISE_DESKTOP_EXPORT bool isInvitationActionable(AbstractChatMessageInvitation::State state);

//! The card's action set for a given state, shared by BOTH the card's own drop-down
//! (AbstractChatMessageInvitation::menuItems(), which delegates here) and a host's right-click
//! submenu over the same card -- the identical "one builder, two call sites" split
//! buildChatFileMenuItems() (chatfileitem.hpp) already uses for file rows, and for the same
//! reason: neither menu can then offer a different action set than the other for the same card.
//! Ids are MenuAction values; a host nesting these under a submenu is expected to offset them
//! into its own id space and unwind that offset when dispatching.
UISE_DESKTOP_EXPORT std::vector<MenuItem> buildInvitationMenuItems(
    AbstractChatMessageInvitation::Kind kind,
    AbstractChatMessageInvitation::State state,
    bool hasUsername,
    bool hasTemporaryCode,
    QWidget* iconContext
);

}

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGEINVITATION_HPP
