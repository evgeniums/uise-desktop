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

/** @file uise/desktop/syntaxtokenizer.cpp
*
*  Defines tokenizeSyntaxLine().
*
*  Deliberately a single left-to-right pass, NOT a port of QSourceHighlite's own
*  qsourcehighliter.cpp scanner (task-message-formatting-plan.md, Stage 3). Upstream paints the
*  whole line with a base format, then runs a word pass, then a string pass, then a number pass,
*  then a dialect pass -- each one OVERWRITING whatever the previous pass wrote. That structure is
*  the direct cause of its own real bugs: a `#` inside a string read as a comment start, a
*  keyword recoloured inside an unterminated C-style block comment because the comment scan and
*  the word scan don't share a single notion of "where are we", block-comment continuation
*  detected by counting comment-close tokens modulo two. A single dispatch-by-position lexer
*  produces non-overlapping spans
*  with correct precedence by construction and needs no such overwrite/coalesce step.
*
*  Word TABLES are still reused from that port (see syntaxlanguagedata.cpp) -- only the SCANNER
*  is new.
*
*/

/****************************************************************************/

#include <optional>
#include <utility>

#include <uise/desktop/syntaxtokenizer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

/*
 * QTextBlock::userState() encoding for a code line (see syntaxtokenizer.hpp's own doc comment
 * on tokenizeSyntaxLine()'s return value). Private to this file -- SyntaxHighlighter only ever
 * threads this value from previousBlockState() to tokenizeSyntaxLine() to setCurrentBlockState(),
 * never decodes it itself.
 *
 *   SyntaxNoCodeState (-1) : nothing open.
 *   bits [0..3]            : Continuation kind.
 *   bits [4..11]            : nesting depth, 0..255 (Rust's nestable block comments; every other
 *                             language stays 0).
 *   bits [12..27]           : SyntaxLanguage::index() that opened the continuation, clamped to
 *                             fit -- a missed continuation on overflow is cosmetic, a wrong
 *                             index is not (see this project's Stage-3 plan for the rationale).
 *   bits [28..31]           : reserved, always 0.
 */
constexpr int ContinuationBits=4;
constexpr int ContinuationMask=(1<<ContinuationBits)-1;
constexpr int DepthBits=8;
constexpr int DepthShift=ContinuationBits;
constexpr int DepthMask=(1<<DepthBits)-1;
constexpr int LanguageShift=DepthShift+DepthBits;
constexpr int LanguageMask=0xFFFF;

enum class Continuation : int
{
    None=0,
    BlockComment=1,
    TripleSingleQuote=2,
    TripleDoubleQuote=3
};

int encodeState(Continuation cont, int depth, int languageIndex) noexcept
{
    if (cont==Continuation::None)
    {
        return SyntaxNoCodeState;
    }
    int d=depth;
    if (d<0) d=0;
    if (d>DepthMask) d=DepthMask;
    int li=languageIndex;
    if (li<0) li=0;
    if (li>LanguageMask) li=LanguageMask;
    return (static_cast<int>(cont)&ContinuationMask)
           | ((d&DepthMask)<<DepthShift)
           | ((li&LanguageMask)<<LanguageShift);
}

struct DecodedState
{
    Continuation cont=Continuation::None;
    int depth=0;
    int languageIndex=-1;
};

DecodedState decodeState(int state) noexcept
{
    DecodedState result;
    if (state<0)
    {
        return result;
    }
    result.cont=static_cast<Continuation>(state&ContinuationMask);
    result.depth=(state>>DepthShift)&DepthMask;
    result.languageIndex=(state>>LanguageShift)&LanguageMask;
    return result;
}

//--------------------------------------------------------------------------

bool isIdentStart(QChar c) noexcept
{
    return c.isLetter() || c==QLatin1Char('_');
}

bool isIdentChar(QChar c, SyntaxDialect dialect) noexcept
{
    if (dialect==SyntaxDialect::Css && c==QLatin1Char('-'))
    {
        // CSS property/selector names and custom properties are hyphenated
        // ("background-color", ".my-class", "--accent") -- without this a hyphenated word
        // splits into fragments and never matches the CSS word table.
        return true;
    }
    return c.isLetterOrNumber() || c==QLatin1Char('_');
}

//! ASCII token match at `pos` with no allocation on either side. Bounds-safe: false whenever
//! `token` would run past `text`'s end.
bool matchesAt(QStringView text, int pos, std::string_view token) noexcept
{
    if (token.empty())
    {
        return false;
    }
    auto tlen=static_cast<int>(token.size());
    if (pos<0 || pos+tlen>text.size())
    {
        return false;
    }
    for (int i=0;i<tlen;++i)
    {
        if (text[pos+i].unicode()!=static_cast<char16_t>(static_cast<unsigned char>(token[static_cast<std::size_t>(i)])))
        {
            return false;
        }
    }
    return true;
}

//! Trims plain spaces off both ends of [begin,end) and returns {trimmedStart,trimmedLength}.
//! Index-based rather than QStringView::trimmed()+pointer-difference, since QStringView's
//! underlying storage_type is not guaranteed to be QChar-sized in every Qt version this repo
//! targets and pointer arithmetic across that boundary is not worth the risk here.
std::pair<int,int> trimRange(QStringView line, int begin, int end) noexcept
{
    while (begin<end && line[begin]==QLatin1Char(' '))
    {
        ++begin;
    }
    while (end>begin && line[end-1]==QLatin1Char(' '))
    {
        --end;
    }
    return {begin,end-begin};
}

//--------------------------------------------------------------------------

//! Scans a `"`/`'`/`` ` `` -delimited string starting at the OPENING quote (`line[pos]==quote`).
//! Honours backslash escapes and a doubled-quote escape (`''` inside a `'...'` string, SQL's own
//! convention -- harmless for languages that don't use it: a doubled delimiter right after a
//! closed string just reopens an adjacent empty one, which is rare and cosmetic).
//! @return Index just past the closing quote, or -1 if the line ends first (caller then emits a
//!  single-line-only Literal span to the end of line; unlike a block comment or a triple-quoted
//!  string, an ordinary quoted string never carries a continuation into the next QTextBlock in
//!  this scanner -- a deliberate scope limit, see this project's Stage-3 plan).
int scanQuotedString(QStringView line, int pos, QChar quote) noexcept
{
    int n=line.size();
    int i=pos+1;
    while (i<n)
    {
        QChar c=line[i];
        if (c==QLatin1Char('\\') && i+1<n)
        {
            i+=2;
            continue;
        }
        if (c==quote)
        {
            if (i+1<n && line[i+1]==quote)
            {
                i+=2;
                continue;
            }
            return i+1;
        }
        ++i;
    }
    return -1;
}

//! Scans a Python-style triple-quoted string; `pos` points at the FIRST of the three opening
//! quote characters (already confirmed present by the caller).
//! @return Index just past the three closing quote characters, or -1 if not closed on this line.
int scanTripleQuotedString(QStringView line, int pos, QChar quote) noexcept
{
    int n=line.size();
    int i=pos+3;
    while (i<n)
    {
        QChar c=line[i];
        if (c==QLatin1Char('\\') && i+1<n)
        {
            i+=2;
            continue;
        }
        if (c==quote && i+3<=n && line[i+1]==quote && line[i+2]==quote)
        {
            return i+3;
        }
        ++i;
    }
    return -1;
}

//! Scans a block comment body starting right AFTER its opening token was already matched/
//! consumed by the caller (`pos` is the first character of the body, not of the opening token).
//! `depth` starts at however many nesting levels are already open (1 for a comment just opened
//! on this line, or whatever was carried in from a previous line) and is updated in place.
//! @return Index just past the token that closed the OUTERMOST level once `depth` reaches 0, or
//!  -1 if the line ends first (the whole remainder of the line belongs to the comment; `depth`
//!  then holds the still-open nesting level to carry into the next line's state).
int scanBlockCommentBody(QStringView line, int pos, std::string_view start, std::string_view end,
                         bool nestable, int& depth) noexcept
{
    int n=line.size();
    int i=pos;
    while (i<n)
    {
        if (nestable && matchesAt(line,i,start))
        {
            ++depth;
            i+=static_cast<int>(start.size());
            continue;
        }
        if (matchesAt(line,i,end))
        {
            --depth;
            i+=static_cast<int>(end.size());
            if (depth<=0)
            {
                depth=0;
                return i;
            }
            continue;
        }
        ++i;
    }
    return -1;
}

int scanNumber(QStringView line, int pos) noexcept
{
    int n=line.size();
    int start=pos;

    // 0x/0o/0b prefix -- consume every following alnum/underscore verbatim (hex digits, and
    // Rust-/Python-style digit-group underscores) rather than re-validating the digit set.
    if (pos<n && line[pos]==QLatin1Char('0') && pos+1<n)
    {
        QChar c2=line[pos+1];
        if (c2==QLatin1Char('x') || c2==QLatin1Char('X')
            || c2==QLatin1Char('o') || c2==QLatin1Char('O')
            || c2==QLatin1Char('b') || c2==QLatin1Char('B'))
        {
            pos+=2;
            while (pos<n && (line[pos].isLetterOrNumber() || line[pos]==QLatin1Char('_')))
            {
                ++pos;
            }
            return pos;
        }
    }

    bool sawDot=false;
    bool sawExp=false;
    while (pos<n)
    {
        QChar c=line[pos];
        if (c.isDigit() || c==QLatin1Char('_'))
        {
            ++pos;
            continue;
        }
        if (c==QLatin1Char('.') && !sawDot && !sawExp && pos+1<n && line[pos+1].isDigit())
        {
            sawDot=true;
            ++pos;
            continue;
        }
        if ((c==QLatin1Char('e') || c==QLatin1Char('E')) && !sawExp && pos+1<n
            && (line[pos+1].isDigit()
                || ((line[pos+1]==QLatin1Char('+') || line[pos+1]==QLatin1Char('-'))
                    && pos+2<n && line[pos+2].isDigit())))
        {
            sawExp=true;
            ++pos;
            if (line[pos]==QLatin1Char('+') || line[pos]==QLatin1Char('-'))
            {
                ++pos;
            }
            continue;
        }
        break;
    }

    // A small known suffix alphabet only (u/l/f in either case) -- NOT "consume every trailing
    // letter", which would misparse "1st" as the number "1" plus garbage rather than leaving
    // "1st" for the identifier scanner to reject as not starting with a letter (it stays two
    // unformatted tokens, "1" then "st", which is the correct outcome either way).
    while (pos<n)
    {
        char16_t u=line[pos].unicode();
        if (u==u'u' || u==u'U' || u==u'l' || u==u'L' || u==u'f' || u==u'F')
        {
            ++pos;
            continue;
        }
        break;
    }

    return pos>start ? pos : start;
}

//--------------------------------------------------------------------------

//! Line-start-anchored constructs (Make target, Asm label, YAML/INI key) -- run ONCE before the
//! generic scan, only for a line with no carried-over continuation (a target/label/key can only
//! begin a genuinely fresh line, never the middle of a still-open comment or string). Appends at
//! most one span and returns where the generic scan should resume (0 if nothing matched).
int applyLineStructuralDialect(const SyntaxLanguage& language, QStringView line,
                               std::vector<SyntaxSpan>& out)
{
    int n=line.size();

    switch (language.dialect())
    {
        case SyntaxDialect::Make:
        {
            if (n>0 && line[0]==QLatin1Char('\t'))
            {
                // A recipe line (Makefiles use a literal leading tab for these) -- generic scan
                // only, never a target.
                return 0;
            }
            for (int i=0;i<n;++i)
            {
                if (line[i]==QLatin1Char('#'))
                {
                    break;
                }
                if (line[i]==QLatin1Char(':'))
                {
                    auto [start,len]=trimRange(line,0,i);
                    if (len>0)
                    {
                        out.push_back({start,len,SyntaxBucket::Callable});
                        return i+1;
                    }
                    break;
                }
            }
            return 0;
        }
        case SyntaxDialect::Asm:
        {
            int i=0;
            while (i<n && line[i]==QLatin1Char(' '))
            {
                ++i;
            }
            if (i>0)
            {
                // Labels sit in column 0 by convention -- an indented line is an instruction.
                return 0;
            }
            int start=i;
            while (i<n && (line[i].isLetterOrNumber() || line[i]==QLatin1Char('_') || line[i]==QLatin1Char('.')))
            {
                ++i;
            }
            if (i>start && i<n && line[i]==QLatin1Char(':'))
            {
                out.push_back({start,i-start,SyntaxBucket::Callable});
                return i+1;
            }
            return 0;
        }
        case SyntaxDialect::Yaml:
        {
            int i=0;
            while (i<n && line[i]==QLatin1Char(' '))
            {
                ++i;
            }
            if (i>=n || line[i]==QLatin1Char('#') || line[i]==QLatin1Char('-'))
            {
                return 0;
            }
            int start=i;
            for (int j=i;j<n;++j)
            {
                QChar c=line[j];
                if (c==QLatin1Char('"') || c==QLatin1Char('\''))
                {
                    // A quoted key -- leave it entirely to the generic string scan instead.
                    return 0;
                }
                if (c==QLatin1Char(':') && (j+1>=n || line[j+1]==QLatin1Char(' ')))
                {
                    auto [keyStart,keyLen]=trimRange(line,start,j);
                    if (keyLen>0)
                    {
                        out.push_back({keyStart,keyLen,SyntaxBucket::Keyword});
                        return j+1;
                    }
                    return 0;
                }
            }
            return 0;
        }
        case SyntaxDialect::Ini:
        {
            int i=0;
            while (i<n && line[i]==QLatin1Char(' '))
            {
                ++i;
            }
            if (i<n && line[i]==QLatin1Char('['))
            {
                for (int j=i+1;j<n;++j)
                {
                    if (line[j]==QLatin1Char(']'))
                    {
                        out.push_back({i,j-i+1,SyntaxBucket::Type});
                        return j+1;
                    }
                }
                return 0;
            }
            if (i>=n || line[i]==QLatin1Char('#') || line[i]==QLatin1Char(';'))
            {
                return 0;
            }
            int start=i;
            for (int j=i;j<n;++j)
            {
                if (line[j]==QLatin1Char('='))
                {
                    auto [keyStart,keyLen]=trimRange(line,start,j);
                    if (keyLen>0)
                    {
                        out.push_back({keyStart,keyLen,SyntaxBucket::Keyword});
                        return j+1;
                    }
                    return 0;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

}

//--------------------------------------------------------------------------

int tokenizeSyntaxLine(const SyntaxLanguage& language, QStringView line, int previousState,
                       std::vector<SyntaxSpan>& out)
{
    out.clear();

    auto decoded=decodeState(previousState);
    if (decoded.languageIndex!=language.index())
    {
        // Either a genuinely fresh start (SyntaxNoCodeState) or a continuation opened by a
        // DIFFERENT language -- e.g. this fence follows another fence of a different language
        // with no non-code paragraph forcing SyntaxHighlighter to reset in between. Either way,
        // an in-progress comment/string from a language that no longer applies here must not
        // leak in (task-message-formatting-plan.md, Stage 3's own "language reset" requirement).
        decoded=DecodedState{};
    }

    int n=line.size();
    int pos=0;
    Continuation cont=decoded.cont;
    int depth=decoded.depth;

    // --- resume a continuation carried in from the previous line, if any ---

    if (cont==Continuation::BlockComment)
    {
        int end=scanBlockCommentBody(line,0,language.blockCommentStart(),language.blockCommentEnd(),
                                     language.nestableBlockComments(),depth);
        if (end<0)
        {
            if (n>0)
            {
                out.push_back({0,n,SyntaxBucket::Comment});
            }
            return encodeState(Continuation::BlockComment,depth,language.index());
        }
        if (end>0)
        {
            out.push_back({0,end,SyntaxBucket::Comment});
        }
        pos=end;
        cont=Continuation::None;
        depth=0;
    }
    else if (cont==Continuation::TripleSingleQuote || cont==Continuation::TripleDoubleQuote)
    {
        QChar quote=(cont==Continuation::TripleSingleQuote) ? QLatin1Char('\'') : QLatin1Char('"');
        // Resuming mid-string: look for the closer from position 0, not position 3 -- there is
        // no opening token to skip on a continuation line, unlike scanTripleQuotedString()'s own
        // pos+3 assumption for a FRESH triple-quote open (used below in the main loop instead).
        int i=0;
        int end=-1;
        while (i<n)
        {
            QChar c=line[i];
            if (c==QLatin1Char('\\') && i+1<n)
            {
                i+=2;
                continue;
            }
            if (c==quote && i+3<=n && line[i+1]==quote && line[i+2]==quote)
            {
                end=i+3;
                break;
            }
            ++i;
        }
        if (end<0)
        {
            if (n>0)
            {
                out.push_back({0,n,SyntaxBucket::Literal});
            }
            return encodeState(cont,0,language.index());
        }
        out.push_back({0,end,SyntaxBucket::Literal});
        pos=end;
        cont=Continuation::None;
    }
    else
    {
        // Genuinely fresh line -- try the line-start-anchored dialect constructs (Make target,
        // Asm label, YAML/INI key) before the generic scan. Never attempted mid-continuation
        // (the branches above never fall through to here).
        pos=applyLineStructuralDialect(language,line,out);
    }

    // --- generic left-to-right scan of whatever remains ---

    bool insideTag=false; // SyntaxDialect::Xml only -- reset per line; a tag split across lines
                          // is a known, documented scope limit (see syntaxtokenizer.hpp).

    while (pos<n)
    {
        QChar c=line[pos];

        // 1. line comment -- consumes the rest of the line outright.
        if (matchesAt(line,pos,language.lineComment()) || matchesAt(line,pos,language.lineComment2()))
        {
            out.push_back({pos,n-pos,SyntaxBucket::Comment});
            pos=n;
            break;
        }

        // 2. block comment open.
        if (!language.blockCommentStart().empty() && matchesAt(line,pos,language.blockCommentStart()))
        {
            int bodyStart=pos+static_cast<int>(language.blockCommentStart().size());
            int d=1;
            int end=scanBlockCommentBody(line,bodyStart,language.blockCommentStart(),
                                         language.blockCommentEnd(),language.nestableBlockComments(),d);
            if (end<0)
            {
                out.push_back({pos,n-pos,SyntaxBucket::Comment});
                return encodeState(Continuation::BlockComment,d,language.index());
            }
            out.push_back({pos,end-pos,SyntaxBucket::Comment});
            pos=end;
            continue;
        }

        // 3. triple-quoted string open (checked before the single-char string case, which would
        //    otherwise treat the first of the three quote characters as an unterminated string).
        if (language.tripleQuoteStrings()
            && (c==QLatin1Char('"') || c==QLatin1Char('\''))
            && pos+3<=n && line[pos+1]==c && line[pos+2]==c)
        {
            int end=scanTripleQuotedString(line,pos,c);
            if (end<0)
            {
                out.push_back({pos,n-pos,SyntaxBucket::Literal});
                auto k=(c==QLatin1Char('\'')) ? Continuation::TripleSingleQuote : Continuation::TripleDoubleQuote;
                return encodeState(k,0,language.index());
            }
            out.push_back({pos,end-pos,SyntaxBucket::Literal});
            pos=end;
            continue;
        }

        // 4. single-delimiter string.
        if ((c==QLatin1Char('"') && language.doubleQuoteStrings())
            || (c==QLatin1Char('\'') && language.singleQuoteStrings())
            || (c==QLatin1Char('`') && language.backtickStrings()))
        {
            int end=scanQuotedString(line,pos,c);
            if (end<0)
            {
                out.push_back({pos,n-pos,SyntaxBucket::Literal});
                pos=n;
                break;
            }
            out.push_back({pos,end-pos,SyntaxBucket::Literal});
            pos=end;
            continue;
        }

        // 5. XML/HTML tag structure -- own bucket assignment, bypasses the word table entirely
        //    (SyntaxDialect::Xml's language descriptor carries no table: every tag/attribute name
        //    is coloured purely by position, never by a lookup miss).
        if (language.dialect()==SyntaxDialect::Xml && c==QLatin1Char('<'))
        {
            int j=pos+1;
            if (j<n && line[j]==QLatin1Char('/'))
            {
                ++j;
            }
            int nameStart=j;
            while (j<n && (line[j].isLetterOrNumber() || line[j]==QLatin1Char('-')
                            || line[j]==QLatin1Char(':') || line[j]==QLatin1Char('_')))
            {
                ++j;
            }
            if (j>nameStart)
            {
                out.push_back({nameStart,j-nameStart,SyntaxBucket::Keyword});
            }
            insideTag=true;
            pos=j;
            continue;
        }
        if (language.dialect()==SyntaxDialect::Xml && c==QLatin1Char('>'))
        {
            insideTag=false;
            ++pos;
            continue;
        }

        // 6. numeric literal.
        if (c.isDigit())
        {
            int end=scanNumber(line,pos);
            if (end>pos)
            {
                out.push_back({pos,end-pos,SyntaxBucket::Literal});
                pos=end;
                continue;
            }
        }

        // 7. identifier -- word-table lookup, then dialect context (CSS selector/property, XML
        //    attribute), then the `(`-callable heuristic, in that order; Text (no span) if none
        //    of those apply.
        if (isIdentStart(c))
        {
            int idStart=pos;
            int j=pos+1;
            while (j<n && isIdentChar(line[j],language.dialect()))
            {
                ++j;
            }

            std::optional<SyntaxBucket> bucket;

            if (language.dialect()==SyntaxDialect::Css)
            {
                if (idStart>0 && (line[idStart-1]==QLatin1Char('.') || line[idStart-1]==QLatin1Char('#')))
                {
                    bucket=SyntaxBucket::Type; // selector
                }
                else
                {
                    int k=j;
                    while (k<n && line[k]==QLatin1Char(' '))
                    {
                        ++k;
                    }
                    if (k<n && line[k]==QLatin1Char(':'))
                    {
                        bucket=SyntaxBucket::Keyword; // property name
                    }
                }
            }
            else if (language.dialect()==SyntaxDialect::Xml && insideTag)
            {
                int k=j;
                while (k<n && line[k]==QLatin1Char(' '))
                {
                    ++k;
                }
                if (k<n && line[k]==QLatin1Char('='))
                {
                    bucket=SyntaxBucket::Callable; // attribute name
                }
            }

            if (!bucket)
            {
                bucket=language.lookupWord(line.mid(idStart,j-idStart));
            }
            if (!bucket)
            {
                int k=j;
                while (k<n && line[k]==QLatin1Char(' '))
                {
                    ++k;
                }
                if (k<n && line[k]==QLatin1Char('('))
                {
                    bucket=SyntaxBucket::Callable;
                }
            }

            if (bucket)
            {
                out.push_back({idStart,j-idStart,*bucket});
            }
            pos=j;
            continue;
        }

        // 8. anything else (punctuation, whitespace) -- left as unformatted primary text.
        ++pos;
    }

    return SyntaxNoCodeState;
}

//--------------------------------------------------------------------------

std::vector<SyntaxSpan> tokenizeSyntaxLine(const SyntaxLanguage& language, QStringView line,
                                           int previousState)
{
    std::vector<SyntaxSpan> out;
    tokenizeSyntaxLine(language,line,previousState,out);
    return out;
}

UISE_DESKTOP_NAMESPACE_END
