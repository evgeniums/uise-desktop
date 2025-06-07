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

/** @file demo/modalpopup/main.cpp
*
*  Demo application of BusyWaiting.
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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/framewithmodalstatus.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    Style::instance().setColorTheme(Style::LightTheme);
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
    tfl->addWidget(testWidget);
    auto wl=Layout::vertical(testWidget,false);
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
    i++;

    auto width=new QSpinBox();
    width->setRange(0,100);
    width->setValue(FrameWithModalPopup::DefaultMaxWidthPercent);
    confl->addWidget(new QLabel("Width percent"),i,0);
    confl->addWidget(width,i,1);
    QObject::connect(width,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxWidthPercent);
    i++;

    QString darkTheme=""
                        "QTextEdit,QLineEdit {background-color: black;color:white;} \n uise--FrameWithModalPopup {background-color: black;}"
                        "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton{color: #CCCCCC;border: 1px solid #999999; background-color: #444444; border-radius: 4px; margin:4px; padding: 4px 16px;}\n"
                        "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton:hover{color: #EEEEEE;border: 1px solid #EEEEEE;}\n"
                        "uise--ModalPopup {background-color: rgba(50,50,50,200);}"
                        "uise--StatusDialog {background-color: #222222; border: 1px solid #999999; border-radius: 2px;}"
                        "uise--StatusDialog #statusFrame {padding: 8px 8px 0x 8px;}"
                        "uise--StatusDialog #buttonsFrame {padding: 4px;}"
                        "uise--StatusDialog #statusFrame QLabel {color: #F0F0F0;}"
                        "uise--StatusDialog #statusFrame,#titleFrame uise--PushButton QPushButton {border:none;background-color:transparent;}"
                        "uise--StatusDialog #statusFrame uise--PushButton QPushButton {icon-size:32px;}"
                        "uise--StatusDialog #statusFrame uise--PushButton {padding: 12px;}"
                        "uise--StatusDialog #buttonsFrame uise--PushButton QPushButton {background-color:transparent;}"
                        "uise--StatusDialog #titleFrame {border-bottom: 1px solid #999999;background-color: #888888;padding: 4px 0;}"
                        "uise--StatusDialog #titleFrame QLabel {color:#FFFFFF;}"
                        "uise--StatusDialog #titleFrame #placeHolder {min-width:14px;max-width:14px;}"
                        "uise--StatusDialog #titleFrame uise--PushButton QPushButton {padding:0; margin:0;icon-size:14px;}"
                        "";
    QString lightTheme=""
                         "QTextEdit,QLineEdit {background-color: white;color:black;} \n uise--FrameWithModalPopup {background-color: lightgrey;}"
                         "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton{color: #555555;background-color: #CCCCCC;border: 1px solid #999999; border-radius: 4px; margin:4px; padding: 2px 12px;}\n"
                         "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton:hover{color: #444444;border: 1px solid #444444;}\n"
                         "uise--ModalPopup {background-color: rgba(200,200,200,200);}"
                         "uise--StatusDialog {background-color: #F0F0F0; border: 1px solid #999999; border-radius: 2px;}"
                         "uise--StatusDialog #statusFrame {padding: 8px 8px 0x 8px;}"
                         "uise--StatusDialog #buttonsFrame {padding: 4px;}"
                         "uise--StatusDialog #statusFrame QLabel {color: #444444;}"
                         "uise--StatusDialog #statusFrame,#titleFrame uise--PushButton QPushButton {border:none;background-color:transparent;}"
                         "uise--StatusDialog #statusFrame uise--PushButton QPushButton {icon-size:32px;}"
                         "uise--StatusDialog #statusFrame uise--PushButton {padding: 12px;}"
                         "uise--StatusDialog #buttonsFrame uise--PushButton QPushButton {background-color:transparent;}"
                         "uise--StatusDialog #titleFrame {border-bottom: 1px solid #999999;background-color: #888888;padding: 4px 0;}"
                         "uise--StatusDialog #titleFrame QLabel {color:#FFFFFF;}"
                         "uise--StatusDialog #titleFrame #placeHolder {min-width:14px;max-width:14px;}"
                         "uise--StatusDialog #titleFrame uise--PushButton QPushButton {padding:0; margin:0;icon-size:14px;}"
                         "";

    auto styleButton=new QPushButton();
    styleButton->setText("Dark theme");
    styleButton->setCheckable(true);
    QObject::connect(styleButton,&QPushButton::toggled,testFrame,[&darkTheme,&lightTheme,testFrame,styleButton](bool enable){
        if (enable)
        {
            Style::instance().setColorTheme(Style::DarkTheme);
            Style::instance().applyStyleSheet();
            testFrame->setStyleSheet(darkTheme);
            styleButton->setText("Light theme");
        }
        else
        {
            Style::instance().setColorTheme(Style::LightTheme);
            Style::instance().applyStyleSheet();
            testFrame->setStyleSheet(lightTheme);
            styleButton->setText("Dark theme");
        }
        testFrame->style()->unpolish(testFrame);
        testFrame->style()->polish(testFrame);
    });
    confl->addWidget(styleButton,i,0,1,2);

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Show busy");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,
    [testWidget](){
            testWidget->popupBusyWaiting();
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
    auto showStatus=new QPushButton("Show status");
    QObject::connect(
        showStatus,
        &QPushButton::clicked,
        testWidget,
        [testWidget,message,statusType]()
        {
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

    testFrame->setStyleSheet(lightTheme);

    testWidget->setMaxWidthPercent(width->value());
    testWidget->setMaxHeightPercent(height->value());
    testWidget->setAutoColor(false);

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Modal popup Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
