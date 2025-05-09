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

/** @file demo/drawer/main.cpp
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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/drawer.hpp>

#include <fwlvtestwidget.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    auto l = Layout::vertical(mainFrame,false);

    auto topFrame=new QFrame();
    l->addWidget(topFrame);
    auto tl=Layout::horizontal(topFrame,false);
    auto testFrame=new QFrame();
    testFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tl->addWidget(testFrame);
    auto tfl=Layout::horizontal(testFrame,false);
    auto testWidget=new FrameWithDrawer(testFrame);
    tfl->addWidget(testWidget);
    auto wl=Layout::vertical(testWidget,false);
    auto editor1=new QTextEdit();
    wl->addWidget(editor1);
    editor1->setText("hkjhas cljhglidsc lhgluisadb liuydfi liuylsiaudfyis fsadiuyaidfy;iu");

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto alpha=new QSpinBox();
    alpha->setRange(0,255);
    alpha->setValue(FrameWithDrawer::DefaultAlpha);
    confl->addWidget(new QLabel("Alpha"),i,0);
    confl->addWidget(alpha,i,1);
    QObject::connect(alpha,&QSpinBox::valueChanged,testWidget,&FrameWithDrawer::setDrawerAlpha);
    i++;

    auto size=new QSpinBox();
    size->setRange(0,100);
    size->setValue(FrameWithDrawer::DefaultSizePercent);
    confl->addWidget(new QLabel("Size percent"),i,0);
    confl->addWidget(size,i,1);
    QObject::connect(size,&QSpinBox::valueChanged,testWidget,&FrameWithDrawer::setSizePercent);
    i++;

    auto duration=new QSpinBox();
    duration->setRange(0,5000);
    duration->setValue(FrameWithDrawer::DefaultSlideDurationMs);
    confl->addWidget(new QLabel("Duration, ms"),i,0);
    confl->addWidget(duration,i,1);
    QObject::connect(duration,&QSpinBox::valueChanged,testWidget,&FrameWithDrawer::setSlideDurationMs);
    i++;

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto setVisible=new QPushButton();
    setVisible->setText("Open drawer");
    setVisible->setCheckable(true);
    QObject::connect(setVisible,&QPushButton::toggled,testWidget,[testWidget,setVisible](bool enable){
        testWidget->setDrawerVisible(enable);
        if (enable)
        {
            setVisible->setText("Close drawer");
        }
        else
        {
            setVisible->setText("Open drawer");
        }
    });
    bl->addWidget(setVisible);

    QObject::connect(testWidget,&FrameWithDrawer::drawerVisibilityChanged,testWidget,
        [setVisible](bool visible)
        {
            setVisible->blockSignals(true);
            setVisible->setChecked(visible);
            if (visible)
            {
                setVisible->setText("Close drawer");
            }
            else
            {
                setVisible->setText("Open drawer");
            }
            setVisible->blockSignals(false);
        }
    );

    auto show=new QPushButton("Show drawer");
    bl->addWidget(show);
    QObject::connect(show,&QPushButton::clicked,testWidget,
        [testWidget,setVisible]()
        {
            testWidget->openDrawer();
            setVisible->blockSignals(true);
            setVisible->setChecked(true);
            setVisible->setText("Close drawer");
            setVisible->blockSignals(false);
        }
    );

    auto hide=new QPushButton("Hide drawer");
    bl->addWidget(hide);
    QObject::connect(hide,&QPushButton::clicked,testWidget,
         [testWidget,setVisible]()
         {
             testWidget->closeDrawer();
             setVisible->blockSignals(true);
             setVisible->setChecked(false);
             setVisible->setText("Open drawer");
             setVisible->blockSignals(false);
         }
    );

    QString darkTheme="*{font-size: 20px;} \n *{color: white;background-color:black;} QPushButton{border: 1px solid white;}";
    QString lightTheme="*{font-size: 20px;} \n *{color: black;background-color:lightgray;} QPushButton{border: 1px solid black;}";
    auto styleButton=new QPushButton();
    styleButton->setText("Dark theme");
    styleButton->setCheckable(true);
    QObject::connect(styleButton,&QPushButton::toggled,testFrame,[&darkTheme,&lightTheme,testFrame,styleButton](bool enable){
        if (enable)
        {
            testFrame->setStyleSheet(darkTheme);
            styleButton->setText("Light theme");
        }
        else
        {
            testFrame->setStyleSheet(lightTheme);
            styleButton->setText("Dark theme");
        }
        testFrame->style()->unpolish(testFrame);
        testFrame->style()->polish(testFrame);
    });
    bl->addWidget(styleButton);

    auto createDrawer=[testWidget](Qt::Edge edge)
    {
        testWidget->setDrawerEdge(edge);
        auto vertical=testWidget->isVertical();

        auto widget=new FlwListType();
        testWidget->setDrawerWidget(widget);
        if (vertical)
        {
            widget->setOrientation(Qt::Horizontal);
        }
        else
        {
            widget->setOrientation(Qt::Vertical);
        }

        std::vector<HelloWorldItemWrapper> newItems;
        for (size_t i=0;i<5;i++)
        {
            newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,i)));
        }
        widget->loadItems(newItems);
    };

    auto orientationList=new QComboBox();
    orientationList->addItems(QStringList{"Left","Top","Right","Bottom"});
    QObject::connect(orientationList,&QComboBox::currentTextChanged,testFrame,[testWidget,createDrawer](const QString &text)
    {
        bool wasVisible=testWidget->isDrawerVisible();
        if (wasVisible)
        {
            testWidget->closeDrawer(true);
        }

        auto edge=Qt::LeftEdge;
        if (text=="Top")
        {
            edge=Qt::TopEdge;
        }
        else if (text=="Right")
        {
            edge=Qt::RightEdge;
        }
        else if (text=="Bottom")
        {
            edge=Qt::BottomEdge;
        }

        createDrawer(edge);

        if (wasVisible)
        {
            testWidget->openDrawer();
        }
    });
    bl->addWidget(orientationList);
    createDrawer(Qt::LeftEdge);

    testFrame->setStyleSheet(lightTheme);
    testWidget->setDrawerAlpha(alpha->value());
    testWidget->setSizePercent(size->value());
    testWidget->setSlideDurationMs(duration->value());

    w.setCentralWidget(mainFrame);
    w.resize(500,400);
    w.setWindowTitle("Drawer Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
