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

/** @file uise/desktop/demo/chatobjectbrowser/main.cpp
*
*  Demo application of the chat object browsers: the thumbnail grid of images and the lists of
*  files, voice messages, links and mentions in a ChatObjectBrowserPanel, fed by a fake owner that
*  answers the browsers' keyset requests from generated data.
*
*  What to try:
*   - scroll down (requests "before" the oldest loaded object) and, after "Open at middle", up
*     (requests "after" the newest loaded one);
*   - the object count 0 (the empty state) and 5000 (a long list);
*   - the answer delay of 2000 ms (the log must show one request per side at a time);
*   - the thumbnail delay (placeholders first, thumbnails later) in the images;
*   - the months: headings above the first object of a month (a heading is a row of its own in
*     the grid), "Group by month" switches them off and on; after "Open at the middle" scroll up
*     through the month you landed in -- the lines of the grid are cut anew, the place stays;
*   - the search line (the owner filters by the text in the request; try "alice", "12");
*   - play a voice message (the fake player drives the row);
*   - the "video" kind, registered here and not in the library.
*
*/

/****************************************************************************/

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/chataudiofileitems.hpp>
#include <uise/desktop/chatobjectbrowserpanel.hpp>
#include <uise/desktop/chatobjectlistbrowser.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

namespace {

//! Images come in albums of this many objects per message, so that the ordinal tie-break of the
//! keys is exercised; the other kinds have one object per message.
constexpr size_t ImagesPerMessage=3;

size_t perMessage(const QString& kind)
{
    return kind==ChatObjectKind::Image ? ImagesPerMessage : 1;
}

//! Key of the n-th oldest object of the kind: zero padded decimal, so that bytewise order is
//! numeric order.
ChatObjectKey keyOf(const QString& kind, size_t n)
{
    const auto per=perMessage(kind);
    ChatObjectKey key;
    key.sortId=QByteArray::number(static_cast<qulonglong>(n/per)).rightJustified(8,'0');
    key.ordinal=static_cast<quint32>(n%per);
    return key;
}

size_t indexOfKey(const QString& kind, const ChatObjectKey& key)
{
    return static_cast<size_t>(key.sortId.toULongLong())*perMessage(kind)+key.ordinal;
}

//--------------------------------------------------------------------------

//! Thumbnails drawn on demand, after a delay, so that the placeholders can be seen.
class DemoThumbnailSource : public RoundedImageSource
{
    public:

        void setDelayMs(int value) noexcept
        {
            m_delayMs=value;
        }

    protected:

        void doLoadPixmap(const PixmapKey& key) override
        {
            // Always deferred, even for 0 ms: a source that answers from inside doLoadPixmap()
            // answers before the consumer has connected.
            std::weak_ptr<PixmapSource> weak=weak_from_this();
            QTimer::singleShot(m_delayMs,qApp,
                [weak,key]()
                {
                    if (auto source=weak.lock())
                    {
                        static_cast<DemoThumbnailSource*>(source.get())->deliver(key);
                    }
                }
            );
        }

    private:

        void deliver(const PixmapKey& key)
        {
            const auto size=key.size();
            if (!size.isValid() || key.path().empty())
            {
                return;
            }

            const auto n=std::stoull(key.path().front());

            QPixmap pixmap(size);
            pixmap.fill(QColor::fromHsv(static_cast<int>((n*37)%360),140,200));

            QPainter painter(&pixmap);
            painter.setPen(Qt::white);
            QFont font=painter.font();
            font.setPixelSize(std::max(10,size.height()/4));
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(pixmap.rect(),Qt::AlignCenter,QString::number(n));
            painter.end();

            updatePixmap(key,pixmap);
        }

        int m_delayMs=0;
};

struct DemoAvatarBuilder
{
    std::shared_ptr<Avatar> operator() (const WithPath& path) const
    {
        auto avatar=std::make_shared<Avatar>();
        if (!path.path().empty())
        {
            avatar->setAvatarName(path.path().front());
        }
        return avatar;
    }
};

//--------------------------------------------------------------------------

//! The fake owner of the data: generates the objects of every kind from their index and answers
//! the keyset requests the way a real owner has to.
struct DemoOwner
{
    size_t count=500;
    std::shared_ptr<DemoThumbnailSource> thumbnails;
    std::shared_ptr<AvatarSource> avatars;
    std::vector<QString> senders{QStringLiteral("Alice"),QStringLiteral("Bob"),QStringLiteral("Carol"),QStringLiteral("Dan"),QStringLiteral("Erin")};

    ChatObjectItem makeItem(const QString& kind, size_t n) const
    {
        static const QDateTime base=QDateTime::currentDateTime();

        ChatObjectItem item;
        item.kind=kind;
        item.key=keyOf(kind,n);
        item.messageId=QStringLiteral("msg-%1").arg(n/perMessage(kind));
        item.dateTime=base.addSecs(-static_cast<qint64>(count-1-n)*28800);
        item.incoming=(n%3)!=0;
        item.senderName=senders[n%senders.size()];
        item.file.setFileId(QString::number(n));
        item.file.setState(ChatFileTransferState::Ready);

        if (kind==ChatObjectKind::Image)
        {
            item.thumbnailPath=WithPath(std::to_string(n));
            item.thumbnailSource=thumbnails;
            item.file.setFileName(QStringLiteral("IMG_%1.jpg").arg(n));
            item.file.setMimeType(QStringLiteral("image/jpeg"));
        }
        else if (kind==ChatObjectKind::File)
        {
            static const char* extensions[]={"pdf","zip","txt","docx","xlsx"};
            item.file.setFileName(QStringLiteral("document-%1.%2").arg(n).arg(QLatin1String(extensions[n%5])));
            item.file.setSize(static_cast<qint64>((n*7919)%5000+1)*1024);
        }
        else if (kind==ChatObjectKind::Voice)
        {
            QByteArray waveform(100,0);
            for (int i=0; i<waveform.size(); ++i)
            {
                waveform[i]=static_cast<char>((n*31+static_cast<size_t>(i)*17)%256);
            }
            item.file.setFileName(QStringLiteral("voice-%1.ogg").arg(n));
            item.file.setVoice(true);
            item.file.setVoiceWaveform(waveform);
            item.file.setVoiceDurationMs(static_cast<quint32>(3000+(n*1731)%90000));
            item.file.setSize(static_cast<qint64>((n*977)%400+20)*1024);
            item.file.setListenedByPeer(n%2==0);
        }
        else if (kind==ChatObjectKind::Link)
        {
            item.url=QStringLiteral("https://example.com/articles/%1").arg(n);
            if (n%3!=0)
            {
                item.title=QStringLiteral("Article number %1: a rather long title that has to be elided somewhere on the way").arg(n);
            }
            item.text=QStringLiteral("Look at this, it is really interesting: %1 and some more words after the link").arg(item.url);
        }
        else if (kind==ChatObjectKind::Mention)
        {
            item.avatarSource=avatars;
            item.senderAvatarPath=WithPath(item.senderName.toStdString());
            item.text=QStringLiteral("@me please have a look at number %1, it is long enough to be elided at the end of the line").arg(n);
        }

        return item;
    }

    //! What the demo searches in: the sender and the number.
    QString labelOf(size_t n) const
    {
        return QStringLiteral("%1 %2").arg(senders[n%senders.size()]).arg(n);
    }

    //! Indices of the objects that match the request's search text, ascending.
    std::vector<size_t> matching(const ChatObjectsRequest& request) const
    {
        std::vector<size_t> ids;
        ids.reserve(count);
        for (size_t n=0; n<count; ++n)
        {
            if (request.query.isEmpty() || labelOf(n).contains(request.query,Qt::CaseInsensitive))
            {
                ids.push_back(n);
            }
        }
        return ids;
    }

    ChatObjectsPage answer(const ChatObjectsRequest& request) const
    {
        ChatObjectsPage page;
        page.requestId=request.requestId;

        // the keyset order is over the objects that match; the end flags are about them
        const auto ids=matching(request);
        const size_t total=ids.size();

        if (total==0)
        {
            page.endBefore=true;
            page.endAfter=true;
            return page;
        }

        if (request.anchor.isNull())
        {
            // the newest end
            const size_t first=total>request.countBefore ? total-request.countBefore : 0;
            for (size_t i=first; i<total; ++i)
            {
                page.items.push_back(makeItem(request.kind,ids[i]));
            }
            page.endBefore=(first==0);
            page.endAfter=true;
            return page;
        }

        // the anchor may not match (the search text changed, the data became shorter): its place
        // among the matching objects is what counts
        const size_t anchorIdx=indexOfKey(request.kind,request.anchor);
        const size_t pos=static_cast<size_t>(std::lower_bound(ids.begin(),ids.end(),anchorIdx)-ids.begin());
        const bool exact=pos<total && ids[pos]==anchorIdx;

        const size_t lo=pos>request.countBefore ? pos-request.countBefore : 0;
        const size_t afterStart=exact ? pos+1 : pos;
        const size_t hi=std::min(total,afterStart+request.countAfter);

        for (size_t i=lo; i<pos; ++i)
        {
            page.items.push_back(makeItem(request.kind,ids[i]));
        }
        if (request.anchorInclusive && exact)
        {
            page.items.push_back(makeItem(request.kind,ids[pos]));
        }
        for (size_t i=afterStart; i<hi; ++i)
        {
            page.items.push_back(makeItem(request.kind,ids[i]));
        }

        page.endBefore=(request.countBefore>0 && lo==0);
        page.endAfter=(request.countAfter>0 && hi==total);
        return page;
    }
};

//--------------------------------------------------------------------------

//! A row of a kind that exists only in this demo, to show that a kind is registered, not wired in.
class DemoVideoRow : public QFrame
{
    public:

        DemoVideoRow(const ChatObjectItem& item, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_item(item),
              m_hooks(hooks)
        {
            auto* layout=Layout::horizontal(this,false);
            layout->addWidget(
                new QLabel(
                    QStringLiteral("Video clip of %1: a kind registered by the demo, not by the library").arg(item.messageId),
                    this
                )
            );
            setCursor(Qt::PointingHandCursor);
        }

    protected:

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            QFrame::mouseReleaseEvent(event);
            if (m_hooks.activated)
            {
                const ChatObjectItem copy=m_item;
                m_hooks.activated(copy);
            }
        }

    private:

        ChatObjectItem m_item;
        ChatObjectRowHooks m_hooks;
};

//! State of the fake voice player.
struct DemoPlayer
{
    ChatObjectItem item;
    qint64 positionMs=0;
    bool active=false;
};

} // anonymous namespace

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    DemoOwner owner;
    owner.thumbnails=std::make_shared<DemoThumbnailSource>();
    owner.avatars=std::make_shared<AvatarSource>();
    owner.avatars->setAvatarBuilder(DemoAvatarBuilder{});

    QMainWindow w;
    w.setWindowTitle("Chat Object Browsers Demo");

    auto* root=new QFrame(&w);
    root->setObjectName("demoRoot");
    auto* rootLayout=Layout::horizontal(root,false);

    // --- the panel with the stock kinds plus one of the demo's own ---

    auto* panel=new ChatObjectBrowserPanel(root);
    panel->setMinimumSize(420,520);

    auto registry=ChatObjectBrowserRegistry::defaults();
    registry.registerKind(
        QStringLiteral("video"),
        ChatObjectBrowserKindInfo{
            []()
            {
                return QStringLiteral("Videos");
            },
            []()
            {
                return QStringLiteral("No videos in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                ChatObjectRowFactory factory;
                // no updater on purpose: updateItem() then replaces the row's widget
                factory.build=[](const ChatObjectItem& item, QWidget* rowParent, const ChatObjectRowHooks& hooks) -> QFrame*
                {
                    return new DemoVideoRow(item,hooks,rowParent);
                };
                return new ChatObjectListBrowser(QStringLiteral("video"),std::move(factory),parent);
            }
        }
    );
    const auto kinds=registry.kinds();
    panel->setRegistry(std::move(registry));
    rootLayout->addWidget(panel,1);

    // --- controls and the log ---

    auto* side=new QFrame(root);
    side->setFixedWidth(340);
    auto* sideLayout=Layout::vertical(side,false);
    rootLayout->addWidget(side);

    auto* log=new QPlainTextEdit(side);
    log->setReadOnly(true);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    auto* kindBox=new QComboBox(side);
    for (const auto& kind : kinds)
    {
        kindBox->addItem(kind,kind);
    }
    sideLayout->addWidget(new QLabel("Kind of objects",side));
    sideLayout->addWidget(kindBox);

    auto* countBox=new QSpinBox(side);
    countBox->setRange(0,20000);
    countBox->setValue(static_cast<int>(owner.count));
    countBox->setKeyboardTracking(false);
    sideLayout->addWidget(new QLabel("Objects in the chat (0 shows the empty state)",side));
    sideLayout->addWidget(countBox);

    auto* presets=new QFrame(side);
    auto* presetsLayout=Layout::horizontal(presets,false);
    for (int preset : {0,5,40,500,5000})
    {
        auto* button=new QPushButton(QString::number(preset),presets);
        QObject::connect(button,&QPushButton::clicked,countBox,
            [countBox,preset]()
            {
                countBox->setValue(preset);
            }
        );
        presetsLayout->addWidget(button);
    }
    sideLayout->addWidget(presets);

    auto* delayBox=new QComboBox(side);
    delayBox->addItem("Answer at once",0);
    delayBox->addItem("Answer in 300 ms",300);
    delayBox->addItem("Answer in 2000 ms",2000);
    delayBox->setCurrentIndex(1);
    sideLayout->addWidget(delayBox);

    auto* thumbBox=new QComboBox(side);
    thumbBox->addItem("Thumbnails at once",0);
    thumbBox->addItem("Thumbnails in 500 ms",500);
    thumbBox->addItem("Thumbnails in 3000 ms",3000);
    thumbBox->setCurrentIndex(1);
    sideLayout->addWidget(thumbBox);

    auto* reloadButton=new QPushButton("Reload (from the newest)",side);
    auto* middleButton=new QPushButton("Open at the middle",side);
    auto* deleteButton=new QPushButton("Delete a message (patches the browser)",side);
    auto* groupBox=new QCheckBox("Group by month",side);
    groupBox->setChecked(true);
    auto* searchBox=new QCheckBox("Show the search line (matches the sender and the number)",side);
    auto* darkBox=new QCheckBox("Dark theme",side);
    sideLayout->addWidget(reloadButton);
    sideLayout->addWidget(middleButton);
    sideLayout->addWidget(deleteButton);
    sideLayout->addWidget(groupBox);
    sideLayout->addWidget(searchBox);
    sideLayout->addWidget(darkBox);
    sideLayout->addWidget(log,1);

    // --- the fake voice player ---

    DemoPlayer player;
    auto* playTimer=new QTimer(&w);
    playTimer->setInterval(100);

    auto voiceRow=[panel,&player]() -> ChatVoiceFileItem*
    {
        auto* browser=qobject_cast<ChatObjectListBrowser*>(panel->currentBrowser());
        if (browser==nullptr)
        {
            return nullptr;
        }
        return qobject_cast<ChatVoiceFileItem*>(browser->rowWidget(player.item.key));
    };

    auto showProgress=[&player,voiceRow]()
    {
        auto* row=voiceRow();
        if (row==nullptr)
        {
            return;
        }
        const qint64 duration=player.item.file.voiceDurationMs();
        row->setPlaybackProgress(duration>0 ? static_cast<qreal>(player.positionMs)/static_cast<qreal>(duration) : 0.0);
        row->setPlaybackPosition(player.positionMs,duration);
    };

    auto stopPlayer=[&player,playTimer,panel,voiceRow]()
    {
        if (!player.active)
        {
            return;
        }
        player.active=false;
        playTimer->stop();
        player.item.file.setPlaying(false);
        if (auto* browser=panel->currentBrowser())
        {
            browser->updateItem(player.item);
        }
        if (auto* row=voiceRow())
        {
            row->setPlaybackProgress(0.0);
            row->setPlaybackPosition(-1,player.item.file.voiceDurationMs());
        }
    };

    QObject::connect(playTimer,&QTimer::timeout,&w,
        [&player,showProgress,stopPlayer]()
        {
            player.positionMs+=100;
            if (player.positionMs>=static_cast<qint64>(player.item.file.voiceDurationMs()))
            {
                stopPlayer();
                return;
            }
            showProgress();
        }
    );

    // --- the owner's side of the requests ---

    panel->setRequestCb(
        [&owner,panel,delayBox,logMsg](const ChatObjectsRequest& request)
        {
            logMsg(
                QStringLiteral("request #%1 %2: anchor=%3%4 before=%5 after=%6 query='%7'")
                    .arg(request.requestId)
                    .arg(request.kind)
                    .arg(request.anchor.isNull() ? QStringLiteral("(newest end)") : QString::fromLatin1(request.anchor.sortId)+QLatin1Char('.')+QString::number(request.anchor.ordinal))
                    .arg(request.anchorInclusive ? QStringLiteral(" inclusive") : QString{})
                    .arg(request.countBefore)
                    .arg(request.countAfter)
                    .arg(request.query)
            );

            QPointer<ChatObjectBrowserPanel> guard(panel);
            QTimer::singleShot(delayBox->currentData().toInt(),panel,
                [&owner,guard,request,logMsg]()
                {
                    if (guard.isNull() || guard->currentBrowser()==nullptr)
                    {
                        return;
                    }
                    const auto page=owner.answer(request);
                    logMsg(
                        QStringLiteral("  answer #%1: %2 items, endBefore=%3 endAfter=%4")
                            .arg(page.requestId)
                            .arg(page.items.size())
                            .arg(page.endBefore ? QStringLiteral("yes") : QStringLiteral("no"))
                            .arg(page.endAfter ? QStringLiteral("yes") : QStringLiteral("no"))
                    );
                    // an answer to a browser that is gone is dropped by the id
                    guard->currentBrowser()->applyPage(page);
                }
            );
        }
    );

    panel->setContextMenuBuilder(
        [](const ChatObjectItem&)
        {
            std::vector<MenuItem> items;
            items.emplace_back(AbstractChatObjectBrowser::MenuFirstCustomId,QStringLiteral("Copy object id (the demo's own entry)"));
            return items;
        }
    );

    // --- what the browsers say ---

    QObject::connect(panel,&ChatObjectBrowserPanel::itemActivated,&w,
        [logMsg](const ChatObjectItem& item)
        {
            logMsg(QStringLiteral("activated: %1 %2 (%3)").arg(item.kind,item.uid(),item.messageId));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::goToMessageRequested,&w,
        [logMsg](const QString& messageId)
        {
            logMsg(QStringLiteral("go to message: %1").arg(messageId));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::contextMenuTriggered,&w,
        [logMsg](const ChatObjectItem& item, int id)
        {
            logMsg(QStringLiteral("menu entry %1 for %2").arg(id).arg(item.uid()));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::backRequested,&w,
        [logMsg]()
        {
            logMsg(QStringLiteral("back requested (a real owner leaves the browser here)"));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::itemActionRequested,&w,
        [&player,playTimer,panel,logMsg,stopPlayer,showProgress](const ChatObjectItem& item, const QString& action, const QVariant& argument)
        {
            logMsg(QStringLiteral("action %1 for %2").arg(action,item.uid()));

            if (action==ChatObjectAction::Play)
            {
                stopPlayer();
                player.item=item;
                player.item.file.setPlaying(true);
                player.positionMs=0;
                player.active=true;
                if (auto* browser=panel->currentBrowser())
                {
                    browser->updateItem(player.item);
                }
                playTimer->start();
            }
            else if (action==ChatObjectAction::Stop)
            {
                stopPlayer();
            }
            else if (action==ChatObjectAction::Seek)
            {
                if (player.active && item.key==player.item.key)
                {
                    player.positionMs=static_cast<qint64>(argument.toDouble()*player.item.file.voiceDurationMs());
                    showProgress();
                }
            }
        }
    );

    // --- the controls ---

    auto openKind=[&owner,panel,kindBox,thumbBox,groupBox,searchBox,stopPlayer,logMsg](const ChatObjectKey& anchor)
    {
        stopPlayer();
        owner.thumbnails->setDelayMs(thumbBox->currentData().toInt());
        logMsg(QStringLiteral("--- open %1 ---").arg(kindBox->currentData().toString()));
        panel->open(kindBox->currentData().toString(),anchor);
        // the options are the demo's, a new browser starts with the defaults
        if (auto* browser=panel->currentBrowser())
        {
            browser->setGroupByMonth(groupBox->isChecked());
            browser->setSearchVisible(searchBox->isChecked());
        }
    };

    QObject::connect(kindBox,&QComboBox::currentIndexChanged,&w,
        [openKind](int)
        {
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(countBox,&QSpinBox::valueChanged,&w,
        [&owner,openKind](int value)
        {
            owner.count=static_cast<size_t>(value);
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(reloadButton,&QPushButton::clicked,&w,
        [openKind]()
        {
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(middleButton,&QPushButton::clicked,&w,
        [&owner,kindBox,openKind]()
        {
            if (owner.count==0)
            {
                return;
            }
            openKind(keyOf(kindBox->currentData().toString(),owner.count/2));
        }
    );
    QObject::connect(deleteButton,&QPushButton::clicked,&w,
        [panel,logMsg]()
        {
            auto* browser=panel->currentBrowser();
            if (browser==nullptr || browser->model().empty())
            {
                return;
            }
            const auto& items=browser->model().items();
            const auto messageId=items[items.size()/2].messageId;
            logMsg(QStringLiteral("removing the objects of %1 from the browser").arg(messageId));
            browser->removeMessageObjects(messageId);
        }
    );
    QObject::connect(groupBox,&QCheckBox::toggled,&w,
        [panel](bool enable)
        {
            if (auto* browser=panel->currentBrowser())
            {
                browser->setGroupByMonth(enable);
            }
        }
    );
    QObject::connect(searchBox,&QCheckBox::toggled,&w,
        [panel](bool enable)
        {
            if (auto* browser=panel->currentBrowser())
            {
                browser->setSearchVisible(enable);
                if (enable)
                {
                    browser->focusSearch();
                }
            }
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::searchTextChanged,&w,
        [logMsg](const QString& text)
        {
            logMsg(QStringLiteral("search text in effect: '%1'").arg(text));
        }
    );
    QObject::connect(darkBox,&QCheckBox::toggled,&w,
        [](bool dark)
        {
            Style::instance().setBaseQss(
                dark ? QStringLiteral("QMainWindow, QFrame#demoRoot { background-color: #1E1E1E; }")
                     : QStringLiteral("QMainWindow, QFrame#demoRoot { background-color: #FFFFFF; }")
            );
            Style::instance().setColorTheme(dark ? Style::DarkTheme : Style::LightTheme);
            Style::instance().applyStyleSheet(true);
        }
    );

    w.setCentralWidget(root);
    w.resize(900,720);
    w.show();

    // the first kind of the registry is open from the start
    openKind(ChatObjectKey{});

    return app.exec();
}
