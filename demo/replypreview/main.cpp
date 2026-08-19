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

/** @file demo/replypreview/main.cpp
*
*  Demo application of the reply-to-message widgets: ReplyBar (short preview above the editor),
*  ChatMessageReply (the bubble's reply section) and ModalReplyDialog (the full preview), all
*  three sharing one AbstractReplyPreview block.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QDateTime>
#include <QPainter>
#include <QImage>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessagefiles.hpp>
#include <uise/desktop/chatmessageimages.hpp>
#include <uise/desktop/chatfileitem.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/chatmessagereply.hpp>
#include <uise/desktop/replybar.hpp>
#include <uise/desktop/replydialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// bubble content width negotiated in this demo -- a real host (ChatMessagesView) recomputes
// this from the viewport width on every resize; this standalone demo has no viewport to derive
// it from, see demo/chatmessagefiles/main.cpp's identical constant.
constexpr int DemoBubbleWidth=360;

// Which body the full-preview dialog below is built around -- exercises whitemdesktop's own
// "quote the file/image comment" path (ChatMessageFiles::selectText()/hasSelectableText()/etc
// forwarding to their embedded ChatMessageText comment), not reachable via the Text body alone.
enum class DialogBodyKind
{
    Text,
    FilesWithComment,
    ImagesWithComment,
    ImagesNoComment
};

// Sample-content helpers, copied from demo/chatmessagefiles/main.cpp -- this demo needs only
// enough of that recipe to give ChatMessageFiles/ChatMessageImages a couple of rows/tiles plus a
// comment; see that demo for the fuller item catalogue (animated GIFs, transfer states, etc).

QImage makeSampleImage(const QSize& size, const QColor& c1, const QColor& c2, const QString& label)
{
    QImage img(size,QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0,0,size.width(),size.height());
    grad.setColorAt(0,c1);
    grad.setColorAt(1,c2);
    p.fillRect(img.rect(),grad);
    p.setPen(Qt::white);
    auto font=p.font();
    font.setPointSize(14);
    p.setFont(font);
    p.drawText(img.rect(),Qt::AlignCenter,label);
    p.end();
    return img;
}

ChatFileItem makeFileEntry(QString name, qint64 size)
{
    ChatFileItem item;
    item.setFileName(std::move(name));
    item.setSize(size);
    item.setState(ChatFileTransferState::Ready);
    return item;
}

ChatFileItem makeImageEntry(const QSize& pixelSize, const QColor& c1, const QColor& c2, const QString& label)
{
    ChatFileItem item;
    item.setFileName(label+QStringLiteral(".png"));
    item.setMimeType(QStringLiteral("image/png"));
    item.setPixelSize(pixelSize);
    item.setSize(static_cast<qint64>(pixelSize.width())*pixelSize.height()*3);
    item.setState(ChatFileTransferState::Ready);
    item.setPreview(makeSampleImage(pixelSize,c1,c2,label));
    return item;
}

// Builds the body the full-preview dialog shows, per DialogBodyKind -- the Files/Images branches
// are what exercise the new selectText()/hasSelectableText()/setCopyable()/selectionChanged
// forwarding this demo was extended to cover.
AbstractChatMessageBody* makeDialogBody(DialogBodyKind kind)
{
    switch (kind)
    {
        case (DialogBodyKind::FilesWithComment):
        {
            auto* body=new ChatMessageFiles();
            body->setItems({
                makeFileEntry(QStringLiteral("quarterly-report.pdf"),842*1024),
                makeFileEntry(QStringLiteral("appendix.docx"),128*1024)
            });
            body->setComment(QStringLiteral(
                "Select some of this comment, then look at the Save button below -- it swaps to "
                "\"Quote selected\" while a selection is active, exactly like it does for a plain "
                "text message."
            ),false);
            return body;
        }

        case (DialogBodyKind::ImagesWithComment):
        {
            auto* body=new ChatMessageImages();
            body->setItems({
                makeImageEntry(QSize(640,480),QColor(80,140,220),QColor(40,80,160),QStringLiteral("A")),
                makeImageEntry(QSize(480,640),QColor(220,140,80),QColor(160,80,40),QStringLiteral("B"))
            });
            body->setComment(QStringLiteral(
                "Select some of this caption, then look at the Save button below -- it swaps to "
                "\"Quote selected\" while a selection is active."
            ),false);
            return body;
        }

        case (DialogBodyKind::ImagesNoComment):
        {
            auto* body=new ChatMessageImages();
            body->setItems({
                makeImageEntry(QSize(640,480),QColor(80,140,220),QColor(40,80,160),QStringLiteral("A"))
            });
            // No comment at all -- regression check: the "you can select part of the text" hint
            // must stay hidden and Save must never swap to "Quote selected" for this one.
            return body;
        }

        case (DialogBodyKind::Text):
        default:
        {
            auto* body=new ChatMessageText();
            body->loadText(
                QStringLiteral("Select some of this text, then look at the Save button below -- "
                                "it swaps to \"Quote selected\" while a selection is active."
                               "Select some of this text, then look at the Save button below -- "
                               "it swaps to \"Quote selected\" while a selection is active."
                               "Select some of this text, then look at the Save button below -- "
                               "it swaps to \"Quote selected\" while a selection is active."
                               "Select some of this text, then look at the Save button below -- "
                               "it swaps to \"Quote selected\" while a selection is active."
                               ),
                false
            );
            return body;
        }
    }
}

// Builds a real ChatMessage/ChatMessageContent bubble around `body` (+ an optional `reply`
// section), so bubble-width negotiation is genuinely exercised -- same recipe as
// demo/chatmessagefiles/main.cpp's own makeMessage().
AbstractChatMessage* makeMessage(QWidget* parent, AbstractChatMessage::Direction direction,
                                 AbstractChatMessageBody* body, AbstractChatMessageReply* reply=nullptr)
{
    AbstractChatMessage* msg=new ChatMessage(parent);
    msg->construct();
    msg->setDirection(direction);
    msg->setDateTime(QDateTime::currentDateTime());

    auto content=new ChatMessageContent(msg);
    content->setChatMessage(msg);

    auto bottom=new ChatMessageBottom(content);
    bottom->setTimeString(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));

    content->setWidgets(body,nullptr,bottom,reply);
    msg->setContent(content);

    content->updateBubbleWidth(DemoBubbleWidth);

    return msg;
}

ReplyPreviewData makeReplyData(ReplyMessageKind kind)
{
    ReplyPreviewData data;
    data.setMessageId(QStringLiteral("msg-42"));
    data.setSenderTitle(QStringLiteral("Alexander Konstantinopolsky"));
    data.setDateTime(QDateTime::currentDateTime());
    data.setKind(kind);

    switch (kind)
    {
        case (ReplyMessageKind::Text):
            data.setText(QStringLiteral(
                "This is a long original message that should be trimmed down to a single-line "
                "preview before it is ever shown here, well past the default two-hundred "
                "character limit, so the trim-length slider below has visible room to work with."
            ));
            break;

        case (ReplyMessageKind::Image):
            data.setText(QStringLiteral("photo.jpg"));
            break;

        case (ReplyMessageKind::File):
            data.setText(QStringLiteral("quarterly-report.pdf"));
            break;

        case (ReplyMessageKind::Call):
            data.setText(QStringLiteral("Call, 4:12"));
            break;

        case (ReplyMessageKind::Deleted):
            data.setDeleted(true);
            break;

        case (ReplyMessageKind::Unknown):
            break;
    }

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

    // --- colour theme selector, pinned at the top -- same recipe as demo/chatmessagefiles ---

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

    // --- message-kind + trim-length controls, applied to every embedded preview block below ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Reply data (drives the bar, both bubbles' reply sections):")));

    auto* kindFrame=new QFrame(central);
    auto* kindLayout=Layout::horizontal(kindFrame);
    rootLayout->addWidget(kindFrame);

    kindLayout->addWidget(new QLabel(QStringLiteral("Message kind:")));
    auto* kindCombo=new QComboBox();
    kindCombo->addItem(QStringLiteral("Text"),static_cast<int>(ReplyMessageKind::Text));
    kindCombo->addItem(QStringLiteral("Image"),static_cast<int>(ReplyMessageKind::Image));
    kindCombo->addItem(QStringLiteral("File"),static_cast<int>(ReplyMessageKind::File));
    kindCombo->addItem(QStringLiteral("Call"),static_cast<int>(ReplyMessageKind::Call));
    kindCombo->addItem(QStringLiteral("Deleted"),static_cast<int>(ReplyMessageKind::Deleted));
    kindLayout->addWidget(kindCombo,1);

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

    // --- reply bar above a dummy editor -- mocks the real chat page bottom, which this library
    // does not itself provide (MessageEditor is a bare layout around one EnhancedTextEdit) ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Reply bar above a dummy editor:")));

    auto* editorFrame=new QFrame(central);
    auto* editorLayout=Layout::vertical(editorFrame);
    rootLayout->addWidget(editorFrame);

    auto* replyBar=new ReplyBar(editorFrame);
    editorLayout->addWidget(replyBar);

    auto* dummyEditor=new QPlainTextEdit(editorFrame);
    dummyEditor->setPlaceholderText(QStringLiteral("Type a message..."));
    dummyEditor->setMaximumHeight(60);
    editorLayout->addWidget(dummyEditor);

    QObject::connect(replyBar,&AbstractReplyBar::cancelRequested,central,[logMsg](){ logMsg(QStringLiteral("replyBar: cancelRequested")); });
    QObject::connect(replyBar,&AbstractReplyBar::clicked,central,[logMsg](){ logMsg(QStringLiteral("replyBar: clicked")); });

    // --- messages with a reply section between header and body ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Messages with a reply section (between header and body):")));

    auto* receivedBody=new ChatMessageText();
    receivedBody->loadText(QStringLiteral("Sure, here you go!"),false);
    auto* receivedReply=new ChatMessageReply();
    auto* receivedMsg=makeMessage(central,AbstractChatMessage::Direction::Received,receivedBody,receivedReply);
    rootLayout->addWidget(receivedMsg);

    auto* sentBody=new ChatMessageText();
    sentBody->loadText(QStringLiteral("Thanks, got it."),false);
    auto* sentReply=new ChatMessageReply();
    auto* sentMsg=makeMessage(central,AbstractChatMessage::Direction::Sent,sentBody,sentReply);
    rootLayout->addWidget(sentMsg);

    QObject::connect(receivedReply,&AbstractChatMessageReply::clicked,central,[logMsg](){ logMsg(QStringLiteral("received bubble reply: clicked")); });
    QObject::connect(sentReply,&AbstractChatMessageReply::clicked,central,[logMsg](){ logMsg(QStringLiteral("sent bubble reply: clicked")); });

    auto applyReplyData=[replyBar,receivedReply,sentReply,kindCombo]()
    {
        auto kind=static_cast<ReplyMessageKind>(kindCombo->currentData().toInt());
        auto data=makeReplyData(kind);
        replyBar->setReplyData(data);
        receivedReply->setReplyData(data);
        sentReply->setReplyData(data);
    };
    QObject::connect(kindCombo,&QComboBox::currentIndexChanged,central,[applyReplyData](int){ applyReplyData(); });

    QObject::connect(
        trimSlider,
        &QSlider::valueChanged,
        central,
        [replyBar,receivedReply,sentReply,trimLabel](int value)
        {
            replyBar->setTextTrimLength(value);
            receivedReply->preview()->setTextTrimLength(value);
            sentReply->preview()->setTextTrimLength(value);
            trimLabel->setText(QString::number(value));
        }
    );
    applyReplyData();

    // --- deleted-tombstone / dynamic attach-detach controls ---

    auto* toolsFrame=new QFrame(central);
    auto* toolsLayout=Layout::horizontal(toolsFrame);
    rootLayout->addWidget(toolsFrame);

    auto* deletedCheck=new QCheckBox(QStringLiteral("Original message deleted (received bubble)"));
    toolsLayout->addWidget(deletedCheck);
    QObject::connect(deletedCheck,&QCheckBox::toggled,receivedReply,&AbstractChatMessageReply::setOriginalDeleted);

    auto* detachButton=new QPushButton(QStringLiteral("Toggle reply attached (sent bubble)"));
    toolsLayout->addWidget(detachButton);
    QObject::connect(
        detachButton,
        &QPushButton::clicked,
        sentMsg,
        [sentMsg,logMsg]()
        {
            auto content=sentMsg->content();
            if (content->reply()!=nullptr)
            {
                content->clearReply();
                logMsg(QStringLiteral("sent bubble: reply detached"));
            }
            else
            {
                auto reply=new ChatMessageReply();
                reply->setReplyData(makeReplyData(ReplyMessageKind::Text));
                content->setReply(reply);
                content->updateBubbleWidth(DemoBubbleWidth);
                logMsg(QStringLiteral("sent bubble: reply reattached"));
            }
        }
    );

    // --- full-preview dialog ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Full-preview dialog -- select text in the bubble below and watch Save become \"Quote selected\":")));

    auto* dialogFrame=new ModalReplyDialog();
    // FrameWithModalPopup sizes its popup as maxWidthPercent()/maxHeightPercent() of ITS OWN
    // rect() (see ModalPopup::updateWidgetGeometry(), src/modalpopup.cpp) -- with no content of
    // its own, dialogFrame would otherwise collapse to a sliver in rootLayout's QVBoxLayout,
    // making every opened dialog a percentage of ~nothing (technically "open", but invisible).
    // demo/passworddialog/main.cpp avoids this by using its own dialogFrame to also host that
    // demo's control panel; this one has no such content, so it needs an explicit floor instead.
    // The floor must be generous, not just non-zero: with isPopupAutoHeight() on, the dialog's
    // natural content height (title + scroll area + comment + actions + buttons) is measured
    // and then capped at maxHeightPercent() (80%) of THIS size -- too small a floor starves
    // that budget and forces the dialog to squeeze into far less height than it needs, which
    // looks like auto-height "not working" even though it's simply out of room to grow into.
    dialogFrame->setMinimumSize(500,750);
    rootLayout->addWidget(dialogFrame);

    // Which body the dialog's own preview bubble is built around -- see DialogBodyKind. Separate
    // from kindCombo above (that one drives the reply BAR/bubble-section preview data, not the
    // dialog's own message widget).
    auto* dialogBodyFrame=new QFrame(central);
    auto* dialogBodyLayout=Layout::horizontal(dialogBodyFrame);
    rootLayout->addWidget(dialogBodyFrame);

    dialogBodyLayout->addWidget(new QLabel(QStringLiteral("Dialog body:")));
    auto* dialogBodyCombo=new QComboBox();
    dialogBodyCombo->addItem(QStringLiteral("Text"),static_cast<int>(DialogBodyKind::Text));
    dialogBodyCombo->addItem(QStringLiteral("Files + comment"),static_cast<int>(DialogBodyKind::FilesWithComment));
    dialogBodyCombo->addItem(QStringLiteral("Images + comment"),static_cast<int>(DialogBodyKind::ImagesWithComment));
    dialogBodyCombo->addItem(QStringLiteral("Images, no comment"),static_cast<int>(DialogBodyKind::ImagesNoComment));
    dialogBodyLayout->addWidget(dialogBodyCombo,1);

    // Shared by the standalone button below AND replyBar's own configure button -- clicking
    // either one is exactly the "user wants to configure the reply operation" gesture the task
    // brief describes, so both open the same dialog.
    auto openReplyDialog=[dialogFrame,central,logMsg,replyBar,kindCombo,dialogBodyCombo]()
    {
        // openDialog(true,false): create (or reuse) the dialog WITHOUT showing/measuring it
        // yet -- ModalPopup::popup() measures the dialog exactly once, from whatever content it
        // finds at that moment, via AbstractDialog::prepareToShow() (see
        // ReplyDialog::prepareToShow()). Calling the show=true default here would measure an
        // EMPTY dialog (setMessage() below hasn't run yet) and lock that in; setMessage() then
        // has no way to make the already-shown ModalPopup frame re-measure itself afterwards
        // (it has no QLayout of its own, see ReplyDialog::prepareToShow()'s own comment). Same
        // "build content before first show" idiom as demo/fileupload/main.cpp's own
        // openDialog(false,false) call.
        bool isNew=dialogFrame->openDialog(true,false);
        if (isNew)
        {
            QObject::connect(
                dialogFrame->dialog(),
                &AbstractReplyDialog::actionTriggered,
                central,
                [logMsg](int id){ logMsg(QString("dialog: actionTriggered(%1)").arg(id)); }
            );
            QObject::connect(
                dialogFrame->dialog(),
                &AbstractReplyDialog::saveRequested,
                central,
                [dialogFrame,replyBar,kindCombo,logMsg](const QString& quoted)
                {
                    logMsg(QString("dialog: saveRequested(\"%1\")").arg(quoted));

                    // Save (quoted empty): revert to the original message's own (trimmed) text.
                    // "Quote selected" (quoted non-empty): show the user-picked fragment instead,
                    // marked isQuote() so the block applies quoteTrimLength() rather than
                    // textTrimLength() to it -- see ReplyPreview::refresh()'s own doc comment.
                    auto kind=static_cast<ReplyMessageKind>(kindCombo->currentData().toInt());
                    auto data=makeReplyData(kind);
                    if (!quoted.isEmpty())
                    {
                        data.setText(quoted);
                        data.setQuote(true);
                    }
                    replyBar->setReplyData(data);

                    dialogFrame->closePopup();
                }
            );
        }

        auto bodyKind=static_cast<DialogBodyKind>(dialogBodyCombo->currentData().toInt());
        auto* body=makeDialogBody(bodyKind);
        auto* msg=makeMessage(dialogFrame,AbstractChatMessage::Direction::Received,body);
        dialogFrame->dialog()->setMessage(msg);

        // NOW show/measure -- messageArea already reflects msg's real height (see
        // ReplyDialog::prepareToShow(), invoked synchronously from inside popup() below).
        dialogFrame->showDialog();
    };

    QObject::connect(
        replyBar,
        &AbstractReplyBar::configureRequested,
        central,
        [openReplyDialog,logMsg]()
        {
            logMsg(QStringLiteral("replyBar: configureRequested"));
            openReplyDialog();
        }
    );

    auto* openDialogButton=new QPushButton(QStringLiteral("Open full preview dialog"));
    rootLayout->addWidget(openDialogButton);
    QObject::connect(openDialogButton,&QPushButton::clicked,central,openReplyDialog);

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(160);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(700,1000);
    w.setWindowTitle("Reply UI Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
