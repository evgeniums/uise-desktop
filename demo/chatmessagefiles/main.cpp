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

/** @file demo/chatmessagefiles/main.cpp
*
*  Demo application of ChatMessageFiles and ChatMessageImages.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QDateTime>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/imagelabel.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagefiles.hpp>
#include <uise/desktop/chatmessageimages.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// bubble content width negotiated in this demo -- a real host (ChatMessagesView) recomputes
// this from the viewport width on every resize (see chatmessagesview.ipp), which this
// standalone demo has no viewport to derive from
constexpr int DemoBubbleWidth=380;

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

ChatFileItem makeFileEntry(QString name, qint64 size, ChatFileTransferState state)
{
    ChatFileItem item;
    item.setFileName(std::move(name));
    item.setSize(size);
    item.setState(state);
    // Paused maps to the same Download/Upload load-control state as NotLoaded (see
    // AbstractLoadControl::State::Download's own docs) -- giving it a transferred() value too
    // is what actually shows the difference: a partially-filled progress ring instead of an
    // empty one, since there's no separate "Resume" icon to carry that information anymore.
    if (state==ChatFileTransferState::Running || state==ChatFileTransferState::Paused)
    {
        item.setTransferred(size/3);
    }
    return item;
}

ChatFileItem makeImageEntry(const QSize& pixelSize, const QColor& c1, const QColor& c2, const QString& label, ChatFileTransferState state)
{
    ChatFileItem item;
    item.setFileName(label+QStringLiteral(".png"));
    item.setMimeType(QStringLiteral("image/png"));
    item.setPixelSize(pixelSize);
    item.setSize(static_cast<qint64>(pixelSize.width())*pixelSize.height()*3);
    item.setState(state);
    if (state==ChatFileTransferState::Running || state==ChatFileTransferState::Paused)
    {
        item.setTransferred(item.size()/4);
    }
    if (state==ChatFileTransferState::Ready)
    {
        item.setPreview(makeSampleImage(pixelSize,c1,c2,label));
    }
    return item;
}

// An item whose localPath() points at an animated GIF, so ChatMessageImageItem feeds it to
// ImageLabel::setImageFile() instead of rendering the static preview() pixmap (see
// animatableLocalPath() in chatmessageimageitem.cpp). preview() is still set as the fallback
// shown if the animated decode ever fails.
ChatFileItem makeAnimatedImageEntry(const QSize& pixelSize, const QColor& c1, const QColor& c2, const QString& label, const QString& resourcePath)
{
    ChatFileItem item;
    item.setFileName(label+QStringLiteral(".gif"));
    item.setMimeType(QStringLiteral("image/gif"));
    item.setPixelSize(pixelSize);
    item.setSize(64*1024);
    item.setState(ChatFileTransferState::Ready);
    item.setPreview(makeSampleImage(pixelSize,c1,c2,label));
    item.setLocalPath(resourcePath);
    return item;
}

// Builds a real ChatMessage/ChatMessageContent bubble around `body`, so bubble-width
// negotiation is genuinely exercised rather than looking at `body` in isolation -- exactly the
// two-line sequence uichatmessage.cpp's doInit() uses (content->setChatMessage() BEFORE
// setWidgets(), so body->setChatMessage(content->chatMessage()) resolves to the real message
// rather than staying null).
// msg is typed as AbstractChatMessage*, not ChatMessage*, so that construct() resolves
// against WidgetBase::construct()'s public declaration (found by starting name lookup at
// AbstractChatMessage, which does not itself override it) rather than ChatMessage's own
// override, which is protected -- virtual dispatch still calls ChatMessage::construct() either
// way, only the compile-time access check differs.
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

    rootLayout->addWidget(new QLabel(QStringLiteral("File messages:")));

    // --- 1. incoming file message: mixed transfer states, with a comment ---

    ChatFileItems fileItems1{
        makeFileEntry(QStringLiteral("quarterly-report.pdf"),842*1024,ChatFileTransferState::Ready),
        makeFileEntry(QStringLiteral("a-very-long-descriptive-filename-that-needs-eliding-in-the-middle.docx"),1024*1024,ChatFileTransferState::NotLoaded),
        makeFileEntry(QStringLiteral("archive.zip"),12*1024*1024,ChatFileTransferState::Running),
        // State::Complete: this item's own transfer is done while archive.zip above is still
        // running -- the load control shows a check icon instead of disappearing, since the
        // whole message is not done yet (see AbstractLoadControl::State::Complete).
        makeFileEntry(QStringLiteral("sync-notes.md"),6*1024,ChatFileTransferState::Complete)
    };
    fileItems1[3].setTransferred(fileItems1[3].size());
    auto* fileBody1=new ChatMessageFiles();
    fileBody1->setItems(fileItems1);
    fileBody1->setComment(QStringLiteral("Here are the files you asked for."));
    auto* fileMsg1=makeMessage(central,AbstractChatMessage::Direction::Received,fileBody1);
    rootLayout->addWidget(fileMsg1);

    // --- 2. outgoing file message: no comment, one failed transfer, one item whose progress
    // is driven manually by the slider added further down ---

    ChatFileItems fileItems2{
        makeFileEntry(QStringLiteral("photo-original.jpg"),3*1024*1024,ChatFileTransferState::Ready),
        makeFileEntry(QStringLiteral("notes.txt"),4*1024,ChatFileTransferState::Failed),
        makeFileEntry(QStringLiteral("manual-control.bin"),8*1024*1024,ChatFileTransferState::Running),
        // Custom setMenuActions(): overrides the library's default {Open,SaveAs,Forward} menu
        // entirely. Both Pause and Resume are listed, but state()==Paused means only Resume
        // actually shows -- buildChatFileMenuItems() filters the pair by transfer state even
        // when a host lists both.
        makeFileEntry(QStringLiteral("draft-proposal.docx"),512*1024,ChatFileTransferState::Paused),
        // Cancelled sits right next to notes.txt's Failed above -- a neutral grey glyph
        // (chatFileLoadControlState() maps it through the plain LoadControl context) versus
        // Failed's red LoadControlError one, direct visual proof they're not the same icon.
        // Neither cancellable nor listing Cancel: isChatFileCancellable() is false for
        // Cancelled, so this item shows no cancel affordance of its own -- it's already done.
        makeFileEntry(QStringLiteral("cancelled-download.iso"),96*1024*1024,ChatFileTransferState::Cancelled),
        // Pending: queued in the transfer scheduler, nothing sent yet -- the hourglass
        // (State::Waiting) load control. Appended last on purpose so it does not shift the
        // indexes the setMenuActions() calls below already address. Clicking its load control
        // behaves exactly like a Running one: pauses immediately, then opens LoadControlMenu's
        // pause-or-cancel popup -- a queued transfer is just as pausable (pausing keeps the
        // scheduler from starting it) and cancellable as one already in flight.
        makeFileEntry(QStringLiteral("queued-upload.tar.gz"),24*1024*1024,ChatFileTransferState::Pending)
    };
    // Cancel is opt-in, like Pause/Resume: only an item whose menuActions() lists it gets the
    // drop-down menu entry (filtered by isChatFileCancellable()). manual-control.bin below is
    // additionally Running, so its load control's own click also offers pause-or-cancel directly
    // -- see LoadControlMenu -- without needing the drop-down menu at all. photo-original.jpg
    // (Ready) is left with the default empty menuActions() to prove an opted-out item is
    // unaffected.
    fileItems2[1].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    fileItems2[2].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Pause,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    fileItems2[3].setMenuActions({
        ChatFileMenuAction::OpenWith,
        ChatFileMenuAction::CopyFileName,
        ChatFileMenuAction::Pause,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    // Pause and Cancel both listed for the Pending item: buildChatFileMenuItems() admits Pause
    // for Running OR Pending (Resume is filtered out here, the pair being mutually exclusive by
    // state), so this row's ⋮ menu must show exactly "Pause sending" + "Cancel sending ...".
    fileItems2[5].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Pause,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    fileItems2[2].setTransferred(0);
    auto manualItemId=fileItems2[2].id();
    auto* fileBody2=new ChatMessageFiles();
    fileBody2->setItems(fileItems2);
    auto* fileMsg2=makeMessage(central,AbstractChatMessage::Direction::Sent,fileBody2);
    rootLayout->addWidget(fileMsg2);

    // slider placed right under the message it controls, not scrolled away at the bottom
    auto* manualProgressFrame=new QFrame(central);
    auto* manualProgressLayout=Layout::horizontal(manualProgressFrame);
    rootLayout->addWidget(manualProgressFrame);

    manualProgressLayout->addWidget(new QLabel(QStringLiteral("manual-control.bin progress:")));

    auto* manualSlider=new QSlider(Qt::Horizontal);
    manualSlider->setRange(0,100);
    manualSlider->setValue(0);
    manualProgressLayout->addWidget(manualSlider,1);

    auto* manualPercentLabel=new QLabel(QStringLiteral("0%"));
    manualProgressLayout->addWidget(manualPercentLabel);

    QObject::connect(
        manualSlider,
        &QSlider::valueChanged,
        fileBody2,
        [fileBody2,manualItemId,manualPercentLabel](int value)
        {
            for (const auto& it : fileBody2->items())
            {
                if (it.id()==manualItemId)
                {
                    auto updated=it;
                    updated.setTransferred(static_cast<qint64>(updated.size())*value/100);
                    fileBody2->updateItem(manualItemId,updated);
                    break;
                }
            }
            manualPercentLabel->setText(QString("%1%").arg(value));
        }
    );

    // pushes manual-control.bin straight to Cancelled -- makes the Running->Cancelled icon swap
    // directly observable, as an alternative to picking Cancel from the load control's own
    // pause-or-cancel popup (click the load control itself while it's Running to try that path)
    auto* manualCancelButton=new QPushButton(QStringLiteral("Cancel"));
    manualProgressLayout->addWidget(manualCancelButton);
    QObject::connect(
        manualCancelButton,
        &QPushButton::clicked,
        fileBody2,
        [fileBody2,manualItemId]()
        {
            for (const auto& it : fileBody2->items())
            {
                if (it.id()==manualItemId)
                {
                    auto updated=it;
                    updated.setState(ChatFileTransferState::Cancelled);
                    fileBody2->updateItem(manualItemId,updated);
                    break;
                }
            }
        }
    );

    // mirrors manualCancelButton above, but driven by the real signals rather than a dedicated
    // button, so the load control's own pause-or-cancel popup (Running/Waiting -> click it), the
    // ⋮ menu's Pause/Resume/Cancel entries, and clicking the control again while Paused all
    // actually change the item's state instead of just being logged -- exercising the whole flow
    // end to end: click load control -> Paused (progress preserved) -> Cancel picked ->
    // Cancelled, or Resume picked (⋮ menu) / control clicked again -> back to Running.
    // LoadControlMenu itself has no silent-resume path (see its own docs) -- dismissing the popup
    // without picking anything just leaves it Paused.
    //
    // Applied to whatever item the signal names, not filtered to manual-control.bin: a host that
    // only reacted for one item would leave every other row's popup/menu inert, which is exactly
    // what hid the fact that queued-upload.tar.gz (Pending) never changed state on pause.
    auto setFileItemState=[fileBody2](const QUuid& id, ChatFileTransferState state)
    {
        for (const auto& it : fileBody2->items())
        {
            if (it.id()==id)
            {
                auto updated=it;
                updated.setState(state);
                fileBody2->updateItem(id,updated);
                return;
            }
        }
    };
    QObject::connect(
        fileBody2,
        &AbstractChatMessageFiles::pauseRequested,
        fileBody2,
        [setFileItemState](const QUuid& id)
        {
            setFileItemState(id,ChatFileTransferState::Paused);
        }
    );
    // manual-control.bin and queued-upload.tar.gz both list ChatFileMenuAction::Resume alongside
    // Pause (see their setMenuActions() above) precisely so this has something to fire from --
    // buildChatFileMenuItems() shows Resume instead of Pause automatically once state() is
    // Paused, no separate wiring needed for the menu content itself, only for what picking it
    // actually does.
    QObject::connect(
        fileBody2,
        &AbstractChatMessageFiles::resumeRequested,
        fileBody2,
        [setFileItemState](const QUuid& id)
        {
            setFileItemState(id,ChatFileTransferState::Running);
        }
    );
    QObject::connect(
        fileBody2,
        &AbstractChatMessageFiles::cancelRequested,
        fileBody2,
        [setFileItemState](const QUuid& id)
        {
            setFileItemState(id,ChatFileTransferState::Cancelled);
        }
    );
    // LoadControlMenu only auto-intercepts a click while state() is Running or Waiting -- once
    // Paused, its load control shows a plain Download/Upload arrow and a click just passes
    // through as an ordinary loadControlClicked(), same as any other pass-through state (see
    // AbstractLoadControl::State::Download's own docs: click means "start or continue", the
    // host decides which). This is that decision: clicking a Paused row resumes it -- the second
    // of the two ways back from Paused, alongside picking Resume from the ⋮ menu above.
    QObject::connect(
        fileBody2,
        &AbstractChatMessageFiles::loadControlClicked,
        fileBody2,
        [fileBody2,setFileItemState](const QUuid& id)
        {
            for (const auto& it : fileBody2->items())
            {
                if (it.id()==id)
                {
                    if (it.state()==ChatFileTransferState::Paused)
                    {
                        setFileItemState(id,ChatFileTransferState::Running);
                    }
                    break;
                }
            }
        }
    );

    // --- text vertical alignment selector, applied to every file body -- the direct
    // demonstration of AbstractChatMessageFiles::setTextVerticalAlignment() being configurable
    // per view/instance, same pattern as the animation-mode selector further down ---

    auto* textAlignFrame=new QFrame(central);
    auto* textAlignLayout=Layout::horizontal(textAlignFrame);
    rootLayout->addWidget(textAlignFrame);

    textAlignLayout->addWidget(new QLabel(QStringLiteral("File name/size text alignment:")));

    // Center listed first so the combo's initial index-0 selection matches
    // ChatMessageFileItem's own actual default (Qt::AlignVCenter) without an extra call here
    auto* textAlignCombo=new QComboBox();
    textAlignCombo->addItem(QStringLiteral("Center"),static_cast<int>(Qt::AlignVCenter));
    textAlignCombo->addItem(QStringLiteral("Top"),static_cast<int>(Qt::AlignTop));
    textAlignLayout->addWidget(textAlignCombo,1);

    QObject::connect(
        textAlignCombo,
        &QComboBox::currentIndexChanged,
        central,
        [fileBody1,fileBody2,textAlignCombo](int index)
        {
            auto alignment=static_cast<Qt::Alignment>(textAlignCombo->itemData(index).toInt());
            for (auto* body : {fileBody1,fileBody2})
            {
                body->setTextVerticalAlignment(alignment);
            }
        }
    );

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Image messages (one per album template):")));

    // --- 3. single image ---

    auto* imgBody1=new ChatMessageImages();
    imgBody1->setItems({
        makeImageEntry(QSize(480,270),QColor("#4C9AFF"),QColor("#0A66C2"),QStringLiteral("1"),ChatFileTransferState::Ready)
    });
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Received,imgBody1));

    // --- 4. two images (both wide) ---

    auto* imgBody2=new ChatMessageImages();
    imgBody2->setItems({
        makeImageEntry(QSize(480,270),QColor("#FF8A65"),QColor("#D84315"),QStringLiteral("2a"),ChatFileTransferState::Ready),
        makeImageEntry(QSize(480,220),QColor("#4DB6AC"),QColor("#00695C"),QStringLiteral("2b"),ChatFileTransferState::Ready)
    });
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Sent,imgBody2));

    // --- 5. three images (one wide + two below), with a comment and one still transferring ---

    auto* imgBody3=new ChatMessageImages();
    ChatFileItems imgItems3{
        makeImageEntry(QSize(480,270),QColor("#BA68C8"),QColor("#6A1B9A"),QStringLiteral("3a"),ChatFileTransferState::Ready),
        makeImageEntry(QSize(240,320),QColor("#4FC3F7"),QColor("#0277BD"),QStringLiteral("3b"),ChatFileTransferState::Running),
        makeImageEntry(QSize(240,320),QColor("#AED581"),QColor("#558B2F"),QStringLiteral("3c"),ChatFileTransferState::Ready)
    };
    // opted into Cancel, same rationale as fileItems2 above -- this item is Running, so
    // clicking its own load control (not just the ⋮ menu) offers pause-or-cancel directly
    imgItems3[1].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Pause,
        ChatFileMenuAction::Cancel
    });
    imgBody3->setItems(imgItems3);
    imgBody3->setComment(QStringLiteral("From the trip last weekend."));
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Received,imgBody3));

    // --- 6. four images (one wide + three below) ---

    auto* imgBody4=new ChatMessageImages();
    imgBody4->setItems({
        makeImageEntry(QSize(480,240),QColor("#FFD54F"),QColor("#F57F17"),QStringLiteral("4a"),ChatFileTransferState::Ready),
        makeImageEntry(QSize(240,240),QColor("#F06292"),QColor("#AD1457"),QStringLiteral("4b"),ChatFileTransferState::Ready),
        makeImageEntry(QSize(240,240),QColor("#7986CB"),QColor("#283593"),QStringLiteral("4c"),ChatFileTransferState::NotLoaded),
        makeImageEntry(QSize(240,240),QColor("#4DD0E1"),QColor("#00838F"),QStringLiteral("4d"),ChatFileTransferState::Ready)
    });
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Sent,imgBody4));

    // --- 7. seven images -- exercises the justified-rows fallback (n>=5) ---

    ChatFileItems sevenImages;
    for (int i=0;i<7;++i)
    {
        auto hue=(i*47)%360;
        auto c1=QColor::fromHsv(hue,180,230);
        auto c2=QColor::fromHsv(hue,220,140);
        auto w=200+(i%3)*90;
        auto h=200+((i+1)%3)*90;
        sevenImages.push_back(makeImageEntry(QSize(w,h),c1,c2,QString::number(i+1),ChatFileTransferState::Ready));
    }
    auto* imgBody5=new ChatMessageImages();
    imgBody5->setItems(sevenImages);
    imgBody5->setComment(QStringLiteral("**Seven** images, justified-rows fallback."));
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Received,imgBody5));

    // --- 8. two images, one static and one animated -- exercises ImageLabel's animation path
    // and the animation-mode selector wired up below ---

    auto* imgBody6=new ChatMessageImages();
    imgBody6->setItems({
        makeImageEntry(QSize(240,240),QColor("#90A4AE"),QColor("#37474F"),QStringLiteral("8a"),ChatFileTransferState::Ready),
        makeAnimatedImageEntry(QSize(220,150),QColor("#EF9A9A"),QColor("#B71C1C"),QStringLiteral("8b"),
            QStringLiteral(":/uise/desktop/demo/chatmessagefiles/assets/animated.gif"))
    });
    imgBody6->setComment(QStringLiteral("One static tile, one animated GIF."));
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Sent,imgBody6));

    // --- 9. single cancellable image, left in a persistent Paused state (untouched by the
    // progress timer below, unlike imgBody3's own Running item) so it stays put to look at.
    // Contrast its load control's click behavior with imgBody3's: Paused isn't Running, so
    // LoadControlMenu just passes the click straight through as loadControlClicked() -- no
    // pause-or-cancel popup -- leaving "what does a click on a paused item do" entirely up to
    // the host, same as every other non-Running state. Cancel is still reachable via the ⋮ menu.
    // The tile shows the same Upload arrow a not-yet-started item would (see
    // AbstractLoadControl::State::Download's own docs), just with a partially-filled ring --
    // makeImageEntry() gives every Paused item a transferred() value now, for exactly this. ---

    auto* imgBody7=new ChatMessageImages();
    ChatFileItems imgItems7{
        makeImageEntry(QSize(360,240),QColor("#FFB74D"),QColor("#E65100"),QStringLiteral("cancel-me"),ChatFileTransferState::Paused)
    };
    imgItems7[0].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    imgBody7->setItems(imgItems7);
    imgBody7->setComment(QStringLiteral("Paused upload -- click the load control (plain click, no popup) vs. a Running one above."));
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Sent,imgBody7));

    // --- 10. single queued (Pending) image upload. Two things to look at here:
    //  * the load control shows the hourglass (State::Waiting) and, like Running, a click on it
    //    pauses and opens LoadControlMenu's pause-or-cancel popup rather than passing straight
    //    through as loadControlClicked() -- compare with imgBody7's Paused tile right above.
    //  * makeImageEntry() only attaches a preview() for Ready items, so this tile has no image
    //    content at all and renders as the empty rounded-outline placeholder
    //    (chatmessagefiles.qss's [placeholder="true"] rule), sized from PlaceholderTileExtent
    //    rather than from a real pixel size. ---

    auto* imgBody8=new ChatMessageImages();
    ChatFileItems imgItems8{
        makeImageEntry(QSize(360,240),QColor("#9575CD"),QColor("#4527A0"),QStringLiteral("queued"),ChatFileTransferState::Pending)
    };
    imgItems8[0].setMenuActions({
        ChatFileMenuAction::Open,
        ChatFileMenuAction::SaveAs,
        ChatFileMenuAction::Forward,
        ChatFileMenuAction::Pause,
        ChatFileMenuAction::Resume,
        ChatFileMenuAction::Cancel
    });
    imgBody8->setItems(imgItems8);
    imgBody8->setComment(QStringLiteral("Queued upload -- hourglass load control, placeholder outline, pause-or-cancel popup on click."));
    rootLayout->addWidget(makeMessage(central,AbstractChatMessage::Direction::Sent,imgBody8));

    // --- wire up logging for every signal on every body ---

    auto logId=[logMsg](const QString& label, const QString& signalName, const QUuid& id)
    {
        logMsg(QString("%1: %2(%3)").arg(label,signalName,id.toString(QUuid::WithoutBraces)));
    };

    auto wireFiles=[&logId](AbstractChatMessageFiles* body, const QString& label)
    {
        QObject::connect(body,&AbstractChatMessageFiles::itemClicked,body,[label,&logId](const QUuid& id){logId(label,"itemClicked",id);});
        QObject::connect(body,&AbstractChatMessageFiles::loadControlClicked,body,[label,&logId](const QUuid& id){logId(label,"loadControlClicked",id);});
        QObject::connect(body,&AbstractChatMessageFiles::openRequested,body,[label,&logId](const QUuid& id){logId(label,"openRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::openWithRequested,body,[label,&logId](const QUuid& id){logId(label,"openWithRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::saveAsRequested,body,[label,&logId](const QUuid& id){logId(label,"saveAsRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::forwardRequested,body,[label,&logId](const QUuid& id){logId(label,"forwardRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::showInFolderRequested,body,[label,&logId](const QUuid& id){logId(label,"showInFolderRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::copyFileNameRequested,body,[label,&logId](const QUuid& id){logId(label,"copyFileNameRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::pauseRequested,body,[label,&logId](const QUuid& id){logId(label,"pauseRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::resumeRequested,body,[label,&logId](const QUuid& id){logId(label,"resumeRequested",id);});
        QObject::connect(body,&AbstractChatMessageFiles::cancelRequested,body,[label,&logId](const QUuid& id){logId(label,"cancelRequested",id);});
    };
    wireFiles(fileBody1,QStringLiteral("file1"));
    wireFiles(fileBody2,QStringLiteral("file2"));

    auto wireImages=[&logId](AbstractChatMessageImages* body, const QString& label)
    {
        QObject::connect(body,&AbstractChatMessageImages::itemClicked,body,[label,&logId](const QUuid& id){logId(label,"itemClicked",id);});
        QObject::connect(body,&AbstractChatMessageImages::loadControlClicked,body,[label,&logId](const QUuid& id){logId(label,"loadControlClicked",id);});
        QObject::connect(body,&AbstractChatMessageImages::openRequested,body,[label,&logId](const QUuid& id){logId(label,"openRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::openWithRequested,body,[label,&logId](const QUuid& id){logId(label,"openWithRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::saveAsRequested,body,[label,&logId](const QUuid& id){logId(label,"saveAsRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::forwardRequested,body,[label,&logId](const QUuid& id){logId(label,"forwardRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::showInFolderRequested,body,[label,&logId](const QUuid& id){logId(label,"showInFolderRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::copyFileNameRequested,body,[label,&logId](const QUuid& id){logId(label,"copyFileNameRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::pauseRequested,body,[label,&logId](const QUuid& id){logId(label,"pauseRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::resumeRequested,body,[label,&logId](const QUuid& id){logId(label,"resumeRequested",id);});
        QObject::connect(body,&AbstractChatMessageImages::cancelRequested,body,[label,&logId](const QUuid& id){logId(label,"cancelRequested",id);});
    };
    wireImages(imgBody1,QStringLiteral("img1"));
    wireImages(imgBody2,QStringLiteral("img2"));
    wireImages(imgBody3,QStringLiteral("img3"));
    wireImages(imgBody4,QStringLiteral("img4"));
    wireImages(imgBody5,QStringLiteral("img5"));
    wireImages(imgBody6,QStringLiteral("img6"));
    wireImages(imgBody7,QStringLiteral("img7"));
    wireImages(imgBody8,QStringLiteral("img8"));

    // --- animation-mode selector, applied to every images body -- the direct demonstration of
    // AbstractChatMessageImages::setAnimationMode() being configurable per view/instance ---

    auto* animModeFrame=new QFrame(central);
    auto* animModeLayout=Layout::horizontal(animModeFrame);
    rootLayout->addWidget(animModeFrame);

    animModeLayout->addWidget(new QLabel(QStringLiteral("Image animation mode:")));

    auto* animModeCombo=new QComboBox();
    animModeCombo->addItem(QStringLiteral("Auto"),static_cast<int>(ImageLabel::AnimationMode::Auto));
    animModeCombo->addItem(QStringLiteral("Never"),static_cast<int>(ImageLabel::AnimationMode::Never));
    animModeCombo->addItem(QStringLiteral("OnHover"),static_cast<int>(ImageLabel::AnimationMode::OnHover));
    animModeCombo->addItem(QStringLiteral("Manual"),static_cast<int>(ImageLabel::AnimationMode::Manual));
    animModeLayout->addWidget(animModeCombo,1);

    QObject::connect(
        animModeCombo,
        &QComboBox::currentIndexChanged,
        central,
        [imgBody1,imgBody2,imgBody3,imgBody4,imgBody5,imgBody6,imgBody7,imgBody8,animModeCombo](int index)
        {
            auto mode=static_cast<ImageLabel::AnimationMode>(animModeCombo->itemData(index).toInt());
            for (auto* body : {imgBody1,imgBody2,imgBody3,imgBody4,imgBody5,imgBody6,imgBody7,imgBody8})
            {
                body->setAnimationMode(mode);
            }
        }
    );

    // --- a timer driving the two "Running" items to completion, so the load-control arc and
    // the "x of y" size text are visible progressing, not just a static snapshot ---

    auto runningFileId=fileItems1[2].id();
    auto runningImageId=imgBody3->items()[1].id();

    auto* progressTimer=new QTimer(&app);
    progressTimer->setInterval(400);
    QObject::connect(
        progressTimer,
        &QTimer::timeout,
        &app,
        [fileBody1,imgBody3,runningFileId,runningImageId,progressTimer]()
        {
            auto advance=[](auto* body, const QUuid& id, auto& items)
            {
                for (const auto& it : items)
                {
                    if (it.id()==id)
                    {
                        auto updated=it;
                        auto transferred=updated.transferred()+updated.size()/8;
                        if (transferred>=updated.size())
                        {
                            updated.setTransferred(updated.size());
                            updated.setState(ChatFileTransferState::Ready);
                        }
                        else
                        {
                            updated.setTransferred(transferred);
                        }
                        body->updateItem(id,updated);
                        return updated.state()==ChatFileTransferState::Ready;
                    }
                }
                return true;
            };

            auto fileDone=advance(fileBody1,runningFileId,fileBody1->items());
            auto imageDone=advance(imgBody3,runningImageId,imgBody3->items());
            if (fileDone && imageDone)
            {
                progressTimer->stop();
            }
        }
    );
    progressTimer->start();

    // --- log ---

    rootLayout->addSpacing(8);
    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    log->setMinimumHeight(200);
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(700,900);
    w.setWindowTitle("ChatMessageFiles / ChatMessageImages Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
