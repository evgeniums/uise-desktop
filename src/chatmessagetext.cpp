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
#include <QTextCursor>
#include <QTextBlock>

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

    // task-urls-and characters-in-messages.md, Stage 1: without this, QTextBrowser's default
    // openLinks=true/openExternalLinks=false makes a click call setSource() on itself, which
    // blanks the bubble instead of doing anything useful. Activation is relayed via
    // linkActivated() instead -- the host (whitemdesktop) decides what a click actually does.
    setOpenLinks(false);
    connect(this,&QTextBrowser::anchorClicked,this,&ChatMessageTextBrowser::linkActivated);

    // This is a read-only display widget -- nothing ever exposes Ctrl+Z/Ctrl+Shift+Z here, and
    // updateHoveredAnchor()'s underline toggling would otherwise silently grow an unused undo
    // stack every time the mouse crosses a link.
    document()->setUndoRedoEnabled(false);

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

void ChatMessageTextBrowser::setHtmlContent(const QString& html)
{
    m_lastHtml=html;
    setHtml(html);
    // setHtml() replaces the document, so any style already set via setDefaultStyleSheet()
    // before this call is naturally in effect -- nothing else to do here, applyLinkStyle() is
    // only needed when the style changes AFTER content is already loaded (see below).
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setLinkColor(const QColor& color)
{
    if (m_linkColor==color)
    {
        return;
    }
    m_linkColor=color;
    applyLinkStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setLinkUnderline(bool enable)
{
    if (m_linkUnderline==enable)
    {
        return;
    }
    m_linkUnderline=enable;
    applyLinkStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyLinkStyle()
{
    QString css=QStringLiteral("a { text-decoration: %1; }").arg(m_linkUnderline ? "underline" : "none");
    if (m_linkColor.isValid())
    {
        css=QStringLiteral("a { color: %1; text-decoration: %2; }")
                .arg(m_linkColor.name(),m_linkUnderline ? "underline" : "none");
    }
    document()->setDefaultStyleSheet(css);

    // setDefaultStyleSheet() only affects content set AFTERWARDS -- reapply the last HTML we
    // know about so an already-rendered bubble picks up a theme/color change immediately (e.g.
    // Style::updateWidgetStyle()'s repolish on a light/dark switch) instead of only the next
    // message that happens to load.
    if (!m_lastHtml.isEmpty())
    {
        setHtml(m_lastHtml);
        // The reload above reset every anchor to the base style, including one that was mid-hover
        // -- re-apply its hover-underline immediately rather than waiting for the next mouse move.
        if (!m_hoveredAnchor.isEmpty())
        {
            setAnchorUnderline(m_hoveredAnchor,true);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setAnchorUnderline(const QString& href, bool enable)
{
    if (href.isEmpty())
    {
        return;
    }
    QTextCursor cursor(document());
    for (auto block=document()->begin();block!=document()->end();block=block.next())
    {
        for (auto it=block.begin();!it.atEnd();++it)
        {
            auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }
            auto format=fragment.charFormat();
            if (!format.isAnchor() || format.anchorHref()!=href)
            {
                continue;
            }
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);
            format.setFontUnderline(enable);
            cursor.setCharFormat(format);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateHoveredAnchor(const QPoint& pos)
{
    auto href=anchorAt(pos);
    if (href==m_hoveredAnchor)
    {
        return;
    }
    if (!m_hoveredAnchor.isEmpty())
    {
        setAnchorUnderline(m_hoveredAnchor,m_linkUnderline);
    }
    m_hoveredAnchor=href;
    if (!m_hoveredAnchor.isEmpty())
    {
        setAnchorUnderline(m_hoveredAnchor,true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::clearHoveredAnchor()
{
    if (m_hoveredAnchor.isEmpty())
    {
        return;
    }
    setAnchorUnderline(m_hoveredAnchor,m_linkUnderline);
    m_hoveredAnchor.clear();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::mousePressEvent(QMouseEvent* event)
{
    // Don't also relay a link click to the parent as a bubble-selection/content-menu gesture --
    // task-urls-and characters-in-messages.md, Stage 1. Only suppressed outside selection mode,
    // where a click already means "select this message", not "follow this link" -- anchorAt()
    // still resolves an anchor there, but activation itself is separately gated the same way in
    // mouseMoveEvent() below.
    bool onLink=!anchorAt(event->pos()).isEmpty();
    bool selecting=m_messageTextWidget && m_messageTextWidget->chatMessage()->isSelectionMode();

    QTextBrowser::mousePressEvent(event);
    if (parentWidget() && !(onLink && !selecting))
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
        clearHoveredAnchor();
        event->ignore();
    }
    else
    {
        if (!rect().contains(event->pos()))
        {
            clearHoveredAnchor();
            event->ignore();
        }
        else
        {
            updateHoveredAnchor(event->pos());
            QTextBrowser::mouseMoveEvent(event);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::leaveEvent(QEvent* event)
{
    clearHoveredAnchor();
    QTextBrowser::leaveEvent(event);
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

    // Same idiom, for a clicked hyperlink (task-urls-and characters-in-messages.md, Stage 1) --
    // see AbstractChatMessageBody::linkActivated()'s own doc comment.
    connect(pimpl->text,&ChatMessageTextBrowser::linkActivated,this,&AbstractChatMessageBody::linkActivated);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageText::~ChatMessageText()
{}

//--------------------------------------------------------------------------

void ChatMessageText::loadText(const QString& text, TextFormat format)
{
    switch (format)
    {
        case TextFormat::Html:
            pimpl->text->setHtmlContent(text);
            break;
        case TextFormat::Markdown:
            pimpl->text->setMarkdown(text);
            break;
        case TextFormat::Plain:
            pimpl->text->setPlainText(text);
            break;
    }
}

//--------------------------------------------------------------------------

void ChatMessageText::clearText()
{
    pimpl->text->setHtmlContent(QString{});
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
    auto wrapWidth=clampToMaxBubbleWidth(forMaxWidth);
    auto t=const_cast<ChatMessageTextBrowser*>(pimpl->text);
    t->setLineWrapColumnOrWidth(wrapWidth);
    pimpl->text->updateSize();
    auto w=static_cast<int>(t->document()->idealWidth());
    if (w>wrapWidth)
    {
        w=wrapWidth;
    }
    return w;
}

//--------------------------------------------------------------------------

void ChatMessageText::updateMaximumBubbleWidth()
{
    auto wrapWidth=clampToMaxBubbleWidth(chatContent()->maximumBubbleWidth());
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

QString ChatMessageText::linkAt(const QPoint& pos) const
{
    return pimpl->text->anchorAt(pimpl->text->mapFrom(this,pos));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
