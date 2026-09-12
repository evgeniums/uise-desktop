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

/** @file uise/desktop/abstractchatmessageerror.hpp
*
*  Declares AbstractChatMessageError.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGEERROR_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGEERROR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//! Body of a chat message that this client cannot correctly render: an unknown msg_type, a known
//! type at a newer version than this build supports, content over the accepted length, or content
//! that failed to deserialize. Rendered instead of the normal type-specific body -- the message
//! really was sent, this is not an app failure, so styling is muted/informational rather than a
//! scary full-red error block.
class UISE_DESKTOP_EXPORT AbstractChatMessageError : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        enum class Reason : uint8_t
        {
            UnknownType,
            NeedsNewerVersion,
            TooLarge,
            Unreadable
        };

        using AbstractChatMessageBody::AbstractChatMessageBody;

        template <typename ReasonT>
        void setReason(ReasonT reason)
        {
            setReason(static_cast<Reason>(reason));
        }

        void setReason(Reason reason)
        {
            m_reason=reason;
            updateReason();
        }

        Reason reason() const noexcept
        {
            return m_reason;
        }

        QString selectedText() const override;

        QString formatTitle() const;
        QString formatDescription() const;

        //! Sets the title/description verbatim, bypassing formatTitle()/formatDescription() and
        //! therefore bypassing localization entirely -- prefer setReason(), which renders through
        //! formatTitle()/formatDescription() and picks up the translated wording. Only appropriate
        //! for a caller that has already localized the text itself.
        virtual void presetTitle(const QString& text) =0;
        virtual void presetDescription(const QString& text) =0;
        virtual void presetIcon(const QString& icon) =0;

    protected:

        virtual void updateReason() =0;

    private:

        Reason m_reason=Reason::UnknownType;
};

//! Error wording, factored out of AbstractChatMessageError::formatTitle()/formatDescription() so
//! a client that only has the reason (no widget) can render the identical, identically-localized
//! sentence -- e.g. whitemdesktop's chat-list row and notification popup, which build a plain
//! QString via msgPreviewText() rather than instantiating a chat message body. The tr() strings
//! stay attached to the AbstractChatMessageError context (Q_OBJECT above), so translations
//! (see translations/uise_ru.ts) keep resolving unchanged.
UISE_DESKTOP_EXPORT QString formatMessageErrorTitle(AbstractChatMessageError::Reason reason);
UISE_DESKTOP_EXPORT QString formatMessageErrorDescription(AbstractChatMessageError::Reason reason);

}

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGEERROR_HPP
