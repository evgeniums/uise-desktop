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

/** @file demo/chatimageviewerflyweight/main.cpp
*
*  Demo application of the flyweight ChatImageViewer: a synthetic 200-image/~60-message chat
*  served from a fake backend, starting mid-chat, prefetched in both directions as the user pages
*  -- exercising the windowed model, the version-ladder loading overlay, sliding "n of N"
*  numbering, and both ImagePreviewStrip scopes (Album/Continuous). No real network or files2
*  backend involved: DemoFlyweightSource stands in for a version-ladder PixmapSource, and
*  DemoChatFetcher (both in demoflyweightbackend.hpp) stands in for a paged chat-history fetch,
*  reacting to ChatImageViewer::imagesRequested() (inherited from AbstractImageViewer).
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>
#include <QComboBox>
#include <QLabel>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/chatimageviewer.hpp>
#include <uise/desktop/chatimageviewercontrols.hpp>

#include "demoflyweightbackend.hpp"

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);

    auto dataset=buildDataset();
    auto totalCount=static_cast<qint64>(dataset.size());

    auto source=std::make_shared<DemoFlyweightSource>(dataset);

    auto chatViewer=new ChatImageViewer(mainFrame);
    chatViewer->initWidget(mainFrame);
    l->addWidget(chatViewer->qWidget(),1);
    chatViewer->setImageSource(source);

    // Snappier than the 10s default so "fail the next fetch" + paging to the loaded edge recovers
    // within a few seconds instead of ten, for a quicker manual check.
    chatViewer->setPendingNavTimeoutMs(3000);

    QObject::connect(
        chatViewer,
        &AbstractImageViewer::closeRequested,
        &app,
        &QApplication::quit
    );

    // Start mid-chat -- images 90..109 of the 200 -- with more on both sides, exactly the
    // scenario a "jump to this message" chat integration would hand the viewer.
    constexpr int windowStart=90;
    constexpr int windowLen=20;
    std::vector<ChatImageViewer::ChatImage> initialImages;
    initialImages.reserve(windowLen);
    for (int i=windowStart; i<windowStart+windowLen; ++i)
    {
        const auto& rec=dataset[static_cast<size_t>(i)];
        initialImages.emplace_back(keyForIndex(i),QPixmap{},rec.sender,rec.dateTime,rec.messageId);
    }
    chatViewer->loadChatImages(std::move(initialImages),true,true,windowStart,totalCount);
    chatViewer->selectImage(size_t{0});

    auto* fetcher=new DemoChatFetcher(chatViewer,dataset,mainFrame);
    QObject::connect(
        chatViewer,
        &AbstractImageViewer::imagesRequested,
        fetcher,
        &DemoChatFetcher::onImagesRequested
    );

    // --- toolbar/menu signals: surfaced as toasts, same as demo/chatimageviewer, except
    //     deleteMessageRequested actually removes the album so the built-in menu action is fully
    //     exercisable end to end, not just observed. ---

    auto toast=new Toast(mainFrame);
    QObject::connect(
        chatViewer,
        &ChatImageViewer::saveAsRequested,
        toast,
        [toast](const PixmapKey&){ toast->show("Save as requested"); }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::copyRequested,
        toast,
        [toast](const PixmapKey&){ toast->show("Copy requested"); }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::forwardRequested,
        toast,
        [toast](const PixmapKey&){ toast->show("Forward requested"); }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::goToMessageRequested,
        toast,
        [toast](const QString& messageId){ toast->show(QString("Go to message: %1").arg(messageId)); }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::deleteMessageRequested,
        toast,
        [toast,chatViewer](const QString& messageId)
        {
            auto removed=chatViewer->removeImagesForMessage(messageId);
            toast->show(QString("Deleted message %1 (%2 image(s) removed)").arg(messageId).arg(removed));
        }
    );

    // --- colour theme selector -- same combo recipe as demo/chatimageviewerwindow/main.cpp's own
    //     3-way selector (Auto/Light/Dark), reused verbatim so both demos behave identically. ---

    auto themeRow=new QFrame(mainFrame);
    auto themeLayout=Layout::horizontal(themeRow);
    l->addWidget(themeRow);

    themeLayout->addWidget(new QLabel(QStringLiteral("Colour theme:"),themeRow));

    auto themeCombo=new QComboBox(themeRow);
    themeCombo->addItem(QStringLiteral("Auto"),static_cast<int>(Style::StyleSheetMode::Auto));
    themeCombo->addItem(QStringLiteral("Light"),static_cast<int>(Style::StyleSheetMode::Light));
    themeCombo->addItem(QStringLiteral("Dark"),static_cast<int>(Style::StyleSheetMode::Dark));
    themeCombo->setCurrentIndex(themeCombo->findData(static_cast<int>(Style::instance().styleSheetMode())));
    themeLayout->addWidget(themeCombo,1);

    QObject::connect(
        themeCombo,
        &QComboBox::currentIndexChanged,
        mainFrame,
        [themeCombo]()
        {
            auto mode=static_cast<Style::StyleSheetMode>(themeCombo->currentData().toInt());
            Style::instance().setStyleSheetMode(mode);
            // reload=true: re-resolves every already-created SvgIcon (prev/next arrows, toolbar
            // icons) against the new theme's colour maps, not just the QSS.
            Style::instance().applyStyleSheet(true);
        }
    );

    // --- bottom control bar: latency, fail-next-fetch, strip scope, delete-current-message ---

    auto controlsBar=new QFrame(mainFrame);
    auto cbl=Layout::horizontal(controlsBar);

    cbl->addWidget(new QLabel("Rung latency:",controlsBar));
    auto latencyCombo=new QComboBox(controlsBar);
    latencyCombo->addItem("None (0 ms)",0);
    latencyCombo->addItem("Normal (~700 ms)",700);
    latencyCombo->addItem("Slow (~3000 ms)",3000);
    latencyCombo->setCurrentIndex(1);
    cbl->addWidget(latencyCombo);
    QObject::connect(
        latencyCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        latencyCombo,
        [source,latencyCombo](int)
        {
            // source (DemoFlyweightSource) is a PixmapSource, not a QObject, so it cannot itself
            // be a connect() context -- latencyCombo doubles as the context here purely for
            // correct lifetime/thread affinity, unrelated to source's own lifetime, which the
            // captured shared_ptr already keeps alive independently.
            source->setLatencyMs(latencyCombo->currentData().toInt());
        }
    );

    auto failNextCheck=new CheckBox("Fail next fetch",controlsBar);
    cbl->addWidget(failNextCheck);
    QObject::connect(
        failNextCheck,
        &CheckBox::toggled,
        fetcher,
        [fetcher](bool checked){ fetcher->setFailNextFetch(checked); }
    );

    cbl->addWidget(new QLabel("Strip scope:",controlsBar));
    auto scopeCombo=new QComboBox(controlsBar);
    scopeCombo->addItem("Album",static_cast<int>(ChatImageViewer::StripScope::Album));
    scopeCombo->addItem("Continuous",static_cast<int>(ChatImageViewer::StripScope::Continuous));
    cbl->addWidget(scopeCombo);
    QObject::connect(
        scopeCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        chatViewer,
        [chatViewer,scopeCombo](int)
        {
            auto scope=static_cast<ChatImageViewer::StripScope>(scopeCombo->currentData().toInt());
            chatViewer->setStripScope(scope);
        }
    );

    auto deleteCurrentButton=new PushButton(controlsBar);
    deleteCurrentButton->setText("Delete current message");
    cbl->addWidget(deleteCurrentButton);
    QObject::connect(
        deleteCurrentButton,
        &PushButton::clicked,
        chatViewer,
        [chatViewer,dataset,toast]()
        {
            auto idx=indexFromKey(chatViewer->currentImageKey());
            if (idx<0 || static_cast<size_t>(idx)>=dataset.size())
            {
                return;
            }
            auto messageId=dataset[static_cast<size_t>(idx)].messageId;
            auto removed=chatViewer->removeImagesForMessage(messageId);
            toast->show(QString("Deleted message %1 (%2 image(s) removed)").arg(messageId).arg(removed));
        }
    );

    cbl->addStretch(1);

    auto statusLabel=new QLabel(controlsBar);
    statusLabel->setObjectName("statusLabel");
    cbl->addWidget(statusLabel);

    auto updateStatus=[chatViewer,statusLabel]()
    {
        statusLabel->setText(
            QString("window=%1 hasBefore=%2 hasAfter=%3 pending=%4")
                .arg(chatViewer->imageCount())
                .arg(chatViewer->hasMoreBefore())
                .arg(chatViewer->hasMoreAfter())
                .arg(chatViewer->isNavigationPending())
        );
    };
    QObject::connect(chatViewer,&AbstractImageViewer::windowChanged,statusLabel,updateStatus);
    QObject::connect(chatViewer,&AbstractImageViewer::currentImagePositionChanged,statusLabel,updateStatus);
    updateStatus();

    l->addWidget(controlsBar);

    w.setCentralWidget(mainFrame);
    w.resize(1050,760);
    w.setWindowTitle("Chat Image Viewer Demo (Flyweight)");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
