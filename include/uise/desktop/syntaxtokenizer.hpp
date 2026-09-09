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

/** @file uise/desktop/syntaxtokenizer.hpp
*
*  Declares tokenizeSyntaxLine() -- the pure, single-pass code-line lexer behind SyntaxHighlighter
*  (task-message-formatting-plan.md, Stage 3).
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SYNTAX_TOKENIZER_HPP
#define UISE_DESKTOP_SYNTAX_TOKENIZER_HPP

#include <vector>

#include <QStringView>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/syntaxtheme.hpp>
#include <uise/desktop/syntaxlanguage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//! One highlighted run within a single code line, [start,start+length) into that line's own
//! text, never overlapping another span and never spanning more than one QTextBlock (see this
//! file's own state-encoding comment on why a code line is always exactly one block).
struct SyntaxSpan
{
    int start;
    int length;
    SyntaxBucket bucket;
};

/**
 * @brief Sentinel `QTextBlock::userState()` value meaning "not a code line" -- SyntaxHighlighter
 *  sets this on every block outside a fenced code block, and it doubles as tokenizeSyntaxLine()'s
 *  own "no continuation, start fresh" input: a first code line (nothing to continue from) and a
 *  non-code line (nothing TO continue) are the same case from the tokenizer's point of view.
 */
constexpr int SyntaxNoCodeState=-1;

/**
 * @brief Tokenize one source line for syntax highlighting.
 *
 * A left-to-right, single-pass lexer: at each position it dispatches on continuation state ->
 * line comment -> block comment open -> string/triple-quote delimiter -> digit -> identifier ->
 * punctuation, producing non-overlapping spans by construction (see syntaxtokenizer.cpp's own
 * top-of-file comment for why this shape was chosen over porting QSourceHighlite's own multi-
 * pass overwrite scanner). An identifier is looked up in `language`'s word table
 * (SyntaxLanguage::lookupWord()); one that matches nothing but is immediately followed by `(`
 * (spaces allowed in between) is bucketed Callable -- the heuristic satisfying the brief's
 * "Callables" bucket, which the seeded upstream tables have no direct equivalent for.
 *
 * @param language Resolved via SyntaxLanguageRegistry::find(). Its dialect() selects one bounded
 *  extra pass (CSS selector/property, YAML/INI key, Make target, Asm label, XML tag/attribute) on
 *  top of the generic scan.
 * @param line One QTextBlock's text -- a single physical source line, never more (see this
 *  file's own SyntaxNoCodeState comment: a fenced code block's lines are one QTextBlock each,
 *  task-message-formatting-plan.md Stage 3's verified-ground-truth table).
 * @param previousState The state tokenizeSyntaxLine() returned for the PREVIOUS line, or
 *  SyntaxNoCodeState for a fresh start (the first code line of a fence, or any line preceded by a
 *  DIFFERENT language -- callers do not need to check this themselves: a `previousState` whose
 *  encoded language does not match `language` is treated as fresh internally).
 * @param out Cleared and filled with this line's spans, in ascending, non-overlapping `start`
 *  order. Its capacity is deliberately left untouched by the clear, so a SyntaxHighlighter member
 *  reused across every block in a rehighlight() pass allocates its backing storage once, not once
 *  per line.
 * @return The state to pass as `previousState` for the NEXT line (an opaque, `>=0` encoding of
 *  which multi-line construct -- block comment, Python triple-quoted string -- is still open,
 *  including nesting depth and which language opened it; SyntaxNoCodeState if nothing is open).
 *  Callers other than SyntaxHighlighter should treat this purely as an opaque token to feed back
 *  in, not decode it -- the encoding is a private implementation detail of syntaxtokenizer.cpp.
 */
UISE_DESKTOP_EXPORT int tokenizeSyntaxLine(const SyntaxLanguage& language,
                                            QStringView line,
                                            int previousState,
                                            std::vector<SyntaxSpan>& out);

//! Convenience by-value overload for tests -- allocates a fresh vector, so prefer the out-param
//! form above in a hot loop (SyntaxHighlighter::highlightBlock() runs once per code line of
//! every rehighlight() pass).
UISE_DESKTOP_EXPORT std::vector<SyntaxSpan> tokenizeSyntaxLine(const SyntaxLanguage& language,
                                                                QStringView line,
                                                                int previousState=SyntaxNoCodeState);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SYNTAX_TOKENIZER_HPP
