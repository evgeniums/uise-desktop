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

/** @file demo/calendardialog/main.cpp
*
*  Demo application of CalendarDialog -- a floating calendar dialog that closes itself after a
*  period of inactivity.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QTime>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/calendar.hpp>
#include <uise/desktop/calendardialog.hpp>
#include <uise/desktop/floatingdialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

QString modeName(CalendarMode mode)
{
    switch (mode)
    {
        case (CalendarMode::Activation): return QStringLiteral("Activation");
        case (CalendarMode::SingleSelection): return QStringLiteral("SingleSelection");
        case (CalendarMode::RangeSelection): return QStringLiteral("RangeSelection");
        case (CalendarMode::MultipleSelection): return QStringLiteral("MultipleSelection");
        case (CalendarMode::Auto): return QStringLiteral("Auto");
        case (CalendarMode::ExtendedSelection): return QStringLiteral("ExtendedSelection");
    }
    return QString();
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

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
        log->appendPlainText(QTime::currentTime().toString("hh:mm:ss.zzz")+"  "+text);
    };

    auto* descriptionLabel=new QLabel(
        QStringLiteral("The dialog below is a FloatingDialog<AbstractCalendarDialog,CalendarDialog>: "
                       "it stays open while you interact with the calendar or the dialog itself, and "
                       "closes on its own after the configured period of inactivity. An open month "
                       "picker or dates dropdown always defers the close.")
    );
    descriptionLabel->setWordWrap(true);
    rootLayout->addWidget(descriptionLabel);

    // --- controls ---

    auto* controlsFrame=new QFrame(central);
    auto* controlsLayout=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

    controlsLayout->addWidget(new QLabel(QStringLiteral("Mode:")));
    auto* modeCombo=new QComboBox(controlsFrame);
    for (auto mode: {CalendarMode::Activation,CalendarMode::SingleSelection,CalendarMode::RangeSelection,
                      CalendarMode::MultipleSelection,CalendarMode::Auto,CalendarMode::ExtendedSelection})
    {
        modeCombo->addItem(modeName(mode),static_cast<int>(mode));
    }
    modeCombo->setCurrentIndex(1); // SingleSelection
    controlsLayout->addWidget(modeCombo);

    controlsLayout->addWidget(new QLabel(QStringLiteral("Auto-close delay:")));
    auto* timeoutSpin=new QSpinBox(controlsFrame);
    timeoutSpin->setRange(1000,120000);
    timeoutSpin->setSingleStep(1000);
    timeoutSpin->setSuffix(QStringLiteral(" ms"));
    timeoutSpin->setValue(AbstractCalendarDialog::DefaultAutoCloseDelayMs);
    controlsLayout->addWidget(timeoutSpin);

    auto* autoCloseCheck=new QCheckBox(QStringLiteral("Auto close enabled"),controlsFrame);
    autoCloseCheck->setChecked(true);
    controlsLayout->addWidget(autoCloseCheck);

    auto* closeOnSelectCheck=new QCheckBox(QStringLiteral("Close on selection"),controlsFrame);
    closeOnSelectCheck->setChecked(false);
    controlsLayout->addWidget(closeOnSelectCheck);

    auto* openButton=new QPushButton(QStringLiteral("Open dialog"),controlsFrame);
    controlsLayout->addWidget(openButton);
    auto* closeButton=new QPushButton(QStringLiteral("Close dialog"),controlsFrame);
    controlsLayout->addWidget(closeButton);

    controlsLayout->addStretch(1);

    // --- floating calendar dialog ---

    auto* floating=new FloatingDialog<AbstractCalendarDialog,CalendarDialog>(central);

    auto applyConfig=[floating,timeoutSpin,autoCloseCheck,closeOnSelectCheck,modeCombo]()
    {
        if (floating->dialog().isNull())
        {
            return;
        }
        auto dlg=floating->dialog();
        dlg->setAutoCloseDelayMs(timeoutSpin->value());
        dlg->setAutoCloseEnabled(autoCloseCheck->isChecked());
        dlg->setCloseMode(closeOnSelectCheck->isChecked()
                               ? CalendarDialogCloseMode::AutoCloseOnSelection
                               : CalendarDialogCloseMode::ExplicitButton);
        dlg->calendar()->setMode(static_cast<CalendarMode>(modeCombo->currentData().toInt()));
    };

    QObject::connect(
        openButton,
        &QPushButton::clicked,
        floating,
        [floating,applyConfig,logMsg,log]()
        {
            auto isNew=floating->openDialog(false /*destroyOnClose*/,false /*show*/);
            if (isNew)
            {
                auto dlg=floating->dialog();

                QObject::connect(
                    dlg->calendar(),
                    &Calendar::activity,
                    dlg,
                    [dlg,logMsg]()
                    {
                        logMsg(QString("activity -- countdown restarted (popup open: %1)")
                                   .arg(dlg->calendar()->isPopupOpen() ? "yes" : "no"));
                    }
                );
                QObject::connect(
                    dlg,
                    &AbstractCalendarDialog::autoClosed,
                    dlg,
                    [dlg,logMsg]()
                    {
                        logMsg(QString("AUTO-CLOSED after %1 ms of inactivity").arg(dlg->autoCloseDelayMs()));
                    }
                );
                QObject::connect(
                    dlg->calendar(),
                    &Calendar::dateActivated,
                    dlg,
                    [logMsg](const QDate& date)
                    {
                        logMsg(QStringLiteral("date activated: ")+date.toString(Qt::ISODate));
                    }
                );
                QObject::connect(
                    dlg->calendar(),
                    &Calendar::selectionChanged,
                    dlg,
                    [logMsg]()
                    {
                        logMsg(QStringLiteral("selection changed"));
                    }
                );
                QObject::connect(
                    floating,
                    &FloatingDialogFrame::closed,
                    log,
                    [logMsg]()
                    {
                        logMsg(QStringLiteral("closed"));
                    }
                );

                logMsg(QStringLiteral("dialog created"));
            }

            applyConfig();
            floating->popup();
            logMsg(isNew ? QStringLiteral("dialog opened") : QStringLiteral("dialog re-shown"));
        }
    );

    QObject::connect(
        closeButton,
        &QPushButton::clicked,
        floating,
        [floating,logMsg]()
        {
            if (floating->dialog().isNull())
            {
                logMsg(QStringLiteral("nothing to close"));
                return;
            }
            floating->close(false);
        }
    );

    QObject::connect(timeoutSpin,QOverload<int>::of(&QSpinBox::valueChanged),floating,applyConfig);
    QObject::connect(autoCloseCheck,&QCheckBox::toggled,floating,applyConfig);
    QObject::connect(closeOnSelectCheck,&QCheckBox::toggled,floating,applyConfig);
    QObject::connect(modeCombo,QOverload<int>::of(&QComboBox::currentIndexChanged),floating,applyConfig);

    // --- log ---

    rootLayout->addWidget(log,1);

    // --- colour theme selector -- same combo recipe as demo/chatimageviewerwindow/main.cpp,
    // demo/chatmessagefiles/main.cpp and demo/chatimageviewerflyweight/main.cpp: a 3-way
    // Auto/Light/Dark selector rather than a 2-state toggle, so Auto (OS-tracked) is reachable
    // too, not just an explicit Light/Dark flip. ---

    auto* themeRow=new QFrame(central);
    auto* themeLayout=Layout::horizontal(themeRow);
    rootLayout->addWidget(themeRow);

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
        &app,
        [themeCombo]()
        {
            auto mode=static_cast<Style::StyleSheetMode>(themeCombo->currentData().toInt());
            Style::instance().setStyleSheetMode(mode);
            // reload=true: re-resolves every already-created SvgIcon (calendar nav chevrons,
            // Apply button) against the new theme's colour maps, not just the QSS.
            Style::instance().applyStyleSheet(true);
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(760,520);
    w.setWindowTitle("CalendarDialog Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
