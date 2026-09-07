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

/** @file uise/desktop/abstractchatmessagecall.hpp
*
*  Declares AbstractChatMessageCall.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGECALL_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGECALL_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT AbstractChatMessageCall : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        enum class Status : uint8_t
        {
            Missed,
            Complete,
            Failed,
            Declined
        };

        using AbstractChatMessageBody::AbstractChatMessageBody;

        void setDuration(uint32_t seconds)
        {
            m_duration=seconds;
            updateDuration();
        }

        template <typename StatusT>
        void setStatus(StatusT status)
        {
            setStatus(static_cast<Status>(status));
        }

        void setStatus(Status status)
        {
            m_status=status;
            updateStatus();
        }

        Status status() const noexcept
        {
            return m_status;
        }

        QString selectedText() const override;

        QString formatText() const;
        QString formatDuration() const;

        //! Sets the label text verbatim, bypassing formatText() and therefore bypassing
        //! localization entirely -- prefer setStatus()/setDuration(), which render through
        //! formatText()/formatCallText() and pick up the translated wording. Only appropriate
        //! for a caller that has already localized the text itself.
        virtual void presetText(const QString& text) =0;
        virtual void presetIcon(const QString& icon) =0;

    protected:

        virtual void updateDuration() =0;
        virtual void updateStatus() =0;

    private:

        uint32_t m_duration=0;
        Status m_status=Status::Missed;
};

//! Call-message wording, factored out of AbstractChatMessageCall::formatText()/formatDuration() so
//! a client that only has the status/duration/direction (no widget) can render the identical,
//! identically-localized sentence -- e.g. whitemdesktop's chat-list row and notification popup,
//! which build a plain QString via msgPreviewText() rather than instantiating a chat message body.
//! The tr() strings stay attached to the AbstractChatMessageCall context (Q_OBJECT above), so
//! existing translations (see translations/uise_ru.ts) keep resolving unchanged.
UISE_DESKTOP_EXPORT QString formatCallText(AbstractChatMessageCall::Status status,bool incoming,uint32_t durationSeconds);
UISE_DESKTOP_EXPORT QString formatCallDuration(uint32_t durationSeconds);

}

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGECALL_HPP
