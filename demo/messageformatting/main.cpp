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
*  Demo of Stage 1 of task-message-formatting-plan.md: the document-level CSS channel
*  (Style::css(), consumed by ChatMessageTextBrowser) and the syntax-colour theme JSON
*  (Style::syntaxColor()), both reloading live across Auto/Light/Dark.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QDateTime>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagetext.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// bubble content width negotiated in this demo -- see demo/chatmessagefiles/main.cpp's own
// DemoBubbleWidth for why a standalone demo has to pick a fixed value.
constexpr int DemoBubbleWidth=380;

// The five overridable syntax buckets (see syntaxtheme.hpp -- "primary text" has no bucket at
// all, by design).
const QStringList SyntaxBuckets{
    QStringLiteral("keyword"),
    QStringLiteral("type"),
    QStringLiteral("literal"),
    QStringLiteral("callable"),
    QStringLiteral("comment")
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

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(160);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(700,700);
    w.setWindowTitle("Message Formatting Foundations Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
