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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

        void setComment(const QString& text, TextFormat format=TextFormat::Markdown) override;
        void clearComment() override;
        QString comment() const override;

        void closeMenus() override;

        void setTextVerticalAlignment(Qt::Alignment alignment) override;
        Qt::Alignment textVerticalAlignment() const override;

        void startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag) override;

        void clearContentSelection() override;

        QString selectedText() const override;

        bool hasSelectableText() const override;

        void setCopyable(bool enable) override;

        void setOwnContextMenuEnabled(bool enable) override;

        void selectText(const QString& text) override;

        QString linkAt(const QPoint& pos) const override;

        QUuid fileItemAt(const QPoint& pos) const override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

    protected:

        void updateChatMessage() override;

    private:

        void rebuildList();

        std::unique_ptr<ChatMessageFiles_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGEFILES_HPP
