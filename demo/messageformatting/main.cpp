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
#include <QUrl>
#include <QDialog>
#include <QListWidget>
#include <QFrame>
#include <QRegularExpression>
#include <QSet>
#include <QHash>

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
#include <uise/desktop/hyperlinkdialog.hpp>

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
        "\n"
        "Stage 6: a mention, styled distinctly from an ordinary link when mentions are enabled on "
        "this bubble -- [Alice Anderson](whitem-mention:usr-0001) beside "
        "[an ordinary link](https://example.com).\n"
        "\n"
        "A PLAIN mention too (MessageEditor::insertMentionText()'s own form, e.g. \"Insert as "
        "@username\" above) -- hi @bob, this needs extraLinkify (see linkifyDemoMentions()) to "
        "become clickable at all, since it carries no anchor formatting of its own.\n"
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

/**
 * @brief Stands in for a real hunspell-backed checker (task-spellcheck.md) -- exercises exactly
 *  what the library actually ships: the AbstractSpellChecker seam, dictionary-vote checking
 *  against a small hardcoded word list, and the async dictionaryChanged() -> rehighlight() path.
 *
 * isReady() starts FALSE and is flipped by the "Load dictionary" button wired up below, so the
 * async leg (an editor already holding text, then squiggles appearing a moment after a dictionary
 * finishes "loading") is actually visible rather than always-on from the first paint.
 */
class DemoSpellChecker : public AbstractSpellChecker
{
    public:

        explicit DemoSpellChecker(QObject* parent=nullptr) : AbstractSpellChecker(parent)
        {
            for (const auto* word : {
                "the","quick","brown","fox","jumps","over","lazy","dog","hello","world",
                "message","editor","formatting","toolbar","spelling","dictionary","word",
                "text","bold","italic","heading","table","link","mention","quote","code",
                "correct","checker","language","custom","suggest","fix","ignore","add",
                "load","auto","enable","disable","chat","send","type","paragraph","example"
            })
            {
                m_dictionary.insert(QString::fromLatin1(word));
            }
            // A couple of deliberate typos with a real suggestion each, so "Suggest fixes" has
            // something to offer the moment the demo text loads.
            m_suggestions.insert(QStringLiteral("wrold"),QStringLiteral("world"));
            m_suggestions.insert(QStringLiteral("recieve"),QStringLiteral("receive"));
            m_suggestions.insert(QStringLiteral("teh"),QStringLiteral("the"));
        }

        bool isReady() const override
        {
            return m_ready;
        }

        void loadDictionary()
        {
            if (m_ready)
            {
                return;
            }
            m_ready=true;
            emit dictionaryChanged();
        }

        SpellCheckVerdict check(const QString& word) const override
        {
            const auto lower=word.toLower();
            if (m_dictionary.contains(lower) || m_ignored.contains(lower) || m_added.contains(lower))
            {
                return SpellCheckVerdict::Correct;
            }
            return SpellCheckVerdict::Misspelled;
        }

        QStringList suggestions(const QString& word, int maxCount) const override
        {
            QStringList out;
            auto it=m_suggestions.find(word.toLower());
            if (it!=m_suggestions.end())
            {
                out << it.value();
            }
            while (out.size()>maxCount)
            {
                out.removeLast();
            }
            return out;
        }

        bool canAddToDictionary() const override
        {
            return true;
        }

        void addToDictionary(const QString& word) override
        {
            m_added.insert(word.toLower());
            m_lastAction=QStringLiteral("added \"%1\" to dictionary").arg(word);
            emit dictionaryChanged();
        }

        void ignoreWord(const QString& word) override
        {
            m_ignored.insert(word.toLower());
            m_lastAction=QStringLiteral("ignored \"%1\"").arg(word);
            emit dictionaryChanged();
        }

        //! What the LAST addToDictionary()/ignoreWord() call did, for the demo's status readout.
        //! Empty until the first one -- loadDictionary() does not set it, since dictionaryChanged()
        //! also fires for that and the "Load dictionary" button already reports it separately.
        QString lastAction() const
        {
            return m_lastAction;
        }

    private:

        QString m_lastAction;

        bool m_ready=false;
        QSet<QString> m_dictionary;
        QSet<QString> m_added;
        QSet<QString> m_ignored;
        QHash<QString,QString> m_suggestions;
};

// Stage 6: stands in for a host's real user directory -- the group-chat picker itself is out of
// scope for this stage (todo-group-chat-mention-picker.md is blocked on group chats, which are
// not implemented), so this demo exercises exactly what the library actually ships: the
// detection SIGNAL (mentionQueryChanged/mentionQueryClosed/mentionRequested) and the two insert
// forms (insertMention()/insertMentionText()) -- not a real selector widget.
//
// username is deliberately EMPTY for one entry (Frank), mirroring
// todo-mentions-in-messages.md's own framing: "Characters might not have valid usernames and
// [are] identified by UID only." insertMentionForUser() below is the POLICY a real host is
// expected to apply from this fact -- the library itself has no opinion (see its own doc comment
// on AbstractMessageEditor::mentionRequested()): insertMention()/insertMentionText() are two
// independent, host-callable functions, and choosing between them is exactly the kind of
// application/character-cache knowledge a generic editor widget cannot have.
struct DemoUser
{
    QString uid;
    QString username;  // empty means "no username set" -- see insertMentionForUser()
    QString title;
};

const std::vector<DemoUser>& demoUsers()
{
    static const std::vector<DemoUser> users{
        {QStringLiteral("usr-0001"),QStringLiteral("alice"),QStringLiteral("Alice Anderson")},
        {QStringLiteral("usr-0002"),QStringLiteral("bob"),QStringLiteral("Bob Brown")},
        {QStringLiteral("usr-0003"),QStringLiteral("carol"),QStringLiteral("Carol Clarke")},
        {QStringLiteral("usr-0004"),QStringLiteral("dave"),QStringLiteral("Dave Dixon")},
        {QStringLiteral("usr-0005"),QStringLiteral("erin"),QStringLiteral("Erin Ellis")},
        {QStringLiteral("usr-0006"),QString{},QStringLiteral("Frank Fisher")}
    };
    return users;
}

/**
 * @brief The production POLICY this demo recommends: prefer the plain "@username" form when the
 *  character has one, and fall back to the hidden-UID anchor form only when they don't.
 *
 * This is host/application logic, not library behaviour -- MessageEditor itself never makes this
 * choice (see insertMention()/insertMentionText()'s own doc comments). It lives here, in the
 * demo, purely to SHOW where a real host is expected to put it; a real whitemdesktop integration
 * would ask its own CharacterCache/username field the same question this checks
 * (user.username.isEmpty()), not re-derive it from a hardcoded list.
 */
void insertMentionForUser(MessageEditor* editor, const DemoUser& user)
{
    if (user.username.isEmpty())
    {
        editor->insertMention(user.uid,user.title);
    }
    else
    {
        editor->insertMentionText(user.username);
    }
}

//! One place building a user's list-row label, so every list (the dialog, the combo) reads the
//! same way and a username-less user (Frank) never renders as the broken-looking "Frank Fisher
//! (@)" a naive %1 (@%2) format would produce for an empty username.
QString demoUserLabel(const DemoUser& user)
{
    return user.username.isEmpty()
        ? QStringLiteral("%1 (no username -- UID only)").arg(user.title)
        : QStringLiteral("%1 (@%2)").arg(user.title,user.username);
}

//! Whether `user` matches an "@word" prefix -- the ONE filter predicate every list in this demo
//! (the combo, the picker dialog, and mentionCompletionRequested()'s own autocomplete below)
//! shares, so they can never quietly disagree about what "matches" means.
bool demoUserMatchesPrefix(const DemoUser& user, const QString& prefix)
{
    return prefix.isEmpty() || user.username.startsWith(prefix,Qt::CaseInsensitive)
        || user.title.contains(prefix,Qt::CaseInsensitive);
}

//! The single BEST match for an autocomplete prefix -- the first demoUsers() entry matching it,
//! or nullptr. Deliberately simplistic (first-match, not best-ranked): a real host's directory
//! would rank by relevance/recency; this demo only needs to prove the gesture round-trips.
const DemoUser* bestMatchingDemoUser(const QString& prefix)
{
    const auto& users=demoUsers();
    for (const auto& user : users)
    {
        if (demoUserMatchesPrefix(user,prefix))
        {
            return &user;
        }
    }
    return nullptr;
}

//! MarkdownRenderOptions::extraLinkify for this demo -- see AbstractChatMessageText::
//! setExtraLinkify(), wired onto mdBody below. Scans a PLAIN text run for "@username" patterns
//! matching a demoUsers() entry and turns each one into a whitem-mention: anchor, exactly the
//! directory lookup a real host's character cache would do (this demo's version just checks the
//! same 6-entry list every other mention widget in it already uses). A "@word" matching no known
//! user is left untouched for the run's own default escaping -- the whole point of returning an
//! EMPTY string when nothing was found (see extraLinkify's own doc comment).
QString linkifyDemoMentions(const QString& text)
{
    static const QRegularExpression atWord(QStringLiteral("@[A-Za-z0-9_]+"));

    QString result;
    int last=0;
    bool foundAny=false;

    auto it=atWord.globalMatch(text);
    while (it.hasNext())
    {
        const auto match=it.next();
        const auto username=match.captured().mid(1); // drop the leading '@'

        const DemoUser* user=nullptr;
        for (const auto& candidate : demoUsers())
        {
            if (!candidate.username.isEmpty()
                && candidate.username.compare(username,Qt::CaseInsensitive)==0)
            {
                user=&candidate;
                break;
            }
        }
        if (user==nullptr)
        {
            continue;
        }

        foundAny=true;
        result+=text.mid(last,match.capturedStart()-last).toHtmlEscaped();
        result+=QStringLiteral("<a href=\"%1\">%2</a>")
            .arg(mentionHref(user->uid),user->title.toHtmlEscaped());
        last=match.capturedEnd();
    }

    if (!foundAny)
    {
        return QString{};
    }
    result+=text.mid(last).toHtmlEscaped();
    return result;
}

/**
 * @brief Mock user-selector dialog -- exercises the toolbar Mention button and the context menu's
 *  "Mention someone" row end to end. NOT library API and not a stand-in for the real group-chat
 *  picker (out of scope for Stage 6, blocked on group chats -- see
 *  todo-group-chat-mention-picker.md); just enough UI that clicking the button actually does
 *  something. mentionRequested() deliberately shows no dialog of its own (the editor has no user
 *  directory to search) -- the HOST opens one, same shape as the hyperlink dialog wired to
 *  linkRequested() below.
 */
class MentionPickerDialog : public QDialog
{
    public:

        //! What the dialog was accepted with -- see acceptedForm().
        enum class Form
        {
            Recommended,  //! insertMentionForUser()'s own policy: @username if set, else anchor.
            ForceAnchor,  //! Always the hidden-UID form, even for a user who HAS a username.
            ForcePlainText //! Always literal "@username" -- disabled when the user has none.
        };

        explicit MentionPickerDialog(QWidget* parent=nullptr) : QDialog(parent)
        {
            setWindowTitle(QStringLiteral("Mention someone (mock picker)"));

            auto* layout=Layout::vertical(this);

            m_prefixLabel=new QLabel(this);
            m_prefixLabel->setWordWrap(true);
            layout->addWidget(m_prefixLabel);

            m_list=new QListWidget(this);
            connect(m_list,&QListWidget::itemDoubleClicked,this,
                [this](QListWidgetItem*) { acceptAs(Form::Recommended); });
            connect(m_list,&QListWidget::currentRowChanged,this,
                [this](int) { updateForceTextEnabled(); });
            layout->addWidget(m_list);

            // The RECOMMENDED policy -- see insertMentionForUser() -- is the primary/default
            // action (double-click and Enter both trigger it), matching what a real host is
            // expected to do automatically rather than ask the end user to choose a wire format.
            m_recommendedButton=new QPushButton(QStringLiteral("Insert"),this);
            m_recommendedButton->setDefault(true);
            layout->addWidget(m_recommendedButton);

            // The two FORCED forms stay available underneath, clearly secondary -- useful for
            // deliberately exercising each library entry point independently of the policy above,
            // which is exactly what this dialog exists to let you do.
            auto* buttonRow=new QFrame(this);
            auto* buttonLayout=Layout::horizontal(buttonRow);
            buttonLayout->addWidget(new QLabel(QStringLiteral("Force:"),buttonRow));
            m_forceAnchorButton=new QPushButton(QStringLiteral("hidden-UID mention"),buttonRow);
            m_forceTextButton=new QPushButton(QStringLiteral("@username text"),buttonRow);
            auto* cancelButton=new QPushButton(QStringLiteral("Cancel"),buttonRow);
            buttonLayout->addWidget(m_forceAnchorButton);
            buttonLayout->addWidget(m_forceTextButton);
            buttonLayout->addStretch(1);
            buttonLayout->addWidget(cancelButton);
            layout->addWidget(buttonRow);

            connect(m_recommendedButton,&QPushButton::clicked,this,[this]() { acceptAs(Form::Recommended); });
            connect(m_forceAnchorButton,&QPushButton::clicked,this,[this]() { acceptAs(Form::ForceAnchor); });
            connect(m_forceTextButton,&QPushButton::clicked,this,[this]() { acceptAs(Form::ForcePlainText); });
            connect(cancelButton,&QPushButton::clicked,this,&QDialog::reject);

            resize(380,340);
        }

        //! Repopulates the list, filtered by the "@word" prefix mentionRequested() handed over --
        //! same filter logic as the combo box below, so both stay consistent.
        //!
        //! Falls back to the FULL list when the filter matches nobody, rather than a silently
        //! empty one: the fake directory is only 5 names (alice/bob/carol/dave/erin), so testing
        //! with any other placeholder word -- "@username", "@test", one's own name -- would
        //! otherwise leave both this dialog and the combo below looking broken (empty, nothing
        //! to click) when detection itself worked correctly. A real host's directory is large
        //! enough that "no matches" is a legitimate, distinct state from "still narrowing it
        //! down" -- this demo's fake one is not, so the fallback is demo-only pragmatism, not a
        //! recommended real-world default.
        void setPrefixFilter(const QString& prefix)
        {
            m_list->clear();
            m_indices.clear();
            const auto& users=demoUsers();
            for (std::size_t i=0;i<users.size();++i)
            {
                const auto& user=users[i];
                if (demoUserMatchesPrefix(user,prefix))
                {
                    m_list->addItem(demoUserLabel(user));
                    m_indices.push_back(i);
                }
            }

            bool fellBackToFullList=false;
            if (m_list->count()==0 && !prefix.isEmpty())
            {
                fellBackToFullList=true;
                for (std::size_t i=0;i<users.size();++i)
                {
                    m_list->addItem(demoUserLabel(users[i]));
                    m_indices.push_back(i);
                }
            }

            if (prefix.isEmpty())
            {
                m_prefixLabel->setText(QStringLiteral("Pick a user to mention:"));
            }
            else if (fellBackToFullList)
            {
                m_prefixLabel->setText(QStringLiteral(
                    "No demo user matches \"@%1\" (only alice/bob/carol/dave/erin exist here) -- "
                    "showing everyone instead:").arg(prefix));
            }
            else
            {
                m_prefixLabel->setText(QStringLiteral("Filtering by \"@%1\":").arg(prefix));
            }

            if (m_list->count()>0)
            {
                m_list->setCurrentRow(0);
            }
            updateForceTextEnabled();
        }

        //! Valid only once exec() has returned QDialog::Accepted.
        Form acceptedForm() const noexcept { return m_form; }

        //! Valid only once exec() has returned QDialog::Accepted. nullptr if the list was empty.
        const DemoUser* selectedUser() const
        {
            auto row=m_list->currentRow();
            if (row<0 || static_cast<std::size_t>(row)>=m_indices.size())
            {
                return nullptr;
            }
            return &demoUsers().at(m_indices.at(static_cast<std::size_t>(row)));
        }

    private:

        //! "Force: @username text" is meaningless (inserts a bare "@" with no name) for a user
        //! with no username -- greyed out rather than silently producing that, so the one demo
        //! user without one (Frank) can't be driven into it by accident.
        void updateForceTextEnabled()
        {
            const auto* user=selectedUser();
            m_forceTextButton->setEnabled(user!=nullptr && !user->username.isEmpty());
        }

        void acceptAs(Form form)
        {
            if (m_list->count()==0)
            {
                return;
            }
            m_form=form;
            accept();
        }

        QLabel* m_prefixLabel=nullptr;
        QListWidget* m_list=nullptr;
        QPushButton* m_recommendedButton=nullptr;
        QPushButton* m_forceAnchorButton=nullptr;
        QPushButton* m_forceTextButton=nullptr;
        std::vector<std::size_t> m_indices;
        Form m_form=Form::Recommended;
};

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

    // --- Bubble-tail sync: a one-line bubble beside a FORCED avatar column. Every message in
    // this demo already gets ChatMessage::ForcedAvatarSize (32px, plus avatarBottomOffset) --
    // makeMessage() leaves AlignSent at its own default (Left) and isLastInBatch() defaults to
    // true -- so the forced avatar column (32+6=38px) is routinely taller than a short one-line
    // bubble (~34px). ChatMessageAvatar paints the tail at its OWN bottom edge, which used to
    // leave it hanging below a bubble this short; setAlignSent()/setLastInBatch() are called
    // explicitly here anyway, so this stays the load-bearing case even if makeMessage()'s own
    // defaults ever change. ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Bubble tail sync (one-line bubble beside a forced avatar):")));

    auto* shortBody=new ChatMessageText();
    shortBody->loadText(QStringLiteral("hi"),TextFormat::Plain);
    auto* shortMessage=makeMessage(central,AbstractChatMessage::Direction::Received,shortBody);
    shortMessage->setAlignSent(AbstractChatMessage::AlignSent::Left);
    shortMessage->setLastInBatch(true);
    shortMessage->setAvatarName(std::string("Demo"));
    rootLayout->addWidget(shortMessage);

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
    // Stage 6: opted in explicitly, same shape as syntaxHighlighting/wideTableScroll above --
    // default false, so an existing host stays unchanged until it asks for mention rendering.
    mdBody->setMentionsEnabled(true);
    // Makes a PLAIN "@alice" mention (MessageEditor::insertMentionText()'s own form) clickable
    // too, not just the explicit [Title](whitem-mention:uid) anchor form -- see
    // linkifyDemoMentions()'s own doc comment. mentionsEnabled alone only allowlists the scheme
    // for anchors already present in the source; recognizing BARE "@word" text needs this too.
    mdBody->setExtraLinkify(linkifyDemoMentions);
    auto* mdMessage=makeMessage(central,AbstractChatMessage::Direction::Sent,mdBody);
    rootLayout->addWidget(mdMessage);

    // Stage 6, item 5: the existing setOpenLinks(false)/linkActivated switchboard needs no change
    // at all for a custom scheme -- clicking a mention logs scheme "whitem-mention", clicking an
    // ordinary link logs "https", through the one connection below.
    QObject::connect(mdBody,&AbstractChatMessageBody::linkActivated,central,
        [logMsg](const QUrl& url)
        {
            logMsg(QStringLiteral("Link activated: scheme=%1 url=%2").arg(url.scheme(),url.toString()));
        }
    );

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
        // NOT toPlainText(): it is documented to replace U+00A0 with an ordinary space, and does.
        // MessageEditor exports a blank line as a NO-BREAK SPACE paragraph (the only way one
        // survives markdown at all -- see fillEmptyBlocksForExport()), and a line holding a single
        // ORDINARY space is a blank line to CommonMark, so the paragraph would be dropped and two
        // tables either side of it would weld back together. Measured end to end: the same source
        // read back with toPlainText() renders "</table><p></p><table>", and read back this way
        // "</table><p> </p><table>".
        //
        // Same trap and same fix as MessageEditor::plainTextKeepingIndent(); worth knowing about in
        // any host that stores this markdown in a QTextDocument-backed widget on its way to the
        // renderer.
        auto src=mdSource->document()->toRawText();
        src.replace(QChar::ParagraphSeparator,QLatin1Char('\n'));
        src.replace(QChar::LineSeparator,QLatin1Char('\n'));

        MarkdownRenderOptions options;
        options.hardLineBreaks=softBreakCheck->isChecked();
        if (mdBody->isMentionsEnabled())
        {
            // Kept in step with what mdBody->loadText(...,Markdown) below does internally when
            // hardLineBreaks is on, so the HTML shown in the inspection pane always matches what
            // the bubble actually rendered, regardless of which branch below produced it.
            options.allowedLinkSchemes.append(mentionUrlScheme());
        }
        // Same reasoning as the scheme above -- mdBody's own extraLinkify, so a plain "@bob"
        // shows up linkified in the inspection pane too, not just in the bubble itself.
        options.extraLinkify=linkifyDemoMentions;
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

                          // Stage 6's OTHER leg: the DEFAULT options above correctly reject the
                          // scheme (checked above), but a caller-owned copy with it added --
                          // exactly what setMentionsEnabled(true) builds internally -- accepts
                          // the identical hostile input. Both legs visible side by side.
                          MarkdownRenderOptions mentionOptions;
                          mentionOptions.allowedLinkSchemes.append(mentionUrlScheme());
                          const auto mentionHtml=markdownToHtml(hostileMarkdown(),mentionOptions);
                          logMsg(QStringLiteral("  (opt-in) whitem-mention: accepted with the scheme added: %1")
                                 .arg(mentionHtml.contains(QStringLiteral("whitem-mention:12345")) ? "yes" : "no"));
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
    // Stage 6: both default off, same reasoning as expandButtonVisible -- a mention button/menu
    // row with no user directory behind it does nothing at all.
    msgEditor->setMentionButtonVisible(true);
    msgEditor->setMentionMenuItemVisible(true);
    // task-spellcheck.md: same off-by-default reasoning as Mention above -- a Check-spelling
    // button/menu row with no checker behind it does nothing at all.
    msgEditor->setSpellCheckButtonVisible(true);
    msgEditor->setSpellCheckMenuItemVisible(true);
    // Ceiling is 40% of the window, never below 180px -- resize the demo window and the editor's
    // growth limit follows it. Both the auto-resize growth and the expanded height stop here.
    msgEditor->setMaxHeight(180);
    msgEditor->setMaxHeightPercent(40);
    msgEditor->setFinishOnEnter(false);
    msgEditor->setPlaceHolderText(QStringLiteral("Type a message..."));
    msgEditor->loadText(QStringLiteral(
        "This line has a wrold of teh "
        "typos to **demonstrate** *spellcheck*."
    ),TextFormat::Markdown);
    rootLayout->addWidget(msgEditor);

    // task-spellcheck.md: no dictionary loaded yet -- squiggles appear only after "Load
    // dictionary" below, so the async dictionaryChanged() -> rehighlight() leg is visible rather
    // than always-on from the first paint.
    auto* spellChecker=new DemoSpellChecker(msgEditor);
    msgEditor->setSpellChecker(spellChecker);
    QObject::connect(spellChecker,&AbstractSpellChecker::dictionaryChanged,central,
        [spellChecker,logMsg]()
        {
            const auto action=spellChecker->lastAction();
            if (!action.isEmpty())
            {
                logMsg(QStringLiteral("Spellcheck: %1").arg(action));
            }
        }
    );

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

    // task-spellcheck.md: "Load dictionary" -- flips DemoSpellChecker::isReady() and emits
    // dictionaryChanged(), which is what actually makes the "wrold"/"teh" squiggles above appear
    // a moment later rather than immediately.
    auto* spellDictionaryLabel=new QLabel(QStringLiteral("dictionary: not loaded"));
    editorStatusLayout->addWidget(spellDictionaryLabel);
    auto* loadDictionaryButton=new QPushButton(QStringLiteral("Load dictionary"));
    editorStatusLayout->addWidget(loadDictionaryButton);
    QObject::connect(loadDictionaryButton,&QPushButton::clicked,central,
        [spellChecker,spellDictionaryLabel,loadDictionaryButton]()
        {
            spellChecker->loadDictionary();
            spellDictionaryLabel->setText(QStringLiteral("dictionary: loaded"));
            loadDictionaryButton->setEnabled(false);
        }
    );

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

    // Stage 5b: the editor has no dialog of its own -- the HOST owns the hyperlink dialog,
    // exactly like every other AbstractHyperlinkDialog-family consumer (mirrors
    // demo/replypreview's own ModalReplyDialog wiring). setMinimumSize() is needed for the same
    // reason noted there: FrameWithModalPopup sizes its popup as a percentage of ITS OWN rect(),
    // and with no content of its own this frame would otherwise collapse to a sliver in
    // rootLayout's QVBoxLayout -- making every dialog opened on it a percentage of ~nothing.
    // The 520px height is not decorative: even with popup auto-height on, the popup's ceiling is
    // maxHeightPercent() of THIS frame, so a host sized to the dialog's own natural height still
    // clips it (measured: a 220px host squeezed the two fields into an 88px popup).
    auto* linkDialogFrame=new ModalHyperlinkDialog();
    linkDialogFrame->setMinimumSize(560,520);
    rootLayout->addWidget(linkDialogFrame);

    QObject::connect(msgEditor,&AbstractMessageEditor::linkRequested,central,
        [linkDialogFrame,msgEditor](const QString& defaultTitle, const QString& existingUrl)
        {
            const bool isNew=linkDialogFrame->openDialog(true,false);
            if (isNew)
            {
                QObject::connect(
                    linkDialogFrame->dialog(),
                    &AbstractHyperlinkDialog::linkAccepted,
                    msgEditor,
                    [linkDialogFrame,msgEditor](const QString& url, const QString& title)
                    {
                        msgEditor->insertLink(url,title);
                        linkDialogFrame->closePopup();
                    }
                );
            }

            linkDialogFrame->dialog()->setUrl(existingUrl);
            linkDialogFrame->dialog()->setLinkTitle(defaultTitle);
            linkDialogFrame->showDialog();
        }
    );

    // Stage 6: the editor has no user selector of its own (see mentionRequested()'s own doc
    // comment) -- the real group-chat picker is out of scope for this stage, blocked on group
    // chats (todo-group-chat-mention-picker.md). This small combo + two buttons stands in for it,
    // exercising both insert forms explicitly rather than building a picker widget.
    auto* mentionFrame=new QFrame(central);
    auto* mentionLayout=Layout::horizontal(mentionFrame);
    rootLayout->addWidget(mentionFrame);

    mentionLayout->addWidget(new QLabel(QStringLiteral("Mention (Stage 6):")));

    auto* mentionCombo=new QComboBox();
    mentionLayout->addWidget(mentionCombo,1);

    auto* insertMentionAnchorButton=new QPushButton(QStringLiteral("Insert as mention (hidden UID)"));
    mentionLayout->addWidget(insertMentionAnchorButton);

    auto* insertMentionTextButton=new QPushButton(QStringLiteral("Insert as @username"));
    mentionLayout->addWidget(insertMentionTextButton);

    // Repopulates mentionCombo filtered by `prefix`, storing each row's REAL demoUsers() index as
    // item data (Qt::UserRole) rather than relying on row position -- once the combo is filtered,
    // row 0 is not demoUsers()[0] any more, and reading currentIndex() straight into demoUsers()
    // (the original version of this demo did exactly that) silently inserted the WRONG user.
    // Falls back to the full list when nothing matches (see MentionPickerDialog::setPrefixFilter()
    // for why -- same reasoning, same 5-name fake directory).
    auto populateMentionCombo=[mentionCombo](const QString& prefix)
    {
        mentionCombo->clear();
        const auto& users=demoUsers();
        for (std::size_t i=0;i<users.size();++i)
        {
            const auto& user=users[i];
            if (demoUserMatchesPrefix(user,prefix))
            {
                mentionCombo->addItem(demoUserLabel(user),QVariant::fromValue(static_cast<qulonglong>(i)));
            }
        }
        if (mentionCombo->count()==0 && !prefix.isEmpty())
        {
            for (std::size_t i=0;i<users.size();++i)
            {
                mentionCombo->addItem(demoUserLabel(users[i]),QVariant::fromValue(static_cast<qulonglong>(i)));
            }
        }
    };
    populateMentionCombo(QString{});

    auto currentMentionComboUser=[mentionCombo]() -> const DemoUser*
    {
        if (mentionCombo->count()==0)
        {
            return nullptr;
        }
        return &demoUsers().at(static_cast<std::size_t>(mentionCombo->currentData().toULongLong()));
    };

    QObject::connect(insertMentionAnchorButton,&QPushButton::clicked,central,
        [msgEditor,currentMentionComboUser]()
        {
            if (const auto* user=currentMentionComboUser())
            {
                msgEditor->insertMention(user->uid,user->title);
            }
        }
    );
    QObject::connect(insertMentionTextButton,&QPushButton::clicked,central,
        [msgEditor,currentMentionComboUser,logMsg]()
        {
            const auto* user=currentMentionComboUser();
            if (user==nullptr)
            {
                return;
            }
            if (user->username.isEmpty())
            {
                // Frank has none -- forcing this form on him would insert a bare "@" with no
                // name, which is not useful to demonstrate; the MentionPickerDialog above greys
                // its own equivalent button out for the same user for the same reason.
                logMsg(QStringLiteral("%1 has no username -- \"Insert as @username\" has nothing "
                                       "to insert (try \"Insert as mention\" instead).").arg(user->title));
                return;
            }
            msgEditor->insertMentionText(user->username);
        }
    );

    // The detection signal made visible -- the running proof it carries enough to drive a real
    // selector, without building one: filters mentionCombo down to matching users as the prefix
    // changes, and restores the full list when the query closes.
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionQueryChanged,central,
        [populateMentionCombo,logMsg](const QString& prefix, int position)
        {
            logMsg(QStringLiteral("Mention query: prefix=\"%1\" at position %2").arg(prefix).arg(position));
            populateMentionCombo(prefix);
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionQueryClosed,central,
        [populateMentionCombo,logMsg]()
        {
            logMsg(QStringLiteral("Mention query closed"));
            populateMentionCombo(QString{});
        }
    );

    // One picker instance, reused for BOTH triggers -- a deliberate toolbar/menu click (a modal-
    // feeling, ACTIVATED popup, keyboard-navigable) and live typing of "@word" (a NON-activating
    // popup that updates as you type without stealing keyboard focus from the text edit -- a
    // truly modal QDialog::exec() would block every further keystroke the moment the first '@'
    // appeared, which is exactly wrong for "keep typing to keep narrowing it down"). Both paths
    // share the same accept/cancel handling below via QDialog::accepted()/rejected(), which
    // accept()/reject() emit regardless of whether the dialog was opened with exec() or show().
    //
    // Selecting a row from the LIVE (non-activating) popup still needs a MOUSE click -- with
    // keyboard focus deliberately left on the text edit so typing keeps working, there is no
    // widget for arrow-key/Enter navigation to reach. A real host wanting full keyboard
    // navigation while typing (Slack/Discord-style) builds an inline overlay that intercepts
    // Up/Down/Enter/Escape itself rather than a separate top-level window -- a materially bigger
    // feature than this demo attempts; mentionQueryChanged()/mentionQueryClosed() are exactly the
    // two signals such an overlay would be driven by.
    auto* mentionPicker=new MentionPickerDialog(&w);

    QObject::connect(mentionPicker,&QDialog::accepted,msgEditor,
        [msgEditor,mentionPicker,logMsg]()
        {
            const auto* user=mentionPicker->selectedUser();
            if (user==nullptr)
            {
                return;
            }

            QString formLabel;
            switch (mentionPicker->acceptedForm())
            {
                case MentionPickerDialog::Form::Recommended:
                    // The production POLICY this demo recommends -- see insertMentionForUser():
                    // @username when the character has one, the hidden-UID anchor only when they
                    // don't. Neither insertMention() nor insertMentionText() makes this choice on
                    // its own; it is host logic, demonstrated here rather than library behaviour.
                    insertMentionForUser(msgEditor,*user);
                    formLabel=user->username.isEmpty()
                        ? QStringLiteral("mention (recommended: no username)")
                        : QStringLiteral("@username (recommended)");
                    break;
                case MentionPickerDialog::Form::ForceAnchor:
                    msgEditor->insertMention(user->uid,user->title);
                    formLabel=QStringLiteral("mention (forced)");
                    break;
                case MentionPickerDialog::Form::ForcePlainText:
                    msgEditor->insertMentionText(user->username);
                    formLabel=QStringLiteral("@username (forced)");
                    break;
            }
            logMsg(QStringLiteral("Mention picker inserted %1 as %2").arg(user->title,formLabel));
        }
    );
    QObject::connect(mentionPicker,&QDialog::rejected,msgEditor,
        [logMsg]()
        {
            logMsg(QStringLiteral("Mention picker cancelled"));
        }
    );

    // Trigger 1: the toolbar Mention button / context menu's "Mention someone" row. A deliberate
    // click, so the popup ACTIVATES and grabs keyboard focus -- fully navigable with the mouse or
    // keyboard, closed by picking a row/button or Cancel.
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionRequested,central,
        [mentionPicker,logMsg](const QString& prefix)
        {
            logMsg(QStringLiteral("Mention requested (toolbar/menu), prefix=\"%1\"").arg(prefix));
            mentionPicker->setPrefixFilter(prefix);
            mentionPicker->setAttribute(Qt::WA_ShowWithoutActivating,false);
            mentionPicker->show();
            mentionPicker->raise();
            mentionPicker->activateWindow();
        }
    );

    // Trigger 2: typing "@word" directly in the editor -- what was previously invisible beyond
    // the combo box below. Shown NON-activating (WA_ShowWithoutActivating) so the text edit keeps
    // keyboard focus and typing the next character keeps working; the popup just tracks along,
    // re-filtered on every keystroke via the same setPrefixFilter() the toolbar path uses.
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionQueryChanged,central,
        [mentionPicker](const QString& prefix, int)
        {
            mentionPicker->setPrefixFilter(prefix);
            mentionPicker->setAttribute(Qt::WA_ShowWithoutActivating,true);
            mentionPicker->show();
            mentionPicker->raise();
        }
    );
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionQueryClosed,central,
        [mentionPicker]()
        {
            mentionPicker->hide();
        }
    );

    // Trigger 3: Tab pressed while "@word" is in progress -- the autocomplete gesture, distinct
    // from BOTH triggers above. Unlike the picker (which always shows a UI to choose from) this
    // is deliberately SILENT-on-no-match, the conventional autocomplete contract: it either
    // completes to the one best guess or does nothing, it never falls back to "show everyone" the
    // way the picker's own small-fake-directory pragmatism does (see
    // MentionPickerDialog::setPrefixFilter()) -- a real host with a real directory would want the
    // same silence, not a promptless directory dump on every unmatched Tab.
    QObject::connect(msgEditor,&AbstractMessageEditor::mentionCompletionRequested,central,
        [msgEditor,mentionPicker,logMsg](const QString& prefix, int position)
        {
            const auto* user=bestMatchingDemoUser(prefix);
            if (user==nullptr)
            {
                logMsg(QStringLiteral("Tab-complete: no demo user matches \"@%1\" at position %2 -- nothing inserted")
                       .arg(prefix).arg(position));
                return;
            }
            // Same recommended POLICY the picker's primary button applies -- see
            // insertMentionForUser(): @username when the character has one, hidden-UID anchor
            // only when they don't. Autocomplete has no separate "force" concept; there is no UI
            // moment to offer one in.
            insertMentionForUser(msgEditor,*user);
            logMsg(QStringLiteral("Tab-complete: \"@%1\" -> %2").arg(prefix,user->title));
            // The live popup (if it happened to be showing for this same query) no longer has
            // anything to track -- the query it was filtering on is gone now that the mention is
            // inserted, so hide it rather than leaving it showing the pre-completion filter.
            mentionPicker->hide();
        }
    );

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

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
