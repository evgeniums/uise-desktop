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

/** @file uise/desktop/chatmessagefiles.hpp
*
*  Declares ChatMessageFiles.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEFILES_HPP
#define UISE_DESKTOP_CHATMESSAGEFILES_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessagefiles.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageFiles_p;

/**
 * @brief Concrete file chat message body: one ChatMessageFileItem row per item, stacked
 *  vertically, followed by an embedded ChatMessageText used as the optional comment -- reusing
 *  its markdown loading, native selection and bubble-width negotiation rather than reimplementing
 *  any of that.
 */
class UISE_DESKTOP_EXPORT ChatMessageFiles : public AbstractChatMessageFiles
{
    Q_OBJECT

    public:

        explicit ChatMessageFiles(QWidget* parent=nullptr);

        ~ChatMessageFiles();

        ChatMessageFiles(const ChatMessageFiles&)=delete;
        ChatMessageFiles(ChatMessageFiles&&)=delete;
        ChatMessageFiles& operator=(const ChatMessageFiles&)=delete;
        ChatMessageFiles& operator=(ChatMessageFiles&&)=delete;

        void setItems(ChatFileItems items) override;
        const ChatFileItems& items() const override;
        void updateItem(const QUuid& id, const ChatFileItem& item) override;

        void setComment(const QString& text, bool markdown=true) override;
        void clearComment() override;
        QString comment() const override;

        void closeMenus() override;

        void setTextVerticalAlignment(Qt::Alignment alignment) override;
        Qt::Alignment textVerticalAlignment() const override;

        void clearContentSelection() override;

        QString selectedText() const override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

    protected:

        void updateChatMessage() override;

    private:

        void rebuildList();

        std::unique_ptr<ChatMessageFiles_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEFILES_HPP
