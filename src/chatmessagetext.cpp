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
#include <QPainter>
#include <algorithm>
#include <QLabel>
#include <QClipboard>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextTable>
#include <QTextFrame>
#include <QTextDocumentFragment>
#include <QPushButton>
#include <QScrollBar>
#include <QMimeData>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QShortcut>
#include <QAbstractTextDocumentLayout>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/syntaxhighlighter.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/toast.hpp>
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

    // Qt's own 40px per indent level leaves even a one-level list a long way from the bubble's
    // edge, and it has to agree with the composer's -- see DefaultListIndentWidth.
    document()->setIndentWidth(DefaultListIndentWidth);

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
    // task-message-formatting-plan.md, Stage 3: do NOT re-enable this. A SyntaxHighlighter's
    // explicit rehighlight() call (ensureSyntaxHighlighter(), applyDocumentStyle()) wraps its
    // pass in QTextCursor::beginEditBlock()/endEditBlock(), and QTextDocumentPrivate::finishEdit()
    // emits contentsChanged() UNCONDITIONALLY at the end of that block -- so with this connected,
    // every theme switch (and every first highlight of a freshly-loaded code block) would re-
    // enter updateSize() via this queued singleShot, which was never exercised while this block
    // stayed disabled and is not something Stage 3 verified.
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
    // Shrink-wrap the document to its content -- but NEVER wider than the wrap width. Without
    // the clamp, a pinned wide table (applyWideTableLayout()) pushes idealWidth() past the wrap
    // width (measured: 380 -> 612 for one 6-column table), and setting THAT as the text width
    // would re-lay every surrounding paragraph out at 612 too, unwrapping the prose the pinning
    // was careful not to touch. Clamping is a no-op for content without a pinned table: Qt
    // already caps idealWidth() at the text width in that case.
    auto ideal=document()->idealWidth();
    auto cap=lineWrapColumnOrWidth();
    document()->setTextWidth((cap>0 && ideal>cap) ? static_cast<qreal>(cap) : ideal);
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setWrapWidth(int w)
{
    setLineWrapColumnOrWidth(w);
    // Re-evaluate wide tables against the NEW width before shrink-wrapping: this is where the
    // wrap width first becomes known at all (setHtmlContent() runs before any negotiation pass,
    // so pinning cannot happen there), and a later pass can widen the bubble enough that a
    // previously pinned table now fits and should be un-pinned again.
    applyWideTableLayout();
    updateSize();
    // AFTER updateSize(), not before: the code-overflow verdict now reads
    // QTextDocument::idealWidth(), which only reflects the NEW wrap width once the document has
    // been laid out at it. Running it first would test the previous pass' layout and lag the
    // scrollbar by one negotiation. applyWideTableLayout() above has already had its say on the
    // shared scrollbar; this runs after it, so the combined decision is the final one.
    applyCodeBlockOverflow();
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

    // The bottom row is a fixed sibling overlay, positioned once per negotiation pass and never
    // re-derived as the user scrolls -- tucking it inline onto a line that sits inside a
    // horizontally scrollable document would let that line slide out from under a timestamp that
    // never moves. reservesHorizontalScrollBar() is exactly "this document currently has a wide
    // table pinned", so bail out before even looking for a last line; row mode's own full-width
    // placement sits below the scrollbar band instead (see sizeHint()'s own reservation of it).
    if (m_hScrollReserved)
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

    // A code block has no trailing space to overlay onto. The whole mechanism rests on there
    // being blank room to the right of the last WORD, but a code block paints a background across
    // the full bubble width, so the timestamp does not land in empty space -- it lands on top of
    // the code. Returning an invalid rect is already the "no room here" signal
    // AbstractChatMessageContent::evaluateInlineBottom() understands, and it puts the row on its
    // own full-width line below, which is exactly what is wanted.
    //
    // Read from m_codeBlocks rather than from the block format: the runs are recorded precisely so
    // every later reader has one answer to "is this position inside code", independent of which
    // formats happen to be on the block at the time. (The format's own nonBreakableLines() would
    // also answer it now that applyCodeBlockLayout() leaves the flag set, but m_codeBlocks stays
    // the single source of truth -- it is also what the painting, the overlays and the overflow
    // verdict all read.)
    const auto position=block.position();
    for (const auto& codeBlock : m_codeBlocks)
    {
        if (position>=codeBlock.firstPosition && position<=codeBlock.lastPosition)
        {
            return {};
        }
    }

    // A table cell is not a good overlay target either, pinned/scrollable or not: a timestamp
    // sitting inside a table cell reads as one of the table's own values, not as the message's
    // status row. m_hScrollReserved above already excludes every PINNED table's document; this
    // additionally excludes a table that fits within the wrap width (never pinned, so it does not
    // reserve a scrollbar) but still ends the message.
    if (qobject_cast<QTextTable*>(doc->frameAt(position))!=nullptr)
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

void ChatMessageTextBrowser::setCodeBlockPadding(int padding)
{
    if (m_codeBlockPadding==padding)
    {
        return;
    }

    m_codeBlockPadding=padding;

    // The padding is reserved as block margins by applyCodeBlockLayout(), so changing it after
    // content is loaded has to redo that pass -- repainting alone would inflate the box over text
    // that has not made room for it.
    applyCodeBlockLayout();
    viewport()->update();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyCodeBlockLayout()
{
    // Overlays are RECYCLED across passes, exactly as applyWideTableLayout() recycles its expand
    // buttons and for the same reasons: this now runs on every content load AND on every
    // applyDocumentStyle() replay, and simply clearing m_codeBlocks would drop the QPointers
    // without destroying the widgets -- leaving each pass' strips parented to viewport() and
    // visible on top of the next pass' own.
    std::vector<QPointer<QWidget>> recycled;
    recycled.reserve(m_codeBlocks.size());
    for (auto& tracked : m_codeBlocks)
    {
        recycled.push_back(tracked.overlay);
    }
    m_codeBlocks.clear();

    auto dropUnusedOverlays=[&recycled](std::size_t keep)
    {
        for (std::size_t i=keep;i<recycled.size();++i)
        {
            if (!recycled[i].isNull())
            {
                // Deleted outright rather than deleteLater()'d -- same reasoning as
                // applyWideTableLayout()'s dropUnusedButtons().
                delete recycled[i].data();
            }
        }
        recycled.clear();
    };

    auto* doc=document();
    if (doc==nullptr)
    {
        dropUnusedOverlays(0);
        m_anyCodeOverflow=false;
        updateHorizontalOverflow();
        return;
    }

    // Collected first, mutated second: clearing nonBreakableLines() as we go would destroy the
    // very flag the run detection reads.
    std::vector<TrackedCodeBlock> found;
    for (auto block=doc->begin(); block.isValid() && block!=doc->end(); block=block.next())
    {
        if (!block.blockFormat().nonBreakableLines())
        {
            continue;
        }

        TrackedCodeBlock tracked;
        tracked.firstPosition=block.position();
        tracked.lastPosition=block.position()+block.length()-1;
        tracked.language=block.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage);

        auto next=block.next();
        while (next.isValid() && next!=doc->end() && next.blockFormat().nonBreakableLines())
        {
            tracked.lastPosition=next.position()+next.length()-1;
            if (tracked.language.isEmpty())
            {
                tracked.language=next.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage);
            }
            block=next;
            next=block.next();
        }

        found.push_back(tracked);
    }

    if (found.empty())
    {
        dropUnusedOverlays(0);
        m_anyCodeOverflow=false;
        updateHorizontalOverflow();
        return;
    }

    // Undo suppressed and signals left alone for the same reason the editor's blockquote
    // normalisation does it: this runs immediately after a whole-document load, where the undo
    // stack is meaningless, and QTextDocumentPrivate::changeObjectFormat() appends an item per
    // format write.
    const auto undoEnabled=doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);

    for (const auto& tracked : found)
    {
        for (auto block=doc->findBlock(tracked.firstPosition);
             block.isValid() && block.position()<=tracked.lastPosition;
             block=block.next())
        {
            QTextCursor cursor(block);
            auto format=cursor.blockFormat();

            // nonBreakableLines is deliberately LEFT SET (an earlier revision cleared it here, to
            // wrap the code the way `white-space: pre-wrap` used to): a code line must keep its
            // natural width, because wrapping it destroys the indentation that carries the code's
            // structure. What happens when that does not fit is answered by codeBlockWidenBubble /
            // codeBlockScroll / codeBlockExpandButton instead -- see applyCodeBlockOverflow().
            // Leaving it set is also what makes this whole method idempotent, so it can safely run
            // again after applyDocumentStyle()'s setHtml() replay restores the flag.

            // The room the painted padding fills. Left/right on every line so the box can inflate
            // sideways without reaching under the text; top only on the first line and bottom only
            // on the last, so the gap does not repeat between the lines of one block.
            format.setLeftMargin(m_codeBlockPadding);
            format.setRightMargin(m_codeBlockPadding);
            format.setTopMargin(block.position()==tracked.firstPosition ? m_codeBlockPadding : 0);
            const auto isLast=(block.position()+block.length()-1)>=tracked.lastPosition;
            format.setBottomMargin(isLast ? m_codeBlockPadding : 0);

            cursor.setBlockFormat(format);
        }
    }

    doc->setUndoRedoEnabled(undoEnabled);

    // The width each block WANTS, measured by laying the whole document out unconstrained -- the
    // same measurement, taken the same way and for the same reason, as applyWideTableLayout()'s
    // own natural-width pass. Done AFTER the margins above so the measurement includes them: they
    // are part of what has to fit.
    //
    // `line.x() + naturalTextWidth() + rightMargin` is NOT an approximation -- it is character for
    // character the formula QTextDocumentLayout::layoutBlock() itself accumulates into
    // layoutStruct->contentsWidth (qtextdocumentlayout.cpp), which is what ends up as the root
    // frame's width and therefore as the horizontal scrollbar's own range. Measuring it any other
    // way would risk disagreeing with the thing that actually decides whether the content
    // overflows. In particular NOT blockBoundingRect().width(), which reports the block's LAYOUT
    // width (the full text width it was laid out into), not the width its text needs -- the same
    // distinction lastLineRect() already relies on when it takes its own width from
    // naturalTextWidth() rather than from the bounding rect it uses for the origin.
    const auto restoreWidth=doc->textWidth();
    doc->setTextWidth(-1);
    // Forces the relayout at the new width before any block layout is read: setTextWidth()
    // invalidates the layouts but does not rebuild them, and an un-laid-out block reports no lines
    // at all -- the same guard, for the same reason, as MessageEditor::textLineCount()'s own
    // QTextDocument::size() call (and as this file's own test helper already does).
    (void)doc->size();
    for (auto& tracked : found)
    {
        qreal natural=0;
        for (auto block=doc->findBlock(tracked.firstPosition);
             block.isValid() && block.position()<=tracked.lastPosition;
             block=block.next())
        {
            auto* layout=block.layout();
            if (layout==nullptr)
            {
                continue;
            }
            const auto rightMargin=block.blockFormat().rightMargin();
            for (int i=0;i<layout->lineCount();++i)
            {
                const auto line=layout->lineAt(i);
                natural=qMax(natural,line.x()+line.naturalTextWidth()+rightMargin);
            }
        }
        tracked.naturalWidth=natural;
    }
    doc->setTextWidth(restoreWidth);

    for (std::size_t i=0;i<found.size();++i)
    {
        if (i<recycled.size())
        {
            found[i].overlay=recycled[i];
        }
    }
    dropUnusedOverlays(found.size());

    m_codeBlocks=std::move(found);

    applyCodeBlockOverflow();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setViewerMode(bool enable)
{
    if (m_viewerMode==enable)
    {
        return;
    }
    m_viewerMode=enable;

    if (m_viewerMode)
    {
        // NoWrap makes lineWrapColumnOrWidth() 0, which is already the "nothing negotiated" case
        // applyCodeBlockOverflow() and updateSize() both guard on -- so the bubble's wrap-width
        // machinery goes quiet on its own and only the scrollbar policy needs taking back, below.
        setLineWrapMode(NoWrap);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }

    updateGeometry();
}

//--------------------------------------------------------------------------

qreal ChatMessageTextBrowser::codeContentToWrapWidth(qreal contentWidth) const
{
    auto* doc=document();
    return contentWidth+(doc!=nullptr ? 2*doc->documentMargin() : 0);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyCodeBlockOverflow()
{
    // A wrap width of 0 means none has been negotiated yet (applyCodeBlockLayout() also runs from
    // setHtmlContent(), which can precede the first bubbleWidthHint()) -- there is nothing to
    // compare against, so nothing overflows this pass; setWrapWidth() calls back once it knows.
    // Same rule, same reason, as applyWideTableLayout()'s own wrapWidth>0 guard.
    const auto wrapWidth=lineWrapColumnOrWidth();

    // QTextDocument::idealWidth() is Qt's OWN answer to "how wide does this document need to be",
    // assembled by the same layout pass that decides what gets clipped -- so it cannot disagree
    // with what is actually on screen the way an independent re-measurement can.
    //
    // This deliberately does NOT test our own TrackedCodeBlock::naturalWidth. That measurement
    // drives the bubble WIDENING (below, via widestCodeBlockWidth()), and it is only ever an
    // estimate of what Qt will do; when it came up short, the bubble was sized from the short
    // value AND the overflow test -- being the same short value compared against a width derived
    // from it -- concluded "fits" by construction. The block was then clipped with no scrollbar at
    // all, by anything from one character to most of a word depending on the content. Asking Qt
    // instead makes those two failure modes independent: an under-measured widening now costs a
    // slightly narrow bubble, never a missing scrollbar.
    //
    // Gated on there being a code block at all, so a document whose idealWidth is raised by
    // something else (a pinned table) is left to m_anyTablePinned, which owns that case.
    m_anyCodeOverflow=false;
    if (m_codeBlockScroll && wrapWidth>0 && !m_codeBlocks.empty() && document()!=nullptr)
    {
        m_anyCodeOverflow=(qCeil(document()->idealWidth())>wrapWidth);
    }

    updateHorizontalOverflow();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateHorizontalOverflow()
{
    // A viewer owns its own scrollbars -- see setViewerMode(). Both bars are permanently
    // as-needed there, and letting the bubble's overflow logic switch the horizontal one off
    // would take away the very thing the expanded viewer exists to provide.
    if (m_viewerMode)
    {
        return;
    }

    const auto anyOverflow=(m_anyTablePinned || m_anyCodeOverflow);
    setHorizontalScrollBarPolicy(anyOverflow ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);

    // Mirrors updateSize()'s own clamp check rather than reading the scroll area's own
    // horizontalScrollBar()->isVisible(), which is a pass behind the decision being made here --
    // see reservesHorizontalScrollBar()'s own doc comment. idealWidth() covers BOTH sources: a
    // pinned table's fixed frame width and a non-wrapping code line's naturalTextWidth() both feed
    // QTextDocumentLayout's contentsWidth, which is what idealWidth() reports.
    const auto cap=lineWrapColumnOrWidth();
    setHScrollReserved(anyOverflow && cap>0
                       && document()!=nullptr && qCeil(document()->idealWidth())>cap);
}

//--------------------------------------------------------------------------

int ChatMessageTextBrowser::widestCodeBlockWidth() const
{
    qreal widest=0;
    for (const auto& tracked : m_codeBlocks)
    {
        widest=qMax(widest,tracked.naturalWidth);
    }
    if (widest<=0)
    {
        return 0;
    }
    // Returned as a WRAP width, not as the raw content width, because that is what every caller
    // compares it against or feeds into setWrapWidth() -- see codeContentToWrapWidth(). Without
    // the conversion a bubble widened to exactly this value came out 2*documentMargin() too narrow
    // for its own code.
    return qCeil(codeContentToWrapWidth(widest));
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCodeBlockWidenBubbleEnabled(bool enable)
{
    if (m_codeBlockWidenBubble==enable)
    {
        return;
    }
    m_codeBlockWidenBubble=enable;
    // Only the HOST's own width negotiation can act on this (ChatMessageText::bubbleWidthHint()),
    // so ask for one rather than trying to re-derive a width here.
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCodeBlockScrollEnabled(bool enable)
{
    if (m_codeBlockScroll==enable)
    {
        return;
    }
    m_codeBlockScroll=enable;
    applyCodeBlockOverflow();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCodeBlockExpandButtonEnabled(bool enable)
{
    if (m_codeBlockExpandButton==enable)
    {
        return;
    }
    m_codeBlockExpandButton=enable;
    updateCodeBlockOverlays();
}

//--------------------------------------------------------------------------

QRect ChatMessageTextBrowser::codeBlockViewportRect(const TrackedCodeBlock& codeBlock) const
{
    auto* doc=document();
    if (doc==nullptr)
    {
        return {};
    }

    auto first=doc->findBlock(codeBlock.firstPosition);
    auto last=doc->findBlock(codeBlock.lastPosition);
    if (!first.isValid() || !last.isValid())
    {
        return {};
    }

    auto* layout=doc->documentLayout();
    // Union rather than first..last arithmetic: a wrapped code line makes a block taller than one
    // line, and blockBoundingRect() already accounts for that.
    auto rect=layout->blockBoundingRect(first).united(layout->blockBoundingRect(last));
    if (!rect.isValid())
    {
        return {};
    }

    // Document coordinates to viewport coordinates. The document's own margin is already in
    // blockBoundingRect()'s origin (same as lastLineRect() relies on).
    rect.translate(-horizontalScrollBar()->value(),-verticalScrollBar()->value());

    // Inflate back over the vertical room applyCodeBlockLayout() reserved as top/bottom block
    // margins. blockBoundingRect() is `layout->boundingRect()` moved to `layout->position()`
    // (qtextdocumentlayout.cpp) -- the union of the block's LINE rects, which excludes the block's
    // own margins entirely. So without this the slab hugs the text exactly and the padding, though
    // reserved in the layout, is never painted: the code sits flush against the top and bottom
    // edges of its own background. The horizontal half needs no equivalent -- the left edge is
    // pinned to documentMargin() below, which is already codeBlockPadding() left of the text
    // (the block's leftMargin), and the right edge spans the viewport.
    rect.adjust(0,-m_codeBlockPadding,0,m_codeBlockPadding);

    // The box spans the full text width rather than only the longest line: a code block reads as
    // a slab, and a ragged right edge following the longest line looks like a mistake.
    rect.setLeft(doc->documentMargin());
    rect.setRight(qMax<qreal>(rect.left(),viewport()->width()-doc->documentMargin()));

    return rect.toAlignedRect();
}

//--------------------------------------------------------------------------

const ChatMessageTextBrowser::TrackedCodeBlock* ChatMessageTextBrowser::codeBlockAt(
        const QPoint& viewportPos) const
{
    for (const auto& codeBlock : m_codeBlocks)
    {
        if (codeBlockViewportRect(codeBlock).contains(viewportPos))
        {
            return &codeBlock;
        }
    }

    return nullptr;
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCodeBlockOverlayEnabled(bool enable)
{
    if (m_codeBlockOverlay==enable)
    {
        return;
    }
    m_codeBlockOverlay=enable;
    updateCodeBlockOverlays();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCodeBlockOverlayVisibleOnHover(bool enable)
{
    if (m_codeBlockOverlayOnHover==enable)
    {
        return;
    }
    m_codeBlockOverlayOnHover=enable;
    updateCodeBlockOverlayVisibility();
}

//--------------------------------------------------------------------------

QString ChatMessageTextBrowser::codeBlockText(const TrackedCodeBlock& codeBlock) const
{
    auto* doc=document();
    if (doc==nullptr)
    {
        return {};
    }

    QStringList lines;
    for (auto block=doc->findBlock(codeBlock.firstPosition);
         block.isValid() && block.position()<=codeBlock.lastPosition;
         block=block.next())
    {
        lines.append(block.text());
    }

    return lines.join(QLatin1Char('\n'));
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::copyCodeBlock(const TrackedCodeBlock& codeBlock)
{
    // setText() rather than the selectAll()+copy() route ChatMessageTableViewer::copyTable()
    // takes. That viewer holds one table and nothing else, so selecting everything IS selecting
    // the table; here the code block is one part of a message, and selecting it would both
    // clobber whatever the user had selected and put the surrounding prose on the clipboard.
    // Code is wanted as plain text anyway -- the html/markdown/ODF flavours the copy path adds
    // are exactly what a paste into an editor or terminal does not want.
    QGuiApplication::clipboard()->setText(codeBlockText(codeBlock));

    // Emitted unconditionally, before the toast: a host that turned the built-in toast off is
    // relying on this to show its own. Same contract as tableCopied().
    emit codeBlockCopied(codeBlock.language);

    if (m_copyToastEnabled)
    {
        auto* toast=ensureCopyToast();
        if (toast!=nullptr)
        {
            toast->show(tr("Copied"));
        }
    }
}

//--------------------------------------------------------------------------

Toast* ChatMessageTextBrowser::ensureCopyToast()
{
    if (m_toast!=nullptr)
    {
        return m_toast;
    }

    if (m_ownToast==nullptr)
    {
        // Parented to the hosting window rather than to this browser, and drawn in-parent rather
        // than as a Qt::Tool window -- both for the reasons ChatMessageTableViewer::ensureToast()
        // records. The window parent matters more here: these widgets are recycled by
        // FlyweightListView, and a toast owned by one would be destroyed mid-animation the moment
        // its bubble was rebound to another message.
        auto* host=window();
        m_ownToast=new Toast(host!=nullptr ? host : static_cast<QWidget*>(this));
        m_ownToast->setDrawInParent(true);
    }

    return m_ownToast;
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateCodeBlockOverlays()
{
    // Overlays are REUSED across passes rather than rebuilt, for the same reason the table
    // buttons are: this runs on every bubble-width negotiation, and a chat load negotiates a lot.
    for (std::size_t i=0;i<m_codeBlocks.size();++i)
    {
        auto& tracked=m_codeBlocks[i];

        if (!m_codeBlockOverlay)
        {
            if (!tracked.overlay.isNull())
            {
                tracked.overlay->hide();
            }
            continue;
        }

        if (tracked.overlay.isNull())
        {
            auto* strip=new QFrame(viewport());
            strip->setObjectName(QStringLiteral("codeBlockOverlay"));
            strip->setCursor(Qt::ArrowCursor);
            strip->setFocusPolicy(Qt::NoFocus);

            auto* layout=Layout::horizontal(strip);

            // Language label first, and only when there is one: an untagged fence has nothing to
            // say here, and an empty label would still take space and draw a background.
            auto* language=new QLabel(strip);
            language->setObjectName(QStringLiteral("codeBlockLanguage"));
            language->setFocusPolicy(Qt::NoFocus);
            layout->addWidget(language);

            auto* copyButton=new IconTextButton(
                Style::instance().svgIconLocator().icon(
                    QStringLiteral("ChatMessageTextBrowser::copyCodeBlock"),this),
                strip);
            copyButton->setObjectName(QStringLiteral("codeBlockCopyButton"));
            copyButton->setCursor(Qt::ArrowCursor);
            copyButton->setFocusPolicy(Qt::NoFocus);
            copyButton->setToolTip(tr("Copy code"));
            layout->addWidget(copyButton);

            const auto index=i;
            connect(copyButton,&IconTextButton::clicked,this,
                [this,index]()
                {
                    // Re-read through the index rather than capturing the text: the document is
                    // rebuilt on every new message this recycled widget shows, and a captured
                    // string would go stale (or, worse, copy a previous sender's code).
                    if (index<m_codeBlocks.size())
                    {
                        copyCodeBlock(m_codeBlocks[index]);
                    }
                }
            );

            auto* expandButton=new IconTextButton(
                Style::instance().svgIconLocator().icon(
                    QStringLiteral("ChatMessageTextBrowser::expandCodeBlock"),this),
                strip);
            expandButton->setObjectName(QStringLiteral("codeBlockExpandButton"));
            expandButton->setCursor(Qt::ArrowCursor);
            expandButton->setFocusPolicy(Qt::NoFocus);
            expandButton->setToolTip(tr("Open code in a larger window"));
            layout->addWidget(expandButton);

            connect(expandButton,&IconTextButton::clicked,this,
                [this,index]()
                {
                    // Same re-read-through-the-index rule as Copy above.
                    openCodeBlockViewer(static_cast<int>(index));
                }
            );

            tracked.overlay=strip;
        }

        auto* strip=qobject_cast<QFrame*>(tracked.overlay.data());
        if (strip==nullptr)
        {
            continue;
        }

        auto* language=strip->findChild<QLabel*>(QStringLiteral("codeBlockLanguage"));
        if (language!=nullptr)
        {
            language->setText(tracked.language);
            language->setVisible(!tracked.language.isEmpty());
        }

        // Per-pass rather than once at creation, for the same reason the language label is: the
        // strip is recycled across content loads, so whatever the property says now has to win
        // over whatever the block it last showed needed.
        auto* expandButton=strip->findChild<IconTextButton*>(QStringLiteral("codeBlockExpandButton"));
        if (expandButton!=nullptr)
        {
            expandButton->setVisible(m_codeBlockExpandButton);
        }

        const auto rect=codeBlockViewportRect(tracked);
        if (!rect.isValid())
        {
            strip->hide();
            continue;
        }

        // adjustSize() rather than sizeHint(), for the reason the table button records: the size
        // is owned by QSS, and positioning from an unclamped hint would put the strip where it
        // does not end up.
        strip->adjustSize();
        const auto size=strip->size();

        // Top-right of the block, inset by the padding so it sits inside the painted box rather
        // than straddling its corner.
        constexpr int margin=2;
        const auto x=rect.right()-size.width()-margin;
        const auto y=rect.top()+margin;
        strip->move(qMax(rect.left(),x),y);
        strip->raise();
    }

    // Overlays for code blocks that no longer exist (a recycled bubble now showing a shorter
    // message) are destroyed with the QPointer entries themselves when m_codeBlocks is rebuilt;
    // this catches any left parented to the viewport in the meantime.
    for (auto* orphan : viewport()->findChildren<QFrame*>(QStringLiteral("codeBlockOverlay")))
    {
        const bool tracked=std::any_of(m_codeBlocks.begin(),m_codeBlocks.end(),
            [orphan](const TrackedCodeBlock& c){ return c.overlay.data()==orphan; });
        if (!tracked)
        {
            orphan->hide();
            orphan->deleteLater();
        }
    }

    updateCodeBlockOverlayVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateCodeBlockOverlayVisibility()
{
    // Deliberately coarse, exactly like updateTableExpandButtonVisibility(): one answer for every
    // code block in the message rather than hit-testing which one the pointer is over. underMouse()
    // stays true while the pointer is on the strip itself, since it is a child of viewport().
    const bool visible=m_codeBlockOverlay && (!m_codeBlockOverlayOnHover || underMouse());
    for (auto& tracked : m_codeBlocks)
    {
        if (!tracked.overlay.isNull())
        {
            tracked.overlay->setVisible(visible && codeBlockViewportRect(tracked).isValid());
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::paintEvent(QPaintEvent* event)
{
    if (!m_codeBlocks.empty())
    {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing,true);
        painter.setPen(Qt::NoPen);

        auto* doc=document();
        for (const auto& codeBlock : m_codeBlocks)
        {
            const auto block=doc->findBlock(codeBlock.firstPosition);
            if (!block.isValid())
            {
                continue;
            }

            // The block's own brush, i.e. whatever messagetext.css's `pre { background-color }`
            // resolved to for the current theme. No second source, so nothing to keep in step.
            const auto brush=block.blockFormat().background();
            if (brush.style()==Qt::NoBrush)
            {
                continue;
            }

            const auto rect=codeBlockViewportRect(codeBlock);
            if (rect.isValid() && rect.intersects(event->rect()))
            {
                painter.setBrush(brush);
                painter.drawRoundedRect(rect,m_codeBlockRadius,m_codeBlockRadius);
            }
        }
    }

    // Painted UNDER the document, not over it: the base class draws the text, and Qt will also
    // repaint the block's own (text-hugging) background on top of ours -- same colour, so the
    // union is simply the inflated box.
    QTextBrowser::paintEvent(event);
}

//--------------------------------------------------------------------------

QSize ChatMessageTextBrowser::sizeHint() const
{
    // A viewer fills the window it was given rather than shrink-wrapping to its content, and its
    // scrollbars are permanent rather than reserved -- see setViewerMode().
    if (m_viewerMode)
    {
        return QTextBrowser::sizeHint();
    }

    if (document())
    {
        QSizeF docSize = document()->size();
        int height = static_cast<int>(docSize.height() + 2 * frameWidth());
        int width = static_cast<int>(document()->idealWidth() + 2 * frameWidth());

        // Same clamp, and for the same reason, as updateSize(): a pinned wide table inflates
        // idealWidth() beyond the wrap width, and reporting THAT here would ask the layout for a
        // bubble as wide as the table -- defeating maxBubbleWidth and the whole point of scrolling
        // to the overflow instead. ChatMessageText::bubbleWidthHint() already clamps its own
        // return value; this is the same guarantee for Qt's direct sizeHint() path.
        auto cap=lineWrapColumnOrWidth();
        if (cap>0 && width>cap+2*frameWidth())
        {
            width=cap+2*frameWidth();
        }

        // An as-needed horizontal scrollbar is drawn INSIDE this widget, so its height has to be
        // part of the hint or the last line of the document is clipped behind it -- the same
        // bookkeeping NavigationBar does for its own as-needed horizontal bar (see
        // src/navigationbar.cpp's sizeHint()). Gated on m_hScrollReserved (set synchronously by
        // applyWideTableLayout()), NOT horizontalScrollBar()->isVisible(): that flag only catches
        // up once the scroll area has actually re-laid out its viewport at the new geometry, one
        // pass behind applyWideTableLayout()'s own pin/un-pin decision -- during a bubble-width
        // negotiation this hint and lastLineRect() (also gated on m_hScrollReserved) are read
        // from the SAME pass, so both must agree with what was just decided, not with what the
        // scroll area has gotten around to rendering yet.
        if (horizontalScrollBar()!=nullptr && m_hScrollReserved)
        {
            height+=horizontalScrollBar()->sizeHint().height();
        }

        return QSize{width,height};
    }

    return QTextBrowser::sizeHint();
}

//--------------------------------------------------------------------------

bool ChatMessageTextBrowser::canScrollHorizontally() const
{
    // The live RANGE, not the policy or m_hScrollReserved: those record what this pass decided,
    // while this has to answer "is there anywhere to go right now" for a gesture arriving between
    // passes. A bubble with nothing to scroll must fall straight through to the list.
    const auto* bar=horizontalScrollBar();
    return bar!=nullptr && bar->maximum()>bar->minimum();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::wheelEvent(QWheelEvent *event)
{
    // A bubble refuses the wheel so the chat LIST scrolls under the pointer instead; a standalone
    // viewer is the thing that should scroll. See setViewerMode().
    if (m_viewerMode)
    {
        QTextBrowser::wheelEvent(event);
        return;
    }

    // ...with one exception: a bubble that has somewhere to scroll HORIZONTALLY (a wide code block
    // or a pinned table) takes a predominantly-horizontal gesture for itself. The two uses do not
    // actually compete -- a wheel event carries both axes, the list only ever wants the vertical
    // one, and a mouse wheel produces no horizontal delta at all -- so an ordinary vertical scroll
    // over a code block still reaches the list exactly as before.
    //
    // What WOULD break the list is deciding per event: a touchpad swipe is never purely vertical,
    // so a few stray horizontal pixels mid-gesture would silently eat frames from the list and
    // read as stutter. Hence the axis is latched ONCE per gesture (ScrollBegin..ScrollEnd) and
    // held, which is the same axis-lock every native scroll view uses. A mouse, which reports no
    // phase at all, is decided per event -- it has no gesture to latch.
    if (!m_horizontalWheelScroll || !canScrollHorizontally())
    {
        m_wheelAxis=WheelAxis::Undecided;
        event->ignore();
        return;
    }

    const auto delta=event->angleDelta();
    const auto phase=event->phase();

    if (phase==Qt::ScrollBegin)
    {
        m_wheelAxis=WheelAxis::Undecided;
    }

    auto axis=m_wheelAxis;
    if (axis==WheelAxis::Undecided && !delta.isNull())
    {
        axis=(qAbs(delta.x())>qAbs(delta.y())) ? WheelAxis::Horizontal : WheelAxis::Vertical;
        // Latched only for a real gesture. Qt::NoScrollPhase is a mouse wheel, where every event
        // stands alone, so carrying a decision over from the previous notch would be wrong.
        if (phase!=Qt::NoScrollPhase)
        {
            m_wheelAxis=axis;
        }
    }

    if (phase==Qt::ScrollEnd)
    {
        m_wheelAxis=WheelAxis::Undecided;
    }

    auto* bar=horizontalScrollBar();
    if (axis!=WheelAxis::Horizontal || bar==nullptr)
    {
        event->ignore();
        return;
    }

    const auto pixels=event->pixelDelta().x();
    // pixelDelta() is exact and is what a touchpad reports; angleDelta() is the fallback for
    // devices that only report notches (8 units per degree, 15 degrees per notch by Qt's own
    // convention -- one singleStep per notch is what QAbstractScrollArea itself would apply).
    const auto step=(pixels!=0) ? pixels : (delta.x()/(8*15))*bar->singleStep();

    const auto target=qBound(bar->minimum(),bar->value()-step,bar->maximum());
    if (target==bar->value())
    {
        // Already at the end in this direction: CHAIN to the parent rather than swallowing the
        // rest of the gesture. The chat view scrolls horizontally too once the window is too
        // narrow for its bubbles, and a bubble that kept eating horizontal input would make that
        // unreachable wherever a code block happened to sit under the pointer. Standard nested-
        // scroll behaviour -- the inner view consumes what it can use, the outer gets the rest.
        event->ignore();
        return;
    }

    bar->setValue(target);
    event->accept();
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
    // setHtml() does NOT replace the underlying QTextDocument object -- QWidgetTextControlPrivate::
    // setContent() only allocates a new one when this widget has none yet, otherwise it calls
    // doc->setHtml() on the SAME object (verified against Qt 6.9.0 source; the comment this
    // replaced claimed the opposite). Any style already set via setDefaultStyleSheet() before
    // this call is therefore naturally still in effect, AND a SyntaxHighlighter already attached
    // to document() survives this call unaffected -- nothing else to do for either concern.
    if (m_syntaxHighlightingEnabled)
    {
        ensureSyntaxHighlighter(html);
    }
    // Both of these re-derive per-message state that setHtml() has just discarded along with the
    // old document. Code blocks first: it clears QTextBlockFormat::nonBreakableLines(), which the
    // wide-table pass has no opinion about but which anything reading block formats afterwards
    // would otherwise see in a transient state.
    applyCodeBlockLayout();
    // setHtml() rebuilt the document, so any table pinned for the PREVIOUS content is gone along
    // with it -- re-measure and re-pin for this one (task-message-formatting-plan.md, Stage 4).
    applyWideTableLayout();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setPlainTextContent(const QString& text)
{
    // WITHOUT clearing m_lastHtml here, the next QEvent::StyleChange's applyDocumentStyle() would
    // replay a PREVIOUS message's HTML back over this plain-text content (m_lastHtml is otherwise
    // only ever cleared by loading new HTML) -- in a recycled flyweight bubble, potentially
    // someone else's message. See this method's own header doc comment.
    m_lastHtml.clear();
    setPlainText(text);
    // Plain text has no code blocks -- run the pass anyway rather than clearing m_codeBlocks by
    // hand: it finds none, and its own empty path is what destroys the previous content's overlay
    // strips (a bare clear() would drop the QPointers and leave the widgets parented to viewport()
    // and visible) and clears the code half of the shared scrollbar decision.
    applyCodeBlockLayout();
    // Plain text has no tables -- this clears any pin/button left over from previous HTML content
    // (whose document setPlainText() has just discarded) and puts the scrollbar policy back.
    applyWideTableLayout();
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

void ChatMessageTextBrowser::setMentionColor(const QColor& color)
{
    if (m_mentionColor==color)
    {
        return;
    }
    m_mentionColor=color;
    applyDocumentStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setSyntaxHighlightingEnabled(bool enable)
{
    if (m_syntaxHighlightingEnabled==enable)
    {
        return;
    }
    m_syntaxHighlightingEnabled=enable;

    if (!enable)
    {
        if (m_highlighter!=nullptr)
        {
            // setDocument(nullptr) clears every layout format the highlighter applied
            // (QSyntaxHighlighter::setDocument()'s own blk.layout()->clearFormats() loop), but
            // does so inside an edit block that leaves QTextDocumentPrivate's docChangeFrom at
            // -1 -- the layout is never told, so nothing repaints on its own. Force it.
            m_highlighter->setDocument(nullptr);
            delete m_highlighter;
            m_highlighter=nullptr;
            viewport()->update();
        }
        return;
    }

    ensureSyntaxHighlighter(m_lastHtml);
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

    // Stage 6: a mention reads as distinct from an ordinary link. Appended after the blanket `a`
    // rule for belt and braces, though measured it wins either way -- Qt's CSS engine uses
    // ordinary specificity, and an attribute selector outranks a bare element selector regardless
    // of append order. Emitted only when a colour is actually set, so an unstyled host leaves a
    // mention on the blanket `a` rule above rather than on Qt's own baked #0000ff default.
    if (m_mentionColor.isValid())
    {
        css+=QStringLiteral("\na[href^=\"%1:\"] { color: %2; text-decoration: %3; }")
                .arg(mentionUrlScheme(),
                     m_mentionColor.name(),
                     m_linkUnderline ? QStringLiteral("underline") : QStringLiteral("none"));
    }

    document()->setDefaultStyleSheet(css);

    if (m_syntaxHighlightingEnabled)
    {
        // Idempotent in the overwhelmingly common case (a highlighter attached during the
        // original setHtmlContent()/setSyntaxHighlightingEnabled(true) call already exists here
        // and this just re-syncs its document() pointer, a no-op absent an external
        // QTextEdit::setDocument() call) -- see ensureSyntaxHighlighter()'s own doc comment.
        ensureSyntaxHighlighter(m_lastHtml);
        if (m_highlighter!=nullptr)
        {
            // Re-pulled BEFORE the replay/rehighlight below, so either path repaints with the
            // NEW palette in one pass rather than the old one followed immediately by a second.
            m_highlighter->refreshColors();
        }
    }

    // setDefaultStyleSheet() only affects content set AFTERWARDS -- reapply the last HTML we
    // know about so an already-rendered bubble picks up a theme/color change immediately (e.g.
    // Style::updateWidgetStyle()'s repolish on a light/dark switch) instead of only the next
    // message that happens to load.
    if (!m_lastHtml.isEmpty())
    {
        setHtml(m_lastHtml);
        // The replay rebuilt the document, discarding every per-message format derived from the
        // PREVIOUS load along with it -- both passes have to run again, exactly as they do after
        // setHtmlContent()'s own setHtml().
        //
        // Code blocks first, same order as setHtmlContent(). Omitting this was a real bug: the
        // replay restores QTextBlockFormat::nonBreakableLines() from the `<pre>` markup and drops
        // the padding margins, so without re-running this pass a code block kept its natural width
        // with nothing tracking it -- no padding, no overflow verdict, and (because m_codeBlocks
        // still held the previous pass' entries) a slab painted from stale positions. The visible
        // symptom was a code block clipped at the bubble's edge with no scrollbar, on any bubble
        // that had been through a style/theme pass -- i.e. in practice, all of them.
        applyCodeBlockLayout();
        applyWideTableLayout();
        // The reload above reset every anchor to the base style, including one that was mid-hover
        // -- re-apply its hover-underline immediately rather than waiting for the next mouse move.
        if (!m_hoveredAnchor.isEmpty())
        {
            setAnchorUnderline(m_hoveredAnchor,true);
        }
    }
    else if (m_highlighter!=nullptr)
    {
        // No HTML to replay (e.g. this bubble currently holds plain text) -- nothing else would
        // repaint the code-block colours, so trigger the pass explicitly rather than leaving them
        // stale until the next message that happens to load.
        m_highlighter->rehighlight();
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::ensureSyntaxHighlighter(const QString& html)
{
    if (m_highlighter!=nullptr)
    {
        // Defensive only -- nothing in this codebase calls QTextEdit::setDocument() on a live
        // ChatMessageTextBrowser today, but SyntaxHighlighter's underlying QPointer<QTextDocument>
        // would otherwise go silently inert (not crash) if it ever did; see this method's own
        // header doc comment.
        if (m_highlighter->document()!=document())
        {
            m_highlighter->setDocument(document());
        }
        return;
    }

    // Cheap reject first -- avoids walking the document at all for the overwhelmingly common
    // case of a message with no code block.
    if (!html.contains(QStringLiteral("<pre"),Qt::CaseInsensitive))
    {
        return;
    }

    // The exact predicate: only the ALREADY-PARSED document is authoritative on whether
    // QTextFormat::BlockCodeLanguage actually survived -- immune to how the class="language-x"
    // attribute happened to be quoted/cased in the source markup, and what actually keeps an
    // untagged fence from attaching a highlighter at all (decision 4: no tag, no highlighting).
    if (!documentHasCodeLanguage())
    {
        return;
    }

    m_highlighter=new SyntaxHighlighter(document());
    // Mandatory, not an optimisation -- attaching to this already-non-empty document set Qt's own
    // rehighlightPending flag and silently discarded the very reformat this call now replaces
    // (see this method's own header doc comment for the qsyntaxhighlighter.cpp citations).
    m_highlighter->rehighlight();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::resizeEvent(QResizeEvent* event)
{
    QTextBrowser::resizeEvent(event);
    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setWideTableScrollEnabled(bool enable)
{
    if (m_wideTableScroll==enable)
    {
        return;
    }
    m_wideTableScroll=enable;

    if (!enable && !m_lastHtml.isEmpty())
    {
        // Cheapest way to discard every pinned QTextTableFormat so Qt goes back to compressing
        // tables to fit: rebuild the document. applyWideTableLayout() below then re-tracks the
        // tables WITHOUT pinning any of them -- expand buttons stay, they are governed
        // separately by tableExpandButton.
        setHtml(m_lastHtml);
    }

    applyWideTableLayout();
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setTableExpandButtonEnabled(bool enable)
{
    if (m_tableExpandButton==enable)
    {
        return;
    }
    m_tableExpandButton=enable;

    if (!enable)
    {
        for (auto& tracked : m_tables)
        {
            if (!tracked.button.isNull())
            {
                // Deleted outright, not deleteLater()'d, for the same reason as
                // applyWideTableLayout()'s own dropUnusedButtons(): a deferred delete leaves the
                // widget parented and findable until the event loop next spins.
                delete tracked.button.data();
                tracked.button=nullptr;
            }
        }
        return;
    }

    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setTableExpandButtonVisibleOnHover(bool enable)
{
    if (m_tableExpandButtonOnHover==enable)
    {
        return;
    }
    m_tableExpandButtonOnHover=enable;
    updateTableExpandButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateTableExpandButtonVisibility()
{
    // Deliberately coarse: one answer for every table in the message rather than hit-testing
    // which table the pointer is over. Note underMouse() stays true while the pointer is on one
    // of the buttons themselves -- they are children of viewport(), and Qt only sends Leave to
    // widgets BELOW the common ancestor of the old and new widget
    // (QApplicationPrivate::dispatchEnterLeave), so a button cannot hide itself out from under
    // the cursor.
    bool visible=!m_tableExpandButtonOnHover || underMouse();
    for (auto& tracked : m_tables)
    {
        if (!tracked.button.isNull())
        {
            tracked.button->setVisible(visible);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::enterEvent(QEnterEvent* event)
{
    QTextBrowser::enterEvent(event);
    updateTableExpandButtonVisibility();
    updateCodeBlockOverlayVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyWideTableLayout()
{
    // Buttons are REUSED across passes, not torn down and rebuilt. applyWideTableLayout() runs on
    // every setWrapWidth(), i.e. on every bubble-width negotiation, so recreating them here meant
    // an icon lookup plus a widget construction per table per negotiation -- and, because
    // deleteLater() only runs when the event loop next spins, the outgoing buttons briefly
    // co-existed with their replacements.
    std::vector<QPointer<QWidget>> recycled;
    recycled.reserve(m_tables.size());
    for (auto& tracked : m_tables)
    {
        recycled.push_back(tracked.button);
    }
    m_tables.clear();

    auto dropUnusedButtons=[&recycled](std::size_t keep)
    {
        for (std::size_t i=keep;i<recycled.size();++i)
        {
            if (!recycled[i].isNull())
            {
                // Deleted outright rather than deleteLater()'d: nothing here is running inside one
                // of these buttons' own signal handlers, and a deferred delete would leave the
                // widget parented (and findable) until the next event-loop turn.
                delete recycled[i].data();
            }
        }
        recycled.clear();
    };

    if (document()==nullptr)
    {
        dropUnusedButtons(0);
        m_anyTablePinned=false;
        updateHorizontalOverflow();
        return;
    }

    std::vector<QTextTable*> tables;
    for (auto* frame : document()->rootFrame()->childFrames())
    {
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table!=nullptr)
        {
            tables.push_back(table);
        }
    }
    if (tables.empty())
    {
        dropUnusedButtons(0);
        m_anyTablePinned=false;
        updateHorizontalOverflow();
        return;
    }

    // Drop any override left from a PREVIOUS pass before measuring: a table pinned for a narrow
    // bubble must be able to un-pin when the bubble grows, and measuring a still-pinned table
    // would just read its pinned width straight back as if that were its natural one.
    for (auto* table : tables)
    {
        auto format=table->format();
        if (format.width().type()!=QTextLength::VariableLength)
        {
            format.setWidth(QTextLength());
            table->setFormat(format);
        }
    }

    // The width each table WANTS, measured by laying the whole document out unconstrained.
    // Only the tables' own rects are read back, so the fact that this also unwraps every
    // paragraph for the duration does not matter -- the real wrap width is restored below,
    // before anything can be painted at this width.
    auto restoreWidth=document()->textWidth();
    document()->setTextWidth(-1);
    std::vector<qreal> naturalWidths;
    naturalWidths.reserve(tables.size());
    for (auto* table : tables)
    {
        naturalWidths.push_back(document()->documentLayout()->frameBoundingRect(table).width());
    }
    document()->setTextWidth(restoreWidth);

    // EVERY table is tracked, not just the wide ones -- the expand button is offered on all of
    // them (a table that fits is still worth opening larger, and it is the only way a short table
    // can be copied AS a table). Pinning stays restricted to those that genuinely do not fit.
    auto wrapWidth=lineWrapColumnOrWidth();
    bool anyPinned=false;
    for (std::size_t i=0;i<tables.size();++i)
    {
        TrackedTable tracked;
        tracked.firstPosition=tables[i]->firstPosition();
        tracked.naturalWidth=naturalWidths[i];

        // A wrap width of 0 means none has been negotiated yet (applyWideTableLayout() also runs
        // from setHtmlContent(), which can precede the first bubbleWidthHint()); there is nothing
        // to compare against, so nothing is pinned this pass -- setWrapWidth() calls back once it
        // knows. Tracking and the buttons do not depend on it, so they happen either way.
        // Otherwise: only a table that does not already fit is worth pinning -- leaving a fitting
        // table alone keeps Qt's own column balancing, which is better than a hard pin.
        if (m_wideTableScroll && wrapWidth>0 && naturalWidths[i]>static_cast<qreal>(wrapWidth))
        {
            auto format=tables[i]->format();
            format.setWidth(QTextLength(QTextLength::FixedLength,naturalWidths[i]));
            tables[i]->setFormat(format);
            tracked.pinned=true;
            anyPinned=true;
        }

        if (i<recycled.size())
        {
            tracked.button=recycled[i];
        }
        m_tables.push_back(tracked);
    }
    dropUnusedButtons(tables.size());

    // The scrollbar policy and height reservation are decided by updateHorizontalOverflow(), which
    // owns them for BOTH overflow sources -- setting them here would switch the bar back off for a
    // code block that had just decided it needs one. See that method's own doc comment.
    m_anyTablePinned=anyPinned;
    updateHorizontalOverflow();
    if (anyPinned)
    {
        // The pins changed the layout; re-shrink-wrap so the document's own height reflects the
        // now-unwrapped cells (a pinned table is typically much SHORTER than the compressed one).
        updateSize();
    }
    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setHScrollReserved(bool reserve)
{
    if (m_hScrollReserved==reserve)
    {
        return;
    }
    m_hScrollReserved=reserve;
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateTableExpandButtons()
{
    if (document()==nullptr)
    {
        return;
    }

    if (!m_tableExpandButton)
    {
        return;
    }

    for (std::size_t i=0;i<m_tables.size();++i)
    {
        auto& tracked=m_tables[i];

        // Re-find the table by the position recorded when it was tracked -- the QTextTable* itself
        // is not safe to keep across a re-layout.
        auto* frame=document()->frameAt(tracked.firstPosition);
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table==nullptr)
        {
            if (!tracked.button.isNull())
            {
                tracked.button->hide();
            }
            continue;
        }

        if (tracked.button.isNull())
        {
            // Icon-only IconTextButton with the curated tabler "arrows-diagonal" glyph, resolved
            // through the ordinary SVG icon locator so it follows the icon theme (and any label's
            // icon substitutions) like every other icon in the library. The alias lives in
            // resources/style/chat.json under this class's own context name.
            auto* button=new IconTextButton(
                Style::instance().svgIconLocator().icon(
                    QStringLiteral("ChatMessageTextBrowser::expandTable"),this),
                viewport());
            button->setObjectName(QStringLiteral("tableExpandButton"));
            button->setCursor(Qt::ArrowCursor);
            button->setFocusPolicy(Qt::NoFocus);
            button->setToolTip(tr("Show the full table"));
            auto index=static_cast<int>(i);
            connect(button,&IconTextButton::clicked,this,[this,index](){openTableViewer(index);});
            tracked.button=button;
        }

        auto* button=qobject_cast<IconTextButton*>(tracked.button.data());
        if (button==nullptr)
        {
            continue;
        }

        auto rect=document()->documentLayout()->frameBoundingRect(table);

        // adjustSize() rather than positioning straight from sizeHint(): the button's size is
        // owned by QSS (min-/max-width in chat.qss), and a font-size large enough to make the
        // glyph readable can push sizeHint() past that max -- positioning from the unclamped hint
        // would then shove the button left of where it actually ends up. adjustSize() applies the
        // constraints, so width()/height() below are the real, final size.
        button->adjustSize();
        auto size=button->size();

        // Anchored to the VIEWPORT's right edge, not the table's -- a pinned table's own right
        // edge is by definition scrolled off-screen, so a button placed there would be invisible
        // until the user had already scrolled to the far end (i.e. exactly when they no longer
        // need it). Only the vertical position tracks the table, and this widget never scrolls
        // vertically (sizeHint() reports the full document height; the chat list scrolls instead),
        // so no vertical scroll offset is involved.
        constexpr int margin=4;
        auto x=viewport()->width()-size.width()-margin;
        auto y=static_cast<int>(rect.top())+margin;
        button->move(x,y);
        button->raise();
    }

    // Newly created buttons must start out matching the current hover state rather than simply
    // appearing -- otherwise a message that re-lays out while the pointer is elsewhere would
    // flash its buttons on.
    updateTableExpandButtonVisibility();

    // Piggy-backing on the same pass on purpose: this is the one that already runs on every
    // bubble-width negotiation, and a code block's overlay has to follow its block when the
    // width changes just as a table's button does.
    updateCodeBlockOverlays();
}

//--------------------------------------------------------------------------

/******************************ChatMessageTableViewer**************************/

//--------------------------------------------------------------------------

namespace {

//! Text of one table cell, blocks joined by a space -- a cell that wrapped onto several blocks is
//! still ONE field in a tab-separated row, so its own internal line breaks must not become row
//! breaks in the output.
QString tableCellText(const QTextTableCell& cell)
{
    QStringList parts;
    for (auto it=cell.begin();it!=cell.end();++it)
    {
        auto text=it.currentBlock().text().trimmed();
        if (!text.isEmpty())
        {
            parts<<text;
        }
    }
    return parts.join(QLatin1Char(' '));
}

/**
 * @brief Tab-separated rendering of the cells a selection covers.
 * @param selectionStart/selectionEnd Document positions; pass -1 for "the whole table".
 *
 * Cells are joined by '\t' and rows by '\n' -- the convention every spreadsheet expects, and the
 * one thing Qt's own text/plain for a table selection does not do (it emits one cell per line).
 */
QString tableToTabSeparated(QTextTable* table, int selectionStart, int selectionEnd)
{
    QString out;
    for (int r=0;r<table->rows();++r)
    {
        QStringList cells;
        for (int c=0;c<table->columns();++c)
        {
            auto cell=table->cellAt(r,c);
            if (!cell.isValid())
            {
                continue;
            }
            // A merged cell is reported at every position it spans -- take it only at its own
            // origin, the same dedup markdownrenderer.cpp's writeTable() does.
            if (cell.row()!=r || cell.column()!=c)
            {
                continue;
            }
            if (selectionStart>=0)
            {
                auto cellStart=cell.firstCursorPosition().position();
                auto cellEnd=cell.lastCursorPosition().position();
                if (cellEnd<selectionStart || cellStart>selectionEnd)
                {
                    continue;
                }
            }
            cells<<tableCellText(cell);
        }
        if (!cells.isEmpty())
        {
            out+=cells.join(QLatin1Char('\t'));
            out+=QLatin1Char('\n');
        }
    }
    return out;
}

//! Floor a viewer may be shrunk to once it is open -- small enough to get out of the way, large
//! enough to still be a viewer. See applyExpandedViewerSize().
constexpr int ViewerMinWidth=280;
constexpr int ViewerMinHeight=200;

/**
 * @brief Initial size for an expanded viewer (table or code), in window coordinates.
 *
 * At least HALF the window it was opened from in each direction: the whole point of expanding is
 * to see more than the bubble showed, and the previous fixed 400px height plus a content-derived
 * width routinely opened a window smaller than the bubble's own content on a large display. The
 * content's own width still wins where it is wider, and the window itself is the ceiling -- a
 * viewer larger than the window it came from cannot be positioned sensibly.
 *
 * @param contentWidth The content's own preferred width, already including whatever slack the
 *  caller wants for a frame and scrollbar.
 */
QSize expandedViewerSize(const QWidget* anchor, int contentWidth)
{
    const auto* win=(anchor!=nullptr) ? anchor->window() : nullptr;
    // A widget with no window yet (constructed off-screen) has nothing to take a fraction OF --
    // fall back to the fixed size this used to open at rather than to zero.
    const QSize windowSize=(win!=nullptr && win->width()>0 && win->height()>0)
                           ? win->size() : QSize{900,400};

    const int w=qMin(qMax(contentWidth,windowSize.width()/2),windowSize.width());
    const int h=qMin(qMax(400,windowSize.height()/2),windowSize.height());
    return QSize{w,h};
}

/**
 * @brief Open `frame` at `size`, then let the user shrink it again.
 *
 * Sizing a FloatingDialogFrame is not a resize() away: popup() calls QWidget::adjustSize(), which
 * takes the frame's SIZE HINT and throws away any geometry set beforehand -- so a resize() on the
 * content (what this used to do) had no effect at all, and the viewer opened at whatever its
 * content happened to hint. For a code viewer that is QTextBrowser's own small default, which is
 * how an expanded code block ended up smaller than the bubble it came from.
 *
 * minimumSize is the one channel adjustSize() must honour, so the size is imposed that way and
 * then relaxed to a usable floor once the frame has taken it. Relaxing afterwards does not resize
 * anything -- the frame already has its geometry -- it only stops the initial size from becoming
 * a permanent lower bound the user cannot drag back. Doing it BEFORE popup() also keeps popup()'s
 * own centring correct, which resizing afterwards would not.
 */
void applyExpandedViewerSize(QWidget* container, FloatingDialogFrame* frame, const QSize& size)
{
    container->setMinimumSize(size);
    frame->popup();
    container->setMinimumSize(qMin(ViewerMinWidth,size.width()),
                              qMin(ViewerMinHeight,size.height()));
}

}

//--------------------------------------------------------------------------

ChatMessageTableViewer::ChatMessageTableViewer(QWidget* parent) : QTextBrowser(parent)
{
    setReadOnly(true);
    setOpenLinks(false);
    setLineWrapMode(QTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

//--------------------------------------------------------------------------

QTextTable* ChatMessageTableViewer::tableOfDocument() const
{
    if (document()==nullptr)
    {
        return nullptr;
    }
    for (auto* frame : document()->rootFrame()->childFrames())
    {
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table!=nullptr)
        {
            return table;
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

Toast* ChatMessageTableViewer::ensureToast()
{
    if (m_toast!=nullptr)
    {
        return m_toast;
    }
    if (m_ownToast==nullptr)
    {
        // Parented to the hosting window rather than to this browser: a Toast child of a
        // QAbstractScrollArea would be positioned against the widget rather than its viewport,
        // and the window is the surface the confirmation logically belongs to anyway.
        auto* host=window();
        m_ownToast=new Toast(host!=nullptr ? host : static_cast<QWidget*>(this));

        // Drawn as a CHILD widget rather than Toast's default Qt::Tool window. A Tool window
        // would already be safe -- FloatingDialogFrame's outside-click filter explicitly exempts
        // Qt::Tool/Popup/ToolTip (see isOutsidePress() in floatingdialog.cpp) -- but a child
        // cannot cause a window activation change at all, which makes "copying must not close
        // the expanded view" structural instead of dependent on that exemption staying in place.
        m_ownToast->setDrawInParent(true);
    }
    return m_ownToast;
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::copyTable()
{
    // Going through selectAll()+copy() rather than assembling a QMimeData by hand keeps this on
    // the ordinary copy path, so the clipboard gets exactly the same flavour set (html, markdown,
    // ODF, and our own text/plain override) as a manual selection would.
    selectAll();
    copy();

    // The select-all is a means to that end, not something the user asked for -- leaving the whole
    // table highlighted afterwards just looks like a stray selection. The toast is the
    // confirmation. Cleared AFTER copy(), which has already taken the clipboard data.
    auto cursor=textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);

    // Emitted unconditionally, before the toast: a host that turned the built-in toast off is
    // relying on this to show its own.
    emit tableCopied();

    if (m_copyToastEnabled)
    {
        auto* toast=ensureToast();
        if (toast!=nullptr)
        {
            toast->show(tr("Copied"));
        }
    }
}

//--------------------------------------------------------------------------

QMimeData* ChatMessageTableViewer::createMimeDataFromSelection() const
{
    auto* mime=QTextBrowser::createMimeDataFromSelection();
    if (mime==nullptr)
    {
        return mime;
    }

    auto* table=tableOfDocument();
    if (table!=nullptr)
    {
        auto cursor=textCursor();
        auto tabbed=cursor.hasSelection()
                        ? tableToTabSeparated(table,cursor.selectionStart(),cursor.selectionEnd())
                        : tableToTabSeparated(table,-1,-1);
        if (!tabbed.isEmpty())
        {
            // Replaces ONLY text/plain; text/html, text/markdown and ODF stay as Qt built them.
            mime->setText(tabbed);
        }
    }

    return mime;
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    auto* copyAction=menu.addAction(tr("Copy"));
    copyAction->setEnabled(textCursor().hasSelection());
    connect(copyAction,&QAction::triggered,this,[this](){copy();});

    auto* copyTableAction=menu.addAction(tr("Copy table"));
    connect(copyTableAction,&QAction::triggered,this,[this](){copyTable();});

    menu.addSeparator();

    auto* selectAllAction=menu.addAction(tr("Select all"));
    connect(selectAllAction,&QAction::triggered,this,[this](){selectAll();});

    menu.exec(event->globalPos());
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::keyPressEvent(QKeyEvent* event)
{
    // QTextEdit::copy() returns early when the cursor has no selection, so a plain Ctrl+C in a
    // freshly opened viewer would do nothing at all -- which reads as "copying is broken" rather
    // than "you were supposed to select something first". Copy the whole table instead.
    if (event->matches(QKeySequence::Copy) && !textCursor().hasSelection())
    {
        copyTable();
        event->accept();
        return;
    }
    QTextBrowser::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::openTableViewer(int index)
{
    if (index<0 || static_cast<std::size_t>(index)>=m_tables.size() || document()==nullptr)
    {
        return;
    }

    auto* table=qobject_cast<QTextTable*>(document()->frameAt(m_tables[static_cast<std::size_t>(index)].firstPosition));
    if (table==nullptr)
    {
        return;
    }

    // Select the whole frame, boundaries included -- Qt's own documented idiom for extracting a
    // frame (the positions either side of firstPosition()/lastPosition() are the frame markers;
    // selecting strictly between them would yield the cell contents WITHOUT the table structure).
    QTextCursor cursor(document());
    cursor.setPosition(std::max(0,table->firstPosition()-1));
    cursor.setPosition(std::min(document()->characterCount()-1,table->lastPosition()+1),
                       QTextCursor::KeepAnchor);
    auto tableHtml=cursor.selection().toHtml();

    auto* container=new QFrame();
    container->setObjectName(QStringLiteral("tableViewerFrame"));
    auto* containerLayout=Layout::vertical(container);

    auto* view=new ChatMessageTableViewer(container);
    view->setObjectName(QStringLiteral("tableViewer"));
    // Same document CSS the bubble itself uses, so the expanded table keeps the theme's own
    // border/padding treatment (resources/style/messagetext.css) instead of Qt's bare defaults.
    view->document()->setDefaultStyleSheet(Style::instance().css());
    view->setHtml(tableHtml);
    containerLayout->addWidget(view,1);

    // Copying already works by selection + Ctrl+C (Qt puts text/html and text/markdown on the
    // clipboard, which is what external apps paste a real table from) -- this button is purely
    // about discoverability, since nothing else tells the user that is possible.
    auto* buttonRow=new QFrame(container);
    buttonRow->setObjectName(QStringLiteral("tableViewerButtons"));
    auto* buttonLayout=Layout::horizontal(buttonRow);
    buttonLayout->addStretch(1);
    // uise::PushButton rather than a bare QPushButton: it carries the library's own click-ripple
    // overlay (see its rippleOverlay()), so the button gives the same feedback as every other
    // button in the app instead of silently doing something invisible.
    auto* fullScreenButton=new PushButton(tr("Full screen"),buttonRow);
    fullScreenButton->setObjectName(QStringLiteral("tableViewerFullScreenButton"));
    buttonLayout->addWidget(fullScreenButton);

    auto* copyButton=new PushButton(tr("Copy table"),buttonRow);
    copyButton->setObjectName(QStringLiteral("tableViewerCopyButton"));
    connect(copyButton,&PushButton::clicked,view,[view](){view->copyTable();});
    buttonLayout->addWidget(copyButton);

    auto* closeButton=new PushButton(tr("Close"),buttonRow);
    closeButton->setObjectName(QStringLiteral("tableViewerCloseButton"));
    buttonLayout->addWidget(closeButton);

    containerLayout->addWidget(buttonRow);

    const auto viewerSize=expandedViewerSize(
        this,static_cast<int>(m_tables[static_cast<std::size_t>(index)].naturalWidth)+48);

    // FloatingDialogFrame is the only shell in this library that hosts an arbitrary widget as a
    // resizable top-level window -- ChatImageViewerWindow, despite the name, is hard-wired to a
    // ChatImageViewer and has no setWidget().
    auto* frame=new FloatingDialogFrame(this);
    frame->setWidget(container,true);
    frame->setAutoCloseOnOutsideClick(true);

    // Toggle, not a one-way trip -- and the button says which way it will go. A wide table is
    // exactly the content most likely to want the whole screen, so this is more than a nicety.
    // The frame lays its content out with a QBoxLayout under SetDefaultConstraint (our content is
    // a plain QFrame, not an AbstractDialog, so it never takes FloatingDialogFrame's
    // isResizable()==false / SetFixedSize path), which is what lets the table actually grow into
    // the extra space rather than sit at its old size in a huge translucent window.
    auto toggleFullScreen=[frame,fullScreenButton]()
    {
        if (frame->isFullScreen())
        {
            frame->showNormal();
            fullScreenButton->setText(tr("Full screen"));
        }
        else
        {
            frame->showFullScreen();
            fullScreenButton->setText(tr("Exit full screen"));
        }
    };
    connect(fullScreenButton,&PushButton::clicked,frame,toggleFullScreen);

    // F11 as well, matching ChatImageViewerWindow's own fullscreen shortcut so the two viewers
    // behave the same way. Escape stays the frame's own close shortcut, as it is everywhere else.
    auto* fullScreenShortcut=new QShortcut(Qt::Key_F11,frame);
    fullScreenShortcut->setContext(Qt::WindowShortcut);
    connect(fullScreenShortcut,&QShortcut::activated,frame,toggleFullScreen);

    connect(closeButton,&PushButton::clicked,frame,[frame](){frame->close();});

    // The frame is parented to this browser, so without this every expand/close cycle would
    // leave one behind for the lifetime of the message bubble. setWidget(...,true) already
    // disposes of the content; this disposes of the shell around it.
    connect(frame,&FloatingDialogFrame::closed,frame,&QObject::deleteLater);

    // Not frame->resize(): popup() adjustSize()s over it -- see applyExpandedViewerSize().
    applyExpandedViewerSize(container,frame,viewerSize);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::openCodeBlockViewer(int index)
{
    if (index<0 || static_cast<std::size_t>(index)>=m_codeBlocks.size() || document()==nullptr)
    {
        return;
    }

    const auto& tracked=m_codeBlocks[static_cast<std::size_t>(index)];

    // Rebuilt from the block's own TEXT and language rather than taken as a selection's toHtml().
    // Qt's HTML exporter does not emit the `class="language-x"` attribute its own PARSER reads
    // back into QTextFormat::BlockCodeLanguage -- the round trip is asymmetric -- so the language
    // was lost, ensureSyntaxHighlighter()'s documentHasCodeLanguage() gate then refused to attach
    // a highlighter at all, and the expanded code came out in one flat colour.
    //
    // Going back through markdownToHtml() is also what guarantees parity rather than merely
    // approximating it: this is the exact path that produced the bubble's own HTML.
    const auto code=codeBlockText(tracked);

    // A fence long enough to survive whatever backtick runs the code itself contains -- CommonMark
    // closes a fence only on a run at least as long as the opening one, so a shorter fence around
    // code containing ``` would terminate early and the tail would render as prose.
    int longestRun=0;
    int run=0;
    for (const auto ch : code)
    {
        run=(ch==QLatin1Char('`')) ? run+1 : 0;
        longestRun=qMax(longestRun,run);
    }
    const QString fence(qMax(3,longestRun+1),QLatin1Char('`'));

    const auto codeHtml=markdownToHtml(fence+tracked.language+QStringLiteral("\n")
                                       +code+QStringLiteral("\n")+fence);

    auto* container=new QFrame();
    container->setObjectName(QStringLiteral("codeBlockViewerFrame"));
    auto* containerLayout=Layout::vertical(container);

    // The SAME widget the bubble renders through, in viewer mode -- not a second browser. That is
    // what makes the expanded code genuinely identical to the bubble's: the syntax highlighting,
    // the painted slab with its padding and radius, messagetext.css and the theme-change replay
    // are all this class's own behaviour, and a plain QTextBrowser has none of them (a
    // QSyntaxHighlighter's formats live on the block layouts, not in the document, so they do not
    // survive the toHtml() above at all -- the expanded code came out unhighlighted and with no
    // background for exactly that reason).
    auto* view=new ChatMessageTextBrowser(container);
    view->setObjectName(QStringLiteral("codeBlockViewer"));
    view->setViewerMode(true);
    view->setToast(m_toast);
    // Selectable, with the same right-click Copy the bubble offers.
    view->setCopyable(true);
    view->setSyntaxHighlightingEnabled(isSyntaxHighlightingEnabled());
    view->setCodeBlockPadding(m_codeBlockPadding);
    view->setCodeBlockRadius(m_codeBlockRadius);
    // The strip would be redundant here: the language is not in doubt once the block is open on
    // its own, and Copy/expand already live in the button row below.
    view->setCodeBlockOverlayEnabled(false);
    // setHtmlContent(), not setHtml(): it is the entry point that tracks the block, reserves the
    // padding and attaches the highlighter. setHtml() alone would load the text and none of that.
    view->setHtmlContent(codeHtml);
    containerLayout->addWidget(view,1);

    auto* buttonRow=new QFrame(container);
    buttonRow->setObjectName(QStringLiteral("codeBlockViewerButtons"));
    auto* buttonLayout=Layout::horizontal(buttonRow);
    buttonLayout->addStretch(1);

    auto* fullScreenButton=new PushButton(tr("Full screen"),buttonRow);
    fullScreenButton->setObjectName(QStringLiteral("codeBlockViewerFullScreenButton"));
    buttonLayout->addWidget(fullScreenButton);

    auto* copyButton=new PushButton(tr("Copy code"),buttonRow);
    copyButton->setObjectName(QStringLiteral("codeBlockViewerCopyButton"));
    // Copies from the ORIGINAL tracked block rather than from the viewer's own document: this is
    // the same text the overlay's Copy button puts on the clipboard (codeBlockText()'s plain,
    // un-highlighted form), so the two routes cannot disagree about what "copy this code" means.
    connect(copyButton,&PushButton::clicked,this,
        [this,index]()
        {
            if (static_cast<std::size_t>(index)<m_codeBlocks.size())
            {
                copyCodeBlock(m_codeBlocks[static_cast<std::size_t>(index)]);
            }
        }
    );
    buttonLayout->addWidget(copyButton);

    auto* closeButton=new PushButton(tr("Close"),buttonRow);
    closeButton->setObjectName(QStringLiteral("codeBlockViewerCloseButton"));
    buttonLayout->addWidget(closeButton);

    containerLayout->addWidget(buttonRow);

    // Same sizing rule as openTableViewer() -- the content's own natural width plus room for the
    // frame and a scrollbar, floored at half the window so expanding always gains real room.
    const auto viewerSize=expandedViewerSize(this,static_cast<int>(tracked.naturalWidth)+48);

    auto* frame=new FloatingDialogFrame(this);
    frame->setWidget(container,true);
    frame->setAutoCloseOnOutsideClick(true);

    auto toggleFullScreen=[frame,fullScreenButton]()
    {
        if (frame->isFullScreen())
        {
            frame->showNormal();
            fullScreenButton->setText(tr("Full screen"));
        }
        else
        {
            frame->showFullScreen();
            fullScreenButton->setText(tr("Exit full screen"));
        }
    };
    connect(fullScreenButton,&PushButton::clicked,frame,toggleFullScreen);

    auto* fullScreenShortcut=new QShortcut(Qt::Key_F11,frame);
    fullScreenShortcut->setContext(Qt::WindowShortcut);
    connect(fullScreenShortcut,&QShortcut::activated,frame,toggleFullScreen);

    connect(closeButton,&PushButton::clicked,frame,[frame](){frame->close();});

    connect(frame,&FloatingDialogFrame::closed,frame,&QObject::deleteLater);

    // Not frame->resize(): popup() adjustSize()s over it -- see applyExpandedViewerSize().
    applyExpandedViewerSize(container,frame,viewerSize);
}

//--------------------------------------------------------------------------

bool ChatMessageTextBrowser::documentHasCodeLanguage() const
{
    for (auto block=document()->begin();block!=document()->end();block=block.next())
    {
        auto fmt=block.blockFormat();
        if (fmt.hasProperty(QTextFormat::BlockCodeLanguage)
            && !fmt.stringProperty(QTextFormat::BlockCodeLanguage).isEmpty())
        {
            return true;
        }
    }
    return false;
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
    updateTableExpandButtonVisibility();
    updateCodeBlockOverlayVisibility();
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

        //! The source text/format last passed to loadText() (task-message-formatting-plan.md,
        //! Stage 2). Not consumed by anything in THIS stage -- markdownToHtml()'s output is
        //! theme-independent, so ChatMessageTextBrowser::applyDocumentStyle()'s existing
        //! m_lastHtml replay already restyles a rendered markdown bubble correctly on its own.
        //! Not needed by Stage 3 either: markdownToHtml() emits a code block's language as
        //! `<pre class="language-x">`, and Qt's own HTML parser reads "class=language-x"
        //! specifically on <pre> back into QTextFormat::BlockCodeLanguage, so that survives a
        //! setHtml() round-trip and Stage 3's highlighter can read it straight off the live
        //! rendered document. Cached anyway, since it costs one refcount bump per message, for a
        //! later stage that genuinely needs the ORIGINAL markdown source rather than the
        //! rendered document (Stage 5b re-render, Stage 6 mentions).
        QString sourceText;
        TextFormat sourceFormat=TextFormat::Markdown;
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
    pimpl->sourceText=text;
    pimpl->sourceFormat=format;

    switch (format)
    {
        case TextFormat::Html:
            pimpl->text->setHtmlContent(text);
            break;
        case TextFormat::Markdown:
        {
            // Rendered to sanitized HTML and routed through setHtmlContent() -- NOT
            // QTextBrowser::setMarkdown() directly -- so this content gets messagetext.css,
            // linkColor/linkUnderline and theme-switch replay the same way Html content already
            // does (task-message-formatting-plan.md, Stage 2; see src/newpasswordwizard.cpp's
            // own comment on why setMarkdown() bypasses setDefaultStyleSheet() entirely).
            MarkdownRenderOptions options;
            if (isMentionsEnabled())
            {
                // A caller-owned COPY -- the shipped DEFAULT options object used everywhere else
                // is never touched, so a plain markdownToHtml(src) call anywhere in the tree
                // still rejects the scheme (Stage 6).
                options.allowedLinkSchemes.append(mentionUrlScheme());
            }
            // Independent of mentionsEnabled above -- see setExtraLinkify()'s own doc comment.
            // Threaded through unconditionally when set; null is the common case and
            // markdownToHtml() already treats a null hook as "not set".
            options.extraLinkify=extraLinkify();
            pimpl->text->setHtmlContent(markdownToHtml(text,options));
            break;
        }
        case TextFormat::Plain:
            // setPlainTextContent(), not setPlainText() directly -- clears m_lastHtml, without
            // which a later theme switch would replay a PREVIOUS message's HTML back over this
            // plain-text content (task-message-formatting-plan.md, Stage 3's own fix; see
            // ChatMessageTextBrowser::setPlainTextContent()'s doc comment).
            pimpl->text->setPlainTextContent(text);
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
    pimpl->sourceText.clear();
    pimpl->sourceFormat=TextFormat::Markdown;
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

void ChatMessageText::updateMentionsEnabled()
{
    if (pimpl->sourceFormat!=TextFormat::Markdown || pimpl->sourceText.isEmpty())
    {
        // Only the Markdown branch of loadText() consults the allowlist -- Html content was
        // already rendered by the host and Plain content has no anchors at all, so there is
        // nothing here to re-render.
        return;
    }
    loadText(pimpl->sourceText,pimpl->sourceFormat);
}

//--------------------------------------------------------------------------

void ChatMessageText::updateExtraLinkify()
{
    // Same gate as updateMentionsEnabled() -- only the Markdown branch of loadText() consults
    // extraLinkify() at all.
    if (pimpl->sourceFormat!=TextFormat::Markdown || pimpl->sourceText.isEmpty())
    {
        return;
    }
    loadText(pimpl->sourceText,pimpl->sourceFormat);
}

//--------------------------------------------------------------------------

int ChatMessageText::bubbleWidthHint(int forMaxWidth)
{
    auto wrapWidth=codeAwareWrapWidth(forMaxWidth);
    pimpl->text->setWrapWidth(wrapWidth);

    // One corrective pass, and the reason it is needed rather than optional: widestCodeBlockWidth()
    // is measured by us, and it is measured too EARLY -- before the widget's style is applied, so
    // the font it measures with is not the font Qt finally lays out with. Instrumented against
    // three real code blocks, our figure came out a constant 1.075x short of Qt's every time
    // (454 vs 488, 642 vs 690, 485 vs 521): a pure font-size ratio, not an offset, which is why
    // adding a fixed fudge could never have fixed it. A block whose corrected width would have
    // cleared maxBubbleWidth but whose measured one did not therefore never widened the bubble at
    // all, and was clipped to the cap instead -- the "second block stays narrow while the first
    // stretches" case.
    //
    // QTextDocument::idealWidth(), read AFTER setWrapWidth() has laid the document out, is Qt's
    // own answer and needs no correction. It is safe to widen to it because the document is
    // wrapped at wrapWidth: prose can never push idealWidth past that, so any excess is content
    // that does not wrap -- i.e. the code block. Gated on there actually being one, so a long
    // unbreakable URL in ordinary prose still respects maxBubbleWidth.
    //
    // One pass suffices: re-laying out at the wider width cannot grow idealWidth again (the code
    // still does not wrap, and the prose only gets more room), so there is nothing to iterate.
    if (pimpl->text->isCodeBlockWidenBubbleEnabled() && pimpl->text->widestCodeBlockWidth()>0)
    {
        const auto needed=qCeil(pimpl->text->document()->idealWidth());
        if (needed>wrapWidth && forMaxWidth>wrapWidth)
        {
            wrapWidth=qMin(needed,forMaxWidth);
            pimpl->text->setWrapWidth(wrapWidth);
        }
    }
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

int ChatMessageText::codeAwareWrapWidth(int forMaxWidth) const
{
    auto wrapWidth=clampToMaxBubbleWidth(forMaxWidth);

    // task-message-formatting-plan.md: a code block may push the bubble past maxBubbleWidth, up to
    // (never beyond) whatever the negotiation itself offered. Code is the one content kind that
    // cannot re-flow -- see ChatMessageTextBrowser::codeBlockWidenBubble's own doc comment -- so
    // spending the view's spare horizontal room on it is strictly better than putting the reader
    // on a scrollbar for width the layout was willing to give. Anything the widened bubble still
    // cannot show falls through to codeBlockScroll/codeBlockExpandButton.
    //
    // qMin against forMaxWidth, not just the code width: the budget is a hard ceiling (exceeding
    // it would overflow the message list itself, not merely the bubble), and a single very long
    // line must not be allowed to ask for more than the view has.
    if (pimpl->text->isCodeBlockWidenBubbleEnabled())
    {
        const auto code=pimpl->text->widestCodeBlockWidth();
        if (code>wrapWidth)
        {
            wrapWidth=qMin(code,forMaxWidth);
        }
    }

    return wrapWidth;
}

//--------------------------------------------------------------------------

void ChatMessageText::updateMaximumBubbleWidth()
{
    auto w=codeAwareWrapWidth(chatContent()->maximumBubbleWidth());

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
    else if (pimpl->lastHintWidth>w
             && pimpl->text->isCodeBlockWidenBubbleEnabled()
             && pimpl->text->widestCodeBlockWidth()>0)
    {
        // lastHintWidth can now legitimately EXCEED what codeAwareWrapWidth() returns here:
        // bubbleWidthHint()'s corrective pass widens past maxBubbleWidth using Qt's own
        // idealWidth, which this function's own seed (our under-measured widestCodeBlockWidth())
        // does not know about. Re-wrapping at the smaller value would undo that correction and
        // clip the block again, one pass after it was fixed.
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
    // codeAwareWrapWidth() against the largest possible value is exactly maxBubbleWidth() itself
    // when the cap is enabled (>0) and no code block wants more, or a pass-through (no cap) when
    // it's disabled -- the same helper bubbleWidthHint()/updateMaximumBubbleWidth() use, so this
    // can never disagree with what they actually enforced.
    //
    // Passing INT_MAX as the budget is what makes the code-block branch report the ceiling this
    // section could ever ask for rather than what it happens to want against the CURRENT budget:
    // this is documented as a ceiling "REGARDLESS of forMaxWidth", and a caller uses it to decide
    // how much room to offer in the first place. Under-reporting it would cap the negotiation
    // below the width a wide code block is about to ask for, and the bubble would never widen.
    return codeAwareWrapWidth(std::numeric_limits<int>::max());
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
