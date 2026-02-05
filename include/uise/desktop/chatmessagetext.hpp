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

/** @file uise/desktop/chatmessagetext.hpp
*
*  Declares ChatMessageText.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGETEXT_HPP
#define UISE_DESKTOP_CHATMESSAGETEXT_HPP

#include <QTextBrowser>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT ChatMessageTextBrowser : public QTextBrowser
{
    Q_OBJECT

    public:

        explicit ChatMessageTextBrowser(QWidget *parent = nullptr);

        QSize sizeHint() const override;

    public slots:

        void updateHeight();
};

class ChatMessageText_p;

class UISE_DESKTOP_EXPORT ChatMessageText : public AbstractChatMessageText
{
    Q_OBJECT

    public:

        ChatMessageText(QWidget* parent=nullptr);

        ~ChatMessageText();
        ChatMessageText(const ChatMessageText&)=delete;
        ChatMessageText& operator=(const ChatMessageText&)=delete;
        ChatMessageText(ChatMessageText&&)=delete;
        ChatMessageText& operator=(ChatMessageText&&)=delete;

        void loadText(const QString& text, bool markdown=true) override;

        void clearText() override;

    private:

        std::unique_ptr<ChatMessageText_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGETEXT_HPP
