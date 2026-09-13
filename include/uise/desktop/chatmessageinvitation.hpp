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

/** @file uise/desktop/chatmessageinvitation.hpp
*
*  Declares ChatMessageInvitation.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEINVITATION_HPP
#define UISE_DESKTOP_CHATMESSAGEINVITATION_HPP

#include <memory>

#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessageinvitation.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatMessageInvitation_p;
class DropdownMenu;

class UISE_DESKTOP_EXPORT ChatMessageInvitation : public AbstractChatMessageInvitation
{
    Q_OBJECT

    public:

        ChatMessageInvitation(QWidget* parent=nullptr);

        ~ChatMessageInvitation();
        ChatMessageInvitation(const ChatMessageInvitation&)=delete;
        ChatMessageInvitation& operator=(const ChatMessageInvitation&)=delete;
        ChatMessageInvitation(ChatMessageInvitation&&)=delete;
        ChatMessageInvitation& operator=(ChatMessageInvitation&&)=delete;

        void presetIdentityText(const QString& text) override;
        void presetIcon(const QString& icon) override;

        //! Styled after ChatMessageFileItem's row -- no single trailing line for the bubble's
        //! bottom row to share (default null rect => the bottom row gets its own line below),
        //! same as ChatMessageError.

    protected:

        void updateKind() override;
        void updateState() override;
        void updateIdentityText() override;
        void updateAvatar() override;

        void updateChatMessage() override;

    private slots:

        void onMenuButtonClicked();
        void onMenuItemTriggered(int id);

    private:

        void updateIcon();
        void updateCaption();

        std::unique_ptr<ChatMessageInvitation_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGEINVITATION_HPP
