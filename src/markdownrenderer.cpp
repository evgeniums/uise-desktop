/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file src/markdownrenderer.cpp
*
*  Implements markdownToHtml() and markdownToPlainText() (task-message-formatting-plan.md,
*  Stage 2).
*
*  markdownToHtml() does NOT parse markdown itself -- QTextDocument::setMarkdown() (Qt's own
*  md4c-based importer, MarkdownDialectGitHub) does that. What this file adds is the walk that
*  turns the resulting QTextDocument back into our OWN small, allowlisted HTML vocabulary rather
*  than trusting QTextDocument::toHtml() (which bakes in `-qt-*`/font-family/font-size inline
*  styles that would defeat resources/style/messagetext.css) or the raw markdown source (which
*  may contain inline HTML the GitHub dialect happily admits).
*
*  Construct table -- what each markdown/HTML construct becomes on the way through:
*
*    | Construct                     | Recovered from                          | Emitted            |
*    |--------------------------------|-----------------------------------------|--------------------|
*    | Heading                        | QTextBlockFormat::headingLevel()        | <h1>..<h6>         |
*    | Paragraph                      | (default)                               | <p>                |
*    | Fenced/indented code block     | QTextFormat::BlockCodeLanguage/Fence     | <pre class=><code> |
*    | Blockquote (nested)            | QTextFormat::BlockQuoteLevel             | nested <blockquote>|
*    | Thematic break                 | QTextFormat::BlockTrailingHorizontalRulerWidth | <hr/>       |
*    | List item                      | QTextBlock::textList()/QTextListFormat   | <ul>/<ol> + <li>   |
*    | Task list item                 | QTextBlockFormat::marker()               | <li> + a checkbox glyph |
*    | Table                          | QTextTable (frame walk, not flat blocks) | <table>/<tr>/<td>  |
*    | Bold / italic / strikethrough  | QTextCharFormat::fontWeight/Italic/StrikeOut | <b>/<i>/<s>    |
*    | Underline                      | QTextCharFormat::fontUnderline() (not on an anchor) | <u> |
*    | Inline code                    | QTextCharFormat::fontFixedPitch()        | <code>             |
*    | Link (explicit or autolink)    | QTextCharFormat::isAnchor()/anchorHref() | <a href=> (scheme-checked) |
*    | Image                          | QTextCharFormat::isImageFormat()         | never <img> -- escaped text, optionally linked |
*    | Soft/hard line break inside a block | QChar::LineSeparator / '\n'          | <br/>              |
*
*  Everything not in that table -- any raw tag the GitHub dialect's inline-HTML support admitted,
*  any attribute, any script -- is simply never looked at: the walk only ever asks the document
*  for the handful of properties above and only ever writes the handful of tags above. That is
*  the entire sanitization model.
*
*/

/****************************************************************************/

#include <vector>

#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextList>
#include <QTextFormat>
#include <QUrl>
#include <QFont>

#include <uise/desktop/markdownrenderer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

/******************************* Soft-newline preprocessor ******************************/

//! A markdown fence opener/closer: 3+ identical '`'/'~' characters, at most 3 leading spaces of
//! indentation (CommonMark: 4+ leading spaces would make it an INDENTED code block instead).
bool matchFence(const QString& line, QChar& fenceChar, int& fenceLen)
{
    int leading=0;
    while (leading<line.size() && leading<4 && line.at(leading)==QLatin1Char(' '))
    {
        ++leading;
    }
    if (leading>=4 || leading>=line.size())
    {
        return false;
    }
    auto c=line.at(leading);
    if (c!=QLatin1Char('`') && c!=QLatin1Char('~'))
    {
        return false;
    }
    int n=0;
    int pos=leading;
    while (pos<line.size() && line.at(pos)==c)
    {
        ++n;
        ++pos;
    }
    if (n<3)
    {
        return false;
    }
    fenceChar=c;
    fenceLen=n;
    return true;
}

bool isIndentedCodeLine(const QString& line)
{
    return line.startsWith(QStringLiteral("    ")) || line.startsWith(QLatin1Char('\t'));
}

//! Whether a line OPENS a new block-level construct: an ATX heading, a blockquote, a bullet or
//! ordered list item, a thematic break, or a setext underline.
//!
//! preserveChatLineBreaks() must never merge such a line into the previous one. Merging is what
//! gives a chat message its "one newline = one visible line break" behaviour, but a block opener
//! only counts as one at the START of a line -- swallow the newline in front of it and markdown
//! stops seeing a construct at all. That is what turned every multi-line list into a single item
//! whose 2nd and 3rd lines were literal "- beta" text joined by <br/>, and it did the same to
//! consecutive headings, blockquotes and thematic breaks.
bool startsBlockConstruct(const QString& line)
{
    // ALL leading whitespace is skipped, deliberately not just the three spaces CommonMark
    // allows before a TOP-LEVEL block. Inside a list, a nested item is indented relative to its
    // parent's content column, so Qt's own toMarkdown() writes a third-level item as
    // "    - text" -- four spaces. Stopping at three classified that as ordinary text, merged the
    // newline in front of it, and dropped the whole third level into the second level's item as
    // literal "- text". Being permissive here is safe: a genuinely indented CODE line is
    // recognised separately by the caller (isIndentedCodeLine() plus a preceding blank), and that
    // path already forces a real newline.
    int i=0;
    while (i<line.size() && (line.at(i)==QLatin1Char(' ') || line.at(i)==QLatin1Char('\t')))
    {
        ++i;
    }
    if (i>=line.size())
    {
        return false;
    }

    const auto c=line.at(i);

    // A fenced code-block delimiter. Load-bearing: Qt's own toMarkdown() happily writes a fence
    // directly under the preceding paragraph with no blank line between them, and merging that
    // newline away stops the fence being a fence at all -- "before / ``` / code / ```" came out
    // as one paragraph reading "before<br/>``` code", with no code block anywhere (measured).
    // matchFence() rather than a hand-rolled check, so the 3-character minimum and the
    // indented-fence rule stay defined in exactly one place.
    {
        QChar fenceChar;
        int fenceLen=0;
        if (matchFence(line,fenceChar,fenceLen))
        {
            return true;
        }
    }

    // Blockquote.
    if (c==QLatin1Char('>'))
    {
        return true;
    }

    // ATX heading: 1-6 '#' followed by a space or end of line.
    if (c==QLatin1Char('#'))
    {
        int hashes=0;
        while (i+hashes<line.size() && line.at(i+hashes)==QLatin1Char('#'))
        {
            ++hashes;
        }
        if (hashes>=1 && hashes<=6
            && (i+hashes>=line.size() || line.at(i+hashes)==QLatin1Char(' ')))
        {
            return true;
        }
    }

    // Bullet list item: '-', '*' or '+' followed by a space or tab. The marker alone is not
    // enough -- "-hello" is ordinary text.
    if (c==QLatin1Char('-') || c==QLatin1Char('*') || c==QLatin1Char('+'))
    {
        if (i+1<line.size()
            && (line.at(i+1)==QLatin1Char(' ') || line.at(i+1)==QLatin1Char('\t')))
        {
            return true;
        }
    }

    // Ordered list item: digits followed by '.' or ')' and then a space or tab.
    if (c.isDigit())
    {
        int digits=0;
        while (i+digits<line.size() && line.at(i+digits).isDigit())
        {
            ++digits;
        }
        auto after=i+digits;
        if (after<line.size()
            && (line.at(after)==QLatin1Char('.') || line.at(after)==QLatin1Char(')'))
            && after+1<line.size()
            && (line.at(after+1)==QLatin1Char(' ') || line.at(after+1)==QLatin1Char('\t')))
        {
            return true;
        }
    }

    // Thematic break, and the setext underlines that share their characters: a run of only '-',
    // '*', '_' or '=' (plus spaces). Kept deliberately loose -- a false positive here costs one
    // preserved newline, a false negative corrupts the construct.
    auto trimmed=line.trimmed();
    if (!trimmed.isEmpty())
    {
        const auto first=trimmed.at(0);
        if (first==QLatin1Char('-') || first==QLatin1Char('*')
            || first==QLatin1Char('_') || first==QLatin1Char('='))
        {
            bool uniform=true;
            for (const auto& ch : trimmed)
            {
                if (ch!=first && ch!=QLatin1Char(' '))
                {
                    uniform=false;
                    break;
                }
            }
            if (uniform)
            {
                return true;
            }
        }
    }

    return false;
}

//! Whether a line's own block ENDS at its newline, so nothing may be merged onto it.
//!
//! Narrower than startsBlockConstruct() on purpose. A list item or a blockquote can legally
//! continue on the following line (CommonMark lazy continuation), and merging there is exactly
//! what the chat convention wants -- the continuation stays inside the same item with a visible
//! <br/>. An ATX heading or a thematic break cannot continue: text merged onto a heading is
//! swallowed into the heading itself, and text merged onto "---" stops it being a break at all.
bool endsBlockAtNewline(const QString& line)
{
    auto trimmed=line.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    // A fence delimiter ends its own line, both halves of the pair. For the CLOSING fence this is
    // the mirror of the case above: by the time this runs the scanner has already left the fence,
    // so nothing else stops the following prose being merged onto the "```" -- which leaves the
    // fence unterminated and swallows the rest of the message into the code block (measured:
    // "``` / code / ``` / after" rendered as "<pre><code>code\n``` after</code></pre>").
    {
        QChar fenceChar;
        int fenceLen=0;
        if (matchFence(line,fenceChar,fenceLen))
        {
            return true;
        }
    }

    if (trimmed.startsWith(QLatin1Char('#')))
    {
        int hashes=0;
        while (hashes<trimmed.size() && trimmed.at(hashes)==QLatin1Char('#'))
        {
            ++hashes;
        }
        if (hashes>=1 && hashes<=6
            && (hashes>=trimmed.size() || trimmed.at(hashes)==QLatin1Char(' ')))
        {
            return true;
        }
    }

    // Thematic break or setext underline: a uniform run of '-', '*', '_' or '='.
    const auto first=trimmed.at(0);
    if (first==QLatin1Char('-') || first==QLatin1Char('*')
        || first==QLatin1Char('_') || first==QLatin1Char('='))
    {
        for (const auto& ch : trimmed)
        {
            if (ch!=first && ch!=QLatin1Char(' '))
            {
                return false;
            }
        }
        return true;
    }

    return false;
}

//! A GFM table delimiter row -- cells made only of '-', ':', '|' and spaces, with at least one
//! '-'. Used to recognise a table's HEADER row one line ahead (the delimiter always follows it
//! immediately), since the header row itself looks like an ordinary line otherwise.
bool isTableDelimiterLine(const QString& line)
{
    auto trimmed=line.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }
    bool sawDash=false;
    for (const auto& c : trimmed)
    {
        if (c==QLatin1Char('-'))
        {
            sawDash=true;
        }
        else if (c!=QLatin1Char('|') && c!=QLatin1Char(':') && c!=QLatin1Char(' '))
        {
            return false;
        }
    }
    return sawDash;
}

/**
 * @brief Preserve a chat message's line breaks through markdown rendering, so a single typed
 *  newline stays visually a new line instead of CommonMark's own rule of joining
 *  single-newline-separated lines into one reflowed paragraph -- see
 *  MarkdownRenderOptions::hardLineBreaks's own doc comment for why this diverges from plain
 *  CommonMark, and note below for why a CommonMark "hard break" (trailing two spaces) is NOT
 *  the mechanism used to do it.
 *
 * The two textbook ways to force a line break both fail here, for the same underlying reason --
 * Qt's markdown importer (QTextDocument::setMarkdown(), md4c-based) turns BOTH a soft break
 * (single '\n', normally reflowed to a space) and a hard break (trailing two spaces / '\\',
 * normally an in-paragraph <br>) into a LITERAL '\n' character that it then hands to
 * QTextCursor::insertText() -- and insertText() splits on '\n' by starting a NEW BLOCK, not by
 * inserting an in-block line break. So neither markdown construct survives as anything other
 * than a paragraph break once round-tripped through this particular importer, and appending
 * "two trailing spaces" (what this function used to do) would silently turn every line of a
 * multi-line chat message into its OWN <p>, each carrying messagetext.css's own paragraph
 * margins -- worse than doing nothing at all.
 *
 * The fix is to never hand md4c a '\n' at a boundary that must stay a break-within-one-block:
 * such a boundary is REMOVED from the line-split view entirely and replaced with a literal
 * QChar::LineSeparator (U+2028) character embedded directly in the text. md4c's tokenizer only
 * recognises ASCII '\n'/'\r' as line terminators, so U+2028 is invisible to it and the two
 * source lines are parsed as one continuous run of ordinary inline text; QTextCursor::insertText()
 * in turn only splits on '\n'/'\r'/ParagraphSeparator, never on LineSeparator, so U+2028 survives
 * intact as a literal character inside the resulting QTextFragment's text() -- exactly what
 * HtmlWriter::escapeText() already converts to <br/>.
 *
 * Skips fenced code (tracks the SAME fence character and at-least-as-long a closing run),
 * indented code blocks (4-space/tab runs immediately following a blank line or another indented
 * line), and table blocks (from a detected header+delimiter pair until the next blank line) --
 * merging a line boundary inside any of those would corrupt the construct itself.
 */
QString preserveChatLineBreaks(const QString& src)
{
    auto lines=src.split(QLatin1Char('\n'));
    if (lines.size()<=1)
    {
        return src;
    }

    bool inFence=false;
    QChar fenceChar;
    int fenceLen=0;
    bool inTable=false;
    bool prevBlank=true; // the start of the document counts as "preceded by a blank line"
    bool prevIndentedCode=false;

    QString out;
    out.reserve(src.size());

    for (int i=0;i<lines.size();++i)
    {
        const auto& line=lines.at(i);
        bool blank=line.trimmed().isEmpty();

        if (!inFence)
        {
            QChar fc;
            int fl=0;
            if (matchFence(line,fc,fl))
            {
                inFence=true;
                fenceChar=fc;
                fenceLen=fl;
            }
        }
        else
        {
            QChar fc;
            int fl=0;
            if (matchFence(line,fc,fl) && fc==fenceChar && fl>=fenceLen)
            {
                inFence=false;
            }
        }

        bool indentedCode=!inFence && !inTable && !blank && isIndentedCodeLine(line)
                           && (prevBlank || prevIndentedCode);

        if (!inFence && !indentedCode && !inTable && !blank
            && i+1<lines.size() && isTableDelimiterLine(lines.at(i+1)))
        {
            inTable=true;
        }

        out+=line;

        bool nextBlank=(i+1>=lines.size()) || lines.at(i+1).trimmed().isEmpty();
        bool skip=inFence || indentedCode || inTable;

        // A newline in front of a block opener is load-bearing syntax, not a visual break: merge
        // it away and the opener stops being one. Likewise a line that IS a heading or thematic
        // break ends its own block, so text merged onto it would be swallowed into the heading.
        bool nextStartsBlock=(i+1<lines.size()) && startsBlockConstruct(lines.at(i+1));
        bool selfClosingBlock=!blank && endsBlockAtNewline(line);

        if (i+1<lines.size())
        {
            if (!skip && !blank && !nextBlank && !nextStartsBlock && !selfClosingBlock)
            {
                // Merge into ONE logical line as far as md4c is concerned -- no '\n' at this
                // boundary at all -- with an embedded LineSeparator marking where the visual
                // break belongs. See this function's own top comment for why a real '\n'
                // (hard-break syntax included) does not work here.
                out+=QChar::LineSeparator;
            }
            else
            {
                out+=QLatin1Char('\n');
            }
        }

        if (inTable && nextBlank)
        {
            inTable=false;
        }

        prevBlank=blank;
        prevIndentedCode=indentedCode;
    }

    return out;
}

/******************************* QTextDocument -> HTML walk ******************************/

//! One currently-open <ul>/<ol> in the nesting stack -- see HtmlWriter::openListItem().
struct OpenList
{
    QTextList* list;
    QString tag;
};

class HtmlWriter
{
    public:

        explicit HtmlWriter(const MarkdownRenderOptions& options)
            : m_options(options)
        {}

        QString render(QTextDocument* doc)
        {
            m_html.clear();
            m_anchorsEmitted=0;
            m_inCodeBlock=false;
            m_codeLanguage.clear();
            m_openLists.clear();
            m_quoteLevel=0;

            walkFrame(doc->rootFrame());

            closeCodeBlockIfOpen();
            closeAllLists();
            adjustBlockquoteLevel(0);

            return m_html;
        }

    private:

        //! Walks a frame's children in document order. A child that is itself a QTextTable is
        //! emitted as one unit by writeTable() -- table CELL blocks must never reach writeBlock()
        //! directly, or the table structure is lost and every cell becomes its own <p>.
        void walkFrame(QTextFrame* frame)
        {
            for (auto it=frame->begin(); !it.atEnd(); ++it)
            {
                auto childFrame=it.currentFrame();
                if (childFrame!=nullptr)
                {
                    auto table=qobject_cast<QTextTable*>(childFrame);
                    if (table!=nullptr)
                    {
                        closeCodeBlockIfOpen();
                        closeAllLists();
                        adjustBlockquoteLevel(0);
                        writeTable(table);
                    }
                    else
                    {
                        walkFrame(childFrame);
                    }
                }
                else
                {
                    auto block=it.currentBlock();
                    if (block.isValid())
                    {
                        writeBlock(block);
                    }
                }
            }
        }

        void writeBlock(const QTextBlock& block)
        {
            auto fmt=block.blockFormat();

            bool isCode=fmt.hasProperty(QTextFormat::BlockCodeFence)
                        || fmt.hasProperty(QTextFormat::BlockCodeLanguage);
            if (isCode)
            {
                writeCodeLine(block,fmt);
                return;
            }
            closeCodeBlockIfOpen();

            int quoteLevel=fmt.hasProperty(QTextFormat::BlockQuoteLevel)
                                ? fmt.intProperty(QTextFormat::BlockQuoteLevel)
                                : 0;
            if (quoteLevel!=m_quoteLevel)
            {
                // Simplification: a list continuing across a blockquote-nesting change is rare
                // enough in chat messages that closing it outright, rather than tracking two
                // interleaved stacks, is an acceptable trade for this stage's complexity budget.
                closeAllLists();
            }
            adjustBlockquoteLevel(quoteLevel);

            if (fmt.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
            {
                closeAllLists();
                m_html+=QStringLiteral("<hr/>");
                return;
            }

            auto* list=block.textList();
            if (list!=nullptr)
            {
                openListItem(list);
            }
            else
            {
                closeAllLists();
            }

            int heading=fmt.headingLevel();
            QString tag;
            if (list!=nullptr)
            {
                tag=QStringLiteral("li");
            }
            else if (heading>0 && heading<=6)
            {
                tag=QStringLiteral("h%1").arg(heading);
            }
            else
            {
                tag=QStringLiteral("p");
            }

            // An ordinary paragraph with nothing in it is emitted as NOTHING. It is not content:
            // a QTextDocument built by setMarkdown() always carries an empty block before and
            // after every table (measured -- "table / table" comes back as
            // "<p></p><table>..</table><p></p><table>..</table><p></p>"), and those blocks exist
            // only because Qt's structure demands them. Rendering them cost real vertical space
            // through messagetext.css's own paragraph margins -- measured, two adjacent tables
            // shrank from 99px to 84px once they were dropped -- which made every gap look larger
            // than what was actually authored.
            //
            // A DELIBERATE blank line is not affected: MessageEditor exports one as a paragraph
            // holding a zero-width space (see fillEmptyBlocksForExport()), which has a fragment
            // and so is not empty by this test. That is the whole point of the marker -- it is
            // what tells an authored blank line apart from structural padding, here as well as in
            // markdown.
            //
            // Lists and headings are excluded: an empty list item still has to render its bullet,
            // and an empty heading still occupies its own line.
            if (tag==QStringLiteral("p") && block.begin()==block.end())
            {
                return;
            }

            m_html+=QLatin1Char('<')+tag+QLatin1Char('>');

            auto marker=fmt.marker();
            if (marker==QTextBlockFormat::MarkerType::Checked)
            {
                m_html+=QChar(0x2612)+QStringLiteral(" "); // BALLOT BOX WITH X
            }
            else if (marker==QTextBlockFormat::MarkerType::Unchecked)
            {
                m_html+=QChar(0x2610)+QStringLiteral(" "); // BALLOT BOX
            }

            writeFragments(block,heading>0 && heading<=6);

            m_html+=QStringLiteral("</")+tag+QLatin1Char('>');
        }

        void writeFragments(const QTextBlock& block, bool suppressBold=false)
        {
            for (auto it=block.begin(); !it.atEnd(); ++it)
            {
                auto frag=it.fragment();
                if (frag.isValid())
                {
                    writeFragment(frag,suppressBold);
                }
            }
        }

        //! @param suppressBold Set for a heading (and a table header cell) block -- Qt's
        //!  markdown importer already bolds a heading's own char format
        //!  (cbEnterBlock(MD_BLOCK_H) sets QFont::Bold on the whole run), and messagetext.css's
        //!  <h1>-<h6> styling renders bold regardless of any inline tag -- so an inline <b>
        //!  spanning a heading's text would be redundant markup with no visual difference either
        //!  way, not a correctness issue. Suppressed unconditionally for simplicity rather than
        //!  trying to distinguish "bold because heading" from "bold because of a nested **...**
        //!  inside the heading" -- fontWeight() alone cannot tell those apart, and the rendered
        //!  result is identical (still bold) whichever way this goes.
        void writeFragment(const QTextFragment& frag, bool suppressBold=false)
        {
            auto cfmt=frag.charFormat();

            if (cfmt.isImageFormat())
            {
                writeImage(cfmt.toImageFormat());
                return;
            }

            bool anchor=false;
            if (cfmt.isAnchor())
            {
                auto href=cfmt.anchorHref();
                if (isSchemeAllowed(href) && m_anchorsEmitted<m_options.maxAnchors)
                {
                    anchor=true;
                    ++m_anchorsEmitted;
                    m_html+=QStringLiteral("<a href=\"")+escapeAttribute(href)+QStringLiteral("\">");
                }
            }

            bool bold=!suppressBold && cfmt.fontWeight()>QFont::Normal;
            bool italic=cfmt.fontItalic();
            bool strike=cfmt.fontStrikeOut();
            // An anchor already carries its own underline via linkColor/linkUnderline
            // (ChatMessageTextBrowser::applyDocumentStyle()) -- emitting <u> on top would fight
            // that rule rather than compose with it.
            bool underline=cfmt.fontUnderline() && !anchor;
            bool code=cfmt.fontFixedPitch();

            if (bold) m_html+=QStringLiteral("<b>");
            if (italic) m_html+=QStringLiteral("<i>");
            if (strike) m_html+=QStringLiteral("<s>");
            if (underline) m_html+=QStringLiteral("<u>");
            if (code) m_html+=QStringLiteral("<code>");

            // MarkdownRenderOptions::extraLinkify -- an optional host hook for link detection
            // this renderer cannot do on its own (its own doc comment's example is bare-domain
            // detection, but a host-resolved "@username" mention is exactly the same shape: a
            // plain-text pattern this generic renderer has no directory to resolve on its own).
            // Consulted only for a plain run -- not already an anchor, not inside inline code or
            // a fenced block -- matching the documented contract precisely. Previously declared
            // but never actually called anywhere in this file; a host setting it had no way to
            // discover that short of reading this source.
            QString extra;
            if (!anchor && !code && m_options.extraLinkify)
            {
                extra=m_options.extraLinkify(frag.text());
            }
            if (!extra.isEmpty())
            {
                m_html+=extra;
            }
            else
            {
                m_html+=escapeText(frag.text());
            }

            if (code) m_html+=QStringLiteral("</code>");
            if (underline) m_html+=QStringLiteral("</u>");
            if (strike) m_html+=QStringLiteral("</s>");
            if (italic) m_html+=QStringLiteral("</i>");
            if (bold) m_html+=QStringLiteral("</b>");

            if (anchor)
            {
                m_html+=QStringLiteral("</a>");
            }
        }

        void writeImage(const QTextImageFormat& imgFmt)
        {
            // An <img> is never emitted regardless of alt text: a remote src would make
            // QTextBrowser fetch it on render, leaking the reader's IP to whoever hosts it, with
            // no user action involved at all -- only the escaped alt text (falling back to the
            // source URL when markdown gave no alt text) is ever shown.
            auto src=imgFmt.name();
            auto alt=imgFmt.stringProperty(QTextFormat::ImageAltText);
            auto display=escapeText(alt.isEmpty() ? src : alt);
            if (isSchemeAllowed(src) && m_anchorsEmitted<m_options.maxAnchors)
            {
                ++m_anchorsEmitted;
                m_html+=QStringLiteral("<a href=\"")+escapeAttribute(src)+QStringLiteral("\">")
                        +display+QStringLiteral("</a>");
            }
            else
            {
                m_html+=display;
            }
        }

        void writeCodeLine(const QTextBlock& block, const QTextBlockFormat& fmt)
        {
            if (!m_inCodeBlock)
            {
                closeAllLists();
                adjustBlockquoteLevel(0);
                m_inCodeBlock=true;
                m_codeLanguage=fmt.hasProperty(QTextFormat::BlockCodeLanguage)
                                    ? fmt.stringProperty(QTextFormat::BlockCodeLanguage)
                                    : QString();
                m_html+=QStringLiteral("<pre");
                if (!m_codeLanguage.isEmpty())
                {
                    // Threaded through for Stage 3's syntax highlighter. Deliberately on <pre>,
                    // not <code>: Qt's own HTML parser (QTextHtmlParser::parseTag()) reads
                    // "class=language-x" only on Html_pre and maps it straight back onto
                    // QTextFormat::BlockCodeLanguage -- so this DOES survive a later setHtml()
                    // round-trip, and Stage 3 can read the language straight off the live
                    // rendered document (block.blockFormat().stringProperty(
                    // QTextFormat::BlockCodeLanguage)) without needing ChatMessageText's cached
                    // source at all. The cache stays (see ChatMessageText_p::sourceText) for
                    // later stages that need the ORIGINAL markdown regardless (Stage 5b re-
                    // render, Stage 6 mentions), but Stage 3 specifically does not depend on it.
                    m_html+=QStringLiteral(" class=\"language-")+escapeAttribute(m_codeLanguage)+QLatin1Char('"');
                }
                m_html+=QStringLiteral("><code>");
            }
            else
            {
                m_html+=QLatin1Char('\n');
            }

            // Code content is escaped as literal text -- Qt applies no inline char formatting
            // inside a code block, only the characters themselves matter here.
            m_html+=block.text().toHtmlEscaped();
        }

        void closeCodeBlockIfOpen()
        {
            if (m_inCodeBlock)
            {
                m_html+=QStringLiteral("</code></pre>");
                m_inCodeBlock=false;
                m_codeLanguage.clear();
            }
        }

        void openListItem(QTextList* list)
        {
            if (!m_openLists.empty() && m_openLists.back().list==list)
            {
                return;
            }

            auto indent=list->format().indent();
            while (!m_openLists.empty()
                   && m_openLists.back().list!=list
                   && m_openLists.back().list->format().indent()>=indent)
            {
                m_html+=QStringLiteral("</")+m_openLists.back().tag+QLatin1Char('>');
                m_openLists.pop_back();
            }

            if (m_openLists.empty() || m_openLists.back().list!=list)
            {
                auto style=list->format().style();
                bool ordered=(style==QTextListFormat::ListDecimal
                              || style==QTextListFormat::ListLowerAlpha
                              || style==QTextListFormat::ListUpperAlpha
                              || style==QTextListFormat::ListLowerRoman
                              || style==QTextListFormat::ListUpperRoman);
                QString tag=ordered ? QStringLiteral("ol") : QStringLiteral("ul");
                m_html+=QLatin1Char('<')+tag;
                if (ordered && list->format().start()!=1)
                {
                    // "3. a" etc. -- QTextListFormat::start() carries the source's own starting
                    // number; Qt's own HTML parser reads a "start" attribute on <ol> back into
                    // the same property (unverified against a live build in this stage -- see
                    // task-message-formatting-plan.md's own risk notes), so this round-trips the
                    // same way the code-language class does.
                    m_html+=QStringLiteral(" start=\"%1\"").arg(list->format().start());
                }
                m_html+=QLatin1Char('>');
                m_openLists.push_back({list,tag});
            }
        }

        void closeAllLists()
        {
            while (!m_openLists.empty())
            {
                m_html+=QStringLiteral("</")+m_openLists.back().tag+QLatin1Char('>');
                m_openLists.pop_back();
            }
        }

        void adjustBlockquoteLevel(int newLevel)
        {
            while (m_quoteLevel>newLevel)
            {
                m_html+=QStringLiteral("</blockquote>");
                --m_quoteLevel;
            }
            while (m_quoteLevel<newLevel)
            {
                m_html+=QStringLiteral("<blockquote>");
                ++m_quoteLevel;
            }
        }

        void writeTable(QTextTable* table)
        {
            // A table directly after a PARAGRAPH gets 3px of top margin, inline. Measured on
            // white, counting the empty rows between one element's last ink and the next's
            // first: two adjacent tables sit 9 rows apart with no styling at all -- the table
            // frame's own spacing -- and 9 is the distance wanted everywhere. A paragraph's
            // margin-bottom is 6px, so text sat 6 rows above a table and read as flush. 3px is
            // exactly the difference, and it scales one row per pixel (2px measured 8, 4px
            // measured 10).
            //
            // Inline, and on the TABLE, because nothing else can express it. Qt's CSS subset has
            // no sibling selector; an inline margin-bottom on the preceding <p> is ignored
            // (measured: 7 rows with or without it); and a stylesheet margin on `table` lands on
            // every table edge, so it doubles up between two tables (8px measured 25 rows) and
            // cannot be taken back on one side without a negative margin -- which Qt honours,
            // but which also pulls the paragraph AFTER a table in from 10 rows to 7 (measured).
            //
            // "</p>" here is always a real paragraph: an empty structural block emits nothing
            // (writeBlock()), and MessageEditor never generates a blank-line marker next to a
            // table (fillEmptyBlocksForExport()). Headings and lists carry margins of their own.
            //
            // Keep in step with `p { margin-bottom }` in messagetext.css: this is 9 minus that.
            if (m_html.endsWith(QStringLiteral("</p>")))
            {
                m_html+=QStringLiteral("<table style=\"margin-top:3px\">");
            }
            else
            {
                m_html+=QStringLiteral("<table>");
            }
            int rows=table->rows();
            int cols=table->columns();
            for (int r=0;r<rows;++r)
            {
                m_html+=QStringLiteral("<tr>");
                for (int c=0;c<cols;++c)
                {
                    auto cell=table->cellAt(r,c);
                    // A merged (row/col-spanning) cell is reported for every slot it spans --
                    // emit it only once, from its own top-left origin.
                    if (cell.row()!=r || cell.column()!=c)
                    {
                        continue;
                    }
                    QString tag=(r==0) ? QStringLiteral("th") : QStringLiteral("td");
                    m_html+=QLatin1Char('<')+tag+QLatin1Char('>');

                    bool first=true;
                    for (auto it=cell.begin(); !it.atEnd(); ++it)
                    {
                        auto childFrame=it.currentFrame();
                        if (childFrame!=nullptr)
                        {
                            // A nested table/frame inside a cell -- not producible from markdown
                            // input, but walked rather than silently dropped just in case.
                            walkFrame(childFrame);
                            continue;
                        }
                        auto block=it.currentBlock();
                        if (block.isValid())
                        {
                            if (!first)
                            {
                                m_html+=QStringLiteral("<br/>");
                            }
                            first=false;
                            // Qt's markdown importer already bolds a header row's own cell
                            // format (same rationale as headings -- see writeFragment()'s own
                            // doc comment); th is rendered bold by default regardless of any
                            // inline tag, so suppress the redundant wrap here too.
                            writeFragments(block,r==0);
                        }
                    }

                    m_html+=QStringLiteral("</")+tag+QLatin1Char('>');
                }
                m_html+=QStringLiteral("</tr>");
            }
            m_html+=QStringLiteral("</table>");
        }

        QString escapeText(const QString& text) const
        {
            auto escaped=text.toHtmlEscaped();
            // toHtmlEscaped() only touches &/</>/"/' -- line separators (soft breaks Qt keeps
            // WITHIN one block) and any literal '\n' still need converting to a real line break
            // in the output, same convention chattextrender.cpp already uses for plain text.
            escaped.replace(QChar::LineSeparator,QStringLiteral("<br/>"));
            escaped.replace(QLatin1Char('\n'),QStringLiteral("<br/>"));
            return escaped;
        }

        QString escapeAttribute(const QString& value) const
        {
            // toHtmlEscaped() covers '"' too, so this is safe verbatim inside a double-quoted
            // attribute value.
            return value.toHtmlEscaped();
        }

        bool isSchemeAllowed(const QString& href) const
        {
            QUrl url(href,QUrl::StrictMode);
            if (!url.isValid())
            {
                return false;
            }
            auto scheme=url.scheme();
            if (scheme.isEmpty())
            {
                // A bare "www.example.com" autolink resolves to a FULLY schemed
                // "https://www.example.com" via QTextDocument::setMarkdown()'s own documented
                // autolink behaviour -- an empty scheme here means something else entirely (a
                // bare "#fragment", say), rejected rather than guessed at.
                return false;
            }
            for (const auto& allowed : m_options.allowedLinkSchemes)
            {
                if (scheme.compare(allowed,Qt::CaseInsensitive)==0)
                {
                    return true;
                }
            }
            return false;
        }

        MarkdownRenderOptions m_options;
        QString m_html;
        int m_anchorsEmitted=0;
        bool m_inCodeBlock=false;
        QString m_codeLanguage;
        std::vector<OpenList> m_openLists;
        int m_quoteLevel=0;
};

} // anonymous namespace

//--------------------------------------------------------------------------

QString markdownToHtml(const QString& markdown, const MarkdownRenderOptions& options)
{
    auto src=markdown;
    if (src.size()>options.maxSourceChars)
    {
        src=src.left(options.maxSourceChars);
    }
    if (options.hardLineBreaks)
    {
        src=preserveChatLineBreaks(src);
    }

    QTextDocument doc;
    doc.setMarkdown(src,QTextDocument::MarkdownDialectGitHub);

    HtmlWriter writer(options);
    return writer.render(&doc);
}

//--------------------------------------------------------------------------

QString markdownToPlainText(const QString& markdown, int maxSourceChars)
{
    auto src=markdown;
    if (src.size()>maxSourceChars)
    {
        src=src.left(maxSourceChars);
    }

    QTextDocument doc;
    doc.setMarkdown(src,QTextDocument::MarkdownDialectGitHub);
    return doc.toPlainText();
}

//--------------------------------------------------------------------------

QString markdownWithChatLineBreaks(const QString& markdown)
{
    // The same call markdownToHtml() makes on the way in, exposed for MessageEditor's own import
    // so the two cannot drift -- see the declaration for why the editor needs it.
    return preserveChatLineBreaks(markdown);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
