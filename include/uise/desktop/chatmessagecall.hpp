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

/** @file uise/desktop/chatmessagecall.hpp
*
*  Declares ChatMessageCall.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGECALL_HPP
#define UISE_DESKTOP_CHATMESSAGECALL_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessagecall.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageCall_p;

class UISE_DESKTOP_EXPORT ChatMessageCall : public AbstractChatMessageCall
{
    Q_OBJECT

    public:

        ChatMessageCall(QWidget* parent=nullptr);

        ~ChatMessageCall();
        ChatMessageCall(const ChatMessageCall&)=delete;
        ChatMessageCall& operator=(const ChatMessageCall&)=delete;
        ChatMessageCall(ChatMessageCall&&)=delete;
        ChatMessageCall& operator=(ChatMessageCall&&)=delete;

        void presetText(const QString& text) override;
        void presetIcon(const QString& icon) override;

    protected:

        void updateDuration() override;
        void updateStatus() override;

        void updateChatMessage() override;

    private:

        void updateIcon();

        std::unique_ptr<ChatMessageCall_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGECALL_HPP
