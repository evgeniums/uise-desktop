/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chatmessagetext.cpp
*
*  Defines ChatMessageText.
*
*/

/****************************************************************************/

#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessagetext.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/******************************EnhancedTextEdit********************************/

//--------------------------------------------------------------------------

ChatMessageTextBrowser::ChatMessageTextBrowser(QWidget* parent) : QTextBrowser(parent)
{
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

    connect(this,
        &QTextBrowser::textChanged,
        this,
        [this]()
        {
            updateHeight();
        }
    );

    setFocusPolicy(Qt::NoFocus);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateHeight()
{
    updateGeometry();
}

//--------------------------------------------------------------------------

QSize ChatMessageTextBrowser::sizeHint() const
{
    auto sz=QTextBrowser::sizeHint();
    if (document())
    {
        document()->adjustSize();
        QSizeF docSize = document()->size();
        int height = static_cast<int>(docSize.height() + 2 * frameWidth());
        return QSize(sz.width(), height);
    }
    return sz;
}

/********************************ChatMessageText****************************/

//--------------------------------------------------------------------------

class ChatMessageText_p
{
    public:

        QBoxLayout* layout;

        ChatMessageTextBrowser* text;
};

//--------------------------------------------------------------------------

ChatMessageText::ChatMessageText(QWidget* parent)
    : AbstractChatMessageText(parent),
      pimpl(std::make_unique<ChatMessageText_p>())
{
    pimpl->layout=Layout::vertical(this);

    pimpl->text=new ChatMessageTextBrowser(this);
    pimpl->layout->addWidget(pimpl->text);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageText::~ChatMessageText()
{}

//--------------------------------------------------------------------------

void ChatMessageText::loadText(const QString& text, bool markdown)
{
    if (markdown)
    {
        pimpl->text->setMarkdown(text);
    }
    else
    {
        pimpl->text->setPlainText(text);
    }
    // QTimer::singleShot(10,this,[this](){pimpl->text->updateSize();});
}

//--------------------------------------------------------------------------

void ChatMessageText::clearText()
{
    pimpl->text->clear();    
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
