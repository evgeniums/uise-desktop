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

/** @file src/markdownwriter.cpp
*
*  Non-wrapping markdown writer, see markdownwriter.hpp.
*
*  Derived from Qt 6.9.3 qtbase/src/gui/text/qtextmarkdownwriter.cpp
*  (Copyright (C) 2019 The Qt Company Ltd., LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only).
*  Changes: the 80-column word wrapping is removed, front matter and the item-model table writer
*  are dropped, and Qt-private headers are replaced by public API.
*
*/

/****************************************************************************/

#include <QFontInfo>
#include <QMap>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextStream>
#include <QTextTable>
#include <QUrl>

#include "markdownwriter.hpp"

namespace uise {

namespace detail {

namespace {

using namespace Qt::StringLiterals;

const QChar Space=u' ';
const QChar Tab=u'\t';
const QChar Newline=u'\n';
const QChar CarriageReturn=u'\r';
const QChar LineBreak=u'\x2028';
const QChar DoubleQuote=u'"';
const QChar Backtick=u'`';
const QChar Backslash=u'\\';
const QChar Period=u'.';

int adjacentBackticksCount(const QString& s)
{
    int start=-1;
    const int len=s.size();
    int ret=0;
    for (int i=0;i<len;++i)
    {
        if (s.at(i)==Backtick)
        {
            if (start<0)
            {
                start=i;
            }
        }
        else if (start>=0)
        {
            ret=qMax(ret,i-start);
            start=-1;
        }
    }
    if (len>0 && s.at(len-1)==Backtick)
    {
        ret=qMax(ret,len-start);
    }
    return ret;
}

//! Escape anything at the beginning of a line that a markdown parser would misread, including a
//! period that follows a number (a numbered list item).
void maybeEscapeFirstChar(QString& s)
{
    static const QRegularExpression numericListRe(uR"(\d+([\.)])\s)"_s);
    constexpr auto specialFirstCharacters="#*+-"_L1;

    const QString sTrimmed=s.trimmed();
    if (sTrimmed.isEmpty())
    {
        return;
    }
    const QChar firstChar=sTrimmed.at(0);
    if (specialFirstCharacters.contains(firstChar))
    {
        const int i=s.indexOf(firstChar); // == 0 unless s got trimmed
        s.insert(i,u'\\');
    }
    else
    {
        const auto match=numericListRe.match(s,0,QRegularExpression::NormalMatch,
                                             QRegularExpression::AnchorAtOffsetMatchOption);
        if (match.hasMatch())
        {
            s.insert(match.capturedStart(1),Backslash);
        }
    }
}

//! Escape all backslashes, then any special character that stands alone or prefixes a "word",
//! including the `<` that starts an HTML tag.
void escapeSpecialCharacters(QString& s)
{
    static const QRegularExpression spaceRe(uR"(\s+)"_s);
    static const QRegularExpression specialRe(uR"([<!*[`&]+[/\w])"_s);

    s.replace("\\"_L1,"\\\\"_L1);

    int i=0;
    while (i>=0)
    {
        const int j=s.indexOf(specialRe,i);
        if (j>=0)
        {
            s.insert(j,Backslash);
            i=j+3;
        }
        i=s.indexOf(spaceRe,i);
        if (i>=0)
        {
            ++i; // past the whitespace, if found
        }
    }
}

struct LineEndPositions
{
    const QChar* lineEnd;
    const QChar* nextLineBegin;
};

LineEndPositions findLineEnd(const QChar* begin, const QChar* end)
{
    LineEndPositions result{end,end};

    while (begin<end)
    {
        if (*begin==Newline)
        {
            result.lineEnd=begin;
            result.nextLineBegin=begin+1;
            break;
        }
        if (*begin==CarriageReturn)
        {
            result.lineEnd=begin;
            result.nextLineBegin=begin+1;
            if (((begin+1)<end) && begin[1]==Newline)
            {
                ++result.nextLineBegin;
            }
            break;
        }
        ++begin;
    }

    return result;
}

bool isBlankLine(const QChar* begin, const QChar* end)
{
    while (begin<end)
    {
        if (*begin!=Space && *begin!=Tab)
        {
            return false;
        }
        ++begin;
    }
    return true;
}

QString createLinkTitle(const QString& title)
{
    QString result;
    result.reserve(title.size()+2);
    result+=DoubleQuote;

    const QChar* data=title.data();
    const QChar* end=data+title.size();

    while (data<end)
    {
        const auto lineEndPositions=findLineEnd(data,end);

        if (!isBlankLine(data,lineEndPositions.lineEnd))
        {
            while (data<lineEndPositions.nextLineBegin)
            {
                if (*data==DoubleQuote)
                {
                    result+=Backslash;
                }
                result+=*data;
                ++data;
            }
        }

        data=lineEndPositions.nextLineBegin;
    }

    result+=DoubleQuote;
    return result;
}

class MarkdownWriter
{
    public:

        explicit MarkdownWriter(QTextStream& stream) : m_stream(stream)
        {}

        void writeAll(const QTextDocument* document)
        {
            writeFrame(document->rootFrame());
        }

    private:

        struct ListInfo
        {
            bool loose=false;
        };

        void writeFrame(const QTextFrame* frame);
        int writeBlock(const QTextBlock& block, bool ignoreFormat, bool ignoreEmpty);
        ListInfo listInfo(QTextList* list);
        void setLinePrefixForBlockQuote(int level);

        QTextStream& m_stream;
        QMap<QTextList*,ListInfo> m_listInfo;
        QString m_linePrefix;
        QString m_codeBlockFence;
        int m_wrappedLineIndent=0;
        int m_lastListIndent=1;
        bool m_doubleNewlineWritten=false;
        bool m_linePrefixWritten=false;
        bool m_indentedCodeBlock=false;
        bool m_fencedCodeBlock=false;
};

void MarkdownWriter::writeFrame(const QTextFrame* frame)
{
    Q_ASSERT(frame);
    const QTextTable* table=qobject_cast<const QTextTable*>(frame);
    QTextFrame::iterator iterator=frame->begin();
    QTextFrame* child=nullptr;
    int tableRow=-1;
    bool lastWasList=false;
    QList<int> tableColumnWidths;
    if (table)
    {
        tableColumnWidths.resize(table->columns());
        for (int col=0;col<table->columns();++col)
        {
            for (int row=0;row<table->rows();++row)
            {
                QTextTableCell cell=table->cellAt(row,col);
                int cellTextLen=0;
                auto it=cell.begin();
                while (it!=cell.end())
                {
                    QTextBlock block=it.currentBlock();
                    if (block.isValid())
                    {
                        cellTextLen+=block.text().size();
                    }
                    ++it;
                }
                if (cell.columnSpan()==1 && tableColumnWidths[col]<cellTextLen)
                {
                    tableColumnWidths[col]=cellTextLen;
                }
            }
        }
    }
    while (!iterator.atEnd())
    {
        if (iterator.currentFrame() && child!=iterator.currentFrame())
        {
            writeFrame(iterator.currentFrame());
        }
        else
        {
            // no frame, it's a block
            QTextBlock block=iterator.currentBlock();
            // Look ahead and detect some cases when we should suppress needless blank lines, when
            // there will be a big change in block format
            bool nextIsDifferent=false;
            bool ending=false;
            int blockQuoteIndent=0;
            int nextBlockQuoteIndent=0;
            {
                QTextFrame::iterator next=iterator;
                ++next;
                QTextBlockFormat format=iterator.currentBlock().blockFormat();
                QTextBlockFormat nextFormat=next.currentBlock().blockFormat();
                blockQuoteIndent=format.intProperty(QTextFormat::BlockQuoteLevel);
                nextBlockQuoteIndent=nextFormat.intProperty(QTextFormat::BlockQuoteLevel);
                if (next.atEnd())
                {
                    nextIsDifferent=true;
                    ending=true;
                }
                else
                {
                    if (nextFormat.indent()!=format.indent()
                        || nextFormat.property(QTextFormat::BlockCodeLanguage)
                               !=format.property(QTextFormat::BlockCodeLanguage))
                    {
                        nextIsDifferent=true;
                    }
                }
            }
            if (table)
            {
                QTextTableCell cell=table->cellAt(block.position());
                if (tableRow<cell.row())
                {
                    if (tableRow==0)
                    {
                        m_stream<<Newline;
                        for (int col=0;col<tableColumnWidths.size();++col)
                        {
                            m_stream<<'|'<<QString(tableColumnWidths[col],u'-');
                        }
                        m_stream<<'|';
                    }
                    m_stream<<Newline<<'|';
                    tableRow=cell.row();
                }
            }
            else if (!block.textList())
            {
                if (lastWasList)
                {
                    m_stream<<Newline;
                    m_linePrefixWritten=false;
                }
            }
            int endingCol=writeBlock(block,table && tableRow==0,nextIsDifferent && !block.textList());
            m_doubleNewlineWritten=false;
            if (table)
            {
                QTextTableCell cell=table->cellAt(block.position());
                int paddingLen=-endingCol;
                int spanEndCol=cell.column()+cell.columnSpan();
                for (int col=cell.column();col<spanEndCol;++col)
                {
                    paddingLen+=tableColumnWidths[col];
                }
                if (paddingLen>0)
                {
                    m_stream<<QString(paddingLen,Space);
                }
                for (int col=cell.column();col<spanEndCol;++col)
                {
                    m_stream<<"|";
                }
            }
            else if (m_fencedCodeBlock && ending)
            {
                m_stream<<Newline<<m_linePrefix<<QString(m_wrappedLineIndent,Space)
                        <<m_codeBlockFence<<Newline<<Newline;
                m_codeBlockFence.clear();
            }
            else if (m_indentedCodeBlock && nextIsDifferent)
            {
                m_stream<<Newline<<Newline;
            }
            else if (endingCol>0)
            {
                if (block.textList() || block.blockFormat().hasProperty(QTextFormat::BlockCodeLanguage))
                {
                    m_stream<<Newline;
                    if (block.textList())
                    {
                        m_stream<<m_linePrefix;
                        m_linePrefixWritten=true;
                    }
                }
                else
                {
                    m_stream<<Newline;
                    if (nextBlockQuoteIndent<blockQuoteIndent)
                    {
                        setLinePrefixForBlockQuote(nextBlockQuoteIndent);
                    }
                    m_stream<<m_linePrefix;
                    m_stream<<Newline;
                    m_doubleNewlineWritten=true;
                }
            }
            lastWasList=block.textList();
        }
        child=iterator.currentFrame();
        ++iterator;
    }
    if (table)
    {
        m_stream<<Newline<<Newline;
        m_doubleNewlineWritten=true;
    }
    m_listInfo.clear();
}

MarkdownWriter::ListInfo MarkdownWriter::listInfo(QTextList* list)
{
    if (!m_listInfo.contains(list))
    {
        // decide whether this list is loose or tight
        ListInfo info;
        info.loose=false;
        if (list->count()>1)
        {
            QTextBlock first=list->item(0);
            QTextBlock last=list->item(list->count()-1);
            QTextBlock next=first.next();
            while (next.isValid())
            {
                if (next==last)
                {
                    break;
                }
                if (!next.textList())
                {
                    // A continuation paragraph makes the list "loose": it needs a blank line to
                    // separate that paragraph.
                    info.loose=true;
                    break;
                }
                next=next.next();
            }
        }
        m_listInfo.insert(list,info);
        return info;
    }
    return m_listInfo.value(list);
}

void MarkdownWriter::setLinePrefixForBlockQuote(int level)
{
    m_linePrefix.clear();
    if (level>0)
    {
        m_linePrefix.reserve(level*2);
        for (int i=0;i<level;++i)
        {
            m_linePrefix+=u"> ";
        }
    }
}

int MarkdownWriter::writeBlock(const QTextBlock& block, bool ignoreFormat, bool ignoreEmpty)
{
    if (block.text().isEmpty() && ignoreEmpty)
    {
        return 0;
    }
    QTextBlockFormat blockFmt=block.blockFormat();
    bool missedBlankCodeBlockLine=false;
    const bool codeBlock=blockFmt.hasProperty(QTextFormat::BlockCodeFence)
                         || blockFmt.stringProperty(QTextFormat::BlockCodeLanguage).size()>0
                         || blockFmt.nonBreakableLines();
    const int blockQuoteLevel=blockFmt.intProperty(QTextFormat::BlockQuoteLevel);
    if (m_fencedCodeBlock && !codeBlock)
    {
        m_stream<<m_linePrefix<<m_codeBlockFence<<Newline;
        m_fencedCodeBlock=false;
        m_codeBlockFence.clear();
        m_linePrefixWritten=m_linePrefix.size()>0;
    }
    m_linePrefix.clear();
    if (!blockFmt.headingLevel() && blockQuoteLevel>0)
    {
        setLinePrefixForBlockQuote(blockQuoteLevel);
        if (!m_linePrefixWritten)
        {
            m_stream<<m_linePrefix;
            m_linePrefixWritten=true;
        }
    }
    if (block.textList())
    {
        // it's a list-item
        auto fmt=block.textList()->format();
        const int listLevel=fmt.indent();
        // Negative numbers don't start a list in Markdown, so ignore them.
        const int start=fmt.start()>=0 ? fmt.start() : 1;
        const int number=block.textList()->itemNumber(block)+start;
        QByteArray bullet=" ";
        bool numeric=false;
        switch (fmt.style())
        {
            case QTextListFormat::ListDisc:
                bullet="-";
                m_wrappedLineIndent=2;
                break;
            case QTextListFormat::ListCircle:
                bullet="*";
                m_wrappedLineIndent=2;
                break;
            case QTextListFormat::ListSquare:
                bullet="+";
                m_wrappedLineIndent=2;
                break;
            case QTextListFormat::ListStyleUndefined:
                break;
            case QTextListFormat::ListDecimal:
            case QTextListFormat::ListLowerAlpha:
            case QTextListFormat::ListUpperAlpha:
            case QTextListFormat::ListLowerRoman:
            case QTextListFormat::ListUpperRoman:
                numeric=true;
                m_wrappedLineIndent=4;
                break;
        }
        switch (blockFmt.marker())
        {
            case QTextBlockFormat::MarkerType::Checked:
                bullet+=" [x]";
                break;
            case QTextBlockFormat::MarkerType::Unchecked:
                bullet+=" [ ]";
                break;
            default:
                break;
        }
        const int indentFirstLine=(listLevel-1)*(numeric ? 4 : 2);
        m_wrappedLineIndent+=indentFirstLine;
        if (m_lastListIndent!=listLevel && !m_doubleNewlineWritten && listInfo(block.textList()).loose)
        {
            m_stream<<Newline;
        }
        m_lastListIndent=listLevel;
        QString prefix(indentFirstLine,Space);
        if (numeric)
        {
            QString suffix=fmt.numberSuffix();
            if (suffix.isEmpty())
            {
                suffix=QString(Period);
            }
            QString numberStr=QString::number(number)+suffix+Space;
            if (numberStr.size()==3)
            {
                numberStr+=Space;
            }
            prefix+=numberStr;
        }
        else
        {
            prefix+=QString::fromLatin1(bullet);
            prefix+=Space;
        }
        m_stream<<prefix;
    }
    else if (blockFmt.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
    {
        m_stream<<"- - -\n"; // unambiguous horizontal rule, not an underline under a heading
        return 0;
    }
    else if (codeBlock)
    {
        // It's important to preserve blank lines in code blocks. But blank lines in code blocks
        // inside block quotes are getting preserved anyway (along with the "> " prefix).
        if (!blockFmt.hasProperty(QTextFormat::BlockQuoteLevel))
        {
            missedBlankCodeBlockLine=true; // only if we don't get any fragments below
        }
        if (!m_fencedCodeBlock)
        {
            QString fenceChar=blockFmt.stringProperty(QTextFormat::BlockCodeFence);
            if (fenceChar.isEmpty())
            {
                fenceChar="`"_L1;
            }
            m_codeBlockFence=QString(3,fenceChar.at(0));
            if (blockFmt.hasProperty(QTextFormat::BlockIndent))
            {
                m_codeBlockFence=QString(m_wrappedLineIndent,Space)+m_codeBlockFence;
            }
            // A block quote can contain an indented code block, but not vice-versa.
            m_stream<<m_codeBlockFence<<blockFmt.stringProperty(QTextFormat::BlockCodeLanguage)
                    <<Newline<<m_linePrefix;
            m_fencedCodeBlock=true;
        }
    }
    else if (!blockFmt.indent())
    {
        m_wrappedLineIndent=0;
        if (blockFmt.hasProperty(QTextFormat::BlockCodeLanguage))
        {
            // A block quote can contain an indented code block, but not vice-versa.
            m_linePrefix+=QString(4,Space);
            m_indentedCodeBlock=true;
        }
        if (!m_linePrefixWritten)
        {
            m_stream<<m_linePrefix;
            m_linePrefixWritten=true;
        }
    }
    if (blockFmt.headingLevel())
    {
        m_stream<<QByteArray(blockFmt.headingLevel(),'#')<<' ';
    }

    const QString wrapIndentString=m_linePrefix+QString(m_wrappedLineIndent,Space);
    // Qt tracks the written column to know where to wrap. Nothing wraps here, but the return value
    // (the column the block ended on) still drives table cell padding and the blank-line logic.
    int col=wrapIndentString.size();
    bool mono=false;
    bool startsOrEndsWithBacktick=false;
    bool bold=false;
    bool italic=false;
    bool underline=false;
    bool strikeOut=false;
    QString backticks(Backtick);
    for (QTextBlock::Iterator frag=block.begin();!frag.atEnd();++frag)
    {
        missedBlankCodeBlockLine=false;
        QString fragmentText=frag.fragment().text();
        while (fragmentText.endsWith(Newline))
        {
            fragmentText.chop(1);
        }
        if (!(m_fencedCodeBlock || m_indentedCodeBlock))
        {
            escapeSpecialCharacters(fragmentText);
            maybeEscapeFirstChar(fragmentText);
        }
        if (block.textList())
        {
            // <li>first line</br>continuation</li>
            const QString newlineIndent=QString(Newline)+QString(m_wrappedLineIndent,Space);
            fragmentText.replace(QString(LineBreak),newlineIndent);
        }
        else if (blockFmt.indent()>0)
        {
            // <li>first line<p>continuation</p></li>
            m_stream<<QString(m_wrappedLineIndent,Space);
        }
        else
        {
            fragmentText.replace(LineBreak,Newline);
        }
        startsOrEndsWithBacktick|=fragmentText.startsWith(Backtick) || fragmentText.endsWith(Backtick);
        QTextCharFormat fmt=frag.fragment().charFormat();
        if (fmt.isImageFormat())
        {
            QTextImageFormat ifmt=fmt.toImageFormat();
            QString desc=ifmt.stringProperty(QTextFormat::ImageAltText);
            if (desc.isEmpty())
            {
                desc="image"_L1;
            }
            QString s=QString::fromLatin1("![")+desc+QString::fromLatin1("](")+ifmt.name();
            QString title=ifmt.stringProperty(QTextFormat::ImageTitle);
            if (!title.isEmpty())
            {
                s+=Space;
                s+=DoubleQuote;
                s+=title;
                s+=DoubleQuote;
            }
            s+=u')';
            m_stream<<s;
            col+=s.size();
        }
        else if (fmt.hasProperty(QTextFormat::AnchorHref))
        {
            const auto href=fmt.property(QTextFormat::AnchorHref).toString();
            const bool hasToolTip=fmt.hasProperty(QTextFormat::TextToolTip);
            QString s;
            if (!hasToolTip && href==fragmentText && !QUrl(href,QUrl::StrictMode).scheme().isEmpty())
            {
                s=QString(u'<')+href+QString(u'>');
            }
            else
            {
                s=QString(u'[')+fragmentText+QString::fromLatin1("](")+href;
                if (hasToolTip)
                {
                    s+=Space;
                    s+=createLinkTitle(fmt.property(QTextFormat::TextToolTip).toString());
                }
                s+=u')';
            }
            m_stream<<s;
            col+=s.size();
        }
        else
        {
            QFontInfo fontInfo(fmt.font());
            const bool monoFrag=fontInfo.fixedPitch() || fmt.fontFixedPitch();
            QString markers;
            if (!ignoreFormat)
            {
                if (monoFrag!=mono && !m_indentedCodeBlock && !m_fencedCodeBlock)
                {
                    if (monoFrag)
                    {
                        backticks=QString(adjacentBackticksCount(fragmentText)+1,Backtick);
                    }
                    markers+=backticks;
                    if (startsOrEndsWithBacktick)
                    {
                        markers+=Space;
                    }
                    mono=monoFrag;
                }
                if (!blockFmt.headingLevel() && !mono)
                {
                    if (fontInfo.bold()!=bold)
                    {
                        markers+="**"_L1;
                        bold=fontInfo.bold();
                    }
                    if (fontInfo.italic()!=italic)
                    {
                        markers+=u'*';
                        italic=fontInfo.italic();
                    }
                    if (fontInfo.strikeOut()!=strikeOut)
                    {
                        markers+="~~"_L1;
                        strikeOut=fontInfo.strikeOut();
                    }
                    if (fontInfo.underline()!=underline)
                    {
                        // CommonMark specifies underline as another way to get emphasis (italics),
                        // but md4c allows us to distinguish them; so underlining is supported
                        // (GitHub dialect).
                        markers+=u'_';
                        underline=fontInfo.underline();
                    }
                }
            }
            if (!m_linePrefixWritten && col==wrapIndentString.size())
            {
                m_stream<<m_linePrefix;
                col+=m_linePrefix.size();
            }
            m_stream<<markers<<fragmentText;
            col+=markers.size()+fragmentText.size();
        }
    }
    if (mono)
    {
        if (startsOrEndsWithBacktick)
        {
            m_stream<<Space;
            col+=1;
        }
        m_stream<<backticks;
        col+=backticks.size();
    }
    if (bold)
    {
        m_stream<<"**";
        col+=2;
    }
    if (italic)
    {
        m_stream<<"*";
        col+=1;
    }
    if (underline)
    {
        m_stream<<"_";
        col+=1;
    }
    if (strikeOut)
    {
        m_stream<<"~~";
        col+=2;
    }
    if (missedBlankCodeBlockLine)
    {
        m_stream<<Newline;
    }
    m_linePrefixWritten=false;
    return col;
}

} // anonymous namespace

QString documentToMarkdown(const QTextDocument* document)
{
    QString result;
    QTextStream stream(&result);
    MarkdownWriter writer(stream);
    writer.writeAll(document);
    stream.flush();
    return result;
}

} // namespace detail

} // namespace uise
