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

/** @file uise/desktop/chatmessage.hpp
*
*  Declares ChatMessage.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGE_HPP
#define UISE_DESKTOP_CHATMESSAGE_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

class QBoxLayout;

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatSeparatorSection_p;

class UISE_DESKTOP_EXPORT ChatSeparatorSection : public AbstractChatSeparatorSection
{
    Q_OBJECT

    public:

        ChatSeparatorSection(QWidget* parent=nullptr);

        ~ChatSeparatorSection();
        ChatSeparatorSection(const ChatSeparatorSection&)=delete;
        ChatSeparatorSection& operator=(const ChatSeparatorSection&)=delete;
        ChatSeparatorSection(ChatSeparatorSection&&)=delete;
        ChatSeparatorSection& operator=(ChatSeparatorSection&&)=delete;

        void setHLineVisible(bool enable) override;
        bool isHLineVisible() const override;

        void setText(const QString& text) override;
        QString text() const override;

        void setIconPath(WithPath path) override;
        WithPath iconPath() const override;

        void setIconSource(std::shared_ptr<AvatarSource> source) override;
        std::shared_ptr<AvatarSource> iconSource() const override;

        void setTailIcon(std::shared_ptr<SvgIcon> icon) override;
        std::shared_ptr<SvgIcon> tailIcon() const override;

        void setClickable(bool enable) override;
        bool isClickable() const override;

    private:

        std::unique_ptr<ChatSeparatorSection_p> pimpl;
};

class UISE_DESKTOP_EXPORT ChatSeparator : public AbstractChatSeparator
{
    public:

        ChatSeparator(QWidget* parent=nullptr);

    protected:

        void doInsertSection(AbstractChatSeparatorSection* section, int index=-1) override;

    private:

        QBoxLayout* m_layout;
};

class UISE_DESKTOP_EXPORT ChatMessageContent : public AbstractChatMessageContent
{
    public:

        ChatMessageContent(QWidget* parent=nullptr);

        void clearContentSelection() override;

    protected:

        void updateWidgets() override;

    private:

        QBoxLayout* m_layout;
};

class ChatMessage_p;

class UISE_DESKTOP_EXPORT ChatMessage : public AbstractChatMessage
{
    Q_OBJECT

    public:

        ChatMessage(QWidget* parent=nullptr);

        ~ChatMessage();
        ChatMessage(const ChatMessage&)=delete;
        ChatMessage& operator=(const ChatMessage&)=delete;
        ChatMessage(ChatMessage&&)=delete;
        ChatMessage& operator=(ChatMessage&&)=delete;

    protected:

        void updateTopSeparator() override;

        void updateSelectable() override;

        void updateSelection() override;

        void updateTopSpaceVisible() override;

        void updateLastInBatch() override;

        void updateContentVisible() override;

        void updateContent() override;

        void updateAvatarVisible() override;

        void mousePressEvent(QMouseEvent* event) override;

    private:

        std::unique_ptr<ChatMessage_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGE_HPP
