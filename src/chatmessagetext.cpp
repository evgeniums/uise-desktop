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
#include <QWheelEvent>
#include <QMouseEvent>

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
            QTimer::singleShot(
                0,
                this,
                [this]()
                {
                    updateHeight();
                }
            );
        }
    );

    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);

    setLineWrapMode(FixedPixelWidth);

    Style::updateWidgetStyle(this);

    auto cm=contentsMargins();
    setLineWrapColumnOrWidth(maximumWidth()-cm.left()-cm.right());
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setWrapWidth(int w)
{
    setLineWrapColumnOrWidth(w);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateHeight()
{
    document()->setTextWidth(document()->idealWidth());
    updateGeometry();
}

//--------------------------------------------------------------------------

QSize ChatMessageTextBrowser::sizeHint() const
{
    if (document())
    {
        QSizeF docSize = document()->size();
        int height = static_cast<int>(docSize.height() + 2 * frameWidth());
        int width = static_cast<int>(document()->idealWidth() + 2 * frameWidth());
        return QSize{width,height};
    }
    return QTextBrowser::sizeHint();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setMessageTextWidget(AbstractChatMessageText* widget)
{
    m_messageTextWidget=widget;
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::mousePressEvent(QMouseEvent* event)
{
    QTextBrowser::mousePressEvent(event);
    if (parentWidget())
    {
        QMouseEvent *cloned = event->clone();
        QCoreApplication::sendEvent(parentWidget(), cloned);
        delete cloned;
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::mouseMoveEvent(QMouseEvent* event)
{
    if (m_messageTextWidget && m_messageTextWidget->chatMessage()->isSelectionMode())
    {
        event->ignore();
    }
    else
    {
        if (!rect().contains(event->pos()))
        {            
            event->ignore();
        }
        else
        {
            QTextBrowser::mouseMoveEvent(event);
        }
    }
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
    pimpl->layout=Layout::horizontal(this);

    pimpl->text=new ChatMessageTextBrowser(this);    
    pimpl->layout->addWidget(pimpl->text);    

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
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
}

//--------------------------------------------------------------------------

void ChatMessageText::clearText()
{
    pimpl->text->clear();    
}

//--------------------------------------------------------------------------

void ChatMessageText::clearContentSelection()
{
    auto cur=pimpl->text->textCursor();
    cur.clearSelection();
    pimpl->text->setTextCursor(cur);
}

//--------------------------------------------------------------------------

void ChatMessageText::updateChatMessage()
{
    pimpl->text->setMessageTextWidget(this);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
