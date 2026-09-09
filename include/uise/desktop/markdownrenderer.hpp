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

/** @file uise/desktop/markdownrenderer.hpp
*
*  Declares markdownToHtml() and markdownToPlainText().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MARKDOWN_RENDERER_HPP
#define UISE_DESKTOP_MARKDOWN_RENDERER_HPP

#include <functional>

#include <QString>
#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Options controlling markdownToHtml().
 *
 * A plain, cheaply-copyable value type -- no pimpl, modelled after ReplyPreviewData. Every field
 * has a default that matches the chat message viewer's own needs, so most callers pass none of
 * this at all (see ChatMessageText::loadText()'s Markdown branch).
 */
struct UISE_DESKTOP_EXPORT MarkdownRenderOptions
{
    /**
     * @brief URL schemes an anchor (explicit `[title](url)`, a bare autolink, or a markdown
     *  image degraded to a link -- see markdownToHtml()'s own doc comment) may carry.
     *
     * Anything else degrades to escaped plain text instead of a link. markdownToHtml() never
     * opens a link itself (ChatMessageTextBrowser::setOpenLinks(false) already keeps that
     * decision with the host, via linkActivated()), but a host handler may still hand the URL
     * straight to QDesktopServices::openUrl() or similar, so a "javascript:"/"file:" href must
     * never reach the emitted HTML in the first place. Stage 6 (mentions) adds
     * "whitem-mention" to a caller-supplied copy of this list.
     */
    QStringList allowedLinkSchemes{
        QStringLiteral("http"),
        QStringLiteral("https"),
        QStringLiteral("mailto"),
        QStringLiteral("ftp"),
        QStringLiteral("ftps"),
        QStringLiteral("tel")
    };

    /**
     * @brief Whether a single newline in the source is treated as a hard line break.
     *
     * True by default to match the chat convention (and today's plain `white-space:pre-wrap`
     * rendering) rather than CommonMark's own rule of joining single-newline-separated lines
     * into one paragraph -- see task-message-formatting-plan.md, Stage 2, "Soft newlines".
     */
    bool hardLineBreaks=true;

    //! Source is truncated to this many characters before parsing -- a guard against a
    //! pathological paste blowing up QTextDocument::setMarkdown()'s own parse cost.
    int maxSourceChars=65536;

    //! Hard cap on the number of anchors emitted, mirroring chattextrender.cpp's
    //! MaxRenderedAnchors -- a message that is nothing but pasted links should not be allowed to
    //! blow up QTextDocument layout cost on the RENDERING side either. Text past the cap is
    //! still emitted, just without the enclosing `<a>`.
    int maxAnchors=128;

    /**
     * @brief Optional host hook for link detection this renderer cannot do on its own.
     *
     * Bare-domain detection (e.g. "example.com" with no scheme and no "www.") needs a
     * maintained TLD table -- exactly what whitemclient::chat::extractTextEntities() already
     * has, entirely outside this repo's scope. When set, called once per plain (non-anchor,
     * non-code) text run with that run's RAW (unescaped) text; must return an already-escaped
     * HTML fragment (its own text re-escaped, with any additional anchors it wants to add) --
     * returning an empty string leaves the run's own default escaping in place. Null by default,
     * meaning this renderer relies solely on Qt's own GitHub-dialect autolink detection.
     */
    std::function<QString(const QString&)> extraLinkify;
};

/**
 * @brief Render markdown source to sanitized HTML suitable for
 *  ChatMessageTextBrowser::setHtmlContent().
 *
 * Parses with Qt's own importer (QTextDocument::setMarkdown(), MarkdownDialectGitHub) and then
 * walks the resulting QTextDocument, emitting a small allowlisted HTML vocabulary of our own --
 * see markdownrenderer.cpp's own top-of-file comment for the full construct table. The walk IS
 * the sanitizer: every text run is escaped, only tags this function chooses are ever emitted,
 * `<img>` is never one of them, and every anchor's scheme is checked against
 * MarkdownRenderOptions::allowedLinkSchemes -- raw inline HTML the GitHub dialect admits in the
 * source has therefore already been reduced to ordinary formatting (or, for something Qt's
 * importer does not recognise as a tag at all, escaped plain text) by the time this function
 * ever sees it. Deliberately does NOT call QTextDocument::toHtml() -- that emits inline
 * `-qt-*`/font-family/font-size styles baked in from the widget's current font, which would
 * silently defeat resources/style/messagetext.css (task-message-formatting-plan.md, Stage 1).
 *
 * A code block's language (if any) is threaded through as `<pre class="language-xxx">` for
 * Stage 3's syntax highlighter -- deliberately on `<pre>`, not `<code>`: Qt's own HTML parser
 * reads a `class="language-x"` attribute specifically on `<pre>` and maps it straight back onto
 * `QTextFormat::BlockCodeLanguage`, so (unlike most attributes) it DOES survive a later
 * `QTextBrowser::setHtml()` round-trip -- Stage 3 can read a code block's language straight off
 * the live rendered document rather than needing the original markdown source.
 *
 * @param markdown Source text, GitHub-flavoured markdown.
 * @param options See MarkdownRenderOptions.
 * @return Sanitized HTML. Never throws; a source Qt's importer cannot parse at all renders as
 *  whatever plain-paragraph fallback QTextDocument::setMarkdown() itself falls back to.
 */
UISE_DESKTOP_EXPORT QString markdownToHtml(const QString& markdown,
                                            const MarkdownRenderOptions& options={});

/**
 * @brief Flatten markdown source to a single line of plain text, for a reply preview.
 *
 * Used by ReplyPreview when ReplyPreviewData::format() is TextFormat::Markdown -- see
 * replypreview.cpp. Deliberately lighter than markdownToHtml(): no hard-line-break
 * preprocessing (ReplyPreview::refresh() collapses whitespace with QString::simplified() right
 * afterwards via trimReplyText(), so preserving line breaks here would be wasted work), and a
 * much smaller default source cap since ReplyPreview::refresh() runs on every flyweight row
 * recycle and only ever needs a short preview's worth of text.
 *
 * @param markdown Source text, GitHub-flavoured markdown.
 * @param maxSourceChars Source is truncated to this many characters before parsing. Truncating
 *  markdown mid-construct (e.g. inside a fence) is harmless here -- the result is only ever
 *  fed through trimReplyText()'s own further truncation and single-line collapse.
 * @return Plain text with all markdown syntax removed.
 */
UISE_DESKTOP_EXPORT QString markdownToPlainText(const QString& markdown, int maxSourceChars=4096);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MARKDOWN_RENDERER_HPP
