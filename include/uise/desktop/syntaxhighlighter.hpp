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

/** @file uise/desktop/syntaxhighlighter.hpp
*
*  Declares SyntaxHighlighter (task-message-formatting-plan.md, Stage 3).
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SYNTAX_HIGHLIGHTER_HPP
#define UISE_DESKTOP_SYNTAX_HIGHLIGHTER_HPP

#include <array>
#include <vector>

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/syntaxtheme.hpp>
#include <uise/desktop/syntaxlanguage.hpp>
#include <uise/desktop/syntaxtokenizer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Colours fenced code blocks (`QTextFormat::BlockCodeLanguage`) inside any QTextDocument.
 *
 * Attachable to any document, not just ChatMessageTextBrowser's -- see
 * ChatMessageTextBrowser::setHtmlContent()/applyDocumentStyle() (src/chatmessagetext.cpp) for the
 * one production consumer and its own doc comments for the two Qt gotchas that govern how it is
 * attached and re-triggered (rehighlightPending eating the first pass; document identity
 * surviving setHtml()).
 *
 * Colour-only by design (task-message-formatting-plan.md, Stage 3, decision 3): never touches
 * weight/italic/family, so a code block's glyph metrics -- and therefore
 * AbstractChatMessageText::maxBubbleWidth negotiation -- are identical with or without this
 * attached. QSyntaxHighlighter formats MERGE onto the fragment's existing char format rather than
 * replacing it (verified against Qt 6.9.0's QTextEngine::resolveFormats()), so this also never
 * fights ChatMessageTextBrowser::setAnchorUnderline() or the `Menlo/Consolas/monospace` family
 * messagetext.css already sets on `pre`.
 */
class UISE_DESKTOP_EXPORT SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

    public:

        //! Attaches immediately (QSyntaxHighlighter's own constructor calls setDocument()) -- if
        //! `document` is non-empty this sets Qt's own `rehighlightPending` flag and queues a
        //! deferred first pass rather than running one now (qsyntaxhighlighter.cpp's own
        //! `setDocument()`/`_q_reformatBlocks()`), so a caller attaching onto content that is
        //! already loaded MUST follow up with an explicit rehighlight() call -- see
        //! ChatMessageTextBrowser::setHtmlContent() (src/chatmessagetext.cpp) for why that call is
        //! mandatory, not an optimisation.
        explicit SyntaxHighlighter(QTextDocument* document);

        /**
         * @brief Re-pull the five bucket colours from Style::instance().syntaxColor() into this
         *  highlighter's cached QTextCharFormat set.
         *
         * Deliberately does NOT call rehighlight() itself -- the caller decides whether a replay
         * (ChatMessageTextBrowser::applyDocumentStyle()'s setHtml(m_lastHtml)) will re-trigger
         * highlighting anyway, in which case an extra explicit pass here would be pure waste.
         * SyntaxBucket::Text is intentionally left with an EMPTY QTextCharFormat -- Stage 1's
         * SyntaxTheme has no bucket for it by design, so "no format" (falls back to whatever
         * colour the surrounding document/CSS already gives plain text) is the correct behaviour,
         * not a lookup miss to warn about.
         */
        void refreshColors();

    protected:

        void highlightBlock(const QString& text) override;

    private:

        void init();

        std::array<QTextCharFormat,6> m_formats; // indexed by static_cast<size_t>(SyntaxBucket)

        // Memoises the language resolved for the LAST block's tag, since adjacent lines of the
        // same fence resolve to the same tag over and over during a full rehighlight() pass --
        // avoids a SyntaxLanguageRegistry::find() QString-normalisation call per line.
        QString m_lastTag;
        const SyntaxLanguage* m_lastLanguage=nullptr;

        // Reused across highlightBlock() calls so the tokenizer's out-vector allocates its
        // backing storage once per highlighter, not once per line (tokenizeSyntaxLine()'s own
        // doc comment).
        std::vector<SyntaxSpan> m_spans;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SYNTAX_HIGHLIGHTER_HPP
