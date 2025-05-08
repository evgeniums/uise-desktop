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

/** @file uise/test/alignedstretchingwidget/testalignedstretchingwidget.cpp
*
*  Test AlignedStretchingWidget.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QFrame>
#include <QMainWindow>
#include <QTextBrowser>
#include <QStyle>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/alignedstretchingwidget.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestAlignedStretchingWidget)

namespace {

constexpr static const int PlayStepPeriod=100;

struct SampleContainer
{
    QMainWindow* mainWindow=nullptr;
    AlignedStretchingWidget* wrapper=nullptr;
    QFrame* widget=nullptr;
    QMargins padding;

    std::function<int()> posX;
    std::function<int()> posY;
};

static void endTestCase(
        SampleContainer* container
    )
{
    container->mainWindow->hide();
    container->mainWindow->deleteLater();

    delete container;
    TestThread::instance()->continueTest();
}

void checkConfiguration(SampleContainer* container)
{
    UISE_TEST_CHECK_EQUAL(container->widget->x(),container->posX());
    UISE_TEST_CHECK_EQUAL(container->widget->y(),container->posY());

    QSize newSize(900,700);
    container->mainWindow->resize(newSize);
    QTimer::singleShot(PlayStepPeriod,container->widget,
        [container]
        {
            UISE_TEST_CHECK_EQUAL(container->wrapper->width(),container->mainWindow->width());
            UISE_TEST_CHECK_EQUAL(container->wrapper->height(),container->mainWindow->height());

            UISE_TEST_CHECK_EQUAL(container->widget->width(),container->widget->maximumWidth());
            UISE_TEST_CHECK_EQUAL(container->widget->height(),container->widget->maximumHeight());

            UISE_TEST_CHECK_EQUAL(container->widget->x(),container->posX());
            UISE_TEST_CHECK_EQUAL(container->widget->y(),container->posY());

            QSize newSize1(container->widget->maximumWidth()-90,container->widget->maximumHeight()-70);
            container->mainWindow->resize(newSize1);
            QTimer::singleShot(PlayStepPeriod,container->widget,
                [container,newSize1]
                {
                    UISE_TEST_CHECK_EQUAL(container->wrapper->width(),container->mainWindow->width());
                    UISE_TEST_CHECK_EQUAL(container->wrapper->height(),container->mainWindow->height());

                    UISE_TEST_CHECK_EQUAL(container->widget->width(),newSize1.width()-container->padding.left()-container->padding.right());
                    UISE_TEST_CHECK_EQUAL(container->widget->height(),newSize1.height()-container->padding.top()-container->padding.bottom());

                    UISE_TEST_CHECK_EQUAL(container->widget->x(),container->posX());
                    UISE_TEST_CHECK_EQUAL(container->widget->y(),container->posY());

                    QSize newSize2(container->widget->minimumWidth()-90,container->widget->minimumHeight()-70);
                    container->mainWindow->resize(newSize2);
                    QTimer::singleShot(PlayStepPeriod,container->widget,
                        [container]
                        {
                            UISE_TEST_CHECK_EQUAL(container->wrapper->width(),container->widget->minimumWidth()+container->padding.left()+container->padding.right());
                            UISE_TEST_CHECK_EQUAL(container->wrapper->height(),container->widget->minimumHeight()+container->padding.top()+container->padding.bottom());

                            UISE_TEST_CHECK_EQUAL(container->widget->width(),container->widget->minimumWidth());
                            UISE_TEST_CHECK_EQUAL(container->widget->height(),container->widget->minimumHeight());

                            UISE_TEST_CHECK_EQUAL(container->widget->x(),container->posX());
                            UISE_TEST_CHECK_EQUAL(container->widget->y(),container->posY());

                            QSize newSize3(container->widget->minimumWidth()+90,container->widget->minimumHeight()+70);
                            container->widget->setMinimumSize(newSize3);
                            container->wrapper->updateMinMaxSize();
                            QTimer::singleShot(PlayStepPeriod,container->widget,
                                [container,newSize3]
                                {
                                    UISE_TEST_CHECK_EQUAL(container->wrapper->width(),newSize3.width()+container->padding.left()+container->padding.right());
                                    UISE_TEST_CHECK_EQUAL(container->wrapper->height(),newSize3.height()+container->padding.top()+container->padding.bottom());

                                    UISE_TEST_CHECK_EQUAL(container->widget->width(),newSize3.width());
                                    UISE_TEST_CHECK_EQUAL(container->widget->height(),newSize3.height());

                                    UISE_TEST_CHECK_EQUAL(container->widget->x(),container->posX());
                                    UISE_TEST_CHECK_EQUAL(container->widget->y(),container->posY());

                                    endTestCase(container);
                                }
                            );
                        }
                    );
                }
            );
        }
    );
}

static void runTestCase(
        Qt::Orientation orientation,
        Qt::Alignment alignment,
        SampleContainer* container
    )
{
    auto init=[orientation,alignment,container]()
    {
        container->mainWindow=new QMainWindow();

        container->wrapper=new AlignedStretchingWidget();
        container->wrapper->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

        container->widget=new QTextBrowser();
        container->widget->setMaximumWidth(500);
        container->widget->setMaximumHeight(300);

        container->widget->setMinimumWidth(300);
        container->widget->setMinimumHeight(200);

        container->wrapper->setWidget(container->widget,orientation,alignment);
        container->mainWindow->setCentralWidget(container->wrapper);

        container->padding.setLeft(10);
        container->padding.setTop(8);
        container->padding.setRight(6);
        container->padding.setBottom(4);

        QString qss=QString(
            "\n uise--desktop--AlignedStretchingWidget {padding-left: %1px; padding-top: %2px; padding-right: %3px; padding-bottom: %4px; background: #0000FF;} \n QTextBrowser {background: yellow}"
        ).arg(container->padding.left())
         .arg(container->padding.top())
         .arg(container->padding.right())
         .arg(container->padding.bottom());

        container->mainWindow->setStyleSheet(qss);
        container->mainWindow->style()->unpolish(container->mainWindow);
        container->mainWindow->style()->polish(container->mainWindow);
        container->wrapper->updateMinMaxSize();

        int width=800;
        int height=600;

        container->mainWindow->resize(width,height);
        container->mainWindow->show();
        container->mainWindow->raise();

        auto checkInit=[container,width,height]()
        {
            UISE_TEST_CHECK_EQUAL(container->mainWindow->width(),width);
            UISE_TEST_CHECK_EQUAL(container->mainWindow->height(),height);

            UISE_TEST_CHECK_EQUAL(container->wrapper->width(),width);
            UISE_TEST_CHECK_EQUAL(container->wrapper->height(),height);

            UISE_TEST_CHECK_EQUAL(container->widget->width(),container->widget->maximumWidth());
            UISE_TEST_CHECK_EQUAL(container->widget->height(),container->widget->maximumHeight());

            checkConfiguration(container);
        };

        QTimer::singleShot(PlayStepPeriod,container->widget,checkInit);
    };

    TestThread::instance()->postGuiThread(init);
    auto ret=TestThread::instance()->execTest(15000);
    UISE_TEST_CHECK(ret);
}

SampleContainer* alignLeftTop()
{
    auto container=new SampleContainer();
    container->posX=[container]()
    {
        return container->padding.left();
    };
    container->posY=[container]()
    {
        return container->padding.top();
    };
    return container;
}

SampleContainer* alignRightBottom()
{
    auto container=new SampleContainer();
    container->posX=[container]()
    {
        auto diff=container->wrapper->width()-container->widget->width();
        auto pos=diff-container->padding.right();
        return pos;
    };
    container->posY=[container]()
    {
        auto diff=container->wrapper->height()-container->widget->height();
        auto pos=diff-container->padding.bottom();
        return pos;
    };
    return container;
}

SampleContainer* alignCenter()
{
    auto container=new SampleContainer();
    container->posX=[container]()
    {
        auto diff=container->wrapper->width()-container->widget->width()
                -container->padding.left()
                -container->padding.right()
                ;
        auto pos=diff/2+container->padding.left();
        return pos;
    };
    container->posY=[container]()
    {
        auto diff=container->wrapper->height()-container->widget->height()
                -container->padding.top()
                -container->padding.bottom()
                ;
        auto pos=diff/2+container->padding.top();
        return pos;
    };
    return container;
}

}

BOOST_AUTO_TEST_CASE(TestHorizontalLeftTop)
{
    runTestCase(Qt::Horizontal, Qt::AlignLeft | Qt::AlignTop, alignLeftTop());
}

BOOST_AUTO_TEST_CASE(TestHorizontalCenter)
{
    runTestCase(Qt::Horizontal, Qt::AlignCenter, alignCenter());
}

BOOST_AUTO_TEST_CASE(TestHorizontalRightBottom)
{
    runTestCase(Qt::Horizontal, Qt::AlignRight | Qt::AlignBottom, alignRightBottom());
}

BOOST_AUTO_TEST_CASE(TestVerticalTopLeft)
{
    runTestCase(Qt::Vertical, Qt::AlignLeft | Qt::AlignTop, alignLeftTop());
}

BOOST_AUTO_TEST_CASE(TestVerticalCenter)
{
    runTestCase(Qt::Vertical, Qt::AlignCenter, alignCenter());
}

BOOST_AUTO_TEST_CASE(TestVerticalBottomRight)
{
    runTestCase(Qt::Vertical, Qt::AlignRight | Qt::AlignBottom, alignRightBottom());
}

BOOST_AUTO_TEST_SUITE_END()
