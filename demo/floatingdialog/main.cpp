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

/** @file demo/floatingdialog/main.cpp
*
*  Demo application of FloatingDialogFrame and FloatingDialog.
*
*/

/****************************************************************************/

#include <memory>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/statusdialog.hpp>
#include <uise/desktop/floatingdialog.hpp>
// Dialog<> is instantiated directly below (section 2) rather than through a pre-built
// concrete subclass like StatusDialog, so -- unlike statusdialog.hpp -- its template
// definition must be visible in this translation unit (see how every Dialog<BaseT> subclass's
// own .cpp, e.g. src/statusdialog.cpp, includes this same header for the same reason).
#include <uise/desktop/ipp/dialog.ipp>

using namespace UISE_DESKTOP_NAMESPACE;

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
        log->appendPlainText(text);
    };

    rootLayout->addWidget(new QLabel(
        QStringLiteral("All dialogs below are non-modal: the main window stays fully usable "
                       "while they are open, and each can be moved by dragging its title bar.")
    ));

    // --- 1. FloatingDialog<AbstractStatusDialog,StatusDialog>: stock dialog, non-modal ---

    auto* statusRow=new QFrame(central);
    auto* statusRowL=Layout::horizontal(statusRow);
    rootLayout->addWidget(statusRow);

    statusRowL->addWidget(new QLabel(QStringLiteral("Stock status dialog:")));

    auto* statusButton=new QPushButton(QStringLiteral("Open floating dialog"),statusRow);
    statusRowL->addWidget(statusButton);
    statusRowL->addStretch(1);

    auto* floatingStatus=new FloatingDialog<AbstractStatusDialog,StatusDialog>(statusRow);
    QObject::connect(
        statusButton,
        &QPushButton::clicked,
        floatingStatus,
        [floatingStatus,logMsg]()
        {
            auto isNew=floatingStatus->openDialog(false,false);
            if (isNew)
            {
                floatingStatus->dialog()->setStatus(
                    QStringLiteral("This dialog is hosted in a FloatingDialog, the floating "
                                   "counterpart of ModalDialog. Drag it by its title bar."),
                    StatusBase::Type::Info,
                    QStringLiteral("Floating status")
                );
            }
            floatingStatus->popup();
            logMsg(isNew ? QStringLiteral("Floating status dialog opened")
                         : QStringLiteral("Floating status dialog re-shown"));
        }
    );
    QObject::connect(
        floatingStatus,
        &FloatingDialogFrame::closed,
        log,
        [logMsg]()
        {
            logMsg(QStringLiteral("Floating status dialog closed"));
        }
    );

    // --- 2. FloatingDialogFrame + a hand-built Dialog<> with custom content and buttons ---

    auto* customRow=new QFrame(central);
    auto* customRowL=Layout::horizontal(customRow);
    rootLayout->addWidget(customRow);

    customRowL->addWidget(new QLabel(QStringLiteral("Custom dialog content:")));

    auto* customButton=new QPushButton(QStringLiteral("Open custom dialog"),customRow);
    customRowL->addWidget(customButton);
    customRowL->addStretch(1);

    auto* customFrame=new FloatingDialogFrame(customRow);
    QObject::connect(
        customButton,
        &QPushButton::clicked,
        customFrame,
        [customFrame,logMsg]()
        {
            if (customFrame->widget()==nullptr)
            {
                auto* dlg=new Dialog<>(customFrame);
                dlg->setTitle(QStringLiteral("Custom floating dialog"));
                dlg->setMinimumWidth(320);

                auto* content=new QFrame(dlg);
                auto* contentL=Layout::vertical(content);
                contentL->addWidget(new QLabel(
                    QStringLiteral("Any widget can be hosted here, exactly like inside a "
                                   "regular Dialog<>.")
                ));
                dlg->setWidget(content);

                dlg->setButtons({
                    AbstractDialog::StandardButton::OK,
                    AbstractDialog::StandardButton::Cancel
                });
                QObject::connect(
                    dlg,
                    &AbstractDialog::buttonClicked,
                    customFrame,
                    [logMsg](int id)
                    {
                        logMsg(QString("Custom dialog: button %1 clicked").arg(id));
                    }
                );
                // Cancel routes through the same closeRequested() as the header's close
                // button (see Dialog<BaseT>'s button-group handler, which calls
                // closeDialog() for Close/Cancel), so this one connection covers both.
                QObject::connect(
                    dlg,
                    &AbstractDialog::closeRequested,
                    customFrame,
                    [customFrame]()
                    {
                        customFrame->close(true);
                    }
                );

                customFrame->setWidget(dlg,true);
            }
            customFrame->popup();
            logMsg(QStringLiteral("Custom floating dialog opened"));
        }
    );
    QObject::connect(
        customFrame,
        &FloatingDialogFrame::closed,
        log,
        [logMsg]()
        {
            logMsg(QStringLiteral("Custom floating dialog closed"));
        }
    );

    // --- 3. setPseudoParent(): frame destroys itself when a watched, non-parent QObject dies ---

    auto* pseudoRow=new QFrame(central);
    auto* pseudoRowL=Layout::horizontal(pseudoRow);
    rootLayout->addWidget(pseudoRow);

    pseudoRowL->addWidget(new QLabel(QStringLiteral("Pseudo-parent lifetime:")));

    auto* pseudoOpenButton=new QPushButton(QStringLiteral("Open with pseudo parent"),pseudoRow);
    pseudoRowL->addWidget(pseudoOpenButton);
    auto* pseudoDestroyButton=new QPushButton(QStringLiteral("Destroy pseudo parent"),pseudoRow);
    pseudoRowL->addWidget(pseudoDestroyButton);
    pseudoRowL->addStretch(1);

    // a throwaway object standing in for "whatever this dialog is logically attached to" --
    // deliberately not the dialog's Qt parent, just something it is told to watch
    auto pseudoParentHolder=std::make_shared<QPointer<QObject>>();
    auto pseudoDialogHolder=std::make_shared<QPointer<FloatingDialog<AbstractStatusDialog,StatusDialog>>>();

    QObject::connect(
        pseudoOpenButton,
        &QPushButton::clicked,
        pseudoRow,
        [pseudoRow,pseudoParentHolder,pseudoDialogHolder,logMsg]()
        {
            if (pseudoParentHolder->isNull())
            {
                *pseudoParentHolder=new QObject();
            }

            if (pseudoDialogHolder->isNull())
            {
                *pseudoDialogHolder=new FloatingDialog<AbstractStatusDialog,StatusDialog>(pseudoRow);
                (*pseudoDialogHolder)->setPseudoParent(*pseudoParentHolder);
                (*pseudoDialogHolder)->openDialog(true,false);
                (*pseudoDialogHolder)->dialog()->setStatus(
                    QStringLiteral("This dialog watches a separate QObject via "
                                   "setPseudoParent(). Destroying that object destroys this "
                                   "dialog too, even though it was never its Qt parent."),
                    StatusBase::Type::Attention,
                    QStringLiteral("Pseudo-parented")
                );
                (*pseudoDialogHolder)->popup();
            }
            else
            {
                (*pseudoDialogHolder)->popup();
            }
            logMsg(QStringLiteral("Pseudo-parented dialog opened"));
        }
    );
    QObject::connect(
        pseudoDestroyButton,
        &QPushButton::clicked,
        pseudoRow,
        [pseudoParentHolder,logMsg]()
        {
            if (pseudoParentHolder->isNull())
            {
                logMsg(QStringLiteral("No pseudo parent to destroy yet"));
                return;
            }
            delete pseudoParentHolder->data();
            logMsg(QStringLiteral("Pseudo parent destroyed -- watching dialog disappears"));
        }
    );

    // --- log ---

    rootLayout->addWidget(log,1);

    // --- controls: theme toggle, host-visibility demo ---

    auto* controlsFrame=new QFrame(central);
    auto* cl=Layout::horizontal(controlsFrame);
    rootLayout->addWidget(controlsFrame);

    auto* themeButton=new QPushButton(QStringLiteral("Toggle theme"));
    cl->addWidget(themeButton);
    QObject::connect(
        themeButton,
        &QPushButton::clicked,
        &app,
        []()
        {
            auto mode=Style::instance().styleSheetMode();
            auto newMode=(mode==Style::StyleSheetMode::Dark)
                            ? Style::StyleSheetMode::Light
                            : Style::StyleSheetMode::Dark;
            Style::instance().setStyleSheetMode(newMode);
            Style::instance().applyStyleSheet(true);
        }
    );

    auto* minimizeButton=new QPushButton(QStringLiteral("Minimize main window"));
    cl->addWidget(minimizeButton);
    QObject::connect(
        minimizeButton,
        &QPushButton::clicked,
        &w,
        [&w,logMsg]()
        {
            logMsg(QStringLiteral("Minimizing main window -- any open floating dialog should "
                                  "hide now and reappear once the window is restored"));
            w.showMinimized();
        }
    );
    cl->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(760,480);
    w.setWindowTitle("FloatingDialog Demo");
    w.show();

    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
