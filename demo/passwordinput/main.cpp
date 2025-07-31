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

/** @file demo/passwordinput/main.cpp
*
*  Demo application of PasswordInput.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QSpinBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/passwordinput.hpp>
#include <uise/desktop/passworddialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto mainL=Layout::vertical(mainFrame);

    auto inputFrame=new QFrame();
    mainL->addWidget(inputFrame);
    auto l = Layout::grid(inputFrame,false);
    l->addWidget(new QLabel("Enter password:"),0,0);
    auto passwordInput=new PasswordInput();
    l->addWidget(passwordInput,0,1);
    l->addWidget(new QLabel("Result:"),1,0);
    auto displayLabel=new QLabel();
    l->addWidget(displayLabel,1,1);
    QObject::connect(
        passwordInput,
        &PasswordInput::returnPressed,
        displayLabel,
        [passwordInput,displayLabel]()
        {
            displayLabel->setText(passwordInput->editor()->text());
        }
    );

    auto showDialog=new QPushButton("Show dialog");
    mainL->addWidget(showDialog);
    auto showError=new QPushButton("Show error");
    showError->setVisible(false);
    mainL->addWidget(showError);
    QObject::connect(
        showDialog,
        &QPushButton::clicked,
        mainFrame,
        [mainL,mainFrame,displayLabel,showDialog,showError]()
        {
            showDialog->setEnabled(false);
            AbstractPasswordDialog* dialog=new PasswordDialog();
            dialog->construct();
            mainL->insertWidget(mainL->count()-1,dialog);
            QObject::connect(
                dialog,
                &PasswordDialog::passwordEntered,
                mainFrame,
                [displayLabel,dialog]()
                {
                    displayLabel->setText(dialog->password());
                }
            );
            QObject::connect(
                dialog,
                &PasswordDialog::closeRequested,
                mainFrame,
                [dialog,showDialog,showError]()
                {
                    destroyWidget(dialog);
                    showDialog->setEnabled(true);
                    showError->setVisible(false);
                }
            );            

            showError->setVisible(true);
            QObject::connect(
                showError,
                &QPushButton::clicked,
                dialog,
                [dialog]()
                {
                    dialog->setError("Some error regarding entered password");
                }
            );

            dialog->setDialogFocus();
        }
    );    
    mainL->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(400,400);
    w.setWindowTitle("Password Input Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
