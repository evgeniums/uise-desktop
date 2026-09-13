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
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Scheme for a character mention's anchor href -- "whitem-mention:<uid>", with the
 *  character's title as the anchor's display text (task-message-formatting-plan.md, Stage 6).
 *
 * Deliberately NOT in MarkdownRenderOptions::allowedLinkSchemes' default list: a renderer that
 * accepted this scheme unconditionally would emit a clickable in-app link for any message text
 * that merely contains one, whether the host understands mentions or not. A viewer opts in per
 * instance -- see AbstractChatMessageText::setMentionsEnabled(), which supplies a caller-owned
 * copy of the allowlist with this scheme appended.
 *
 * An inline function rather than a QString constant, so nothing depends on cross-TU static
 * initialization order.
 */
inline QString mentionUrlScheme()
{
    return QStringLiteral("whitem-mention");
}

/**
 * @brief Build a mention anchor's href from a character uid.
 *
 * @param uid Must contain no raw SPACE: measured, a space inside an href comes back EMPTY through
 *  QTextDocument::toMarkdown()/setMarkdown() -- the same "a space truncates an href unless
 *  angle-bracketed" property AbstractHyperlinkDialog's own URL validator already guards against
 *  for ordinary URLs. Every other uid shape tested (alnum, '/'-separated, ':'-separated, dashes,
 *  underscores, parentheses) round-trips byte-identically.
 */
inline QString mentionHref(const QString& uid)
{
    return mentionUrlScheme()+QLatin1Char(':')+uid;
}

//! Whether `href` is a mention anchor's href. Prefix test on the scheme plus its colon -- the
//! same shape as the `a[href^="whitem-mention:"]` CSS rule
//! ChatMessageTextBrowser::applyDocumentStyle() emits, so the editor and the viewer can never
//! disagree about what counts as a mention.
inline bool isMentionHref(const QString& href)
{
    return href.startsWith(mentionUrlScheme()+QLatin1Char(':'));
}

/**
 * @brief Scheme for an inline emoji image's src -- "whitem-emoji:<reaction id>", where the
 *  reaction id is ChatReactionId::make(iconId,packUri), the SAME identifier a chat message
 *  reaction uses (see chatreaction.hpp).
 *
 * Reusing that identifier verbatim is the point: a reaction and an inline emoji naming the same
 * graphic are provably the same thing, and ReactionIconPacks::instance().iconInfo() resolves
 * either one with no separate lookup path.
 *
 * Deliberately NOT in MarkdownRenderOptions::allowedLinkSchemes' default list, and it must never
 * be added there -- for a STRONGER reason than mentionUrlScheme() above. There is no anchor form
 * of an emoji at all, so the only thing allowlisting it could do is let an emoji whose icon is
 * NOT locally available degrade, through writeImage()'s own scheme-allowed branch, into a
 * clickable <a href="whitem-emoji:..."> -- a dead in-app link. The correct degradation is the alt
 * text (the emoji character itself), which is what the default already produces.
 */
inline QString emojiUrlScheme()
{
    return QStringLiteral("whitem-emoji");
}

/**
 * @brief Build an inline emoji image's src from a reaction id.
 *
 * The payload is PERCENT-ENCODED, which is load-bearing rather than cosmetic: a pack URI is
 * free to be a full URL (see AbstractReactionIconPack::uri()), and two independent parsers see
 * this string raw -- Qt's markdown writer copies it verbatim into "![alt](src)", where a space
 * or a ')' would corrupt the markdown outright, and markdownToHtml()'s own isSchemeAllowed()
 * parses it with QUrl(src,QUrl::StrictMode). Encoding makes the payload one opaque token that
 * survives both.
 *
 * For the DEFAULT pack (empty uri, so ChatReactionId::make() returns the bare icon id) encoding
 * is a no-op, so the common case stays readable: "whitem-emoji:thumbsup".
 */
inline QString emojiSrc(const QString& reactionId)
{
    return emojiUrlScheme()+QLatin1Char(':')
           +QString::fromLatin1(QUrl::toPercentEncoding(reactionId));
}

//! Whether `src` is an inline emoji image's src. Prefix test on the scheme plus its colon, the
//! same shape as isMentionHref() above.
inline bool isEmojiSrc(const QString& src)
{
    return src.startsWith(emojiUrlScheme()+QLatin1Char(':'));
}

//! Inverse of emojiSrc(). Returns an empty string when `src` is not an emoji src at all, which
//! is also what an emoji src with an empty payload yields -- both are "nothing to resolve".
inline QString emojiReactionId(const QString& src)
{
    if (!isEmojiSrc(src))
    {
        return QString{};
    }
    return QUrl::fromPercentEncoding(
        src.mid(emojiUrlScheme().size()+1).toLatin1()
    );
}

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
     * never reach the emitted HTML in the first place. Stage 6 (mentions) adds mentionUrlScheme()
     * to a caller-supplied copy of this list -- see AbstractChatMessageText::setMentionsEnabled(),
     * the only place that does so; the DEFAULT list below is never touched.
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
     * has, entirely outside this repo's scope. A plain "@username" mention (task-message-
     * formatting-plan.md, Stage 6's insertMentionText() form) is the identical shape: it needs a
     * character-directory lookup this generic renderer has no business doing, to turn it into a
     * `whitem-mention:` anchor the way an explicit `[Title](whitem-mention:uid)` anchor already
     * renders as one. When set, called once per plain (non-anchor, non-code) text run with that
     * run's RAW (unescaped) text; must return an already-escaped HTML fragment (its own text
     * re-escaped, with any additional anchors it wants to add) -- returning an empty string
     * leaves the run's own default escaping in place. Null by default, meaning this renderer
     * relies solely on Qt's own GitHub-dialect autolink detection.
     *
     * TRUST BOUNDARY: the returned fragment is inserted VERBATIM, bypassing every check this
     * renderer otherwise applies to itself -- it is not re-escaped, and any `<a href>` inside it
     * is never run through isSchemeAllowed()/allowedLinkSchemes above. This is the one place a
     * host can put arbitrary HTML into a rendered bubble, so the callback is entirely responsible
     * for escaping its own output and for only ever emitting hrefs it trusts (e.g. a scheme it
     * knows to be safe, built from data it resolved itself) -- never rendering untrusted
     * user-supplied text back out unescaped.
     *
     * NOTE, when emojiEnabled is set: a plain run containing emoji characters is split around
     * them, and this hook is then called once per non-emoji SEGMENT rather than once for the run
     * as a whole ("see @bob <emoji> now" invokes it twice). Composing this way is deliberate --
     * running the hook first would have it rewriting inside the `<a href>` attributes of its own
     * output, and running it second would hand it HTML where its contract promises raw text. An
     * emoji code point is never part of an "@username" or a bare domain, so a segment boundary is
     * always a token boundary and no match this hook could have made is lost.
     */
    std::function<QString(const QString&)> extraLinkify;

    /**
     * @brief Whether emoji are recognized at all.
     *
     * With this on, two things become an `<img>`: a markdown image whose src is an emojiSrc()
     * AND whose icon is registered locally, and a literal emoji CHARACTER in a plain text run
     * that the default pack has a graphic for. Everything else about the rendering is unchanged.
     *
     * Default FALSE, the same opt-in shape (and for the same reason) as
     * AbstractChatMessageText::setMentionsEnabled(): a renderer that emitted `<img>`
     * unconditionally would change what every existing caller's bubbles look like.
     *
     * This is the ONLY switch that can make markdownToHtml() emit an `<img>` at all, so with it
     * off the function's "`<img>` is never one of them" sanitization contract holds verbatim.
     * With it on, the only `<img>` ever emitted is one this function BUILDS ITSELF out of a
     * reaction id it has already resolved against ReactionIconPacks -- a src is never copied
     * through from the source document, so no attacker-supplied URL can reach the output, and
     * every other image still degrades to escaped alt text exactly as before.
     */
    bool emojiEnabled=false;

    /**
     * @brief Pixel size of an INLINE emoji image, so it mirrors the viewer's own text.
     *
     * This renderer has neither a widget nor a font, so it cannot derive this -- a caller
     * computes it from the target's QFontMetrics (see ChatMessageText::loadText()). 0 emits no
     * width/height at all and lets Qt fall back to the pixmap's own size.
     */
    int emojiInlineSize=0;

    /**
     * @brief A message that is NOTHING BUT emoji, at most this many of them, renders as one row
     *  of large images (emojiOnlySize) instead of inline-sized ones. 0 disables the case.
     *
     * Both forms count and may be mixed: literal emoji characters and emoji markdown images.
     * Every one of them must resolve locally, or the message falls back to ordinary inline
     * rendering -- a half-large, half-inline row would look like a fault.
     */
    int emojiOnlyMaxCount=3;

    //! Pixel size used by the emojiOnlyMaxCount case above.
    int emojiOnlySize=64;
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

/**
 * @brief Rewrite the single newlines that a CHAT message means as visible line breaks into
 *  U+2028 LINE SEPARATOR, leaving every newline that is markdown SYNTAX alone.
 *
 * This is the rule markdownToHtml() already applies internally, exposed because the message
 * EDITOR needs the identical decision when it imports markdown back into a WYSIWYG document, and
 * the two must not drift.
 *
 * Why the editor needs it: `QTextDocument::setMarkdown()` follows CommonMark, where a single
 * newline inside a paragraph is a SPACE. Measured, "aa\nbb" comes back as one block reading
 * "aa bb" -- so a line break the user typed, exported correctly as one newline, was silently
 * eaten the moment the document was re-imported (switching to Markdown mode and back, or loading
 * a message for editing). Feeding the source through this first turns that newline into a
 * character `setMarkdown()` keeps verbatim, and the break survives: same source comes back as one
 * block reading "aa<LS>bb", which re-exports byte-identically.
 *
 * Newlines that carry syntax are untouched: blank lines, fenced regions, table rows, list items,
 * headings and every other block opener -- see the implementation's own notes for the full set
 * and for what merging them away used to break.
 *
 * @param markdown Source text, GitHub-flavoured markdown.
 * @return The same source with in-paragraph newlines replaced by U+2028.
 */
UISE_DESKTOP_EXPORT QString markdownWithChatLineBreaks(const QString& markdown);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MARKDOWN_RENDERER_HPP
