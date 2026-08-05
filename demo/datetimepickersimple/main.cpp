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

/** @file demo/datetimepickersimple/main.cpp
*
*  Demo application of DateTimePickerSimple.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/datetimepickersimple.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);

    auto pf=new QFrame(mainFrame);
    auto pl=Layout::vertical(pf);
    l->addWidget(pf);

    pl->addWidget(new QLabel("<->"));

    auto picker1=new DateTimePickerSimple(pf);
    picker1->setVisible(false);
    pl->addWidget(picker1);
    auto picker2=new DateTimePickerSimple(pf);
    picker2->setVisible(true);
    pl->addWidget(picker2);

    auto log=new QTextEdit();
    log->setReadOnly(true);
    log->setMinimumHeight(120);
    l->addWidget(log,1);

    QObject::connect(picker1,&DateTimePickerSimple::dateChanged,[log](const QDate& date)
    {
        log->append(QString("picker1 dateChanged: %1").arg(date.toString(Qt::ISODate)));
        log->ensureCursorVisible();
    });
    QObject::connect(picker2,&DateTimePickerSimple::dateChanged,[log](const QDate& date)
    {
     log->append(QString("picker2 dateChanged: %1").arg(date.toString(Qt::ISODate)));
     log->ensureCursorVisible();
    });

    auto todayBtn=new QPushButton("Set today");
    l->addWidget(todayBtn);
    QObject::connect(todayBtn,&QPushButton::clicked,[picker1,picker2]()
    {
        picker1->setDate(QDate::currentDate());
        picker2->setDate(QDate::currentDate());
    });

    log->append(QString("initial picker1 date: %1").arg(picker1->date().toString(Qt::ISODate)));
    log->append(QString("initial picker1 date: %1").arg(picker2->date().toString(Qt::ISODate)));

    w.setCentralWidget(mainFrame);
    w.resize(500,600);
    w.setWindowTitle("DateTimePickerSimple Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
