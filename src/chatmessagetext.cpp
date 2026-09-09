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

#include <limits>

#include <QCoreApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessagetext.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

void ChatMessageTextBrowser::setWrapWidth(int w)
{
    setLineWrapColumnOrWidth(w);
    updateSize();
}

//--------------------------------------------------------------------------

int ChatMessageTextBrowser::textWidthHint() const
{
    // Rounded UP, not truncated: callers use this as a width the document is guaranteed to fit
    // into without any further wrapping -- see ChatMessageText::bubbleWidthHint()'s identical
    // reasoning for its own lastHintWidth.
    return qCeil(document()->idealWidth());
}

//--------------------------------------------------------------------------

QRect ChatMessageTextBrowser::lastLineRect() const
{
    auto* doc=document();
    if (doc==nullptr || doc->isEmpty())
    {
        return {};
    }

    // Walk back from the last block to the last one that actually rendered a line -- a trailing
    // empty block (e.g. text ending in a blank line) has a valid QTextBlock but an empty layout.
    auto block=doc->lastBlock();
    while (block.isValid() && (!block.isVisible() || block.layout()==nullptr
                                || block.layout()->lineCount()==0))
    {
        block=block.previous();
    }
    if (!block.isValid())
    {
        return {};
    }

    // Trailing-space overlay only makes sense for LTR text: the blank space after the last word
    // sits on the RIGHT for LTR, but on the LEFT for RTL, and naturalTextWidth() below always
    // measures the line's own extent regardless of direction -- treating that as "room after the
    // text" would be backwards for RTL and would incorrectly widen every RTL message. RTL inline
    // placement is a separate task; these messages simply keep the full-width row (see
    // AbstractChatMessageContent::evaluateInlineBottom()'s handling of an invalid rect here).
    if (block.textDirection()==Qt::RightToLeft)
    {
        return {};
    }

    auto* lay=block.layout();
    auto line=lay->lineAt(lay->lineCount()-1);
    // blockBoundingRect() reads the layout bubbleWidthHint()/updateMaximumBubbleWidth() already
    // computed -- no second document layout pass here, which matters since this can run once per
    // message during a chat load (see [[todo-chat-message-paint-and-layout-cost]]). Its origin
    // already carries the document's own margin (QTextDocument::documentMargin()).
    auto br=doc->documentLayout()->blockBoundingRect(block);
    int offX=frameWidth()+contentsMargins().left();
    int offY=frameWidth()+contentsMargins().top();
    return QRect{qFloor(br.x()+line.x())+offX, qFloor(br.y()+line.y())+offY,
                 qCeil(line.naturalTextWidth()), qCeil(line.height())};
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
    // before this call is naturally in effect -- nothing else to do here, applyDocumentStyle() is
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
    applyDocumentStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setLinkUnderline(bool enable)
{
    if (m_linkUnderline==enable)
    {
        return;
    }
    m_linkUnderline=enable;
    applyDocumentStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::changeEvent(QEvent* event)
{
    QTextBrowser::changeEvent(event);
    if (event->type()==QEvent::StyleChange)
    {
        // Style::instance().css() can change on a theme switch even when this widget's OWN
        // linkColor/linkUnderline qproperty values do not (e.g. a switch that only touches
        // messagetext.css) -- setLinkColor()/setLinkUnderline() early-return in that case (see
        // their bodies above) and never reapply, so the base document CSS is re-pulled
        // unconditionally here instead of depending on the qproperty writers alone.
        applyDocumentStyle();
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyDocumentStyle()
{
    // Theme's document-level CSS (task-message-formatting-plan.md, Stage 1) -- e.g.
    // resources/style/messagetext.css -- forms the base; the link colour/underline rule is
    // appended last so it always wins over anything messagetext.css declares for `a` (it
    // deliberately declares none, to avoid needing to reason about override order between two
    // sources of anchor styling).
    QString css=Style::instance().css();

    QString linkCss=QStringLiteral("a { text-decoration: %1; }").arg(m_linkUnderline ? "underline" : "none");
    if (m_linkColor.isValid())
    {
        linkCss=QStringLiteral("a { color: %1; text-decoration: %2; }")
                .arg(m_linkColor.name(),m_linkUnderline ? "underline" : "none");
    }
    css+=QStringLiteral("\n")+linkCss;

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
    updateContextMenuPolicy();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setOwnContextMenuEnabled(bool enable)
{
    if (m_ownContextMenu==enable)
    {
        return;
    }
    m_ownContextMenu=enable;
    updateContextMenuPolicy();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateContextMenuPolicy()
{
    // CustomContextMenu routes right-clicks to showCopyMenu() instead of Qt's own
    // createStandardContextMenu() -- see setCopyable()'s own doc comment for why. Suppressed
    // entirely (NoContextMenu, letting a right-click bubble up to a host's own message-level
    // menu) when m_ownContextMenu is off -- see setOwnContextMenuEnabled()'s own doc comment.
    setContextMenuPolicy((m_copyable && m_ownContextMenu) ? Qt::CustomContextMenu : Qt::NoContextMenu);
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

        //! The width bubbleWidthHint() last wrapped the document at (0 = none yet, or the
        //! content has changed since -- see loadText()/clearText()). updateMaximumBubbleWidth()
        //! pins the final re-wrap to this instead of re-deriving it from the bubble's own final
        //! width -- see that method's own doc comment for why.
        int lastHintWidth=0;
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
    // A stale pin from the PREVIOUS content must never survive a content change -- the new
    // document's true idealWidth() (measured fresh by the next bubbleWidthHint() call) is what
    // updateMaximumBubbleWidth() must pin to instead.
    pimpl->lastHintWidth=0;
}

//--------------------------------------------------------------------------

void ChatMessageText::clearText()
{
    pimpl->text->setHtmlContent(QString{});
    pimpl->text->clear();
    pimpl->lastHintWidth=0;
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

int ChatMessageText::bubbleWidthHint(int forMaxWidth)
{
    auto wrapWidth=clampToMaxBubbleWidth(forMaxWidth);
    pimpl->text->setWrapWidth(wrapWidth);
    // qCeil, not a plain truncating cast: lastHintWidth must be >= the document's true
    // idealWidth() (the natural width of its widest line) for updateMaximumBubbleWidth()'s pin
    // below to be sound -- rounding DOWN could land under idealWidth and force an extra wrap
    // when re-applied there, moving the very line lastTextLineRect() measures.
    auto w=qCeil(pimpl->text->document()->idealWidth());
    if (w>wrapWidth)
    {
        w=wrapWidth;
    }
    pimpl->lastHintWidth=w;
    return w;
}

//--------------------------------------------------------------------------

void ChatMessageText::updateMaximumBubbleWidth()
{
    auto w=clampToMaxBubbleWidth(chatContent()->maximumBubbleWidth());

    // Pin the re-wrap to the width bubbleWidthHint() actually measured this pass' document at,
    // instead of re-deriving it from the bubble's own final width -- which, in inline mode, can
    // be WIDER than what was measured (the bubble widened specifically to seat the bottom row
    // beside this text). Re-wrapping at ANY width >= lastHintWidth reproduces byte-identical
    // line breaks: by definition of idealWidth(), every line already fit within lastHintWidth,
    // so a wider (or equal) constraint cannot force any line to wrap differently. This is what
    // keeps lastTextLineRect() -- measured during bubbleWidthHint(), before this call -- still
    // accurate afterwards, with no re-measurement needed. Same idiom, same reason, as
    // ChatMessageImages::updateMaximumBubbleWidth()'s own lastLayoutForMaxWidth pin on the album
    // body -- see its doc comment for the real-world case where re-laying out at a different
    // budget genuinely changes the result.
    if (pimpl->lastHintWidth>0 && pimpl->lastHintWidth<=w)
    {
        w=pimpl->lastHintWidth;
    }
    pimpl->text->setWrapWidth(w);
}

//--------------------------------------------------------------------------

QRect ChatMessageText::lastTextLineRect() const
{
    auto rect=pimpl->text->lastLineRect();
    if (!rect.isValid())
    {
        return {};
    }
    // ChatMessageText itself carries no QSS padding today (Layout::horizontal(this) zeroes its
    // contentsMargins, and nothing overrides them) -- translating by them anyway keeps this
    // correct if that ever changes, matching how bubbleWidthHint()/updateMaximumBubbleWidth()
    // treat the browser as filling this frame's own contents rect.
    return rect.translated(contentsMargins().left(),contentsMargins().top());
}

//--------------------------------------------------------------------------

int ChatMessageText::ownWidthCeiling() const
{
    // clampToMaxBubbleWidth() against the largest possible value is exactly maxBubbleWidth()
    // itself when the cap is enabled (>0), or a pass-through (no cap) when it's disabled -- same
    // helper bubbleWidthHint()/updateMaximumBubbleWidth() already use, so this can never
    // disagree with what they actually enforced.
    return clampToMaxBubbleWidth(std::numeric_limits<int>::max());
}

//--------------------------------------------------------------------------

QString ChatMessageText::selectedText() const
{
    // QTextCursor::selectedText() uses U+2029 (ParagraphSeparator) for a line break, not '\n' --
    // left as-is, a multi-line selection would reach the clipboard/composer as one run of text
    // with an invisible separator instead of a real line break.
    auto text=pimpl->text->textCursor().selectedText();
    text.replace(QChar(QChar::ParagraphSeparator),QChar('\n'));
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

void ChatMessageText::setOwnContextMenuEnabled(bool enable)
{
    pimpl->text->setOwnContextMenuEnabled(enable);
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

}
