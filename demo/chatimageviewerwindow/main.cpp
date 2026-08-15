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

/** @file demo/chatimageviewerwindow/main.cpp
*
*  Demo application of ChatImageViewerWindow: a launcher window that opens a
*  ChatImageViewer either fullscreen (default -- Esc or a click outside the controls closes it)
*  or as an ordinary window (native close button, no click-to-close), with caption and
*  destroy-on-close both configurable before opening.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QDateTime>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/chatimageviewerwindow.hpp>
#include <uise/desktop/chatimageviewer.hpp>
#include <uise/desktop/chatimageviewercontrols.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// Same synthetic-gradient recipe as demo/chatimageviewer/main.cpp's own makeSampleImage() -- no
// asset files needed, so this demo stays self-contained (no .qrc).
QPixmap makeSampleImage(const QSize& size, const QColor& c1, const QColor& c2, const QString& label)
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
    font.setPointSize(20);
    font.setBold(true);
    p.setFont(font);
    p.drawText(img.rect(),Qt::AlignCenter,label);
    p.end();
    return QPixmap::fromImage(img);
}

struct MessageSpec
{
    QString messageId;
    QString sender;
    int imageCount;
    QColor c1;
    QColor c2;
};

// Embedded via chatimageviewerwindowdemo.qrc, which references demo/imagelabel/assets/
// animated.gif by relative path rather than duplicating the file.
const char* AnimatedAsset=":/uise/desktop/demo/chatimageviewerwindow/animated.gif";

// Same four-message fixture as demo/chatimageviewer/main.cpp: a single-image message exercises
// ImagePreviewStrip's own count()<=1 hide rule, the rest exercise album grouping. Bob's first
// image also carries animated GIF content, same as demo/chatimageviewer/main.cpp's own fixture.
std::vector<ChatImageViewer::ChatImage> makeChatImages()
{
    std::vector<MessageSpec> messages=
    {
        {"msg-1","Alice",1,QColor("#4895ef"),QColor("#4361ee")},
        {"msg-2","Bob",3,QColor("#f72585"),QColor("#b5179e")},
        {"msg-3","Alice",2,QColor("#4cc9f0"),QColor("#4895ef")},
        {"msg-4","Carol",4,QColor("#f9c74f"),QColor("#f8961e")}
    };

    std::vector<ChatImageViewer::ChatImage> chatImages;
    auto baseDateTime=QDateTime::currentDateTime().addDays(-1);
    int globalIndex=0;
    for (const auto& msg : messages)
    {
        auto dt=baseDateTime.addSecs(globalIndex*180);
        for (int i=0;i<msg.imageCount;++i)
        {
            auto label=QString("%1.%2").arg(msg.messageId).arg(i+1);
            auto pixmap=makeSampleImage(QSize(640,480),msg.c1,msg.c2,label);
            PixmapKey key{std::string("demo-window-image-")+std::to_string(globalIndex)};

            chatImages.emplace_back(
                std::move(key),
                std::move(pixmap),
                msg.sender,
                dt,
                msg.messageId
            );
            if (msg.messageId=="msg-2" && i==0)
            {
                chatImages.back().animation=AnimationContent{QString::fromUtf8(AnimatedAsset)};
            }
            ++globalIndex;
        }
    }
    return chatImages;
}

} // anonymous namespace

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto* mainFrame=new QFrame();
    auto* l=Layout::vertical(mainFrame);

    auto* descriptionLabel=new QLabel(
        QStringLiteral("Opens ChatImageViewer in a top-level ChatImageViewerWindow. Fullscreen "
                       "mode (default) closes on Esc or a click anywhere outside the controls; "
                       "Window mode closes only via Esc or its native close button.")
    );
    descriptionLabel->setWordWrap(true);
    l->addWidget(descriptionLabel);

    auto* toast=new Toast(mainFrame);

    // --- colour theme selector -- same combo recipe as demo/chatmessagefiles/main.cpp and
    // demo/chatimageviewer/main.cpp's own theme toggle, just exposed as a 3-way selector
    // (including Auto) rather than a two-state toggle. Since ChatImageViewerWindow opens
    // fullscreen by default, this is the only on-screen control once the viewer is up, so it's
    // worth confirming the chosen theme before opening. ---

    auto* themeRow=new QFrame(mainFrame);
    auto* themeLayout=Layout::horizontal(themeRow);
    l->addWidget(themeRow);

    themeLayout->addWidget(new QLabel(QStringLiteral("Colour theme:")));

    auto* themeCombo=new QComboBox(themeRow);
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

    // --- launcher controls ---

    auto* controlsRow=new QFrame(mainFrame);
    auto* cl=Layout::horizontal(controlsRow);
    l->addWidget(controlsRow);

    cl->addWidget(new QLabel(QStringLiteral("Caption:")));
    auto* captionEdit=new QLineEdit(controlsRow);
    captionEdit->setPlaceholderText(QStringLiteral("(empty)"));
    cl->addWidget(captionEdit,1);

    cl->addWidget(new QLabel(QStringLiteral("Open mode:")));
    auto* openModeCombo=new QComboBox(controlsRow);
    openModeCombo->addItem(QStringLiteral("Full screen"),static_cast<int>(ChatImageViewerWindow::OpenMode::FullScreen));
    openModeCombo->addItem(QStringLiteral("Window"),static_cast<int>(ChatImageViewerWindow::OpenMode::Window));
    cl->addWidget(openModeCombo);

    auto* destroyCheck=new QCheckBox(QStringLiteral("Destroy on close"),controlsRow);
    destroyCheck->setChecked(true);
    cl->addWidget(destroyCheck);

    auto* closeOnClickCheck=new QCheckBox(QStringLiteral("Close on click (fullscreen)"),controlsRow);
    closeOnClickCheck->setChecked(true);
    cl->addWidget(closeOnClickCheck);

    cl->addStretch(1);

    auto* openButton=new QPushButton(QStringLiteral("Open viewer"),mainFrame);
    l->addWidget(openButton);
    l->addStretch(1);

    QObject::connect(
        openButton,
        &QPushButton::clicked,
        mainFrame,
        [&w,captionEdit,openModeCombo,destroyCheck,closeOnClickCheck,toast]()
        {
            auto* window=new ChatImageViewerWindow(&w);
            window->setCaption(captionEdit->text());
            window->setOpenMode(static_cast<ChatImageViewerWindow::OpenMode>(openModeCombo->currentData().toInt()));
            window->setDestroyOnClose(destroyCheck->isChecked());
            window->setCloseOnClick(closeOnClickCheck->isChecked());

            QObject::connect(
                window,
                &ChatImageViewerWindow::closed,
                toast,
                [toast]()
                {
                    toast->show("Viewer window closed");
                }
            );

            // Every toolbar/menu action is signal-only (see ChatImageViewerControls' class
            // docs) -- surface each as a toast so this demo proves the controls still work
            // unchanged when hosted inside the new top-level window, not just embedded directly.
            auto* viewer=window->viewer();
            QObject::connect(
                viewer,
                &ChatImageViewer::saveAsRequested,
                toast,
                [toast](const PixmapKey&){ toast->show("Save as requested"); }
            );
            QObject::connect(
                viewer,
                &ChatImageViewer::copyRequested,
                toast,
                [toast](const PixmapKey&){ toast->show("Copy requested"); }
            );
            QObject::connect(
                viewer,
                &ChatImageViewer::forwardRequested,
                toast,
                [toast](const PixmapKey&){ toast->show("Forward requested"); }
            );
            QObject::connect(
                viewer,
                &ChatImageViewer::goToMessageRequested,
                toast,
                [toast](const QString& messageId){ toast->show(QString("Go to message: %1").arg(messageId)); }
            );
            QObject::connect(
                viewer,
                &ChatImageViewer::deleteMessageRequested,
                toast,
                [toast](const QString& messageId){ toast->show(QString("Delete message: %1").arg(messageId)); }
            );

            viewer->loadChatImages(makeChatImages());
            viewer->selectImage(size_t{0});

            window->popup();
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(700,300);
    w.setWindowTitle("Chat Image Viewer Window Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
