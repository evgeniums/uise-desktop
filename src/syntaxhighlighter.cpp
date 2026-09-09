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

/** @file uise/desktop/syntaxhighlighter.cpp
*
*  Defines SyntaxHighlighter.
*
*/

/****************************************************************************/

#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>

#include <uise/desktop/style.hpp>
#include <uise/desktop/syntaxhighlighter.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
{
    init();
}

//--------------------------------------------------------------------------

void SyntaxHighlighter::init()
{
    refreshColors();
    // A generous starting capacity for one code line's span count -- avoids the first few
    // highlightBlock() calls each growing tokenizeSyntaxLine()'s out-vector from empty.
    m_spans.reserve(64);
}

//--------------------------------------------------------------------------

void SyntaxHighlighter::refreshColors()
{
    for (std::size_t i=0;i<m_formats.size();++i)
    {
        auto bucket=static_cast<SyntaxBucket>(i);
        QTextCharFormat format;
        // SyntaxBucket::Text has no JSON bucket (syntaxBucketName() returns empty for it by
        // design) -- left as a default-constructed, empty QTextCharFormat, i.e. "apply no
        // format", which is correct even though no SyntaxSpan ever actually carries Text as its
        // bucket (see this class's own header doc comment).
        auto name=syntaxBucketName(bucket);
        if (!name.isEmpty())
        {
            auto color=Style::instance().syntaxColor(name);
            if (color)
            {
                format.setForeground(*color);
            }
        }
        m_formats[i]=format;
    }
}

//--------------------------------------------------------------------------

void SyntaxHighlighter::highlightBlock(const QString& text)
{
    // highlightBlock() must never touch the document itself (QTextCursor writes, setHtml(), ...)
    // -- reformatBlock() asserts against exactly that kind of recursive re-entry
    // (qsyntaxhighlighter.cpp). Everything below only reads currentBlock()'s own format and
    // calls setFormat()/setCurrentBlockState(), both of which are the sanctioned mutation points
    // for a QSyntaxHighlighter subclass.
    auto blockFormat=currentBlock().blockFormat();
    if (!blockFormat.hasProperty(QTextFormat::BlockCodeLanguage))
    {
        setCurrentBlockState(SyntaxNoCodeState);
        return;
    }

    auto tag=blockFormat.stringProperty(QTextFormat::BlockCodeLanguage);
    if (tag.isEmpty())
    {
        // Property present but empty: QTextMarkdownImporter sets BlockCodeLanguage
        // unconditionally on every code block regardless of fence info string
        // (qtextmarkdownimporter.cpp), and a host's own raw `<pre class="language-">` is not
        // filtered by markdownToHtml() either. Treated identically to "not a code block" --
        // decision 4's "untagged fence gets no highlighting" applies here too.
        setCurrentBlockState(SyntaxNoCodeState);
        return;
    }

    const SyntaxLanguage* language=nullptr;
    if (m_lastLanguage!=nullptr && m_lastTag==tag)
    {
        // Adjacent lines of the same fence resolve to the same tag over and over during a full
        // rehighlight() pass -- skip SyntaxLanguageRegistry::find()'s own normalisation/lookup.
        language=m_lastLanguage;
    }
    else
    {
        language=SyntaxLanguageRegistry::instance().find(tag);
        m_lastTag=tag;
        m_lastLanguage=language;
    }

    if (language==nullptr)
    {
        // find() only returns nullptr for an empty tag, already handled above -- defensive only.
        setCurrentBlockState(SyntaxNoCodeState);
        return;
    }

    auto newState=tokenizeSyntaxLine(*language,QStringView(text),previousBlockState(),m_spans);
    setCurrentBlockState(newState);

    for (const auto& span : m_spans)
    {
        setFormat(span.start,span.length,m_formats[static_cast<std::size_t>(span.bucket)]);
    }
}

UISE_DESKTOP_NAMESPACE_END
