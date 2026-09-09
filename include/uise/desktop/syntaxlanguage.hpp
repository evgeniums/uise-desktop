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

/** @file uise/desktop/syntaxlanguage.hpp
*
*  Declares SyntaxLanguage and SyntaxLanguageRegistry (task-message-formatting-plan.md, Stage 3).
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SYNTAX_LANGUAGE_HPP
#define UISE_DESKTOP_SYNTAX_LANGUAGE_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <QString>
#include <QStringList>
#include <QStringView>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/syntaxtheme.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief One word -> bucket mapping entry in a language's seeded table (syntaxlanguagedata.cpp),
 *  sorted by `text` and deduplicated -- see that file's own top-of-file comment for the port and
 *  collision-precedence rules that produced it.
 */
struct SyntaxWord
{
    std::string_view text;
    SyntaxBucket bucket;
};

/**
 * @brief Non-owning view over a language's constexpr word table.
 *
 * A plain pointer+size pair rather than std::span: this repo does not otherwise commit to
 * C++20, and the whole point of the constexpr tables in syntaxlanguagedata.cpp is a zero-
 * allocation, zero-runtime-initialization lookup, which a pointer+size pair gives just as well.
 */
struct SyntaxWordTable
{
    const SyntaxWord* data=nullptr;
    std::size_t count=0;

    const SyntaxWord* begin() const noexcept
    {
        return data;
    }

    const SyntaxWord* end() const noexcept
    {
        return data+count;
    }
};

/**
 * @brief Extra per-dialect scanning a language needs beyond the generic word/string/number/
 *  comment pass -- see SyntaxTokenizer, which branches on this to run one bounded extra pass
 *  (CSS selector/property, YAML/INI key, Make target, Asm label, XML tag/attribute) on top of
 *  the generic scan rather than instead of it.
 */
enum class SyntaxDialect : uint8_t
{
    Generic,
    Css,
    Yaml,
    Xml,
    Make,
    Asm,
    Ini
};

/**
 * @brief Lexical description of one programming/markup language for SyntaxTokenizer/
 *  SyntaxHighlighter -- a word table plus the handful of scanning traits that differ between
 *  languages (comment syntax, string delimiters, case sensitivity).
 *
 * A value type over data that outlives it (the constexpr word table in syntaxlanguagedata.cpp,
 * string literals for id/name) -- cheap to copy, and every instance in SyntaxLanguageRegistry is
 * built once at registry-construction time and never mutated afterwards. Traits are set through
 * the fluent with...()-style setters below rather than a single large constructor, since most
 * languages only override two or three of them; SyntaxLanguageRegistry::registerBuiltins() is
 * the one place that chains them.
 */
class UISE_DESKTOP_EXPORT SyntaxLanguage
{
    // Only SyntaxLanguageRegistry::registerLanguage() may renumber a language -- to the index
    // its position in the registry's own storage actually ends up at (a language descriptor is
    // constructed with a placeholder index=0 by every SyntaxLanguageRegistry::registerBuiltins()
    // call site, since the caller does not know its final slot up front).
    friend class SyntaxLanguageRegistry;

    public:

        SyntaxLanguage(
                int index,
                QString id,
                QString name,
                SyntaxWordTable words,
                SyntaxDialect dialect=SyntaxDialect::Generic
            ) : m_index(index),
                m_id(std::move(id)),
                m_name(std::move(name)),
                m_words(words),
                m_dialect(dialect)
        {}

        //! Registry-assigned, append-only index -- embedded directly in
        //! QTextBlock::userState() by SyntaxHighlighter (see syntaxtokenizer.hpp's own doc
        //! comment on the state encoding), so it must never change once assigned.
        int index() const noexcept
        {
            return m_index;
        }

        const QString& id() const noexcept
        {
            return m_id;
        }

        const QString& name() const noexcept
        {
            return m_name;
        }

        const SyntaxWordTable& words() const noexcept
        {
            return m_words;
        }

        SyntaxDialect dialect() const noexcept
        {
            return m_dialect;
        }

        SyntaxLanguage& setLineComment(std::string_view token, std::string_view token2={}) noexcept
        {
            m_lineComment=token;
            m_lineComment2=token2;
            return *this;
        }

        std::string_view lineComment() const noexcept
        {
            return m_lineComment;
        }

        std::string_view lineComment2() const noexcept
        {
            return m_lineComment2;
        }

        SyntaxLanguage& setBlockComment(std::string_view start, std::string_view end) noexcept
        {
            m_blockCommentStart=start;
            m_blockCommentEnd=end;
            return *this;
        }

        std::string_view blockCommentStart() const noexcept
        {
            return m_blockCommentStart;
        }

        std::string_view blockCommentEnd() const noexcept
        {
            return m_blockCommentEnd;
        }

        SyntaxLanguage& setNestableBlockComments(bool enable=true) noexcept
        {
            m_nestableBlockComments=enable;
            return *this;
        }

        bool nestableBlockComments() const noexcept
        {
            return m_nestableBlockComments;
        }

        SyntaxLanguage& setSingleQuoteStrings(bool enable) noexcept
        {
            m_singleQuoteStrings=enable;
            return *this;
        }

        bool singleQuoteStrings() const noexcept
        {
            return m_singleQuoteStrings;
        }

        SyntaxLanguage& setDoubleQuoteStrings(bool enable) noexcept
        {
            m_doubleQuoteStrings=enable;
            return *this;
        }

        bool doubleQuoteStrings() const noexcept
        {
            return m_doubleQuoteStrings;
        }

        //! JS/TS template literals, Go raw strings -- a third string delimiter, never triple-
        //! quoted (that's tripleQuoteStrings(), Python's construct, a different mechanism).
        SyntaxLanguage& setBacktickStrings(bool enable=true) noexcept
        {
            m_backtickStrings=enable;
            return *this;
        }

        bool backtickStrings() const noexcept
        {
            return m_backtickStrings;
        }

        //! Python's `"""`/`'''` -- both quote characters are always accepted together when this
        //! is set, since Python itself does not distinguish them for this purpose.
        SyntaxLanguage& setTripleQuoteStrings(bool enable=true) noexcept
        {
            m_tripleQuoteStrings=enable;
            return *this;
        }

        bool tripleQuoteStrings() const noexcept
        {
            return m_tripleQuoteStrings;
        }

        //! SQL only, among the built-ins -- SEELCT/select/SeLeCt all resolve to the same bucket.
        //! Relies on the word table for this language having been generated in a single fixed
        //! case (SyntaxLanguageRegistry::registerBuiltins()'s own comment has the detail on why
        //! that keeps the binary search in lookupWord() valid).
        SyntaxLanguage& setCaseInsensitiveWords(bool enable=true) noexcept
        {
            m_caseInsensitiveWords=enable;
            return *this;
        }

        bool caseInsensitiveWords() const noexcept
        {
            return m_caseInsensitiveWords;
        }

        /**
         * @brief Look up a word in this language's sorted table.
         * @param word Candidate identifier, as scanned by SyntaxTokenizer -- NOT required to be
         *  null-terminated or otherwise owned; no copy is made.
         * @return The bucket the word belongs to, or empty if the table has no entry for it (the
         *  caller then falls through to the `(`-callable heuristic, then to plain text).
         */
        std::optional<SyntaxBucket> lookupWord(QStringView word) const;

    private:

        void setIndex(int index) noexcept
        {
            m_index=index;
        }

        int m_index;
        QString m_id;
        QString m_name;
        SyntaxWordTable m_words;
        SyntaxDialect m_dialect;

        std::string_view m_lineComment{};
        std::string_view m_lineComment2{};
        std::string_view m_blockCommentStart{};
        std::string_view m_blockCommentEnd{};
        bool m_nestableBlockComments=false;
        bool m_singleQuoteStrings=true;
        bool m_doubleQuoteStrings=true;
        bool m_backtickStrings=false;
        bool m_tripleQuoteStrings=false;
        bool m_caseInsensitiveWords=false;
};

/**
 * @brief Registry of built-in and host-registered SyntaxLanguage descriptors, resolving a
 *  fenced code block's language tag (e.g. from `QTextFormat::BlockCodeLanguage`) to one.
 *
 * Singleton, same shape as Style::instance() (src/style.cpp's own function-local static). Built-
 * ins are constructed once, lazily, on first instance() call; SyntaxLanguage::index() values are
 * therefore stable for the lifetime of the process once assigned, which is what
 * SyntaxTokenizer's block-state encoding depends on.
 */
class UISE_DESKTOP_EXPORT SyntaxLanguageRegistry
{
    public:

        static SyntaxLanguageRegistry& instance();

        /**
         * @brief The language every SyntaxLanguageRegistry is guaranteed to contain at this
         *  fixed index, used by SyntaxTokenizer as the "no BlockCodeLanguage / caller looked up
         *  nothing" case and as SyntaxHighlighter's own reset sentinel -- see
         *  syntaxtokenizer.hpp's state-encoding comment.
         */
        constexpr static int GenericLanguageIndex=0;

        /**
         * @brief Resolve a fenced code block's language tag to a SyntaxLanguage.
         * @param tag As found in `QTextFormat::BlockCodeLanguage` (e.g. "cpp", "C++", " Python ",
         *  ".py") or typed by a host directly. Normalised by trimming, lower-casing, and
         *  stripping one leading '.', then matched against every registered id/alias.
         * @return The matching language, or nullptr if `tag` is empty (an untagged fence gets no
         *  highlighting at all -- decision 4) -- NEVER nullptr for a non-empty, unrecognised tag,
         *  which resolves to the generic fallback (GenericLanguageIndex) instead, so callers never
         *  need a second null check for that case.
         */
        const SyntaxLanguage* find(QStringView tag) const;

        //! Look up a language by its own registered index (SyntaxLanguage::index()), as decoded
        //! back out of a QTextBlock::userState() by SyntaxHighlighter. Returns nullptr for an
        //! index that was never registered (defensive only -- SyntaxHighlighter should never
        //! construct one).
        const SyntaxLanguage* byIndex(int index) const;

        /**
         * @brief Register a host-supplied language. Always APPENDS -- see SyntaxLanguage::index()'s
         *  own doc comment for why an index must never be reassigned or reused once handed out.
         * @param aliases Extra tags this language should also resolve from, alongside `id()`
         *  itself. If an alias is already claimed (by a built-in or an earlier host
         *  registration), the new registration wins the ALIAS lookup only -- the previously
         *  registered language keeps its own index and stays reachable via byIndex().
         * @return The new language's assigned index -- stable for the lifetime of the process.
         *  DO NOT unregister languages once content in a live document may reference their index.
         */
        int registerLanguage(SyntaxLanguage language, const QStringList& aliases={});

        //! Every registered language's display name(), in registration order (built-ins first,
        //! then host-registered ones) -- for a demo/settings language picker. Position i in this
        //! list corresponds to byIndex(i) -- see demo/messageformatting/main.cpp's own language
        //! picker for that exact pairing.
        QStringList names() const;

    private:

        SyntaxLanguageRegistry();

        void registerBuiltins();

        std::vector<SyntaxLanguage> m_languages;
        std::map<QString,std::size_t> m_aliasToLanguage;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SYNTAX_LANGUAGE_HPP
