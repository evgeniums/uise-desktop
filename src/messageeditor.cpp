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
#include <memory>
#include <vector>

#include <QKeyEvent>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextList>
#include <QTextTable>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QFontDatabase>
#include <QSyntaxHighlighter>
#include <QFontMetricsF>
#include <QRegularExpression>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QTimer>
#include <QBoxLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>
#include <uise/desktop/markdownrenderer.hpp>
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
        cursor.insertText(QString(QChar::LineSeparator));
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
    return collapseBlankRuns(restoreCodeFences(clone->toMarkdown()));
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

        explicit MessageEditorHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
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
         * Runs LAST so a link inside a blockquote reads as a link rather than as quoted text --
         * later setFormat() calls win over earlier ones on an overlapping range.
         */
        void highlightLinks()
        {
            if (!m_linkColor.isValid() && !m_linkUnderline)
            {
                return;
            }

            const auto block=currentBlock();
            const auto blockStart=block.position();
            for (auto it=block.begin(); !it.atEnd(); ++it)
            {
                const auto fragment=it.fragment();
                if (!fragment.isValid() || !fragment.charFormat().isAnchor())
                {
                    continue;
                }

                QTextCharFormat format;
                if (m_linkColor.isValid())
                {
                    format.setForeground(m_linkColor);
                }
                // Set either way, so linkUnderline:false also strips an underline an imported
                // document happened to carry.
                format.setFontUnderline(m_linkUnderline);

                setFormat(fragment.position()-blockStart,fragment.length(),format);
            }
        }

        QColor m_blockquoteColor;
        QColor m_codeBlockColor;
        QColor m_linkColor;
        bool m_linkUnderline=false;
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
    m_highlighter=new MessageEditorHighlighter(document());

    // Right-click is handled by MessageEditor's own DropdownMenu (see showContextMenu()),
    // not Qt's stock createStandardContextMenu() -- its Paste entry is driven by canPaste(),
    // which would report nothing-to-paste for an attachment payload (see
    // canInsertFromMimeData() below).
    setContextMenuPolicy(Qt::CustomContextMenu);
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

    // Tab/Shift+Tab mean one of two things, and a literal tab character is neither of them.
    //
    // INSIDE A TABLE they move between cells. Qt does NOT do this on its own: neither
    // QWidgetTextControl nor QTextEdit has any NextCell/PreviousCell handling, so Tab would just
    // insert a tab into the current cell and there would be no keyboard way across a table at
    // all -- only clicking, or walking the arrow keys through every character.
    //
    // OUTSIDE ONE they are an indent gesture, handed to MessageEditor as a signal because what a
    // step means depends on the editing mode -- see indentStepRequested(). The key is consumed
    // either way; a tab character never reaches the document, which is the point (a leading tab
    // makes markdown read the line as an indented code block, and a mid-line tab is collapsed to
    // one space by the time the message is rendered as HTML).
    if ((event->key()==Qt::Key_Tab || event->key()==Qt::Key_Backtab)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
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

    QTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::focusInEvent(QFocusEvent* event)
{
    QTextEdit::focusInEvent(event);
    emit activated();
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

void EnhancedTextEdit::applyTabStopDistance()
{
    // Qt's default is a flat 80px that ignores the font entirely; at this widget's space width
    // that is more than twenty spaces per tab. Measured against the font in force instead, so a
    // tab lines up with what DefaultTabStopSpaces actually promises at any font size.
    setTabStopDistance(DefaultTabStopSpaces*QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
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

    // Stage 6: Mention stays hidden and unconnected -- no toolbar API change is needed to wire
    // it up when that stage lands.

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
            updateArrangementForContent();
            emit textChanged();
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
        &EnhancedTextEdit::editPreviousRequested,
        this,
        &AbstractMessageEditor::editPreviousRequested
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
                case (TextFormat::Plain): return plainTextKeepingIndent(pimpl->editor->document());
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
                case (TextFormat::Plain): return plainTextKeepingIndent(cursor);
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

bool MessageEditor::canPasteFromClipboard() const
{
    return pimpl->editor->canPasteFromClipboard();
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
    // Before the stretch (last) only -- the expand button belongs to the leading group.
    pimpl->trailingLayout->insertWidget(qMax(0,pimpl->trailingLayout->count()-1),widget);
}

//--------------------------------------------------------------------------

void MessageEditor::applyArrangement()
{
    const auto stacked=isStackedArrangement();

    // The whole switch. leadingFrame | text | trailingFrame stays horizontal forever; only the
    // direction of each frame's OWN layout changes, so no widget is ever reparented or moved
    // between layouts and the two orders cannot drift apart.
    //
    // BottomToTop rather than TopToBottom because it is what "the left widget becomes the bottom
    // widget" means: the group's first member, leftmost in the row, ends up lowest in the column
    // -- nearest the text area's bottom edge, where it already was. It also puts each layout's
    // trailing stretch at the TOP, which is what packs the buttons downward.
    const auto direction=stacked ? QBoxLayout::BottomToTop : QBoxLayout::LeftToRight;
    pimpl->leadingLayout->setDirection(direction);
    pimpl->trailingLayout->setDirection(direction);

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
    pimpl->editor->setNewLineOnEnter(!isFinishOnEnter());
}

//--------------------------------------------------------------------------

void MessageEditor::setPlaceHolderText(const QString& text)
{
    pimpl->editor->setPlaceholderText(text);
}

//--------------------------------------------------------------------------

void MessageEditor::updateExpanded()
{
    pimpl->editor->setExpandedEnabled(isExpanded());
    pimpl->toolbar->setVisible(isExpanded());

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
    state.insideLink=cf.isAnchor();
    state.insideTable=cursor.currentTable()!=nullptr;

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
    else if (!cursor.charFormat().isAnchor())
    {
        // A selection exists but the caret's own end of it isn't inside a link -- nothing
        // reliable to remove; same kind of guard applyTableAction() uses outside a table.
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
            if (fragmentEnd<=start || fragmentStart>=end || !fragment.charFormat().isAnchor())
            {
                continue;
            }

            // clear-then-setCharFormat, not merge{anchor=false}: measured that a merge alone --
            // with or without also merging an empty href -- leaves "[LINK]()" (empty-URL
            // markdown, still a link). Per fragment, not one blanket format over the whole
            // range, so mixed bold/italic runs inside the link survive (measured: an anchor
            // applied over an already-formatted selection splits into same-href fragments with
            // different weight). Also clears the colour Qt's own importer bakes onto an anchor
            // (setMarkdown()'s blue), so removed link text does not stay coloured.
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
    const auto position=cursor.position();

    // QTextCursor::charFormat() with no selection is documented to return the format of the
    // character immediately PRECEDING position() -- matched below by requiring start<position,
    // so the fragment this finds is exactly the one charFormat() itself just consulted.
    if (!cursor.charFormat().isAnchor())
    {
        return false;
    }
    const auto href=cursor.charFormat().anchorHref();

    struct FragmentSpan
    {
        int start;
        int end;
        bool isLink;
    };
    std::vector<FragmentSpan> spans;
    int currentIndex=-1;

    // Links do not cross block boundaries in this editor (nothing here ever inserts one that
    // does), so the walk is block-local.
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
        spans.push_back(FragmentSpan{start,end,format.isAnchor() && format.anchorHref()==href});

        if (currentIndex==-1 && position>start && position<=end)
        {
            currentIndex=static_cast<int>(spans.size())-1;
        }
    }

    if (currentIndex==-1 || !spans[static_cast<std::size_t>(currentIndex)].isLink)
    {
        return false;
    }

    // Extend outward while the immediate neighbour is both a link AND the SAME href, and
    // contiguous -- a different href never merges (measured), so this cannot walk past one link
    // into an adjacent one.
    auto first=static_cast<std::size_t>(currentIndex);
    while (first>0 && spans[first-1].isLink && spans[first-1].end==spans[first].start)
    {
        --first;
    }

    auto last=static_cast<std::size_t>(currentIndex);
    while (last+1<spans.size() && spans[last+1].isLink && spans[last+1].start==spans[last].end)
    {
        ++last;
    }

    cursor.setPosition(spans[first].start);
    cursor.setPosition(spans[last].end,QTextCursor::KeepAnchor);
    return true;
}

//--------------------------------------------------------------------------

void MessageEditor::onLinkButtonRequested()
{
    const auto state=currentFormatState();
    if (state.codeBlock)
    {
        // Refused: an anchor's href is not backslash-escaped by Qt's markdown writer the way
        // fence content is, so restoreCodeFences() cannot safely unescape it (measured) -- same
        // gate insertLink() itself applies, kept here too so the dialog is never opened for a
        // link that could not be inserted anyway.
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

    if (currentFormatState().codeBlock)
    {
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

        case (MessageEditorMenuAction::ClearFormatting):
        {
            applyClearFormatting();
            break;
        }

        default:
        {
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

        default:
        {
            emit contextMenuItemToggled(id,checked);
            break;
        }
    }
}

//--------------------------------------------------------------------------

}
