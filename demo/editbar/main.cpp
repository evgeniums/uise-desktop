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

/** @file demo/editbar/main.cpp
*
*  Demo application of the edit-preview bar (EditBar) shown above the chat editor while an
*  existing message is being edited -- shares the same AbstractReplyPreview block already built
*  for the reply-to-message feature, see demo/replypreview/main.cpp, this demo's template.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QDateTime>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/editbar.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

ReplyPreviewData makeEditData(const QString& text)
{
    ReplyPreviewData data;
    data.setMessageId(QStringLiteral("msg-42"));
    // Deliberately left empty -- an edit is always the user's own message, so the bar's title
    // ("Edit message, <datetime>") never names a sender. See editbar.cpp's ctor.
    data.setSenderTitle(QString());
    data.setDateTime(QDateTime::currentDateTime());
    data.setKind(ReplyMessageKind::Text);
    data.setText(text);
    return data;
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

    // --- colour theme selector, pinned at the top -- same recipe as demo/replypreview ---

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
        [themeCombo](int index)
        {
            auto mode=static_cast<Style::StyleSheetMode>(themeCombo->itemData(index).toInt());
            Style::instance().setStyleSheetMode(mode);
            Style::instance().applyStyleSheet(true);
        }
    );

    // --- edit bar above a dummy editor -- mocks the real chat page bottom, which this library
    // does not itself provide (MessageEditor is a bare layout around one EnhancedTextEdit) ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Edit bar above a dummy editor:")));

    auto* editorFrame=new QFrame(central);
    auto* editorLayout=Layout::vertical(editorFrame);
    rootLayout->addWidget(editorFrame);

    auto* editBar=new EditBar(editorFrame);
    editorLayout->addWidget(editBar);

    auto* dummyEditor=new QPlainTextEdit(editorFrame);
    dummyEditor->setPlaceholderText(QStringLiteral("Type a message..."));
    dummyEditor->setMaximumHeight(60);
    editorLayout->addWidget(dummyEditor);

    QObject::connect(editBar,&AbstractEditBar::jumpRequested,central,[logMsg](){ logMsg(QStringLiteral("editBar: jumpRequested")); });
    QObject::connect(editBar,&AbstractEditBar::cancelRequested,central,[logMsg](){ logMsg(QStringLiteral("editBar: cancelRequested")); });

    // --- original-text + trim-length controls ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Original message text (drives the bar's preview):")));

    auto* textEdit=new QPlainTextEdit(central);
    textEdit->setPlainText(QStringLiteral(
        "This is a long original message that should be trimmed down to a single-line preview "
        "before it is ever shown here, well past the default two-hundred character limit, so the "
        "trim-length slider below has visible room to work with."
    ));
    textEdit->setMaximumHeight(80);
    rootLayout->addWidget(textEdit);

    auto* trimFrame=new QFrame(central);
    auto* trimLayout=Layout::horizontal(trimFrame);
    rootLayout->addWidget(trimFrame);

    trimLayout->addWidget(new QLabel(QStringLiteral("Text trim length:")));
    auto* trimSlider=new QSlider(Qt::Horizontal);
    trimSlider->setRange(20,400);
    trimSlider->setValue(DefaultReplyTextTrimLength);
    trimLayout->addWidget(trimSlider,1);
    auto* trimLabel=new QLabel(QString::number(DefaultReplyTextTrimLength));
    trimLayout->addWidget(trimLabel);

    auto applyEditData=[editBar,textEdit]()
    {
        // Re-stamps the datetime on every edit too -- title format is "Edit message, %2", so this
        // is what actually exercises the ReplyPreview::refresh() %2-only substitution branch,
        // proving it renders the datetime rather than silently dropping it.
        editBar->setEditData(makeEditData(textEdit->toPlainText()));
    };
    QObject::connect(textEdit,&QPlainTextEdit::textChanged,central,applyEditData);
    QObject::connect(
        trimSlider,
        &QSlider::valueChanged,
        central,
        [editBar,trimLabel](int value)
        {
            editBar->setTextTrimLength(value);
            trimLabel->setText(QString::number(value));
        }
    );
    applyEditData();

    // --- closeOnEscape toggle -- demonstrates the ambiguous-shortcut trade-off the property
    // exists to avoid (see AbstractEditBar::closeOnEscape's own doc comment): with it ON, Escape
    // over this window closes the bar; a real host with its own window-level Escape ladder (e.g.
    // whitemdesktop's ChatPage) must turn it OFF and react to cancelRequested() from its own
    // ladder instead. ---

    auto* toolsFrame=new QFrame(central);
    auto* toolsLayout=Layout::horizontal(toolsFrame);
    rootLayout->addWidget(toolsFrame);

    auto* escCheck=new QCheckBox(QStringLiteral("closeOnEscape"));
    escCheck->setChecked(editBar->isCloseOnEscape());
    toolsLayout->addWidget(escCheck);
    QObject::connect(escCheck,&QCheckBox::toggled,editBar,&AbstractEditBar::setCloseOnEscape);

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(160);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(700,600);
    w.setWindowTitle("Edit Bar Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
