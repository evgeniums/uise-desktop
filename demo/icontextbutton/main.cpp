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

/** @file demo/icontextbutton/avatar.cpp
*
*  Demo application of IconTextButton.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);

    auto buttonsFrame=new QFrame(mainFrame);
    auto buttonsLayout=Layout::grid(buttonsFrame,false);
    buttonsLayout->setColumnStretch(2,1);
    mainFrame->setWidget(buttonsFrame);

    buttonsFrame->setStyleSheet(""
                                "uise--RoundedImage {min-width:18px;max-width:18px;min-height:18px;max-height:18px;}"
                                "uise--IconTextButton QLabel {color:#999999}"
                                "uise--IconTextButton QLabel[hovered=\"true\"] {color:#FFFFFF}"
                                "uise--IconTextButton QLabel[checked=\"true\"][hovered=\"false\"] {color:#D8D8D8}"
                                "uise--IconTextButton QLabel[checked=\"true\"][hovered=\"true\"] {color:#E8E8E8}"
                                "");

    int row=0;

    auto addButton=[buttonsFrame,buttonsLayout,&row](IconTextButton::IconPosition iconPosition, bool checkable, const QString& text)
    {
        auto button=new IconTextButton(buttonsFrame);
        button->setIconPosition(iconPosition);

        auto icon=Style::instance().svgIconLocator().icon("ImageEditor::brush");
        button->setSvgIcon(icon);
        button->setText("Button text");

        auto target=new QLineEdit(buttonsFrame);
        target->setClearButtonEnabled(true);
        if (checkable)
        {
            button->setCheckable(true);
            QObject::connect(
                button,
                &IconTextButton::toggled,
                target,
                [target,row](bool checked)
                {
                    target->setText(QString("Toggled %1 %2").arg(row+1).arg(checked));
                }
            );
        }
        else
        {
            QObject::connect(
                button,
                &IconTextButton::clicked,
                target,
                [target,row]()
                {
                    target->setText(QString("Clicked %1").arg(row+1));
                }
            );
        }

        buttonsLayout->addWidget(new QLabel(text),row,0);
        buttonsLayout->addWidget(button,row,1,Qt::AlignLeft);
        buttonsLayout->addWidget(target,row,2);

        row++;
    };

    addButton(IconTextButton::IconPosition::BeforeText,false,"Icon before text");
    addButton(IconTextButton::IconPosition::AfterText,false,"Icon after text");
    addButton(IconTextButton::IconPosition::AboveText,false,"Icon above text");
    addButton(IconTextButton::IconPosition::BelowText,false,"Icon below text");

    addButton(IconTextButton::IconPosition::BeforeText,true,"Checkable icon before text");
    addButton(IconTextButton::IconPosition::AfterText,true,"Checkable icon after text");
    addButton(IconTextButton::IconPosition::AboveText,true,"Checkable icon above text");
    addButton(IconTextButton::IconPosition::BelowText,true,"Checkable icon below text");

    w.setCentralWidget(mainFrame);
    w.resize(600,800);
    w.setWindowTitle("Avatar Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
