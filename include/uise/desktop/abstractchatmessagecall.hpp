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

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AbstractChatMessageCall : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        enum class Status : uint8_t
        {
            Missed,
            Complete,
            Failed
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

    protected:

        virtual void updateDuration() =0;
        virtual void updateStatus() =0;

    private:

        uint32_t m_duration=0;
        Status m_status=Status::Missed;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGECALL_HPP
