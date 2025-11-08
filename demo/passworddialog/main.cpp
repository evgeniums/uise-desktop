/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file demo/passworddialog/main.cpp
*
*  Demo application of modal PasswordDialog.
*
*/

/****************************************************************************/

#include <QtMath>
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QMenu>
#include <QMessageBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/framewithmodalstatus.hpp>
#include <uise/desktop/passworddialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    QString darkTheme="QTextEdit,QLineEdit {background-color: black;color:white;} \n uise--FrameWithModalPopup {background-color: black;}";
    QString lightTheme="QTextEdit,QLineEdit {background-color: white;color:black;} \n uise--FrameWithModalPopup {background-color: lightgrey;}";

    Style::instance().applyStyleSheet();

    auto l = Layout::vertical(mainFrame,false);

    auto topFrame=new QFrame();
    l->addWidget(topFrame);
    auto tl=Layout::horizontal(topFrame,false);
    auto testFrame=new QFrame();
    testFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tl->addWidget(testFrame);
    auto tfl=Layout::horizontal(testFrame,false);
    auto testWidget=new FrameWithModalStatus(testFrame);
    testWidget->construct();
    tfl->addWidget(testWidget);
    auto wl1=Layout::vertical(testWidget,false);

    auto dialogFrame=new ModalPasswordDialog();
    wl1->addWidget(dialogFrame);
    auto wl=Layout::vertical(dialogFrame,false);

    auto editor1=new QTextEdit();
    wl->addWidget(editor1);
    editor1->setText("Lorem ipsum dolor sit amet. Id enim repudiandae quo nulla quia id quisquam tempore et nostrum illum. Ex inventore repellendus et nobis molestiae aut facere dolores eos quod totam qui galisum quaerat ex illo aspernatur in quaerat quaerat. Ex unde galisum est omnis iure et ducimus suscipit in maiores officiis aut voluptas molestiae et fuga quia?");

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto height=new QSpinBox();
    height->setRange(0,100);
    height->setValue(FrameWithModalPopup::DefaultMaxHeightPercent);
    confl->addWidget(new QLabel("Height percent"),i,0);
    confl->addWidget(height,i,1);
    QObject::connect(height,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxHeightPercent);
    QObject::connect(height,&QSpinBox::valueChanged,dialogFrame,&FrameWithModalPopup::setMaxHeightPercent);
    i++;

    auto width=new QSpinBox();
    width->setRange(0,100);
    width->setValue(FrameWithModalPopup::DefaultMaxWidthPercent);
    confl->addWidget(new QLabel("Width percent"),i,0);
    confl->addWidget(width,i,1);
    QObject::connect(width,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxWidthPercent);
    QObject::connect(width,&QSpinBox::valueChanged,dialogFrame,&FrameWithModalPopup::setMaxWidthPercent);
    i++;

    auto styleButton=new QPushButton();
    styleButton->setText("Light theme");
    styleButton->setCheckable(true);
    if (Style::instance().checkDarkTheme())
    {
        styleButton->setChecked(true);
        styleButton->setText("Dark theme");
    }
    QObject::connect(styleButton,&QPushButton::toggled,testFrame,[&darkTheme,&lightTheme,testFrame,styleButton](bool enable){
        if (enable)
        {
            Style::instance().setBaseQss(darkTheme);
            Style::instance().setColorTheme(Style::DarkTheme);
            Style::instance().applyStyleSheet(true);
            styleButton->setText("Dark theme");
        }
        else
        {
            Style::instance().setBaseQss(lightTheme);
            Style::instance().setColorTheme(Style::LightTheme);
            Style::instance().applyStyleSheet(true);
            styleButton->setText("Light theme");
        }
        testFrame->style()->unpolish(testFrame);
        testFrame->style()->polish(testFrame);
    });
    confl->addWidget(styleButton,i,0,1,2);

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Show dialog");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,
    [dialogFrame,testWidget](){

            bool newDialog=dialogFrame->openDialog();
            if (newDialog)
            {
                QObject::connect(
                    dialogFrame->dialog(),
                    &PasswordDialog::passwordEntered,
                    testWidget,
                    &FrameWithModalStatus::popupBusyWaiting
                );
            }
        }
    );

    auto hide=new QPushButton("Finish busy");
    bl->addWidget(hide);
    QObject::connect(hide,&QPushButton::clicked,testWidget,&FrameWithModalStatus::finish);

    auto cancellableButton=new QPushButton();
    cancellableButton->setText("Uncancellable");
    cancellableButton->setCheckable(true);
    QObject::connect(cancellableButton,&QPushButton::toggled,testWidget,[testWidget,cancellableButton](bool enable){
        if (enable)
        {
            cancellableButton->setText("Cancellable");
        }
        else
        {
            cancellableButton->setText("Uncancellable");
        }
        testWidget->setCancellableBusyWaiting(enable);
    });
    bl->addWidget(cancellableButton);

    auto statusType=new QComboBox();
    bl->addWidget(statusType);
    statusType->addItems({"Error","Warning","Information","Question"});

    auto message=new QLineEdit();
    message->setPlaceholderText("Enter status message");
    bl->addWidget(message);

    auto dialogButtons=new QPushButton("Dialog buttons");
    auto dialogButtonsMenu=new QMenu();
    QAction* acceptButton=new QAction("Accept");acceptButton->setCheckable(true);
    QAction* okButton=new QAction("OK");okButton->setCheckable(true);
    QAction* ignoreButton=new QAction("Ignore");ignoreButton->setCheckable(true);
    QAction* skipButton=new QAction("Skip");skipButton->setCheckable(true);
    QAction* retryButton=new QAction("Retry");retryButton->setCheckable(true);
    QAction* yesButton=new QAction("Yes");yesButton->setCheckable(true);
    QAction* noButton=new QAction("No");noButton->setCheckable(true);
    QAction* cancelButton=new QAction("Cancel");cancelButton->setCheckable(true);
    QAction* closeButton=new QAction("Close");closeButton->setCheckable(true);closeButton->setChecked(true);
    dialogButtonsMenu->addActions({okButton,acceptButton,yesButton,ignoreButton,skipButton,retryButton,noButton,cancelButton,closeButton});
    dialogButtons->setMenu(dialogButtonsMenu);
    bl->addWidget(dialogButtons);

    auto buttonsAlignment=new QComboBox();
    bl->addWidget(buttonsAlignment);
    buttonsAlignment->addItems({"Right","Center","Left"});

    auto showStatus=new QPushButton("Show status");
    QObject::connect(
        showStatus,
        &QPushButton::clicked,
        testWidget,
        [testWidget,message,statusType
         ,acceptButton,okButton,ignoreButton,cancelButton,closeButton
         ,skipButton,retryButton,yesButton,noButton,
        buttonsAlignment
        ]()
        {
            ButtonsStyle buttonStyle;
            if (buttonsAlignment->currentText()=="Left")
            {
                buttonStyle.alignment=Qt::AlignLeft;
            } else if (buttonsAlignment->currentText()=="Right")
            {
                buttonStyle.alignment=Qt::AlignRight;
            }
            else if (buttonsAlignment->currentText()=="Center")
            {
                buttonStyle.alignment=Qt::AlignCenter;
            }
            Style::instance().setDefaultButtonsStyle(buttonStyle);

            auto statusDialog=testWidget->statusDialog();
            std::vector<StatusDialog::ButtonConfig> buttons;
            if (okButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::OK);
            }
            if (acceptButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Accept);
            }
            if (yesButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Yes);
            }
            if (ignoreButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Ignore);
            }
            if (skipButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Skip);
            }
            if (retryButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Retry);
            }
            if (noButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::No);
            }
            if (cancelButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Cancel);
            }
            if (closeButton->isChecked())
            {
                buttons.push_back(StatusDialog::StandardButton::Close);
            }
            statusDialog->setButtons(buttons);

            auto text=message->text();

            StatusDialog::Type type=StatusDialog::Type::Error;
            if (statusType->currentText()=="Warning")
            {
                type=StatusDialog::Type::Warning;
                if (text.isEmpty())
                {
                    text="This is warning!";
                }
            }
            else if (statusType->currentText()=="Information")
            {
                type=StatusDialog::Type::Info;
                if (text.isEmpty())
                {
                    text="This is some information.";
                }
            }
            else if (statusType->currentText()=="Question")
            {
                type=StatusDialog::Type::Question;
                if (text.isEmpty())
                {
                    text="This is a question, isn't it?";
                }
            }
            else
            {
                if (text.isEmpty())
                {
                    text="This is error description.";
                }
            }

            testWidget->popupStatus(text,type);
        }
    );
    bl->addWidget(showStatus);

    testWidget->setMaxWidthPercent(width->value());
    testWidget->setMaxHeightPercent(height->value());
    testWidget->setAutoColor(false);

    dialogFrame->setMaxHeightPercent(height->value());
    testWidget->setMaxWidthPercent(width->value());
    dialogFrame->setAutoColor(false);

    QObject::connect(
        testWidget->statusDialog(),
        &StatusDialog::buttonClicked,
        testFrame,
        [testFrame](int id)
        {
            auto idT=static_cast<StatusDialog::StandardButton>(id);
            auto button=StatusDialog::standardButton(idT);

            if (idT!=StatusDialog::StandardButton::Close)
            {
                QString msg{"Clicked on button \"%1\""};
                msg=msg.arg(button.name);
                QMessageBox::information(testFrame,"Button clicked",msg);
            }
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Password Dialog Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
