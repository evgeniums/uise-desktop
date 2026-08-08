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

/** @file demo/filedropoverlay/main.cpp
*
*  Demo application of FileDropOverlay.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QImage>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/filedropoverlay.hpp>
#include <uise/desktop/fileuploaddialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

namespace {

QString panelName(FileDropOverlay::Panel panel)
{
    switch (panel)
    {
        case (FileDropOverlay::Panel::Documents): return QStringLiteral("Documents");
        case (FileDropOverlay::Panel::Images): return QStringLiteral("Images");
        case (FileDropOverlay::Panel::None): break;
    }
    return QStringLiteral("None");
}

// Real files on disk, not just fake paths -- mimeDataHasImages()/addFromMimeData() both filter
// dropped URLs down to QFileInfo::isFile(), matching what a real OS-level drag always carries,
// so the "Preview" buttons below need something that actually exists to demonstrate the same
// URL-suffix sniffing a real drag would exercise (see mimedatautils.hpp's own docs on why an
// image FILE has no image/* mime format at all).

QString demoDocPath()
{
    auto path=QDir::temp().filePath(QStringLiteral("uise-filedropoverlay-demo-doc.txt"));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
    {
        f.write("FileDropOverlay demo file");
    }
    return path;
}

QString demoImagePath()
{
    auto path=QDir::temp().filePath(QStringLiteral("uise-filedropoverlay-demo-photo.png"));
    if (!QFile::exists(path))
    {
        QImage img(4,4,QImage::Format_RGB32);
        img.fill(Qt::blue);
        img.save(path,"PNG");
    }
    return path;
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    // Style starts in StyleSheetMode::Auto (OS-detected). Lock that in as an explicit mode
    // right away -- same reasoning as demo/fileupload/main.cpp -- so the theme toggle button
    // below always actually flips on its first click.
    Style::instance().setStyleSheetMode(
        Style::instance().isDarkTheme() ? Style::StyleSheetMode::Dark : Style::StyleSheetMode::Light
    );

    QMainWindow w;

    auto* mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);
    auto* central=new QFrame(mainFrame);
    auto* rootLayout=Layout::vertical(central,false);
    mainFrame->setWidget(central);

    rootLayout->addWidget(new QLabel(QStringLiteral(
        "Drag files/images from your file manager onto the chat page area below.\n"
        "A plain file shows one panel; an image file shows two -- Send as documents / Send as images.\n"
        "The Preview buttons show the same layouts without needing an actual OS-level drag.")));

    // --- the "chat page": the whole point of auto-show mode is that this two-line integration
    // is everything a consumer needs -- setAcceptDrops(true) + install(), nothing else ---

    auto* chatPage=new QFrame();
    chatPage->setObjectName("chatPage");
    // Tall enough for the two-panel layout stacked *vertically* (setPanelOrientation), not just
    // side by side -- each panel needs room for its icon (44px) + caption + subtitle plus its
    // own margin/padding (see filedropoverlay.qss), and Qt::Vertical stacks two of those inside
    // the same host height that Qt::Horizontal only ever needed once.
    chatPage->setMinimumHeight(460);
    chatPage->setStyleSheet(QStringLiteral(
        "QFrame#chatPage { border: 1px solid #999999; border-radius: 8px; background-color: palette(base); }"));
    auto* chatPageLayout=Layout::vertical(chatPage);
    chatPageLayout->addWidget(new QLabel(QStringLiteral("  Chat page (drop target)")));
    chatPageLayout->addWidget(new QLabel(QStringLiteral("  -- a fake message bubble --")));
    chatPageLayout->addStretch(1);
    rootLayout->addWidget(chatPage,1);

    chatPage->setAcceptDrops(true);
    auto* overlay=FileDropOverlay::install(chatPage);

    // --- log ---

    auto* log=new QPlainTextEdit();
    log->setReadOnly(true);
    log->setMaximumHeight(140);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    QObject::connect(overlay,&FileDropOverlay::activeChanged,&app,
        [logMsg](bool active)
        {
            logMsg(active ? QStringLiteral("-- overlay shown --") : QStringLiteral("-- overlay hidden --"));
        }
    );
    QObject::connect(overlay,&FileDropOverlay::panelHovered,&app,
        [logMsg](FileDropOverlay::Panel panel)
        {
            logMsg(QString("panelHovered: %1").arg(panelName(panel)));
        }
    );

    // --- modal upload dialog host. FrameWithModalPopup sizes its popup as a percentage of its
    // OWN rect, so this needs real, laid-out geometry before openDialog() is ever called -- see
    // demo/fileupload/main.cpp's "Modal dialog hosts" section for the documented trap. ---

    auto* uploadDialogFrame=new ModalFileUploadDialog(central);
    uploadDialogFrame->setObjectName("uploadDialogFrame");
    uploadDialogFrame->setMinimumHeight(560);
    rootLayout->addWidget(uploadDialogFrame);

    // --- the real payoff path: forward a drop into the upload dialog, presetting "send as
    // documents" from which panel received it. dropped()'s mimeData is only valid for the
    // duration of this slot (see FileDropOverlay::dropped()'s own docs) -- addFromMimeData()
    // consumes it synchronously here; opening the (modal) dialog itself does not need
    // deferring in this demo since nothing here spins the event loop first, but a consumer
    // that does more work before opening should still defer with QTimer::singleShot(0,...),
    // as showing a modal dialog from inside a drop handler blocks the platform's drag session. ---

    QObject::connect(overlay,&FileDropOverlay::dropped,&app,
        [logMsg,uploadDialogFrame](FileDropOverlay::Panel panel, const QMimeData* mimeData)
        {
            logMsg(QString("dropped: %1").arg(panelName(panel)));

            uploadDialogFrame->openDialog(false);
            auto* widget=uploadDialogFrame->dialog()->fileUploadWidget();
            widget->setSendAsDocuments(panel==FileDropOverlay::Panel::Documents);
            widget->addFromMimeData(mimeData);
        }
    );

    // --- preview buttons: exercise the QSS/layout without needing an actual OS-level drag ---

    auto* previewFrame=new QFrame();
    auto* previewLayout=Layout::horizontal(previewFrame);
    rootLayout->addWidget(previewFrame);

    auto* previewDocsButton=new QPushButton(QStringLiteral("Preview (no images)"));
    previewLayout->addWidget(previewDocsButton);
    QObject::connect(previewDocsButton,&QPushButton::clicked,overlay,
        [overlay]()
        {
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(demoDocPath())});
            overlay->showForMimeData(&mime);
        }
    );

    auto* previewImagesButton=new QPushButton(QStringLiteral("Preview (with images)"));
    previewLayout->addWidget(previewImagesButton);
    QObject::connect(previewImagesButton,&QPushButton::clicked,overlay,
        [overlay]()
        {
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(demoImagePath())});
            overlay->showForMimeData(&mime);
        }
    );

    auto* dismissButton=new QPushButton(QStringLiteral("Dismiss"));
    previewLayout->addWidget(dismissButton);
    QObject::connect(dismissButton,&QPushButton::clicked,overlay,&FileDropOverlay::dismiss);

    previewLayout->addStretch(1);

    // --- controls ---

    auto* controlsFrame=new QFrame();
    auto* controlsLayout=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

    auto* autoShowCheck=new QCheckBox(QStringLiteral("Auto-show"));
    autoShowCheck->setChecked(overlay->isAutoShow());
    controlsLayout->addWidget(autoShowCheck);
    QObject::connect(autoShowCheck,&QCheckBox::toggled,overlay,&FileDropOverlay::setAutoShow);

    auto* imagesAllowedCheck=new QCheckBox(QStringLiteral("Images panel allowed"));
    imagesAllowedCheck->setChecked(overlay->isImagesPanelAllowed());
    controlsLayout->addWidget(imagesAllowedCheck);
    QObject::connect(imagesAllowedCheck,&QCheckBox::toggled,overlay,&FileDropOverlay::setImagesPanelAllowed);

    controlsLayout->addWidget(new QLabel(QStringLiteral("Panel orientation:")));
    auto* orientationCombo=new QComboBox();
    orientationCombo->addItem(QStringLiteral("Horizontal"),static_cast<int>(Qt::Horizontal));
    orientationCombo->addItem(QStringLiteral("Vertical"),static_cast<int>(Qt::Vertical));
    orientationCombo->setCurrentIndex(overlay->panelOrientation()==Qt::Vertical ? 1 : 0);
    controlsLayout->addWidget(orientationCombo);
    QObject::connect(orientationCombo,&QComboBox::currentIndexChanged,overlay,
        [overlay,orientationCombo](int index)
        {
            overlay->setPanelOrientation(static_cast<Qt::Orientation>(orientationCombo->itemData(index).toInt()));
        }
    );

    controlsLayout->addWidget(new QLabel(QStringLiteral("Leave watchdog (ms, 0=off):")));
    auto* watchdogSpin=new QSpinBox();
    watchdogSpin->setRange(0,5000);
    watchdogSpin->setSingleStep(50);
    watchdogSpin->setValue(overlay->leaveWatchdogIntervalMs());
    controlsLayout->addWidget(watchdogSpin);
    QObject::connect(watchdogSpin,&QSpinBox::valueChanged,overlay,&FileDropOverlay::setLeaveWatchdogIntervalMs);

    controlsLayout->addStretch(1);

    auto* themeButton=new QPushButton();
    themeButton->setCheckable(true);
    auto updateThemeButtonLabel=[](QPushButton* button)
    {
        auto dark=Style::instance().isDarkTheme();
        button->setChecked(dark);
        button->setText(dark ? QStringLiteral("Dark theme") : QStringLiteral("Light theme"));
    };
    updateThemeButtonLabel(themeButton);
    controlsLayout->addWidget(themeButton);
    QObject::connect(themeButton,&QPushButton::clicked,&app,
        [updateThemeButtonLabel,themeButton]()
        {
            auto mode=Style::instance().styleSheetMode();
            auto newMode=(mode==Style::StyleSheetMode::Dark)
                            ? Style::StyleSheetMode::Light
                            : Style::StyleSheetMode::Dark;
            Style::instance().setStyleSheetMode(newMode);
            Style::instance().applyStyleSheet(true);
            updateThemeButtonLabel(themeButton);
        }
    );

    rootLayout->addWidget(log);

    w.setCentralWidget(mainFrame);
    w.resize(760,900);
    w.show();

    return app.exec();
}
