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

/** @file uise/desktop/chatreaction.hpp
*
*  Declares ChatReaction.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATREACTION_HPP
#define UISE_DESKTOP_CHATREACTION_HPP

#include <memory>
#include <vector>

#include <QString>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class AvatarSource;

/**
 * @brief One user shown on a reaction chip (see ChatReaction::avatars()).
 *
 * A plain, cheaply-copyable value type -- no identity of its own, just enough to hand to
 * AvatarWidget::setAvatarSource()/setAvatarPath()/setAvatarName().
 */
struct UISE_DESKTOP_EXPORT ChatReactionAvatar
{
    QString name;                            //!< For the initials fallback (AvatarWidget::setAvatarName()).
    QString path;                            //!< Local/remote path, if known (AvatarWidget::setAvatarPath()).
    std::shared_ptr<AvatarSource> source;     //!< Async provider, if the host has one; takes priority
                                              //!< over path when both are set (AvatarWidget's own rule).
};

/**
 * @brief How a message's reactions are rendered -- decided once per message from the reaction
 *  set as a whole, never per chip (see ChatMessageReactionsRow::setReactions()).
 */
enum class ChatReactionDisplayMode : uint8_t
{
    Avatars,    //!< Icon + up to 3 small avatars of the users who set it.
    Counts      //!< Icon + a plain numeric count.
};

/**
 * @brief One reaction type set on a chat message, with its current count and (if known) the
 *  users who set it.
 *
 * A plain, cheaply-copyable value type -- no pimpl, modelled after ChatFileItem. id() is this
 * reaction's identity, "<icon id>@<icon pack URI>" (see ChatReactionId::make()); everything else
 * is presentation data the host refreshes wholesale via
 * AbstractChatMessageReactions::setReactions().
 */
class UISE_DESKTOP_EXPORT ChatReaction
{
    public:

        QString id() const
        {
            return m_id;
        }

        void setId(QString id)
        {
            m_id=std::move(id);
        }

        /**
         * @brief Get the icon to paint on the chip.
         * @return The icon set via setIcon(), or null. A null icon is resolved by the host/chip
         *  through ReactionIconPacks::instance().icon(id()) -- see reactioniconpack.hpp -- so a
         *  descriptor that only carries id() (no icon) is a valid, common case, not an error.
         */
        std::shared_ptr<SvgIcon> icon() const
        {
            return m_icon;
        }

        void setIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_icon=std::move(icon);
        }

        size_t count() const noexcept
        {
            return m_count;
        }

        void setCount(size_t count) noexcept
        {
            m_count=count;
        }

        /**
         * @brief Get the earliest users who set this reaction.
         * @return At most 3 entries -- callers (the ledger-backed host, per the reactions task
         *  spec) are expected to already have truncated this; ChatMessageReactionsRow does not
         *  re-truncate it, it only refuses avatar-form display when count() exceeds this list's
         *  practical size (see ChatReactionDisplayMode).
         */
        const std::vector<ChatReactionAvatar>& avatars() const noexcept
        {
            return m_avatars;
        }

        void setAvatars(std::vector<ChatReactionAvatar> avatars)
        {
            m_avatars=std::move(avatars);
        }

        //! Whether the current user is among the users who set this reaction.
        bool isOwn() const noexcept
        {
            return m_own;
        }

        void setOwn(bool enable) noexcept
        {
            m_own=enable;
        }

    private:

        QString m_id;
        std::shared_ptr<SvgIcon> m_icon;
        size_t m_count=0;
        std::vector<ChatReactionAvatar> m_avatars;
        bool m_own=false;
};

using ChatReactions=std::vector<ChatReaction>;

/**
 * @brief Helpers for a reaction's id, "<icon id>@<icon pack URI>" -- see the reactions task
 *  spec's "Full reaction unique ID is constructed as <icon ID>@<icon pack URI>".
 *
 * The pack URI half may be empty, meaning "the default embedded pack" -- iconId() and packUri()
 * both tolerate that (an id with no '@' at all is treated as icon id + empty pack URI) rather
 * than treating it as malformed, since the first iteration ships only that one pack.
 */
namespace ChatReactionId
{

UISE_DESKTOP_EXPORT QString make(const QString& iconId, const QString& packUri);
UISE_DESKTOP_EXPORT QString iconId(const QString& reactionId);
UISE_DESKTOP_EXPORT QString packUri(const QString& reactionId);

}

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::ChatReaction)

#endif // UISE_DESKTOP_CHATREACTION_HPP
