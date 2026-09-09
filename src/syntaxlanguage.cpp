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

/** @file uise/desktop/syntaxlanguage.cpp
*
*  Defines SyntaxLanguage and SyntaxLanguageRegistry.
*
*/

/****************************************************************************/

#include <algorithm>

#include <uise/desktop/syntaxlanguage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

// Word tables are generated, pure-data constexpr arrays in syntaxlanguagedata.cpp -- deliberately
// exposed through plain forward-declared free functions rather than a header (this repo has no
// headers under src/, and these are implementation detail of registerBuiltins() below, nowhere
// else). Ordinary external linkage across the two translation units does the rest.
namespace langdata {

SyntaxWordTable cppWords();
SyntaxWordTable pythonWords();
SyntaxWordTable javascriptWords();
SyntaxWordTable typescriptWords();
SyntaxWordTable shellWords();
SyntaxWordTable phpWords();
SyntaxWordTable qmlWords();
SyntaxWordTable rustWords();
SyntaxWordTable javaWords();
SyntaxWordTable csharpWords();
SyntaxWordTable goWords();
SyntaxWordTable vWords();
SyntaxWordTable sqlWords();
SyntaxWordTable jsonWords();
SyntaxWordTable cssWords();
SyntaxWordTable yamlWords();
SyntaxWordTable vexWords();
SyntaxWordTable cmakeWords();
SyntaxWordTable makeWords();
SyntaxWordTable rhaiWords();
SyntaxWordTable luaWords();
SyntaxWordTable asmWords();

}

//--------------------------------------------------------------------------

namespace {

//! Compares an ASCII table word against a (possibly non-ASCII) QStringView candidate with no
//! allocation on either side -- called once per identifier scanned, so this is the hot path.
//! Non-ASCII candidate characters simply compare unequal to every table entry, which is correct:
//! every seeded word is plain ASCII.
int compareAsciiWord(std::string_view a, QStringView b, bool caseInsensitive) noexcept
{
    auto aLen=a.size();
    auto bLen=static_cast<std::size_t>(b.size());
    auto n=std::min(aLen,bLen);
    for (std::size_t i=0;i<n;++i)
    {
        char ca=a[i];
        char16_t cb=b[static_cast<qsizetype>(i)].unicode();
        if (caseInsensitive)
        {
            if (ca>='a' && ca<='z')
            {
                ca=static_cast<char>(ca-'a'+'A');
            }
            if (cb>=u'a' && cb<=u'z')
            {
                cb=static_cast<char16_t>(cb-u'a'+u'A');
            }
        }
        auto cav=static_cast<char16_t>(static_cast<unsigned char>(ca));
        if (cav<cb) return -1;
        if (cav>cb) return 1;
    }
    if (aLen<bLen) return -1;
    if (aLen>bLen) return 1;
    return 0;
}

}

//--------------------------------------------------------------------------

std::optional<SyntaxBucket> SyntaxLanguage::lookupWord(QStringView word) const
{
    if (word.isEmpty() || m_words.count==0)
    {
        return std::optional<SyntaxBucket>{};
    }

    // Binary search: valid because syntaxlanguagedata.cpp's tables are sorted by their own
    // (fixed-case) text, and for the one caseInsensitiveWords() language (SQL) that text is
    // itself already uniformly upper-case, so folding both sides to upper-case during the
    // search preserves the same ordering the table was sorted under -- see
    // SyntaxLanguageRegistry::registerBuiltins()'s comment on the SQL table for the detail.
    std::size_t lo=0;
    std::size_t hi=m_words.count;
    while (lo<hi)
    {
        std::size_t mid=lo+(hi-lo)/2;
        const auto& entry=m_words.data[mid];
        int cmp=compareAsciiWord(entry.text,word,m_caseInsensitiveWords);
        if (cmp==0)
        {
            return entry.bucket;
        }
        if (cmp<0)
        {
            lo=mid+1;
        }
        else
        {
            hi=mid;
        }
    }
    return std::optional<SyntaxBucket>{};
}

//--------------------------------------------------------------------------

SyntaxLanguageRegistry& SyntaxLanguageRegistry::instance()
{
    static SyntaxLanguageRegistry inst;
    return inst;
}

//--------------------------------------------------------------------------

SyntaxLanguageRegistry::SyntaxLanguageRegistry()
{
    registerBuiltins();
}

//--------------------------------------------------------------------------

namespace {

QString normalizeTag(QStringView tag)
{
    auto s=tag.trimmed().toString().toLower();
    if (s.startsWith(QLatin1Char('.')))
    {
        s=s.mid(1);
    }
    return s;
}

}

//--------------------------------------------------------------------------

const SyntaxLanguage* SyntaxLanguageRegistry::find(QStringView tag) const
{
    if (tag.trimmed().isEmpty())
    {
        // Untagged fence -- no highlighting at all (task-message-formatting-plan.md, Stage 3,
        // decision 4). This is the ONLY case find() returns nullptr for.
        return nullptr;
    }

    auto norm=normalizeTag(tag);
    auto it=m_aliasToLanguage.find(norm);
    if (it!=m_aliasToLanguage.end())
    {
        return &m_languages[it->second];
    }

    // Recognised-but-unknown tag (e.g. ```kotlin) -- the generic fallback, never nullptr, so
    // callers never need a second null check for this case (see this method's own doc comment).
    return &m_languages[static_cast<std::size_t>(GenericLanguageIndex)];
}

//--------------------------------------------------------------------------

const SyntaxLanguage* SyntaxLanguageRegistry::byIndex(int index) const
{
    if (index<0 || static_cast<std::size_t>(index)>=m_languages.size())
    {
        return nullptr;
    }
    return &m_languages[static_cast<std::size_t>(index)];
}

//--------------------------------------------------------------------------

int SyntaxLanguageRegistry::registerLanguage(SyntaxLanguage language, const QStringList& aliases)
{
    auto index=static_cast<int>(m_languages.size());
    language.setIndex(index);

    auto id=language.id();
    m_languages.emplace_back(std::move(language));

    m_aliasToLanguage[normalizeTag(id)]=static_cast<std::size_t>(index);
    for (const auto& alias : aliases)
    {
        m_aliasToLanguage[normalizeTag(alias)]=static_cast<std::size_t>(index);
    }

    return index;
}

//--------------------------------------------------------------------------

QStringList SyntaxLanguageRegistry::names() const
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(m_languages.size()));
    for (const auto& lang : m_languages)
    {
        result<<lang.name();
    }
    return result;
}

//--------------------------------------------------------------------------

void SyntaxLanguageRegistry::registerBuiltins()
{
    // Index 0, no word table, no aliases of its own -- the generic fallback used for a
    // recognised-but-unmapped tag (find()'s own doc comment) AND as the "strings/numbers/
    // //,#,/* */ comments only, no keywords" pass for decision 4. Two line-comment styles
    // covers the two most common conventions at once for content with an unknown/no tag.
    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("generic"),QStringLiteral("Generic"),SyntaxWordTable{})
            .setLineComment("//","#")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("cpp"),QStringLiteral("C/C++"),langdata::cppWords())
            .setLineComment("//")
            .setBlockComment("/*","*/"),
        {QStringLiteral("c"),QStringLiteral("c++"),QStringLiteral("cxx"),QStringLiteral("cc"),
         QStringLiteral("h"),QStringLiteral("hpp"),QStringLiteral("hh"),QStringLiteral("hxx")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("python"),QStringLiteral("Python"),langdata::pythonWords())
            .setLineComment("#")
            .setTripleQuoteStrings(),
        {QStringLiteral("py")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("javascript"),QStringLiteral("JavaScript"),langdata::javascriptWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
            .setBacktickStrings(),
        {QStringLiteral("js")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("typescript"),QStringLiteral("TypeScript"),langdata::typescriptWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
            .setBacktickStrings(),
        {QStringLiteral("ts")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("shell"),QStringLiteral("Shell"),langdata::shellWords())
            .setLineComment("#"),
        {QStringLiteral("sh"),QStringLiteral("bash"),QStringLiteral("zsh")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("php"),QStringLiteral("PHP"),langdata::phpWords())
            .setLineComment("//","#")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("qml"),QStringLiteral("QML"),langdata::qmlWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("rust"),QStringLiteral("Rust"),langdata::rustWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
            .setNestableBlockComments(),
        {QStringLiteral("rs")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("java"),QStringLiteral("Java"),langdata::javaWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("csharp"),QStringLiteral("C#"),langdata::csharpWords())
            .setLineComment("//")
            .setBlockComment("/*","*/"),
        {QStringLiteral("cs"),QStringLiteral("c#")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("go"),QStringLiteral("Go"),langdata::goWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
            .setBacktickStrings(), // Go raw string literals
        {QStringLiteral("golang")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("v"),QStringLiteral("V"),langdata::vWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
    );

    // SQL's word table is generated entirely upper-case (upstream's own convention) --
    // caseInsensitiveWords() folds a query word to upper-case before the binary search, which
    // stays consistent with the table's own (already upper-case) sort order. See lookupWord()'s
    // comment for why that specific combination is safe.
    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("sql"),QStringLiteral("SQL"),langdata::sqlWords())
            .setLineComment("--")
            .setBlockComment("/*","*/")
            .setCaseInsensitiveWords()
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("json"),QStringLiteral("JSON"),langdata::jsonWords())
            .setSingleQuoteStrings(false) // strict JSON strings are double-quoted only
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("css"),QStringLiteral("CSS"),langdata::cssWords(),SyntaxDialect::Css)
            .setBlockComment("/*","*/") // CSS has no line-comment syntax
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("yaml"),QStringLiteral("YAML"),langdata::yamlWords(),SyntaxDialect::Yaml)
            .setLineComment("#"),
        {QStringLiteral("yml")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("vex"),QStringLiteral("VEX"),langdata::vexWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("cmake"),QStringLiteral("CMake"),langdata::cmakeWords())
            .setLineComment("#"),
        {QStringLiteral("cmakelists")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("make"),QStringLiteral("Make"),langdata::makeWords(),SyntaxDialect::Make)
            .setLineComment("#"),
        {QStringLiteral("makefile")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("rhai"),QStringLiteral("Rhai"),langdata::rhaiWords())
            .setLineComment("//")
            .setBlockComment("/*","*/")
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("lua"),QStringLiteral("Lua"),langdata::luaWords())
            .setLineComment("--")
            .setBlockComment("--[[","]]")
    );

    // Upstream's own choice of comment character for asm is '#' (qsourcehighliter.cpp); real-
    // world x86 asm pasted into a chat is at least as often NASM/MASM-style ';' -- both are
    // accepted rather than picking one over the other.
    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("asm"),QStringLiteral("Assembly (x86)"),langdata::asmWords(),SyntaxDialect::Asm)
            .setLineComment(";","#"),
        {QStringLiteral("nasm"),QStringLiteral("x86asm")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("ini"),QStringLiteral("INI"),SyntaxWordTable{},SyntaxDialect::Ini)
            .setLineComment("#",";"),
        {QStringLiteral("cfg"),QStringLiteral("conf")}
    );

    registerLanguage(
        SyntaxLanguage(0,QStringLiteral("xml"),QStringLiteral("XML/HTML"),SyntaxWordTable{},SyntaxDialect::Xml)
            .setBlockComment("<!--","-->"),
        {QStringLiteral("html"),QStringLiteral("htm"),QStringLiteral("svg")}
    );
}

UISE_DESKTOP_NAMESPACE_END
