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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

}

#endif // UISE_DESKTOP_CHATMESSAGECALL_HPP
