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

/** @file demo/fileupload/main.cpp
*
*  Demo application of FileUploadWidget and ModalFileUploadDialog.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPainter>
#include <QImage>
#include <QPixmap>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/framewithmodalstatus.hpp>
#include <uise/desktop/imageeditdialog.hpp>
#include <uise/desktop/fileuploadwidget.hpp>
#include <uise/desktop/fileuploaddialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

QImage makeGradientImage(int w, int h, const QString& text)
{
    QImage img(w,h,QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0,0,w,h);
    grad.setColorAt(0,QColor("#4C9AFF"));
    grad.setColorAt(1,QColor("#0A66C2"));
    p.fillRect(img.rect(),grad);
    p.setPen(Qt::white);
    auto font=p.font();
    font.setPointSize(qMin(18,qMax(6,qMin(w,h)/3)));
    p.setFont(font);
    p.drawText(img.rect(),Qt::AlignCenter,text);
    p.end();
    return img;
}

QImage makeSampleImage()
{
    return makeGradientImage(320,200,QStringLiteral("Sample image"));
}

//! 900x30 -- 30:1, past FileUploadWidget::DefaultMaxImageAspectRatio (10) -- so this stages as a
//! plain file row out of the box; lower the "Max aspect ratio" spinbox to 0 to see it switch
//! back to an image preview row instead.
QImage makeDisproportionalImage()
{
    return makeGradientImage(900,30,QStringLiteral("Banner"));
}

QString logOptions(const FileUploadItems& items, const FileUploadOptions& opts)
{
    QString text;
    text+=QString("items: %1\n").arg(static_cast<int>(items.size()));
    for (size_t i=0;i<items.size();++i)
    {
        const auto& it=items[i];
        auto sz=it.pixelSize();
        text+=QString("  [%1] %2 (%3, %4, isImage=%5, presentAsImage=%6, pixelSize=%7x%8, maxAspectRatio=%9)\n")
                .arg(static_cast<int>(i))
                .arg(it.fileName())
                .arg(it.type()==FileUploadItem::Type::File ? QStringLiteral("file") : QStringLiteral("image data"))
                .arg(it.size())
                .arg(it.isImage() ? "true" : "false")
                .arg(it.presentAsImage() ? "true" : "false")
                .arg(sz.width())
                .arg(sz.height())
                .arg(it.maxImageAspectRatio());
    }
    text+=QString("highQuality=%1 sendAsDocuments=%2 groupItems=%3 rememberChoice=%4\n")
            .arg(opts.highQuality ? "true" : "false")
            .arg(opts.sendAsDocuments ? "true" : "false")
            .arg(opts.groupItems ? "true" : "false")
            .arg(opts.rememberChoice ? "true" : "false");
    text+=QString("comment: %1").arg(opts.comment);
    return text;
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    // Style starts in StyleSheetMode::Auto (OS-detected). Lock that in as an explicit mode
    // right away: the theme toggle button below flips Dark<->Light off of styleSheetMode(),
    // and if that stayed Auto, the very first click would always land on Dark regardless of
    // what the OS/auto-detected theme actually was -- e.g. starting in an auto-detected dark
    // OS, the first "toggle" would re-select Dark and visibly do nothing.
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
    log->setMinimumHeight(200);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    // window-level image editor, kept separate from the file-upload dialog's own popup so it
    // is never confined to that dialog's <=90% rect (see fileuploaddialog.hpp). Constructed
    // here (captured by the lambdas below) but given its real, bordered/labeled layout
    // placement further down, grouped with the upload dialog host -- see "Modal dialog hosts".
    auto* imageEditDialog=new ModalImageEditDialog(central);

    // The dialog/editor are reused across edit sessions -- openDialog() only returns true the
    // FIRST time (see the file-upload dialog's own isNew pattern). A connection made only under
    // that guard would stay bound to whichever (uw,index) was current the first time it fired,
    // silently misapplying every later edit to the wrong item -- so instead this is rebound
    // (disconnect+reconnect) on every call, to the session actually in progress.
    auto applyConnection=std::make_shared<QMetaObject::Connection>();

    auto openImageEditor=[imageEditDialog,logMsg,applyConnection](AbstractFileUploadWidget* uw, int index)
    {
        imageEditDialog->openDialog();
        auto dlg=imageEditDialog->dialog();
        if (dlg==nullptr)
        {
            return;
        }
        auto* editor=dlg->editor();
        editor->loadImage(QPixmap::fromImage(uw->itemImage(index)));

        QObject::disconnect(*applyConnection);
        *applyConnection=QObject::connect(
            dlg,
            &AbstractDialog::buttonClicked,
            imageEditDialog,
            [dlg,editor,uw,index,logMsg](int id)
            {
                if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Apply))
                {
                    uw->setItemImage(index,editor->editedImage().toImage());
                    logMsg(QString("edited image at index %1").arg(index));
                }
                dlg->closeDialog();
            }
        );
    };

    auto wireUploadWidget=[openImageEditor,logMsg](AbstractFileUploadWidget* uw, const QString& label)
    {
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::editImageRequested,
            uw,
            [uw,openImageEditor](int index)
            {
                openImageEditor(uw,index);
            }
        );
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::sendRequested,
            uw,
            [uw,label,logMsg]()
            {
                logMsg(QString("--- %1: Send ---\n%2").arg(label,logOptions(uw->items(),uw->options())));
            }
        );
        // Logged immediately on every add/remove -- not just at Send -- so the isImage()/
        // pixelSize()/maxAspectRatio breakdown from logOptions() is visible right after clicking
        // e.g. "Add disproportional image", with no extra click needed to see the effect.
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::itemsChanged,
            uw,
            [uw,label,logMsg]()
            {
                logMsg(QString("--- %1: itemsChanged ---\n%2").arg(label,logOptions(uw->items(),uw->options())));
            }
        );
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::cancelled,
            uw,
            [label,logMsg]()
            {
                logMsg(QString("--- %1: Cancelled ---").arg(label));
            }
        );
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::maxFileCountExceeded,
            uw,
            [label,logMsg](int rejected)
            {
                logMsg(QString("--- %1: rejected %2 item(s), max file count reached ---").arg(label).arg(rejected));
            }
        );
        QObject::connect(
            uw,
            &AbstractFileUploadWidget::emptied,
            uw,
            [label,logMsg]()
            {
                logMsg(QString("--- %1: emptied() ---").arg(label));
            }
        );
    };

    // Demo-only theming helpers. Both are driven by Style::instance().isDarkTheme() rather
    // than checkDarkTheme() or CSS "palette(...)" roles: checkDarkTheme() always re-detects
    // the OS/application palette live and ignores an explicitly-set uise style mode (so it
    // would keep reporting the OS theme even after "Toggle theme" below switches uise to the
    // other one), and "palette(...)" resolves against Qt's own application palette, which is
    // independent of uise's theme in exactly the same way.
    //
    // Plain QPushButtons (as opposed to uise::PushButton) carry no uise QSS of their own --
    // every QPushButton rule in this library's stylesheets is scoped under a uise--Something
    // ancestor selector, by design (context-specific styling, e.g. dialog buttons vs. a
    // toolbar button), so a bare QPushButton used directly in demo code has nothing to fall
    // back on and renders with whatever bare-minimum chrome the active QStyle gives an
    // unadorned button. applyButtonStyle() below gives the demo's own controls the same
    // bordered look as the widget's Add/Cancel/Send buttons, explicitly.

    std::vector<QPushButton*> demoButtons;

    auto applyButtonStyle=[](QPushButton* button)
    {
        auto dark=Style::instance().isDarkTheme();
        auto color=dark ? QStringLiteral("#CCCCCC") : QStringLiteral("#555555");
        auto border=QStringLiteral("#999999");
        auto hoverColor=dark ? QStringLiteral("#EEEEEE") : QStringLiteral("#444444");
        button->setStyleSheet(
            QString(
                "QPushButton {"
                "color: %1;"
                "border: 1px solid %2;"
                "border-radius: 4px;"
                "padding: 4px 12px;"
                "background-color: transparent;"
                "}"
                "QPushButton:hover {"
                "color: %3;"
                "border-color: %3;"
                "}"
            ).arg(color,border,hoverColor)
        );
    };

    auto applyWrapperTheme=[](QFrame* wrapper)
    {
        auto dark=Style::instance().isDarkTheme();
        auto border=dark ? QStringLiteral("#444444") : QStringLiteral("#D0D0D0");
        auto background=dark ? QStringLiteral("#2B2B2B") : QStringLiteral("#FFFFFF");
        wrapper->setStyleSheet(
            QString(
                "QFrame#%1 {"
                "border: 1px solid %2;"
                "border-radius: 8px;"
                "padding: 12px;"
                "padding-top: 0px;"
                "background-color: %3;"
                "}"
            ).arg(wrapper->objectName(),border,background)
        );
    };

    // --- 1. standalone embedded widget, in a bordered/padded wrapper so it reads as a
    // distinct block against the rest of the demo UI rather than blending into it ---

    rootLayout->addWidget(new QLabel(QStringLiteral("Standalone FileUploadWidget:")));

    auto* standaloneWrapper=new QFrame(central);
    standaloneWrapper->setObjectName("standaloneWrapper");
    applyWrapperTheme(standaloneWrapper);
    auto* standaloneWrapperLayout=Layout::vertical(standaloneWrapper);
    rootLayout->addWidget(standaloneWrapper);
    rootLayout->addSpacing(8);

    auto* standaloneWidget=new FileUploadWidget(standaloneWrapper);
    standaloneWrapperLayout->addWidget(standaloneWidget);
    wireUploadWidget(standaloneWidget,QStringLiteral("standalone"));

    // --- 2. modal dialog hosts. FrameWithModalPopup sizes its popup as a percentage of its
    // OWN rect (see FrameWithModalPopup::resizeEvent / ModalPopup::updateWidgetGeometry), so
    // each host needs a real, deterministic area of its own: an unmanaged widget defaults to a
    // tiny top-left placeholder rect, and a layout-managed one with no content of its own
    // collapses to 0x0. Bordered/labeled the same way as the standalone wrapper above so this
    // reads as an intentional test area rather than a layout bug, and grouped together in one
    // place rather than scattered through the rest of the demo. uploadDialogFrame is kept
    // alive across openings so reset() has something to reuse.

    rootLayout->addWidget(new QLabel(QStringLiteral("Modal dialog hosts:")));

    auto* uploadDialogFrame=new ModalFileUploadDialog(central);
    uploadDialogFrame->setObjectName("uploadDialogFrame");
    applyWrapperTheme(uploadDialogFrame);
    // even with popup auto-height enabled (see ModalFileUploadDialog's ctor), the reflowed
    // height is still capped at maxHeightPercent() of THIS frame's own height (see
    // ModalPopup::updateWidgetGeometry's auto-height branch) -- so the host still needs to be
    // tall enough that 90% of it comfortably clears FileUploadWidget's natural maximum height
    // (header + max list area 360 + 3 checkboxes + max comments 110 + button row + padding).
    uploadDialogFrame->setMinimumHeight(800);

    // The two buttons below are added to the layout BEFORE uploadDialogFrame/imageEditDialog
    // (both ~800px+360px tall), and right after their own frames are constructed -- not after,
    // as originally written -- so clicking either one does not require first scrolling the
    // dialog host itself out of view: the popup opens inside uploadDialogFrame, which stays
    // visible directly below the button that opened it.

    auto* openDialogButton=new QPushButton(QStringLiteral("Open upload dialog"));
    applyButtonStyle(openDialogButton);
    demoButtons.push_back(openDialogButton);
    rootLayout->addWidget(openDialogButton);
    QObject::connect(
        openDialogButton,
        &QPushButton::clicked,
        uploadDialogFrame,
        [uploadDialogFrame,wireUploadWidget]()
        {
            // openDialog(false,false): create/reuse the dialog WITHOUT showing it yet. This
            // button opens empty, so the ordering makes no visible difference here -- but it
            // is the same open-hidden-then-show shape "Open dialog with sample files" below
            // relies on, wiring under `if (isNew)` and showing last either way.
            auto isNew=uploadDialogFrame->openDialog(false,false);
            if (isNew)
            {
                wireUploadWidget(uploadDialogFrame->dialog()->fileUploadWidget(),QStringLiteral("dialog"));
            }
            uploadDialogFrame->showDialog();
        }
    );

    // Reproduces the pre-populated-dialog flicker this change targets: without
    // openDialog(...,/*show*/false) + showDialog(), a dialog opened already carrying files
    // would appear at its empty size and then visibly jump as setSendAsDocuments()/addItems()
    // land. Here the widget is filled while still invisible, so it should appear once, already
    // at its final size.
    auto* openWithFilesButton=new QPushButton(QStringLiteral("Open dialog with sample files"));
    applyButtonStyle(openWithFilesButton);
    demoButtons.push_back(openWithFilesButton);
    rootLayout->addWidget(openWithFilesButton);
    QObject::connect(
        openWithFilesButton,
        &QPushButton::clicked,
        uploadDialogFrame,
        [uploadDialogFrame,wireUploadWidget]()
        {
            auto isNew=uploadDialogFrame->openDialog(false,false);
            auto* widget=uploadDialogFrame->dialog()->fileUploadWidget();
            if (isNew)
            {
                wireUploadWidget(widget,QStringLiteral("dialog"));
            }
            widget->addFiles({QString::fromUtf8(__FILE__)});
            widget->addItems(
                {
                    FileUploadItem::fromImage(makeSampleImage()),
                    FileUploadItem::fromImage(makeDisproportionalImage())
                }
            );
            uploadDialogFrame->showDialog();
        }
    );

    rootLayout->addWidget(uploadDialogFrame);
    rootLayout->addSpacing(8);

    imageEditDialog->setObjectName("imageEditWrapper");
    applyWrapperTheme(imageEditDialog);
    imageEditDialog->setMinimumHeight(360);
    rootLayout->addWidget(imageEditDialog);
    rootLayout->addSpacing(8);

    // --- 3. controls ---

    auto* controlsFrame=new QFrame(central);
    auto* cl=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

    auto addSampleFiles=new QPushButton(QStringLiteral("Add sample files"));
    applyButtonStyle(addSampleFiles);
    demoButtons.push_back(addSampleFiles);
    cl->addWidget(addSampleFiles);
    QObject::connect(
        addSampleFiles,
        &QPushButton::clicked,
        standaloneWidget,
        [standaloneWidget]()
        {
            standaloneWidget->addFiles({QString::fromUtf8(__FILE__)});
        }
    );

    auto addSampleImage=new QPushButton(QStringLiteral("Add sample image"));
    applyButtonStyle(addSampleImage);
    demoButtons.push_back(addSampleImage);
    cl->addWidget(addSampleImage);
    QObject::connect(
        addSampleImage,
        &QPushButton::clicked,
        standaloneWidget,
        [standaloneWidget]()
        {
            standaloneWidget->addItems({FileUploadItem::fromImage(makeSampleImage())});
        }
    );

    // Exercises FileUploadItem::isImage()'s extreme-aspect-ratio check (see the "Max aspect
    // ratio" spinbox below) -- with that spinbox at its own default (FileUploadWidget::
    // DefaultMaxImageAspectRatio, 10), this 30:1 image stages as a plain file row straight away;
    // lower the spinbox to 0 to see it switch to a normal image preview row instead, like "Add
    // sample image" above.
    auto addDisproportionalImage=new QPushButton(QStringLiteral("Add disproportional image"));
    applyButtonStyle(addDisproportionalImage);
    demoButtons.push_back(addDisproportionalImage);
    cl->addWidget(addDisproportionalImage);
    QObject::connect(
        addDisproportionalImage,
        &QPushButton::clicked,
        standaloneWidget,
        [standaloneWidget]()
        {
            standaloneWidget->addItems({FileUploadItem::fromImage(makeDisproportionalImage())});
        }
    );

    cl->addWidget(new QLabel(QStringLiteral("Max aspect ratio:")));
    auto* maxAspectRatio=new QSpinBox();
    maxAspectRatio->setRange(0,100);
    maxAspectRatio->setSpecialValueText(QStringLiteral("off"));
    maxAspectRatio->setValue(static_cast<int>(standaloneWidget->maxImageAspectRatio()));
    cl->addWidget(maxAspectRatio);
    QObject::connect(
        maxAspectRatio,
        &QSpinBox::valueChanged,
        standaloneWidget,
        [standaloneWidget,uploadDialogFrame,logMsg](int val)
        {
            standaloneWidget->setMaxImageAspectRatio(static_cast<uint32_t>(val));
            if (uploadDialogFrame->dialog()!=nullptr)
            {
                uploadDialogFrame->dialog()->fileUploadWidget()->setMaxImageAspectRatio(static_cast<uint32_t>(val));
            }
            // setMaxImageAspectRatio() re-stamps existing items in place but does not itself
            // emit itemsChanged() (it's a re-classification of what's already staged, not an
            // add/remove) -- log explicitly here so raising/lowering the spinbox shows its
            // effect on already-added items without requiring another add/remove first.
            logMsg(QString("--- standalone: setMaxImageAspectRatio(%1) ---\n%2")
                .arg(val)
                .arg(logOptions(standaloneWidget->items(),standaloneWidget->options())));
        }
    );

    cl->addWidget(new QLabel(QStringLiteral("Max files:")));
    auto* maxCount=new QSpinBox();
    maxCount->setRange(1,50);
    maxCount->setValue(standaloneWidget->maxFileCount());
    cl->addWidget(maxCount);
    QObject::connect(
        maxCount,
        &QSpinBox::valueChanged,
        standaloneWidget,
        [standaloneWidget,uploadDialogFrame](int val)
        {
            standaloneWidget->setMaxFileCount(val);
            if (uploadDialogFrame->dialog()!=nullptr)
            {
                uploadDialogFrame->dialog()->fileUploadWidget()->setMaxFileCount(val);
            }
        }
    );

    auto* closeWhenEmpty=new QCheckBox(QStringLiteral("Close dialog when empty"));
    cl->addWidget(closeWhenEmpty);
    QObject::connect(
        closeWhenEmpty,
        &QCheckBox::toggled,
        uploadDialogFrame,
        [uploadDialogFrame](bool enable)
        {
            if (uploadDialogFrame->dialog()!=nullptr)
            {
                uploadDialogFrame->dialog()->setCloseWhenEmpty(enable);
            }
        }
    );

    auto* resetDialogButton=new QPushButton(QStringLiteral("Reset dialog"));
    applyButtonStyle(resetDialogButton);
    demoButtons.push_back(resetDialogButton);
    cl->addWidget(resetDialogButton);
    QObject::connect(
        resetDialogButton,
        &QPushButton::clicked,
        uploadDialogFrame,
        [standaloneWidget,uploadDialogFrame]()
        {
            if (uploadDialogFrame->dialog()!=nullptr)
            {
                uploadDialogFrame->dialog()->reset();
            }
            standaloneWidget->reset();
        }
    );

    // checkable, and its initial checked/label state reflects whatever theme was locked in
    // at startup above -- not a fixed "Toggle theme" that silently assumes a starting side
    auto* themeButton=new QPushButton();
    themeButton->setCheckable(true);
    applyButtonStyle(themeButton);
    demoButtons.push_back(themeButton);
    cl->addWidget(themeButton);

    auto updateThemeButtonLabel=[](QPushButton* button)
    {
        auto dark=Style::instance().isDarkTheme();
        button->setChecked(dark);
        button->setText(dark ? QStringLiteral("Dark theme") : QStringLiteral("Light theme"));
    };
    updateThemeButtonLabel(themeButton);

    QObject::connect(
        themeButton,
        &QPushButton::clicked,
        &app,
        [applyWrapperTheme,applyButtonStyle,updateThemeButtonLabel,standaloneWrapper,uploadDialogFrame,imageEditDialog,themeButton,&demoButtons]()
        {
            auto mode=Style::instance().styleSheetMode();
            auto newMode=(mode==Style::StyleSheetMode::Dark)
                            ? Style::StyleSheetMode::Light
                            : Style::StyleSheetMode::Dark;
            Style::instance().setStyleSheetMode(newMode);
            Style::instance().applyStyleSheet(true);

            applyWrapperTheme(standaloneWrapper);
            applyWrapperTheme(uploadDialogFrame);
            applyWrapperTheme(imageEditDialog);
            for (auto* button : demoButtons)
            {
                applyButtonStyle(button);
            }
            updateThemeButtonLabel(themeButton);
        }
    );

    cl->addStretch(1);

    // --- log ---

    rootLayout->addWidget(new QLabel(QStringLiteral("Log:")));
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(900,900);
    w.setWindowTitle("FileUpload Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
