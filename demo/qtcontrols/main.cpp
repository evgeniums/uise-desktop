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

/** @file demo/qtcontrols/main.cpp
*
*  Demo application of standard Qt controls that are not redefined by a
*  dedicated uise widget class (QComboBox, QSpinBox, QDoubleSpinBox,
*  QDateEdit/QTimeEdit/QDateTimeEdit, QTextEdit, QTabWidget, item views,
*  menus, ...) -- their whole appearance comes from the bundled
*  reset.qss/global.qss cascading over the native Qt widgets. QCheckBox and
*  QRadioButton are intentionally excluded: uise redefines those as
*  CheckBox/RadioBox, which already have their own demo (see demo/checkbox).
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QFileDialog>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;

    // ---- menu bar / status bar: also styled purely via global.qss/reset.qss ----
    auto fileMenu=w.menuBar()->addMenu("File");
    fileMenu->addAction("Open...",&w,
        [&w]()
        {
            QFileDialog::getOpenFileName(&w,"Open File");
        });
    fileMenu->addSeparator();
    fileMenu->addAction("Quit",&app,&QApplication::quit);
    auto viewMenu=w.menuBar()->addMenu("View");
    viewMenu->addAction("Refresh");
    viewMenu->addAction("Disabled action")->setEnabled(false);

    w.statusBar()->showMessage("Ready");

    auto scrollArea=new QScrollArea();
    scrollArea->setWidgetResizable(true);

    auto mainFrame=new QFrame(scrollArea);
    mainFrame->setObjectName("mainFrame");
    auto ml=Layout::vertical(mainFrame,false);
    ml->setContentsMargins(12,12,12,12);
    ml->setSpacing(12);
    scrollArea->setWidget(mainFrame);

    // ---- text and value editors ----
    auto editorsGroup=new QGroupBox("Text and value editors");
    auto eg=Layout::grid(editorsGroup,false);
    eg->setColumnStretch(1,1);
    eg->setSpacing(8);

    int row=0;
    auto addRow=[&eg,&row](const QString& label, QWidget* widget)
    {
        eg->addWidget(new QLabel(label),row,0);
        eg->addWidget(widget,row,1);
        row++;
    };

    addRow("QLabel",new QLabel("Sample label text"));

    auto lineEdit=new QLineEdit();
    lineEdit->setPlaceholderText("Type something...");
    lineEdit->setClearButtonEnabled(true);
    addRow("QLineEdit",lineEdit);

    auto textEdit=new QTextEdit();
    textEdit->setPlainText("Multi-line\ntext edit.");
    textEdit->setFixedHeight(70);
    addRow("QTextEdit",textEdit);

    auto combo=new QComboBox();
    combo->addItems({"One","Two","Three","Four","Five"});
    addRow("QComboBox",combo);

    auto editableCombo=new QComboBox();
    editableCombo->setEditable(true);
    editableCombo->addItems({"Alpha","Beta","Gamma"});
    addRow("QComboBox (editable)",editableCombo);

    auto spin=new QSpinBox();
    spin->setRange(0,1000);
    spin->setValue(42);
    addRow("QSpinBox",spin);

    auto doubleSpin=new QDoubleSpinBox();
    doubleSpin->setRange(-100.0,100.0);
    doubleSpin->setDecimals(2);
    doubleSpin->setValue(3.14);
    addRow("QDoubleSpinBox",doubleSpin);

    auto dateEdit=new QDateEdit(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    addRow("QDateEdit",dateEdit);

    auto timeEdit=new QTimeEdit(QTime::currentTime());
    addRow("QTimeEdit",timeEdit);

    auto dateTimeEdit=new QDateTimeEdit(QDateTime::currentDateTime());
    dateTimeEdit->setCalendarPopup(true);
    addRow("QDateTimeEdit",dateTimeEdit);

    auto pushButton=new QPushButton("Native QPushButton");
    addRow("QPushButton",pushButton);

    auto disabledButton=new QPushButton("Disabled");
    disabledButton->setEnabled(false);
    addRow("QPushButton (disabled)",disabledButton);

    ml->addWidget(editorsGroup);

    // ---- item views, tabs and scroll bars ----
    auto viewsGroup=new QGroupBox("Item views, tabs and scroll bars");
    auto vgl=Layout::vertical(viewsGroup,false);
    vgl->setSpacing(8);

    auto splitter=new QSplitter(Qt::Horizontal);

    auto listWidget=new QListWidget();
    for (int i=1;i<=30;++i)
    {
        listWidget->addItem(QString("List item %1").arg(i));
    }
    splitter->addWidget(listWidget);

    auto treeWidget=new QTreeWidget();
    treeWidget->setHeaderLabels({"Name","Value"});
    for (int i=1;i<=10;++i)
    {
        auto item=new QTreeWidgetItem({QString("Node %1").arg(i),QString::number(i*10)});
        for (int j=1;j<=3;++j)
        {
            item->addChild(new QTreeWidgetItem({QString("Child %1.%2").arg(i).arg(j),QString::number(j)}));
        }
        treeWidget->addTopLevelItem(item);
    }
    splitter->addWidget(treeWidget);
    splitter->setMinimumHeight(180);
    vgl->addWidget(splitter);

    auto tabs=new QTabWidget();
    auto tab1=new QLabel("Content of the first tab.");
    tab1->setAlignment(Qt::AlignCenter);
    tabs->addTab(tab1,"Tab 1");
    auto tab2=new QLabel("Content of the second tab.");
    tab2->setAlignment(Qt::AlignCenter);
    tabs->addTab(tab2,"Tab 2");
    auto tab3=new QLabel("Disabled tab.");
    tabs->addTab(tab3,"Tab 3");
    tabs->setTabEnabled(2,false);
    tabs->setMinimumHeight(100);
    vgl->addWidget(tabs);

    ml->addWidget(viewsGroup);
    ml->addStretch(1);

    // ---- theme toggle ----
    auto themeGroup=new QGroupBox("Theme");
    auto thl=Layout::horizontal(themeGroup,false);
    auto themeButton=new QPushButton();
    themeButton->setCheckable(true);
    const bool startDark=Style::instance().checkDarkTheme();
    themeButton->setChecked(startDark);
    themeButton->setText(startDark ? "Dark" : "Light");
    QObject::connect(themeButton,&QPushButton::toggled,&w,
        [&w,themeButton](bool dark)
        {
            Style::instance().setColorTheme(dark ? Style::DarkTheme : Style::LightTheme);
            themeButton->setText(dark ? "Dark" : "Light");
            Style::instance().applyStyleSheet(true);
            Style::repolishRecursive(&w);
        });
    thl->addWidget(themeButton);
    ml->addWidget(themeGroup);

    w.setCentralWidget(scrollArea);
    w.resize(760,780);
    w.setWindowTitle("Standard Qt Controls Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
