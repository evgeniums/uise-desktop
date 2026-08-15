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

/** @file demo/chatimageviewer/main.cpp
*
*  Demo application of ChatImageViewer: a Telegram-style overlay image viewer with a custom
*  bottom widget (ChatImageViewerControls) showing an "n of N" counter, sender/datetime, a
*  clickable album strip (ImagePreviewStrip), and a toolbar (save as / rotate / zoom / play-pause /
*  menu, the latter with a custom "Toggle Light/Dark Theme" entry appended below the built-in
*  ones). One image (Bob's first) carries animated GIF content via ChatImage::animation, seeded
*  directly since this demo has no PixmapSource.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QDateTime>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/chatimageviewer.hpp>
#include <uise/desktop/chatimageviewercontrols.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

// Synthetic gradient thumbnail, same recipe as demo/chatmessagefiles/main.cpp's own
// makeSampleImage() -- no asset files needed, so this demo stays self-contained (no .qrc).
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

// Id for the custom menu item added below via ChatImageViewerControls::addMenuItem() -- must
// not collide with ChatImageViewerControls::MenuAction's own ids (1-5).
constexpr int ToggleThemeMenuItemId=100;

// Embedded via chatimageviewerdemo.qrc, which references demo/imagelabel/assets/animated.gif by
// relative path rather than duplicating the file. Attached to exactly one image below (Bob's
// first) so both a still and an animated image are visible in the same album strip.
const char* AnimatedAsset=":/uise/desktop/demo/chatimageviewer/animated.gif";

} // anonymous namespace

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);

    // WidgetController-based widgets built directly (not via the makeWidget<>() factory
    // pathway, which is only reachable from another WidgetBase) -- initWidget() is the public
    // entry point for exactly this case, see WidgetController::initWidget()'s own docs.
    auto chatViewer=new ChatImageViewer(mainFrame);
    chatViewer->initWidget(mainFrame);
    l->addWidget(chatViewer->qWidget(),1);

    // Escape closes the viewer; here (no wrapping dialog) that just quits the demo.
    QObject::connect(
        chatViewer,
        &AbstractImageViewer::closeRequested,
        &app,
        &QApplication::quit
    );

    // Four synthetic messages: a single-image one exercises ImagePreviewStrip's own
    // count()<=1 hide rule, the rest exercise the album grouping/opacity falloff.
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
            PixmapKey key{std::string("demo-image-")+std::to_string(globalIndex)};

            chatImages.emplace_back(
                std::move(key),
                std::move(pixmap),
                msg.sender,
                dt,
                msg.messageId
            );
            if (msg.messageId=="msg-2" && i==0)
            {
                // The poster pixmap above stays as the seed/fallback -- see AbstractImageViewer::
                // imageAnimation()'s producer-first precedence, which doesn't apply to a source-
                // less demo like this one, so the seed is what's actually used.
                chatImages.back().animation=AnimationContent{QString::fromUtf8(AnimatedAsset)};
            }
            ++globalIndex;
        }
    }

    chatViewer->loadChatImages(std::move(chatImages));
    chatViewer->selectImage(size_t{0});

    // Every action the toolbar/menu can raise is signal-only (see ChatImageViewerControls'
    // class docs) -- this demo's whole job is to prove each one actually fires with the right
    // context, so just surface them as toasts rather than implementing real save/copy/etc.
    auto toast=new Toast(mainFrame);
    QObject::connect(
        chatViewer,
        &ChatImageViewer::saveAsRequested,
        toast,
        [toast](const PixmapKey&)
        {
            toast->show("Save as requested");
        }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::copyRequested,
        toast,
        [toast](const PixmapKey&)
        {
            toast->show("Copy requested");
        }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::forwardRequested,
        toast,
        [toast](const PixmapKey&)
        {
            toast->show("Forward requested");
        }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::goToMessageRequested,
        toast,
        [toast](const QString& messageId)
        {
            toast->show(QString("Go to message: %1").arg(messageId));
        }
    );
    QObject::connect(
        chatViewer,
        &ChatImageViewer::deleteMessageRequested,
        toast,
        [toast](const QString& messageId)
        {
            toast->show(QString("Delete message: %1").arg(messageId));
        }
    );

    // Custom menu item, demonstrating ChatImageViewerControls::addMenuItem()/
    // customMenuItemTriggered() -- toggles the whole application's light/dark theme, same
    // toggle recipe as demo/dropdownmenu/main.cpp's own "Toggle theme" button.
    auto* controls=chatViewer->controls();
    controls->addMenuSeparator();
    controls->addMenuItem(MenuItem(ToggleThemeMenuItemId,"Toggle Light/Dark Theme"));
    QObject::connect(
        controls,
        &ChatImageViewerControls::customMenuItemTriggered,
        &app,
        [](int id)
        {
            if (id!=ToggleThemeMenuItemId)
            {
                return;
            }
            auto mode=Style::instance().styleSheetMode();
            auto newMode=(mode==Style::StyleSheetMode::Dark)
                            ? Style::StyleSheetMode::Light
                            : Style::StyleSheetMode::Dark;
            Style::instance().setStyleSheetMode(newMode);
            Style::instance().applyStyleSheet(true);
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(1000,700);
    w.setWindowTitle("Chat Image Viewer Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
