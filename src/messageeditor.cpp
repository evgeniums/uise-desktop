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

/** @file uise/desktop/messageeditor.cpp
*
*  Defines MessageEditor.
*
*/

/****************************************************************************/

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextList>
#include <QTextTable>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QFontDatabase>
#include <QSyntaxHighlighter>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QTextImageFormat>
#include <QUrl>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QTimer>
#include <QBoxLayout>
#include <QHash>
#include <QStringView>
#include <QVariant>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/abstractspellchecker.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/emojigallerydialog.hpp>
#include <uise/desktop/voicerecorderdialog.hpp>
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/messageeditor.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Rows of MessageEditor's own Cut/Copy/Paste/Select all/Clear context menu -- ordinary
//! drop-down menu rows on the menu background, so they resolve against the generic
//! DropdownMenu context rather than owning one of their own (compare LoadControlMenu's
//! private context, which exists because those rows sit on a different background).
std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("DropdownMenu::%1").arg(alias),context);
}

//! U+00A0 NO-BREAK SPACE -- what a plain paragraph's indent is made of. See
//! MessageEditor::applyIndentStep() for why it is not an ordinary space.
constexpr const char16_t NoBreakSpace=0x00a0;

//! Longest "@word" prefix reported through EnhancedTextEdit::mentionQueryChanged() (Stage 6).
//! Past this the candidate is dropped rather than truncated: nothing a user TYPES to pick a name
//! is this long, so a longer run is a paste, and a paste is not a mention gesture.
constexpr const int MaxMentionQueryChars=64;

//! Longest ":name" a shortcode auto-replace candidate can be -- see
//! EnhancedTextEdit::shortcodeCandidateAtCursor(). Same reasoning as MaxMentionQueryChars just
//! above (nothing a user TYPES is this long -- a longer run is a paste, which does not go
//! through this guard at all). Measured against src/emojiicontable.inc: the longest shortcode or
//! alias gen-emoji-pack.py's selection actually produces is 40 characters
//! ("hand_with_index_finger_and_thumb_crossed"); this is that measurement plus headroom, not a
//! guess -- re-check it if the generator's selection rule ever changes.
constexpr const int MaxShortcodeChars=48;

/** @brief U+200B ZERO WIDTH SPACE -- what an otherwise-empty paragraph is exported as, so the
 *  blank line it draws survives markdown at all. See fillEmptyBlocksForExport().
 *
 * Deliberately NOT the no-break space above, even though that is what the indent uses and the
 * first cut of this used it too. Measured, side by side:
 *
 *  |                                  | U+00A0 | U+200B |
 *  |----------------------------------|--------|--------|
 *  | QChar::isSpace()                 | true   | false  |
 *  | stripped by QString::trimmed()   | yes    | no     |
 *  | survives QTextDocument::toPlainText() | NO | yes   |
 *  | rendered gap in the bubble       | 15px   | 15px   |
 *
 * That third row is the one that matters: toPlainText() is documented to replace U+00A0 with an
 * ORDINARY space, and a line holding one of those is a blank line to CommonMark -- so the moment
 * this markdown passed through any QTextDocument-backed plain-text widget on its way to the
 * renderer, the paragraph was dropped and the two tables either side of it welded back together.
 * A zero-width space is not whitespace by any of these measures, so it survives that round trip
 * (and any trimmed()-based blank-line logic) while drawing the same gap. Being zero-width it also
 * leaves no visible artefact in the message text.
 */
constexpr const char16_t BlankLineMarker=0x200b;

/** @brief QTextDocument::toPlainText()'s character normalization, MINUS its no-break-space leg.
 *
 * toPlainText() is documented to replace U+00A0 with an ordinary space, and it does. That is
 * ruinous for this editor's paragraph indent (see MessageEditor::applyIndentStep()): the indent is
 * made of no-break spaces precisely BECAUSE four ordinary leading spaces are markdown's
 * indented-code-block syntax, so normalizing them on the way out re-creates the exact bug the
 * no-break space was chosen to avoid -- silently, at the export boundary, and worst of all in
 * Plaintext mode, which is the mode a real composer runs in.
 *
 * toRawText() is the non-normalizing accessor, but on its own it is not a drop-in replacement:
 * block boundaries come back as U+2029 PARAGRAPH SEPARATOR rather than '\n', which would break
 * every consumer of this text. So the remaining substitutions are done here, exactly as
 * qtextdocument.cpp does them. For a document containing no no-break space the result is
 * character-for-character what toPlainText() returns.
 */
QString normalizedRawText(QString text)
{
    for (auto& character: text)
    {
        if (character==QChar(0xfdd0)                    // QTextBeginningOfFrame
            || character==QChar(0xfdd1)                 // QTextEndOfFrame
            || character==QChar(QChar::ParagraphSeparator)
            || character==QChar(QChar::LineSeparator))
        {
            character=QLatin1Char('\n');
        }
    }

    return text;
}

//! Blocks a cursor touches: the one block it sits in, or every block its selection reaches into.
std::vector<QTextBlock> touchedBlocks(const QTextCursor& cursor)
{
    std::vector<QTextBlock> blocks;

    auto* document=cursor.document();
    if (document==nullptr)
    {
        return blocks;
    }

    const auto lastPos=qMax(cursor.selectionStart(),cursor.selectionEnd());
    for (auto block=document->findBlock(qMin(cursor.selectionStart(),cursor.selectionEnd()));
         block.isValid();
         block=block.next())
    {
        blocks.push_back(block);

        // position()+length()-1 is the block separator, i.e. the last position still INSIDE this
        // block. A selection ending exactly at the next block's start therefore stops here
        // rather than dragging in a block the user did not visibly select.
        if (block.position()+block.length()-1>=lastPos)
        {
            break;
        }
    }

    return blocks;
}

/** @brief Put one block at blockquote `level`, visibly.
 *
 * QTextFormat::BlockQuoteLevel alone is INERT: it is metadata that qtextmarkdownwriter reads on
 * export, and nothing in Qt's layout draws anything for it. Setting only the property is why a
 * blockquote applied from the toolbar or with Tab used to have no visual effect whatsoever, while
 * the very same document reloaded from markdown came back visibly indented -- Qt's markdown
 * IMPORTER sets the property AND a left/right margin (qtextmarkdownimporter.cpp:631-634, a flat
 * 40px per level), and its own source carries a "TODO maybe eliminate the margins after all views
 * recognize BlockQuoteLevel". So the margin is the only thing that renders, and both routes have
 * to set it or they disagree.
 *
 * The right margin is deliberately cleared rather than mirrored from Qt: in a chat composer a
 * right margin only narrows the usable text width, and the visual goal here is an indented
 * paragraph, not a centred pull-quote.
 *
 * Clearing the left margin on removal is safe in this editor specifically: nothing else here sets
 * a block left margin -- list nesting uses QTextListFormat::indent() against the document's
 * indentWidth, not margins.
 */
void setBlockquoteLevel(QTextBlockFormat& format, int level, qreal indentPerLevel)
{
    if (level>0)
    {
        format.setProperty(QTextFormat::BlockQuoteLevel,level);
        format.setLeftMargin(indentPerLevel*level);
    }
    else
    {
        // clearProperty(), NOT setProperty(...,0) -- qtextmarkdownwriter.cpp tests
        // hasProperty(BlockQuoteLevel), so a present-but-zero value would still emit a "> "
        // prefix on export.
        format.clearProperty(QTextFormat::BlockQuoteLevel);
        format.setLeftMargin(0);
    }

    format.setRightMargin(0);
}

//! normalizedRawText() for a whole document -- see there.
QString plainTextKeepingIndent(const QTextDocument* document)
{
    return normalizedRawText(document->toRawText());
}

//! normalizedRawText() for one selection. QTextCursor::selectedText() is already raw (it is
//! documented to return U+2029 between blocks), so unlike QTextDocumentFragment::toPlainText() it
//! never reaches toPlainText()'s no-break-space substitution in the first place.
QString plainTextKeepingIndent(const QTextCursor& cursor)
{
    return normalizedRawText(cursor.selectedText());
}

//! The delimiter a code block is written with, as literal document text. Three backticks: the
//! CommonMark minimum, and what Qt's own writer emits.
const QString CodeFence=QStringLiteral("```");

/** @brief Text of a code fence delimiter, or an empty string if this line is not one.
 *
 * Returned rather than a bool so the caller can compare RUN LENGTH and character: a fence closes
 * only on a run of the same character at least as long as the one that opened it.
 */
QString fenceRun(const QString& line)
{
    int i=0;
    while (i<line.size() && i<4 && line.at(i)==QLatin1Char(' '))
    {
        ++i;
    }
    if (i>=4 || i>=line.size())
    {
        return {};
    }

    const auto c=line.at(i);
    if (c!=QLatin1Char('`') && c!=QLatin1Char('~'))
    {
        return {};
    }

    int n=0;
    while (i+n<line.size() && line.at(i+n)==c)
    {
        ++n;
    }

    return n>=3 ? QString(n,c) : QString{};
}

//! Reverse of the backslash escaping qtextmarkdownwriter applies to ordinary paragraphs.
//!
//! Exact, not approximate: that writer only ever INSERTS a backslash before a character, and it
//! doubles every backslash already present (escapeSpecialCharacters() starts with
//! s.replace("\\", "\\\\")). So every backslash in its output is either one it added or the first
//! half of a pair, and dropping each one while taking the next character literally undoes both
//! cases -- `printf("\\n")` comes back as `printf("\n")`, measured.
QString unescapeMarkdown(const QString& line)
{
    if (!line.contains(QLatin1Char('\\')))
    {
        return line;
    }

    QString out;
    out.reserve(line.size());
    for (int i=0;i<line.size();++i)
    {
        if (line.at(i)==QLatin1Char('\\') && i+1<line.size())
        {
            out+=line.at(i+1);
            ++i;
            continue;
        }
        out+=line.at(i);
    }

    return out;
}

/** @brief Put the code fences a WYSIWYG document carries as LITERAL TEXT back the way the user
 *  typed them, on the way out to markdown.
 *
 * In WYSIWYG a fenced code block is three ordinary paragraphs -- "```", the code, "```" -- which
 * is what makes it visible and editable in place, language tag included. The cost is that
 * qtextmarkdownwriter treats those paragraphs like any other prose, and does two things to them:
 *
 *  - escapes every markdown special, so `int x = *p;` exports as `int x = \*p;` and, worse,
 *    ` ```cpp ` exports as ` \```cpp ` -- an escaped fence is not a fence at all, and the whole
 *    block silently stops being code;
 *  - writes a blank line between every pair of paragraphs, which inside a fence is content, so
 *    the code comes out double-spaced.
 *
 * Both are undone here, and only BETWEEN fences -- text outside one is passed through untouched.
 * Blank lines are dropped rather than collapsed because an intentionally empty line cannot be
 * told apart from a separator anyway: Qt writes nothing at all for an empty block, so
 * "a / <empty> / b" and "a / b" produce byte-identical markdown (measured).
 *
 * This is only correct because a WYSIWYG document never holds a PROPERTY-based code block -- see
 * convertCodeBlocksToText(), which turns every one Qt's importers create into literal text on the
 * way in. Qt writes its own fences with the content unescaped, so unescaping those as well would
 * eat real backslashes.
 */
QString restoreCodeFences(const QString& markdown)
{
    if (!markdown.contains(QLatin1Char('`')) && !markdown.contains(QLatin1Char('~')))
    {
        return markdown;
    }

    const auto lines=markdown.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size());

    QString openFence;

    for (const auto& line : lines)
    {
        // Unescaped FIRST: the opening fence of a block with a language tag reaches us as
        // "\```cpp", and would not be recognised as a fence at all otherwise.
        const auto unescaped=unescapeMarkdown(line);
        const auto fence=fenceRun(unescaped);

        if (openFence.isEmpty())
        {
            if (!fence.isEmpty())
            {
                openFence=fence;
                out.append(unescaped);
            }
            else
            {
                out.append(line);
            }
            continue;
        }

        // A fence closes only on the same character, at least as long.
        if (!fence.isEmpty() && fence.at(0)==openFence.at(0) && fence.size()>=openFence.size())
        {
            openFence.clear();
            out.append(unescaped);
            continue;
        }

        if (line.trimmed().isEmpty())
        {
            continue;
        }

        out.append(unescaped);
    }

    return out.join(QLatin1Char('\n'));
}

/** @brief Give every otherwise-empty paragraph a ZERO WIDTH SPACE, so the blank line it draws
 *  survives being exported to markdown.
 *
 * `qtextmarkdownwriter` writes NOTHING for an empty block, so a blank line simply ceases to exist
 * the moment a WYSIWYG document is exported: measured, "Hello / <blank> / World" comes back as two
 * blocks rather than three, and two tables separated by blank lines come back welded together
 * (13 blocks in, 11 out) -- in the composer after a Markdown round trip, and in the rendered bubble
 * too, since the bubble only ever sees the exported markdown.
 *
 * A zero-width space fixes it for the same reason a no-break space fixes a paragraph indent (see
 * MessageEditor::applyIndentStep()): it carries no markdown meaning, is not CommonMark whitespace,
 * and is not collapsed by the HTML the message is finally rendered as. Measured: such a paragraph
 * round-trips through toMarkdown()/setMarkdown() byte-identically, the block count is preserved
 * exactly (13 in, 13 out), and it adds a 15px gap in the rendered bubble. See BlankLineMarker for
 * why it is that character and not the indent's own U+00A0.
 *
 * Four kinds of empty block are deliberately left alone:
 *  - anything INSIDE a fenced code block, tracked with the same fenceRun() the highlighter uses:
 *    a marker character there would be injected into the user's code;
 *  - anything inside a TABLE, where an empty cell must stay an empty cell rather than gain a
 *    character of content;
 *  - the TRAILING run of empty blocks, which draws nothing at the end of a message anyway -- there
 *    is nothing after it to be separated from, and preserving it would append stray whitespace to
 *    every message that happens to end with a Return;
 *  - the one empty block Qt FORCES around a table. A QTextDocument always carries a block before a
 *    leading table and one after every table -- measured, and the user cannot delete them. They are
 *    structure, not a blank line somebody typed, so filling them would put a blank line above every
 *    message that merely starts with a table. Only the extras beyond that first one are the user's,
 *    which is why a run adjacent to a table has exactly one skipped: authored as
 *    "table / Return / Return / table", the document holds three empty blocks and the export
 *    carries two.
 */
void fillEmptyBlocksForExport(QTextDocument* document)
{
    struct BlockInfo
    {
        int position;
        bool empty;
        bool inTable;
        bool inFence;
        QString text;
    };

    std::vector<BlockInfo> blocks;
    QString openFence;
    for (auto block=document->begin(); block.isValid() && block!=document->end();
         block=block.next())
    {
        const auto text=block.text();
        const auto fence=fenceRun(text);

        bool inFence=!openFence.isEmpty();
        if (inFence)
        {
            if (!fence.isEmpty() && fence.at(0)==openFence.at(0) && fence.size()>=openFence.size())
            {
                openFence.clear();
            }
        }
        else if (!fence.isEmpty())
        {
            openFence=fence;
            inFence=true;
        }

        QTextCursor probe(block);
        blocks.push_back(BlockInfo{block.position(),text.isEmpty(),
                                   probe.currentTable()!=nullptr,inFence,text});
    }

    int lastMeaningfulIndex=-1;
    for (std::size_t i=0; i<blocks.size(); ++i)
    {
        if (!blocks[i].empty)
        {
            lastMeaningfulIndex=static_cast<int>(i);
        }
    }

    std::vector<int> positions;
    for (std::size_t i=0; i<blocks.size(); ++i)
    {
        const auto& info=blocks[i];
        if (!info.empty || info.inTable || info.inFence)
        {
            continue;
        }
        if (static_cast<int>(i)>lastMeaningfulIndex)
        {
            continue;
        }

        // An empty block ADJACENT TO A TABLE is never marked. Partly because Qt forces one there
        // (a document always carries a block before a leading table and after every table, and
        // the user cannot delete them), and partly because a table does not need a blank line to
        // stand apart from its neighbours the way two paragraphs do: the table frame's own
        // spacing already separates it (measured 9 rows between two tables), and the renderer
        // gives a table that follows a paragraph the few pixels that boundary lacks
        // (markdownrenderer.cpp, writeTable()). Marking these produced a doubled gap -- that
        // spacing AND a blank line -- for a Return the user had only pressed to get out of the
        // paragraph above.
        const bool nextIsTable=(i+1<blocks.size()) && blocks[i+1].inTable;
        const bool previousIsTable=(i>0) && blocks[i-1].inTable;
        if (nextIsTable || previousIsTable)
        {
            continue;
        }

        positions.push_back(info.position);
    }

    // Empty visual lines INSIDE a block. Since Return inserts a soft break rather than starting a
    // new paragraph (see EnhancedTextEdit::keyPressEvent()), a blank line a user types is now two
    // consecutive U+2028s inside ONE block -- there is no empty BLOCK to find. Without this pass
    // the loop above sees nothing to mark, the export writes a bare "aa\n\nbb", and the blank
    // line is gone again on the next import.
    for (const auto& info : blocks)
    {
        if (info.inTable || info.inFence || info.text.isEmpty())
        {
            continue;
        }

        const auto segments=info.text.split(QChar::LineSeparator);
        int offset=0;
        for (int segment=0; segment<segments.size(); ++segment)
        {
            // The trailing segment draws nothing after it, exactly as a trailing run of empty
            // blocks does -- see this function's own doc comment.
            if (segments.at(segment).isEmpty() && segment+1<segments.size())
            {
                positions.push_back(info.position+offset);
            }
            offset+=segments.at(segment).size()+1;
        }
    }

    // Back to front: inserting a character shifts every later position.
    std::sort(positions.begin(),positions.end());
    for (auto it=positions.rbegin(); it!=positions.rend(); ++it)
    {
        QTextCursor cursor(document);
        cursor.setPosition(*it);
        cursor.insertText(QString(QChar(BlankLineMarker)));
    }
}

/** @brief Collapse runs of consecutive blank lines down to one, outside fenced code blocks.
 *
 * Cosmetic, and worth it because the source is something a Markdown-mode author reads. Qt's own
 * writer puts a bare extra newline in front of every table (measured -- "|3|4|\n\n\n|a|b|"), so
 * even a document with no authored blank line at all showed two blank lines between two tables,
 * and one WITH an authored blank showed four. One blank line is all markdown needs to separate any
 * two block constructs, and more mean exactly the same thing to every parser -- so this changes
 * nothing about how the message renders, only how much empty space the source view shows. Measured
 * after: 2 blank source lines become 1 with no authored gap, and 4 become 3 with one.
 *
 * The blank-line MARKER is unaffected: a paragraph holding a zero-width space is not an empty
 * line, so it is never part of a run and never collapsed -- which is exactly the distinction the
 * marker exists to draw.
 *
 * Fenced regions are skipped: blank lines inside a code block are content.
 */
QString collapseBlankRuns(const QString& markdown)
{
    const auto lines=markdown.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size());

    QString openFence;
    int blankRun=0;

    for (const auto& line : lines)
    {
        const auto fence=fenceRun(line);
        if (!openFence.isEmpty())
        {
            if (!fence.isEmpty() && fence.at(0)==openFence.at(0) && fence.size()>=openFence.size())
            {
                openFence.clear();
            }
            out.append(line);
            continue;
        }
        if (!fence.isEmpty())
        {
            openFence=fence;
            blankRun=0;
            out.append(line);
            continue;
        }

        if (line.isEmpty())
        {
            ++blankRun;
            // ONE empty line already separates any two block constructs in markdown; further ones
            // mean exactly the same thing to every parser, so they are dropped. Verified by
            // rendering rather than assumed, because welding two tables together is precisely the
            // bug this whole section exists to fix: "|3|4|\n\n|a|b|" still renders as two
            // separate <table> elements.
            if (blankRun>1)
            {
                continue;
            }
        }
        else
        {
            blankRun=0;
        }

        out.append(line);
    }

    return out.join(QLatin1Char('\n'));
}

/** @brief The inverse of mergeProseBlocksForExport(): give every typed line its own block again
 *  on the way IN.
 *
 * `setMarkdown()` follows CommonMark, where a single newline inside a paragraph is a SPACE --
 * measured, "aa\nbb" comes back as ONE block reading "aa bb", so a line break the user typed and
 * exported correctly was destroyed the moment the document was re-imported (a Markdown-mode round
 * trip, or loading a message to edit).
 *
 * markdownWithChatLineBreaks() already knows which newlines are prose and which are syntax -- it is
 * the rule markdownToHtml() applies, shared rather than duplicated so the editor and the bubble
 * cannot disagree. It marks the prose ones with U+2028; turning each of those into a paragraph
 * break is what gives the editor one block per line, which is what every block-level format needs.
 */
QString markdownWithParagraphPerLine(const QString& markdown)
{
    auto prepared=markdownWithChatLineBreaks(markdown);
    prepared.replace(QChar::LineSeparator,QStringLiteral("\n\n"));
    return prepared;
}

/** @brief Join each run of consecutive ORDINARY paragraphs into one, separated by U+2028, so the
 *  export spells a typed line break as ONE newline instead of a blank line.
 *
 * A Return in this editor creates a new QTextBlock, and that is deliberate -- every block-level
 * format (list, heading, blockquote, code block, horizontal rule, indent) acts on a block, so a
 * message whose lines are all one block would apply a bullet to every line at once. The cost is
 * that qtextmarkdownwriter spells a block boundary as a BLANK LINE, so one typed line break left
 * the editor as "aa\n\nbb" -- two paragraphs, which the bubble then draws with messagetext.css's
 * paragraph margins, visibly looser than the tight line that was typed.
 *
 * Joining them HERE, on the throwaway export clone, gets both: the live document keeps one block
 * per line so formatting still works, while the exported markdown says "aa\nbb", which
 * markdownToHtml() renders as "<p>aa<br/>bb</p>" -- the composer and the bubble agreeing line for
 * line. MessageEditor's import does the inverse (see loadText()), so the round trip is stable.
 *
 * Only ORDINARY paragraphs are joined. A block that is a list item, a heading, a blockquote, a
 * horizontal rule, a table cell or part of a fenced code block keeps its own boundary, because in
 * every one of those the boundary is what the construct is made of.
 */
void mergeProseBlocksForExport(QTextDocument* document)
{
    struct Boundary
    {
        int position;
    };
    std::vector<Boundary> boundaries;

    QString openFence;
    bool previousWasProse=false;
    int previousEnd=-1;

    for (auto block=document->begin(); block.isValid() && block!=document->end();
         block=block.next())
    {
        const auto text=block.text();
        const auto fence=fenceRun(text);

        bool inFence=!openFence.isEmpty();
        if (inFence)
        {
            if (!fence.isEmpty() && fence.at(0)==openFence.at(0) && fence.size()>=openFence.size())
            {
                openFence.clear();
            }
        }
        else if (!fence.isEmpty())
        {
            openFence=fence;
            inFence=true;
        }

        QTextCursor probe(block);
        const auto blockFormat=block.blockFormat();
        const bool prose=!inFence
                         && probe.currentTable()==nullptr
                         && block.textList()==nullptr
                         && blockFormat.headingLevel()==0
                         && !blockFormat.hasProperty(QTextFormat::BlockQuoteLevel)
                         && !blockFormat.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);

        if (prose && previousWasProse && previousEnd>=0)
        {
            boundaries.push_back(Boundary{previousEnd});
        }

        previousWasProse=prose;
        previousEnd=block.position()+block.length()-1;
    }

    // Back to front, so joining one pair cannot shift the position of a pair still to be joined.
    for (auto it=boundaries.rbegin(); it!=boundaries.rend(); ++it)
    {
        QTextCursor cursor(document);
        cursor.setPosition(it->position);
        // Removing the paragraph separator merges the next block into this one; the line separator
        // put in its place is what qtextmarkdownwriter writes as a single newline.
        cursor.deleteChar();

        // Stage 6 regression, root-caused here rather than where it was first reported: insertText()
        // with no explicit format inherits cursor.charFormat() -- the format of the PRECEDING
        // character, per its own documented rule -- and if the first block ends on an ANCHOR (an
        // ordinary link or a mention), the separator is written WITH that anchor format and Qt then
        // merges it into the anchor's own fragment, since adjacent same-format runs merge. Measured:
        // a block ending "...Alice"(anchor) joined this way left the fragment as "Alice<U+2028>",
        // and qtextmarkdownwriter serialized that as a literal newline INSIDE the link's own title --
        // "[Alice\n](whitem-mention:usr1)" -- corrupting the mention regardless of what, if anything,
        // was typed on the next line. A blank, explicitly-constructed format has none of that: the
        // separator becomes its OWN fragment, never merges into a neighbour's formatting, and
        // qtextmarkdownwriter's "one U+2028 -> one newline" rule (this function's own reason for
        // being) applies exactly once, outside any anchor/bold/italic run either side of it.
        cursor.insertText(QString(QChar::LineSeparator),QTextCharFormat{});
    }
}

/** @brief Give one space to every table column that is empty in EVERY row, so the exported
 *  markdown is still a table.
 *
 * Qt's markdown writer sizes each column to its widest cell and writes exactly that many
 * characters -- including, for a column nothing has ever been typed into, ZERO. That is fine for
 * a content row (an empty cell is legal markdown) but fatal for the delimiter row, where a cell
 * must hold at least one "-": a 2x3 table with its last column empty is written as
 *
 *     |1| ||
 *     |-|-||
 *     | |3||
 *
 * and the empty third delimiter cell makes the line stop being a delimiter at all, so the whole
 * construct degrades to a paragraph of literal pipes -- measured, QTextDocument::setMarkdown()
 * finds 0 tables in it, and that is exactly what the bubble showed. A table with NO text in it
 * yet is worse still: all three lines come out as "|||". Which is to say the failure needs no
 * unusual input at all -- insert a table, type in one cell, send.
 *
 * A space is the smallest thing that makes the writer allot the column a width, and it costs
 * nothing on the way back: markdown strips a cell's surrounding whitespace, so the column
 * re-imports as the empty column it was (measured -- the round trip returns the identical 2x3
 * shape with the identical cells) and re-exports identically. Repairing the delimiter row in the
 * generated TEXT instead was the alternative and is worse: in the all-empty table above the
 * delimiter row is indistinguishable from the content rows, so there is nothing for a text pass
 * to key on. Here the structure is still in hand.
 *
 * A column that has content SOMEWHERE is never touched, and the export of a table with no empty
 * column is byte-identical with and without this pass.
 *
 * Runs on the export clone only -- the live document must not gain characters the user did not
 * type. Nested tables are walked too, unlike normalizeImportedTables(): this one is about what
 * the writer emits, and the writer descends into them.
 */
void padEmptyTableColumnsForExport(QTextFrame* frame)
{
    for (auto it=frame->begin(); !it.atEnd(); ++it)
    {
        auto* child=it.currentFrame();
        if (child==nullptr)
        {
            continue;
        }

        auto* table=qobject_cast<QTextTable*>(child);
        if (table!=nullptr)
        {
            for (int column=0; column<table->columns(); ++column)
            {
                bool empty=true;
                for (int row=0; row<table->rows() && empty; ++row)
                {
                    auto cell=table->cellAt(row,column);
                    for (auto cellIt=cell.begin(); !cellIt.atEnd(); ++cellIt)
                    {
                        const auto block=cellIt.currentBlock();
                        if (block.isValid() && !block.text().isEmpty())
                        {
                            empty=false;
                            break;
                        }
                    }
                }

                if (empty)
                {
                    table->cellAt(0,column).firstCursorPosition()
                        .insertText(QStringLiteral(" "));
                }
            }
        }

        padEmptyTableColumnsForExport(child);
    }
}

/** @brief Replace emoji images with their literal emoji characters.
 *
 * An emoji inserted in WYSIWYG mode is an image carrying an "whitem-emoji:<reaction id>" src.
 * On the way out, one from the default pack becomes the plain Unicode character instead: that
 * says exactly the same thing in a form EVERY client understands, with no pack lookup and no
 * agreement about icon ids required.
 *
 * @param defaultPackOnly When true (the MARKDOWN export), an image from any other pack is left
 *  alone, to be written as "![code](src)". Its graphic may simply not exist on the receiving
 *  side, so the pack-qualified reference is the only thing that can find it again -- and a client
 *  that cannot resolve it still has the alt text, which is the emoji character.
 *  When false (the PLAIN-TEXT export), every pack is resolved: plain text cannot express a pack
 *  reference at all, so the character is the only representation available and keeping the image
 *  would just lose the emoji outright.
 *
 * Fragments are collected first and applied BACK TO FRONT: replacing one shifts the positions of
 * everything after it. Same idiom, and the same reason, as convertCodeBlocksToText().
 */
void replaceEmojiImagesForExport(QTextDocument* document, bool defaultPackOnly)
{
    struct Replacement
    {
        int position;
        int length;
        QString code;
        QTextCharFormat format;
    };
    std::vector<Replacement> replacements;

    for (auto block=document->begin(); block.isValid(); block=block.next())
    {
        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }
            auto charFormat=fragment.charFormat();
            if (!charFormat.isImageFormat())
            {
                continue;
            }

            const auto imgFmt=charFormat.toImageFormat();
            const auto reactionId=emojiReactionId(imgFmt.name());
            if (reactionId.isEmpty())
            {
                continue;
            }
            if (defaultPackOnly && !ChatReactionId::packUri(reactionId).isEmpty())
            {
                continue;
            }
            const auto* info=ReactionIconPacks::instance().iconInfo(reactionId);
            if (info==nullptr || info->emojiCode.isEmpty())
            {
                continue;
            }

            // Strip the image-ness off the format the replacement TEXT will carry, or the
            // inserted characters would simply become another image fragment.
            auto plainFormat=charFormat;
            plainFormat.setObjectType(QTextFormat::NoObject);
            plainFormat.clearProperty(QTextFormat::ImageName);
            plainFormat.clearProperty(QTextFormat::ImageAltText);
            plainFormat.clearProperty(QTextFormat::ImageTitle);
            plainFormat.clearProperty(QTextFormat::ImageWidth);
            plainFormat.clearProperty(QTextFormat::ImageHeight);

            // emojiText, not emojiCode: this is the character the message is SENT as, so it has
            // to carry the variation selector that makes a heart render in colour rather than as
            // a monochrome "\u2764" in every reader's system font -- the chat-list preview and the
            // notification popup included. See ReactionIconInfo::emojiText.
            //
            // embeddedObjectCount(), not "1": the SAME emoji inserted twice in a row shares one
            // char format (name/alt/width/height all equal), so Qt's own unite() merges the two
            // insertions into a single fragment of length 2 rather than two fragments of length 1
            // -- see embeddedObjectCount()'s own doc comment. Repeating the code that many times
            // is what keeps a run of several identical emoji from being sent as just one.
            QString code;
            const auto objectCount=embeddedObjectCount(fragment.text());
            code.reserve(info->emojiText.size()*objectCount);
            for (int i=0; i<objectCount; ++i)
            {
                code+=info->emojiText;
            }
            replacements.push_back({fragment.position(),fragment.length(),
                                    code,plainFormat});
        }
    }

    for (auto it=replacements.rbegin(); it!=replacements.rend(); ++it)
    {
        QTextCursor cursor(document);
        cursor.setPosition(it->position);
        cursor.setPosition(it->position+it->length,QTextCursor::KeepAnchor);
        cursor.insertText(it->code,it->format);
    }
}

/** @brief The WYSIWYG markdown export, in one place: blank lines preserved, code fences restored.
 *
 * Works on a CLONE rather than the live document -- fillEmptyBlocksForExport() inserts real
 * characters, and an export must never mutate what the user is editing (nor push anything onto
 * their undo stack).
 */
QString wysiwygMarkdown(const QTextDocument* document)
{
    std::unique_ptr<QTextDocument> clone(document->clone());
    fillEmptyBlocksForExport(clone.get());
    mergeProseBlocksForExport(clone.get());
    padEmptyTableColumnsForExport(clone->rootFrame());
    replaceEmojiImagesForExport(clone.get(),true);
    return collapseBlankRuns(restoreCodeFences(clone->toMarkdown()));
}

/** @brief plainTextKeepingIndent() with emoji images resolved to their codes.
 *
 * The TextFormat::Plain counterpart of wysiwygMarkdown(), and the two must stay in step. Without
 * it a WYSIWYG emoji comes out of toRawText() as U+FFFC OBJECT REPLACEMENT CHARACTER -- the image
 * is simply lost, silently, on every Plain export and every Plain copy.
 *
 * Works on a clone for the same reason wysiwygMarkdown() does.
 */
QString plainTextWithEmoji(const QTextDocument* document)
{
    std::unique_ptr<QTextDocument> clone(document->clone());
    replaceEmojiImagesForExport(clone.get(),false);
    return plainTextKeepingIndent(clone.get());
}

//! plainTextWithEmoji() for a selection.
QString plainTextWithEmoji(const QTextCursor& cursor)
{
    if (!cursor.hasSelection())
    {
        return QString{};
    }
    QTextDocument temp;
    QTextCursor tempCursor(&temp);
    tempCursor.insertFragment(cursor.selection());
    replaceEmojiImagesForExport(&temp,false);
    return plainTextKeepingIndent(&temp);
}

//! wysiwygMarkdown() for a selection -- same treatment, so copying a range with blank lines in it
//! yields the same markdown as sending the whole thing would.
QString wysiwygMarkdown(const QTextDocumentFragment& fragment)
{
    QTextDocument temp;
    QTextCursor cursor(&temp);
    cursor.insertFragment(fragment);
    fillEmptyBlocksForExport(&temp);
    mergeProseBlocksForExport(&temp);
    padEmptyTableColumnsForExport(temp.rootFrame());
    replaceEmojiImagesForExport(&temp,true);
    return collapseBlankRuns(restoreCodeFences(temp.toMarkdown()));
}

/** @brief Turn every property-based code block in a document into the literal "```" text form.
 *
 * Qt's markdown importer consumes the fences it reads into QTextBlockFormat properties, so a code
 * block loaded into WYSIWYG has no "```" anywhere in the document -- it is invisible, and its
 * language tag is gone from view entirely. This puts both back as ordinary text, which is the form
 * the user can see and edit, and the only form restoreCodeFences() can safely export.
 *
 * Runs are processed back to front so that inserting the two fence lines for one run cannot shift
 * the block numbers of the runs still to be handled.
 *
 * @param suppressUndo Disable undo around the rewrite. Correct for the whole-document LOADS this
 *  was written for (loadText(), a mode switch), where the undo stack is meaningless anyway --
 *  but it must be false on the PASTE path, because QTextDocument::setUndoRedoEnabled(false)
 *  CLEARS the undo stack outright (measured: one undo step before, zero after), which would
 *  silently throw away everything the user had typed before pasting. With it false the rewrite
 *  simply joins the caller's own edit block, so one Ctrl+Z still takes the whole paste back out.
 */
void convertCodeBlocksToText(QTextDocument* document, bool suppressUndo=true)
{
    const auto isCode=[](const QTextBlock& block)
    {
        const auto format=block.blockFormat();
        // The same predicate qtextmarkdownwriter uses to decide it is looking at a code block.
        return format.hasProperty(QTextFormat::BlockCodeFence)
            || !format.stringProperty(QTextFormat::BlockCodeLanguage).isEmpty()
            || format.nonBreakableLines();
    };

    struct Run
    {
        int first;
        int last;
        QString language;
    };

    std::vector<Run> runs;
    for (auto block=document->begin(); block.isValid() && block!=document->end();
         block=block.next())
    {
        if (!isCode(block))
        {
            continue;
        }

        Run run{block.blockNumber(),block.blockNumber(),
                block.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage)};
        auto next=block.next();
        while (next.isValid() && next!=document->end() && isCode(next))
        {
            run.last=next.blockNumber();
            block=next;
            next=block.next();
        }
        runs.push_back(run);
    }

    if (runs.empty())
    {
        return;
    }

    const auto undoEnabled=document->isUndoRedoEnabled();
    if (suppressUndo)
    {
        document->setUndoRedoEnabled(false);
    }

    for (auto run=runs.rbegin(); run!=runs.rend(); ++run)
    {
        const auto lastBlock=document->findBlockByNumber(run->last);
        QTextCursor cursor(document);

        // Closing fence first: appending it leaves every block number at or before run->last
        // exactly where it was, so the opening insert below can still be addressed by number.
        cursor.setPosition(lastBlock.position()+lastBlock.length()-1);
        cursor.insertText(QStringLiteral("\n")+CodeFence);

        const auto firstBlock=document->findBlockByNumber(run->first);
        cursor.setPosition(firstBlock.position());
        cursor.insertText(CodeFence+run->language+QStringLiteral("\n"));

        // Both fence lines inherited the code block's own formats on insertion, so the whole
        // region -- fences and content alike -- is flattened to plain paragraphs in one pass:
        // properties gone, and the importer's monospace family gone with them. What the reader
        // sees as code from here on is the highlighter's doing, not the document's.
        const auto regionFirst=document->findBlockByNumber(run->first);
        const auto regionLast=document->findBlockByNumber(run->last+2);
        QTextCursor region(document);
        region.setPosition(regionFirst.position());
        region.setPosition(regionLast.position()+regionLast.length()-1,QTextCursor::KeepAnchor);
        region.setBlockFormat(QTextBlockFormat{});
        region.setCharFormat(QTextCharFormat{});
    }

    if (suppressUndo)
    {
        document->setUndoRedoEnabled(undoEnabled);
    }
}

//! One matched emoji occurrence within a fragment's text, expressed in UTF-16 code units so the
//! caller can slice QString/QTextCursor positions directly.
struct EmojiCodePointMatch
{
    int offset;
    int length;
    QString reactionId;
};

/** @brief Scan `text` by CODE POINT for every character `pack` resolves to a default-pack icon.
 *
 * Never by QChar: every supplementary-plane emoji is a surrogate pair. Shared by
 * normalizeImportedEmoji() (which turns each match into an image) and MessageEditor::hasEmoji()
 * (which only needs to know whether the result is non-empty), so the two can never disagree about
 * what counts as "this text has an emoji".
 *
 * A following ZERO WIDTH JOINER suppresses a match: a family emoji opens with a code point the
 * pack may carry alone, and substituting just that one would render one person plus orphan
 * glyphs. A following variation selector (U+FE0F/FE0E) is swallowed into the match's length so it
 * is never left stranded next to a substituted image.
 */
std::vector<EmojiCodePointMatch> matchEmojiCodePoints(const QString& text,
                                                      const AbstractReactionIconPack* pack)
{
    std::vector<EmojiCodePointMatch> matches;
    if (pack==nullptr)
    {
        return matches;
    }

    const auto ucs4=text.toUcs4();
    int offset=0;
    for (qsizetype i=0; i<ucs4.size(); ++i)
    {
        const char32_t cp=ucs4[i];
        const auto chars=QString::fromUcs4(&cp,1);

        const auto* info=pack->findByCode(chars);
        const bool zwjFollows=(i+1<ucs4.size()) && ucs4[i+1]==0x200D;

        if (info!=nullptr && info->icon && !zwjFollows)
        {
            auto length=static_cast<int>(chars.size());
            if (i+1<ucs4.size() && (ucs4[i+1]==0xFE0F || ucs4[i+1]==0xFE0E))
            {
                const char32_t vs=ucs4[i+1];
                length+=static_cast<int>(QString::fromUcs4(&vs,1).size());
                ++i;
            }
            matches.push_back({offset,length,ChatReactionId::make(info->iconId,pack->uri())});
            offset+=length;
            continue;
        }
        offset+=static_cast<int>(chars.size());
    }
    return matches;
}

/** @brief Drop the presentation Qt's own importers bake onto every anchor they read.
 *
 * `setMarkdown()`/`setHtml()` do not merely record an anchor's href -- they also write
 * `foreground=#0000ff` straight into the document's char format (measured, and visible in
 * `toHtml()` as `<span style=" color:#0000ff;">`). Before this stage that was the *only* reason
 * an imported link looked like a link at all, since Qt's layout draws an anchor exactly like
 * ordinary text otherwise -- which is precisely why a freshly inserted link looked plain until it
 * had been round-tripped through Markdown mode.
 *
 * It does NOT fight the highlighter for the on-screen colour: measured, a layout format set by
 * QSyntaxHighlighter already wins over the document's own char format, so links render in
 * linkColor either way. What it does do is freeze a theme-specific colour into the DOCUMENT,
 * where it leaks into every `toHtml()` export and travels with the message.
 *
 * Stripping it here leaves the anchor itself (href and `isAnchor()`) completely intact -- the
 * markdown still exports as `[title](url)` -- and hands presentation to
 * MessageEditorHighlighter::highlightLinks(), which paints typed and imported links alike in one
 * themed colour. Same shape and same reasoning as normalizeBlockquoteIndent() re-indenting the
 * 40px margin those importers bake in beside it.
 *
 * @param suppressUndo See convertCodeBlocksToText() -- true for the whole-document loads, false
 *  on the paste path, where disabling undo would clear the stack.
 */
/** @brief Make emoji in a freshly IMPORTED document displayable and consistent.
 *
 * Two jobs, both of which have to happen for every route markdown/HTML takes into a WYSIWYG
 * document (loadText() and the mode switch):
 *
 * 1. **Re-apply size and re-register the resource for every emoji image.** QTextDocument::
 *    setMarkdown() restores neither: Qt's markdown importer builds a fresh QTextImageFormat
 *    carrying only name/alt/title, and nothing ever re-populates the document's resource table.
 *    Without this pass every Wysiwyg->Markdown->Wysiwyg switch fills the editor with Qt's 16px
 *    broken-file icon. Exactly the class of bug normalizeImportedTables() exists for.
 *
 * 2. **Convert literal default-pack emoji CHARACTERS into images.** An emoji typed with the OS
 *    picker, or authored in Markdown mode and then switched to WYSIWYG, would otherwise render in
 *    the system font right beside a gallery-inserted one rendered as pack art -- the same emoji,
 *    two different pictures, in one composer. Converting costs nothing on export, because
 *    replaceEmojiImagesForExport() turns default-pack images straight back into characters.
 *
 * @param font Font whose metrics decide the inline size, i.e. the text edit's own.
 * @param dpr Device pixel ratio the pixmaps are rasterized at, i.e. the text edit's own.
 *
 * Runs BACK TO FRONT, like every other pass here that changes text lengths.
 */
void normalizeImportedEmoji(QTextDocument* document, const QFont& font, qreal dpr,
                            bool suppressUndo=true)
{
    const auto px=MessageEditor::emojiInlineSizeForFont(font);
    const auto devicePx=qRound(px*dpr);

    struct Insert
    {
        int position;
        int length;
        int count;
        QString reactionId;
        QTextCharFormat baseFormat;
    };
    std::vector<Insert> inserts;

    auto defaultPack=ReactionIconPacks::instance().defaultPack();

    for (auto block=document->begin(); block.isValid(); block=block.next())
    {
        // Never inside a code block: an emoji character there is content, and turning it into an
        // image would corrupt the code the user is looking at.
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage)
            || block.blockFormat().nonBreakableLines())
        {
            continue;
        }

        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }
            auto charFormat=fragment.charFormat();

            if (charFormat.isImageFormat())
            {
                const auto reactionId=emojiReactionId(charFormat.toImageFormat().name());
                if (!reactionId.isEmpty())
                {
                    // embeddedObjectCount(), not "1": the same emoji inserted repeatedly can be
                    // ONE merged fragment carrying several U+FFFC characters -- see
                    // embeddedObjectCount()'s own doc comment.
                    inserts.push_back({fragment.position(),fragment.length(),
                                       embeddedObjectCount(fragment.text()),reactionId,charFormat});
                }
                continue;
            }

            if (charFormat.fontFixedPitch() || !defaultPack)
            {
                // Inline code -- same reasoning as the code-block skip above.
                continue;
            }

            const auto matches=matchEmojiCodePoints(fragment.text(),defaultPack.get());
            for (const auto& match : matches)
            {
                inserts.push_back({fragment.position()+match.offset,match.length,1,
                                   match.reactionId,charFormat});
            }
        }
    }

    if (inserts.empty())
    {
        return;
    }

    if (suppressUndo)
    {
        document->setUndoRedoEnabled(false);
    }

    for (auto it=inserts.rbegin(); it!=inserts.rend(); ++it)
    {
        const auto* info=ReactionIconPacks::instance().iconInfo(it->reactionId);
        if (info==nullptr || !info->icon)
        {
            continue;
        }

        const auto src=emojiSrc(it->reactionId);
        // Registered before the image is (re)inserted -- see MessageEditor::insertEmoji() for
        // why an unregistered src poisons its own key with Qt's broken-file placeholder.
        document->addResource(QTextDocument::ImageResource,QUrl(src),
                              info->icon->pixmap(QSize(devicePx,devicePx)));

        QTextImageFormat imgFmt;
        imgFmt.setName(src);
        imgFmt.setProperty(QTextFormat::ImageAltText,
                           info->emojiCode.isEmpty() ? info->iconId : info->emojiText);
        imgFmt.setWidth(px);
        imgFmt.setHeight(px);

        QTextCursor cursor(document);
        cursor.setPosition(it->position);
        cursor.setPosition(it->position+it->length,QTextCursor::KeepAnchor);
        cursor.insertImage(imgFmt);
        // it->count-1 more: insertImage() left the selection replaced by ONE image and the
        // cursor positioned right after it, so repeating the call keeps inserting contiguously --
        // exactly what a merged multi-object fragment (see embeddedObjectCount()) needs restored.
        for (int i=1; i<it->count; ++i)
        {
            cursor.insertImage(imgFmt);
        }
    }

    if (suppressUndo)
    {
        document->setUndoRedoEnabled(true);
    }
}

void stripImportedAnchorStyle(QTextDocument* document, bool suppressUndo=true)
{
    // Collected first, mutated after -- see stripBakedRichTextFormatting() for why a format write
    // must not happen while a block's fragment iterator is live.
    struct Run
    {
        int start;
        int end;
        QTextCharFormat format;
    };
    std::vector<Run> runs;

    for (auto block=document->begin(); block.isValid() && block!=document->end();
         block=block.next())
    {
        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            const auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }

            auto format=fragment.charFormat();
            if (!format.isAnchor())
            {
                continue;
            }
            if (!format.hasProperty(QTextFormat::ForegroundBrush)
                && !format.hasProperty(QTextFormat::FontUnderline))
            {
                continue;
            }

            format.clearProperty(QTextFormat::ForegroundBrush);
            format.clearProperty(QTextFormat::FontUnderline);
            runs.push_back(Run{fragment.position(),fragment.position()+fragment.length(),format});
        }
    }

    if (runs.empty())
    {
        return;
    }

    const auto undoEnabled=document->isUndoRedoEnabled();
    if (suppressUndo)
    {
        document->setUndoRedoEnabled(false);
    }

    for (const auto& run : runs)
    {
        QTextCursor fragmentCursor(document);
        fragmentCursor.setPosition(run.start);
        fragmentCursor.setPosition(run.end,QTextCursor::KeepAnchor);
        fragmentCursor.setCharFormat(run.format);
    }

    if (suppressUndo)
    {
        document->setUndoRedoEnabled(undoEnabled);
    }
}

/**
 * @brief The border/collapse/brush half of a visible table -- shared by MessageEditor::
 *  applyTable() (a freshly typed table) and normalizeImportedTables() below (one that arrived from outside).
 *
 * Factored out so the two can never drift: see MessageEditor::applyTable()'s own comment for the
 * full measured rationale (setBorderCollapse(false) being load-bearing, the theme-neutral mid
 * grey). Width/cell padding are deliberately NOT touched here -- a typed table wants its own
 * defaults (applyTable() sets those separately), while a pasted table's existing width/padding
 * are left alone in normalizeImportedTables() so real imported content is not reflowed.
 */
void applyTableVisibilityFormat(QTextTableFormat& format)
{
    format.setBorderCollapse(false);
    format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    format.setBorder(1);
    format.setBorderBrush(QColor(0x80,0x80,0x80));
}

/** @brief Force every top-level table overlapping [rangeStart,rangeEnd) to the same visible,
 *  theme-neutral border Stage 4/5a already give a typed table.
 *
 * An externally pasted table (Qt's own external-HTML import) measured to arrive with
 * `border=0, borderCollapse=false` -- 0 painted border pixels, i.e. invisible, the identical
 * root cause Stage 4 fixed for typed tables. Reusing applyTableVisibilityFormat() closes it here
 * too, without touching whatever width/cell padding the pasted markup specified.
 *
 * Only top-level tables are walked (document->rootFrame()'s direct children) -- a table nested
 * inside another table's cell is not reached, matching applyTable()'s own single-level scope.
 *
 * Runs on EVERY route foreign content takes into the document, not just paste: a table that
 * arrives through setMarkdown()/setHtml() -- a loadText(), or the Markdown-mode round trip -- is
 * rebuilt by Qt's importer with border=0 and borderCollapse=true and is just as invisible as a
 * pasted one. Wiring it only to paste is what made a table lose its grid the moment the editor
 * was switched to Markdown mode and back.
 *
 * @param suppressUndo See convertCodeBlocksToText() -- true for the whole-document loads, false
 *  on the paste path, where disabling undo would clear the user's stack.
 */
void normalizeImportedTables(QTextDocument* document, int rangeStart, int rangeEnd,
                             bool suppressUndo=true)
{
    const auto undoEnabled=document->isUndoRedoEnabled();
    if (suppressUndo)
    {
        document->setUndoRedoEnabled(false);
    }

    auto* root=document->rootFrame();
    for (auto it=root->begin(); !it.atEnd(); ++it)
    {
        auto* table=qobject_cast<QTextTable*>(it.currentFrame());
        if (table==nullptr)
        {
            continue;
        }

        if (table->lastPosition()<rangeStart || table->firstPosition()>=rangeEnd)
        {
            continue;
        }

        auto format=table->format();
        applyTableVisibilityFormat(format);
        table->setFormat(format);

        // Drop the PER-CELL borders an importer bakes in, so the grid is drawn by the table's own
        // theme-neutral format above and by nothing else -- exactly the state a typed table is in.
        //
        // Not hypothetical: measured against a real table copied from macOS Numbers, whose HTML
        // gives every one of its cells an explicit #000000 border. Fixing only the table format
        // left those in place, so the pasted table drew a black grid that vanished against a dark
        // background (13833 painted pixels on black against 17071 on white). With them cleared
        // the same table paints 22801/23347 -- the same grid in both themes, which is the whole
        // point of the mid grey.
        for (int row=0; row<table->rows(); ++row)
        {
            for (int column=0; column<table->columns(); ++column)
            {
                auto cell=table->cellAt(row,column);
                auto cellFormat=cell.format().toTableCellFormat();

                bool changed=false;
                // BackgroundBrush rides along with the borders for the same reason and one more.
                // Measured on the real Numbers payload: 14 of its 20 cells carry a baked fill
                // (#b0b3b2/#d4d4d4/#f2f2f2) on the CELL format, which nothing else in this file
                // touches -- stripBakedRichTextFormatting() only ever saw CHAR formats. Keeping
                // those while stripping the char foregrounds was the worst of both worlds: the
                // fills stayed light and the text fell back to the palette, i.e. WHITE text on a
                // near-white cell in a dark theme.
                //
                // Stripped rather than kept-with-their-text because markdown has no cell-fill
                // syntax at all: toMarkdown() drops these, so a composer that showed them would
                // be showing the author something the recipient can never receive.
                const std::array<QTextFormat::Property,9> cellProperties{{
                    QTextFormat::TableCellLeftBorder,
                    QTextFormat::TableCellTopBorder,
                    QTextFormat::TableCellRightBorder,
                    QTextFormat::TableCellBottomBorder,
                    QTextFormat::TableCellLeftBorderBrush,
                    QTextFormat::TableCellTopBorderBrush,
                    QTextFormat::TableCellRightBorderBrush,
                    QTextFormat::TableCellBottomBorderBrush,
                    QTextFormat::BackgroundBrush
                }};
                for (auto property : cellProperties)
                {
                    if (cellFormat.hasProperty(property))
                    {
                        cellFormat.clearProperty(property);
                        changed=true;
                    }
                }

                if (changed)
                {
                    cell.setFormat(cellFormat);
                }
            }
        }
    }

    if (suppressUndo)
    {
        document->setUndoRedoEnabled(undoEnabled);
    }
}

/**
 * @brief Strip baked colour/font formatting a rich-text/HTML paste carries, over one range.
 *
 * External HTML bakes `color`/`font-family`/`font-size` per run (measured: a browser's own
 * `#1f2937`/`Calibri`/`14pt` survive straight through QTextEdit::insertFromMimeData()) -- the
 * exact theme-rot trap already documented for table borders and blockquote colour elsewhere in
 * this file: correct only until the user switches theme, forever after that.
 *
 * Measured non-lossy: clearing ForegroundBrush/BackgroundBrush/FontFamilies/FontPointSize per
 * fragment leaves bold/italic/anchors/headings/lists/tables untouched and the resulting
 * toMarkdown() output byte-identical to the unstripped version -- this removes cosmetic paint,
 * nothing markdown itself carries.
 *
 * Ranged rather than whole-document: existing content in the editor never carries baked colours
 * in the first place (nothing else in this file sets ForegroundBrush), so a whole-document pass
 * would be harmless here too, but restricting to the pasted range is the more defensible
 * invariant and costs nothing extra to write.
 */
void stripBakedRichTextFormatting(QTextDocument* document, int rangeStart, int rangeEnd)
{
    // Collected first, mutated after: writing a fragment's format back via setCharFormat() can
    // merge it with an identically-formatted neighbour, which would invalidate the block's own
    // fragment iterator if a mutation happened mid-walk (the same reason applyClearFormatting()
    // re-fetches its block by position each iteration rather than holding a fragment iterator
    // across an edit).
    struct Run
    {
        int start;
        int end;
        QTextCharFormat format;
    };
    std::vector<Run> runs;

    // Block-level fills, collected the same way. Qt's HTML importer writes a cell's
    // background-color onto the BLOCK format inside the cell as well as onto the cell itself
    // (measured: the same #b0b3b2/#d4d4d4/#f2f2f2 appear in both places for a Numbers table), so
    // clearing only one of the two leaves the fill on screen.
    std::vector<int> blockBackgrounds;

    for (auto block=document->findBlock(rangeStart); block.isValid() && block.position()<rangeEnd;
         block=block.next())
    {
        if (block.blockFormat().hasProperty(QTextFormat::BackgroundBrush))
        {
            blockBackgrounds.push_back(block.position());
        }

        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }

            const auto fragmentStart=fragment.position();
            const auto fragmentEnd=fragmentStart+fragment.length();
            if (fragmentEnd<=rangeStart || fragmentStart>=rangeEnd)
            {
                continue;
            }

            auto format=fragment.charFormat();
            if (!format.hasProperty(QTextFormat::ForegroundBrush)
                && !format.hasProperty(QTextFormat::BackgroundBrush)
                && !format.hasProperty(QTextFormat::FontFamilies)
                && !format.hasProperty(QTextFormat::FontPointSize))
            {
                continue;
            }

            format.clearProperty(QTextFormat::ForegroundBrush);
            format.clearProperty(QTextFormat::BackgroundBrush);
            format.clearProperty(QTextFormat::FontFamilies);
            format.clearProperty(QTextFormat::FontPointSize);

            runs.push_back(Run{fragmentStart,fragmentEnd,format});
        }
    }

    for (const auto& run : runs)
    {
        QTextCursor fragmentCursor(document);
        fragmentCursor.setPosition(run.start);
        fragmentCursor.setPosition(run.end,QTextCursor::KeepAnchor);
        fragmentCursor.setCharFormat(run.format);
    }

    for (auto position : blockBackgrounds)
    {
        QTextCursor blockCursor(document->findBlock(position));
        auto blockFormat=blockCursor.blockFormat();
        blockFormat.clearProperty(QTextFormat::BackgroundBrush);
        blockCursor.setBlockFormat(blockFormat);
    }
}

//! An anchor carrying the mention scheme -- see mentionUrlScheme() (Stage 6).
bool isMentionFormat(const QTextCharFormat& format)
{
    return format.isAnchor() && isMentionHref(format.anchorHref());
}

//! An anchor that is NOT a mention -- what "a link" means throughout this editor as of Stage 6.
bool isLinkFormat(const QTextCharFormat& format)
{
    return format.isAnchor() && !isMentionHref(format.anchorHref());
}

//! Which side of cursor.position() an anchor-run walk looks at -- see selectAnchorRun().
enum class AnchorRunSide
{
    //! The run containing the character BEFORE position() -- QTextCursor::charFormat()'s own
    //! documented rule (format of the preceding character, cursor unselected), and what
    //! Backspace and an insert-at-the-caret act on.
    Before,

    //! The run containing the character AT position() -- what a forward Delete acts on.
    After
};

/**
 * @brief Extend `cursor`'s selection to the full contiguous anchor run at its position.
 * @param side Which neighbouring character the run is anchored on -- see AnchorRunSide.
 * @param matches Which anchors this walk may consider AT ALL -- isLinkFormat for an ordinary
 *  link, isMentionFormat for a mention. The SAME-HREF contiguity rule is applied on top of it
 *  regardless, so a walk can never cross from one anchor into an adjacent DIFFERENT one
 *  (measured: two adjacent anchors with different hrefs never merge into one fragment).
 * @return false, cursor untouched, if the position is not inside a matching run.
 *
 * Generalized out of what used to be MessageEditor::selectLinkRunAtCursor()'s body (Stage 5b) so
 * the Stage 6 atomicity guard, which lives on EnhancedTextEdit and cannot reach a private
 * MessageEditor member, reuses the identical walk instead of a second copy of it.
 * selectLinkRunAtCursor() is now a two-line wrapper over this, with `side` fixed to Before
 * (its own behaviour is unchanged: computing currentIndex from the block walk and matching on the
 * SPAN's own format is equivalent to the old "check cursor.charFormat() first" shape whenever the
 * cursor carries no selection, since the span satisfying position>start && position<=end is
 * exactly the one holding the character immediately preceding position()).
 *
 * A run built by two separate char-format writes, or one with mixed bold/italic inside it, stays
 * split across several fragments with identical hrefs, which is what the outward walk is for.
 * Anchors do not cross block boundaries in this editor, so the walk is block-local.
 */
template <typename PredicateT>
bool selectAnchorRun(QTextCursor& cursor, AnchorRunSide side, const PredicateT& matches)
{
    const auto position=cursor.position();

    struct FragmentSpan
    {
        int start;
        int end;
        bool isAnchor;
        QTextCharFormat format;
    };
    std::vector<FragmentSpan> spans;
    int currentIndex=-1;

    const auto block=cursor.block();
    for (auto it=block.begin(); !it.atEnd(); ++it)
    {
        const auto fragment=it.fragment();
        if (!fragment.isValid())
        {
            continue;
        }

        const auto start=fragment.position();
        const auto end=start+fragment.length();
        const auto format=fragment.charFormat();
        spans.push_back(FragmentSpan{start,end,format.isAnchor(),format});

        const auto inRange=(side==AnchorRunSide::Before)
            ? (position>start && position<=end)
            : (position>=start && position<end);
        if (currentIndex==-1 && inRange)
        {
            currentIndex=static_cast<int>(spans.size())-1;
        }
    }

    if (currentIndex==-1 || !matches(spans[static_cast<std::size_t>(currentIndex)].format))
    {
        return false;
    }
    const auto href=spans[static_cast<std::size_t>(currentIndex)].format.anchorHref();

    auto isSameRun=[&](const FragmentSpan& span)
    {
        return span.isAnchor && matches(span.format) && span.format.anchorHref()==href;
    };

    // Extend outward while the immediate neighbour matches AND carries the SAME href, and is
    // contiguous.
    auto first=static_cast<std::size_t>(currentIndex);
    while (first>0 && isSameRun(spans[first-1]) && spans[first-1].end==spans[first].start)
    {
        --first;
    }

    auto last=static_cast<std::size_t>(currentIndex);
    while (last+1<spans.size() && isSameRun(spans[last+1]) && spans[last+1].start==spans[last].end)
    {
        ++last;
    }

    cursor.setPosition(spans[first].start);
    cursor.setPosition(spans[last].end,QTextCursor::KeepAnchor);
    return true;
}

//! Longest run tokenized as a spell-checkable word (task-spellcheck.md). Past this it is a paste
//! of something that is not prose.
constexpr const int MaxSpellWordChars=64;

//! Blocks longer than this are skipped by the spell pass entirely -- a pasted wall of text should
//! not make every keystroke in it quadratic.
constexpr const int MaxSpellBlockChars=10000;

//! Offsets WITHIN one block's text -- see spellTokens()/nonProseRuns().
struct SpellToken
{
    int start=0;
    int length=0;
};

//! Whether [start,start+length) overlaps any range in `excluded`.
bool spellRangeExcluded(int start, int length, const std::vector<SpellToken>& excluded)
{
    const auto end=start+length;
    for (const auto& range : excluded)
    {
        if (start<range.start+range.length && end>range.start)
        {
            return true;
        }
    }
    return false;
}

/** @brief Ranges of a block that carry no prose and must never be tokenized as words.
 *
 * Anchors -- an ordinary link AND a mention, one isAnchor() test covers both, exactly as
 * EnhancedTextEdit::mentionQueryAtCursor()'s own gate does -- and fixed-pitch runs (inline code).
 * A fenced code BLOCK never reaches this function at all: MessageEditorHighlighter::
 * highlightBlock() returns before ever calling the spell pass for one, see
 * MessageEditorHighlighter::highlightMisspellings()'s own call site.
 *
 * Links and mentions are excluded on purpose, not merely as a courtesy: QSyntaxHighlighter::
 * setFormat() ASSIGNS the format it is given rather than merging it onto whatever this
 * highlighter already painted, so a spell format applied over a link range would erase the
 * link's own foreground colour. (The squiggle itself no longer competes with an underline
 * property the way it used to -- see EnhancedTextEdit::SpellCheckUnderlineProperty -- but a URL
 * or "@handle" is still not prose, and offering one to a dictionary's check() would only ever
 * produce noise.) Skipping anchors here removes both problems outright rather than trying to
 * merge around them, which also happens to be exactly what the task's own brief asks for
 * ("hyperlinks and usernames should pass spell checker").
 */
std::vector<SpellToken> nonProseRuns(const QTextBlock& block)
{
    std::vector<SpellToken> runs;
    const auto blockStart=block.position();
    for (auto it=block.begin(); !it.atEnd(); ++it)
    {
        const auto fragment=it.fragment();
        if (!fragment.isValid())
        {
            continue;
        }
        const auto format=fragment.charFormat();
        if (format.isAnchor() || format.fontFixedPitch())
        {
            runs.push_back(SpellToken{fragment.position()-blockStart,fragment.length()});
        }
    }
    return runs;
}

//! A whitespace-delimited run that is not prose at all: a URL, an e-mail address, or a plain
//! "@handle"/"#tag". One textual test applied to the WHOLE run rather than three separate checks
//! on individual words, so e.g. "teh" inside "https://x.example/teh" is never offered as a
//! misspelling on its own.
bool isSkippableSpellRun(QStringView run)
{
    return run.contains(QLatin1String("://"))
        || run.contains(QLatin1Char('@'))
        || run.startsWith(QLatin1String("www."))
        || run.startsWith(QLatin1Char('#'));
}

/** @brief Cut `text` into spell-checkable words (task-spellcheck.md).
 *
 * Genuinely new code -- nothing else in this editor does word boundaries.
 * EnhancedTextEdit::mentionQueryAtCursor()'s own word rule ("anything that is neither whitespace
 * nor '@'") is deliberately NOT reused here: it is permissive on purpose, for a username alphabet
 * that is the HOST's business, and would tokenize a whole URL as one "word".
 *
 * Rules, in order:
 *  1. Split on QChar::isSpace(). A run containing "://" or '@', or starting with "www." or '#',
 *     is dropped WHOLE (isSkippableSpellRun()) -- a URL/e-mail/handle/tag is not prose.
 *  2. Inside a surviving run, a word STARTS only at QChar::isLetter() -- a digit never starts one.
 *  3. A word CONTINUES over letters, and over an apostrophe/right single quote/hyphen only when
 *     the NEXT character is also a letter -- so "don't" and "well-known" are one token each,
 *     while a trailing "word-" or "word'" stops at the letter.
 *  4. A token containing any digit is dropped whole (abc123, v1beta) -- one cheap rule that
 *     removes most code-ish noise a chat composer can carry unfenced.
 *  5. A token of length 1 is dropped -- "a"/"I" are correct anyway, and a single-letter squiggle
 *     is only ever an annoyance.
 *  6. A token longer than MaxSpellWordChars is dropped.
 *  7. A token is dropped if it has an uppercase letter anywhere past position 0 while not being
 *     ALL uppercase -- camelCase/PascalCase identifiers ("getFooBar"). "NASA" and "Alice" both
 *     survive. Dropped whole rather than split: splitting would yield fragments that mostly pass
 *     the dictionary, which looks like it works while adding cost for no real benefit.
 */
std::vector<SpellToken> spellTokens(const QString& text)
{
    std::vector<SpellToken> tokens;

    int runStart=0;
    auto flushRun=[&](int runEnd)
    {
        if (runEnd<=runStart)
        {
            return;
        }
        const QStringView run(text.constData()+runStart,runEnd-runStart);
        if (isSkippableSpellRun(run))
        {
            return;
        }

        int i=runStart;
        while (i<runEnd)
        {
            if (!text.at(i).isLetter())
            {
                ++i;
                continue;
            }

            const auto wordStart=i;
            bool hasDigit=false;
            bool hasUpperPastFirst=false;
            bool allUpper=text.at(i).isUpper();
            ++i;
            while (i<runEnd)
            {
                const auto c=text.at(i);
                if (c.isLetter())
                {
                    if (c.isUpper())
                    {
                        hasUpperPastFirst=true;
                    }
                    else
                    {
                        allUpper=false;
                    }
                    ++i;
                    continue;
                }
                if (c.isDigit())
                {
                    hasDigit=true;
                    ++i;
                    continue;
                }
                if ((c==QLatin1Char('\'') || c==QChar(0x2019) || c==QLatin1Char('-'))
                    && i+1<runEnd && text.at(i+1).isLetter())
                {
                    ++i;
                    continue;
                }
                break;
            }

            const auto length=i-wordStart;
            if (!hasDigit && length>1 && length<=MaxSpellWordChars
                && !(hasUpperPastFirst && !allUpper))
            {
                tokens.push_back(SpellToken{wordStart,length});
            }
        }
    };

    for (int i=0; i<=text.size(); ++i)
    {
        if (i==text.size() || text.at(i).isSpace())
        {
            flushRun(i);
            runStart=i+1;
        }
    }

    return tokens;
}

/** @brief The spell-checkable token `offset` sits INSIDE -- the word a caret there is typing.
 *
 * Deliberately stricter than EnhancedTextEdit::spellWordAt()'s rule, which also answers for a
 * caret sitting at the word's very first character: such a caret has not touched the word yet, so
 * dropping its squiggle merely because the caret was clicked in front of it would be a change the
 * user did not ask for. A caret at the word's END is the typing case proper -- that is exactly
 * where every keystroke of it leaves the caret.
 *
 * @return A token of length 0 when `offset` is inside no spell-checkable word (whitespace, an
 *  anchor/inline-code run, or a run the tokenizer drops whole -- see spellTokens()).
 */
SpellToken typedSpellToken(const QTextBlock& block, int offset)
{
    if (!block.isValid())
    {
        return SpellToken{};
    }

    const auto text=block.text();
    if (offset<0 || offset>text.size())
    {
        return SpellToken{};
    }

    const auto excluded=nonProseRuns(block);
    for (const auto& token : spellTokens(text))
    {
        if (offset<=token.start || offset>token.start+token.length)
        {
            continue;
        }
        if (spellRangeExcluded(token.start,token.length,excluded))
        {
            return SpellToken{};
        }
        return token;
    }

    return SpellToken{};
}

//! macOS shows a DOTTED spelling underline -- qcocoatheme.mm's own
//! QPlatformTheme::SpellCheckUnderlineStyle answer, which this editor used to defer to before it
//! started painting the squiggle itself (see EnhancedTextEdit::SpellCheckUnderlineProperty).
//! Every other platform gets a wave, Qt's own fallback shape. Preserved verbatim here so the
//! change of MECHANISM is not also a change of LOOK -- only the width is.
constexpr bool isDottedSpellUnderline()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

//! Pen width of the spellcheck squiggle when EnhancedTextEdit::spellCheckUnderlineWidth() is 0
//! ("auto"). Platform-dependent on purpose -- see that property's own doc comment for why.
qreal autoSpellUnderlineWidth(const QFontMetricsF& metrics)
{
    if (isDottedSpellUnderline())
    {
        return std::max(qreal(2),metrics.lineWidth()*qreal(2));
    }
    return std::max(qreal(1),metrics.lineWidth());
}

//! Round dots, macOS's own spelling-underline shape -- a dash pattern whose "on" segment is
//! effectively zero-length, so a round cap turns every dash into a circle of diameter `width`.
//! Pattern entries are multiples of the pen width, so the spacing scales with the width and the
//! dots never crowd as the pen grows. Antialiasing is mandatory here: without it the round caps
//! square off and the line reads as a thin dashed rule rather than as dots.
void drawSpellDots(QPainter& painter, qreal x1, qreal x2, qreal y, qreal width, const QColor& color)
{
    QPen pen(color);
    pen.setWidthF(width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setDashPattern(QList<qreal>{0.01,2.0});
    painter.setPen(pen);
    painter.drawLine(QPointF(x1,y),QPointF(x2,y));
}

//! A wave, Qt's own fallback shape for SpellCheckUnderline on every platform but macOS, built
//! from quadratic curves at OUR pen width rather than from Qt's cached wavy pixmap, which is
//! generated at the font's lineThickness() and cannot be widened through any QTextCharFormat API
//! -- the whole reason this squiggle is painted by hand now.
//!
//! `y` is the CENTRE line of the wave; it peaks `width` above and below it, so the band is
//! 2*width tall plus the pen itself. A quadratic Bezier peaks at HALF its control-point offset,
//! hence the 2* on the control points below. A final half-period truncated by x2 is left as-is --
//! Qt's own tiled pixmap does the same, and a wave that stops short of the last letter looks
//! worse than one that overshoots by half a period.
void drawSpellWave(QPainter& painter, qreal x1, qreal x2, qreal y, qreal width, const QColor& color)
{
    const auto amplitude=std::max(qreal(1),width);
    const auto halfPeriod=std::max(qreal(2),amplitude*qreal(2));

    QPainterPath path;
    path.moveTo(x1,y);
    auto x=x1;
    auto direction=qreal(-1);
    while (x<x2)
    {
        const auto next=std::min(x+halfPeriod,x2);
        path.quadTo(x+(next-x)/qreal(2),y+direction*amplitude*qreal(2),next,y);
        x=next;
        direction=-direction;
    }

    QPen pen(color);
    pen.setWidthF(width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

//! One SpellCheckUnderlineProperty range, split across however many wrapped QTextLines it spans,
//! and drawn below wherever Qt puts the run's OWN solid underline (if any) so a misspelling
//! inside underlined text shows both marks.
//!
//! `origin` is the block's layout origin in VIEWPORT coordinates -- QTextLine::y() and
//! QTextLine::cursorToX() are both relative to it (cursorToX() already folds in the line's own x
//! offset and the paragraph alignment).
void drawSpellRange(QPainter& painter, const QTextBlock& block, const QPointF& origin,
                     int start, int length, const QColor& color, qreal configuredWidth)
{
    auto* layout=block.layout();
    if (layout==nullptr)
    {
        return;
    }
    const auto end=start+length;

    // The word's OWN format, not its predecessor's: QTextCursor::charFormat() reports the
    // character BEFORE the position, hence the +1. Constructed from the BLOCK, not
    // block.document() -- that returns a const QTextDocument*, and QTextCursor has no
    // const-document constructor; the block constructor gets at the same document without one.
    QTextCursor probe(block);
    probe.setPosition(block.position()+start+1);
    const QFontMetricsF metrics(probe.charFormat().font());

    const auto width=(configuredWidth>0) ? configuredWidth : autoSpellUnderlineWidth(metrics);

    for (int i=0; i<layout->lineCount(); ++i)
    {
        const auto line=layout->lineAt(i);
        const auto lineStart=line.textStart();
        const auto lineEnd=lineStart+line.textLength();

        const auto from=std::max(start,lineStart);
        const auto to=std::min(end,lineEnd);
        if (from>=to)
        {
            continue;
        }

        auto x1=line.cursorToX(from);
        auto x2=line.cursorToX(to);
        if (x2<x1)
        {
            // RTL: the logical start of the word is its visual RIGHT edge.
            std::swap(x1,x2);
        }
        if (x2-x1<qreal(1))
        {
            continue;
        }

        // Where Qt puts the user's OWN solid underline (drawTextItemDecoration(), qpainter.cpp)
        // -- reproduced rather than guessed, because the whole reason this squiggle is painted by
        // hand is so the two can coexist, which means knowing what to stay clear of.
        const auto baseline=origin.y()+line.y()+line.ascent();
        auto underlineOffset=std::ceil(metrics.underlinePos())+qreal(0.5);
        if (metrics.underlinePos()<=metrics.descent())
        {
            underlineOffset=std::min(underlineOffset,metrics.descent()-qreal(0.5));
        }
        const auto solidBottom=baseline+underlineOffset+metrics.lineWidth()/qreal(2);

        // Bottom-anchored in the line's own text box (ascent+descent, leading excluded) so runs
        // of different sizes on one line put their squiggles on one visual line, and so the gap
        // above the word below is as large as the descent allows.
        auto bandBottom=origin.y()+line.y()+line.ascent()+line.descent()-qreal(0.5);
        if (bandBottom-width<solidBottom+qreal(1))
        {
            // Too tight to fit both inside the line's own text box (a small font with a deep
            // underline position). Pushed below the solid line anyway and allowed to spill by a
            // couple of pixels into the line's leading -- a squiggle drawn ON TOP of the user's
            // underline is worse than one that reaches slightly into the gap.
            bandBottom=solidBottom+qreal(1)+width;
        }
        const auto lineBottom=origin.y()+line.y()+line.height();
        bandBottom=std::min(bandBottom,lineBottom+qreal(2));

        if (isDottedSpellUnderline())
        {
            drawSpellDots(painter,origin.x()+x1,origin.x()+x2,
                          bandBottom-width/qreal(2),width,color);
        }
        else
        {
            drawSpellWave(painter,origin.x()+x1,origin.x()+x2,
                          bandBottom-width/qreal(2)-std::max(qreal(1),width),width,color);
        }
    }
}

}

/*****************************MessageEditorHighlighter*************************/

/** @brief Paints the constructs Qt's layout does not: blockquotes and fenced code blocks.
 *
 * A QSyntaxHighlighter rather than char-format writes, for reasons measured rather than assumed --
 * see EnhancedTextEdit::setBlockquoteColor()'s doc comment for the full list. The short version:
 * this touches the block LAYOUT, not the document, so nothing is baked in, nothing leaks into
 * toMarkdown()/toHtml(), no undo step is added, and a theme switch costs one rehighlight().
 *
 * A QTextDocument supports exactly ONE QSyntaxHighlighter, so everything the editor needs to
 * highlight lives here rather than in a second highlighter that would fight this one over the same
 * layout formats.
 *
 * No Q_OBJECT: it declares no signals or slots of its own, so it needs no moc pass.
 */
class MessageEditorHighlighter : public QSyntaxHighlighter
{
    public:

        /**
         * @param document The document to highlight, as QSyntaxHighlighter takes it.
         * @param editor The widget that owns both -- READ ONLY, and only ever for
         *  EnhancedTextEdit::typedSpellWord(), which highlightMisspellings() has to ask for the
         *  caret's current whereabouts at highlight time rather than be told in advance. See
         *  that function for why being told would always be one keystroke late.
         */
        explicit MessageEditorHighlighter(QTextDocument* document, EnhancedTextEdit* editor=nullptr)
            : QSyntaxHighlighter(document),
              m_editor(editor)
        {}

        //! QSyntaxHighlighter block states. Deliberately not -1, which is the "never highlighted"
        //! value previousBlockState() reports for a block Qt has not visited.
        //!
        //! Public (Stage 5b): QTextFormat::BlockCodeFence, the property-based predicate
        //! currentFormatState() otherwise uses for state.codeBlock, is NEVER set on a literal
        //! fence block -- convertCodeBlocksToText() strips exactly that property on the way in,
        //! since a literal fence carries no block properties at all (that is the whole point of
        //! it being visible, editable text). Measured: QTextBlock::userState() reflects fence
        //! membership correctly and SYNCHRONOUSLY once this highlighter is attached to a real
        //! QTextEdit's document (no explicit rehighlight() needed, unlike a bare, view-less
        //! QTextDocument) -- so block.userState()==InFence is the query that actually works for
        //! "is the caret on/inside a literal fence", and MessageEditor::currentFormatState() uses
        //! it below.
        enum BlockState : int
        {
            OutsideFence=0,
            InFence=1
        };

        void setBlockquoteColor(const QColor& color)
        {
            if (m_blockquoteColor==color)
            {
                return;
            }

            m_blockquoteColor=color;
            rehighlight();
        }

        void setCodeBlockColor(const QColor& color)
        {
            if (m_codeBlockColor==color)
            {
                return;
            }

            m_codeBlockColor=color;
            rehighlight();
        }

        void setLinkColor(const QColor& color)
        {
            if (m_linkColor==color)
            {
                return;
            }

            m_linkColor=color;
            rehighlight();
        }

        void setLinkUnderline(bool enable)
        {
            if (m_linkUnderline==enable)
            {
                return;
            }

            m_linkUnderline=enable;
            rehighlight();
        }

        //! See EnhancedTextEdit::mentionColor -- Stage 6. Applied by highlightLinks() below, on
        //! the same display-only terms as setLinkColor().
        void setMentionColor(const QColor& color)
        {
            if (m_mentionColor==color)
            {
                return;
            }

            m_mentionColor=color;
            rehighlight();
        }

        //! task-spellcheck.md. NOT owned -- EnhancedTextEdit::setSpellChecker() nulls this back
        //! out itself when the checker changes or is destroyed.
        void setSpellChecker(AbstractSpellChecker* checker)
        {
            if (m_spellChecker==checker)
            {
                return;
            }

            m_spellChecker=checker;
            clearSpellCache();
        }

        void setSpellCheckEnabled(bool enable)
        {
            if (m_spellCheckEnabled==enable)
            {
                return;
            }

            m_spellCheckEnabled=enable;
            rehighlight();
        }

        //! See EnhancedTextEdit::spellCheckUnderlineColor. Applied by highlightMisspellings()
        //! below, on the same display-only terms as every colour property above -- except that,
        //! unlike them, an INVALID colour does not disable the pass: see that property's own doc
        //! comment for why.
        void setSpellCheckUnderlineColor(const QColor& color)
        {
            if (m_spellCheckUnderlineColor==color)
            {
                return;
            }

            m_spellCheckUnderlineColor=color;
            rehighlight();
        }

        //! Drops every cached verdict and re-highlights -- called when the checker itself
        //! changes, and (debounced, from EnhancedTextEdit::onSpellDictionaryChanged()) when the
        //! checker's own AbstractSpellChecker::dictionaryChanged() fires.
        void clearSpellCache()
        {
            m_spellCache.clear();
            rehighlight();
        }

        /** @brief Re-run the spell pass over the blocks holding `positionA` and `positionB`
         *  (document coordinates; -1 for "none", the same block twice counts once).
         *
         * What EnhancedTextEdit::updateTypedSpellWord() calls when the word being typed changes
         * WITHOUT the document changing -- the caret walked out of it with an arrow key, a click,
         * or the focus left the widget. A text edit needs nothing: Qt already re-runs the edited
         * block, and highlightMisspellings() reads the caret live, so that pass has the answer
         * already.
         *
         * Never rehighlight(): this runs on caret moves and a full pass is O(document).
         */
        void rehighlightSpellWordBlocks(int positionA, int positionB)
        {
            if (!m_spellCheckEnabled || m_spellChecker==nullptr)
            {
                return;
            }

            auto* doc=document();
            if (doc==nullptr)
            {
                return;
            }

            const auto blockA=positionA>=0 ? doc->findBlock(positionA) : QTextBlock();
            if (blockA.isValid())
            {
                rehighlightBlock(blockA);
            }

            const auto blockB=positionB>=0 ? doc->findBlock(positionB) : QTextBlock();
            if (blockB.isValid() && blockB.blockNumber()!=blockA.blockNumber())
            {
                rehighlightBlock(blockB);
            }
        }

    protected:

        void highlightBlock(const QString& text) override
        {
            // NO early return on empty text: an empty line inside a fence still has to carry the
            // fence state forward to the next block, or the run would silently end at it.
            const auto blockFormat=currentBlock().blockFormat();

            // A fenced code block, recognised from the TEXT rather than from block properties,
            // because in this editor the "```" delimiters are ordinary document text -- that is
            // what makes them visible and editable in place, language tag and all. Fence state has
            // to be carried across blocks, which is exactly what QSyntaxHighlighter's per-block
            // state is for; without it every line would be judged in isolation and only the
            // delimiters themselves would ever be marked.
            //
            // The property test is kept alongside it as a belt-and-braces case: a host that builds
            // a document by hand, or content that reached the document by some path that does not
            // run convertCodeBlocksToText(), is still recognised. It is the same predicate the
            // markdown writer uses (qtextmarkdownwriter.cpp:415).
            const auto fence=fenceRun(text);
            const bool wasInFence=(previousBlockState()==InFence);

            bool inFence=wasInFence;
            if (!fence.isEmpty())
            {
                // A delimiter belongs to the block it opens or closes, so it is painted either
                // way; only the state flips.
                inFence=!wasInFence;
            }
            setCurrentBlockState(inFence ? InFence : OutsideFence);

            if (wasInFence || !fence.isEmpty()
                || blockFormat.hasProperty(QTextFormat::BlockCodeFence)
                || !blockFormat.stringProperty(QTextFormat::BlockCodeLanguage).isEmpty()
                || blockFormat.nonBreakableLines())
            {
                QTextCharFormat format;
                format.setFontFixedPitch(true);
                format.setFontFamilies(
                    QFontDatabase::systemFont(QFontDatabase::FixedFont).families()
                );
                if (m_codeBlockColor.isValid())
                {
                    format.setForeground(m_codeBlockColor);
                }
                if (!text.isEmpty())
                {
                    setFormat(0,static_cast<int>(text.size()),format);
                }
                return;
            }

            // An invalid colour means "leave quoted text alone" -- the state a host that ships no
            // stylesheet is in. The indent alone marks the quote there.
            //
            // Any depth, one colour -- matching messagetext.css, which likewise does not deepen
            // the colour with nesting.
            if (m_blockquoteColor.isValid()
                && blockFormat.intProperty(QTextFormat::BlockQuoteLevel)>0)
            {
                QTextCharFormat format;
                format.setForeground(m_blockquoteColor);

                // Merges onto whatever the fragment already has, so bold/italic inside a quote
                // survive -- QSyntaxHighlighter formats are applied over the document's own,
                // never instead of them.
                setFormat(0,static_cast<int>(text.size()),format);
            }

            highlightLinks();

            // LAST, after highlightLinks(), on the same rule this file already documents: a
            // later setFormat() wins over an earlier one on an overlapping range. Nothing here
            // actually overlaps a link or mention -- anchors are skipped wholesale, see
            // nonProseRuns() -- but running last is what keeps a misspelling's squiggle visible
            // over a blockquote's own colour (highlightMisspellings() reads that colour back via
            // format() and re-applies it, rather than starting from a blank format).
            highlightMisspellings(text);
        }

    private:

        /** @brief Paint every anchor run in the current block, on the same display-only terms as
         *  the blockquote and code-block colours above.
         *
         * Needed because an anchor renders as ORDINARY TEXT otherwise: measured, a document with
         * an anchor and the same document without one paint pixel-for-pixel identically (429 px
         * each). Qt's own markdown importer papers over this by baking foreground=#0000ff onto
         * every anchor it reads, which is why a link only *looked* right after a round trip
         * through Markdown mode -- but that colour is frozen at import time, ignores the theme,
         * and leaks into toHtml(). stripImportedAnchorStyle() removes it on the way in, and this
         * paints all links, typed and imported alike, in one themed colour.
         *
         * Stage 6: a MENTION is an anchor too, so it is painted by this same pass -- but with
         * m_mentionColor when that is valid, so it reads as distinct from an ordinary link. One
         * pass with a per-fragment colour choice rather than a second pass that overwrites the
         * first: the result does not then depend on setFormat() call ORDER, and it matches
         * ChatMessageTextBrowser::applyDocumentStyle() exactly, where the
         * `a[href^="whitem-mention:"]` rule wins over the blanket `a` rule by CSS specificity
         * when present, and leaves it in charge when absent -- an unset mentionColor falls back
         * to linkColor here for the identical reason.
         *
         * Runs LAST so a link (or mention) inside a blockquote reads as one rather than as quoted
         * text -- later setFormat() calls win over earlier ones on an overlapping range.
         */
        void highlightLinks()
        {
            if (!m_linkColor.isValid() && !m_mentionColor.isValid() && !m_linkUnderline)
            {
                return;
            }

            const auto block=currentBlock();
            const auto blockStart=block.position();
            for (auto it=block.begin(); !it.atEnd(); ++it)
            {
                const auto fragment=it.fragment();
                const auto anchorFormat=fragment.charFormat();
                if (!fragment.isValid() || !anchorFormat.isAnchor())
                {
                    continue;
                }

                const auto& color=(isMentionHref(anchorFormat.anchorHref()) && m_mentionColor.isValid())
                    ? m_mentionColor
                    : m_linkColor;

                QTextCharFormat format;
                if (color.isValid())
                {
                    format.setForeground(color);
                }
                // Set either way, so linkUnderline:false also strips an underline an imported
                // document happened to carry.
                format.setFontUnderline(m_linkUnderline);

                setFormat(fragment.position()-blockStart,fragment.length(),format);
            }
        }

        /** @brief Mark every word no active dictionary accepts (task-spellcheck.md).
         *
         * Runs after highlightLinks() -- see that call site's own comment for why -- and skips
         * anything nonProseRuns() marks (an anchor or an inline-code run), so a link/mention/code
         * span is never handed to check() at all, never mind marked.
         *
         * This pass only ever MARKS a range with EnhancedTextEdit::SpellCheckUnderlineProperty --
         * it never touches underlineStyle/underlineColor any more, see that property's own doc
         * comment for why. EnhancedTextEdit::paintEvent() is what actually draws the squiggle.
         *
         * QSyntaxHighlighter::setFormat() ASSIGNS the format it is given rather than merging it
         * onto whatever this highlighter already painted for the range (it merges only onto the
         * DOCUMENT's own char formats, which is what the blockquote pass's comment above is
         * describing) -- so seeding `format` from this->format(token.start) rather than from a
         * default-constructed QTextCharFormat is what keeps a misspelling's blockquote colour
         * intact instead of erasing it.
         */
        void highlightMisspellings(const QString& text)
        {
            if (!m_spellCheckEnabled || m_spellChecker==nullptr || !m_spellChecker->isReady()
                || text.isEmpty() || text.size()>MaxSpellBlockChars)
            {
                return;
            }

            const auto block=currentBlock();
            const auto blockStart=block.position();
            const auto excluded=nonProseRuns(block);

            /* The word being typed right now, asked for LIVE rather than pushed in advance.
             *
             * A half-typed word is not a misspelling: squiggling every prefix of one ("h", "he",
             * "hel", ...) as it is typed is the one thing a check-as-you-type pass must not do, so
             * the check is deferred to the word boundary -- the moment the caret leaves the word,
             * or typing stops (see EnhancedTextEdit::typedSpellWord() for the exact rule).
             *
             * Read here, inside the pass, and not stored: Qt runs this pass from
             * QTextDocument::contentsChange, which QTextDocumentPrivate::finishEdit() emits BEFORE
             * the cursorPositionChanged()/contentsChanged() that any push would have to ride on,
             * while the caret itself is adjusted DURING the edit that precedes all three. So
             * asking now is asking about the keystroke that just landed, whereas being told would
             * always describe the one before it -- which would mark the word for one pass and
             * unmark it in a second, and hand every prefix to check() (an async checker would
             * queue a dictionary lookup for each) on the way.
             *
             * Asked only for the block the caret is actually in -- no other block can hold the word
             * being typed -- so a full rehighlight() over a long document still tokenizes each block
             * once rather than tokenizing the caret's block again for every block it visits.
             */
            const auto caretPosition=m_editor!=nullptr ? m_editor->textCursor().position() : -1;
            const auto typed=(m_editor!=nullptr && caretPosition>=blockStart
                              && caretPosition<=blockStart+text.size())
                                    ? m_editor->typedSpellWord()
                                    : EnhancedTextEdit::SpellWord{};

            for (const auto& token : spellTokens(text))
            {
                if (spellRangeExcluded(token.start,token.length,excluded))
                {
                    continue;
                }

                // Matched by EXACT (position,length) rather than by overlap: `typed` is always a
                // token this same tokenizer produced, so exactness costs nothing and cannot
                // swallow a neighbouring word.
                if (typed.isValid
                    && blockStart+token.start==typed.position
                    && token.length==typed.length)
                {
                    continue;
                }

                const auto word=text.mid(token.start,token.length);
                const auto cached=m_spellCache.constFind(word);
                bool correct=false;
                if (cached!=m_spellCache.constEnd())
                {
                    correct=cached.value();
                }
                else
                {
                    const auto verdict=m_spellChecker->check(word);
                    if (verdict==SpellCheckVerdict::Unknown)
                    {
                        // No answer yet: paint nothing and cache nothing. The checker emits
                        // dictionaryChanged() once it can answer, and EnhancedTextEdit
                        // re-highlights from that (debounced).
                        continue;
                    }
                    correct=(verdict==SpellCheckVerdict::Correct);
                    if (m_spellCache.size()>SpellCacheLimit)
                    {
                        // A composer never approaches this in practice; a wholesale clear rather
                        // than an LRU because the refill cost is a handful of cheap lookups.
                        m_spellCache.clear();
                    }
                    m_spellCache.insert(word,correct);
                }

                if (correct)
                {
                    continue;
                }

                // A property of our OWN rather than QTextCharFormat::SpellCheckUnderline, which
                // this pass used until the collision was measured: setUnderlineStyle() and
                // setFontUnderline() write the SAME QTextFormat::TextUnderlineStyle property, and
                // a QSyntaxHighlighter format is merged OVER the document's own char format at
                // paint time -- so a misspelled word inside text the user underlined from the
                // toolbar lost its solid underline entirely, leaving only the squiggle. Marking
                // instead of styling leaves the document's fontUnderline untouched, and
                // EnhancedTextEdit::paintEvent() draws the squiggle below it, so both are visible.
                // It is also what makes a squiggle thicker than the font's own lineThickness()
                // possible at all -- there is no QTextCharFormat knob for that either. See
                // EnhancedTextEdit::SpellCheckUnderlineProperty for the full story.
                auto format=this->format(token.start);
                format.setProperty(EnhancedTextEdit::SpellCheckUnderlineProperty,true);
                if (m_spellCheckUnderlineColor.isValid())
                {
                    // Deliberately NOT underlineColor(): merged over a document format that has
                    // fontUnderline=true, that would recolour the user's OWN solid underline red
                    // the moment the two coincide. EnhancedTextEdit::paintEvent() reads this
                    // property to colour the squiggle it draws instead.
                    format.setProperty(
                        EnhancedTextEdit::SpellCheckUnderlineColorProperty,
                        m_spellCheckUnderlineColor
                    );
                }
                setFormat(token.start,token.length,format);
            }
        }

        //! Verdicts cleared wholesale past this size -- see highlightMisspellings().
        constexpr static const int SpellCacheLimit=5000;

        QColor m_blockquoteColor;
        QColor m_codeBlockColor;
        QColor m_linkColor;
        bool m_linkUnderline=false;
        QColor m_mentionColor;

        AbstractSpellChecker* m_spellChecker=nullptr;
        bool m_spellCheckEnabled=true;
        QColor m_spellCheckUnderlineColor;

        //! Not owned -- the editor owns this highlighter (through the document). Read only, and
        //! only for typedSpellWord(); see the ctor.
        EnhancedTextEdit* m_editor=nullptr;

        //! true==correct. SpellCheckVerdict::Unknown is NEVER cached, so an async checker that
        //! has not answered yet converges instead of being poisoned by a stale miss.
        mutable QHash<QString,bool> m_spellCache;
};

/******************************EnhancedTextEdit********************************/

//--------------------------------------------------------------------------

EnhancedTextEdit::EnhancedTextEdit(QWidget* parent) : QTextEdit(parent),
    m_autoResize(true),
    m_newLineOnEnter(false)
{
    // Qt's own 40px per indent level leaves a one-level list sitting a long way from the margin
    // -- fine for a word processor, too much for a chat composer. See DefaultListIndentWidth.
    document()->setIndentWidth(DefaultListIndentWidth);

    applyTabStopDistance();

    // Attached unconditionally, but inert until a stylesheet gives it a colour (see
    // setBlockquoteColor()). Parented to the document by QSyntaxHighlighter's own constructor, so
    // it is not deleted here.
    m_highlighter=new MessageEditorHighlighter(document(),this);

    // Right-click is handled by MessageEditor's own DropdownMenu (see showContextMenu()),
    // not Qt's stock createStandardContextMenu() -- its Paste entry is driven by canPaste(),
    // which would report nothing-to-paste for an attachment payload (see
    // canInsertFromMimeData() below).
    setContextMenuPolicy(Qt::CustomContextMenu);

    // Stage 6: '@'-word detection is pure OBSERVATION -- no key is intercepted for it and nothing
    // about typing changes (the Stage 5b lesson about not altering global typing semantics to
    // achieve a feature). Driven off these two signals rather than QTextDocument::contentsChange
    // because what updateMentionQuery() needs is not the EDIT DELTA but the caret's CURRENT
    // surroundings, and because paste, IME commit and undo/redo all reach these two anyway.
    // Neither signal implies the other (a caret move with no text change fires only
    // cursorPositionChanged; a programmatic setPlainText() fires only textChanged), and
    // updateMentionQuery() recomputes from scratch and early-outs when nothing has actually
    // changed, so firing twice for one edit costs nothing.
    connect(this,&QTextEdit::textChanged,this,&EnhancedTextEdit::updateMentionQuery);
    connect(this,&QTextEdit::cursorPositionChanged,this,&EnhancedTextEdit::updateMentionQuery);

    // The spell pass reads the word being typed itself (see typedSpellWord()), so these three are
    // here only to notice when that word has CHANGED with no edit to provoke a rehighlight of its
    // own: the caret was walked out of it (cursorPositionChanged), or a selection appeared over it,
    // which stops it counting as typed at all and does NOT always move the caret -- selecting a
    // just-typed word backwards from its end leaves the position where it was and reports only
    // selectionChanged. textChanged carries no case of its own; it is what keeps the document
    // revision updateTypedSpellWord() uses to tell an edit from a bare caret move up to date.
    connect(this,&QTextEdit::textChanged,this,&EnhancedTextEdit::updateTypedSpellWord);
    connect(this,&QTextEdit::cursorPositionChanged,this,&EnhancedTextEdit::updateTypedSpellWord);
    connect(this,&QTextEdit::selectionChanged,this,&EnhancedTextEdit::updateTypedSpellWord);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setAutoResizingEnabled(bool enable)
{
    m_autoResize=enable;

    // Only the auto-resizing state may suppress the scrollbar: the widget grows to fit its
    // content there, so a scrollbar would never have anything to scroll and would only steal
    // width. The non-auto-resizing state is exactly the case that needs one -- expanded/clamped
    // mode (see setExpandedEnabled()) pins the height and lets content overflow. Setting
    // AlwaysOff unconditionally here, as this used to, made that overflow silently unreachable.
    setVerticalScrollBarPolicy(enable ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);

    if (enable)
    {
        connect(this,
                &QTextEdit::textChanged,
                this,
                &EnhancedTextEdit::updateSize
            );
    }
    else
    {
        disconnect(this,
                &QTextEdit::textChanged,
                this,
                &EnhancedTextEdit::updateSize
            );
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::updateSize()
{
    updateGeometry();

    if (m_autoResize)
    {
        // Below the ceiling the widget is sized exactly to its content, so a scrollbar would have
        // nothing to scroll and would only steal width -- that is why auto-resize suppresses it.
        // Once sizeHint() starts clamping, though, the content no longer fits and the bar has to
        // come back, or everything past the cap is unreachable. Driven off the same comparison
        // sizeHint() makes rather than a stored flag, so the two cannot disagree.
        const auto contentHeight=static_cast<int>(document()->size().height())+frameWidth()*2;
        setVerticalScrollBarPolicy(
            contentHeight>effectiveMaxHeight() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff
        );
    }
}

//--------------------------------------------------------------------------

QSize EnhancedTextEdit::sizeHint() const
{
    if (m_expanded)
    {
        // Expanded mode occupies the whole allowance at once -- the point of the toggle is a
        // roomy writing area, not one that still creeps up from a single line like auto-resize
        // does. Overflow beyond this is reachable via the scrollbar (see setExpandedEnabled()).
        return QSize(width(),effectiveMaxHeight());
    }

    if (m_autoResize)
    {
        QSizeF size = document()->size();

        // add a small margin for the frame/margins
        int height = static_cast<int>(size.height()) + frameWidth() * 2;

        // The same ceiling expanded mode pins to also CAPS growth here -- an auto-resizing
        // composer that grows without limit eventually swallows the window it lives in. Past the
        // cap the widget stops growing and scrolls instead (see updateSize(), which turns the
        // scrollbar on exactly when this clamp starts biting).
        return QSize(width(), qMin(height,effectiveMaxHeight()));
    }
    return QTextEdit::sizeHint();
}

//--------------------------------------------------------------------------

QSize EnhancedTextEdit::minimumSizeHint() const
{
    auto hint=QTextEdit::minimumSizeHint();

    if (m_autoResize)
    {
        // Height only -- see the doc comment in the header for why the width must keep Qt's own
        // value rather than sizeHint()'s current-width report. Expanded mode deliberately keeps
        // Qt's floor entirely: it pins the height at effectiveMaxHeight(), which is far above the
        // ~90px minimum anyway, and a hard minimum there would stop a cramped host shrinking the
        // editor at all.
        hint.setHeight(sizeHint().height());
    }

    return hint;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setExpandedEnabled(bool enable)
{
    m_expanded=enable;
    setAutoResizingEnabled(!enable);

    // Expanded mode pins the height at effectiveMaxHeight() instead of tracking the document, so
    // sizeHint() now returns a completely different KIND of value. Two separate things have to
    // happen for that to actually take effect, and neither does on its own:
    //
    // 1. QSizePolicy. QAbstractScrollArea's constructor sets Expanding in BOTH directions
    //    (qabstractscrollarea.cpp:275, verified against Qt 6.9.0 source), which means a layout
    //    with any leftover vertical space stretches this widget PAST sizeHint() rather than
    //    stopping at it -- so returning the ceiling from sizeHint() would not, by itself, pin
    //    anything. Fixed makes sizeHint() the only acceptable height. Restoring Expanding on
    //    collapse puts back exactly what QAbstractScrollArea itself installed.
    //
    //    Deliberately done via the size POLICY rather than setMinimumHeight()/setMaximumHeight():
    //    a host's QSS "max-height" is already a real QWidget::maximumHeight() (see
    //    effectiveMaxHeight()), re-applied on every repolish, so writing those properties here
    //    would mean this widget and the host's stylesheet fighting over the same two values.
    //    The policy is ours alone and nothing in QSS touches it.
    //
    // 2. updateGeometry(). Nothing else calls it on this transition: the textChanged ->
    //    updateSize() connection that normally drives it is precisely what setAutoResizingEnabled()
    //    just DISCONNECTED (there is nothing left to resize on typing once the height is pinned).
    //    Without this the parent layout keeps reusing the sizeHint it cached while collapsed, and
    //    the editor visibly does not resize at all when the toolbar appears.
    auto policy=sizePolicy();
    policy.setVerticalPolicy(enable ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    setSizePolicy(policy);

    updateGeometry();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setMaxHeightReferenceWidget(QWidget* widget)
{
    m_maxHeightReference=widget;
    updateGeometry();
}

//--------------------------------------------------------------------------

QWidget* EnhancedTextEdit::maxHeightReferenceWidget() const
{
    // window() rather than parentWidget() by default -- see the setter's doc comment for why a
    // parent sized BY this editor would be a feedback loop.
    return m_maxHeightReference.isNull() ? window() : m_maxHeightReference.data();
}

//--------------------------------------------------------------------------

int EnhancedTextEdit::effectiveMaxHeight() const
{
    // A QSS "max-height" rule is already a real setMaximumHeight() on this widget
    // (QStyleSheetStyle::setGeometry()) -- honour it rather than fighting it, so a host that
    // already caps the composer in its own stylesheet (whitemdesktop's chatpage.qss does, at
    // 300px) keeps exactly the cap it declared. Only when nothing has capped us do our own
    // properties decide.
    const auto qssMax=maximumHeight();
    if (qssMax<QWIDGETSIZE_MAX)
    {
        return qssMax;
    }

    auto height=m_maxHeight;

    if (m_maxHeightPercent>0)
    {
        auto* reference=maxHeightReferenceWidget();
        if (reference!=nullptr && reference!=this)
        {
            // qMax, so the percentage can only ever RAISE the ceiling: on a short window it
            // would otherwise squeeze the editor down to a couple of lines, which is worse than
            // the fixed default it replaced.
            height=qMax(height,reference->height()*m_maxHeightPercent/100);
        }
    }

    return height;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::keyPressEvent(QKeyEvent* event)
{
    // The "the user is typing in here right now" latch the spell pass needs -- see
    // m_spellCheckTyping and typedSpellWord(). Raised at the very top, before any guard below can
    // consume the key: every one of those guards that does consume one still ends up editing the
    // document, and the spell pass reads the latch DURING that edit, so raising it afterwards would
    // be one pass too late. Keys that produce no text and delete none (arrows, modifiers, plain
    // shortcuts) deliberately do not raise it -- they edit nothing for the pass to run over.
    if (!event->text().isEmpty()
        || event->key()==Qt::Key_Backspace
        || event->key()==Qt::Key_Delete)
    {
        m_spellCheckTyping=true;
    }

    // Up-arrow in an EMPTY editor is a free gesture: there is no line above the caret to move to,
    // so the default handling is a visible no-op and claiming the key here costs nothing. Gated
    // strictly on emptiness -- with any text present, Up must keep moving the caret, and a host
    // acting on this signal would clobber an in-progress draft. Unmodified Up only: a modified Up
    // (Shift-select, Ctrl-scroll, ...) keeps its standard meaning.
    //
    // Tested as a MASK over the modifiers that actually change the gesture's meaning, never as
    // `modifiers()==Qt::NoModifier`: macOS sets NSEventModifierFlagNumericPad on the arrow keys,
    // which Qt surfaces as Qt::KeypadModifier, so an equality test silently never matches there.
    // (Same reason the Key_Return branch just below masks instead of comparing, and why every
    // other arrow-key handler in this library -- Calendar, Spinner, DateTimeInput -- ignores
    // modifiers altogether.)
    if (event->key() == Qt::Key_Up
        && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier))
        && document()->isEmpty())
    {
        emit editPreviousRequested();
        return;
    }

    // Tab/Shift+Tab mean one of THREE things, and a literal tab character is none of them.
    if ((event->key()==Qt::Key_Tab || event->key()==Qt::Key_Backtab)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        // Stage 6, checked FIRST: while an "@word" is in progress, Tab means "complete it" --
        // the classic autocomplete keyboard gesture -- rather than either meaning below. The
        // editor cannot resolve "eri" to anyone on its own (no user directory; see
        // AbstractMessageEditor::mentionRequested()'s own doc comment), so this is pure gesture
        // recognition: the key is consumed regardless of whether a host is even connected to
        // mentionCompletionRequested(), so a mention query never "eats" a literal tab character
        // or silently triggers an indent step instead. Direction (Tab vs Shift+Tab) is not
        // distinguished -- there is no "previous candidate" concept here, since the editor holds
        // no candidate list to step through. mentionQueryAtCursor() already excludes being
        // inside a table (see its own doc comment), so there is no real ambiguity with the
        // table-cell-navigation branch below even though this check runs before it.
        const auto query=mentionQueryAtCursor();
        if (query.isActive)
        {
            emit mentionCompletionRequested(query.prefix,query.position);
            return;
        }

        // INSIDE A TABLE they move between cells. Qt does NOT do this on its own: neither
        // QWidgetTextControl nor QTextEdit has any NextCell/PreviousCell handling, so Tab would
        // just insert a tab into the current cell and there would be no keyboard way across a
        // table at all -- only clicking, or walking the arrow keys through every character.
        //
        // OUTSIDE ONE they are an indent gesture, handed to MessageEditor as a signal because
        // what a step means depends on the editing mode -- see indentStepRequested(). The key is
        // consumed either way; a tab character never reaches the document, which is the point (a
        // leading tab makes markdown read the line as an indented code block, and a mid-line tab
        // is collapsed to one space by the time the message is rendered as HTML).
        const auto backwards=(event->key()==Qt::Key_Backtab)
            || (event->modifiers() & Qt::ShiftModifier);

        auto cursor=textCursor();
        if (cursor.currentTable()!=nullptr)
        {
            // Deliberately no wrap and no row-append past the last cell: movePosition() simply
            // returns false at either end, leaving the caret where it is. Appending a row on Tab
            // out of the last cell is a word-processor convention that would silently grow a
            // chat message's table, which is not obviously wanted here.
            if (cursor.movePosition(backwards ? QTextCursor::PreviousCell : QTextCursor::NextCell))
            {
                setTextCursor(cursor);
            }
            return;
        }

        emit indentStepRequested(backwards ? -1 : 1);
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (m_newLineOnEnter)
        {
            if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
            {
                emit returnPressed();
                return;
            }
        }
        else
        {
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            {
                emit returnPressed();
                return;
            }
        }
        event->setModifiers(event->modifiers() & ~(Qt::ControlModifier | Qt::ShiftModifier));
    }

    // --- Stage 6: mention atomicity -----------------------------------------------------------
    //
    // A mention is one user-visible token. Qt gives it no such status on its own; measured, all
    // five: a character typed anywhere in (runStart, runEnd] is absorbed INTO the anchor,
    // splitting the title and keeping the href; Backspace and Delete shrink the run one character
    // at a time, never crossing out of it; deleting a selection that only PARTLY covers the run
    // leaves the remainder as a smaller anchor still carrying the same href -- a live, clickable,
    // WRONG mention; a Return pressed right at/inside the run does not start a clean new block at
    // all -- the anchor format survives onto it and the mention's own text gains an embedded
    // '\r'; selecting the run whole and deleting it removes it cleanly, in one user-visible
    // action, with no residue. That last shape is the target every branch of the guard converges
    // on -- see applyMentionAtomicityGuard()'s own doc comment for the five branches.
    //
    // --- ":shortcode:" auto-replace, checked BEFORE mention atomicity -----------------------
    //
    // Ordering is free, not just safe: an emoji image is never an anchor run, so nothing
    // applyMentionAtomicityGuard() does (all five of its branches key off isMentionFormat()) can
    // ever apply to a shortcode replacement, and vice versa. Inert by construction whenever
    // isEmojiShortcodeAutoReplaceEnabled() is false (the default) -- see that property's own doc
    // comment on why it defaults off independently of the emoji BUTTON's own visibility.
    if (applyEmojiShortcodeGuard(event))
    {
        return;
    }

    // Inert by construction outside Wysiwyg: a Markdown/Plaintext document holds no anchors, so
    // every predicate the guard checks is false and the key falls through untouched.
    if (applyMentionAtomicityGuard(event))
    {
        return;
    }

    // Escape closes an in-progress mention query -- REPORTED, never CONSUMED: a host's own Escape
    // handling (closing a dialog, cancelling a reply bar) must keep working exactly as it did, and
    // a host that wants to eat Escape for its own popup installs its own filter for that.
    if (event->key()==Qt::Key_Escape && m_lastMentionQuery.isActive)
    {
        m_dismissedMentionPosition=m_lastMentionQuery.position;
        m_lastMentionQuery=MentionQuery{};
        emit mentionQueryClosed();
    }

    QTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::focusInEvent(QFocusEvent* event)
{
    QTextEdit::focusInEvent(event);
    emit activated();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::focusOutEvent(QFocusEvent* event)
{
    QTextEdit::focusOutEvent(event);

    // Nobody is typing in an unfocused composer, so the word under its caret is checked like any
    // other one: a misspelling left half-typed when the user clicked away has to be marked, not
    // hidden indefinitely. The latch goes back up on the next keystroke here (see keyPressEvent()).
    m_spellCheckTyping=false;
    updateTypedSpellWord();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::inputMethodEvent(QInputMethodEvent* event)
{
    // Same latch keyPressEvent() sets, for input that never reaches it: an IME commit, dictation,
    // or a QInputMethodEvent-based on-screen keyboard.
    if (!event->commitString().isEmpty() || !event->preeditString().isEmpty())
    {
        m_spellCheckTyping=true;
    }

    QTextEdit::inputMethodEvent(event);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::changeEvent(QEvent* event)
{
    QTextEdit::changeEvent(event);

    if (event->type()==QEvent::FontChange)
    {
        applyTabStopDistance();
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::paintEvent(QPaintEvent* event)
{
    // The text first: the squiggle goes ON TOP of it, which is the whole point -- the user's own
    // solid underline is drawn by Qt from the document's fontUnderline, untouched by the spell
    // pass (see SpellCheckUnderlineProperty), and ours lands below it.
    QTextEdit::paintEvent(event);

    if (!m_spellCheckEnabled || m_spellChecker.isNull())
    {
        return;
    }

    auto* doc=document();
    auto* docLayout=doc->documentLayout();
    if (docLayout==nullptr)
    {
        return;
    }

    const auto eventRect=event->rect();
    const QPointF scroll(horizontalScrollBar()->value(),verticalScrollBar()->value());

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing,true);

    const auto fallbackColor=palette().color(foregroundRole());

    // Start at the block under the top of the damaged strip rather than at the document's first
    // block: a composer holding a long paste must not be walked end to end on every caret blink.
    auto block=cursorForPosition(QPoint(0,eventRect.top())).block();
    for (; block.isValid(); block=block.next())
    {
        auto* layout=block.layout();
        if (layout==nullptr)
        {
            continue;
        }

        // Document coordinates to viewport coordinates -- the same conversion (and the same
        // reason) as ChatMessageTextBrowser::codeBlockViewportRect(): the document's own margin
        // is already inside blockBoundingRect()'s origin, and blockBoundingRect() moves the
        // block's bounding rect to layout->position(), so its topLeft IS the layout origin that
        // QTextLine::y()/cursorToX() are relative to.
        const auto blockRect=docLayout->blockBoundingRect(block).translated(-scroll);
        if (blockRect.top()>eventRect.bottom()+viewport()->height())
        {
            // Not a strict "past the bottom" break: inside a QTextTable the next block in
            // document order can sit HIGHER than this one (the next cell of the same row), so
            // stopping there would leave the rest of a table row unpainted. One viewport of
            // slack is more than any single row is ever tall.
            break;
        }
        if (!blockRect.intersects(QRectF(eventRect)))
        {
            continue;
        }

        const auto formats=layout->formats();
        if (formats.isEmpty())
        {
            continue;
        }

        for (const auto& range : formats)
        {
            if (!range.format.boolProperty(SpellCheckUnderlineProperty))
            {
                continue;
            }

            auto color=qvariant_cast<QColor>(range.format.property(SpellCheckUnderlineColorProperty));
            if (!color.isValid())
            {
                // What Qt's own underline used to inherit when no underlineColor was set: the
                // pen the text itself is drawn with. Keeps a host that ships no stylesheet
                // exactly where it was before this widget started painting the squiggle itself.
                color=fallbackColor;
            }

            drawSpellRange(painter,block,blockRect.topLeft(),range.start,range.length,color,
                m_spellCheckUnderlineWidth);
        }
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setBlockquoteColor(const QColor& color)
{
    m_blockquoteColor=color;

    // Repainting is driven from the SETTER rather than from a QEvent::StyleChange handler on
    // purpose: QSS re-applies a qproperty- declaration during polish, and pinning the repaint to
    // the moment the value actually arrives avoids having to reason about whether polish runs
    // before or after the event. setColor() is a no-op when the colour has not changed, so the
    // repolish that a theme switch triggers on every widget costs nothing here.
    if (m_highlighter!=nullptr)
    {
        m_highlighter->setBlockquoteColor(color);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setCodeBlockColor(const QColor& color)
{
    m_codeBlockColor=color;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setCodeBlockColor(color);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setLinkColor(const QColor& color)
{
    m_linkColor=color;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setLinkColor(color);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setLinkUnderline(bool enable)
{
    m_linkUnderline=enable;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setLinkUnderline(enable);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setMentionColor(const QColor& color)
{
    m_mentionColor=color;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setMentionColor(color);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setSpellCheckUnderlineColor(const QColor& color)
{
    m_spellCheckUnderlineColor=color;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setSpellCheckUnderlineColor(color);
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setSpellCheckUnderlineWidth(qreal width)
{
    if (qFuzzyCompare(m_spellCheckUnderlineWidth,width))
    {
        return;
    }

    m_spellCheckUnderlineWidth=width;

    // No rehighlight: nothing about WHICH ranges are marked changes, only how thick the squiggle
    // paintEvent() draws for them is -- that is read at paint time, never baked into a format.
    viewport()->update();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setSpellChecker(AbstractSpellChecker* checker)
{
    if (m_spellChecker==checker)
    {
        return;
    }

    if (!m_spellChecker.isNull())
    {
        disconnect(m_spellChecker,nullptr,this,nullptr);
    }
    m_spellChecker=checker;

    if (checker!=nullptr)
    {
        // The signal lives on the CHECKER, not on the highlighter: MessageEditorHighlighter
        // declares no signals and needs no moc pass, and giving it a Q_OBJECT purely to carry one
        // notification would add a moc pass to the whole editor for nothing. This widget is
        // already a QObject, so it does the connecting.
        connect(checker,&AbstractSpellChecker::dictionaryChanged,this,
            &EnhancedTextEdit::onSpellDictionaryChanged);
        // A checker the host destroys out from under an attached editor detaches cleanly rather
        // than leaving m_spellChecker dangling.
        connect(checker,&QObject::destroyed,this,
            [this]()
            {
                setSpellChecker(nullptr);
            }
        );
    }

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setSpellChecker(checker);
    }

    // Attaching or detaching a checker changes typedSpellWord()'s own answer -- same bookkeeping
    // reason as in setSpellCheckEnabled() just below.
    updateTypedSpellWord();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setSpellCheckEnabled(bool enable)
{
    m_spellCheckEnabled=enable;

    if (m_highlighter!=nullptr)
    {
        m_highlighter->setSpellCheckEnabled(enable);
    }

    // Toggling this changes typedSpellWord()'s own answer, so the record updateTypedSpellWord()
    // keeps of it has to follow -- otherwise the next caret move would compare against a word that
    // was never the one in force and re-highlight the wrong block (or none). The highlighter's
    // setter above has already re-run the pass itself, which is why this is not called before it.
    updateTypedSpellWord();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::onSpellDictionaryChanged()
{
    // Coalesces a burst of these (several dictionaries finishing within a few ms is several
    // signals) into ONE rehighlight(), which is O(document) and not worth paying more than once
    // per dictionary-load event.
    if (m_spellRehighlightTimer==nullptr)
    {
        m_spellRehighlightTimer=new QTimer(this);
        m_spellRehighlightTimer->setSingleShot(true);
        m_spellRehighlightTimer->setInterval(150);
        connect(m_spellRehighlightTimer,&QTimer::timeout,this,
            [this]()
            {
                if (m_highlighter!=nullptr)
                {
                    m_highlighter->clearSpellCache();
                }
            }
        );
    }
    m_spellRehighlightTimer->start();
}

//--------------------------------------------------------------------------

EnhancedTextEdit::SpellWord EnhancedTextEdit::typedSpellWord() const
{
    SpellWord result;

    if (!m_spellCheckTyping || !m_spellCheckEnabled || m_spellChecker.isNull())
    {
        return result;
    }

    const auto cursor=textCursor();

    // A selection is not typing: double-clicking a misspelled word -- which is exactly how its
    // suggestions are reached from the context menu, see MessageEditor::showContextMenu() -- must
    // leave its squiggle alone.
    if (cursor.hasSelection())
    {
        return result;
    }

    const auto block=cursor.block();
    const auto token=typedSpellToken(block,cursor.position()-block.position());
    if (token.length==0)
    {
        return result;
    }

    result.isValid=true;
    result.position=block.position()+token.start;
    result.length=token.length;
    result.text=block.text().mid(token.start,token.length);
    return result;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::updateTypedSpellWord()
{
    if (m_highlighter==nullptr)
    {
        return;
    }

    // QTextDocument::revision() moves with every edit, which is what tells "the caret moved because
    // the text changed" (Qt is rehighlighting the edited block anyway, with the live answer already
    // in hand) apart from "the caret moved on its own" (nothing will rehighlight anything unless
    // this does). Read on BOTH connected signals, since cursorPositionChanged() is emitted FIRST
    // for an edit -- QTextDocumentPrivate::finishEdit() emits it before contentsChanged().
    const auto revision=document()->revision();
    const auto documentChanged=(revision!=m_typedSpellWordRevision);
    m_typedSpellWordRevision=revision;

    const auto word=typedSpellWord();
    const auto position=word.isValid ? word.position : -1;
    const auto length=word.isValid ? word.length : 0;
    if (position==m_typedSpellWordPosition && length==m_typedSpellWordLength)
    {
        return;
    }

    auto previousPosition=m_typedSpellWordPosition;
    m_typedSpellWordPosition=position;
    m_typedSpellWordLength=length;

    if (documentChanged)
    {
        // The edit's own pass covered the caret's block with the live answer, so only a word left
        // behind in ANOTHER block (a programmatic edit elsewhere can move the caret out of one)
        // still needs its squiggle putting back.
        const auto caretBlock=textCursor().blockNumber();
        if (previousPosition>=0 && document()->findBlock(previousPosition).blockNumber()==caretBlock)
        {
            previousPosition=-1;
        }
        m_highlighter->rehighlightSpellWordBlocks(previousPosition,-1);
        return;
    }

    m_highlighter->rehighlightSpellWordBlocks(previousPosition,position);
}

//--------------------------------------------------------------------------

EnhancedTextEdit::SpellWord EnhancedTextEdit::spellWordAt(int documentPosition) const
{
    SpellWord result;

    auto* doc=document();
    const auto block=doc->findBlock(documentPosition);
    if (!block.isValid())
    {
        return result;
    }

    const auto blockStart=block.position();
    const auto offset=documentPosition-blockStart;
    const auto text=block.text();
    const auto excluded=nonProseRuns(block);

    for (const auto& token : spellTokens(text))
    {
        if (offset<token.start || offset>token.start+token.length)
        {
            continue;
        }
        if (spellRangeExcluded(token.start,token.length,excluded))
        {
            return result;
        }

        result.isValid=true;
        result.position=blockStart+token.start;
        result.length=token.length;
        result.text=text.mid(token.start,token.length);
        return result;
    }

    return result;
}

//--------------------------------------------------------------------------

EnhancedTextEdit::SpellWord EnhancedTextEdit::spellWordAtCursor() const
{
    return spellWordAt(textCursor().position());
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::selectSpellWord(QTextCursor& cursor, const SpellWord& word) const
{
    if (!word.isValid)
    {
        return false;
    }

    cursor.setPosition(word.position);
    cursor.setPosition(word.position+word.length,QTextCursor::KeepAnchor);
    return true;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::applyTabStopDistance()
{
    // Qt's default is a flat 80px that ignores the font entirely; at this widget's space width
    // that is more than twenty spaces per tab. Measured against the font in force instead, so a
    // tab lines up with what DefaultTabStopSpaces actually promises at any font size.
    setTabStopDistance(DefaultTabStopSpaces*QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
}

//--------------------------------------------------------------------------

EnhancedTextEdit::MentionQuery EnhancedTextEdit::mentionQueryAtCursor() const
{
    MentionQuery query;

    const auto cursor=textCursor();
    if (cursor.hasSelection())
    {
        // A selection means the user is not mid-typing a word.
        return query;
    }

    // --- gates, all four from the Stage 6 brief's own enumeration ("inside a code fence, inside
    // an existing link, inside a table cell") plus the anchor test that also covers a mention
    // already inserted --------------------------------------------------------------------------
    //
    // A fenced code block. block.userState() rather than only QTextFormat::BlockCodeFence, for
    // exactly the reason MessageEditor::currentFormatState() spells out: convertCodeBlocksToText()
    // strips that property, because a literal fence is ordinary text. The property test is kept
    // alongside as the same belt-and-braces case.
    const auto block=cursor.block();
    const auto blockFormat=cursor.blockFormat();
    if (block.userState()==MessageEditorHighlighter::InFence
        || blockFormat.hasProperty(QTextFormat::BlockCodeFence))
    {
        return query;
    }

    // Inside an existing anchor: an ordinary hyperlink's title, or a mention already inserted.
    // One test covers both -- offering a selector inside either is meaningless, and it is what
    // stops the ANCHOR form of a mention re-triggering detection from within itself. (The PLAIN
    // "@username" form deliberately does re-trigger: it is indistinguishable from a hand-typed
    // "@alice", and both are legitimately still an @-word the user may want to re-pick.)
    if (cursor.charFormat().isAnchor())
    {
        return query;
    }

    // Inside a table cell -- per the brief's own enumeration. Deliberately NOT applied to a
    // deliberate insert (see MessageEditor::canInsertMentionAtCursor()); this gate is about not
    // auto-popping a host's selector inside a compact grid.
    if (cursor.currentTable()!=nullptr)
    {
        return query;
    }

    // --- the walk back ------------------------------------------------------------------------
    const auto text=block.text();
    const auto offset=cursor.position()-block.position();

    // Back over the word characters. A word character is anything that is neither whitespace nor
    // '@' -- deliberately permissive, since a username's alphabet is the HOST's business, not
    // this widget's, and the host filters the prefix it is handed anyway.
    auto i=offset;
    while (i>0 && !text.at(i-1).isSpace() && text.at(i-1)!=QLatin1Char('@'))
    {
        --i;
    }
    if (i==0 || text.at(i-1)!=QLatin1Char('@'))
    {
        return query;
    }
    const auto at=i-1;

    // The '@' must START a word: whitespace before it, or the start of the block. An '@' in the
    // middle of a token ("a@b", an e-mail address) is not a mention gesture.
    if (at>0 && !text.at(at-1).isSpace())
    {
        return query;
    }

    const auto prefix=text.mid(at+1,offset-at-1);
    if (prefix.size()>MaxMentionQueryChars)
    {
        // A guard against a pathological paste handing a host's selector a giant filter string.
        return query;
    }

    query.isActive=true;
    query.position=block.position()+at;
    query.prefix=prefix;
    return query;
}

//--------------------------------------------------------------------------

EnhancedTextEdit::ShortcodeCandidate EnhancedTextEdit::shortcodeCandidateAtCursor() const
{
    ShortcodeCandidate candidate;

    const auto cursor=textCursor();
    if (cursor.hasSelection())
    {
        // A selection means the user is not mid-typing a token.
        return candidate;
    }

    // Same suppression insertEmoji()/normalizeImportedEmoji() already apply: a fixed-pitch
    // (inline code) run, a fenced code block, or inside an existing mention. Tables are
    // deliberately NOT suppressed -- insertEmoji() allows them there.
    if (cursor.charFormat().fontFixedPitch())
    {
        return candidate;
    }
    const auto block=cursor.block();
    const auto blockFormat=cursor.blockFormat();
    if (block.userState()==MessageEditorHighlighter::InFence
        || blockFormat.hasProperty(QTextFormat::BlockCodeFence))
    {
        return candidate;
    }
    if (isMentionFormat(cursor.charFormat()))
    {
        return candidate;
    }

    // A name character is exactly gen-emoji-pack.py's shortcode/alias charset -- see
    // reactioniconpack.hpp's ReactionIconInfo::shortcode doc comment. ASCII only, deliberately
    // narrower than mentionQueryAtCursor()'s own permissive "anything but whitespace/'@'" rule:
    // a username's alphabet is the HOST's business, but a shortcode's is fixed by the pack data.
    auto isNameChar=[](QChar c)
    {
        const auto u=c.unicode();
        return (u>=u'a' && u<=u'z') || (u>=u'A' && u<=u'Z') || (u>=u'0' && u<=u'9')
            || u==u'_' || u==u'+' || u==u'-';
    };

    const auto text=block.text();
    const auto offset=cursor.position()-block.position();

    // Back over the name characters, same walk-back shape as mentionQueryAtCursor()'s own --
    // including relying on the SAME post-hoc length check (below) rather than an in-loop bound,
    // since nothing a user TYPES reaches that length anyway (see MaxShortcodeChars's own
    // comment). An inline emoji IMAGE (U+FFFC) is itself outside this charset, so the walk stops
    // there with no special case -- which is what makes ":star::fire:" match twice in a row: the
    // second candidate's "character before the opening ':'" is the FIRST emoji's image, not a
    // colon.
    auto i=offset;
    while (i>0 && isNameChar(text.at(i-1)))
    {
        --i;
    }
    if (i==offset || i==0 || text.at(i-1)!=QLatin1Char(':'))
    {
        // No name characters at all, ran off the start of the block, or the character before the
        // run is not ':' -- no candidate.
        return candidate;
    }
    const auto colonPos=i-1;

    // The character before the OPENING ':' must be neither ':' nor a name character. One test
    // rejects both "foo::bar:"/"::" (":" immediately before) and "a:star:" (a name character
    // immediately before, which reads as "a:" followed by "star:", not as a shortcode "a:star").
    if (colonPos>0)
    {
        const auto before=text.at(colonPos-1);
        if (before==QLatin1Char(':') || isNameChar(before))
        {
            return candidate;
        }
    }

    const auto name=text.mid(i,offset-i);
    if (name.isEmpty())
    {
        // "::" must never trigger -- it is the C++ scope operator, and this editor is used in
        // developer chats.
        return candidate;
    }
    if (name.size()>MaxShortcodeChars)
    {
        return candidate;
    }

    candidate.isActive=true;
    candidate.position=block.position()+colonPos;
    candidate.name=name;
    return candidate;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::updateMentionQuery()
{
    auto query=mentionQueryAtCursor();

    // An '@' dismissed with Escape stays dismissed while the caret is still in that same word;
    // starting a NEW one (a different '@' position) clears the latch.
    if (query.isActive && query.position==m_dismissedMentionPosition)
    {
        query=MentionQuery{};
    }
    else
    {
        m_dismissedMentionPosition=-1;
    }

    if (query.isActive==m_lastMentionQuery.isActive
        && query.position==m_lastMentionQuery.position
        && query.prefix==m_lastMentionQuery.prefix)
    {
        return;
    }

    const auto wasActive=m_lastMentionQuery.isActive;
    m_lastMentionQuery=query;

    if (query.isActive)
    {
        emit mentionQueryChanged(query.prefix,query.position);
    }
    else if (wasActive)
    {
        emit mentionQueryClosed();
    }
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::selectMentionQueryAtCursor(QTextCursor& cursor) const
{
    const auto query=mentionQueryAtCursor();
    if (!query.isActive)
    {
        return false;
    }
    // The '@' is INCLUDED: an inserted mention replaces the whole gesture, not just its tail.
    cursor.setPosition(query.position);
    cursor.setPosition(query.position+1+query.prefix.size(),QTextCursor::KeepAnchor);
    return true;
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::snapSelectionToMentions(QTextCursor& cursor) const
{
    const auto start=cursor.selectionStart();
    const auto end=cursor.selectionEnd();
    auto newStart=start;
    auto newEnd=end;

    QTextCursor probe(document());

    // START side: the run containing the character AT `start`. If `start` is strictly inside it,
    // widen backwards; a selection that already begins at the run's first character covers it
    // whole.
    probe.setPosition(start);
    auto run=probe;
    if (selectAnchorRun(run,AnchorRunSide::After,isMentionFormat) && run.selectionStart()<start)
    {
        newStart=run.selectionStart();
    }

    // END side: the run containing the character BEFORE `end`, mirrored.
    probe.setPosition(end);
    run=probe;
    if (selectAnchorRun(run,AnchorRunSide::Before,isMentionFormat) && run.selectionEnd()>end)
    {
        newEnd=run.selectionEnd();
    }

    if (newStart==start && newEnd==end)
    {
        return false;
    }
    cursor.setPosition(newStart);
    cursor.setPosition(newEnd,QTextCursor::KeepAnchor);
    return true;
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::applyMentionAtomicityGuard(QKeyEvent* event)
{
    const bool isBackspace=(event->key()==Qt::Key_Backspace);
    const bool isDelete=(event->key()==Qt::Key_Delete);
    const bool isReturn=(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter);

    // "This key will put text into the document." Tested on the delivered text rather than a
    // key-code range: that is what actually reaches the document (an IME commit string included),
    // while leaving every navigation/shortcut key alone. Control/Meta chords are excluded, so
    // Ctrl+V is NOT caught here -- a paste reaches insertFromMimeData(), a separate path, and one
    // the '@'-detection observers (textChanged/cursorPositionChanged) already pick up. Return's
    // own event->text() ("\r", 0x0D) fails this test, which is exactly why it needs its own
    // branch below rather than falling into the "inserts" one.
    const auto typed=event->text();
    const bool inserts=!typed.isEmpty()
        && typed.at(0).unicode()>=0x20
        && !(event->modifiers() & (Qt::ControlModifier|Qt::MetaModifier));

    if (!isBackspace && !isDelete && !isReturn && !inserts)
    {
        return false;
    }

    auto cursor=textCursor();

    // --- BRANCH 1: a selection that only PARTLY covers a mention ------------------------------
    // Widen it to whole runs at both ends and hand the key straight to Qt: the edit it is about
    // to do now lands on the measured CLEAN shape (whole run, one edit, no residue) instead of
    // the measured mangled one (a selection deleted across only PART of a run leaves the
    // remainder as a smaller anchor still carrying the same href). Covers Backspace, Delete and
    // typing-over-a-selection in one place. Never consumes the key.
    if (cursor.hasSelection())
    {
        if (snapSelectionToMentions(cursor))
        {
            setTextCursor(cursor);
        }
        return false;
    }

    // --- BRANCH 2: Backspace at/inside a mention -> delete the WHOLE run ----------------------
    if (isBackspace)
    {
        auto run=cursor;
        // position() > run.selectionStart() guards the document-position-0 edge case: charFormat()
        // there falls back to the FOLLOWING character (measured), so without this check a
        // Backspace at the very START of a mention that starts the document -- an ordinary no-op
        // in Qt -- would delete the whole mention instead.
        if (selectAnchorRun(run,AnchorRunSide::Before,isMentionFormat)
            && cursor.position()>run.selectionStart())
        {
            run.beginEditBlock();
            run.removeSelectedText();
            run.endEditBlock();
            setTextCursor(run);
            return true;
        }
        return false;
    }

    // --- BRANCH 3: Delete (forward) at/inside a mention -- the same, looking the other way ----
    if (isDelete)
    {
        auto run=cursor;
        if (selectAnchorRun(run,AnchorRunSide::After,isMentionFormat)
            && cursor.position()<run.selectionEnd())
        {
            run.beginEditBlock();
            run.removeSelectedText();
            run.endEditBlock();
            setTextCursor(run);
            return true;
        }
        return false;
    }

    // --- BRANCH 3.5: Return/Enter at/inside a mention -----------------------------------------
    // Measured against a real EnhancedTextEdit (QApplication::sendEvent(Key_Return) right after
    // insertMention()): the new block does NOT start clean -- the mention's anchor format
    // survives onto it and the mention's own fragment gains an embedded '\r' instead of a real
    // block split, so toMarkdown() serializes "[Alice\n](whitem-mention:usr1)X" for what should
    // have been two separate lines. A bare QTextCursor::insertBlock() call in ISOLATION (no
    // QTextEdit/QWidgetTextControl involved) does NOT reproduce this on the same document
    // content -- so whatever produces it lives specifically inside QWidgetTextControl's own
    // Return handling, not in insertBlock() itself, and chasing the exact undocumented mechanism
    // is not worth it.
    //
    // Fixed by sidestepping the question entirely: insert the block OURSELVES, with an explicitly
    // cleared format, and CONSUME the key so QTextEdit::keyPressEvent()'s own Return handling
    // never runs for this case at all -- the same "build it right rather than let Qt guess" shape
    // Branch 4 uses for typing, just applied to block insertion instead of character insertion.
    // (Separately verified with a standalone probe: cursor.insertBlock(blockFormat,
    // clearedFormat) alone does produce the clean two-block shape this branch relies on.)
    //
    // A caret INSIDE the run (not just at its end) is handled by the same code: `run.
    // selectionEnd()` is always the mention's own end regardless of where inside it the caret
    // was, so the whole mention stays intact on the FIRST line and the new block starts empty
    // right after it -- consistent with Branch 4 never splitting a mention's title either.
    if (isReturn)
    {
        auto run=cursor;
        bool atMention=selectAnchorRun(run,AnchorRunSide::Before,isMentionFormat);
        if (!atMention && cursor.position()==0)
        {
            atMention=selectAnchorRun(run,AnchorRunSide::After,isMentionFormat);
        }
        if (!atMention)
        {
            return false;
        }

        cursor.setPosition(run.selectionEnd());
        QTextCharFormat continuation=cursor.charFormat();
        continuation.clearProperty(QTextFormat::IsAnchor);
        continuation.clearProperty(QTextFormat::AnchorHref);
        continuation.clearProperty(QTextFormat::AnchorName);

        cursor.insertBlock(cursor.blockFormat(),continuation);
        setTextCursor(cursor);
        setCurrentCharFormat(continuation);
        return true;
    }

    // --- BRANCH 4: typing at/inside a mention ---------------------------------------------------
    // Resolution is NON-DESTRUCTIVE by design (confirmed choice): move the caret past the run's
    // end and strip the anchor properties off the format the insert would otherwise inherit, then
    // let the key do exactly what it always did. Nothing the user typed is lost and nothing
    // already written is replaced.
    //
    // insertMention()/insertMentionText() already apply this same strip, but only at the INSTANT
    // of insertion (Stage 5b's insertLink() established the pattern); this re-applies it for a
    // caret the user later navigated back to (click or arrow key), which is exactly the case that
    // still reproduced the absorption bug.
    auto run=cursor;
    bool atMention=selectAnchorRun(run,AnchorRunSide::Before,isMentionFormat);
    if (!atMention && cursor.position()==0)
    {
        // Document position 0: charFormat() looks FORWARD there (measured), so a mention starting
        // the document is "hot" on its leading edge too.
        atMention=selectAnchorRun(run,AnchorRunSide::After,isMentionFormat);
    }
    if (!atMention)
    {
        return false;
    }

    cursor.setPosition(run.selectionEnd());
    QTextCharFormat continuation=cursor.charFormat();
    continuation.clearProperty(QTextFormat::IsAnchor);
    continuation.clearProperty(QTextFormat::AnchorHref);
    continuation.clearProperty(QTextFormat::AnchorName);
    setTextCursor(cursor);
    // THIS is the load-bearing call: QTextCursor::setCharFormat() on a COLLAPSED cursor is
    // documented to do nothing, and the widget tracks its own insertion format separately.
    setCurrentCharFormat(continuation);

    return false;
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setEmojiShortcodeAutoReplaceEnabled(bool enable)
{
    m_emojiShortcodeAutoReplaceEnabled=enable;
    if (!enable)
    {
        // Turning the feature off strands nothing: without this, a replacement armed while it
        // was still on would keep reverting for one more Backspace after being switched off,
        // which is a stranger surprise than simply losing that one revert.
        m_shortcodeReplacement=EmojiShortcodeReplacement{};
        m_revertedShortcodePosition=-1;
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::armEmojiShortcodeRevert(int position, int length, const QString& literal,
                                               const QTextCharFormat& format)
{
    m_shortcodeReplacement.armed=true;
    m_shortcodeReplacement.position=position;
    m_shortcodeReplacement.length=length;
    m_shortcodeReplacement.literal=literal;
    m_shortcodeReplacement.format=format;
    m_shortcodeReplacement.documentRevision=document()->revision();
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::applyEmojiShortcodeGuard(QKeyEvent* event)
{
    if (!m_emojiShortcodeAutoReplaceEnabled)
    {
        return false;
    }

    const bool isBackspace=(event->key()==Qt::Key_Backspace);
    const bool isDelete=(event->key()==Qt::Key_Delete);
    // Same event->text() test applyMentionAtomicityGuard() uses to classify "inserts" -- here
    // narrowed to the one character that can ever complete a candidate. Excludes a Ctrl/Meta
    // chord (so Ctrl+Shift+; or similar never misfires) the same way that guard's own test does.
    const bool isColon=event->text()==QLatin1String(":")
        && !(event->modifiers() & (Qt::ControlModifier|Qt::MetaModifier));

    if (!isBackspace && !isDelete && !isColon)
    {
        return false;
    }
    if (textCursor().hasSelection())
    {
        // Typing ':' over a selection, or Backspace/Delete-ing one, is an ordinary edit -- not a
        // revert (there is nothing armed to revert if a selection exists) and not a completion
        // (the candidate scan requires a collapsed cursor). Falls through untouched.
        return false;
    }

    const auto pos=textCursor().position();

    // --- REVERT: Backspace/Delete right at the edge of an armed replacement -------------------
    if ((isBackspace || isDelete) && m_shortcodeReplacement.armed
        && document()->revision()==m_shortcodeReplacement.documentRevision)
    {
        const auto start=m_shortcodeReplacement.position;
        const auto end=start+m_shortcodeReplacement.length;
        // Backspace reverts from AFTER the replacement (the classic autocorrect-undo gesture);
        // Delete reverts from BEFORE it -- exact mirror of applyMentionAtomicityGuard()'s own
        // Branch 2/Branch 3 pair. At the OTHER edge, Backspace/Delete deletes forward/backward
        // AWAY from the replacement, which must not revert it.
        const auto atRevertEdge=(isBackspace && pos==end) || (isDelete && pos==start);
        if (atRevertEdge)
        {
            auto cursor=textCursor();
            cursor.beginEditBlock();
            cursor.setPosition(start);
            cursor.setPosition(end,QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(m_shortcodeReplacement.literal,m_shortcodeReplacement.format);
            cursor.endEditBlock();
            setTextCursor(cursor);

            // Latch this word so an immediate retype of the same closing ':' does not silently
            // re-expand what the user just rejected -- see m_revertedShortcodePosition's own
            // doc comment.
            m_revertedShortcodePosition=start;
            m_shortcodeReplacement=EmojiShortcodeReplacement{};
            return true;
        }
        // Backspace/Delete near, but not exactly at, the revert edge falls through to the
        // "still armed" case below, same as any other Backspace/Delete elsewhere: it disarms
        // implicitly the next time document()->revision() no longer matches (this key, once it
        // reaches QTextEdit, is itself an edit that will bump it).
    }

    // A Backspace/Delete that reaches here is not reverting anything -- but if the caret is
    // clearly nowhere near the LATCHED (already-rejected) word, that latch is stale and would
    // otherwise linger until the exact same position happened to matter again. This is an
    // approximation of "the caret left that word" (applyMentionAtomicityGuard()'s equivalent
    // latch is re-evaluated on every edit via updateMentionQuery()'s continuous observer; this
    // guard, deliberately, has no such observer -- see this method's own doc comment).
    if ((isBackspace || isDelete) && m_revertedShortcodePosition>=0)
    {
        const auto lo=m_revertedShortcodePosition;
        const auto hi=m_revertedShortcodePosition+MaxShortcodeChars+2;
        if (pos<lo || pos>hi)
        {
            m_revertedShortcodePosition=-1;
        }
    }

    if (isBackspace || isDelete)
    {
        // Neither branch above consumed the key -- an ordinary Backspace/Delete, unrelated to
        // this guard.
        return false;
    }

    // --- FORWARD: does this ':' complete a candidate? -----------------------------------------
    const auto candidate=shortcodeCandidateAtCursor();
    if (!candidate.isActive)
    {
        return false;
    }

    if (candidate.position==m_revertedShortcodePosition)
    {
        // Latched: this exact expansion was just rejected with Backspace/Delete and the caret
        // never left the word -- type the ':' literally rather than silently re-expanding it.
        return false;
    }
    // A genuinely different word than whatever the latch remembered (or no latch at all) -- the
    // old position is no longer relevant to anything.
    m_revertedShortcodePosition=-1;

    m_shortcodeReplacement=EmojiShortcodeReplacement{};
    const auto length=pos-candidate.position;
    Q_EMIT emojiShortcodeTyped(candidate.name,candidate.position,length);
    // Consumed only if the handler called armEmojiShortcodeRevert() synchronously in response --
    // an unconnected signal, or a lookup miss, leaves m_shortcodeReplacement exactly as reset
    // just above, so the ':' falls through and types literally, exactly as it did before this
    // feature existed.
    return m_shortcodeReplacement.armed;
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::canInsertFromMimeData(const QMimeData* source) const
{
    // Deliberately the BROAD mimeDataHasAttachments() test, not isAttachmentPaste(): this is the
    // predicate Qt's drag-and-drop machinery consults, and refusing every image/file-bearing
    // payload here is what lets a DROP fall through to an ancestor FileDropOverlay. Paste does
    // not come through here at all (QWidgetTextControl::paste() calls insertFromMimeData()
    // directly, no canPaste() gate -- see pasteFromClipboard()), so the finer paste-side
    // classification below cannot change how a drop is routed.
    if (mimeDataHasAttachments(source))
    {
        return false;
    }

    return QTextEdit::canInsertFromMimeData(source);
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::isAttachmentPaste(const QMimeData* source) const
{
    if (!mimeDataHasAttachments(source))
    {
        return false;
    }

    // A file payload is unambiguous -- those are attachments however much text rides along (a
    // file dragged from Finder carries its own path as text/plain).
    if (!mimeDataLocalFilePaths(source).isEmpty())
    {
        return true;
    }

    // Image bits PLUS text that actually renders is a DOCUMENT SELECTION with a picture preview
    // attached, not a picture. Measured on macOS: Numbers and Pages both put text/html (the real
    // table), text/plain (tab-separated) and an image rendition on the pasteboard at once -- and
    // Qt reports hasImage()==true for that rendition even though its data is ZERO BYTES. Judging
    // on the image alone therefore turned every copied spreadsheet table into an attachment, with
    // nothing inserted. See mimeDataHasRenderableText() for how the two are told apart, and why
    // an image-only payload (a screenshot, a browser's "copy image") still lands here.
    return !mimeDataHasRenderableText(source);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::insertFromMimeData(const QMimeData* source)
{
    if (isAttachmentPaste(source))
    {
        emit attachmentsPasted(source);
        return;
    }

    // task-message-text-length-limits.md: reject the WHOLE insert rather than truncating it --
    // this is the single funnel every paste/drop path (Ctrl+V, context-menu Paste, middle-click
    // paste, a text drop) reaches, per this override's own class-level doc comment, so one check
    // here covers all of them. Typing is deliberately NOT gated the same way: a keystroke-by-
    // keystroke cap would need to fight IME composition and would need its own throttled error
    // presentation to avoid a toast per character -- out of scope, and unnecessary in practice
    // (typing to 100k characters by hand is not a realistic path here the way a paste is).
    //
    // source->text() is Qt's own best plain-text rendering of the payload (it degrades HTML/rich
    // content the same way QTextEdit::insertFromMimeData() below would), so the length check and
    // the actual insert always agree on how much text is about to land. QString::length() counts
    // UTF-16 code units, not Unicode code points -- an approximation shared with every other
    // length here (document()->characterCount(), QTextCursor::selectedText().length()); this is a
    // client-side convenience gate, not the authoritative limit (whitemclient's own UTF-8
    // codepoint count, enforced server-side of this widget, is what actually decides).
    if (maxLength()>0)
    {
        const auto incomingLength=source->text().length();
        // characterCount() counts the implicit trailing paragraph separator as one character;
        // subtracted here so an empty document reads as length 0, not 1.
        const auto currentLength=document()->characterCount()-1;
        const auto selectedLength=textCursor().selectedText().length();
        const auto resultLength=currentLength-selectedLength+incomingLength;
        if (resultLength>maxLength())
        {
            emit insertRejected(resultLength,maxLength());
            return;
        }
    }

    // Stage 5b: normalize a rich-text/table paste so it satisfies the same invariants typed
    // WYSIWYG content already does -- no baked colour/font (theme-rot, measured: an external
    // #1f2937/Calibri/14pt survives straight through otherwise), a visible table border (Stage
    // 4's own fix, measured invisible at border=0 otherwise), and a literal "```" fence rather
    // than a property-based code block (so restoreCodeFences() keeps seeing only the one form it
    // knows how to export).
    //
    // Gated on acceptRichText() rather than an explicit mode check: MessageEditor already turns
    // it off in Markdown/Plaintext mode (updateMessageEditingMode()), and Qt's own
    // QTextEdit::insertFromMimeData() degrades a rich payload to plain text there on its own --
    // so this block is naturally unreachable outside Wysiwyg, with no mode plumbing needed here.
    auto cursor=textCursor();
    cursor.beginEditBlock();

    const auto insertStart=cursor.selectionStart();
    QTextEdit::insertFromMimeData(source);
    const auto insertEnd=textCursor().position();

    // The insert's own edit block is CLOSED before anything below looks at the result, and that
    // is load-bearing rather than tidiness. Qt rebuilds the frame hierarchy only in
    // QTextDocumentPrivate::finishEdit(), which returns immediately while an edit block is open --
    // so a table created during this block is not in document()->rootFrame()'s children yet, and
    // normalizeImportedTables() below walks straight past it.
    //
    // Measured, pasting the same table three ways with the block still open: into an empty
    // document the walk found 0 tables, into a document holding only text 0, and into one that
    // already contained a table 2 -- an existing table leaves the hierarchy populated, which is
    // exactly why this looked like it worked. The reported symptom was precisely that shape:
    // "insert a table with the toolbar first and the paste is cleaned; paste into a fresh editor
    // and the cells keep their original fills". After endEditBlock() all three find every table.
    cursor.endEditBlock();

    if (acceptRichText() && insertEnd>insertStart)
    {
        // Rejoin the paste's own undo action rather than opening a new one: everything below is
        // repair work on what was just inserted, so one Ctrl+Z must take the paste and its
        // normalization away together. Verified -- one undo restores the pre-paste document
        // exactly, and redo brings back the NORMALIZED content, in all three shapes above.
        cursor.joinPreviousEditBlock();

        stripBakedRichTextFormatting(document(),insertStart,insertEnd);
        normalizeImportedTables(document(),insertStart,insertEnd,false);
        // Whole-document, not ranged -- but idempotent on content that already satisfies the
        // literal-fence invariant (nothing else in this editor ever creates a property-based
        // code block), so re-running it here only ever touches what was just pasted.
        //
        // suppressUndo=false: undo is NOT disabled around a paste, because doing that clears the
        // whole undo stack (measured) -- see the function's own doc comment. Everything here is
        // already inside this function's edit block, so it groups into the paste's single
        // undoable action instead.
        convertCodeBlocksToText(document(),false);
        // stripBakedRichTextFormatting() above already cleared the foreground off every pasted
        // fragment, anchors included -- this also drops an underline a pasted link carried, so a
        // pasted link and a typed one are painted by the same linkColor/linkUnderline rule.
        stripImportedAnchorStyle(document(),false);
        // Without this a pasted emoji CHARACTER stays a character and renders in the system font,
        // right beside a gallery-inserted one rendered as pack art -- the same emoji, two
        // different pictures, in one composer. Every other route into a WYSIWYG document
        // (loadText(), the Markdown->Wysiwyg switch) already normalizes; the paste path was the
        // one that did not.
        //
        // Whole-document and suppressUndo=false for exactly the reasons spelled out for
        // convertCodeBlocksToText() above: it is idempotent on content that is already normalized
        // (an emoji image re-registers to the same pixmap under the same key), and disabling undo
        // around a paste would clear the whole stack instead of joining this block.
        normalizeImportedEmoji(document(),font(),devicePixelRatioF(),false);
        // Inside the rejoined block, so a handler's own edits (normalizeBlockquoteIndent()) join
        // the paste's single undoable action too.
        emit pastedRichText();

        cursor.endEditBlock();
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::pasteFromClipboard()
{
    QTextEdit::paste();
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::canPasteFromClipboard() const
{
    if (isReadOnly())
    {
        return false;
    }

    const auto* mimeData=QApplication::clipboard()->mimeData();
    return mimeDataHasAttachments(mimeData) || canPaste();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::clearUndoable()
{
    auto cursor=textCursor();
    cursor.select(QTextCursor::Document);
    cursor.removeSelectedText();
}

/********************************MessageEditor*********************************/

//--------------------------------------------------------------------------

class MessageEditor_p
{
    public:

        QBoxLayout* layout;

        //! leadingFrame | text edit | trailingFrame. ALWAYS horizontal -- the arrangement switch
        //! never touches this row, only the direction of the two frames' own layouts.
        QHBoxLayout* editorRow;

        //! Host widgets added before the text, plus the expand button as their last member. Its
        //! layout flips between LeftToRight and BottomToTop; nothing is ever moved between
        //! layouts.
        QFrame* leadingFrame;
        QBoxLayout* leadingLayout;

        //! Host widgets added after the text. Same treatment as leadingFrame.
        QFrame* trailingFrame;
        QBoxLayout* trailingLayout;

        MessageEditorToolbar* toolbar;
        EnhancedTextEdit* editor;
        IconTextButton* expandButton;

        //! First member of the trailing group, mirroring expandButton's place in the leading one.
        IconTextButton* emojiButton;

        //! The floating emoji picker THIS editor currently has -- built lazily on first use and
        //! then KEPT (closed, not destroyed) -- reopening is the common case, and rebuilding the
        //! gallery would re-rasterize the whole pack every time. It is actually a shared,
        //! per-WINDOW object (EmojiGallerySharedState, in messageeditor.cpp): current -- safe to
        //! act on as "mine" -- exactly while this editor is that shared state's owner, which
        //! ensureEmojiGallery() (via claimEmojiGallery()) guarantees on return. It is set once by
        //! ensureEmojiGallery() and, deliberately, never cleared just because ownership later
        //! moves to a different composer in the same window (see claimEmojiGallery()) -- every
        //! call site that matters reads it before emojiDialogOpen would even be false yet (see
        //! openEmojiGallery()), so gating on that flag instead would be both unnecessary and, in
        //! that specific ordering, wrong.
        QPointer<FloatingEmojiGalleryDialog> emojiDialog;

        //! Authoritative "is the picker open" -- see MessageEditor::isEmojiGalleryOpen().
        bool emojiDialogOpen=false;

        //! Whether the picker was opened (or promoted) by a CLICK rather than by hovering. ONLY a
        //! click ever sets this -- picking an emoji deliberately does not, see
        //! ensureEmojiGallery()'s emojiPicked handler. Only a pinned one checks the emoji button,
        //! and only an unpinned one closes itself when the pointer wanders off -- see
        //! MessageEditor::isEmojiGalleryPinned().
        //!
        //! Survives a CLOSE that was not itself a user dismissal (see emojiCloseProgrammatic
        //! below): it is the REMEMBERED pin, seeded by a host through setEmojiGalleryPinned() and
        //! meant to be app-wide, not merely "is the gallery on screen right now pinned" -- that
        //! narrower question is isEmojiGalleryPinned(), which is emojiDialogOpen&&emojiDialogPinned.
        bool emojiDialogPinned=false;

        //! Set for the duration of a close the USER did not ask for (the editor being hidden, a
        //! switch to MessageEditingMode::Plaintext, the emoji button being hidden, a host's own
        //! setEmojiGalleryPinned(false)) -- see closeEmojiGalleryInternal(). Consumed and reset by
        //! the FloatingDialogFrame::closed handler in ensureEmojiGallery(), which is where
        //! emojiDialogPinned is actually cleared -- close() itself is asynchronous, behind the
        //! frame's fade, so nothing can be cleared at the call site.
        bool emojiCloseProgrammatic=false;

        //! Recently-used emoji, most recent first, bare icon ids -- see
        //! AbstractMessageEditor::setEmojiRecentIds(). Empty leaves the picker's row on the pack's
        //! own basics, which is what it showed before this existed.
        QStringList emojiRecentIds;

        //! Armed by a pointer entering the emoji button, disarmed by it leaving or by a click.
        QTimer* emojiHoverOpenTimer=nullptr;

        //! Runs only while an UNPINNED gallery is open; closes it once the pointer has been away
        //! from both it and the button for EmojiHoverCloseDelayMs.
        QTimer* emojiHoverCloseTimer=nullptr;

        //! Consecutive emojiHoverCloseTimer ticks with the pointer over neither.
        int emojiAwayTicks=0;

        //! The microphone button, right after the emoji button in the trailing group.
        IconTextButton* micButton=nullptr;

        //! The recorder popup THIS editor has: built on the first press and then KEPT (closed, not
        //! destroyed). Not shared between the editors of a window as the emoji gallery is -- what
        //! a recording is belongs to one composer.
        QPointer<FloatingVoiceRecorderDialog> voiceDialog;

        //! Authoritative "is the recorder up". Stays true through the popup's fade-out, until the
        //! frame's closed() has run -- see onVoiceRecorderClosed().
        bool voiceOpen=false;

        //! A recording that does not belong to THIS editor is in progress and a host has claimed
        //! this composer for it -- see AbstractMessageEditor::setVoiceRecordingHeldByHost(). Never
        //! true at the same time voiceOpen is true for a reason this editor caused itself (this
        //! editor's own press sets voiceOpen, not this).
        bool voiceHeldByHost=false;

        //! The mouse button is down on the mic button: the press opened the popup and the release
        //! has not come yet.
        bool micHeld=false;

        //! Whether the text area was enabled when the popup came up, which disables it.
        bool editorWasEnabled=true;

        //! The icon that follows the pointer while the mic button is held.
        QPointer<QLabel> micDragProxy;

        //! The placeholder the HOST asked for, which is not always the one the text edit currently
        //! carries -- see MessageEditor::updatePlaceHolderText(), which suppresses it while the
        //! empty block has block formatting of its own to show.
        QString placeHolderText;

        QPointer<DropdownMenu> contextMenu;

        //! The mode updateMessageEditingMode() last actually applied to the document -- distinct
        //! from AbstractMessageEditor::messageEditingMode() only for the instant inside
        //! updateMessageEditingMode() itself, where it is still the OLD mode while the base
        //! class's own member already holds the new one. Needed because the round-trip has to
        //! know which mode the document is CURRENTLY in, not which mode it is becoming.
        MessageEditingMode appliedMode=MessageEditingMode::Wysiwyg;

        //! See MessageEditor::setParagraphIndentSpaces().
        int paragraphIndentSpaces=MessageEditor::DefaultParagraphIndentSpaces;

        //! See MessageEditor::setBlockquoteIndent().
        qreal blockquoteIndent=MessageEditor::DefaultBlockquoteIndent;

        //! task-spellcheck.md. The misspelled word the CURRENTLY OPEN context menu was built
        //! for, captured at build time from the MOUSE position rather than the caret -- see
        //! MessageEditor::showContextMenu(). Reset on every menu open.
        EnhancedTextEdit::SpellWord spellContextWord;
        QStringList spellSuggestions;
};

//--------------------------------------------------------------------------

//! The emoji gallery dialog and its current owner, shared by every MessageEditor in one
//! top-level WINDOW (not merely one composer) -- so a chat switch within that window hands the
//! same dialog off rather than closing one and opening another, and anywhere the user has
//! dragged it stays exactly where they left it. Stored as a dynamic property on the window
//! itself (see emojiGallerySharedState()), with `this` parented to that same window -- so both
//! this object and the dialog it points at are destroyed exactly when the window is, with no
//! separate cleanup needed anywhere. Forward-declared in messageeditor.hpp, same as
//! MessageEditor_p, so MessageEditor's private claimEmojiGallery()/buildEmojiGalleryDialog()/
//! releaseEmojiGallery() can take it by pointer.
class EmojiGallerySharedState : public QObject
{
    public:

        using QObject::QObject;

        QPointer<FloatingEmojiGalleryDialog> dialog;

        //! Which composer currently receives emojiPicked()/setEmojiRecentIds() promotion and owns
        //! the "is it open" bookkeeping. Null between the moment a close finishes and the next
        //! claim (there is always at most one owner, never zero once something is open).
        QPointer<MessageEditor> owner;

        //! The pack currently handed to `dialog`, and the editing mode it was built for -- a
        //! property of the DIALOG, not of whichever editor happens to own it right now: which
        //! pack is correct depends only on the mode, the same pack serves every composer in this
        //! window, and MessageEditor::emojiPackForCurrentMode() allocates a fresh filtered view on
        //! every call. Living here (rather than per-editor, as before the gallery was shared)
        //! is what lets a claim between two composers in the SAME mode skip setPack() entirely --
        //! keeping it per-editor instead made every ordinary chat switch rebuild the whole grid
        //! for nothing, even though nothing about the pack had actually changed.
        std::shared_ptr<AbstractReactionIconPack> pack;
        MessageEditingMode packMode=MessageEditingMode::Wysiwyg;
        bool packValid=false;
};

//--------------------------------------------------------------------------

MessageEditor::MessageEditor(QWidget* parent)
    : AbstractMessageEditor(parent),
      pimpl(std::make_unique<MessageEditor_p>())
{
    pimpl->layout=Layout::vertical(this);

    // --- formatting toolbar: built hidden, so a host that never calls setExpanded(true) (every
    // existing host, until it opts in) gets exactly today's bare layout -- Layout::vertical()
    // resets contents margins/spacing to 0, so a hidden widget in this layout costs zero pixels.
    pimpl->toolbar=new MessageEditorToolbar(this);
    pimpl->toolbar->setVisible(false);
    pimpl->layout->addWidget(pimpl->toolbar);

    // --- editor row: the expand button sits BESIDE the text edit, on its left, not on a row of
    // its own below it -- an own row would push the text edit up by the button's full height for
    // every host that opts in, and the brief asks for a bottom-LEFT corner button, i.e. one
    // occupying the corner of the editor itself. Nested layout rather than a wrapper QFrame:
    // nothing needs to style or address the row as a widget, and a QFrame would add another
    // paintable, QSS-selectable box between the editor and its parent for no gain.
    pimpl->editorRow=new QHBoxLayout();
    Layout::clear(pimpl->editorRow);
    pimpl->layout->addLayout(pimpl->editorRow);

    // --- leading/trailing frames. Each owns its own QBoxLayout, and that layout's DIRECTION is
    // the only thing the stacked arrangement changes -- the frames themselves never leave this
    // row, and no widget is ever moved between layouts.
    //
    // Both layouts keep a stretch as their last item. In LeftToRight it costs nothing (the frame
    // is sized to its content by editorRow's stretch-0 slot), while in BottomToTop it lands at
    // the TOP and is what packs the buttons down against the text area's bottom edge.
    auto makeSideFrame=[this](const QString& objectName, QFrame*& frame, QBoxLayout*& frameLayout)
    {
        frame=new QFrame(this);
        frame->setObjectName(objectName);
        frameLayout=Layout::horizontal(frame);
        frameLayout->addStretch(1);
        pimpl->editorRow->addWidget(frame,0,Qt::AlignBottom);
    };
    makeSideFrame(QStringLiteral("leadingWidgets"),pimpl->leadingFrame,pimpl->leadingLayout);

    // --- expand button: built hidden, see AbstractMessageEditor::expandButtonVisible's own doc
    // comment. Bottom-aligned so it stays in the editor's bottom-left corner as the text edit
    // grows upward with its content rather than drifting to the vertical centre. Carries the
    // "typography" glyph -- it reveals the FORMATTING toolbar, which is what the icon should say;
    // the mode button inside that toolbar is the one wearing a per-mode glyph, so there is no
    // duplication between the two.
    pimpl->expandButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("MessageEditor::expand"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->expandButton->setObjectName("expandButton");
    pimpl->expandButton->setText(QString());
    pimpl->expandButton->setCursor(Qt::PointingHandCursor);
    pimpl->expandButton->setFocusPolicy(Qt::NoFocus);
    pimpl->expandButton->setToolTip(tr("Show formatting toolbar"));
    pimpl->expandButton->setCheckable(true);
    pimpl->expandButton->setVisible(false);
    // Last member of the leading group, so it sits nearest the text area in the row and, once
    // the group turns into a column, ends up at its top (BottomToTop) -- see applyArrangement().
    pimpl->leadingLayout->insertWidget(pimpl->leadingLayout->count()-1,pimpl->expandButton);

    pimpl->editor=new EnhancedTextEdit(this);
    pimpl->editor->setAutoResizingEnabled(true);
    pimpl->editorRow->addWidget(pimpl->editor,1);

    makeSideFrame(QStringLiteral("trailingWidgets"),pimpl->trailingFrame,pimpl->trailingLayout);

    // --- emoji button: the trailing group's counterpart of the expand button, built hidden for
    // the same reason (see AbstractMessageEditor::emojiButtonVisible).
    //
    // Index 0, so it is the group's FIRST member: nearest the text area in the row, and -- because
    // the trailing group stacks TopToBottom (see applyArrangement()) -- ABOVE the host's own
    // buttons in the column. That is what leaves Send, added after it, in the bottom corner in
    // both arrangements.
    pimpl->emojiButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("MessageEditor::emoji"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->emojiButton->setObjectName("emojiButton");
    pimpl->emojiButton->setText(QString());
    pimpl->emojiButton->setCursor(Qt::PointingHandCursor);
    // Mandatory, not cosmetic: the picker inserts AT THE CARET and replaces the selection, so a
    // click that moved focus out of the text edit would collapse the very selection the user is
    // about to replace. Same rule every MessageEditorToolbar button follows.
    pimpl->emojiButton->setFocusPolicy(Qt::NoFocus);
    pimpl->emojiButton->setToolTip(tr("Insert emoji"));
    pimpl->emojiButton->setCheckable(true);
    pimpl->emojiButton->setVisible(false);
    pimpl->trailingLayout->insertWidget(0,pimpl->emojiButton);

    // --- microphone button: right after the emoji button, built hidden for the same reason. It
    // is the trailing group's press-and-hold control, so all of its mouse events are taken by
    // handleMicButtonEvent() from the event filter, and none reach IconTextButton -- which would
    // otherwise turn the release into a click and toggle its checked state.
    pimpl->micButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("MessageEditor::microphone"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->micButton->setObjectName("micButton");
    pimpl->micButton->setText(QString());
    pimpl->micButton->setCursor(Qt::PointingHandCursor);
    // Same rule as every button here: a press must not move focus out of the text edit.
    pimpl->micButton->setFocusPolicy(Qt::NoFocus);
    pimpl->micButton->setToolTip(tr("Hold to record a voice message"));
    // Checked for exactly as long as it is held: that is what the stylesheet colours it by, and
    // the icon takes its "on" mode.
    pimpl->micButton->setCheckable(true);
    pimpl->micButton->setVisible(false);
    // No ripple here, being held is highlight enough.
    if (auto* ripple=pimpl->micButton->rippleOverlay())
    {
        ripple->setRippleEnabled(false);
    }
    pimpl->micButton->installEventFilter(this);
    pimpl->trailingLayout->insertWidget(1,pimpl->micButton);

    // Hover-to-open: the pointer has to REST on the button, see EmojiHoverOpenDelayMs.
    pimpl->emojiHoverOpenTimer=new QTimer(this);
    pimpl->emojiHoverOpenTimer->setSingleShot(true);
    pimpl->emojiHoverOpenTimer->setInterval(EmojiHoverOpenDelayMs);
    connect(pimpl->emojiHoverOpenTimer,&QTimer::timeout,this,
        [this]()
        {
            // Re-check rather than trust the timer: the pointer may have left, the button may
            // have been hidden by a mode switch, or a click may have pinned the gallery already
            // during the delay.
            if (pimpl->emojiDialogOpen || !pimpl->emojiButton->isVisible()
                || !pimpl->emojiButton->isEnabled() || !isCursorOverEmojiUi())
            {
                return;
            }
            openEmojiGallery(false);
        }
    );

    pimpl->emojiHoverCloseTimer=new QTimer(this);
    pimpl->emojiHoverCloseTimer->setInterval(EmojiHoverPollMs);
    connect(pimpl->emojiHoverCloseTimer,&QTimer::timeout,this,&MessageEditor::onEmojiHoverPoll);

    pimpl->emojiButton->installEventFilter(this);

    connect(pimpl->emojiButton,&IconTextButton::clicked,this,
        [this]()
        {
            // A click is always decisive, so it never waits for (or races) the hover delay.
            pimpl->emojiHoverOpenTimer->stop();

            if (pimpl->emojiDialogOpen && !pimpl->emojiDialogPinned)
            {
                // Clicking a gallery that is merely hovered into view PINS it. Closing it here
                // instead would be the natural-looking implementation and the wrong behaviour:
                // the user's gesture was "keep this", and the gallery is under their pointer.
                pinEmojiGallery();
                // A genuine user gesture -- unlike pinEmojiGallery() calls reached through
                // openEmojiGallery(true) from showEvent() or a host's setEmojiGalleryPinned(),
                // neither of which should echo back at the host that pushed them.
                emit emojiGalleryPinnedChanged(true);
            }
            else if (pimpl->emojiDialogOpen)
            {
                closeEmojiGallery();
            }
            else
            {
                openEmojiGallery();
                if (isEmojiGalleryPinned())
                {
                    emit emojiGalleryPinnedChanged(true);
                }
            }
            // Deliberately NO restoreEditorFocus() here. Unlike every other button in this
            // editor, this one opens a picker that STAYS OPEN and has a search box of its own --
            // pulling focus back to the text edit now would take it straight out from under a
            // user about to type a search. Focus returns when the picker closes, from the
            // FloatingDialogFrame::closed() handler in openEmojiGallery().
        }
    );
    // Same always-toggles trap as expandButton above: click() unconditionally toggle()s right
    // after emitting clicked(), so the button's own checked state is never authoritative --
    // re-assert it from emojiDialogOpen, which is. Terminates because setChecked() re-emits
    // toggled() only on an actual change.
    connect(pimpl->emojiButton,&IconTextButton::toggled,this,
        [this](bool checked)
        {
            if (checked!=isEmojiGalleryPinned())
            {
                syncEmojiButtonChecked();
            }
        }
    );

    connect(pimpl->expandButton,&IconTextButton::clicked,this,
        [this]()
        {
            setExpanded(!isExpanded());
            // Revealing or hiding the toolbar is a step on the way to typing, not a destination
            // -- same reasoning as every toolbar action, see restoreEditorFocus().
            restoreEditorFocus();
        }
    );
    // Same always-toggles trap as every checkable IconTextButton in MessageEditorToolbar --
    // click() unconditionally toggle()s right after emitting clicked(), so without this the
    // button's own checked state would race ahead of/behind updateExpanded()'s own setChecked().
    connect(pimpl->expandButton,&IconTextButton::toggled,this,
        [this](bool checked)
        {
            if (checked!=isExpanded())
            {
                pimpl->expandButton->setChecked(isExpanded());
            }
        }
    );

    connect(pimpl->toolbar,&MessageEditorToolbar::closeRequested,this,
        [this]()
        {
            setExpanded(false);
            restoreEditorFocus();
        }
    );

    // Deliberately routed through a lambda rather than connected straight to
    // setMessageEditingMode(): focus must come back to the text edit only for a mode the USER
    // picked from the drop-down, never for a programmatic setMessageEditingMode()/setEditingMode()
    // call -- a host that configures its composer's mode during construction (whitemdesktop's
    // ChatPageBottom::construct() does exactly that) would otherwise steal focus at startup.
    connect(pimpl->toolbar,&MessageEditorToolbar::modeRequested,this,
        [this](MessageEditingMode mode)
        {
            setMessageEditingMode(mode);
            restoreEditorFocus();
        }
    );

    // Undo/redo are plain document edits -- no format applier, but the same finishFormatAction()
    // tail so the toolbar re-reads the caret's state and focus goes back to the text edit.
    connect(pimpl->toolbar,&MessageEditorToolbar::undoRequested,this,
        [this]()
        {
            pimpl->editor->undo();
            finishFormatAction();
        }
    );
    connect(pimpl->toolbar,&MessageEditorToolbar::redoRequested,this,
        [this]()
        {
            pimpl->editor->redo();
            finishFormatAction();
        }
    );

    // Availability is the document's to report, not something syncToolbarState() can compute from
    // the caret -- and it must stay correct even while the toolbar is hidden, since the toolbar
    // reads its own enabled state when it is next shown.
    connect(pimpl->editor,&QTextEdit::undoAvailable,this,
        [this](bool available)
        {
            pimpl->toolbar->setButtonEnabled(MessageEditorToolbarButton::Undo,available);
        }
    );
    connect(pimpl->editor,&QTextEdit::redoAvailable,this,
        [this](bool available)
        {
            pimpl->toolbar->setButtonEnabled(MessageEditorToolbarButton::Redo,available);
        }
    );
    pimpl->toolbar->setButtonEnabled(
        MessageEditorToolbarButton::Undo,pimpl->editor->document()->isUndoAvailable()
    );
    pimpl->toolbar->setButtonEnabled(
        MessageEditorToolbarButton::Redo,pimpl->editor->document()->isRedoAvailable()
    );

    connect(pimpl->toolbar,&MessageEditorToolbar::boldRequested,this,&MessageEditor::applyBold);
    connect(pimpl->toolbar,&MessageEditorToolbar::italicRequested,this,&MessageEditor::applyItalic);
    connect(pimpl->toolbar,&MessageEditorToolbar::underlineRequested,this,&MessageEditor::applyUnderline);
    connect(pimpl->toolbar,&MessageEditorToolbar::strikethroughRequested,this,&MessageEditor::applyStrikethrough);
    connect(pimpl->toolbar,&MessageEditorToolbar::inlineCodeRequested,this,&MessageEditor::applyInlineCode);
    connect(pimpl->toolbar,&MessageEditorToolbar::bulletListRequested,this,&MessageEditor::applyBulletList);
    connect(pimpl->toolbar,&MessageEditorToolbar::numberedListRequested,this,&MessageEditor::applyNumberedList);
    connect(pimpl->toolbar,&MessageEditorToolbar::blockquoteRequested,this,&MessageEditor::applyBlockquote);
    connect(pimpl->toolbar,&MessageEditorToolbar::headingRequested,this,&MessageEditor::applyHeading);
    connect(pimpl->toolbar,&MessageEditorToolbar::codeBlockRequested,this,&MessageEditor::applyCodeBlock);
    connect(pimpl->toolbar,&MessageEditorToolbar::tableRequested,this,&MessageEditor::applyTable);
    connect(pimpl->toolbar,&MessageEditorToolbar::horizontalRuleRequested,this,&MessageEditor::applyHorizontalRule);
    connect(pimpl->toolbar,&MessageEditorToolbar::tableActionRequested,this,&MessageEditor::applyTableAction);
    connect(pimpl->toolbar,&MessageEditorToolbar::indentIncreaseRequested,this,[this]{ applyIndentStep(1); });
    connect(pimpl->toolbar,&MessageEditorToolbar::indentDecreaseRequested,this,[this]{ applyIndentStep(-1); });
    connect(pimpl->toolbar,&MessageEditorToolbar::clearFormattingRequested,this,&MessageEditor::applyClearFormatting);

    // Stage 5b: Link is a shipped feature, not a hidden placeholder like Mention (Stage 6) still
    // is below -- made permanently visible here rather than left at the toolbar's own
    // ctor-default hidden state. RemoveLink's visibility stays dynamic (see syncToolbarState()):
    // it appears only while the caret is inside an existing link.
    pimpl->toolbar->setButtonVisible(MessageEditorToolbarButton::Link,true);

    connect(pimpl->toolbar,&MessageEditorToolbar::linkRequested,this,&MessageEditor::onLinkButtonRequested);
    connect(pimpl->toolbar,&MessageEditorToolbar::removeLinkRequested,this,&MessageEditor::removeLink);

    // Stage 6: unlike Link, Mention stays HIDDEN until a host opts in via
    // setMentionButtonVisible(true) -- a mention button with no user directory behind it does
    // nothing at all (the editor has no selector of its own; see
    // AbstractMessageEditor::mentionRequested()). It IS enabled in every editing mode, though,
    // because its plain "@username" form (insertMentionText()) is valid in all three -- see
    // FormattingButtons' own comment in messageeditortoolbar.cpp.
    connect(pimpl->toolbar,&MessageEditorToolbar::mentionRequested,this,&MessageEditor::onMentionButtonRequested);

    // task-spellcheck.md. `enable` is authoritative editor state computed by wireCheckable(), the
    // same contract as boldRequested()/italicRequested()/etc. above -- setSpellCheckEnabled()
    // both applies it and pushes it back onto the toolbar/menu.
    connect(pimpl->toolbar,&MessageEditorToolbar::spellCheckRequested,this,&AbstractMessageEditor::setSpellCheckEnabled);

    connect(pimpl->editor,&QTextEdit::cursorPositionChanged,this,&MessageEditor::syncToolbarState);
    connect(pimpl->editor,&QTextEdit::selectionChanged,this,&MessageEditor::syncToolbarState);
    connect(pimpl->editor,&QTextEdit::currentCharFormatChanged,this,
        [this](const QTextCharFormat&)
        {
            syncToolbarState();
        }
    );

    setupReturnPressed();
    connect(
        pimpl->editor,
        &EnhancedTextEdit::returnPressed,
        this,
        &AbstractMessageEditor::finishEditing
    );

    connect(
        pimpl->editor,
        &QTextEdit::textChanged,
        this,
        [this]()
        {
            // Send and the mic button swap places in the same trailing layout slot, but they are
            // toggled by two different classes off this one signal: applyMicButtonVisibility()
            // below flips the mic button now, and the emit at the end reaches
            // ChatPageBottom::updateSendButtonActive() (connected to AbstractMessageEditor::
            // textChanged), which flips Send. Left unbatched, Qt can paint the moment in between --
            // both visible at once, or neither -- as its own frame: the trailing group widens or
            // collapses, the text area resizes to fill the gap, then everything snaps back once the
            // second toggle lands. setUpdatesEnabled(false) suppresses painting for that whole
            // in-between window so only the FINAL, consistent state (task-composer-trailing-
            // flicker.md) ever reaches the screen.
            setUpdatesEnabled(false);
            updateArrangementForContent();
            // QTextEdit::textChanged is relayed from QTextDocument::contentsChanged, which
            // QTextDocumentPrivate::finishEdit() emits for a FORMAT-only edit too -- so this also
            // fires for "turn the empty block into a list item", which is precisely the case the
            // placeholder has to react to. See updatePlaceHolderText().
            updatePlaceHolderText();
            // Send takes the mic button's place as soon as there is something to send.
            applyMicButtonVisibility();
            emit textChanged();
            setUpdatesEnabled(true);
        }
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::activated,
        this,
        &AbstractMessageEditor::activated
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::attachmentsPasted,
        this,
        &AbstractMessageEditor::attachmentsPasted
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::insertRejected,
        this,
        &AbstractMessageEditor::insertRejected
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::editPreviousRequested,
        this,
        &AbstractMessageEditor::editPreviousRequested
    );

    // See AbstractMessageEditor::mentionQueryChanged()/mentionQueryClosed()/
    // mentionCompletionRequested() -- relayed here verbatim, same arrangement as
    // editPreviousRequested() above.
    connect(
        pimpl->editor,
        &EnhancedTextEdit::mentionQueryChanged,
        this,
        &AbstractMessageEditor::mentionQueryChanged
    );
    connect(
        pimpl->editor,
        &EnhancedTextEdit::mentionQueryClosed,
        this,
        &AbstractMessageEditor::mentionQueryClosed
    );
    connect(
        pimpl->editor,
        &EnhancedTextEdit::mentionCompletionRequested,
        this,
        &AbstractMessageEditor::mentionCompletionRequested
    );

    // See EnhancedTextEdit::emojiShortcodeTyped()'s own doc comment -- this connection must be
    // DIRECT (the default for a same-thread connection, which this always is) so
    // armEmojiShortcodeRevert() runs synchronously inside applyEmojiShortcodeGuard()'s own
    // Q_EMIT, which is what lets that guard decide whether to consume the triggering ':'.
    connect(
        pimpl->editor,
        &EnhancedTextEdit::emojiShortcodeTyped,
        this,
        &MessageEditor::onEmojiShortcodeTyped
    );

    // The mechanical half of paste normalization (strip baked colour/font, fix an invisible
    // pasted table, convert a pasted code block to literal fences) is done inside
    // EnhancedTextEdit itself, which needs no MessageEditor state for any of it. Re-indenting a
    // pasted blockquote DOES need blockquoteIndent(), which lives here -- so that one step runs
    // from this handler instead.
    connect(
        pimpl->editor,
        &EnhancedTextEdit::pastedRichText,
        this,
        [this]()
        {
            // suppressUndo=false -- disabling undo here would clear the entire undo stack on
            // every paste (measured), throwing away everything typed before it.
            normalizeBlockquoteIndent(false);
        }
    );

    // Tab/Shift+Tab land in exactly the same applier the toolbar's indent buttons use, so the
    // keyboard and the toolbar can never drift apart.
    connect(
        pimpl->editor,
        &EnhancedTextEdit::indentStepRequested,
        this,
        &MessageEditor::applyIndentStep
    );

    connect(
        pimpl->editor,
        &QWidget::customContextMenuRequested,
        this,
        &MessageEditor::showContextMenu
    );
}

//--------------------------------------------------------------------------

MessageEditor::~MessageEditor()
{
    // parentless, so nothing else would delete it
    if (!pimpl->micDragProxy.isNull())
    {
        delete pimpl->micDragProxy.data();
    }
    if (!pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu->closeDropdown(true);
        destroyWidget(pimpl->contextMenu);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::loadText(const QString& text, TextFormat format)
{
    // Mode table (task-message-formatting-plan.md, Stage 5a):
    //  - Wysiwyg: today's Qt::RichText branch, verbatim.
    //  - Markdown: the document IS the source, so every format loads as plain text -- there is
    //    no markdown to interpret, only source to hold. An Html source is converted to markdown
    //    first via a scratch QTextDocument, so "load this rendered HTML as editable markdown
    //    source" still does something sensible.
    //  - Plaintext: always setPlainText(), ignoring format entirely -- bit-identical to today's
    //    `if (editingMode()!=Qt::RichText) setPlainText(text)` for every argument, which is
    //    exactly whitemdesktop's one call site (ChatPageBottom::construct(), on an empty
    //    document) and must keep behaving unchanged.
    switch (messageEditingMode())
    {
        case (MessageEditingMode::Wysiwyg):
        {
            switch (format)
            {
                case (TextFormat::Markdown):
                {
                    // markdownWithChatLineBreaks() first: setMarkdown() follows CommonMark, where
                    // a single newline inside a paragraph is a SPACE -- measured, "aa\nbb" comes
                    // back as one block reading "aa bb", so a line break the user typed and
                    // exported correctly was eaten on the way back in. See that function's own
                    // declaration; it is the identical rule markdownToHtml() applies, shared
                    // rather than duplicated so the editor and the bubble cannot disagree.
                    pimpl->editor->setMarkdown(markdownWithParagraphPerLine(text));
                    normalizeBlockquoteIndent();
                    convertCodeBlocksToText(pimpl->editor->document());
                    stripImportedAnchorStyle(pimpl->editor->document());
                    normalizeImportedTables(pimpl->editor->document(),0,
                                            pimpl->editor->document()->characterCount());
                    normalizeImportedEmoji(pimpl->editor->document(),pimpl->editor->font(),
                                           pimpl->editor->devicePixelRatioF());
                    break;
                }

                case (TextFormat::Plain):
                {
                    pimpl->editor->setPlainText(text);
                    break;
                }

                case (TextFormat::Html):
                {
                    pimpl->editor->setHtml(text);
                    normalizeBlockquoteIndent();
                    convertCodeBlocksToText(pimpl->editor->document());
                    stripImportedAnchorStyle(pimpl->editor->document());
                    normalizeImportedTables(pimpl->editor->document(),0,
                                            pimpl->editor->document()->characterCount());
                    normalizeImportedEmoji(pimpl->editor->document(),pimpl->editor->font(),
                                           pimpl->editor->devicePixelRatioF());
                    break;
                }
            }
            break;
        }

        case (MessageEditingMode::Markdown):
        {
            if (format==TextFormat::Html)
            {
                QTextDocument scratch;
                scratch.setHtml(text);
                pimpl->editor->setPlainText(scratch.toMarkdown());
            }
            else
            {
                pimpl->editor->setPlainText(text);
            }
            break;
        }

        case (MessageEditingMode::Plaintext):
        {
            pimpl->editor->setPlainText(text);
            break;
        }
    }

    // setPlainText()/setMarkdown()/setHtml() all leave the cursor at the start of the document;
    // loading a message for editing should instead land the cursor where typing continues.
    auto cursor=pimpl->editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    pimpl->editor->setTextCursor(cursor);
}

//--------------------------------------------------------------------------

QString MessageEditor::text(TextFormat format) const
{
    // Mode table, mirroring loadText() above -- see its comment. This is where Stage 5a's
    // headline bug is fixed: MessageEditingMode::Markdown's document already IS the user's
    // markdown source, so TextFormat::Markdown must return it VERBATIM via toPlainText(), not
    // toMarkdown() -- toMarkdown() on a plain document backslash-escapes every markdown special
    // character (qtextmarkdownwriter.cpp's escapeSpecialCharacters()/maybeEscapeFirstChar()),
    // turning "**bold**" into "\**bold**". MessageEditingMode::Plaintext deliberately keeps
    // toMarkdown() here: text typed there is LITERAL, and the markdown that renders four literal
    // asterisks around a word IS "\**bold**" -- escaping is correct there, and this is also what
    // keeps whitemdesktop's one Plaintext-mode call site bit-identical to today.
    switch (messageEditingMode())
    {
        case (MessageEditingMode::Wysiwyg):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return wysiwygMarkdown(pimpl->editor->document());
                // plainTextWithEmoji(), not plainTextKeepingIndent(): only WYSIWYG can hold an
                // emoji IMAGE, and toRawText() renders one as U+FFFC -- so the plain leg has to
                // resolve them back to characters or silently drop them. The other two modes
                // hold literal characters already and need no such pass.
                case (TextFormat::Plain): return plainTextWithEmoji(pimpl->editor->document());
                case (TextFormat::Html): return pimpl->editor->toHtml();
            }
            break;
        }

        case (MessageEditingMode::Markdown):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return plainTextKeepingIndent(pimpl->editor->document());
                case (TextFormat::Plain): return plainTextKeepingIndent(pimpl->editor->document());
                case (TextFormat::Html): return markdownToHtml(plainTextKeepingIndent(pimpl->editor->document()));
            }
            break;
        }

        case (MessageEditingMode::Plaintext):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return pimpl->editor->toMarkdown();
                case (TextFormat::Plain): return plainTextKeepingIndent(pimpl->editor->document());
                case (TextFormat::Html): return pimpl->editor->toHtml();
            }
            break;
        }
    }

    return QString{};
}

//--------------------------------------------------------------------------

QString MessageEditor::selectedText(TextFormat format) const
{
    auto cursor = pimpl->editor->textCursor();
    auto fragment = cursor.selection();

    // Same table as text() above, applied to the selected fragment instead of the whole
    // document.
    switch (messageEditingMode())
    {
        case (MessageEditingMode::Wysiwyg):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return wysiwygMarkdown(fragment);
                //! @see MessageEditor::text()'s own Plain row -- same emoji-image reason.
                case (TextFormat::Plain): return plainTextWithEmoji(cursor);
                case (TextFormat::Html): return fragment.toHtml();
            }
            break;
        }

        case (MessageEditingMode::Markdown):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return plainTextKeepingIndent(cursor);
                case (TextFormat::Plain): return plainTextKeepingIndent(cursor);
                case (TextFormat::Html): return markdownToHtml(plainTextKeepingIndent(cursor));
            }
            break;
        }

        case (MessageEditingMode::Plaintext):
        {
            switch (format)
            {
                case (TextFormat::Markdown): return fragment.toMarkdown();
                case (TextFormat::Plain): return plainTextKeepingIndent(cursor);
                case (TextFormat::Html): return fragment.toHtml();
            }
            break;
        }
    }

    return QString{};
}

//--------------------------------------------------------------------------

void MessageEditor::clear()
{
    pimpl->editor->clear();

    // QTextEdit::clear() does NOT reset the format the next typed character will use:
    // QWidgetTextControlPrivate::setContent() saves the cursor's char format before rebuilding the
    // document and re-applies it afterwards (`charFormatForInsertion`), deliberately, so that
    // setPlainText() keeps a caller's formatting. For a composer that is wrong -- clear() is what
    // runs after a message is SENT, and a heading, bold or inline-code format left over from it
    // would silently style the next message too. The block format needs no such care: the document
    // rebuild drops it (list object included) on its own.
    pimpl->editor->setCurrentCharFormat(QTextCharFormat{});
}

//--------------------------------------------------------------------------

void MessageEditor::clearSelection()
{
    auto cursor = pimpl->editor->textCursor();
    cursor.clearSelection();
    pimpl->editor->setTextCursor(cursor);
}

//--------------------------------------------------------------------------

void MessageEditor::selectAll()
{
    pimpl->editor->selectAll();
}

//--------------------------------------------------------------------------

void MessageEditor::cut()
{
    // Stage 6: the same whole-run rule the Backspace/Delete atomicity guard applies -- a
    // selection deleted across only PART of a mention would leave the remainder as a smaller
    // anchor still carrying the same href. copy() is deliberately left alone: copying a partial
    // mention mangles nothing in the live document, only its clipboard text.
    auto cursor=pimpl->editor->textCursor();
    if (pimpl->editor->snapSelectionToMentions(cursor))
    {
        pimpl->editor->setTextCursor(cursor);
    }

    // QTextEdit's own clipboard behavior, matching the composer's stock text-edit actions --
    // deliberately not selectedText(TextFormat::Markdown) plus a manual delete.
    pimpl->editor->cut();
}

//--------------------------------------------------------------------------

void MessageEditor::copy()
{
    pimpl->editor->copy();
}

//--------------------------------------------------------------------------

void MessageEditor::paste()
{
    pimpl->editor->pasteFromClipboard();
}

//--------------------------------------------------------------------------

bool MessageEditor::hasSelection() const
{
    return pimpl->editor->textCursor().hasSelection();
}

//--------------------------------------------------------------------------

bool MessageEditor::isEmpty() const
{
    return pimpl->editor->document()->isEmpty();
}

//--------------------------------------------------------------------------

namespace {

//! Whether a block carries any of the block-level formatting this editor can apply.
bool blockIsFormatted(const QTextBlock& block)
{
    if (block.textList()!=nullptr)
    {
        return true;
    }
    const auto bf=block.blockFormat();
    return bf.headingLevel()>0
           || bf.hasProperty(QTextFormat::BlockQuoteLevel)
           || bf.hasProperty(QTextFormat::BlockCodeFence)
           || bf.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)
           || bf.nonBreakableLines()
           || bf.indent()>0
           || bf.leftMargin()>0;
}

//! Whether a fragment's char format differs from the document's own default in any way this
//! editor treats as formatting. Compared against the DEFAULT font rather than against a blank
//! QTextCharFormat: the editor's own font is not necessarily Qt's, and every fragment carries it.
bool fragmentIsFormatted(const QTextFragment& fragment, const QFont& defaultFont)
{
    const auto cf=fragment.charFormat();
    if (cf.isAnchor())
    {
        return true;
    }
    if (cf.intProperty(QTextFormat::FontSizeAdjustment)!=0)
    {
        return true;
    }
    if (cf.hasProperty(QTextFormat::ForegroundBrush) || cf.hasProperty(QTextFormat::BackgroundBrush))
    {
        return true;
    }
    return cf.fontItalic()!=defaultFont.italic()
           || cf.fontUnderline()!=defaultFont.underline()
           || cf.fontStrikeOut()!=defaultFont.strikeOut()
           || cf.fontFixedPitch()!=defaultFont.fixedPitch()
           || (cf.hasProperty(QTextFormat::FontWeight) && cf.fontWeight()!=defaultFont.weight());
}

}

//--------------------------------------------------------------------------

bool MessageEditor::hasAppliedFormatting() const
{
    auto* doc=pimpl->editor->document();
    if (doc==nullptr || doc->isEmpty())
    {
        return false;
    }

    // What the user APPLIED. Read straight off the document, so it is exact and says nothing
    // about the text's own characters: "2 * 3" typed with no formatting is plain here, even
    // though exporting it as markdown would escape that asterisk.
    //
    // A child frame means a table, the one construct that is not a block property.
    if (!doc->rootFrame()->childFrames().isEmpty())
    {
        return true;
    }
    for (auto block=doc->begin(); block.isValid() && block!=doc->end(); block=block.next())
    {
        if (blockIsFormatted(block))
        {
            return true;
        }
        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            const auto fragment=it.fragment();
            if (fragment.isValid() && fragmentIsFormatted(fragment,doc->defaultFont()))
            {
                return true;
            }
        }
    }
    return false;
}

//--------------------------------------------------------------------------

bool MessageEditor::hasFormatting() const
{
    auto* doc=pimpl->editor->document();
    if (doc==nullptr || doc->isEmpty())
    {
        return false;
    }

    // Step 1 -- what the user APPLIED, see hasAppliedFormatting().
    if (hasAppliedFormatting())
    {
        return true;
    }

    // Step 2 -- markdown SYNTAX the user typed by hand rather than applied. Nothing above can see
    // it: this editor's fenced code blocks are deliberately ordinary text carrying no block
    // properties at all (convertCodeBlocksToText()), and the same goes for a hand-typed "**bold**"
    // or "- item". Rendering is the only reliable test, so ask the renderer: if stripping markdown
    // changes the text, the text is markdown.
    //
    // Compared against the PLAIN serialization, never the markdown one: text(Markdown) escapes
    // specials, so comparing against that would report "2 \* 3" as formatted for a message that
    // has none.
    const auto plain=text(TextFormat::Plain);
    // maxSourceChars raised from markdownToPlainText()'s own preview-sized default: truncation
    // here would make a long plain message differ from itself and be labelled markdown.
    return markdownToPlainText(plain,std::numeric_limits<int>::max()).trimmed()!=plain.trimmed();
}

//--------------------------------------------------------------------------

bool MessageEditor::hasEmoji() const
{
    auto* doc=pimpl->editor->document();
    if (doc==nullptr || doc->isEmpty())
    {
        return false;
    }

    auto defaultPack=ReactionIconPacks::instance().defaultPack();

    for (auto block=doc->begin(); block.isValid(); block=block.next())
    {
        // Same skip normalizeImportedEmoji() applies -- a code block's content is never
        // substituted, so it cannot make this true either.
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage)
            || block.blockFormat().nonBreakableLines())
        {
            continue;
        }

        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            const auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }
            const auto charFormat=fragment.charFormat();

            if (charFormat.isImageFormat())
            {
                if (!emojiReactionId(charFormat.toImageFormat().name()).isEmpty())
                {
                    return true;
                }
                continue;
            }

            if (charFormat.fontFixedPitch() || !defaultPack)
            {
                continue;
            }

            if (!matchEmojiCodePoints(fragment.text(),defaultPack.get()).empty())
            {
                return true;
            }
        }
    }
    return false;
}

//--------------------------------------------------------------------------

bool MessageEditor::canPasteFromClipboard() const
{
    return pimpl->editor->canPasteFromClipboard();
}

//--------------------------------------------------------------------------

void MessageEditor::setMaxLength(int length)
{
    pimpl->editor->setMaxLength(length);
}

//--------------------------------------------------------------------------

int MessageEditor::maxLength() const
{
    return pimpl->editor->maxLength();
}

//--------------------------------------------------------------------------

void MessageEditor::addLeadingWidget(QWidget* widget)
{
    if (widget==nullptr)
    {
        return;
    }
    // Before the expand button (second to last) and the stretch (last), so host widgets keep the
    // order they were added in and the expand button stays the group's last member.
    pimpl->leadingLayout->insertWidget(qMax(0,pimpl->leadingLayout->count()-2),widget);
}

//--------------------------------------------------------------------------

void MessageEditor::addTrailingWidget(QWidget* widget)
{
    if (widget==nullptr)
    {
        return;
    }
    // Positioned relative to the STRETCH rather than at a fixed index: unlike the leading group's,
    // this layout's stretch moves to the front in the stacked arrangement (see applyArrangement()),
    // so a hard-coded count()-1 would drop a widget added while stacked into the wrong place.
    // Ordinary hosts add their buttons at construction time, when the group is still a row, but
    // FileUploadWidget-style late additions must land correctly too.
    //
    // The emoji button is the group's FIRST member and stays there, so host widgets follow it in
    // the row and sit BELOW it in the column -- which is what puts Send at the bottom corner.
    const auto stretch=stretchIndex(pimpl->trailingLayout);
    const auto index=(stretch==0) ? pimpl->trailingLayout->count()
                                  : qMax(0,pimpl->trailingLayout->count()-1);
    pimpl->trailingLayout->insertWidget(index,widget);
}

//--------------------------------------------------------------------------

int MessageEditor::stretchIndex(QBoxLayout* layout)
{
    for (int i=0; i<layout->count(); ++i)
    {
        if (layout->itemAt(i)->spacerItem()!=nullptr)
        {
            return i;
        }
    }
    return -1;
}

//--------------------------------------------------------------------------

void MessageEditor::moveStretch(QBoxLayout* layout, bool toFront)
{
    const auto from=stretchIndex(layout);
    if (from<0)
    {
        return;
    }
    const auto target=toFront ? 0 : layout->count()-1;
    if (from==target)
    {
        return;
    }

    // takeAt() detaches the item without deleting it; count() has already dropped by one by the
    // time insertItem() runs, so appending is insertItem(count()), not count()-1.
    auto* item=layout->takeAt(from);
    layout->insertItem(toFront ? 0 : layout->count(),item);
}

//--------------------------------------------------------------------------

void MessageEditor::applyArrangement()
{
    const auto stacked=isStackedArrangement();

    // The whole switch. leadingFrame | text | trailingFrame stays horizontal forever; only the
    // direction of each frame's OWN layout changes, so no widget is ever reparented or moved
    // between layouts and the two orders cannot drift apart.
    //
    // The two groups map their row order onto the column in OPPOSITE directions, because "keep
    // each button where it already was" means opposite things on the two sides:
    //
    //  - LEADING (BottomToTop): the group's first member, LEFTMOST in the row, ends up lowest in
    //    the column -- nearest the text area's bottom edge, where it already was. The layout's
    //    trailing stretch lands at the TOP, which is what packs the buttons downward.
    //
    //  - TRAILING (TopToBottom): reversed, so the group's LAST member -- the one furthest from
    //    the text area, which for a chat composer is Send -- ends up at the BOTTOM of the column
    //    rather than the top. Send is the action the user reaches for constantly and it belongs
    //    at the bottom corner in both arrangements; leaving this BottomToTop put it above the
    //    emoji button instead. Because this direction puts the stretch at the bottom, it has to
    //    be moved to the front to keep the group packed downward -- see moveStretch().
    pimpl->leadingLayout->setDirection(stacked ? QBoxLayout::BottomToTop
                                              : QBoxLayout::LeftToRight);
    pimpl->trailingLayout->setDirection(stacked ? QBoxLayout::TopToBottom
                                                : QBoxLayout::LeftToRight);
    moveStretch(pimpl->trailingLayout,stacked);

    // As a row the frames pin to the text area's bottom edge; as a column they have to span its
    // full height instead, or there would be no vertical room for the buttons to spread into.
    const auto alignment=stacked ? Qt::Alignment{} : Qt::Alignment{Qt::AlignBottom};
    pimpl->editorRow->setAlignment(pimpl->leadingFrame,alignment);
    pimpl->editorRow->setAlignment(pimpl->trailingFrame,alignment);

    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

int MessageEditor::textLineCount() const
{
    auto* document=pimpl->editor->document();

    // Force a layout pass before reading it: this runs from textChanged, where the block layouts
    // have been invalidated but not necessarily rebuilt yet, and an un-laid-out block reports
    // lineCount()==0 -- which would read as "no lines" and never trip the stacked arrangement.
    // QTextDocument::size() lays the document out on demand.
    (void)document->size();

    int lines=0;
    for (auto block=document->begin(); block!=document->end(); block=block.next())
    {
        auto* blockLayout=block.layout();
        // A wrapped block counts as the several lines it actually occupies -- text that has wrapped
        // is visually multi-line even though it is one block.
        lines += (blockLayout!=nullptr && blockLayout->lineCount()>0) ? blockLayout->lineCount() : 1;
        if (lines>1)
        {
            // Every caller only asks "is it more than one", so stop as soon as that is settled
            // rather than walking a long document.
            break;
        }
    }

    return lines;
}

//--------------------------------------------------------------------------

void MessageEditor::updateArrangementForContent()
{
    // Expanded means a tall text area by definition -- it is pinned at effectiveMaxHeight()
    // whatever the content -- so side widgets belong underneath it for exactly the reason
    // multi-line content moves them there. Without this an expanded but EMPTY editor would strand
    // them halfway down a 300px-tall box, which is the stranded look this arrangement exists to
    // remove.
    if (isExpanded())
    {
        setStackedArrangement(true);
        return;
    }

    if (isStackedArrangement())
    {
        // Deliberately asymmetric: going back inline waits for an EMPTY editor rather than for
        // the text to drop under one line again. Mirroring the outward trip would let a composer
        // being edited around the boundary flip its own layout back and forth while the user
        // types, moving the send button out from under the pointer.
        if (pimpl->editor->document()->isEmpty())
        {
            setStackedArrangement(false);
        }
        return;
    }

    if (textLineCount()>1)
    {
        setStackedArrangement(true);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::updateStackedArrangement()
{
    applyArrangement();
}

//--------------------------------------------------------------------------

QFrame* MessageEditor::leadingWidgetsFrame() const
{
    return pimpl->leadingFrame;
}

//--------------------------------------------------------------------------

QFrame* MessageEditor::trailingWidgetsFrame() const
{
    return pimpl->trailingFrame;
}

//--------------------------------------------------------------------------

MessageEditorToolbar* MessageEditor::toolbar() const
{
    return pimpl->toolbar;
}

//--------------------------------------------------------------------------

IconTextButton* MessageEditor::expandButton() const
{
    return pimpl->expandButton;
}

//--------------------------------------------------------------------------

EnhancedTextEdit* MessageEditor::textEdit() const
{
    return pimpl->editor;
}

//--------------------------------------------------------------------------

void MessageEditor::setMaxHeight(int height)
{
    pimpl->editor->setMaxHeight(height);
}

//--------------------------------------------------------------------------

void MessageEditor::setMaxHeightPercent(int percent)
{
    pimpl->editor->setMaxHeightPercent(percent);
}

//--------------------------------------------------------------------------

int MessageEditor::maxHeightPercent() const
{
    return pimpl->editor->maxHeightPercent();
}

//--------------------------------------------------------------------------

void MessageEditor::setMaxHeightReferenceWidget(QWidget* widget)
{
    pimpl->editor->setMaxHeightReferenceWidget(widget);
}

//--------------------------------------------------------------------------

int MessageEditor::maxHeight() const
{
    return pimpl->editor->maxHeight();
}

//--------------------------------------------------------------------------

void MessageEditor::setFocusIn()
{
    pimpl->editor->setFocus();
}

//--------------------------------------------------------------------------

void MessageEditor::updateMessageEditingMode()
{
    const auto from=pimpl->appliedMode;
    const auto to=messageEditingMode();
    if (from==to)
    {
        return;
    }

    // Markdown source of the current document, expressed in terms the TARGET mode understands:
    // a rich Wysiwyg document converts to markdown source; a Markdown/Plaintext document already
    // IS its own source/literal text.
    const auto src=(from==MessageEditingMode::Wysiwyg)
        ? wysiwygMarkdown(pimpl->editor->document())
        : plainTextKeepingIndent(pimpl->editor->document());

    switch (to)
    {
        case (MessageEditingMode::Wysiwyg):
        {
            pimpl->editor->setAcceptRichText(true);
            // See loadText()'s own note: without this the Markdown-mode round trip silently
            // folds every typed line break into a space.
            pimpl->editor->setMarkdown(markdownWithParagraphPerLine(src));
            normalizeBlockquoteIndent();
            convertCodeBlocksToText(pimpl->editor->document());
            stripImportedAnchorStyle(pimpl->editor->document());
            // Without this a table loses its grid the moment the editor is switched to Markdown
            // mode and back: setMarkdown() rebuilds it with border=0/borderCollapse=true, which
            // paints nothing at all.
            normalizeImportedTables(pimpl->editor->document(),0,
                                    pimpl->editor->document()->characterCount());
            // Same class of bug as the tables above, and the more visible one: setMarkdown()
            // restores neither an image's size nor its document resource, so without this every
            // emoji comes back as Qt's 16px broken-file icon. This also turns literal emoji
            // characters into pack images, so one typed in Markdown mode looks the same here as
            // one picked from the gallery.
            normalizeImportedEmoji(pimpl->editor->document(),pimpl->editor->font(),
                                   pimpl->editor->devicePixelRatioF());
            break;
        }

        case (MessageEditingMode::Markdown):
        case (MessageEditingMode::Plaintext):
        {
            pimpl->editor->setAcceptRichText(false);
            pimpl->editor->setPlainText(src);
            break;
        }
    }

    // Same reasoning as loadText()'s own cursor placement -- land the caret where typing
    // continues rather than at the very start of the just-reloaded document.
    auto cursor=pimpl->editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    pimpl->editor->setTextCursor(cursor);

    pimpl->appliedMode=to;

    pimpl->toolbar->setMode(to);
    // Stage 5a decision: formatting is WYSIWYG-only (task-message-formatting-plan.md) -- the
    // toolbar's formatting half greys out rather than vanishing, so the bar's width stays
    // stable across a mode switch.
    pimpl->toolbar->setFormattingEnabled(to==MessageEditingMode::Wysiwyg);
    // Stage 5b: Link is excluded from that blanket rule (see FormattingButtons's own comment in
    // messageeditortoolbar.cpp) because it stays useful in Markdown mode too -- only Plaintext,
    // which carries no markup meaning at all, disables it.
    pimpl->toolbar->setButtonEnabled(MessageEditorToolbarButton::Link,to!=MessageEditingMode::Plaintext);

    applyEmojiButtonVisibility();
    applyEmojiShortcodeAutoReplace();
    if (to==MessageEditingMode::Plaintext)
    {
        // Never leave a picker open over a mode whose insert would be refused. Not a user
        // dismissal -- the pin is remembered, and returning to Wysiwyg/Markdown re-opens it.
        closeEmojiGalleryInternal(false);
    }
    else
    {
        // Wysiwyg <-> Markdown: which icons are offerable just changed (Markdown can only insert
        // a literal emojiCode). Re-hand the gallery the right pack whether it is open or merely
        // warmed up, so a later hover never shows the previous mode's icon set for an instant.
        applyEmojiPackForCurrentMode();
        if (pimpl->emojiDialogPinned && !pimpl->emojiDialogOpen && isVisible()
            && isEmojiButtonVisible())
        {
            // Coming back from Plaintext (the only mode that closes a pinned gallery, just
            // above) with the pin still remembered -- reopen it, same as showEvent() does for a
            // composer that was hidden rather than switched.
            openEmojiGallery(true);
        }
    }
    // The emoji button may have just appeared in or vanished from the trailing group.
    Layout::activateUpward(this);

    syncToolbarState();
}

//--------------------------------------------------------------------------

void MessageEditor::updateFinishOnEnter()
{
    setupReturnPressed();
}

//--------------------------------------------------------------------------

void MessageEditor::updateEditingFinished()
{
}

//--------------------------------------------------------------------------

void MessageEditor::setupReturnPressed()
{
    // effectiveFinishOnEnter(), not isFinishOnEnter(): Enter inserts a line break while the editor
    // is expanded regardless of the host's setting -- see that method's own doc comment. Re-run
    // from updateExpanded() as well as from updateFinishOnEnter(), since either input can change
    // the answer.
    pimpl->editor->setNewLineOnEnter(!effectiveFinishOnEnter());
}

//--------------------------------------------------------------------------

void MessageEditor::setPlaceHolderText(const QString& text)
{
    pimpl->placeHolderText=text;
    updatePlaceHolderText();
}

//--------------------------------------------------------------------------

void MessageEditor::updatePlaceHolderText()
{
    // Qt paints the placeholder on QTextDocument::isEmpty(), which is a pure CHARACTER count
    // (`d->length() <= 1`, qtextdocument.cpp) -- block FORMAT is invisible to it. So an empty block
    // that has just been turned into a list item is still "empty" as far as Qt is concerned, and
    // the placeholder gets drawn straight over the bullet or number the layout is also painting.
    //
    // Suppressed by clearing the base class's own value rather than by intercepting the paint:
    // QTextEdit draws the placeholder inside its own paintEvent(), and save/restore around a call
    // to the base paintEvent() would recurse -- QTextEdit::setPlaceholderText() calls
    // viewport->update() whenever the document is empty, which is exactly this case.
    pimpl->editor->setPlaceholderText(
        hasVisibleBlockFormatting() ? QString{} : pimpl->placeHolderText);
}

//--------------------------------------------------------------------------

bool MessageEditor::hasVisibleBlockFormatting() const
{
    // Only ever interesting while the placeholder could actually be drawn, i.e. while the document
    // holds ONE empty block: a second block already makes QTextDocument::isEmpty() false (a block
    // separator is itself a character), so Qt stops painting the placeholder on its own and there
    // is nothing here to suppress.
    auto* doc=pimpl->editor->document();
    if (doc==nullptr || !doc->isEmpty())
    {
        return false;
    }

    const auto block=doc->firstBlock();
    if (!block.isValid())
    {
        return false;
    }

    // A list paints a marker, and a heading changes the line's own metrics -- either way the
    // editor is no longer visually pristine and a "Write a message..." in body text sitting in it
    // reads as leftover content rather than as a prompt. Blockquote and indent are included for
    // the same reason: both move the caret away from the margin the placeholder is drawn at.
    if (block.textList()!=nullptr)
    {
        return true;
    }
    const auto bf=block.blockFormat();
    return bf.headingLevel()>0
           || bf.hasProperty(QTextFormat::BlockQuoteLevel)
           || bf.indent()>0
           || bf.leftMargin()>0;
}

//--------------------------------------------------------------------------

void MessageEditor::updateExpanded()
{
    pimpl->editor->setExpandedEnabled(isExpanded());
    pimpl->toolbar->setVisible(isExpanded());

    // Enter stops sending while expanded and starts again on collapse -- see
    // AbstractMessageEditor::effectiveFinishOnEnter(), which owns that rule. Nothing is saved or
    // restored here: the host's own finishOnEnter is never written to, so re-running this is all a
    // collapse needs to put the previous behaviour back.
    setupReturnPressed();

    {
        // setChecked() below would otherwise re-emit toggled() -> our re-assert handler ->
        // harmlessly re-asserts the same value, but blocking it here is cheaper and matches
        // the same style already used for the toolbar's own checkable buttons.
        QSignalBlocker blocker(pimpl->expandButton);
        pimpl->expandButton->setChecked(isExpanded());
    }

    if (isExpanded())
    {
        pimpl->toolbar->setMode(messageEditingMode());
        pimpl->toolbar->setFormattingEnabled(messageEditingMode()==MessageEditingMode::Wysiwyg);
        syncToolbarState();
    }

    // Expanding makes the text area tall regardless of content, so the arrangement has to be
    // re-evaluated here as well as on textChanged. Collapsing runs the ordinary content rule
    // again, which keeps the widgets below whenever real multi-line text is still there and only
    // brings them back beside an empty editor -- the same no-flip-while-typing hysteresis, not a
    // special case for the toggle.
    updateArrangementForContent();

    // setVisible()/setExpandedEnabled()'s own updateGeometry() only POST a QEvent::LayoutRequest
    // -- flush it now so the bar and the new height appear in the same frame as the click, same
    // fix FileUploadWidget::updateCommentsAreaHeight() applies (src/fileuploadwidget.cpp).
    // Deliberately WITHOUT that call's extra coalesced 50ms re-run: that one exists because it
    // fires on every keystroke and races an ancestor QScrollArea's own posted-event tracking;
    // this fires once per deliberate user gesture (or explicit host call), so the posted
    // LayoutRequest settles before the next paint regardless.
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

void MessageEditor::updateExpandButtonVisible()
{
    pimpl->expandButton->setVisible(isExpandButtonVisible());
    pimpl->toolbar->setButtonVisible(MessageEditorToolbarButton::Close,isExpandButtonVisible());
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

void MessageEditor::updateMentionButtonVisible()
{
    pimpl->toolbar->setButtonVisible(MessageEditorToolbarButton::Mention,isMentionButtonVisible());
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

void MessageEditor::updateSpellCheckButtonVisible()
{
    pimpl->toolbar->setButtonVisible(MessageEditorToolbarButton::SpellCheck,isSpellCheckButtonVisible());
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

void MessageEditor::updateSpellCheckEnabled()
{
    pimpl->editor->setSpellCheckEnabled(isSpellCheckEnabled());
    syncToolbarState();
}

//--------------------------------------------------------------------------

MessageEditorFormatState MessageEditor::currentFormatState() const
{
    const auto cf=pimpl->editor->currentCharFormat();
    const auto cursor=pimpl->editor->textCursor();
    const auto bf=cursor.blockFormat();
    const auto* list=cursor.currentList();

    MessageEditorFormatState state;
    state.bold=cf.fontWeight()>=QFont::DemiBold;
    state.italic=cf.fontItalic();
    state.underline=cf.fontUnderline();
    state.strikeOut=cf.fontStrikeOut();
    // A code BLOCK also sets fixed-pitch on its char format -- exclude it here so an inline-code
    // span and a fenced code block do not both light up the same InlineCode button.
    state.inlineCode=cf.fontFixedPitch() && !bf.hasProperty(QTextFormat::BlockCodeFence);
    state.blockquote=bf.hasProperty(QTextFormat::BlockQuoteLevel);
    // BlockCodeFence is kept as a belt-and-braces case (content that reached the document by a
    // path that skipped convertCodeBlocksToText()), but it is NOT what a literal fence carries --
    // convertCodeBlocksToText() strips that property on the way in, since a literal fence is
    // ordinary text with no block properties at all. MessageEditorHighlighter::InFence, tracked
    // via QTextBlock::userState() and measured to update synchronously on every edit once
    // attached to a real QTextEdit, is what actually reflects "the caret is on/inside a fence"
    // for the form this editor's fences actually take (Stage 5b, fixing a state.codeBlock that
    // was otherwise always false for one).
    state.codeBlock=bf.hasProperty(QTextFormat::BlockCodeFence)
        || cursor.block().userState()==MessageEditorHighlighter::InFence;
    state.headingLevel=bf.headingLevel();
    // Stage 6: a mention is an anchor too, but not a LINK for any purpose this state drives --
    // the Remove-link button/row must never offer to unlink one, and "Edit link" must never open
    // the hyperlink dialog on one. The two flags are mutually exclusive.
    state.insideMention=isMentionFormat(cf);
    state.insideLink=isLinkFormat(cf);
    state.insideTable=cursor.currentTable()!=nullptr;
    // Editor-WIDE, not caret-derived like everything else here -- see
    // MessageEditorFormatState::spellCheckEnabled's own doc comment for why it still rides in
    // this struct.
    state.spellCheckEnabled=isSpellCheckEnabled();

    if (list!=nullptr)
    {
        const auto style=list->format().style();
        state.bulletList=style==QTextListFormat::ListDisc
            || style==QTextListFormat::ListCircle
            || style==QTextListFormat::ListSquare;
        state.numberedList=!state.bulletList && style!=QTextListFormat::ListStyleUndefined;
    }

    return state;
}

//--------------------------------------------------------------------------

int MessageEditor::emojiInlineSizeForFont(const QFont& font)
{
    QFontMetrics metrics(font);

    // Ascent, not height(): Qt puts an inline image's BOTTOM on the baseline, so the ascent is
    // exactly the room a glyph occupies above it before scaling.
    auto size=metrics.ascent();
    if (size<=0)
    {
        size=metrics.height();
    }

    // Scaled up so emoji read larger than the surrounding text -- see EmojiInlineScaleNumerator/
    // EmojiInlineScaleDenominator's own comment. This does make a mixed text+emoji line taller
    // than a pure-text neighbour; that is the intended look, not the ascent-matching this used to
    // do.
    size=(size*EmojiInlineScaleNumerator)/EmojiInlineScaleDenominator;

    // Round UP to the quantum -- see EmojiSizeQuantum. Rounding down could reach 0 for a tiny
    // font, which would make the image vanish rather than merely look wrong.
    const auto quantum=EmojiSizeQuantum;
    size=((size+quantum-1)/quantum)*quantum;
    return std::max(size,quantum);
}

//--------------------------------------------------------------------------

void MessageEditor::restoreEditorFocus()
{
    QPointer<EnhancedTextEdit> editor=pimpl->editor;
    QTimer::singleShot(0,this,
        [editor]()
        {
            if (!editor.isNull())
            {
                editor->setFocus();
            }
        }
    );
}

//--------------------------------------------------------------------------

void MessageEditor::finishFormatAction()
{
    syncToolbarState();
    // Deterministic rather than relying on the textChanged relay to also cover format-only edits:
    // applying a list to an EMPTY block changes no characters at all, and that is exactly the case
    // the placeholder has to react to. Idempotent, so running from both paths costs nothing --
    // QTextEdit::setPlaceholderText() early-outs on an unchanged value.
    updatePlaceHolderText();
    restoreEditorFocus();
}

//--------------------------------------------------------------------------

void MessageEditor::syncToolbarState()
{
    // Cheap early-out: nothing to reflect while the toolbar isn't shown, and computing the
    // state on every cursorPositionChanged/currentCharFormatChanged would otherwise run on every
    // caret move even while collapsed.
    if (pimpl->toolbar->isHidden())
    {
        return;
    }

    const auto state=currentFormatState();
    pimpl->toolbar->setFormatState(state);

    // Stage 5b: "a second Remove link ... appears only when the caret is inside an existing
    // link" -- setFormatState() already computes insideLink but (per its own Stage 5a doc
    // comment) does not act on it; this is where it is acted on.
    pimpl->toolbar->setButtonVisible(MessageEditorToolbarButton::RemoveLink,state.insideLink);
}

//--------------------------------------------------------------------------

void MessageEditor::applyBold(bool enable)
{
    QTextCharFormat format;
    format.setFontWeight(enable ? QFont::Bold : QFont::Normal);
    pimpl->editor->mergeCurrentCharFormat(format);
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyItalic(bool enable)
{
    QTextCharFormat format;
    format.setFontItalic(enable);
    pimpl->editor->mergeCurrentCharFormat(format);
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyUnderline(bool enable)
{
    QTextCharFormat format;
    format.setFontUnderline(enable);
    pimpl->editor->mergeCurrentCharFormat(format);
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyStrikethrough(bool enable)
{
    QTextCharFormat format;
    format.setFontStrikeOut(enable);
    pimpl->editor->mergeCurrentCharFormat(format);
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyInlineCode(bool enable)
{
    QTextCharFormat format;
    if (enable)
    {
        // Same fixed-pitch family choice QTextMarkdownImporter itself uses for inline code
        // spans, so a WYSIWYG-typed inline-code run round-trips through toMarkdown()/setMarkdown()
        // looking the same either way.
        format.setFontFixedPitch(true);
        format.setFontFamilies(QFontDatabase::systemFont(QFontDatabase::FixedFont).families());
    }
    else
    {
        // A merge SETS a property, it cannot CLEAR one -- turning inline code off is therefore
        // "set back to the document's own default family", not a true property removal. A true
        // removal would need setCharFormat() over the whole selection, which would also flatten
        // any per-fragment bold/italic already present -- an acceptable, documented trade.
        format.setFontFixedPitch(false);
        format.setFontFamilies(pimpl->editor->document()->defaultFont().families());
    }
    pimpl->editor->mergeCurrentCharFormat(format);
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyBulletList(bool enable)
{
    auto cursor=pimpl->editor->textCursor();
    if (enable)
    {
        cursor.createList(QTextListFormat::ListDisc);
    }
    else if (auto* list=cursor.currentList())
    {
        list->remove(cursor.block());
        auto bf=cursor.blockFormat();
        bf.setIndent(0);
        cursor.setBlockFormat(bf);
    }
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyNumberedList(bool enable)
{
    auto cursor=pimpl->editor->textCursor();
    if (enable)
    {
        cursor.createList(QTextListFormat::ListDecimal);
    }
    else if (auto* list=cursor.currentList())
    {
        list->remove(cursor.block());
        auto bf=cursor.blockFormat();
        bf.setIndent(0);
        cursor.setBlockFormat(bf);
    }
    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyBlockquote(bool enable)
{
    auto cursor=pimpl->editor->textCursor();
    auto bf=cursor.blockFormat();

    // Level 1 or none: this is the toolbar's on/off toggle. Tab/Shift+Tab step the level instead
    // (applyIndentStep()), and both go through the same setBlockquoteLevel() so the toggle and
    // the key can never render a quote differently. markdownrenderer.cpp (Stage 2) reads the same
    // BlockQuoteLevel property on the viewer side, so this round-trips both ways.
    setBlockquoteLevel(bf,enable ? 1 : 0,blockquoteIndent());
    cursor.setBlockFormat(bf);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyHeading(int level)
{
    auto cursor=pimpl->editor->textCursor();

    auto bf=cursor.blockFormat();
    if (level>0)
    {
        bf.setHeadingLevel(level);
    }
    else
    {
        bf.clearProperty(QTextFormat::HeadingLevel);
    }
    cursor.setBlockFormat(bf);

    // Mirrors QTextMarkdownImporter's own heading char format (sizeAdjustment = 4 - level, bold)
    // so a WYSIWYG-typed heading looks the same as one loaded from markdown source.
    cursor.select(QTextCursor::BlockUnderCursor);
    QTextCharFormat cf;
    cf.setProperty(QTextFormat::FontSizeAdjustment,level>0 ? (4-level) : 0);
    cf.setFontWeight(level>0 ? QFont::Bold : QFont::Normal);
    cursor.mergeCharFormat(cf);

    // ...and onto what the editor types NEXT, which is a separate thing and the only one that
    // exists on an EMPTY block: mergeCharFormat() above applies to a selection, and an empty block
    // has no characters to select, so on its own it changes nothing and the heading only appeared
    // once the text had been round-tripped through markdown. (Setting a heading on a block that
    // already had text worked precisely because the selection was non-empty.) Merging into the
    // widget's current format also makes the caret itself take the heading's height straight away,
    // which is the only feedback there is that the mode took effect on an empty line.
    //
    // Applied through the WIDGET's own cursor, not the local copy above -- the copy's selection is
    // BlockUnderCursor, and the current char format belongs to the real caret.
    pimpl->editor->mergeCurrentCharFormat(cf);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyCodeBlock()
{
    auto cursor=pimpl->editor->textCursor();
    cursor.beginEditBlock();

    // Three ordinary lines of TEXT -- an opening fence, a placeholder, a closing fence -- and no
    // block properties whatsoever. That is the whole feature: the fences are in the document, so
    // they are visible, editable in place, and a language tag can simply be typed onto the
    // opening one ("```cpp") without leaving WYSIWYG. Everything that makes this survive the trip
    // to markdown happens on export, in restoreCodeFences().
    if (!cursor.block().text().isEmpty())
    {
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertBlock(QTextBlockFormat{},QTextCharFormat{});
    }
    else
    {
        // Start from a clean slate even on an empty line: the caret may be sitting in a list
        // item, a quote or a heading, and a fence inheriting any of those would export wrapped
        // in it.
        cursor.setBlockFormat(QTextBlockFormat{});
        cursor.setBlockCharFormat(QTextCharFormat{});
    }

    cursor.insertText(CodeFence+QStringLiteral("\n")+tr("code here")
                      +QStringLiteral("\n")+CodeFence);

    cursor.endEditBlock();

    // Leave the placeholder SELECTED, so the first thing typed replaces it.
    cursor.movePosition(QTextCursor::PreviousBlock);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock,QTextCursor::KeepAnchor);
    pimpl->editor->setTextCursor(cursor);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyHorizontalRule()
{
    auto cursor=pimpl->editor->textCursor();

    // One undoable action, not three: without this the user would have to press Ctrl+Z up to three
    // times to take one rule back out.
    cursor.beginEditBlock();

    // A rule lives in a block of its OWN. Qt draws it along the bottom edge of its block unless
    // that block is empty, in which case it is centred (qtextdocumentlayout.cpp:2086) -- the empty
    // block is the form that looks like a rule rather than an underline, so give it one.
    if (!cursor.block().text().isEmpty())
    {
        cursor.insertBlock();
    }

    // A FRESH QTextBlockFormat, never a copy of the current one: a rule inherited into a list
    // item, a blockquote or a heading would carry that construct's indent, marker and margins
    // along with it. Starting clean also drops list membership, which travels in the block
    // format's own object index.
    QTextBlockFormat ruleFormat;
    // Deliberately the plain int Qt's own markdown importer writes (qtextmarkdownimporter.cpp:335)
    // rather than a QTextLength: QTextFormat::lengthProperty() returns a default VariableLength
    // for a non-length value, which resolves to the FULL available width, and
    // QTextDocument::toHtml() omits the width attribute for exactly that type. So this one value
    // gives a full-width rule, a bare "<hr />" on export, and a document identical to one loaded
    // from markdown -- verified in both directions.
    ruleFormat.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth,1);
    cursor.setBlockFormat(ruleFormat);

    // Land the caret on a clean block BELOW the rule, so typing continues after it instead of
    // inside it. Both formats are passed explicitly -- an argument-less insertBlock() would
    // inherit the rule property itself and draw a second rule on every subsequent line.
    cursor.insertBlock(QTextBlockFormat{},QTextCharFormat{});

    cursor.endEditBlock();

    pimpl->editor->setTextCursor(cursor);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyTable(int rows, int columns)
{
    if (rows<=0 || columns<=0)
    {
        return;
    }

    // A default-constructed QTextTableFormat has border 0 and no width, so a freshly inserted
    // EMPTY table is literally invisible: no gridlines to see, and columns that size themselves
    // to content collapse to nothing when every cell is empty. Give it a real grid and a full
    // width so it is something the user can see and click into.
    //
    // setBorderCollapse(false) is LOAD-BEARING and must be set EXPLICITLY: QTextTableFormat's own
    // constructor calls setBorderCollapse(true) (qtextformat.cpp), so simply not asking for
    // border collapse still leaves it ON -- which is why an earlier attempt at this fix, that
    // merely omitted the call, changed nothing at all.
    //
    // Why it matters, verified against Qt 6.9.0 (qtextdocumentlayout.cpp): with collapse ON the
    // grid is painted only by drawTableCellBorder(), which drawTableCell() calls solely when the
    // CELL carries explicit TableCellLeft/Top/Right/BottomBorder properties (:1899) -- while the
    // path that draws a 1px border around every cell from the TABLE's own format is itself gated
    // on `!borderCollapse` (:1828). A freshly created table has no per-cell border properties, so
    // with collapse on it hits neither path and draws absolutely nothing.
    //
    // Measured with a standalone offscreen probe against Qt 6.8.2, rendering this exact format to
    // a QImage and counting border pixels: collapse=true -> 0 pixels, collapse=false -> 2720.
    // Border/collapse/brush are set by applyTableVisibilityFormat() below, shared with
    // normalizeImportedTables() so a typed table and an imported one cannot end up with different
    // visibility fixes.
    QTextTableFormat format;
    format.setCellPadding(4);
    format.setCellSpacing(0);

    // Percentage of the text area by default, so an empty table is immediately visible and
    // clickable; 0 opts into Qt's own content sizing instead (see
    // setInsertedTableWidthPercent()). Clamped rather than trusted: a percentage above 100 makes
    // the table overflow the text area horizontally with no way to scroll to it, and a negative
    // one is meaningless.
    const auto widthPercent=insertedTableWidthPercent();
    if (widthPercent>0)
    {
        format.setWidth(QTextLength(QTextLength::PercentageLength,qMin(widthPercent,100)));
    }

    // A deliberately THEME-NEUTRAL gridline, not one derived from the palette.
    //
    // A QTextTableFormat's brush is baked into the document when the table is created, and
    // nothing re-reads it afterwards -- so a palette-derived colour is correct only until the
    // user switches theme, at which point every table already in the document keeps a colour
    // chosen for the old background. Measured, rendering this exact format and counting painted
    // pixels: a table inserted in the light theme drew 2120 border pixels there and *0* after a
    // switch to dark; one inserted in dark likewise vanished in light. Invisible either way.
    //
    // Restyling the tables on QEvent::StyleChange was the obvious alternative and is worse:
    // QTextDocumentPrivate::changeObjectFormat() calls appendUndoItem(), so every theme switch
    // would push an undo entry -- Ctrl+Z after switching themes would revert a border colour
    // instead of the user's last edit -- and it emits contentsChanged, i.e. a spurious
    // textChanged to the host. The signal is blockable; the undo entry is not (disabling undo
    // around it CLEARS the stack, which is worse still).
    //
    // Mid grey sits between the two editor backgrounds (#FFFFFF light, #000000 dark), so one
    // colour works in both and cannot rot on a theme change. Opaque rather than alpha-softened:
    // measured contrast 3.95:1 on light and 5.32:1 on dark, where alpha 170 gave only 2.32:1 and
    // 2.82:1 -- and an invisible table has been reported twice already.
    applyTableVisibilityFormat(format);

    auto cursor=pimpl->editor->textCursor();

    cursor.insertTable(rows,columns,format);

    // insertTable() moves ITS OWN cursor into the first cell (qtextcursor.cpp: d->setPosition(
    // pos+1)) -- but that cursor is a COPY of the widget's, so without assigning it back the
    // visible caret stays wherever it was before the table and the user has no obvious way in.
    pimpl->editor->setTextCursor(cursor);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyTableAction(MessageEditorTableAction action)
{
    auto cursor=pimpl->editor->textCursor();
    auto* table=cursor.currentTable();
    if (table==nullptr)
    {
        return;
    }

    // Everything is relative to the cell the caret is in, which is what makes these read as
    // "here" rather than "somewhere in the table".
    const auto cell=table->cellAt(cursor);
    if (!cell.isValid())
    {
        return;
    }

    switch (action)
    {
        case (MessageEditorTableAction::InsertRowAbove):
        {
            table->insertRows(cell.row(),1);
            break;
        }

        case (MessageEditorTableAction::InsertRowBelow):
        {
            // rowSpan() rather than row()+1: with a merged cell the row below is the one after
            // everything this cell covers, not the next index.
            table->insertRows(cell.row()+cell.rowSpan(),1);
            break;
        }

        case (MessageEditorTableAction::InsertColumnLeft):
        {
            table->insertColumns(cell.column(),1);
            break;
        }

        case (MessageEditorTableAction::InsertColumnRight):
        {
            table->insertColumns(cell.column()+cell.columnSpan(),1);
            break;
        }

        case (MessageEditorTableAction::RemoveRow):
        {
            // Removing the last row would leave a 0-row table, which Qt keeps in the document as
            // an invisible husk the caret can still enter. Remove the whole table instead, so
            // "delete the only row" means what the user expects.
            if (table->rows()<=1)
            {
                table->removeRows(0,table->rows());
            }
            else
            {
                table->removeRows(cell.row(),1);
            }
            break;
        }

        case (MessageEditorTableAction::RemoveColumn):
        {
            if (table->columns()<=1)
            {
                table->removeColumns(0,table->columns());
            }
            else
            {
                table->removeColumns(cell.column(),1);
            }
            break;
        }

        case (MessageEditorTableAction::RemoveTable):
        {
            // Same mechanism the last-row/last-column cases fall back on: removing every row
            // takes the table object with it, rather than leaving the 0-row husk Qt would
            // otherwise keep in the document.
            table->removeRows(0,table->rows());
            break;
        }
    }

    finishFormatAction();
}

//--------------------------------------------------------------------------

namespace {

//! How deep Tab may nest a blockquote before it stops. Arbitrary, but holding Tab down should
//! reach a stop rather than build "> > > > > > > >" forever.
constexpr const int MaxBlockquoteLevel=6;


//! Spaces one nesting level is worth in markdown SOURCE, matching what Qt's own markdown writer
//! emits for a bullet list (qtextmarkdownwriter.cpp: (level-1)*2).
constexpr const int MarkdownListSourceIndent=2;

//! CommonMark advances a tab to the next 4-column tab stop; used only to convert existing tab
//! indentation to spaces on the way past.
constexpr const int MarkdownSourceTabWidth=4;

//! A markdown source line that opens a list item, with its leading whitespace captured so an
//! indent step can rewrite exactly that run: "- x", "  * x", "1. x", "3) x".
const QRegularExpression& markdownListRe()
{
    static const QRegularExpression re(QStringLiteral("^([ \\t]*)(?:[-*+]|\\d+[.)])(?:\\s|$)"));
    return re;
}

//! The leading run of markdown blockquote markers on a source line: "> ", "> > ", ">>".
const QRegularExpression& markdownQuoteRe()
{
    static const QRegularExpression re(QStringLiteral("^((?:>[ \\t]?)*)"));
    return re;
}


//! The list an item moving to `level` should JOIN rather than start: the one its immediate
//! neighbour already belongs to, if that list sits at the target level in the same style.
//!
//! Previous block first, then next, because a step is normally applied top to bottom -- by the
//! time the block below is looked at, the block above already carries its new list.
QTextList* adjacentListAt(const QTextBlock& block, int level, QTextListFormat::Style style)
{
    for (const auto& neighbour: {block.previous(),block.next()})
    {
        if (!neighbour.isValid())
        {
            continue;
        }

        auto* list=neighbour.textList();
        if (list!=nullptr && list->format().indent()==level && list->format().style()==style)
        {
            return list;
        }
    }

    return nullptr;
}

//! Step the nesting level of every list item among these blocks.
void stepListLevel(const std::vector<QTextBlock>& blocks, int delta)
{
    QTextCursor edit(blocks.front());
    edit.beginEditBlock();

    for (const auto& block: blocks)
    {
        auto* list=block.textList();
        if (list==nullptr)
        {
            continue;
        }

        const auto style=list->format().style();
        const auto level=list->format().indent()+delta;

        if (level<1)
        {
            // Below level 1 there is no list left -- stepping out of the last level takes the
            // block out of the list entirely rather than leaving an indent-0 list, which Qt
            // renders without a marker and which nothing can step back into.
            list->remove(block);

            QTextCursor cursor(block);
            auto blockFormat=cursor.blockFormat();
            blockFormat.setIndent(0);
            cursor.setBlockFormat(blockFormat);

            continue;
        }

        // JOIN an adjacent list at the target level wherever there is one, and only start a new
        // list when there is not. This is the whole of the numbering fix, and it is not an
        // optimisation: QTextCursor::createList() builds a brand new list object on every call,
        // and an ordered list numbers each object from 1 independently. Outdenting one item back
        // to its old level therefore used to leave it in a list of its own, rendering
        // "1. alpha / 1. beta / 2. gamma" -- the reported bug. Indenting siblings one at a time
        // produced the same thing a level down. Measured: QTextList::add() renumbers
        // POSITIONALLY, so an item added back between blocks the list already owns becomes 2 of
        // 3, which is exactly what the markdown export always claimed (md4c merges adjacent
        // same-level items into one list, which is why the bubble looked right while the editor
        // did not).
        //
        // A bullet list hides every one of these cases completely.
        if (auto* joined=adjacentListAt(block,level,style); joined!=nullptr)
        {
            joined->add(block);
            continue;
        }

        // A NEW list at the target level, not format().setIndent() on the existing one: the
        // existing list object is shared by every sibling item, so editing its format in place
        // would re-level all of them instead of just the items being stepped.
        QTextListFormat nested;
        nested.setStyle(style);
        nested.setIndent(level);

        QTextCursor cursor(block);
        cursor.createList(nested);
    }

    edit.endEditBlock();
}

//! Step the blockquote level of every one of these blocks, as document structure.
void stepBlockquoteLevel(const std::vector<QTextBlock>& blocks, int delta, qreal indentPerLevel)
{
    QTextCursor edit(blocks.front());
    edit.beginEditBlock();

    for (const auto& block: blocks)
    {
        QTextCursor cursor(block);
        auto format=cursor.blockFormat();
        const auto level=qBound(0,
                                format.intProperty(QTextFormat::BlockQuoteLevel)+delta,
                                MaxBlockquoteLevel);
        setBlockquoteLevel(format,level,indentPerLevel);
        cursor.setBlockFormat(format);
    }

    edit.endEditBlock();
}

//! Add or remove `count` no-break spaces AT THE CARET, which is where a Tab with nothing selected
//! belongs: Tab is a "widen the gap here" gesture, not only a "shift this line right" one, so
//! mid-line it has to act mid-line. The caret is left after the inserted spaces, ready to type.
void stepNoBreakSpacesAtCursor(QTextEdit* editor, int delta, int count)
{
    if (count<=0)
    {
        return;
    }

    auto cursor=editor->textCursor();
    cursor.beginEditBlock();

    if (delta>0)
    {
        cursor.insertText(QString(count*delta,QChar(NoBreakSpace)));
    }
    else
    {
        // Only ever removes no-break spaces directly behind the caret, so an outdent is the exact
        // inverse of the indent that produced them and can never start eating real characters.
        const auto block=cursor.block();
        const auto offset=cursor.position()-block.position();
        const auto text=block.text();

        int available=0;
        while (available<offset && text.at(offset-1-available)==QChar(NoBreakSpace))
        {
            ++available;
        }

        const auto removed=qMin(available,count*(-delta));
        if (removed>0)
        {
            cursor.setPosition(cursor.position()-removed,QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    }

    cursor.endEditBlock();

    // Written back explicitly: the edits above happen on a COPY of the visible cursor, so without
    // this the caret would not follow the spaces it just inserted.
    editor->setTextCursor(cursor);
}

//! Add or remove `count` no-break spaces at the very start of every one of these blocks.
//!
//! Only reached for a SELECTION in Plaintext mode, which is the one case with no blockquote to
//! fall back on: indenting a selected run of lines has to happen here or not at all. Line starts
//! rather than the caret, because that is a block indent, not a gap in the middle of a sentence.
void stepLineStartNoBreakSpaces(const std::vector<QTextBlock>& blocks, int delta, int count)
{
    if (count<=0)
    {
        return;
    }

    QTextCursor edit(blocks.front());
    edit.beginEditBlock();

    for (const auto& block: blocks)
    {
        // Recomputed per block on purpose: each insertion shifts every later block along, and a
        // QTextBlock reports its CURRENT position rather than the one it had when collected.
        QTextCursor cursor(block);
        cursor.setPosition(block.position());

        if (delta>0)
        {
            cursor.insertText(QString(count*delta,QChar(NoBreakSpace)));
        }
        else
        {
            const auto text=block.text();
            int leading=0;
            while (leading<text.size() && text.at(leading)==QChar(NoBreakSpace))
            {
                ++leading;
            }

            // Only ever removes indentation this same function could have added, so an outdent
            // can never start eating the line's actual first characters.
            const auto removed=qMin(leading,count*(-delta));
            if (removed>0)
            {
                cursor.setPosition(block.position()+removed,QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
        }
    }

    edit.endEditBlock();
}

//! Step the SOURCE indentation in front of every list marker among these lines, which is how
//! markdown spells a nested list item.
void stepSourceListIndent(const std::vector<QTextBlock>& blocks, int delta)
{
    QTextCursor edit(blocks.front());
    edit.beginEditBlock();

    for (const auto& block: blocks)
    {
        const auto text=block.text();
        const auto match=markdownListRe().match(text);
        if (!match.hasMatch())
        {
            continue;
        }

        const auto leading=match.captured(1);
        int width=0;
        for (const auto character: leading)
        {
            width+=(character==QLatin1Char('\t')) ? MarkdownSourceTabWidth : 1;
        }

        const auto target=qMax(0,width+delta*MarkdownListSourceIndent);

        // Replaces the whole existing run, so any tab in it is normalized to spaces on the way
        // past -- leaving tabs there would preserve exactly the ambiguity this change is about.
        QTextCursor cursor(block);
        cursor.setPosition(block.position());
        cursor.setPosition(block.position()+leading.size(),QTextCursor::KeepAnchor);
        cursor.insertText(QString(target,QLatin1Char(' ')));
    }

    edit.endEditBlock();
}

//! Step the "> " prefix on every one of these markdown source lines.
void stepSourceBlockquote(const std::vector<QTextBlock>& blocks, int delta)
{
    QTextCursor edit(blocks.front());
    edit.beginEditBlock();

    for (const auto& block: blocks)
    {
        const auto prefix=markdownQuoteRe().match(block.text()).captured(1);
        const auto level=qBound(0,
                                static_cast<int>(prefix.count(QLatin1Char('>')))+delta,
                                MaxBlockquoteLevel);

        QTextCursor cursor(block);
        cursor.setPosition(block.position());
        cursor.setPosition(block.position()+prefix.size(),QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("> ").repeated(level));
    }

    edit.endEditBlock();
}

}

//--------------------------------------------------------------------------

void MessageEditor::applyIndentStep(int delta)
{
    if (delta==0)
    {
        return;
    }

    switch (messageEditingMode())
    {
        case (MessageEditingMode::Wysiwyg):
        {
            applyRichIndentStep(delta);
            break;
        }

        case (MessageEditingMode::Markdown):
        {
            applySourceIndentStep(delta,true);
            break;
        }

        case (MessageEditingMode::Plaintext):
        {
            applySourceIndentStep(delta,false);
            break;
        }
    }

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applyRichIndentStep(int delta)
{
    auto cursor=pimpl->editor->textCursor();
    const auto blocks=touchedBlocks(cursor);
    if (blocks.empty())
    {
        return;
    }

    // The FIRST touched block decides which of the three meanings applies, so a selection that
    // starts inside a list stays a list operation even where it runs off the end of one. Any
    // rule for genuinely mixed selections would be more surprising than it is useful.
    if (blocks.front().textList()!=nullptr)
    {
        stepListLevel(blocks,delta);
        return;
    }

    if (cursor.hasSelection())
    {
        stepBlockquoteLevel(blocks,delta,blockquoteIndent());
        return;
    }

    stepNoBreakSpacesAtCursor(pimpl->editor,delta,paragraphIndentSpaces());
}

//--------------------------------------------------------------------------

void MessageEditor::applySourceIndentStep(int delta, bool markdownSource)
{
    auto cursor=pimpl->editor->textCursor();
    const auto blocks=touchedBlocks(cursor);
    if (blocks.empty())
    {
        return;
    }

    // In Plaintext mode neither branch below exists: the mode's whole definition is that nothing
    // in the document carries markup meaning, so a "- " is a hyphen and a "> " is a greater-than
    // sign. Only the literal-character indent applies there.
    if (markdownSource)
    {
        if (markdownListRe().match(blocks.front().text()).hasMatch())
        {
            stepSourceListIndent(blocks,delta);
            return;
        }

        if (cursor.hasSelection())
        {
            stepSourceBlockquote(blocks,delta);
            return;
        }
    }

    // Plaintext with a selection is the only case that indents whole lines: every other route
    // out of here has a caret and no selection, and Tab there means "widen the gap I am standing
    // in".
    if (cursor.hasSelection())
    {
        stepLineStartNoBreakSpaces(blocks,delta,paragraphIndentSpaces());
        return;
    }

    stepNoBreakSpacesAtCursor(pimpl->editor,delta,paragraphIndentSpaces());
}

//--------------------------------------------------------------------------

void MessageEditor::normalizeBlockquoteIndent(bool suppressUndo)
{
    auto* document=pimpl->editor->document();

    // Qt's own importers hardcode a 40px-per-level left margin AND a 40px right margin for a
    // blockquote (qtextmarkdownimporter.cpp:631-634 for setMarkdown(), and
    // QTextHtmlParserNode::initializeProperties for setHtml()). Left alone, that is a third
    // rendering of the same construct: one from the toolbar, one from Tab, and a wider one from
    // any content that arrived through markdown -- which is exactly what "switch to markdown and
    // back and the indent changes" looked like. Rewriting it here makes every route agree, and
    // makes blockquoteIndent() actually mean something.
    const auto indent=blockquoteIndent();

    // Only blocks that ARE quoted are touched. A blanket pass would zero left margins Qt may have
    // set on other constructs for reasons of its own.
    const auto undoEnabled=document->isUndoRedoEnabled();
    if (suppressUndo)
    {
        document->setUndoRedoEnabled(false);
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (auto block=document->begin(); block!=document->end(); block=block.next())
    {
        const auto level=block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
        if (level<=0)
        {
            continue;
        }

        QTextCursor blockCursor(block);
        auto format=blockCursor.blockFormat();
        setBlockquoteLevel(format,level,indent);
        blockCursor.setBlockFormat(format);
    }
    cursor.endEditBlock();

    // Undo is suppressed rather than grouped for the whole-document LOAD callers, where the undo
    // stack is meaningless anyway, and where QTextDocumentPrivate::changeObjectFormat() appending
    // an undo item per format write would otherwise bury the user's first real edit under a pile
    // of invisible ones.
    //
    // It must NOT be suppressed on the paste path, and that is not a preference: measured,
    // QTextDocument::setUndoRedoEnabled(false) CLEARS the undo stack outright (one step before,
    // zero after), so doing it on every paste threw away everything the user had typed before --
    // Ctrl+V then Ctrl+Z did nothing at all. With suppressUndo false the re-indent just joins the
    // caller's own edit block instead, and one Ctrl+Z still takes the whole paste back out.
    if (suppressUndo)
    {
        document->setUndoRedoEnabled(undoEnabled);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::setBlockquoteIndent(qreal indent)
{
    pimpl->blockquoteIndent=qMax(qreal(0),indent);
}

//--------------------------------------------------------------------------

qreal MessageEditor::blockquoteIndent() const
{
    return pimpl->blockquoteIndent;
}

//--------------------------------------------------------------------------

void MessageEditor::setParagraphIndentSpaces(int count)
{
    pimpl->paragraphIndentSpaces=qMax(0,count);
}

//--------------------------------------------------------------------------

int MessageEditor::paragraphIndentSpaces() const
{
    return pimpl->paragraphIndentSpaces;
}

//--------------------------------------------------------------------------

void MessageEditor::setListIndentWidth(qreal width)
{
    pimpl->editor->document()->setIndentWidth(width);
}

//--------------------------------------------------------------------------

qreal MessageEditor::listIndentWidth() const
{
    return pimpl->editor->document()->indentWidth();
}

//--------------------------------------------------------------------------

void MessageEditor::applyClearFormatting()
{
    auto cursor=pimpl->editor->textCursor();
    if (!cursor.hasSelection())
    {
        cursor.select(QTextCursor::BlockUnderCursor);
    }

    auto start=cursor.selectionStart();
    auto end=cursor.selectionEnd();

    QTextCursor editCursor(pimpl->editor->document());
    editCursor.beginEditBlock();
    for (auto pos=start; pos<=end; )
    {
        auto block=pimpl->editor->document()->findBlock(pos);
        if (!block.isValid())
        {
            break;
        }

        QTextCursor blockCursor(block);
        blockCursor.select(QTextCursor::BlockUnderCursor);
        blockCursor.setCharFormat(QTextCharFormat{});
        blockCursor.setBlockFormat(QTextBlockFormat{});
        if (auto* list=blockCursor.currentList())
        {
            list->remove(block);
        }

        pos=block.position()+block.length();
    }
    editCursor.endEditBlock();

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::removeLink()
{
    auto cursor=pimpl->editor->textCursor();
    if (!cursor.hasSelection())
    {
        if (!selectLinkRunAtCursor(cursor))
        {
            return;
        }
    }
    else if (!isLinkFormat(cursor.charFormat()))
    {
        // A selection exists but the caret's own end of it isn't inside an ORDINARY link --
        // nothing reliable to remove; same kind of guard applyTableAction() uses outside a table.
        // Stage 6: a selection spanning a link and a mention unlinks only the link -- a mention
        // has no "remove" action of its own (Backspace/Delete already delete it whole, see the
        // atomicity guard in EnhancedTextEdit::keyPressEvent()).
        return;
    }

    const auto start=cursor.selectionStart();
    const auto end=cursor.selectionEnd();
    auto* document=pimpl->editor->document();

    // Collected first, mutated after -- same reason as stripBakedRichTextFormatting(): writing a
    // fragment's format back can merge it with a neighbour, which would invalidate the block's
    // fragment iterator if that happened mid-walk.
    struct Run
    {
        int start;
        int end;
        QTextCharFormat format;
    };
    std::vector<Run> runs;

    for (auto block=document->findBlock(start); block.isValid() && block.position()<end;
         block=block.next())
    {
        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            const auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }

            const auto fragmentStart=fragment.position();
            const auto fragmentEnd=fragmentStart+fragment.length();
            if (fragmentEnd<=start || fragmentStart>=end || !isLinkFormat(fragment.charFormat()))
            {
                continue;
            }

            // clear-then-setCharFormat, not merge{anchor=false}: measured that a merge alone --
            // with or without also merging an empty href -- leaves "[LINK]()" (empty-URL
            // markdown, still a link). Per fragment, not one blanket format over the whole
            // range, so mixed bold/italic runs inside the link survive (measured: an anchor
            // applied over an already-formatted selection splits into same-href fragments with
            // different weight). Also clears the colour Qt's own importer bakes onto an anchor
            // (setMarkdown()'s blue), so removed link text does not stay coloured. Stage 6:
            // isLinkFormat() excludes a mention fragment from this walk entirely, so a selection
            // spanning both leaves the mention untouched.
            auto format=fragment.charFormat();
            format.clearProperty(QTextFormat::IsAnchor);
            format.clearProperty(QTextFormat::AnchorHref);
            format.clearProperty(QTextFormat::AnchorName);
            format.clearProperty(QTextFormat::ForegroundBrush);

            runs.push_back(Run{fragmentStart,fragmentEnd,format});
        }
    }

    if (runs.empty())
    {
        return;
    }

    QTextCursor editCursor(document);
    editCursor.beginEditBlock();
    for (const auto& run : runs)
    {
        QTextCursor fragmentCursor(document);
        fragmentCursor.setPosition(run.start);
        fragmentCursor.setPosition(run.end,QTextCursor::KeepAnchor);
        fragmentCursor.setCharFormat(run.format);
    }
    editCursor.endEditBlock();

    finishFormatAction();
}

//--------------------------------------------------------------------------

bool MessageEditor::selectLinkRunAtCursor(QTextCursor& cursor) const
{
    // A thin wrapper over the generalized walk (Stage 6) -- see selectAnchorRun()'s own doc
    // comment for why this is behaviourally identical to the pre-Stage-6 body.
    return selectAnchorRun(cursor,AnchorRunSide::Before,isLinkFormat);
}

//--------------------------------------------------------------------------

void MessageEditor::onLinkButtonRequested()
{
    const auto state=currentFormatState();
    if (state.codeBlock || state.insideMention)
    {
        // codeBlock: an anchor's href is not backslash-escaped by Qt's markdown writer the way
        // fence content is, so restoreCodeFences() cannot safely unescape it (measured) -- same
        // gate insertLink() itself applies, kept here too so the dialog is never opened for a
        // link that could not be inserted anyway. insideMention (Stage 6): "Edit link" must never
        // open the hyperlink dialog on a mention -- state.insideLink already excludes mentions
        // (see currentFormatState()), so this can only be reached by a host that calls this
        // directly against the documented contract; refused here too for the same reason.
        return;
    }

    auto cursor=pimpl->editor->textCursor();

    QString existingUrl;
    if (state.insideLink && selectLinkRunAtCursor(cursor))
    {
        // Select the whole run now, before the dialog opens: the dialog is modal, so this
        // selection is still exactly what insertLink() replaces once the user accepts.
        pimpl->editor->setTextCursor(cursor);
        existingUrl=cursor.charFormat().anchorHref();
    }

    const auto defaultTitle=cursor.hasSelection() ? plainTextKeepingIndent(cursor) : QString{};

    emit linkRequested(defaultTitle,existingUrl);
}

//--------------------------------------------------------------------------

void MessageEditor::insertLink(const QString& url, const QString& title)
{
    if (url.isEmpty())
    {
        return;
    }
    const auto displayTitle=title.isEmpty() ? url : title;

    if (messageEditingMode()==MessageEditingMode::Markdown)
    {
        // Markdown mode's document IS source text -- a literal insert, no escaping, same
        // philosophy as applySourceIndentStep()'s own "Markdown mode edits source" rule.
        auto cursor=pimpl->editor->textCursor();
        cursor.insertText(QLatin1Char('[')+displayTitle+QStringLiteral("](")+url+QLatin1Char(')'));
        pimpl->editor->setTextCursor(cursor);
        finishFormatAction();
        return;
    }

    if (messageEditingMode()!=MessageEditingMode::Wysiwyg)
    {
        // Plaintext: Link is disabled in the toolbar for this mode
        // (updateMessageEditingMode()) -- reached only if a host calls insertLink() directly,
        // against the documented contract.
        return;
    }

    const auto state=currentFormatState();
    if (state.codeBlock || state.insideMention)
    {
        // insideMention (Stage 6): inserting an anchor at a position inside a mention run would
        // split the run into two half-titles both still carrying the mention href -- the same
        // mangled shape the Backspace/Delete atomicity guard exists to prevent, arrived at from
        // the other side.
        return;
    }

    auto cursor=pimpl->editor->textCursor();
    auto format=cursor.charFormat();
    format.setAnchor(true);
    format.setAnchorHref(url);
    // Deliberately no colour set here -- measured that setMarkdown()'s own importer bakes
    // foreground=#0000ff onto an anchor, and this editor must not do the same: link colour is
    // the viewer's job (ChatMessageTextBrowser::applyLinkStyle()), same rule already applied to
    // blockquote/code-block colour elsewhere in this file.
    //
    // insertText() replaces the selection if cursor has one -- which is also how "edit an
    // existing link" works, since onLinkButtonRequested() already extended the selection to the
    // whole previous run before emitting linkRequested().
    cursor.insertText(displayTitle,format);

    // The caret is left holding the anchor format, so WITHOUT this the next thing typed is
    // swallowed into the link: measured, "Example" + typing " plain" exported as
    // "[Example plain](url)" rather than "[Example](url) plain". Clearing the anchor properties
    // on the collapsed cursor (and on the widget, which tracks its own insertion format) fixes
    // it, and measurably does NOT disturb the text just inserted.
    QTextCharFormat continuation=cursor.charFormat();
    continuation.clearProperty(QTextFormat::IsAnchor);
    continuation.clearProperty(QTextFormat::AnchorHref);
    continuation.clearProperty(QTextFormat::AnchorName);
    cursor.setCharFormat(continuation);

    // Same fix applyTable()'s own comment documents: insertText() on a COPY of the widget's
    // cursor does not by itself move the widget's own caret.
    pimpl->editor->setTextCursor(cursor);
    pimpl->editor->setCurrentCharFormat(continuation);

    finishFormatAction();
}

//--------------------------------------------------------------------------

bool MessageEditor::canInsertMentionAtCursor() const
{
    const auto state=currentFormatState();
    // codeBlock: same measured reason insertLink() refuses there -- an anchor's href is not
    // backslash-escaped by Qt's markdown writer the way fence content is. insideMention: would
    // split one mention run into two half-titles sharing the same href. insideLink: a hidden-uid
    // anchor has no business living inside an ordinary hyperlink's title.
    //
    // Deliberately does NOT gate on state.insideTable -- the '@'-DETECTION side gate
    // (EnhancedTextEdit::mentionQueryAtCursor()) exists to stop an AUTO-POPPED selector from
    // appearing inside a compact table cell; a deliberate toolbar click or context-menu selection
    // is an explicit request, and a mention inside a table cell is ordinary content.
    return !state.codeBlock && !state.insideMention && !state.insideLink;
}

//--------------------------------------------------------------------------

void MessageEditor::onMentionButtonRequested()
{
    if (!canInsertMentionAtCursor())
    {
        return;
    }

    const auto query=pimpl->editor->mentionQueryAtCursor();
    emit mentionRequested(query.isActive ? query.prefix : QString{});
}

//--------------------------------------------------------------------------

void MessageEditor::insertMention(const QString& uid, const QString& title)
{
    if (uid.isEmpty() || uid.contains(QLatin1Char(' ')))
    {
        // A raw space inside the href comes back EMPTY through Qt's markdown writer (measured),
        // producing an anchor that renders but refers to nobody. UIDs in this project never
        // contain one; refused rather than silently emitted if one ever does.
        return;
    }
    const auto displayTitle=title.isEmpty() ? uid : title;

    if (messageEditingMode()==MessageEditingMode::Markdown)
    {
        // Markdown mode's document IS source text -- a literal insert, no escaping, same
        // philosophy as insertLink()'s own Markdown branch.
        auto cursor=pimpl->editor->textCursor();
        if (!cursor.hasSelection())
        {
            pimpl->editor->selectMentionQueryAtCursor(cursor);
        }
        cursor.insertText(
            QLatin1Char('[')+displayTitle+QStringLiteral("](")+mentionHref(uid)+QLatin1Char(')')
        );
        pimpl->editor->setTextCursor(cursor);
        finishFormatAction();
        return;
    }

    if (messageEditingMode()!=MessageEditingMode::Wysiwyg)
    {
        // Plaintext: there is no way to carry a hidden uid in a document with no markup, and
        // quietly writing the title instead would send a message that mentions nobody --
        // insertMentionText() is that mode's route. Reached only if a host calls this directly
        // against the documented contract.
        return;
    }

    if (!canInsertMentionAtCursor())
    {
        return;
    }

    auto cursor=pimpl->editor->textCursor();
    if (!cursor.hasSelection())
    {
        // Replace the in-progress "@word" (the '@' included) rather than inserting beside it --
        // same rule insertLink() follows for an existing link run.
        pimpl->editor->selectMentionQueryAtCursor(cursor);
    }

    auto format=cursor.charFormat();
    format.setAnchor(true);
    format.setAnchorHref(mentionHref(uid));
    // Deliberately no colour set here, same reasoning as insertLink() -- mention colour is the
    // highlighter's job (EnhancedTextEdit::mentionColor), not this editor's document.
    cursor.insertText(displayTitle,format);

    // Same continuation-clearing fix insertLink() established (Stage 5b): without it, the next
    // character typed is absorbed into the mention. The atomicity guard
    // (EnhancedTextEdit::applyMentionAtomicityGuard()) re-applies this same fix for a caret the
    // user later navigates back to; this call covers the instant right after insertion.
    QTextCharFormat continuation=cursor.charFormat();
    continuation.clearProperty(QTextFormat::IsAnchor);
    continuation.clearProperty(QTextFormat::AnchorHref);
    continuation.clearProperty(QTextFormat::AnchorName);
    cursor.setCharFormat(continuation);

    pimpl->editor->setTextCursor(cursor);
    pimpl->editor->setCurrentCharFormat(continuation);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::insertMentionText(const QString& username)
{
    // Valid in EVERY MessageEditingMode, Plaintext included (the confirmed Stage 6 design
    // choice): the payload is ordinary text carrying no markup meaning, so there is no mode that
    // cannot express it -- unlike insertLink()/insertMention(), which refuse in Plaintext.
    const auto text=username.startsWith(QLatin1Char('@'))
        ? username
        : QLatin1Char('@')+username;

    auto cursor=pimpl->editor->textCursor();
    if (!cursor.hasSelection())
    {
        pimpl->editor->selectMentionQueryAtCursor(cursor);
    }

    // Always inserted with the anchor properties cleared off the inherited char format, so a
    // plain mention typed right after a link (or an existing mention) is provably plain.
    auto format=cursor.charFormat();
    format.clearProperty(QTextFormat::IsAnchor);
    format.clearProperty(QTextFormat::AnchorHref);
    format.clearProperty(QTextFormat::AnchorName);
    cursor.insertText(text,format);

    pimpl->editor->setTextCursor(cursor);
    pimpl->editor->setCurrentCharFormat(format);

    finishFormatAction();
}

//--------------------------------------------------------------------------

IconTextButton* MessageEditor::emojiButton() const
{
    return pimpl->emojiButton;
}

//--------------------------------------------------------------------------

constexpr const char* EmojiGallerySharedStateProperty="uiseEmojiGallerySharedState";

//! Find (or, with create=true, create) the given window's shared gallery state. Returns nullptr
//! for a null window, or when create=false and none exists yet -- callers that only ever want to
//! LOOK, never to build one just by asking (e.g. warmEmojiGallery()'s own early-out, or a hidden
//! composer's showEvent()), pass false. Internal linkage: nothing outside this file ever calls
//! it, even though EmojiGallerySharedState itself (see messageeditor.hpp) is not anonymous --
//! MessageEditor_p is not either, for the same reason: a type only forward-declared in the
//! header, for a pimpl-style member, cannot also live in this file's anonymous namespace (that
//! would make it a second, unrelated type of the same name, not the one the header refers to).
static EmojiGallerySharedState* emojiGallerySharedState(QWidget* win, bool create)
{
    if (win==nullptr)
    {
        return nullptr;
    }
    // static_cast, not qobject_cast: EmojiGallerySharedState carries no Q_OBJECT/moc of its own
    // (it is a plain class defined in this .cpp, not run through moc), so qobject_cast could not
    // safely narrow from QObject* to it. Safe here because this function is the ONLY code that
    // ever writes EmojiGallerySharedStateProperty, always with a freshly-built
    // EmojiGallerySharedState -- the property never holds any other type.
    auto* state=static_cast<EmojiGallerySharedState*>(
        win->property(EmojiGallerySharedStateProperty).value<QObject*>()
    );
    if (state==nullptr && create)
    {
        state=new EmojiGallerySharedState(win);
        win->setProperty(EmojiGallerySharedStateProperty,QVariant::fromValue<QObject*>(state));
    }
    return state;
}

//--------------------------------------------------------------------------

bool MessageEditor::isEmojiGalleryOpen() const noexcept
{
    return pimpl->emojiDialogOpen;
}

//--------------------------------------------------------------------------

bool MessageEditor::isEmojiGalleryPinned() const noexcept
{
    return pimpl->emojiDialogOpen && pimpl->emojiDialogPinned;
}

//--------------------------------------------------------------------------

bool MessageEditor::isCursorOverEmojiUi() const
{
    const auto pos=QCursor::pos();

    if (pimpl->emojiButton->isVisible()
        && pimpl->emojiButton->rect().contains(pimpl->emojiButton->mapFromGlobal(pos)))
    {
        return true;
    }

    if (!pimpl->emojiDialog.isNull() && pimpl->emojiDialog->isVisible()
        && pimpl->emojiDialog->frameGeometry().contains(pos))
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------

void MessageEditor::startEmojiHoverPoll()
{
    pimpl->emojiAwayTicks=0;
    pimpl->emojiHoverCloseTimer->start();
}

//--------------------------------------------------------------------------

void MessageEditor::stopEmojiHoverPoll()
{
    pimpl->emojiHoverCloseTimer->stop();
    pimpl->emojiAwayTicks=0;
}

//--------------------------------------------------------------------------

void MessageEditor::onEmojiHoverPoll()
{
    if (!pimpl->emojiDialogOpen || pimpl->emojiDialogPinned)
    {
        stopEmojiHoverPoll();
        return;
    }

    if (isCursorOverEmojiUi())
    {
        pimpl->emojiAwayTicks=0;
        return;
    }

    // Counted in ticks rather than measured against a clock: the poll is the only thing that can
    // observe "away" at all, so ticks ARE the available resolution.
    ++pimpl->emojiAwayTicks;
    const auto graceTicks=(EmojiHoverCloseDelayMs+EmojiHoverPollMs-1)/EmojiHoverPollMs;
    if (pimpl->emojiAwayTicks>=graceTicks)
    {
        closeEmojiGallery();
    }
}

//--------------------------------------------------------------------------

void MessageEditor::pinEmojiGallery()
{
    if (!pimpl->emojiDialogOpen || pimpl->emojiDialogPinned)
    {
        return;
    }
    pimpl->emojiDialogPinned=true;
    stopEmojiHoverPoll();
    syncEmojiButtonChecked();
}

//--------------------------------------------------------------------------

bool MessageEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->micButton && handleMicButtonEvent(event))
    {
        return true;
    }

    if (watched==pimpl->emojiButton)
    {
        switch (event->type())
        {
            case QEvent::Enter:
            {
                // Only when the button is UNCHECKED, per the feature's own rule -- an already
                // pinned gallery is not re-opened, and a hovered one simply stays up (the poll
                // sees the pointer over the button and keeps resetting its away count).
                // A disabled widget still receives Enter and Leave, so the recorder's pinned
                // state, which disables this button, has to be checked here or a hover would
                // open the gallery over the recorder.
                if (!pimpl->emojiDialogOpen && pimpl->emojiButton->isVisible()
                    && pimpl->emojiButton->isEnabled())
                {
                    pimpl->emojiHoverOpenTimer->start();
                }
                break;
            }

            case QEvent::Leave:
            {
                // Leaving before the delay elapses cancels the open outright: the hover was a
                // pass-through, not an intent.
                pimpl->emojiHoverOpenTimer->stop();
                break;
            }

            default:
                break;
        }
    }
    // Never consumed -- this filter only observes.
    return AbstractMessageEditor::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void MessageEditor::applyEmojiButtonVisibility()
{
    // Plaintext has no way to express an emoji at all -- neither an image (no markup) nor, by the
    // same token, any reason to offer a picker whose insert would be refused. See insertEmoji().
    const auto modeAllows=messageEditingMode()!=MessageEditingMode::Plaintext;
    pimpl->emojiButton->setVisible(isEmojiButtonVisible() && modeAllows);
}

//--------------------------------------------------------------------------

void MessageEditor::applyEmojiShortcodeAutoReplace()
{
    // Same "property AND mode allows it" shape as applyEmojiButtonVisibility() just above --
    // Plaintext refuses insertEmoji() outright, so auto-replace would either silently do nothing
    // (confusing: the ':' still gets consumed with no visible effect) or, worse, invite a second
    // code path that tries to insert a literal character there. Neither is wanted; the guard in
    // EnhancedTextEdit only needs one bool to consult.
    const auto enabled=isEmojiShortcodeAutoReplaceEnabled()
        && messageEditingMode()!=MessageEditingMode::Plaintext;
    pimpl->editor->setEmojiShortcodeAutoReplaceEnabled(enabled);
}

//--------------------------------------------------------------------------

void MessageEditor::updateEmojiShortcodeAutoReplace()
{
    applyEmojiShortcodeAutoReplace();
}

//--------------------------------------------------------------------------

void MessageEditor::updateEmojiButtonVisible()
{
    applyEmojiButtonVisibility();
    if (isEmojiButtonVisible())
    {
        // Opting in is the signal that this composer will actually use the picker -- build it now,
        // off the hover path, so the first hover shows an already-constructed dialog.
        warmEmojiGallery();
    }
    else
    {
        // Hiding the button must not strand an open picker with nothing to toggle it shut. Also
        // cancels any hover-open still counting down -- the button is going away. Not a user
        // dismissal -- the pin is remembered, in case the button reappears later.
        closeEmojiGalleryInternal(false);
    }
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

std::shared_ptr<AbstractReactionIconPack> MessageEditor::emojiPackForCurrentMode() const
{
    auto pack=ReactionIconPacks::instance().defaultPack();
    if (!pack)
    {
        return pack;
    }

    // Markdown mode can only insert the literal emojiCode, so an icon that has none is an icon
    // whose click would silently do nothing -- filter those out rather than offer them. Wysiwyg
    // inserts an image and can carry any icon in the pack, so it takes the pack whole.
    if (messageEditingMode()==MessageEditingMode::Markdown)
    {
        return std::make_shared<EmojiCodeReactionIconPack>(std::move(pack));
    }
    return pack;
}

//--------------------------------------------------------------------------

bool MessageEditor::buildEmojiGalleryDialog(EmojiGallerySharedState* state, QWidget* win)
{
    // Parented to the WINDOW, not this editor: the whole point of a shared gallery is that it
    // outlives any one composer -- see task-emoji-gallery-shared-per-window.md. destroyOnClose is
    // therefore moot (nothing ever destroys it early); show=false because the dialog has to be
    // filled and measured before it can be anchored -- see openEmojiGallery().
    auto* frame=new FloatingEmojiGalleryDialog(win);
    state->dialog=frame;

    frame->openDialog(false,false);
    if (frame->dialog().isNull())
    {
        state->dialog=nullptr;
        return false;
    }

    // Connected to the FRAME, not to any one editor: the shared dialog outlives every editor
    // that ever uses it, and both handlers dispatch to state->owner -- whichever editor
    // currently has it -- rather than to a fixed capture, since ownership can change (claimed by
    // a different composer in the same window) without the dialog itself ever being rebuilt.
    connect(frame->dialog(),&AbstractEmojiGalleryDialog::emojiPicked,frame,
        [frame](const QString& reactionId)
        {
            auto* st=emojiGallerySharedState(frame->parentWidget(),false);
            if (st==nullptr || st->owner.isNull())
            {
                return;
            }
            // Deliberately does NOT pin: a hover-opened gallery stays hover-opened through any
            // number of picks, and only an explicit click on the emoji button ever pins (and so
            // checks the button). Picking is aimed at the GALLERY, not at the button.
            //
            // Nothing is lost by not pinning: reaching for a second emoji keeps the pointer OVER
            // the gallery, and onEmojiHoverPoll()/isCursorOverEmojiUi() test the dialog's whole
            // frameGeometry() as well as the button -- so the away counter keeps resetting and
            // the picker cannot dissolve mid-reach. It closes once the pointer has actually left
            // both, which is exactly what "opened on hover" should mean.
            st->owner->insertEmoji(reactionId);
            // iconId(), not the full reaction id: the recents row resolves what it is given
            // against the pack itself -- see ChatReactionQuickBar::setLeadingIconIds(). Unresolvable ids
            // are dropped inside promoteEmojiRecent() rather than guarded here.
            st->owner->promoteEmojiRecent(ChatReactionId::iconId(reactionId));
        }
    );

    // The single place the CURRENT owner's button goes back up, so every close path -- the
    // title-bar X, Escape, an outside dismissal, the hover poll, closeEmojiGallery() -- lands
    // here and nowhere else. Also releases ownership: a claim (composer switch within the same
    // window) never fires this at all, see claimEmojiGallery().
    connect(frame,&FloatingDialogFrame::closed,frame,
        [frame]()
        {
            auto* st=emojiGallerySharedState(frame->parentWidget(),false);
            if (st==nullptr)
            {
                return;
            }
            auto* owner=st->owner.data();
            st->owner=nullptr;
            if (owner!=nullptr)
            {
                owner->onEmojiGalleryClosed();
            }
        }
    );

    return true;
}

//--------------------------------------------------------------------------

void MessageEditor::claimEmojiGallery(EmojiGallerySharedState* state)
{
    auto* previousOwner=state->owner.data();
    state->owner=this;
    if (previousOwner!=nullptr && previousOwner!=this)
    {
        // Retargeted to a different composer in the SAME window, not closed: no fade, no
        // repositioning, no emojiGalleryPinnedChanged() emit. The previous owner's own "is it
        // open for ME" bookkeeping just silently ends; its remembered pin, if any, is untouched
        // -- see emojiDialogPinned's own doc comment. Private members of another MessageEditor
        // instance are reachable here because access control in C++ is per-CLASS, not per-object.
        previousOwner->pimpl->emojiHoverOpenTimer->stop();
        previousOwner->stopEmojiHoverPoll();
        previousOwner->pimpl->emojiDialogOpen=false;
        previousOwner->syncEmojiButtonChecked();
    }
    // Deliberately does NOT touch state->packValid: the pack is a property of the DIALOG (see
    // EmojiGallerySharedState::pack), not of whichever editor owns it, so a claim between two
    // composers in the SAME mode -- the ordinary case -- must NOT force applyEmojiPackForCurrentMode()
    // to rebuild the whole grid for a pack that is already correct. A genuine mode difference is
    // caught there anyway, by comparing against the NEW owner's messageEditingMode().
}

//--------------------------------------------------------------------------

FloatingEmojiGalleryDialog* MessageEditor::ensureEmojiGallery()
{
    if (messageEditingMode()==MessageEditingMode::Plaintext)
    {
        return nullptr;
    }

    auto* win=window();
    if (win==nullptr)
    {
        return nullptr;
    }
    auto* state=emojiGallerySharedState(win,true);

    if (state->dialog.isNull() && !buildEmojiGalleryDialog(state,win))
    {
        return nullptr;
    }

    if (state->owner.data()!=this)
    {
        claimEmojiGallery(state);
    }
    pimpl->emojiDialog=state->dialog;

    return pimpl->emojiDialog.data();
}

//--------------------------------------------------------------------------

void MessageEditor::applyEmojiPackForCurrentMode()
{
    if (pimpl->emojiDialog.isNull() || pimpl->emojiDialog->dialog().isNull())
    {
        return;
    }
    // The cache lives on the shared state, not on this editor -- see
    // EmojiGallerySharedState::pack's own doc comment. Absent only if the window itself somehow
    // vanished between ensureEmojiGallery() setting pimpl->emojiDialog and this call, which never
    // actually happens (both run back to back on the GUI thread) -- defensive, not load-bearing.
    auto* state=emojiGallerySharedState(window(),false);
    if (state==nullptr)
    {
        return;
    }

    const auto mode=messageEditingMode();
    if (state->packValid && state->packMode==mode)
    {
        // Nothing about which icons are offerable has changed, and setPack() would rebuild the
        // entire grid -- including on an ordinary claim between two composers in the SAME mode,
        // which is the common case. The per-open search reset
        // (EmojiGalleryDialog::prepareToShow()) rebuilds it once anyway, which is all an open
        // actually needs.
        return;
    }

    state->pack=emojiPackForCurrentMode();
    state->packMode=mode;
    state->packValid=true;
    pimpl->emojiDialog->dialog()->setPack(state->pack);
}

//--------------------------------------------------------------------------

void MessageEditor::warmEmojiGallery()
{
    if (!isEmojiButtonVisible())
    {
        return;
    }
    if (auto* win=window())
    {
        auto* state=emojiGallerySharedState(win,false);
        if (state!=nullptr && !state->dialog.isNull())
        {
            // Already built -- by this editor or, in a window with more than one composer,
            // possibly another. Warming has nothing left to do, and must NOT claim ownership
            // just for having warmed up: only an actual open (hover, click, or a remembered pin
            // reopening on show) does that, see ensureEmojiGallery().
            return;
        }
    }
    // Deferred by one event-loop turn, never inline: this runs from the visibility update, which
    // is itself reached from a QSS property write during polish -- building a whole second widget
    // tree from inside that would re-enter the style engine on a widget it is still polishing.
    // Same deferral rule, and the same reason, as restoreEditorFocus().
    QPointer<MessageEditor> self=this;
    QTimer::singleShot(0,this,
        [self]()
        {
            if (self.isNull() || !self->isEmojiButtonVisible())
            {
                return;
            }
            auto* win=self->window();
            if (win==nullptr)
            {
                return;
            }
            auto* state=emojiGallerySharedState(win,true);
            if (state->dialog.isNull())
            {
                self->buildEmojiGalleryDialog(state,win);
            }
        }
    );
}

//--------------------------------------------------------------------------

void MessageEditor::openEmojiGallery(bool pinned)
{
    if (messageEditingMode()==MessageEditingMode::Plaintext)
    {
        return;
    }

    // Not while the recorder is up: two floating windows over one composer arm two Escape
    // shortcuts, and the emoji button is disabled for the recorder's pinned state anyway.
    if (pimpl->voiceOpen)
    {
        return;
    }

    if (pimpl->emojiDialogOpen)
    {
        // Already up: a click on a hovered gallery pins it rather than reopening it.
        if (pinned)
        {
            pinEmojiGallery();
        }
        return;
    }

    auto* frame=ensureEmojiGallery();
    if (frame==nullptr || frame->dialog().isNull())
    {
        return;
    }

    // A no-op unless the editing mode changed since the gallery was last filled. The search box
    // is cleared, and the grid rebuilt, by EmojiGalleryDialog::prepareToShow() -- which popupAt()
    // invokes after polishing and before measuring, the only moment at which the grid can be
    // rebuilt against its final, QSS-applied cell size.
    applyEmojiPackForCurrentMode();
    // Re-pushed on every open, not just at build time: the dialog is created once and kept, so a
    // host that seeded or updated the list in between (see setEmojiRecentIds()) would otherwise
    // not be reflected until the editor was rebuilt. setLeadingIconIds() ignores an unchanged list, so
    // the common case costs nothing.
    applyEmojiRecentIds();

    if (!frame->isVisible())
    {
        // Bottom-left corner of the frame onto the top-left corner of the button: the dialog
        // therefore unfolds UPWARD and to the RIGHT. The two-argument popupAt() also keeps the
        // whole frame on screen, which is what a popup anchored to a control the user just
        // clicked wants -- and it REMEMBERS this anchor, re-applying it if the frame's height
        // changes afterwards. That matters here specifically: ChatReactionGallery builds its rows
        // from its own showEvent(), so the very first popup of a freshly built gallery is
        // measured with an empty grid (~163px instead of ~389px) and would otherwise be placed
        // against that height and then grow off the bottom of the screen.
        const auto anchor=pimpl->emojiButton->mapToGlobal(pimpl->emojiButton->rect().topLeft())
                          -QPoint(0,EmojiGalleryGap);
        frame->popupAt(anchor,Qt::BottomLeftCorner);
    }
    // else: already up on screen, handed over from whichever composer in this window owned it a
    // moment ago (ensureEmojiGallery() -> claimEmojiGallery()) -- deliberately left exactly where
    // it is, including anywhere the user dragged it. This is the whole point of one shared dialog
    // per window: a chat switch never moves it.

    pimpl->emojiDialogOpen=true;
    pimpl->emojiDialogPinned=pinned;
    syncEmojiButtonChecked();

    if (!pinned)
    {
        startEmojiHoverPoll();
    }
}

//--------------------------------------------------------------------------

void MessageEditor::closeEmojiGallery()
{
    // The public, AbstractMessageEditor-facing close -- always a user-equivalent dismissal (a
    // host calls this to mean "the user is done here", e.g. the chat page itself being closed by
    // the user), so it clears the remembered pin same as the button's own close branch.
    closeEmojiGalleryInternal(true);
}

//--------------------------------------------------------------------------

void MessageEditor::closeEmojiGalleryInternal(bool userInitiated)
{
    // A close request also cancels a hover-open that has not fired yet, or the gallery would
    // reappear a moment after being dismissed.
    pimpl->emojiHoverOpenTimer->stop();
    stopEmojiHoverPoll();

    if (!pimpl->emojiDialogOpen)
    {
        // Nothing open FOR THIS EDITOR -- checked on emojiDialogOpen, not on whether
        // pimpl->emojiDialog is null, because in the shared-per-window gallery that pointer can
        // still be valid while pointing at a dialog a DIFFERENT composer in this window now owns
        // and is showing (see claimEmojiGallery()); closing it here would yank it out from under
        // them. Covers both "no dialog was ever built" and "one exists but is not mine right
        // now" uniformly. Still worth honouring a user-initiated unpin synchronously, since
        // there is no closed() round trip coming to do it for us. Also re-asserts the button: it
        // may have been left checked by its own unconditional toggle() with no dialog ever
        // created (e.g. a click while already in Plaintext).
        const auto wasPinned=pimpl->emojiDialogPinned;
        if (userInitiated)
        {
            pimpl->emojiDialogPinned=false;
        }
        syncEmojiButtonChecked();
        if (userInitiated && wasPinned)
        {
            emit emojiGalleryPinnedChanged(false);
        }
        return;
    }
    // From here this editor IS the current owner of an open dialog, so pimpl->emojiDialog is
    // current, not stale.
    if (pimpl->emojiDialog.isNull())
    {
        return;
    }
    // close() drives FloatingDialogFrame::closed() -- asynchronous, behind the frame's fade --
    // which clears emojiDialogOpen, re-asserts the button, and (only for a user-initiated close)
    // clears the pin and emits emojiGalleryPinnedChanged(false). See onEmojiGalleryClosed() and
    // emojiCloseProgrammatic just below.
    pimpl->emojiCloseProgrammatic=!userInitiated;
    pimpl->emojiDialog->close(false);
}

//--------------------------------------------------------------------------

void MessageEditor::onEmojiGalleryClosed()
{
    // Dispatched by the shared dialog's closed() handler (see buildEmojiGalleryDialog()) to
    // whichever editor currently owns it -- the single place the remembered pin is actually
    // cleared, and the single place the button goes back up, for every close path: the
    // title-bar X, Escape, an outside dismissal, the hover poll, closeEmojiGallery(). A claim
    // (a composer switch within the same window) never reaches here at all -- see
    // claimEmojiGallery(), which retargets the still-open dialog without closing it.
    const auto wasPinned=pimpl->emojiDialogPinned;
    const auto programmatic=pimpl->emojiCloseProgrammatic;
    pimpl->emojiCloseProgrammatic=false;
    pimpl->emojiDialogOpen=false;
    if (!programmatic)
    {
        pimpl->emojiDialogPinned=false;
    }
    stopEmojiHoverPoll();
    syncEmojiButtonChecked();
    // The one place focus returns to the text edit; see the emoji button's own clicked() handler
    // for why it deliberately does not do this itself.
    restoreEditorFocus();
    if (wasPinned && !pimpl->emojiDialogPinned)
    {
        emit emojiGalleryPinnedChanged(false);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::setEmojiGalleryPinned(bool pinned)
{
    if (pimpl->emojiDialogPinned==pinned)
    {
        return;
    }
    if (!pinned)
    {
        // Set synchronously, BEFORE closeEmojiGalleryInternal(): its own close() may be
        // asynchronous (behind the frame's fade), and by the time the closed() handler reads
        // emojiDialogPinned to decide whether to emit, it must already see the new value -- a
        // host push must never re-emit emojiGalleryPinnedChanged() back at itself.
        pimpl->emojiDialogPinned=false;
        closeEmojiGalleryInternal(false);
        return;
    }
    if (pimpl->emojiDialogOpen)
    {
        // Hover-opened and unpinned -- promote it exactly as a click would (sets the flag, stops
        // the hover poll, re-asserts the button). Delegated rather than set directly here: setting
        // the flag first would make pinEmojiGallery()'s own guard treat it as already pinned and
        // skip stopping the poll.
        pinEmojiGallery();
        return;
    }
    pimpl->emojiDialogPinned=true;
    // A hidden composer (e.g. a cached chat page that is not the current one) only remembers the
    // pin -- showEvent() opens it once the page actually comes up, where the emoji button has a
    // real screen position to anchor to.
    if (isVisible() && isEmojiButtonVisible())
    {
        openEmojiGallery(true);
    }
    else
    {
        syncEmojiButtonChecked();
    }
}

//--------------------------------------------------------------------------

void MessageEditor::setEmojiRecentIds(QStringList ids)
{
    // Capped on the way IN as well as on promotion: a host restoring a longer list from an older
    // build (or a hand-edited settings file) must not make the row wider than it can be.
    while (ids.size()>EmojiRecentsMax)
    {
        ids.removeLast();
    }
    if (pimpl->emojiRecentIds==ids)
    {
        return;
    }
    pimpl->emojiRecentIds=std::move(ids);
    // No emojiRecentIdsChanged() here -- this IS the host's own write, and echoing it back would
    // have a host that persists the signal write what it just read. See the signal's doc comment.
    applyEmojiRecentIds();
}

//--------------------------------------------------------------------------

QStringList MessageEditor::emojiRecentIds() const
{
    return pimpl->emojiRecentIds;
}

//--------------------------------------------------------------------------

void MessageEditor::applyEmojiRecentIds()
{
    if (pimpl->emojiDialog.isNull() || pimpl->emojiDialog->dialog().isNull())
    {
        // Built lazily and kept: openEmojiGallery() re-pushes on every open, so a list set before
        // the dialog exists is not lost.
        return;
    }
    pimpl->emojiDialog->dialog()->setRecentIds(pimpl->emojiRecentIds);
}

//--------------------------------------------------------------------------

void MessageEditor::promoteEmojiRecent(const QString& iconId)
{
    if (iconId.isEmpty())
    {
        return;
    }
    // Resolved against the pack the gallery is actually showing, so an id no pack here carries
    // never enters the list -- it could only ever be skipped by the row and would sit in the
    // host's store forever. This is also the only guard: the pick handler calls straight through.
    // The pack lives on the shared state now (see EmojiGallerySharedState::pack), not on this
    // editor, so it is reachable even when the dispatched-to owner never built the pack itself.
    auto* state=emojiGallerySharedState(window(),false);
    auto pack=(state!=nullptr && state->pack) ? state->pack : ReactionIconPacks::instance().defaultPack();
    if (!pack || pack->find(iconId)==nullptr)
    {
        return;
    }

    if (!pimpl->emojiRecentIds.isEmpty() && pimpl->emojiRecentIds.front()==iconId)
    {
        // Already the most recent -- nothing moves, so nothing is written back either.
        return;
    }

    pimpl->emojiRecentIds.removeAll(iconId);
    pimpl->emojiRecentIds.prepend(iconId);
    while (pimpl->emojiRecentIds.size()>EmojiRecentsMax)
    {
        pimpl->emojiRecentIds.removeLast();
    }

    applyEmojiRecentIds();
    emit emojiRecentIdsChanged(pimpl->emojiRecentIds);
}

//--------------------------------------------------------------------------

void MessageEditor::syncEmojiButtonChecked()
{
    // PINNED, not merely open: a hover-opened gallery deliberately leaves the button unchecked,
    // which is what makes "hovering an unchecked button shows the gallery" a stable rule rather
    // than one that disables itself the instant it fires -- and it stays unchecked through any
    // number of picks, because picking does not pin either. See isEmojiGalleryPinned().
    const auto checked=isEmojiGalleryPinned();
    if (pimpl->emojiButton->isChecked()!=checked)
    {
        pimpl->emojiButton->setChecked(checked);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::releaseEmojiGallery()
{
    pimpl->emojiHoverOpenTimer->stop();
    if (!pimpl->emojiDialogOpen)
    {
        return;
    }
    pimpl->emojiDialogOpen=false;
    stopEmojiHoverPoll();
    // Deliberately NOT touching the shared dialog's owner or visibility below -- see the comment
    // on the deferred check. The remembered pin (emojiDialogPinned) is untouched either way:
    // hiding is not a user dismissal.

    auto* state=emojiGallerySharedState(window(),false);
    if (state==nullptr || state->owner.data()!=this)
    {
        return;
    }

    // Still nominally "owned" by this now-hidden editor -- give another composer in the SAME
    // window a chance to claim it right back, synchronously, from its own showEvent(), which is
    // exactly what an ordinary chat switch does (QStackedWidget::setCurrentWidget() hides the old
    // page and shows the new one, in either order, within the same call). Only if nothing has
    // claimed it by the next event-loop turn -- e.g. the window itself losing focus, not a chat
    // switch -- does the dialog actually fade out.
    //
    // Anchored on `state` (parented to the window, so it outlives any one composer), not on
    // `this`: an editor destroyed before this turn elapses must not cancel the close and orphan
    // the dialog. Closes the shared dialog directly rather than going through
    // closeEmojiGalleryInternal() -- that function's own "nothing open for THIS editor" guard
    // reads emojiDialogOpen, already cleared above, so it would always take the early return and
    // silently no-op every hide instead of ever closing anything.
    QPointer<MessageEditor> self=this;
    QPointer<EmojiGallerySharedState> stateGuard=state;
    QTimer::singleShot(0,state,
        [self,stateGuard]()
        {
            if (stateGuard.isNull())
            {
                return;
            }
            // Still unclaimed, or reclaimed by THIS same editor -- proceed. Claimed by a
            // DIFFERENT composer in the meantime is the ordinary chat-switch hand-off this
            // deferral exists for: leave the dialog exactly as it is. A null owner (this editor
            // died before this turn ran, and nothing else ever claimed it) also proceeds, so a
            // destroyed composer never strands the dialog open.
            if (!stateGuard->owner.isNull() && stateGuard->owner.data()!=self.data())
            {
                return;
            }
            if (!self.isNull())
            {
                // Not a user dismissal -- FloatingDialogFrame::closed() below dispatches to
                // whichever editor still owns the shared state, which reads this flag to keep
                // the remembered pin and skip emojiGalleryPinnedChanged(). See
                // onEmojiGalleryClosed().
                self->pimpl->emojiCloseProgrammatic=true;
            }
            if (!stateGuard->dialog.isNull() && stateGuard->dialog->isVisible())
            {
                stateGuard->dialog->close(false);
            }
        }
    );
}

//--------------------------------------------------------------------------

IconTextButton* MessageEditor::micButton() const
{
    return pimpl->micButton;
}

//--------------------------------------------------------------------------

void MessageEditor::applyMicButtonVisibility()
{
    const auto allowed=isMicButtonVisible()
                       && isVoiceMessageEnabled()
                       && pimpl->editor->document()->isEmpty();

    // The document is empty for as long as the recorder is up (the text area is disabled), so
    // this cannot take the button from under the finger that is holding it -- but be explicit
    // about it, a hidden widget stops receiving the release that ends the gesture.
    pimpl->micButton->setVisible(allowed || pimpl->micHeld);
}

//--------------------------------------------------------------------------

void MessageEditor::updateMicButton()
{
    applyMicButtonVisibility();
    if ((!isMicButtonVisible() || !isVoiceMessageEnabled()) && !pimpl->voiceHeldByHost)
    {
        // Turning voice messages off must not strand a popup with nothing left to have opened it.
        // Not while a host has claimed this composer for a recording that is not this editor's
        // own (setVoiceRecordingHeldByHost()): that recording is the host's to end, not this
        // composer's -- voiceOpen is false in that case anyway, so this would be a no-op, but the
        // guard says so rather than relying on that being true.
        closeVoiceRecorder();
    }
    Layout::activateUpward(this);
}

//--------------------------------------------------------------------------

//! Take a composer button out of the mouse's reach while the voice recorder is open, or give it back.
static void lockVoiceButton(IconTextButton* button, bool lock)
{
    // Disabled alone is not enough: IconTextButton shows its hover in enterEvent() whether it is enabled or
    // not. With the mouse events off it gets no hover, no ripple, no press and no toggle at all, and the
    // events go on to the widget behind it.
    button->setEnabled(!lock);
    button->setAttribute(Qt::WA_TransparentForMouseEvents,lock);
    if (lock)
    {
        // A widget that stops receiving mouse events is never sent the Leave for a pointer that is on it, and
        // IconTextButton clears its hovered look only in leaveEvent(): send it, and drop WA_UnderMouse,
        // which the :hover pseudo-state reads.
        button->setAttribute(Qt::WA_UnderMouse,false);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(button,&leave);
        return;
    }

    // Given back. A short click leaves the pointer resting on the button, and nothing then moves it: Qt still
    // takes the pointer for being in the button, so no Enter is coming, not even when it moves inside, and the
    // button that was put out above would stay dark. Light it by an Enter of our own, but only if the pointer
    // is over the button and nothing else (the popup may still be fading over part of it).
    const auto global=QCursor::pos();
    auto* under=QApplication::widgetAt(global);
    if (button->isVisible() && under!=nullptr && (under==button || button->isAncestorOf(under)))
    {
        button->setAttribute(Qt::WA_UnderMouse,true);
        const auto local=button->mapFromGlobal(global);
        QEnterEvent enter(local,local,global);
        QCoreApplication::sendEvent(button,&enter);
    }
}

//--------------------------------------------------------------------------

FloatingVoiceRecorderDialog* MessageEditor::ensureVoiceRecorder()
{
    if (!pimpl->voiceDialog.isNull())
    {
        return pimpl->voiceDialog.data();
    }

    // Built at the press, not ahead of it: only then is this editor certainly in its real window,
    // which is what the popup is parented to. It is a top-level itself, so the parent is only
    // Qt ownership and the window it centres on.
    auto* win=window();
    if (win==nullptr)
    {
        return nullptr;
    }

    auto* frame=new FloatingVoiceRecorderDialog(win);
    frame->openDialog(false,false);
    if (frame->dialog().isNull())
    {
        frame->deleteLater();
        return nullptr;
    }
    pimpl->voiceDialog=frame;

    // Send and Cancel end the recording, so the popup goes with them. But not before the host has
    // heard of it: the host connected its own slots to these two signals after this one, when
    // voiceRecorderOpened() was emitted, and it must see sendRequested() BEFORE the closed()
    // that follows, or it would take a sent message for an abandoned one. Hence the extra turn of
    // the event loop.
    QPointer<MessageEditor> self=this;
    auto closeLater=[self]()
    {
        QTimer::singleShot(0,self.data(),
            [self]()
            {
                if (!self.isNull())
                {
                    self->closeVoiceRecorder();
                }
            }
        );
    };
    // Once pinned the recording no longer depends on the mouse being held, so the buttons that
    // open something over the composer must not do it: pressing the mic again would start a second
    // recording over the first, and the emoji gallery would be a second floating window. Both stay
    // locked until the popup closes, through Paused and Listening too -- see onVoiceRecorderClosed().
    connect(frame->dialog(),&AbstractVoiceRecorderDialog::pinned,this,
        [this]()
        {
            lockVoiceButton(pimpl->micButton,true);
            lockVoiceButton(pimpl->emojiButton,true);
        }
    );
    connect(frame->dialog(),&AbstractVoiceRecorderDialog::sendRequested,this,closeLater);
    connect(frame->dialog(),&AbstractVoiceRecorderDialog::cancelRequested,this,closeLater);

    // Every way the popup goes ends here: Send, Cancel, Escape and the title bar's close in
    // Paused, closeVoiceRecorder(), the editor being hidden.
    connect(frame,&FloatingDialogFrame::closed,this,&MessageEditor::onVoiceRecorderClosed);

    return frame;
}

//--------------------------------------------------------------------------

QPoint MessageEditor::voiceRecorderAnchor(Qt::Corner& corner) const
{
    corner=Qt::BottomRightCorner;

    // Bottom-right corner of the popup onto the top-right corner of the button, so it unfolds
    // upward and to the LEFT. The mic button sits at the right edge of the composer, so growing to
    // the left keeps the popup over the composer rather than hanging off the window, and its right
    // edge stays lined up with the button as it grows from Held to Paused.
    //
    // QRect::topRight() is the last pixel column, one short of the button's right edge, hence the width.
    if (pimpl->micButton==nullptr || !pimpl->micButton->isVisible())
    {
        return QPoint();
    }
    return pimpl->micButton->mapToGlobal(QPoint(pimpl->micButton->width(),0))
          -QPoint(0,VoiceRecorderGap);
}

//--------------------------------------------------------------------------

void MessageEditor::openVoiceRecorder()
{
    if (pimpl->voiceOpen)
    {
        return;
    }

    auto* frame=ensureVoiceRecorder();
    if (frame==nullptr || frame->dialog().isNull())
    {
        return;
    }

    // Two floating windows over one composer would arm two Escape shortcuts in the same window,
    // and Escape would then close neither -- see FloatingDialogFrame. Not a dismissal the user
    // asked for, so the emoji gallery's pin is remembered.
    closeEmojiGalleryInternal(false);

    auto dialog=frame->dialog();

    // The popup is kept between recordings, so it starts from scratch
    dialog->setState(AbstractVoiceRecorderDialog::State::Held);
    dialog->setElapsedMs(0);
    dialog->setPlaybackMs(0);
    dialog->setWaveform(QByteArray());
    dialog->setCropRange(0.0,1.0);
    dialog->setComment(QString());

    // The two-argument popupAt() keeps it on the screen and re-applies the anchor whenever the
    // popup resizes. See voiceRecorderAnchor() for what the anchor is and why.
    Qt::Corner corner;
    const auto anchor=voiceRecorderAnchor(corner);
    frame->popupAt(anchor,corner);

    pimpl->voiceOpen=true;

    // The emoji button is out of reach from the moment the popup is there. The mic button is the one that
    // is held, and its gesture needs the mouse: it is locked when the gesture ends, or at pinned().
    lockVoiceButton(pimpl->emojiButton,true);

    // The composer is not for typing while a message is being recorded. Held, the pointer is on
    // the mic button anyway, but Pinned it is free to wander onto the text area.
    pimpl->editorWasEnabled=pimpl->editor->isEnabled();
    pimpl->editor->setEnabled(false);

    emit voiceRecorderOpened(dialog.data());
}

//--------------------------------------------------------------------------

void MessageEditor::onVoiceRecorderClosed()
{
    const auto wasOpen=pimpl->voiceOpen;
    pimpl->voiceOpen=false;

    // A popup that went while the button was still down (closeVoiceRecorder(), a hide) ends the
    // gesture too: the release that is still to come has nothing to act on.
    pimpl->micHeld=false;
    pimpl->micButton->setChecked(false);
    hideMicDragProxy();

    // both were locked while the popup was open, see ensureVoiceRecorder() and openVoiceRecorder()
    // -- but not if a host has claimed this composer for a DIFFERENT recording in the meantime
    // (setVoiceRecordingHeldByHost()): that claim's own locks must survive this popup's close.
    if (!pimpl->voiceHeldByHost)
    {
        lockVoiceButton(pimpl->micButton,false);
        lockVoiceButton(pimpl->emojiButton,false);
    }

    if (!wasOpen)
    {
        return;
    }

    if (!pimpl->voiceHeldByHost)
    {
        pimpl->editor->setEnabled(pimpl->editorWasEnabled);
    }
    applyMicButtonVisibility();
    restoreEditorFocus();

    emit voiceRecorderClosed();
}

//--------------------------------------------------------------------------

bool MessageEditor::isVoiceRecorderOpen() const
{
    return pimpl->voiceOpen;
}

//--------------------------------------------------------------------------

AbstractVoiceRecorderDialog* MessageEditor::voiceRecorder() const
{
    if (!pimpl->voiceOpen || pimpl->voiceDialog.isNull())
    {
        return nullptr;
    }
    return pimpl->voiceDialog->dialog().data();
}

//--------------------------------------------------------------------------

void MessageEditor::closeVoiceRecorder()
{
    if (!pimpl->voiceOpen || pimpl->voiceDialog.isNull())
    {
        return;
    }

    // A programmatic close: FloatingDialogFrame refuses only what it does on the user's behalf
    // (Escape, the outside click), so this works on a dialog that is not closable while recording.
    // It is asynchronous behind the frame's fade; onVoiceRecorderClosed() finishes the job.
    pimpl->voiceDialog->close(false);
}

//--------------------------------------------------------------------------

void MessageEditor::setVoiceRecordingHeldByHost(bool enable)
{
    if (pimpl->voiceHeldByHost==enable)
    {
        return;
    }
    pimpl->voiceHeldByHost=enable;

    // While THIS editor's own popup is up, its own state machine (openVoiceRecorder(), the
    // pinned() lock above, onVoiceRecorderClosed()) already governs these locks. Touching them
    // here too would fight the press-and-hold gesture, which needs the mic button UNLOCKED to
    // receive the mouse events the gesture is built from in the first place.
    if (pimpl->voiceOpen)
    {
        return;
    }

    lockVoiceButton(pimpl->micButton,enable);
    lockVoiceButton(pimpl->emojiButton,enable);
    if (enable)
    {
        pimpl->editorWasEnabled=pimpl->editor->isEnabled();
        pimpl->editor->setEnabled(false);
    }
    else
    {
        pimpl->editor->setEnabled(pimpl->editorWasEnabled);
        applyMicButtonVisibility();
    }
}

//--------------------------------------------------------------------------

bool MessageEditor::isVoiceRecordingHeldByHost() const noexcept
{
    return pimpl->voiceHeldByHost;
}

//--------------------------------------------------------------------------

bool MessageEditor::handleMicButtonEvent(QEvent* event)
{
    switch (event->type())
    {
        case (QEvent::MouseButtonPress):
        case (QEvent::MouseButtonDblClick):
        {
            auto* mouseEvent=static_cast<QMouseEvent*>(event);
            if (mouseEvent->button()!=Qt::LeftButton)
            {
                return false;
            }

            // Taken even when there is nothing to do, so a disabled feature does not fall through
            // to IconTextButton and become a click on a button that is not there to be clicked.
            if (!isVoiceMessageEnabled() || pimpl->micHeld)
            {
                return true;
            }

            pimpl->micHeld=true;
            pimpl->micButton->setChecked(true);
            openVoiceRecorder();
            if (!pimpl->voiceOpen)
            {
                pimpl->micHeld=false;
                pimpl->micButton->setChecked(false);
                return true;
            }
            showMicDragProxy(mouseEvent->globalPosition().toPoint());
            return true;
        }

        case (QEvent::MouseMove):
        {
            if (!pimpl->micHeld)
            {
                return false;
            }

            // The button keeps the mouse for the whole gesture, so the popup never sees the
            // pointer and is told where it is.
            auto* mouseEvent=static_cast<QMouseEvent*>(event);
            const auto pos=mouseEvent->globalPosition().toPoint();
            moveMicDragProxy(pos);
            if (auto* dialog=voiceRecorder())
            {
                dialog->pointerMoved(pos);
            }
            return true;
        }

        case (QEvent::MouseButtonRelease):
        {
            auto* mouseEvent=static_cast<QMouseEvent*>(event);
            if (!pimpl->micHeld || mouseEvent->button()!=Qt::LeftButton)
            {
                return pimpl->micHeld;
            }

            pimpl->micHeld=false;
            pimpl->micButton->setChecked(false);
            hideMicDragProxy();

            // Over "continue" the popup pins itself, over "cancel" it cancels, anywhere else it
            // sends. What it decided, it reports through its own signals.
            const auto releasePos=mouseEvent->globalPosition().toPoint();
            if (auto* dialog=voiceRecorder())
            {
                dialog->pointerReleased(releasePos);
            }

            // The button had the mouse grabbed, so when it is let go over the popup -- another
            // top-level window -- the button is never sent the Leave for the pointer having gone:
            // Qt delivers it only once the pointer is next seen in the button's own window.
            // IconTextButton clears its hovered look only in leaveEvent(), so it would stay lit.
            // Send it by hand, and drop WA_UnderMouse, which the :hover pseudo-state reads.
            if (!pimpl->micButton->rect().contains(pimpl->micButton->mapFromGlobal(releasePos)))
            {
                pimpl->micButton->setAttribute(Qt::WA_UnderMouse,false);
                QEvent leave(QEvent::Leave);
                QCoreApplication::sendEvent(pimpl->micButton,&leave);
            }

            // The gesture is over. Until the popup is gone, however it ends, the button is not to be pressed
            // again: that is what its fade is, too.
            if (pimpl->voiceOpen)
            {
                lockVoiceButton(pimpl->micButton,true);
            }

            applyMicButtonVisibility();
            return true;
        }

        default:
            break;
    }
    return false;
}

//--------------------------------------------------------------------------

void MessageEditor::showMicDragProxy(const QPoint& globalPos)
{
    if (pimpl->micDragProxy.isNull())
    {
        // A top-level of its own, so it can follow the pointer over the recorder popup, which is
        // another top-level. Transparent for input, or it would take the release from the button.
        auto* proxy=new QLabel(nullptr,Qt::ToolTip | Qt::FramelessWindowHint
                                       | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
        proxy->setObjectName("micDragProxy");
        proxy->setAttribute(Qt::WA_TranslucentBackground,true);
        proxy->setAttribute(Qt::WA_ShowWithoutActivating,true);
        pimpl->micDragProxy=proxy;
    }

    // the button as it looks now: held, so highlighted
    pimpl->micDragProxy->setPixmap(pimpl->micButton->grab());
    pimpl->micDragProxy->adjustSize();
    moveMicDragProxy(globalPos);
    pimpl->micDragProxy->show();
}

//--------------------------------------------------------------------------

void MessageEditor::moveMicDragProxy(const QPoint& globalPos)
{
    if (pimpl->micDragProxy.isNull())
    {
        return;
    }
    const auto size=pimpl->micDragProxy->size();
    pimpl->micDragProxy->move(globalPos-QPoint(size.width()/2,size.height()/2));
}

//--------------------------------------------------------------------------

void MessageEditor::hideMicDragProxy()
{
    if (pimpl->micDragProxy.isNull())
    {
        return;
    }
    pimpl->micDragProxy->hide();
    // built afresh for the next press, it is small
    pimpl->micDragProxy->deleteLater();
    pimpl->micDragProxy=nullptr;
}

//--------------------------------------------------------------------------

void MessageEditor::hideEvent(QHideEvent* event)
{
    // Not a user dismissal -- the pin is remembered, and showEvent() re-opens (or, more often,
    // silently keeps) it. A composer being hidden is typically a chat page going into the cache,
    // not the user closing anything.
    releaseEmojiGallery();

    // Unlike the emoji gallery nothing is remembered: a recording of a composer that has gone is
    // not something to come back to, and the host is told, and discards it -- UNLESS a host has
    // claimed this popup as its own (setVoiceRecordingHeldByHost()), in which case the recording
    // is the host's to keep going regardless of what this composer is doing.
    if (!pimpl->voiceHeldByHost)
    {
        closeVoiceRecorder();
    }

    AbstractMessageEditor::hideEvent(event);

    emit editorHidden();
}

//--------------------------------------------------------------------------

void MessageEditor::showEvent(QShowEvent* event)
{
    AbstractMessageEditor::showEvent(event);

    // Emitted here, ahead of the emoji-gallery handling below (which has its own early returns),
    // so it fires for every show regardless of that logic's outcome.
    emit editorShown();

    if (!pimpl->emojiDialogPinned || pimpl->emojiDialogOpen)
    {
        return;
    }

    auto* state=emojiGallerySharedState(window(),false);
    if (state!=nullptr && !state->dialog.isNull() && state->dialog->isVisible())
    {
        // Already up on screen -- handed over from whichever composer in this window owned it a
        // moment ago (most often the chat page just switched away from). A synchronous claim: no
        // anchor is computed in this case (see openEmojiGallery()), so there is no layout-timing
        // reason to defer it, and deferring it would cost one visible frame of "not there yet".
        openEmojiGallery(true);
        return;
    }

    // Nothing visible to claim -- open one from scratch. Deferred one event-loop turn, never
    // inline: openEmojiGallery() anchors on emojiButton->mapToGlobal(), and inside showEvent()
    // the button has not necessarily been laid out at its final position yet. Same deferral rule
    // as warmEmojiGallery() and restoreEditorFocus().
    QPointer<MessageEditor> self=this;
    QTimer::singleShot(0,this,
        [self]()
        {
            if (!self.isNull() && self->isVisible() && self->isEmojiButtonVisible())
            {
                // Guards Plaintext and an already-open gallery itself.
                self->openEmojiGallery(true);
            }
        }
    );
}

//--------------------------------------------------------------------------

void MessageEditor::insertEmoji(const QString& reactionId)
{
    const auto mode=messageEditingMode();
    if (mode==MessageEditingMode::Plaintext)
    {
        // No markup to carry an image, and writing the bare character instead would contradict
        // the button being hidden in this mode. See AbstractMessageEditor::insertEmoji().
        return;
    }

    const auto* info=ReactionIconPacks::instance().iconInfo(reactionId);
    if (info==nullptr)
    {
        return;
    }

    auto cursor=pimpl->editor->textCursor();

    if (mode==MessageEditingMode::Markdown)
    {
        if (info->emojiCode.isEmpty())
        {
            return;
        }

        // The document IS markdown source here, so this is a plain text insert with no escaping
        // -- an emoji character carries no markdown meaning. No fenced-code guard either: in this
        // mode a fence is ordinary literal text (see convertCodeBlocksToText()), and an emoji
        // inside one is simply a character. Same shape as insertMentionText() above.
        auto format=cursor.charFormat();
        format.clearProperty(QTextFormat::IsAnchor);
        format.clearProperty(QTextFormat::AnchorHref);
        format.clearProperty(QTextFormat::AnchorName);
        // emojiText, not emojiCode -- Markdown mode's document IS the message source, so the
        // inserted character must be the colour-presentation form. See ReactionIconInfo::emojiText.
        cursor.insertText(info->emojiText,format);

        pimpl->editor->setTextCursor(cursor);
        pimpl->editor->setCurrentCharFormat(format);
    }
    else
    {
        if (!info->icon)
        {
            return;
        }

        const auto state=currentFormatState();
        // Same two refusals as insertLink(): a markdown image is not backslash-escaped the way
        // fence content is, so restoreCodeFences() could not safely unescape it; and inserting
        // into a mention would split one run into two halves sharing an href.
        if (state.codeBlock || state.insideMention)
        {
            return;
        }

        const auto src=emojiSrc(reactionId);
        const auto px=emojiInlineSizeForFont(pimpl->editor->font());
        const auto dpr=pimpl->editor->devicePixelRatioF();

        // MUST precede insertImage(). With no resource registered under this URL, Qt's image
        // handler falls back to its own broken-file placeholder AND caches that placeholder under
        // our URL via an addResource() call of its own -- after which the key is poisoned for the
        // document's whole life and a later registration is simply ignored.
        //
        // The pixmap is requested in DEVICE pixels (px*dpr) and comes back tagged with that
        // ratio, so it is crisp on a high-DPI screen; the logical size stays px, set on the format
        // below. Note SvgIcon always rasterizes at the PRIMARY screen's ratio, so a window living
        // on a differently-scaled secondary screen is a known, pre-existing rough edge.
        const auto devicePx=qRound(px*dpr);
        pimpl->editor->document()->addResource(
            QTextDocument::ImageResource,
            QUrl(src),
            info->icon->pixmap(QSize(devicePx,devicePx))
        );

        QTextImageFormat imgFmt;
        imgFmt.setName(src);
        // NEVER left empty: Qt's markdown writer substitutes the literal word "image" for an
        // empty ImageAltText, so an unset alt would export as "![image](whitem-emoji:...)" --
        // visible junk in any client that does not know the scheme. The emoji character is also
        // the right thing for such a client to show, and is what markdownToHtml() falls back to
        // when the icon is not locally available.
        imgFmt.setProperty(QTextFormat::ImageAltText,
                           info->emojiCode.isEmpty() ? info->iconId : info->emojiText);
        imgFmt.setWidth(px);
        imgFmt.setHeight(px);
        imgFmt.clearProperty(QTextFormat::IsAnchor);
        imgFmt.clearProperty(QTextFormat::AnchorHref);
        imgFmt.clearProperty(QTextFormat::AnchorName);

        // Captured BEFORE the insert, not derived after it: once insertImage() has run, the
        // cursor's char format IS the image format (ObjectType, ImageName, width, height), and
        // the next character typed would inherit every bit of it. Restoring the pre-insert format
        // is both shorter and harder to get wrong than clearing all of that back off by hand.
        auto continuation=cursor.charFormat();
        continuation.clearProperty(QTextFormat::IsAnchor);
        continuation.clearProperty(QTextFormat::AnchorHref);
        continuation.clearProperty(QTextFormat::AnchorName);

        // insertImage() on a cursor with a selection replaces it, exactly like insertText().
        cursor.insertImage(imgFmt);
        cursor.setCharFormat(continuation);

        pimpl->editor->setTextCursor(cursor);
        pimpl->editor->setCurrentCharFormat(continuation);
    }

    // Deliberately NOT finishFormatAction(): that ends in restoreEditorFocus(), and the emoji
    // picker -- unlike every dropdown this editor opens -- stays open across a pick, quite
    // possibly with a half-typed search in its box. Stealing focus back on every insert would
    // make searching and inserting mutually exclusive. Everything else finishFormatAction() does
    // is still wanted, so it is spelled out here instead.
    syncToolbarState();
    updatePlaceHolderText();
}

//--------------------------------------------------------------------------

void MessageEditor::onEmojiShortcodeTyped(const QString& shortcode, int position, int length)
{
    // Defensive, not load-bearing: applyEmojiShortcodeAutoReplace() already pushes false down to
    // pimpl->editor in Plaintext (and whenever the property itself is off), so
    // EnhancedTextEdit::applyEmojiShortcodeGuard() should never even emit this in either case.
    if (!isEmojiShortcodeAutoReplaceEnabled() || messageEditingMode()==MessageEditingMode::Plaintext)
    {
        return;
    }

    // emojiPackForCurrentMode(), NOT ReactionIconPacks::defaultPack() or the shared gallery
    // state's own pack cache (EmojiGallerySharedState::pack) -- this is the single source of
    // truth for "what can THIS mode actually insert" (in Markdown, a view that hides codeless
    // icons), and the shared cache may not even exist yet if the gallery has never been
    // warmed/opened in this window (a host may enable auto-replace without ever setting
    // emojiButtonVisible(true)).
    const auto pack=emojiPackForCurrentMode();
    if (!pack)
    {
        return;
    }
    const auto* info=pack->findByShortcode(shortcode);
    if (info==nullptr)
    {
        // Unknown shortcode -- leave m_shortcodeReplacement exactly as
        // applyEmojiShortcodeGuard() reset it right before emitting, so that guard's own
        // post-emit check sees "not armed" and lets the ':' type literally.
        return;
    }
    if (messageEditingMode()==MessageEditingMode::Wysiwyg && !info->icon)
    {
        // Same refusal insertEmoji() itself would apply -- do not arm a revert over a replace
        // that cannot actually happen.
        return;
    }

    const auto reactionId=ChatReactionId::make(info->iconId,pack->uri());

    // One edit block for the whole replacement -- select-and-insert, never the closing ':'
    // itself (the key that triggered this is still uncommitted; applyEmojiShortcodeGuard()
    // consumes it once this returns having armed a revert). beginEditBlock()/endEditBlock() are
    // DOCUMENT-level, not cursor-instance-level, so opening the block on this local cursor
    // correctly wraps the inserts insertEmoji() makes through pimpl->editor's OWN cursor object
    // just below.
    auto cursor=pimpl->editor->textCursor();
    cursor.beginEditBlock();

    cursor.setPosition(position);
    cursor.setPosition(position+length,QTextCursor::KeepAnchor);
    // Captured BEFORE the replace -- same reasoning insertEmoji() already documents for its own
    // `continuation`: once the selection is replaced with an image, the format at that position
    // IS the image format.
    const auto format=cursor.charFormat();
    // Makes this the ACTIVE selection: insertEmoji() reads pimpl->editor->textCursor() itself
    // and replaces whatever it finds selected there, exactly like QTextCursor::insertText().
    pimpl->editor->setTextCursor(cursor);

    insertEmoji(reactionId);

    // Measured, never assumed to be 1: Markdown mode inserts ReactionIconInfo::emojiText, which
    // is 1-3 UTF-16 units (a supplementary-plane surrogate pair, plus a possible VS16).
    const auto newLength=pimpl->editor->textCursor().position()-position;
    cursor.endEditBlock();

    // Case preserved exactly as typed (":Star:" stays ":Star:") -- shortcode is the raw
    // EnhancedTextEdit::ShortcodeCandidate::name, not case-folded (folding happens only at
    // findByShortcode()'s own lookup).
    const auto literal=QStringLiteral(":%1:").arg(shortcode);
    pimpl->editor->armEmojiShortcodeRevert(position,newLength,literal,format);

    // An auto-replaced emoji is a real pick -- belongs in the recents row and the host's
    // persisted store exactly like one chosen from the gallery.
    promoteEmojiRecent(info->iconId);
}

//--------------------------------------------------------------------------

void MessageEditor::setSpellChecker(AbstractSpellChecker* checker)
{
    pimpl->editor->setSpellChecker(checker);
}

//--------------------------------------------------------------------------

AbstractSpellChecker* MessageEditor::spellChecker() const
{
    return pimpl->editor->spellChecker();
}

//--------------------------------------------------------------------------

void MessageEditor::replaceSpellWord(const EnhancedTextEdit::SpellWord& word, const QString& replacement)
{
    auto cursor=pimpl->editor->textCursor();
    if (!pimpl->editor->selectSpellWord(cursor,word))
    {
        return;
    }

    // Keeps the word's own char format (bold/italic survive), same idiom as insertMentionText()
    // minus its anchor-clearing step -- the tokenizer that produced `word` never yields one
    // inside an anchor, see nonProseRuns().
    const auto format=cursor.charFormat();
    cursor.insertText(replacement,format);

    pimpl->editor->setTextCursor(cursor);
    pimpl->editor->setCurrentCharFormat(format);

    finishFormatAction();
}

//--------------------------------------------------------------------------

void MessageEditor::applySpellSuggestion(int index)
{
    if (index<0 || index>=pimpl->spellSuggestions.size())
    {
        return;
    }
    replaceSpellWord(pimpl->spellContextWord,pimpl->spellSuggestions.at(index));
}

//--------------------------------------------------------------------------

void MessageEditor::addSpellWordToDictionary()
{
    auto* checker=pimpl->editor->spellChecker();
    if (checker==nullptr || !pimpl->spellContextWord.isValid)
    {
        return;
    }
    // The checker is expected to emit AbstractSpellChecker::dictionaryChanged() itself, which is
    // what actually removes the squiggle -- no editor-side signal is added for persistence: the
    // host owns the checker, so persisting the word is the checker's job, and a relay signal here
    // would be a second, redundant contract for the same fact.
    checker->addToDictionary(pimpl->spellContextWord.text);
    restoreEditorFocus();
}

//--------------------------------------------------------------------------

void MessageEditor::ignoreSpellWord()
{
    auto* checker=pimpl->editor->spellChecker();
    if (checker==nullptr || !pimpl->spellContextWord.isValid)
    {
        return;
    }
    checker->ignoreWord(pimpl->spellContextWord.text);
    restoreEditorFocus();
}

//--------------------------------------------------------------------------

void MessageEditor::showContextMenu(const QPoint& pos)
{
    if (!isContextMenuEnabled())
    {
        return;
    }

    if (pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu=new DropdownMenu();
        // a context menu is expected to pop up instantly, like the native QTextEdit menu it
        // replaces, rather than visibly grow -- same reasoning as ChatMessage's own context
        // menu (uichatmessage.cpp).
        pimpl->contextMenu->setAnimationDurationMs(0);
        // deliberately no setTriggerWidget(): DropdownFrame::eventFilter() consumes a press on
        // the trigger widget to turn it into a toggle-close, which would swallow a *second*
        // right-click instead of letting the menu reopen at the new cursor position. A genuine
        // outside click already closes the frame and passes the press through, which is what
        // moves the caret on a plain left-click elsewhere in the editor.
        connect(pimpl->contextMenu,&DropdownMenu::itemTriggered,this,&MessageEditor::onContextMenuItemTriggered);
        // Checkable rows (the Stage 5a Formatting submenu) emit itemToggled(id,checked), not
        // itemTriggered(id) -- see DropdownMenu::onItemToggled(). Without this connection a
        // ContextMenuHandler that adds its own checkable row would also get no callback at all
        // for it, a pre-existing gap closed here as a drive-by fix.
        connect(pimpl->contextMenu,&DropdownMenu::itemToggled,this,&MessageEditor::onContextMenuItemToggled);
    }
    else if (pimpl->contextMenu->isOpen())
    {
        pimpl->contextMenu->closeDropdown(true);
    }

    std::vector<MenuItem> items;

    // task-spellcheck.md. Spelling rows LAST, below Clear -- Cut/Copy/Paste/Formatting/Select
    // all/Clear are the rows reached on every right-click regardless of what's under the cursor,
    // so they stay first; the spelling section is conditional on the click landing near a word and
    // is appended below via `spelling` once the rest of the menu is built (see near the bottom of
    // this function).
    //
    // The word is taken from the MOUSE position, not the caret: right-clicking a misspelling is
    // the gesture, and Qt does not move the caret on a right-press. Nothing in the document is
    // SELECTED while the menu is open -- a visible selection sitting under a non-modal popup is
    // destroyed by the first caret move and worse than useless, the same reasoning
    // mentionRequested()'s own doc comment records -- the range is remembered in pimpl instead and
    // selected only when a fix is actually applied (see replaceSpellWord()).
    std::vector<MenuItem> spelling;
    pimpl->spellContextWord=EnhancedTextEdit::SpellWord{};
    pimpl->spellSuggestions.clear();
    if (isSpellCheckMenuItemVisible() && pimpl->editor->spellChecker()!=nullptr)
    {
        auto* checker=pimpl->editor->spellChecker();
        if (isSpellCheckEnabled() && checker->isReady() && !pimpl->editor->isReadOnly())
        {
            const auto cursor=pimpl->editor->cursorForPosition(pos);
            const auto word=pimpl->editor->spellWordAt(cursor.position());

            // Suppressed while a SELECTION spans anything other than exactly this one word: the
            // three rows below act on a single word (suggest/add/ignore), which is ambiguous the
            // moment more than one word is selected -- there is no such thing as a multi-word
            // dictionary entry (the tokenizer splits every check() call on whitespace before it
            // ever runs, so a phrase added here could never be matched again). A selection made
            // by double-clicking the misspelling itself still matches this word's own span
            // exactly, so that case is deliberately still allowed through.
            const auto selection=pimpl->editor->textCursor();
            const bool selectionAllowsWordActions=!selection.hasSelection()
                || (selection.selectionStart()==word.position
                    && selection.selectionEnd()==word.position+word.length);

            if (word.isValid && selectionAllowsWordActions
                && checker->check(word.text)==SpellCheckVerdict::Misspelled)
            {
                pimpl->spellContextWord=word;
                pimpl->spellSuggestions=checker->suggestions(word.text,MaxSpellSuggestions);

                const auto count=qMin(static_cast<int>(pimpl->spellSuggestions.size()),MaxSpellSuggestions);
                for (int i=0; i<count; ++i)
                {
                    spelling.push_back(MenuItem(
                        static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst)+i,
                        pimpl->spellSuggestions.at(i)
                    ));
                }
                if (count==0)
                {
                    // A section row rather than a plain one: DropdownMenu renders a
                    // section+disabled row as an inert label -- exactly an unclickable "nothing
                    // to offer" line.
                    spelling.push_back(MenuItem::section(
                        static_cast<int>(MessageEditorMenuAction::SpellNoSuggestions),
                        tr("No suggestions")
                    ));
                    spelling.back().isEnabled=false;
                }

                if (checker->canAddToDictionary())
                {
                    spelling.push_back(MenuItem(
                        static_cast<int>(MessageEditorMenuAction::AddToDictionary),
                        tr("Add to dictionary"),
                        menuIcon(QStringLiteral("addToDictionary"),pimpl->editor)
                    ));
                }
                spelling.push_back(MenuItem(
                    static_cast<int>(MessageEditorMenuAction::IgnoreWord),
                    tr("Ignore word"),
                    menuIcon(QStringLiteral("ignoreWord"),pimpl->editor)
                ));
                spelling.push_back(MenuItem::separator());
            }
        }

        // Offered whether or not the click landed on a misspelling -- turning the feature off is
        // most wanted precisely when the underlines are wrong about correct text.
        spelling.push_back(MenuItem::checkable(
            static_cast<int>(MessageEditorMenuAction::SpellCheckEnabled),
            tr("Check spelling"),
            isSpellCheckEnabled(),
            menuIcon(QStringLiteral("spellCheck"),pimpl->editor)
        ));
    }

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Cut),
        tr("Cut"),
        menuIcon(QStringLiteral("cut"),pimpl->editor)
    ));
    items.back().isEnabled=hasSelection() && !pimpl->editor->isReadOnly();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Copy),
        tr("Copy"),
        menuIcon(QStringLiteral("copy"),pimpl->editor)
    ));
    items.back().isEnabled=hasSelection();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Paste),
        tr("Paste"),
        menuIcon(QStringLiteral("paste"),pimpl->editor)
    ));
    items.back().isEnabled=canPasteFromClipboard();

    items.push_back(MenuItem::separator());

    // Stage 6: TOP-LEVEL row, not inside the Formatting submenu below, and for that reason
    // checked/built regardless of messageEditingMode() -- the submenu is built only in Wysiwyg
    // (see the comment just below), while a mention (its plain "@username" form most of all) is
    // valid in every mode. Hidden unless a host opts in (setMentionMenuItemVisible()), so no
    // existing menu changes shape by default.
    if (isMentionMenuItemVisible())
    {
        items.push_back(MenuItem(
            static_cast<int>(MessageEditorMenuAction::Mention),
            tr("Mention someone"),
            menuIcon(QStringLiteral("mention"),pimpl->editor)
        ));
        items.back().isEnabled=!pimpl->editor->isReadOnly() && canInsertMentionAtCursor();

        items.push_back(MenuItem::separator());
    }

    // Formatting submenu -- only in MessageEditingMode::Wysiwyg (Stage 5a decision: formatting
    // is WYSIWYG-only). Fifteen greyed rows in Markdown/Plaintext mode would be worse than no
    // submenu at all; the persistently-visible toolbar greys its formatting half instead so its
    // width stays stable, but a context menu has no such stable-width concern, so it simply
    // omits the row. A ContextMenuHandler that wants formatting gone entirely erases this one
    // "Formatting" id rather than fifteen individual ones.
    if (messageEditingMode()==MessageEditingMode::Wysiwyg)
    {
        const auto state=currentFormatState();
        const bool canFormat=hasSelection() || !pimpl->editor->isReadOnly();

        std::vector<MenuItem> formatting;

        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Bold),tr("Bold"),state.bold,menuIcon(QStringLiteral("bold"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Italic),tr("Italic"),state.italic,menuIcon(QStringLiteral("italic"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Underline),tr("Underline"),state.underline,menuIcon(QStringLiteral("underline"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Strikethrough),tr("Strikethrough"),state.strikeOut,menuIcon(QStringLiteral("strikethrough"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::InlineCode),tr("Inline code"),state.inlineCode,menuIcon(QStringLiteral("code"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;

        formatting.push_back(MenuItem::separator());

        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Heading1),tr("Heading 1"),state.headingLevel==1,menuIcon(QStringLiteral("heading1"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.back().group=1;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Heading2),tr("Heading 2"),state.headingLevel==2,menuIcon(QStringLiteral("heading2"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.back().group=1;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Heading3),tr("Heading 3"),state.headingLevel==3,menuIcon(QStringLiteral("heading3"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.back().group=1;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::HeadingNormal),tr("Normal text"),state.headingLevel==0,menuIcon(QStringLiteral("normalText"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.back().group=1;

        formatting.push_back(MenuItem::separator());

        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::BulletList),tr("Bulleted list"),state.bulletList,menuIcon(QStringLiteral("list"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::NumberedList),tr("Numbered list"),state.numberedList,menuIcon(QStringLiteral("listNumbers"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem::checkable(static_cast<int>(MessageEditorMenuAction::Blockquote),tr("Blockquote"),state.blockquote,menuIcon(QStringLiteral("blockquote"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::CodeBlock),tr("Code block"),menuIcon(QStringLiteral("codeBlock"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;
        formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::Table),tr("Table"),menuIcon(QStringLiteral("table"),pimpl->editor)));
        formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::HorizontalRule),tr("Horizontal rule"),menuIcon(QStringLiteral("horizontalRule"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;

        formatting.push_back(MenuItem::separator());

        formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::Link),tr("Insert link"),menuIcon(QStringLiteral("link"),pimpl->editor)));
        formatting.back().isEnabled=canFormat && !state.codeBlock;
        // "Remove link" appears only while the caret is inside an existing link -- unlike the
        // rest of this submenu, omitted rather than greyed: a context menu is one-shot and has
        // no stable-width concern the way the persistent toolbar does (see that comment on
        // MessageEditorToolbar::setFormattingEnabled()).
        if (state.insideLink)
        {
            formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::RemoveLink),tr("Remove link"),menuIcon(QStringLiteral("removeLink"),pimpl->editor)));
        }

        formatting.push_back(MenuItem::separator());

        formatting.push_back(MenuItem(static_cast<int>(MessageEditorMenuAction::ClearFormatting),tr("Clear formatting"),menuIcon(QStringLiteral("clearFormatting"),pimpl->editor)));
        formatting.back().isEnabled=canFormat;

        items.push_back(MenuItem::submenu(
            static_cast<int>(MessageEditorMenuAction::Formatting),
            tr("Formatting"),
            std::move(formatting),
            menuIcon(QStringLiteral("formatting"),pimpl->editor)
        ));

        items.push_back(MenuItem::separator());
    }

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::SelectAll),
        tr("Select all"),
        menuIcon(QStringLiteral("selectAll"),pimpl->editor)
    ));
    items.back().isEnabled=!isEmpty();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Clear),
        tr("Clear"),
        menuIcon(QStringLiteral("clear"),pimpl->editor)
    ));
    items.back().isEnabled=!isEmpty();

    if (!spelling.empty())
    {
        items.push_back(MenuItem::separator());
        for (auto& item : spelling)
        {
            items.push_back(std::move(item));
        }
    }

    if (contextMenuHandler())
    {
        contextMenuHandler()(items);
    }

    if (items.empty())
    {
        return;
    }

    pimpl->contextMenu->setItems(std::move(items));
    pimpl->contextMenu->popupAt(pimpl->editor->mapToGlobal(pos));
}

//--------------------------------------------------------------------------

void MessageEditor::onContextMenuItemTriggered(int id)
{
    switch (static_cast<MessageEditorMenuAction>(id))
    {
        case (MessageEditorMenuAction::Cut):
        {
            cut();
            break;
        }

        case (MessageEditorMenuAction::Copy):
        {
            copy();
            break;
        }

        case (MessageEditorMenuAction::Paste):
        {
            paste();
            break;
        }

        case (MessageEditorMenuAction::SelectAll):
        {
            selectAll();
            break;
        }

        case (MessageEditorMenuAction::Clear):
        {
            // Not clear(): that also purges the undo/redo history (QTextEdit::clear()'s
            // documented behavior), which is right for the "message sent, wipe everything"
            // call sites (ChatPageBottom::acceptOperation(), FileUploadWidget::reset()) but
            // wrong for this menu action, which the user expects to be undoable like any other
            // edit.
            pimpl->editor->clearUndoable();
            break;
        }

        case (MessageEditorMenuAction::CodeBlock):
        {
            applyCodeBlock();
            break;
        }

        case (MessageEditorMenuAction::Table):
        {
            // The menu's Table row is a single click, unlike the toolbar's own rows×columns
            // picker submenu -- a small fixed default keeps the menu a one-step action.
            applyTable(2,2);
            break;
        }

        case (MessageEditorMenuAction::HorizontalRule):
        {
            applyHorizontalRule();
            break;
        }

        case (MessageEditorMenuAction::Link):
        {
            onLinkButtonRequested();
            break;
        }

        case (MessageEditorMenuAction::RemoveLink):
        {
            removeLink();
            break;
        }

        case (MessageEditorMenuAction::Mention):
        {
            onMentionButtonRequested();
            break;
        }

        case (MessageEditorMenuAction::ClearFormatting):
        {
            applyClearFormatting();
            break;
        }

        case (MessageEditorMenuAction::AddToDictionary):
        {
            addSpellWordToDictionary();
            break;
        }

        case (MessageEditorMenuAction::IgnoreWord):
        {
            ignoreSpellWord();
            break;
        }

        case (MessageEditorMenuAction::SpellNoSuggestions):
        {
            // The inert "No suggestions" row -- isEnabled=false already stops DropdownMenu from
            // triggering it, this case exists only so it is not silently relayed as a consumer id
            // through the default branch below.
            break;
        }

        default:
        {
            // A RANGE rather than eight case labels: the band is contiguous by construction
            // (MessageEditorMenuAction::SpellSuggestionFirst..SpellSuggestionLast), so raising
            // MessageEditor::MaxSpellSuggestions later needs no new enumerator and no new case.
            if (id>=static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst)
                && id<=static_cast<int>(MessageEditorMenuAction::SpellSuggestionLast))
            {
                applySpellSuggestion(id-static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst));
                break;
            }
            emit contextMenuItemTriggered(id);
            break;
        }
    }
}

//--------------------------------------------------------------------------

void MessageEditor::onContextMenuItemToggled(int id, bool checked)
{
    switch (static_cast<MessageEditorMenuAction>(id))
    {
        case (MessageEditorMenuAction::Bold):
        {
            applyBold(checked);
            break;
        }

        case (MessageEditorMenuAction::Italic):
        {
            applyItalic(checked);
            break;
        }

        case (MessageEditorMenuAction::Underline):
        {
            applyUnderline(checked);
            break;
        }

        case (MessageEditorMenuAction::Strikethrough):
        {
            applyStrikethrough(checked);
            break;
        }

        case (MessageEditorMenuAction::InlineCode):
        {
            applyInlineCode(checked);
            break;
        }

        case (MessageEditorMenuAction::BulletList):
        {
            applyBulletList(checked);
            break;
        }

        case (MessageEditorMenuAction::NumberedList):
        {
            applyNumberedList(checked);
            break;
        }

        case (MessageEditorMenuAction::Blockquote):
        {
            applyBlockquote(checked);
            break;
        }

        case (MessageEditorMenuAction::Heading1):
        {
            // Group exclusivity also emits itemToggled(other,false) for the row being unchecked
            // -- act only on the row actually becoming checked (same pattern as
            // MessageEditorToolbar's own heading/mode menu handlers).
            if (checked)
            {
                applyHeading(1);
            }
            break;
        }

        case (MessageEditorMenuAction::Heading2):
        {
            if (checked)
            {
                applyHeading(2);
            }
            break;
        }

        case (MessageEditorMenuAction::Heading3):
        {
            if (checked)
            {
                applyHeading(3);
            }
            break;
        }

        case (MessageEditorMenuAction::HeadingNormal):
        {
            if (checked)
            {
                applyHeading(0);
            }
            break;
        }

        case (MessageEditorMenuAction::SpellCheckEnabled):
        {
            setSpellCheckEnabled(checked);
            break;
        }

        default:
        {
            emit contextMenuItemToggled(id,checked);
            break;
        }
    }
}

//--------------------------------------------------------------------------

}
