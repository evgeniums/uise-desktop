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

/** @file demo/messageformatting/main.cpp
*
*  Demo of Stages 1-2 of task-message-formatting-plan.md: Stage 1's document-level CSS channel
*  (Style::css(), consumed by ChatMessageTextBrowser) and the syntax-colour theme JSON
*  (Style::syntaxColor()), both reloading live across Auto/Light/Dark; and Stage 2's
*  markdown-to-sanitized-HTML renderer (markdownToHtml()/markdownToPlainText()), exercised live
*  against an editable source pane, a generated-HTML inspection pane, a sanitization smoke test,
*  and a ReplyPreview showing the markdown-to-plain-text strip.
*
*/

/****************************************************************************/

#include <functional>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QDateTime>
#include <QTimer>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFormat>
#include <QStringList>
#include <QDebug>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/replypreview.hpp>
#include <uise/desktop/replypreviewdata.hpp>
#include <uise/desktop/syntaxtheme.hpp>
#include <uise/desktop/syntaxlanguage.hpp>
#include <uise/desktop/messageeditor.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>
#include <uise/desktop/icontextbutton.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// bubble content width negotiated in this demo -- see demo/chatmessagefiles/main.cpp's own
// DemoBubbleWidth for why a standalone demo has to pick a fixed value.
constexpr int DemoBubbleWidth=380;

// The five overridable syntax buckets (see syntaxtheme.hpp -- SyntaxBucket::Text has no JSON
// bucket at all, by design, so it is deliberately excluded from this swatch list). Pulled
// straight from the library's own syntaxBucketName() (task-message-formatting-plan.md, Stage 3)
// rather than hand-written, so this demo doubles as proof the names match
// resources/style/{light,dark}/syntax.json.
const QStringList SyntaxBuckets{
    syntaxBucketName(SyntaxBucket::Keyword),
    syntaxBucketName(SyntaxBucket::Type),
    syntaxBucketName(SyntaxBucket::Literal),
    syntaxBucketName(SyntaxBucket::Callable),
    syntaxBucketName(SyntaxBucket::Comment)
};

// Picks black or white text for legibility against an arbitrary swatch background -- some
// syntax-theme colours are deliberately bright (e.g. dark.json's literal #F1FA8C), so a
// hardcoded white label text would be unreadable on those.
QColor readableTextColor(const QColor& background)
{
    auto luma=0.299*background.redF()+0.587*background.greenF()+0.114*background.blueF();
    return luma>0.6 ? QColor(Qt::black) : QColor(Qt::white);
}

// Sample HTML exercising every selector resources/style/messagetext.css defines -- <code>, <pre>,
// <blockquote>, <table> -- loaded as TextFormat::Html (NOT Plain, see the plan's Design decision
// 9): only the Html branch of ChatMessageText::loadText() routes through setHtmlContent(), which
// is the only path that remembers the content for ChatMessageTextBrowser::applyDocumentStyle()
// to reapply on a theme switch.
QString sampleHtml()
{
    return QStringLiteral(
        "<div style=\"white-space:pre-wrap;line-height:125%;\">"
        "Inline <code>monospace code</code> and a block below:"
        "<pre>function greet(name) {\n    return &quot;Hello, &quot; + name;\n}</pre>"
        "<blockquote>A quoted line, indented and left-bordered by messagetext.css.</blockquote>"
        "<table>"
        "<tr><th>Bucket</th><th>Sample</th></tr>"
        "<tr><td>keyword</td><td>if / return</td></tr>"
        "<tr><td>literal</td><td>&quot;Hello&quot;</td></tr>"
        "</table>"
        "Also a plain <a href=\"https://example.com\">hyperlink</a>, styled by the existing "
        "linkColor/linkUnderline mechanism, layered on top of the CSS above."
        "</div>"
    );
}

// Stage 2 sample markdown, exercising every construct table.md-renderer's own comment lists:
// headings, emphasis, inline code, a fenced block with a language, an indented code block,
// nested/task lists, a two-level blockquote, a thematic break, an aligned table, an explicit
// link, a bare URL, a www. URL, an email, a backtick-escaped mention, and -- most importantly --
// a plain two-line paragraph exercising the chat line-break policy (task-message-formatting-
// plan.md, Stage 2, "Soft newlines"). Also -- Stage 3 -- a second fenced block in a DIFFERENT
// language (Python) plus an untagged fence and one in an unrecognised language, exercising
// SyntaxHighlighter's three cases side by side: a known language, the generic fallback (strings/
// numbers/comments only, no keywords), and no highlighting at all.
QString sampleMarkdown()
{
    return QStringLiteral(
        "# Message formatting demo\n"
        "\n"
        "A two-line paragraph, typed as one newline in the source:\n"
        "line one of the message\n"
        "line two, right below it\n"
        "\n"
        "**bold**, *italic*, ~~strikethrough~~, `inline code`, and a [link](https://example.com).\n"
        "\n"
        "```cpp\n"
        "int greet(const char* name)\n"
        "{\n"
        "    return printf(\"Hello, %s\\n\", name);\n"
        "}\n"
        "```\n"
        "\n"
        "    an indented code block\n"
        "    second line\n"
        "\n"
        "- bullet one\n"
        "  - nested bullet\n"
        "- bullet two\n"
        "\n"
        "1. first\n"
        "2. second\n"
        "\n"
        "- [ ] todo item\n"
        "- [x] done item\n"
        "\n"
        "> outer quote\n"
        "> > nested quote\n"
        "\n"
        "---\n"
        "\n"
        "| Left | Center | Right |\n"
        "|:---|:---:|---:|\n"
        "| a | b | c |\n"
        "| d | e | f |\n"
        "\n"
        "A table too wide for the bubble (Stage 4) -- pinned to its natural width and reachable by\n"
        "the horizontal scrollbar, with an expand button floated at its top-right:\n"
        "\n"
        "| Package | Version | Licence | Maintainer | Updated | Description |\n"
        "|---|---|---|---|---|---|\n"
        "| libexample-core | 1.24.7 | Apache-2.0 | infrastructure-team | 2026-08-14 | Shared runtime helpers |\n"
        "| libexample-net | 0.9.3 | MIT | networking-team | 2026-09-01 | Transport and retry policy |\n"
        "| libexample-ui | 3.11.0 | LGPL-3.0 | interface-team | 2026-07-22 | Widget toolkit bindings |\n"
        "\n"
        "Bare link: https://example.com, www.example.org, or contact me@example.com.\n"
        "\n"
        "A second fenced block, a different language (Stage 3):\n"
        "\n"
        "```python\n"
        "def greet(name):\n"
        "    # a comment\n"
        "    return f\"Hello, {name}\"\n"
        "```\n"
        "\n"
        "An untagged fence -- no syntax highlighting at all (decision 4):\n"
        "\n"
        "```\n"
        "plain fenced text, no colours\n"
        "```\n"
        "\n"
        "An unrecognised language tag -- the generic fallback (strings/numbers/comments only):\n"
        "\n"
        "```kotlin\n"
        "// a comment\n"
        "fun greet(name: String) = \"Hello, $name\"\n"
        "```\n"
        "\n"
        "A backtick-escaped mention prints literally: `@alice`.\n"
    );
}

// Deliberately hostile input -- every line is a sanitization case markdownToHtml() must defeat.
// See checkSanitization() below for the assertions run against its output.
QString hostileMarkdown()
{
    return QStringLiteral(
        "<script>alert('xss')</script>\n"
        "\n"
        "<img src=\"https://tracker.example/pixel.png\">\n"
        "\n"
        "![local file](file:///etc/passwd)\n"
        "\n"
        "[click me](javascript:alert(1))\n"
        "\n"
        "[data url](data:text/html,<h1>x</h1>)\n"
        "\n"
        "[mention](whitem-mention:12345)\n"
        "\n"
        "`<b>literal, not bold</b>` and `a & b < c`\n"
        "\n"
        "<a href=\"javascript:alert(2)\">raw html anchor</a>\n"
    );
}

// Asserts the invariants markdownToHtml() must hold against hostileMarkdown()'s output, logging
// PASS/FAIL for each -- doubles as a running smoke test in all three theme modes, since nothing
// about sanitization is otherwise visible in the rendered bubble (that is the whole point).
void checkSanitization(const QString& html, const std::function<void(const QString&)>& logMsg)
{
    struct Check
    {
        QString description;
        bool passed;
    };
    std::vector<Check> checks{
        {QStringLiteral("no <script> tag"),!html.contains(QStringLiteral("<script"))},
        {QStringLiteral("no <img> tag"),!html.contains(QStringLiteral("<img"))},
        {QStringLiteral("no javascript: scheme"),!html.contains(QStringLiteral("javascript:"))},
        {QStringLiteral("no data: scheme"),!html.contains(QStringLiteral("data:"))},
        {QStringLiteral("no file: scheme"),!html.contains(QStringLiteral("file:"))},
        {QStringLiteral("no whitem-mention: (not in default allowlist)"),!html.contains(QStringLiteral("whitem-mention:"))},
        {QStringLiteral("literal <b> inside code span stays escaped"),html.contains(QStringLiteral("&lt;b&gt;"))},
    };
    for (const auto& c : checks)
    {
        logMsg((c.passed ? QStringLiteral("  PASS: ") : QStringLiteral("  FAIL: "))+c.description);
    }
}

// Logs every distinct code-block language tag recoverable from ALREADY-RENDERED html, with no
// access to the original markdown source at all -- the running proof that Stage 3's syntax
// highlighter can read QTextFormat::BlockCodeLanguage straight off a live QTextDocument (the
// <pre class="language-x"> markdownToHtml() emits round-trips through setHtml(), see
// markdownrenderer.hpp's own doc comment). Re-parses independently rather than reaching into
// ChatMessageText's own (unexposed) internal document -- exactly what SyntaxHighlighter, attached
// to the bubble's real document via ChatMessageTextBrowser::ensureSyntaxHighlighter(), also does.
// Alongside each raw tag, logs what SyntaxLanguageRegistry::find() actually resolves it to --
// "cpp" and "python" resolve to themselves, "kotlin" resolves to the generic fallback (not a
// nullptr, not itself), and an untagged fence contributes no tag at all (decision 4: no
// highlighting), all visible side by side here.
void logCodeLanguages(const QString& html, const std::function<void(const QString&)>& logMsg)
{
    QTextDocument doc;
    doc.setHtml(html);
    QStringList languages;
    for (auto block=doc.begin(); block!=doc.end(); block=block.next())
    {
        auto fmt=block.blockFormat();
        if (fmt.hasProperty(QTextFormat::BlockCodeLanguage))
        {
            auto lang=fmt.stringProperty(QTextFormat::BlockCodeLanguage);
            if (!lang.isEmpty() && !languages.contains(lang))
            {
                languages<<lang;
            }
        }
    }
    if (languages.isEmpty())
    {
        logMsg(QStringLiteral("  Code languages recovered from rendered document: (none)"));
        return;
    }
    QStringList resolved;
    for (const auto& tag : languages)
    {
        auto* language=SyntaxLanguageRegistry::instance().find(tag);
        resolved<<QStringLiteral("%1->%2").arg(tag,language ? language->name() : QStringLiteral("(unresolved)"));
    }
    logMsg(QStringLiteral("  Code languages recovered from rendered document: ")+resolved.join(QStringLiteral(", ")));
}

// Builds a real ChatMessage/ChatMessageContent bubble around `body`, so bubble-width negotiation
// is genuinely exercised -- see demo/chatmessagefiles/main.cpp's own makeMessage(), this demo's
// template for that part.
AbstractChatMessage* makeMessage(QWidget* parent, AbstractChatMessage::Direction direction, AbstractChatMessageBody* body)
{
    AbstractChatMessage* msg=new ChatMessage(parent);
    msg->construct();
    msg->setDirection(direction);
    msg->setDateTime(QDateTime::currentDateTime());

    auto content=new ChatMessageContent(msg);
    content->setChatMessage(msg);

    auto bottom=new ChatMessageBottom(content);
    bottom->setTimeString(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));

    content->setWidgets(body,nullptr,bottom);
    msg->setContent(content);

    content->updateBubbleWidth(DemoBubbleWidth);

    return msg;
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();
    Style::instance().setStyleSheetMode(
        Style::instance().isDarkTheme() ? Style::StyleSheetMode::Dark : Style::StyleSheetMode::Light
    );

    QMainWindow w;

    auto* mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);
    auto* central=new QFrame(mainFrame);
    auto* rootLayout=Layout::vertical(central,false);
    mainFrame->setWidget(central);

    auto* log=new QPlainTextEdit();
    log->setReadOnly(true);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    // --- syntax theme swatches, built ahead of the theme combo since the combo's handler
    // refreshes them --- one QLabel per bucket, background colour pulled from
    // Style::instance().syntaxColor(). A bucket the active theme doesn't define renders as a
    // neutral grey "(unset)" tile instead of vanishing silently. ---

    auto* swatchFrame=new QFrame(central);
    auto* swatchLayout=Layout::horizontal(swatchFrame);
    std::vector<QLabel*> swatchLabels;
    for (const auto& bucket : SyntaxBuckets)
    {
        auto* label=new QLabel(bucket);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(90,28);
        label->setAutoFillBackground(true);
        swatchLayout->addWidget(label);
        swatchLabels.push_back(label);
    }

    auto refreshSwatches=[swatchLabels,logMsg]()
    {
        int unsetCount=0;
        for (size_t i=0;i<SyntaxBuckets.size();++i)
        {
            auto color=Style::instance().syntaxColor(SyntaxBuckets[static_cast<int>(i)]);
            auto* label=swatchLabels[i];
            if (color)
            {
                label->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                          .arg(color->name(),readableTextColor(*color).name()));
                label->setText(SyntaxBuckets[static_cast<int>(i)]);
            }
            else
            {
                ++unsetCount;
                QColor unsetBg(QStringLiteral("#888888"));
                label->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                          .arg(unsetBg.name(),readableTextColor(unsetBg).name()));
                label->setText(SyntaxBuckets[static_cast<int>(i)]+QStringLiteral(" (unset)"));
            }
        }
        logMsg(QStringLiteral("Reloaded: %1/%2 syntax buckets set, css=%3 chars")
                   .arg(SyntaxBuckets.size()-unsetCount)
                   .arg(SyntaxBuckets.size())
                   .arg(Style::instance().css().size()));
    };

    // --- colour theme selector -- same recipe as demo/replypreview/main.cpp ---

    auto* themeFrame=new QFrame(central);
    auto* themeLayout=Layout::horizontal(themeFrame);
    rootLayout->addWidget(themeFrame);

    themeLayout->addWidget(new QLabel(QStringLiteral("Colour theme:")));

    auto* themeCombo=new QComboBox();
    themeCombo->addItem(QStringLiteral("Auto"),static_cast<int>(Style::StyleSheetMode::Auto));
    themeCombo->addItem(QStringLiteral("Light"),static_cast<int>(Style::StyleSheetMode::Light));
    themeCombo->addItem(QStringLiteral("Dark"),static_cast<int>(Style::StyleSheetMode::Dark));
    themeCombo->setCurrentIndex(themeCombo->findData(static_cast<int>(Style::instance().styleSheetMode())));
    themeLayout->addWidget(themeCombo,1);

    QObject::connect(
        themeCombo,
        &QComboBox::currentIndexChanged,
        central,
        [themeCombo,refreshSwatches](int index)
        {
            auto mode=static_cast<Style::StyleSheetMode>(themeCombo->itemData(index).toInt());
            Style::instance().setStyleSheetMode(mode);
            Style::instance().applyStyleSheet(true);
            // The syntax swatches are pulled, not pushed -- reloadStyleSheet() (inside
            // applyStyleSheet()) already rebuilt m_syntaxThemes by the time this runs, but
            // nothing re-queries Style::syntaxColor() on its own, unlike the bubble below whose
            // ChatMessageTextBrowser::changeEvent(QEvent::StyleChange) reapplies itself.
            refreshSwatches();
        }
    );

    refreshSwatches();

    rootLayout->addWidget(new QLabel(QStringLiteral("Syntax theme swatches (keyword / type / literal / callable / comment):")));
    rootLayout->addWidget(swatchFrame);

    // --- the message bubble exercising messagetext.css, loaded as Html so it is reachable by
    // setDefaultStyleSheet() / ChatMessageTextBrowser::applyDocumentStyle() at all. Switch the
    // theme combo above and watch this bubble restyle immediately -- that immediacy is the
    // actual proof the changeEvent(QEvent::StyleChange) fix works, not just that the CSS loads
    // once at startup. ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Message text styled from messagetext.css (switch theme above to see it restyle live):")));

    auto* htmlBody=new ChatMessageText();
    htmlBody->loadText(sampleHtml(),TextFormat::Html);
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Received,htmlBody));

    // --- Stage 2: markdown source -> live bubble + generated-HTML inspection pane. The bubble
    // is the only thing a real chat message would show; the HTML pane exists purely so
    // sanitization (invisible in the rendered bubble by design) can actually be inspected. ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Markdown source (edit freely -- re-renders after a short pause):")));

    auto* mdSource=new QPlainTextEdit();
    mdSource->setPlainText(sampleMarkdown());
    QFont monoFont(QStringLiteral("Menlo"));
    monoFont.setStyleHint(QFont::Monospace);
    mdSource->setFont(monoFont);
    mdSource->setMinimumHeight(220);
    rootLayout->addWidget(mdSource);

    auto* softBreakCheck=new QCheckBox(QStringLiteral("Single newline = visible line break (chat convention)"));
    softBreakCheck->setChecked(true);
    rootLayout->addWidget(softBreakCheck);

    auto* hostileButton=new QPushButton(QStringLiteral("Load hostile sample (sanitization smoke test)"));
    rootLayout->addWidget(hostileButton);

    // --- Stage 3: language picker -- insert a fenced sample for ANY registered language (not
    // just the two baked into sampleMarkdown()) so every one of the 21+ seeded languages, plus
    // the generic fallback, is reachable and inspectable at demo runtime, not just the two hand-
    // picked ones in the static sample. ---

    auto* langFrame=new QFrame(central);
    auto* langLayout=Layout::horizontal(langFrame);
    rootLayout->addWidget(langFrame);

    langLayout->addWidget(new QLabel(QStringLiteral("Insert a fenced sample for:")));

    auto* langCombo=new QComboBox();
    for (const auto& name : SyntaxLanguageRegistry::instance().names())
    {
        langCombo->addItem(name);
    }
    langLayout->addWidget(langCombo,1);

    auto* insertLangButton=new QPushButton(QStringLiteral("Insert"));
    langLayout->addWidget(insertLangButton);

    rootLayout->addWidget(new QLabel(QStringLiteral("Live-rendered bubble:")));
    auto* mdBody=new ChatMessageText();
    auto* mdMessage=makeMessage(central,AbstractChatMessage::Direction::Sent,mdBody);
    rootLayout->addWidget(mdMessage);

    rootLayout->addWidget(new QLabel(QStringLiteral("Generated HTML (what actually reaches setHtmlContent()):")));
    auto* htmlOutput=new QPlainTextEdit();
    htmlOutput->setReadOnly(true);
    htmlOutput->setFont(monoFont);
    htmlOutput->setMinimumHeight(120);
    rootLayout->addWidget(htmlOutput);

    rootLayout->addWidget(new QLabel(QStringLiteral("Reply preview of the same source (markdownToPlainText() strip):")));
    auto* mdReply=new ReplyPreview();
    // Capped because ReplyPreview cannot currently shrink below its own text: its ElidedLabels
    // leave m_label at QSizePolicy::MinimumExpanding, which has no ShrinkFlag, so qSmartMinSize()
    // takes qMax(sizeHint,minimumSizeHint) -- the FULL un-elided width. Fed this demo's whole
    // sample markdown (trimmed to trimReplyText()'s 200 chars) that measured 1083px, second only
    // to the caption above in forcing the window wide. A maximum width bounds it, since
    // qSmartMinSize() applies boundedTo(maxSize) before the explicit minimum. In a real host the
    // containing bar bounds it the same way; ElidedLabel::setIgnoreSizeHint(true) is the proper
    // library-side cure, but ReplyPreview is shared with the app's reply/forward/edit bars, so
    // that belongs in its own change rather than as a side effect of a demo layout fix.
    mdReply->setMaximumWidth(560);
    rootLayout->addWidget(mdReply);

    auto renderMarkdown=[mdSource,softBreakCheck,mdBody,mdMessage,htmlOutput,mdReply,logMsg]()
    {
        auto src=mdSource->toPlainText();

        MarkdownRenderOptions options;
        options.hardLineBreaks=softBreakCheck->isChecked();
        auto html=markdownToHtml(src,options);

        if (options.hardLineBreaks)
        {
            // Exercises the real production call path: ChatMessageText::loadText(...,
            // TextFormat::Markdown) internally calls markdownToHtml() with
            // MarkdownRenderOptions{} defaults, which already has hardLineBreaks=true.
            mdBody->loadText(src,TextFormat::Markdown);
        }
        else
        {
            // loadText()'s Markdown branch always uses default options -- to demo the checkbox
            // OFF state (pure CommonMark reflow, no chat-style line-break preservation) the
            // custom-options render computed above is fed in as already-rendered Html instead.
            mdBody->loadText(html,TextFormat::Html);
        }
        mdMessage->content()->updateBubbleWidth(DemoBubbleWidth);

        htmlOutput->setPlainText(html);

        ReplyPreviewData replyData;
        replyData.setSenderTitle(QStringLiteral("Demo Sender"));
        replyData.setDateTime(QDateTime::currentDateTime());
        replyData.setKind(ReplyMessageKind::Text);
        replyData.setFormat(TextFormat::Markdown);
        replyData.setText(src);
        mdReply->setData(replyData);

        logCodeLanguages(html,logMsg);
    };

    auto* debounce=new QTimer(central);
    debounce->setSingleShot(true);
    debounce->setInterval(250);
    QObject::connect(debounce,&QTimer::timeout,central,renderMarkdown);

    QObject::connect(mdSource,&QPlainTextEdit::textChanged,central,
                      [debounce]()
                      {
                          debounce->start();
                      });
    QObject::connect(softBreakCheck,&QCheckBox::toggled,central,renderMarkdown);

    QObject::connect(hostileButton,&QPushButton::clicked,central,
                      [mdSource,htmlOutput,renderMarkdown,logMsg]()
                      {
                          // setPlainText() queues textChanged -> the debounce timer, but the
                          // sanitization check wants an answer immediately rather than 250ms
                          // later -- render synchronously here instead of waiting for it.
                          mdSource->setPlainText(hostileMarkdown());
                          renderMarkdown();
                          logMsg(QStringLiteral("Sanitization checks against the hostile sample:"));
                          checkSanitization(htmlOutput->toPlainText(),logMsg);
                      });

    QObject::connect(insertLangButton,&QPushButton::clicked,central,
                      [mdSource,langCombo]()
                      {
                          // langCombo's population order (SyntaxLanguageRegistry::names()) is the
                          // registry's own registration order, which IS index() order (append-
                          // only) -- so currentIndex() directly names the selected language.
                          auto* language=SyntaxLanguageRegistry::instance().byIndex(langCombo->currentIndex());
                          if (language==nullptr)
                          {
                              return;
                          }
                          // Deliberately no comment token in the sample -- comment syntax differs
                          // per language ("//" vs "#" vs "--") and getting it wrong would look
                          // like a highlighter bug rather than a demo shortcut; the bare number
                          // literal alone is still enough to visibly prove a real language (not
                          // just plain text) got attached.
                          auto sample=QStringLiteral("\n```%1\nvalue = 42\n```\n").arg(language->id());
                          mdSource->insertPlainText(sample);
                      });

    renderMarkdown();

    // --- Stage 5a: message editor -- modes, formatting toolbar, expand toggle. Opted in
    // explicitly (setExpandButtonVisible(true)), since both default to off/hidden so an existing
    // host's composer stays pixel-identical until it opts in. ---

    rootLayout->addSpacing(8);
    // Word-wrapped: a QLabel does not wrap by default, so a long one-line caption puts its ENTIRE
    // sentence width under the scroll area's content as a hard minimum (measured: 1087px for the
    // readout caption below, against a 752px viewport -- that alone forced a horizontal
    // scrollbar). Wrapping drops that floor to a single word.
    auto* editorSectionLabel=new QLabel(QStringLiteral(
        "Message editor (click the expand toggle in the bottom-left corner to reveal the toolbar). "
        "Tab/Shift+Tab indent: inside a list they change its level, with a selection they quote "
        "the selected paragraphs, and on a single line they indent it -- send it to the bubble "
        "above to see each one survive the markdown round trip:"
    ));
    editorSectionLabel->setWordWrap(true);
    rootLayout->addWidget(editorSectionLabel);

    auto* msgEditor=new MessageEditor(central);
    msgEditor->setExpandButtonVisible(true);
    // Ceiling is 40% of the window, never below 180px -- resize the demo window and the editor's
    // growth limit follows it. Both the auto-resize growth and the expanded height stop here.
    msgEditor->setMaxHeight(180);
    msgEditor->setMaxHeightPercent(40);
    msgEditor->setFinishOnEnter(false);
    msgEditor->setPlaceHolderText(QStringLiteral("Type a message..."));
    msgEditor->loadText(QStringLiteral("Try **bold**, *italic*, a heading, or a fenced code block."),TextFormat::Markdown);
    rootLayout->addWidget(msgEditor);

    // Leading/trailing widgets INSIDE the editor, mirroring the attach/send pair a real composer
    // puts either side of the text (whitemdesktop's ChatPageBottom builds exactly this shape by
    // hand today). They stay either side of the text area always; type a second line to watch
    // each side group turn from a row of buttons into a COLUMN of buttons, and clear the editor
    // to watch them lie back down.
    // Built exactly like MessageEditor's own expand button -- an icon-only IconTextButton with
    // no focus policy -- and given NO explicit size: messageeditor.qss sizes every
    // uise--IconTextButton inside #leadingWidgets/#trailingWidgets the same way it sizes the
    // expand button, so a host icon button matches its neighbour without redeclaring anything.
    // Borrows FileUpload's own "add" alias, which already resolves to the paperclip -- the same
    // glyph the app's "attach" alias uses, and semantically the attach-a-file icon. Demos reuse
    // another component's alias rather than inventing one (see demo/icontextbutton's
    // "ImageEditor::brush", demo/elidedcontainer's "EditableLabel::edit"): attaching is a host
    // concern, so there is no MessageEditor:: alias for it, and a bare "paperclip" would NOT
    // resolve -- lookup searches <iconDir>/<name>, i.e. ":/icons/paperclip.svg", while the file
    // lives at ":/icons/tabler-icons/outline/paperclip.svg". The "${uise-svg-icons-1}" prefix
    // that bridges the two is substituted when the style JSON is parsed, not at lookup time, so
    // only a name that has been through an alias reaches the right directory.
    auto* attachButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("FileUpload::add"),msgEditor),
        msgEditor,
        IconTextButton::IconPosition::BeforeText
    );
    attachButton->setObjectName(QStringLiteral("attachButton"));
    attachButton->setText(QString());
    attachButton->setCursor(Qt::PointingHandCursor);
    attachButton->setFocusPolicy(Qt::NoFocus);
    attachButton->setToolTip(QStringLiteral("Leading widget (attach)"));
    msgEditor->addLeadingWidget(attachButton);

    auto* sendButton=new QPushButton(QStringLiteral("Send"));
    sendButton->setToolTip(QStringLiteral("Trailing widget (send)"));
    msgEditor->addTrailingWidget(sendButton);

    auto* editorStatusFrame=new QFrame(central);
    auto* editorStatusLayout=Layout::horizontal(editorStatusFrame);
    rootLayout->addWidget(editorStatusFrame);
    auto* editorModeLabel=new QLabel(QStringLiteral("mode: Formatted text"));
    editorStatusLayout->addWidget(editorModeLabel);
    auto* editorExpandedLabel=new QLabel(QStringLiteral("expanded: no"));
    editorStatusLayout->addWidget(editorExpandedLabel);
    auto* editorStackedLabel=new QLabel(QStringLiteral("side widgets: row"));
    editorStatusLayout->addWidget(editorStackedLabel);
    editorStatusLayout->addStretch(1);

    auto* editorReadoutLabel=new QLabel(QStringLiteral(
        "text(TextFormat::Markdown) -- live (the headline Stage 5a fix: switch to \"Markdown "
        "source\" mode and type markdown syntax -- it must come back verbatim, not "
        "backslash-escaped):"
    ));
    editorReadoutLabel->setWordWrap(true);
    rootLayout->addWidget(editorReadoutLabel);
    auto* editorMarkdownOutput=new QPlainTextEdit();
    editorMarkdownOutput->setReadOnly(true);
    editorMarkdownOutput->setFont(monoFont);
    editorMarkdownOutput->setMinimumHeight(100);
    rootLayout->addWidget(editorMarkdownOutput);

    auto* sendToBubbleButton=new QPushButton(QStringLiteral("Send this into the bubble above"));
    rootLayout->addWidget(sendToBubbleButton);

    auto modeName=[](MessageEditingMode mode)
    {
        switch (mode)
        {
            case (MessageEditingMode::Wysiwyg): return QStringLiteral("Formatted text");
            case (MessageEditingMode::Markdown): return QStringLiteral("Markdown source");
            case (MessageEditingMode::Plaintext): return QStringLiteral("Plain text");
        }
        return QString();
    };

    auto* editorDebounce=new QTimer(central);
    editorDebounce->setSingleShot(true);
    editorDebounce->setInterval(250);
    QObject::connect(editorDebounce,&QTimer::timeout,central,
        [msgEditor,editorMarkdownOutput]()
        {
            editorMarkdownOutput->setPlainText(msgEditor->text(TextFormat::Markdown));
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::textChanged,central,
        [editorDebounce]()
        {
            editorDebounce->start();
        }
    );
    editorDebounce->start(0);

    QObject::connect(msgEditor,&AbstractMessageEditor::messageEditingModeChanged,central,
        [editorModeLabel,modeName,logMsg](MessageEditingMode mode)
        {
            editorModeLabel->setText(QStringLiteral("mode: %1").arg(modeName(mode)));
            logMsg(QStringLiteral("Editor mode changed to: %1").arg(modeName(mode)));
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::expandedChanged,central,
        [editorExpandedLabel,logMsg](bool expanded)
        {
            editorExpandedLabel->setText(QStringLiteral("expanded: %1").arg(expanded ? "yes" : "no"));
            logMsg(QStringLiteral("Editor expanded changed to: %1").arg(expanded ? "true" : "false"));
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::stackedArrangementChanged,central,
        [editorStackedLabel,logMsg](bool stacked)
        {
            editorStackedLabel->setText(QStringLiteral("side widgets: %1").arg(stacked ? "column" : "row"));
            logMsg(QStringLiteral("Side widget groups became %1").arg(stacked ? "COLUMNS" : "ROWS"));
        }
    );

    // Logs every item MessageEditor's own context menu offers, incl. the Stage 5a Formatting
    // submenu -- makes the Wysiwyg-only mode gating (task-message-formatting-plan.md, decision
    // D3) observable: right-click while in Markdown/Plaintext mode and the submenu is absent.
    msgEditor->setContextMenuHandler(
        [logMsg](std::vector<MenuItem>& items)
        {
            QStringList ids;
            for (const auto& item : items)
            {
                ids << QString::number(item.id);
            }
            logMsg(QStringLiteral("Context menu items: %1").arg(ids.join(QStringLiteral(", "))));
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::contextMenuItemTriggered,central,
        [logMsg](int id)
        {
            logMsg(QStringLiteral("Context menu item triggered: %1").arg(id));
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::contextMenuItemToggled,central,
        [logMsg](int id, bool checked)
        {
            logMsg(QStringLiteral("Context menu item toggled: %1 -> %2").arg(id).arg(checked));
        }
    );

    QObject::connect(sendToBubbleButton,&QPushButton::clicked,central,
        [msgEditor,mdSource,renderMarkdown]()
        {
            // Closes the loop editor -> Stage 2 renderer -> Stage 3 highlighter -> styled
            // bubble, all in one click.
            mdSource->setPlainText(msgEditor->text(TextFormat::Markdown));
            renderMarkdown();
        }
    );

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(160);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(760,900);
    w.setWindowTitle("Message Formatting Demo");
    w.show();

    // TEMPORARY WIDTH DIAGNOSTIC -- remove once the horizontal-scrollbar cause is identified.
    // Reports what actually floors this scroll area's content width: with widgetResizable(true)
    // a horizontal scrollbar appears exactly when central's minimum width exceeds the viewport,
    // and central's minimum is the widest minimum among the rows below. Deferred so the first
    // real layout pass has happened before anything is measured.
    QTimer::singleShot(0,central,
        [central,mainFrame,rootLayout,logMsg]()
        {
            // Written to BOTH the demo's own log pane and qDebug(): the pane is at the very
            // bottom of a long scrolling window, while qDebug() lands in Qt Creator's
            // Application Output where it can just be copied out.
            auto report=[logMsg](const QString& line)
            {
                logMsg(line);
                qDebug().noquote()<<line;
            };

            report(QStringLiteral("--- WIDTH DIAGNOSTIC (temporary) ---"));
            report(QStringLiteral("viewport=%1  central.min=%2  central.hint=%3")
                       .arg(mainFrame->viewport()->width())
                       .arg(central->minimumSizeHint().width())
                       .arg(central->sizeHint().width()));
            for (int i=0;i<rootLayout->count();++i)
            {
                auto* item=rootLayout->itemAt(i);
                auto* widget=item!=nullptr ? item->widget() : nullptr;
                if (widget==nullptr)
                {
                    continue;
                }
                report(QStringLiteral("  [%1] %2%3 min=%4 hint=%5")
                           .arg(i,2)
                           .arg(QString::fromLatin1(widget->metaObject()->className()))
                           .arg(widget->objectName().isEmpty()
                                    ? QString()
                                    : QStringLiteral("#%1").arg(widget->objectName()))
                           .arg(widget->minimumSizeHint().width())
                           .arg(widget->sizeHint().width()));
            }
            report(QStringLiteral("--- end width diagnostic ---"));
        }
    );

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
