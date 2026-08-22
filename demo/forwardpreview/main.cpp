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

/** @file demo/forwardpreview/main.cpp
*
*  Demo application of the message-forwarding widgets (task-message-forwarding.md, Stage 1):
*  ForwardBar (short preview above the editor), ChatMessageForwardHeader (the bubble's
*  "Forwarded from <author>" header), ChatMessageComment (the bubble's own sender-comments
*  section) and ModalForwardDialog (the full preview), the first three sharing the same
*  AbstractReplyPreview block already built for the reply-to-message feature -- see
*  demo/replypreview/main.cpp, this demo's direct template.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QSpinBox>
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
#include <uise/desktop/chatmessageforwardheader.hpp>
#include <uise/desktop/chatmessagecomment.hpp>
#include <uise/desktop/forwardbar.hpp>
#include <uise/desktop/forwarddialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// bubble content width negotiated in this demo -- a real host (ChatMessagesView) recomputes
// this from the viewport width on every resize; this standalone demo has no viewport to derive
// it from, see demo/replypreview/main.cpp's identical constant.
constexpr int DemoBubbleWidth=360;

// Which body a message/the dialog's own preview bubble is built around -- same catalogue as
// demo/replypreview/main.cpp's DialogBodyKind, reused here for both the standing bubbles and the
// dialog.
enum class DemoBodyKind
{
    Text,
    FilesWithComment,
    ImagesWithComment,
    ImagesNoComment
};

// Sample-content helpers, copied from demo/replypreview/main.cpp -- this demo needs only enough
// of that recipe to give ChatMessageFiles/ChatMessageImages a couple of rows/tiles; see that
// demo (or demo/chatmessagefiles/main.cpp) for the fuller item catalogue.

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

AbstractChatMessageBody* makeBody(DemoBodyKind kind)
{
    switch (kind)
    {
        case (DemoBodyKind::FilesWithComment):
        {
            auto* body=new ChatMessageFiles();
            body->setItems({
                makeFileEntry(QStringLiteral("quarterly-report.pdf"),842*1024),
                makeFileEntry(QStringLiteral("appendix.docx"),128*1024)
            });
            body->setComment(QStringLiteral(
                "This is the FILE BODY's own comment (e.g. a caption typed when the file was "
                "first sent) -- distinct from the forwarding sender's comment, which shows up as "
                "a separate section below the whole bubble."
            ),false);
            return body;
        }

        case (DemoBodyKind::ImagesWithComment):
        {
            auto* body=new ChatMessageImages();
            body->setItems({
                makeImageEntry(QSize(640,480),QColor(80,140,220),QColor(40,80,160),QStringLiteral("A")),
                makeImageEntry(QSize(480,640),QColor(220,140,80),QColor(160,80,40),QStringLiteral("B"))
            });
            body->setComment(QStringLiteral("The IMAGE BODY's own caption."),false);
            return body;
        }

        case (DemoBodyKind::ImagesNoComment):
        {
            auto* body=new ChatMessageImages();
            body->setItems({
                makeImageEntry(QSize(640,480),QColor(80,140,220),QColor(40,80,160),QStringLiteral("A"))
            });
            return body;
        }

        case (DemoBodyKind::Text):
        default:
        {
            auto* body=new ChatMessageText();
            body->loadText(
                QStringLiteral("This is the original message's own text, forwarded as-is (the "
                                "\"cited\" form) -- select some of it in the full-preview dialog "
                                "below and Save swaps to \"Quote selected\", exactly like the "
                                "reply feature's own dialog."),
                false
            );
            return body;
        }
    }
}

// Builds a real ChatMessage/ChatMessageContent bubble around `body`, plus the two new sections
// this demo exercises: `header` ("Forwarded from <author>") and `comment` (the forwarding
// sender's own comments) -- same recipe as demo/replypreview/main.cpp's own makeMessage(), with
// the reply slot dropped (not relevant here) and the header/comment slots added.
AbstractChatMessage* makeMessage(QWidget* parent, AbstractChatMessage::Direction direction,
                                 AbstractChatMessageBody* body,
                                 AbstractChatMessageHeader* header=nullptr,
                                 AbstractChatMessageComment* comment=nullptr)
{
    AbstractChatMessage* msg=new ChatMessage(parent);
    msg->construct();
    msg->setDirection(direction);
    msg->setDateTime(QDateTime::currentDateTime());

    auto content=new ChatMessageContent(msg);
    // Must run BEFORE setWidgets() -- rebuildSections() reparents every section via
    // setChatMessage(chatMessage()); with chatMessage() still null at that point every section
    // becomes a top-level window and flashes on screen.
    content->setChatMessage(msg);

    auto bottom=new ChatMessageBottom(content);
    bottom->setTimeString(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));

    content->setWidgets(body,header,bottom,nullptr,comment);
    msg->setContent(content);

    content->updateBubbleWidth(DemoBubbleWidth);

    return msg;
}

ReplyPreviewData makeForwardData(ReplyMessageKind kind)
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

    // --- author kind + trim-length controls, applied to the bar and both bubbles' headers ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Forward data (drives the bar and both bubbles' headers):")));

    auto* kindFrame=new QFrame(central);
    auto* kindLayout=Layout::horizontal(kindFrame);
    rootLayout->addWidget(kindFrame);

    kindLayout->addWidget(new QLabel(QStringLiteral("Original message kind:")));
    auto* kindCombo=new QComboBox();
    kindCombo->addItem(QStringLiteral("Text"),static_cast<int>(ReplyMessageKind::Text));
    kindCombo->addItem(QStringLiteral("Image"),static_cast<int>(ReplyMessageKind::Image));
    kindCombo->addItem(QStringLiteral("File"),static_cast<int>(ReplyMessageKind::File));
    kindCombo->addItem(QStringLiteral("Call"),static_cast<int>(ReplyMessageKind::Call));
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

    // --- forward bar above a dummy editor, single- vs multi-message mode ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Forward bar above a dummy editor:")));

    auto* editorFrame=new QFrame(central);
    auto* editorLayout=Layout::vertical(editorFrame);
    rootLayout->addWidget(editorFrame);

    auto* forwardBar=new ForwardBar(editorFrame);
    editorLayout->addWidget(forwardBar);

    auto* dummyEditor=new QPlainTextEdit(editorFrame);
    dummyEditor->setPlaceholderText(QStringLiteral("Type a message..."));
    dummyEditor->setMaximumHeight(60);
    editorLayout->addWidget(dummyEditor);

    QObject::connect(forwardBar,&AbstractForwardBar::cancelRequested,central,[logMsg](){ logMsg(QStringLiteral("forwardBar: cancelRequested")); });
    QObject::connect(forwardBar,&AbstractForwardBar::clicked,central,[logMsg](){ logMsg(QStringLiteral("forwardBar: clicked")); });

    auto* countFrame=new QFrame(central);
    auto* countLayout=Layout::horizontal(countFrame);
    rootLayout->addWidget(countFrame);

    countLayout->addWidget(new QLabel(QStringLiteral("Messages selected for forward:")));
    auto* countSpin=new QSpinBox();
    countSpin->setRange(0,20);
    countSpin->setValue(1);
    countLayout->addWidget(countSpin,1);

    QObject::connect(
        countSpin,
        &QSpinBox::valueChanged,
        forwardBar,
        [forwardBar](int value){ forwardBar->setMessageCount(value); }
    );

    // --- messages with a "Forwarded from <author>" header + a sender-comments section ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(
        QStringLiteral("Messages with a forward header (top of the bubble) and a comments section (below the body):")
    ));

    auto* receivedBody=new ChatMessageText();
    receivedBody->loadText(QStringLiteral("Sure, here you go!"),false);
    auto* receivedHeader=new ChatMessageForwardHeader();
    auto* receivedComment=new ChatMessageComment();
    receivedComment->setComment(QStringLiteral("Thought you might find this useful."),false);
    auto* receivedMsg=makeMessage(central,AbstractChatMessage::Direction::Received,receivedBody,receivedHeader,receivedComment);
    rootLayout->addWidget(receivedMsg);

    auto* sentBody=new ChatMessageText();
    sentBody->loadText(QStringLiteral("Thanks, got it."),false);
    auto* sentHeader=new ChatMessageForwardHeader();
    auto* sentComment=new ChatMessageComment();
    sentComment->setComment(QStringLiteral("Passing this along, let me know what you think."),false);
    auto* sentMsg=makeMessage(central,AbstractChatMessage::Direction::Sent,sentBody,sentHeader,sentComment);
    rootLayout->addWidget(sentMsg);

    QObject::connect(receivedHeader,&ChatMessageForwardHeader::authorClicked,central,[logMsg](){ logMsg(QStringLiteral("received bubble header: authorClicked")); });
    QObject::connect(sentHeader,&ChatMessageForwardHeader::authorClicked,central,[logMsg](){ logMsg(QStringLiteral("sent bubble header: authorClicked")); });

    auto applyForwardData=[forwardBar,receivedHeader,sentHeader,kindCombo]()
    {
        auto kind=static_cast<ReplyMessageKind>(kindCombo->currentData().toInt());
        auto data=makeForwardData(kind);
        forwardBar->setForwardData(data);
        receivedHeader->setAuthorTitle(data.senderTitle());
        sentHeader->setAuthorTitle(data.senderTitle());
    };
    QObject::connect(kindCombo,&QComboBox::currentIndexChanged,central,[applyForwardData](int){ applyForwardData(); });

    QObject::connect(
        trimSlider,
        &QSlider::valueChanged,
        central,
        [forwardBar,trimLabel](int value)
        {
            forwardBar->setTextTrimLength(value);
            trimLabel->setText(QString::number(value));
        }
    );
    applyForwardData();

    // --- selection/sent styling + dynamic comment attach-detach ---

    auto* toolsFrame=new QFrame(central);
    auto* toolsLayout=Layout::horizontal(toolsFrame);
    rootLayout->addWidget(toolsFrame);

    auto* selectedCheck=new QCheckBox(QStringLiteral("Selected (both bubbles)"));
    toolsLayout->addWidget(selectedCheck);
    QObject::connect(selectedCheck,&QCheckBox::toggled,central,
        [receivedMsg,sentMsg](bool checked)
        {
            receivedMsg->setSelected(checked);
            sentMsg->setSelected(checked);
        }
    );

    auto* detachButton=new QPushButton(QStringLiteral("Toggle comment attached (sent bubble)"));
    toolsLayout->addWidget(detachButton);
    QObject::connect(
        detachButton,
        &QPushButton::clicked,
        sentMsg,
        [sentMsg,logMsg]()
        {
            auto content=sentMsg->content();
            if (content->comment()!=nullptr)
            {
                content->clearComment();
                logMsg(QStringLiteral("sent bubble: comment detached"));
            }
            else
            {
                // Exercises the AbstractChatMessageContent::setSelected()/setSent() "late
                // attach" fix -- a comment reattached here must immediately pick up whatever
                // [selected]/[sent] state the bubble already has, not just whatever it had at
                // construction.
                auto* comment=new ChatMessageComment();
                comment->setComment(QStringLiteral("Re-attached comment section."),false);
                content->setComment(comment);
                content->updateBubbleWidth(DemoBubbleWidth);
                logMsg(QStringLiteral("sent bubble: comment reattached"));
            }
        }
    );

    // --- full-preview "configure forward" dialog ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral(
        "Full-preview dialog -- select text in the bubble below and watch Send become \"Quote selected\":"
    )));

    auto* dialogFrame=new ModalForwardDialog();
    // See demo/replypreview/main.cpp's identical comment for why this floor is needed and why
    // it must be generous, not just non-zero -- same FrameWithModalPopup percentage-of-own-rect
    // sizing applies here.
    dialogFrame->setMinimumSize(500,750);
    rootLayout->addWidget(dialogFrame);

    auto* dialogBodyFrame=new QFrame(central);
    auto* dialogBodyLayout=Layout::horizontal(dialogBodyFrame);
    rootLayout->addWidget(dialogBodyFrame);

    dialogBodyLayout->addWidget(new QLabel(QStringLiteral("Dialog body:")));
    auto* dialogBodyCombo=new QComboBox();
    dialogBodyCombo->addItem(QStringLiteral("Text"),static_cast<int>(DemoBodyKind::Text));
    dialogBodyCombo->addItem(QStringLiteral("Files + comment"),static_cast<int>(DemoBodyKind::FilesWithComment));
    dialogBodyCombo->addItem(QStringLiteral("Images + comment"),static_cast<int>(DemoBodyKind::ImagesWithComment));
    dialogBodyCombo->addItem(QStringLiteral("Images, no comment"),static_cast<int>(DemoBodyKind::ImagesNoComment));
    dialogBodyLayout->addWidget(dialogBodyCombo,1);

    auto* dialogCountFrame=new QFrame(central);
    auto* dialogCountLayout=Layout::horizontal(dialogCountFrame);
    rootLayout->addWidget(dialogCountFrame);

    dialogCountLayout->addWidget(new QLabel(QStringLiteral("Dialog message count:")));
    auto* dialogCountSpin=new QSpinBox();
    dialogCountSpin->setRange(0,20);
    dialogCountSpin->setValue(1);
    dialogCountLayout->addWidget(dialogCountSpin,1);

    // Shared by the standalone button below AND forwardBar's own configure button -- clicking
    // either one is exactly the "user wants to configure the forward operation" gesture the task
    // brief describes, so both open the same dialog.
    auto openForwardDialog=[dialogFrame,central,logMsg,forwardBar,kindCombo,dialogBodyCombo,dialogCountSpin]()
    {
        // openDialog(true,false): create (or reuse) the dialog WITHOUT showing/measuring it yet
        // -- see demo/replypreview/main.cpp's identical comment for why the ordering below
        // (build content, THEN show) matters.
        bool isNew=dialogFrame->openDialog(true,false);
        if (isNew)
        {
            QObject::connect(
                dialogFrame->dialog(),
                &AbstractForwardDialog::actionTriggered,
                central,
                [logMsg](int id){ logMsg(QString("dialog: actionTriggered(%1)").arg(id)); }
            );
            QObject::connect(
                dialogFrame->dialog(),
                &AbstractForwardDialog::hideSenderNameChanged,
                central,
                [logMsg](bool enable){ logMsg(QString("dialog: hideSenderNameChanged(%1)").arg(enable)); }
            );
            QObject::connect(
                dialogFrame->dialog(),
                &AbstractForwardDialog::saveRequested,
                central,
                [dialogFrame,forwardBar,kindCombo,logMsg](const QString& quoted, bool hideSenderName)
                {
                    logMsg(QString("dialog: saveRequested(\"%1\", hideSenderName=%2)").arg(quoted).arg(hideSenderName));

                    auto kind=static_cast<ReplyMessageKind>(kindCombo->currentData().toInt());
                    auto data=makeForwardData(kind);
                    if (!quoted.isEmpty())
                    {
                        data.setText(quoted);
                        data.setQuote(true);
                    }
                    forwardBar->setForwardData(data);

                    dialogFrame->closePopup();
                }
            );
        }

        auto count=dialogCountSpin->value();
        dialogFrame->dialog()->setMessageCount(count);

        if (count<=1)
        {
            auto bodyKind=static_cast<DemoBodyKind>(dialogBodyCombo->currentData().toInt());
            auto* body=makeBody(bodyKind);
            auto* msg=makeMessage(dialogFrame,AbstractChatMessage::Direction::Received,body);
            dialogFrame->dialog()->setMessage(msg);
        }

        // NOW show/measure -- messageArea (or countLabel) already reflects the real content, see
        // ForwardDialog::prepareToShow(), invoked synchronously from inside popup() below.
        dialogFrame->showDialog();
    };

    QObject::connect(
        forwardBar,
        &AbstractForwardBar::configureRequested,
        central,
        [openForwardDialog,logMsg]()
        {
            logMsg(QStringLiteral("forwardBar: configureRequested"));
            openForwardDialog();
        }
    );

    auto* openDialogButton=new QPushButton(QStringLiteral("Open full preview dialog"));
    rootLayout->addWidget(openDialogButton);
    QObject::connect(openDialogButton,&QPushButton::clicked,central,openForwardDialog);

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(160);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(700,900);
    w.setWindowTitle("Message Forwarding UI Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}
