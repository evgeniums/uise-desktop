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

#include <QCoreApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>

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
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);

    setLineWrapMode(FixedPixelWidth);

    // Qt's macOS style draws the native Cocoa focus ring itself, outside the stylesheet paint
    // path entirely -- chat.qss's "outline: none;" on :focus (a QSS-level property) has no
    // effect on it. WA_MacShowFocusRect is the Qt-level switch for that ring specifically; a
    // no-op on every other platform. Only reachable at all once a host opts into
    // setCopyable(true), see that method's own doc comment.
    setAttribute(Qt::WA_MacShowFocusRect,false);

    // Wired unconditionally -- inert while contextMenuPolicy() stays Qt::NoContextMenu (the
    // default, see setCopyable()), so this only ever fires once a host actually opts in.
    connect(this,&QWidget::customContextMenuRequested,this,&ChatMessageTextBrowser::showCopyMenu);

    Style::updateWidgetStyle(this);

#if 0
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
                    updateSize();
                }
            );
        }
    );
#endif
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateSize()
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

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCopyable(bool enable)
{
    if (m_copyable==enable)
    {
        return;
    }
    m_copyable=enable;

    // NoFocus (the ctor's default) means Ctrl+C/Cmd+C can never reach this widget -- keyboard
    // shortcuts go to whichever widget currently HAS focus, and a widget with Qt::NoFocus can
    // never receive it via click or Tab. Mouse-drag selection (and so selectedText()) works
    // regardless of focus policy, which is why the live chat page's "Quote selected" flow was
    // never affected by this.
    setFocusPolicy(enable ? Qt::StrongFocus : Qt::NoFocus);
    // CustomContextMenu routes right-clicks to showCopyMenu() instead of Qt's own
    // createStandardContextMenu() -- see setCopyable()'s own doc comment for why.
    setContextMenuPolicy(enable ? Qt::CustomContextMenu : Qt::NoContextMenu);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::showCopyMenu(const QPoint& pos)
{
    QMenu menu(this);

    auto copyAction=menu.addAction(tr("Copy"));
    copyAction->setEnabled(textCursor().hasSelection());
    connect(copyAction,&QAction::triggered,this,&QTextEdit::copy);

    auto selectAllAction=menu.addAction(tr("Select All"));
    connect(selectAllAction,&QAction::triggered,this,&QTextEdit::selectAll);

    menu.exec(mapToGlobal(pos));
}

/********************************ChatMessageText****************************/

//--------------------------------------------------------------------------

class ChatMessageText_p
{
    public:

        QBoxLayout* layout;

        ChatMessageTextBrowser* text;
        int m_widthHint=0;
};

//--------------------------------------------------------------------------

ChatMessageText::ChatMessageText(QWidget* parent)
    : AbstractChatMessageText(parent),
      pimpl(std::make_unique<ChatMessageText_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->text=new ChatMessageTextBrowser(this);
    pimpl->layout->addWidget(pimpl->text);

    pimpl->text->setContextMenuPolicy(Qt::NoContextMenu);

    // QTextBrowser inherits QTextEdit::selectionChanged() -- relayed here so a host (e.g.
    // ReplyDialog's Save/"Quote selected" button swap) can react to selection changes via
    // AbstractChatMessageBody alone, without depending on this concrete body type.
    connect(pimpl->text,&QTextEdit::selectionChanged,this,&AbstractChatMessageBody::selectionChanged);

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

void ChatMessageText::adjustWrapWidth(int& value, bool add)
{
    auto op=[add](auto a, auto b)
    {
        if (add)
        {
            return a+b;
        }
        else
        {
            return a-b;
        }
    };

    op(value,pimpl->text->frameWidth()*2);
    auto applyMargins=[this,&value,op](QMargins cm)
    {
        op(value,cm.left());
        op(value,cm.right());
    };
    applyMargins(contentsMargins());
    applyMargins(pimpl->text->contentsMargins());
}

//--------------------------------------------------------------------------

int ChatMessageText::bubbleWidthHint(int forMaxWidth)
{
    auto t=const_cast<ChatMessageTextBrowser*>(pimpl->text);
    t->setLineWrapColumnOrWidth(forMaxWidth);
    pimpl->text->updateSize();
    auto w=static_cast<int>(t->document()->idealWidth());
    return w;
}

//--------------------------------------------------------------------------

void ChatMessageText::updateMaximumBubbleWidth()
{
    auto wrapWidth=chatContent()->maximumBubbleWidth();
    pimpl->text->setLineWrapColumnOrWidth(wrapWidth);
    pimpl->text->updateSize();
}

//--------------------------------------------------------------------------

QString ChatMessageText::selectedText() const
{
    auto text=pimpl->text->textCursor().selectedText();
    return text;
}

//--------------------------------------------------------------------------

bool ChatMessageText::hasSelectableText() const
{
    return !pimpl->text->toPlainText().trimmed().isEmpty();
}

//--------------------------------------------------------------------------

void ChatMessageText::setCopyable(bool enable)
{
    pimpl->text->setCopyable(enable);
}

//--------------------------------------------------------------------------

void ChatMessageText::selectText(const QString& text)
{
    if (text.isEmpty())
    {
        return;
    }
    auto cursor=pimpl->text->document()->find(text);
    if (cursor.isNull())
    {
        // Not found -- e.g. the quote was picked before an edit changed this text. Best-effort,
        // see this method's own doc comment (abstractchatmessage.hpp): silently do nothing.
        return;
    }
    pimpl->text->setTextCursor(cursor);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
