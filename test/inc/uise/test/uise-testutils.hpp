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

/** @file uise/test/inc/testutils.hpp
*
*  Utils for testing.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_UTILS_HPP
#define UISE_DESKTOP_TEST_UTILS_HPP

#include <boost/test/unit_test.hpp>

#include <QFrame>
#include <QMainWindow>
#include <QPointer>
#include <QApplication>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

UISE_TEST_NAMESPACE_BEGIN

template <typename T>
struct TestWidgetContainer
{
    QPointer<QMainWindow> mainWindow;
    QFrame* content=nullptr;
    T* testWidget=nullptr;
    QSize windowSize;

    inline static int PlayStepPeriod=200;

    void destroy()
    {
        if (mainWindow)
        {
            UISE_TEST_MESSAGE("TestWidgetContainer destroy begin");

            mainWindow->hide();
            mainWindow->deleteLater();

            UISE_TEST_MESSAGE("TestWidgetContainer destroy end");
        }
    }

    TestWidgetContainer(int width=800, int height=600)
        :windowSize(width,height)
    {
        UISE_TEST_MESSAGE("TestWidgetContainer constructor");
    }

    TestWidgetContainer(const TestWidgetContainer&)=delete;
    TestWidgetContainer& operator=(const TestWidgetContainer&)=delete;
    TestWidgetContainer(TestWidgetContainer&&)=delete;
    TestWidgetContainer& operator=(TestWidgetContainer&&)=delete;

    ~TestWidgetContainer()
    {}

    static void runStep(std::shared_ptr<TestWidgetContainer<T>> container,
                 std::vector<std::function<void (std::shared_ptr<TestWidgetContainer<T>>)>> steps,
                 size_t index)
    {
        if (index>=steps.size())
        {
            container->destroy();
            TestThread::instance()->continueTest();
            return;
        }
        steps[index](container);
        QTimer::singleShot(PlayStepPeriod,container->content,[container,steps,index](){
            runStep(container,steps,index+1);
        });
    }

    static void runTestCase(
            std::vector<std::function<void (std::shared_ptr<TestWidgetContainer<T>>)>> steps
        )
    {
        auto container=std::make_shared<TestWidgetContainer<T>>();
        auto run=[container,steps]()
        {
            runStep(container,steps,0);
        };

        TestThread::instance()->postGuiThread(run);
        auto ret=TestThread::instance()->execTest(15000);
        UISE_TEST_CHECK(ret);
    }

    static void beginTestCase(std::shared_ptr<TestWidgetContainer<T>> container, T* widget, const QString& testName)
    {
        UISE_TEST_CHECK_EQUAL(QThread::currentThread(),qApp->thread());

        container->testWidget=widget;
        container->mainWindow=new QMainWindow();
        container->content=new QFrame(container->mainWindow);
        auto l = UISE_DESKTOP_NAMESPACE::Layout::vertical(container->content);

        container->mainWindow->setCentralWidget(container->content);
        container->mainWindow->resize(container->windowSize);
        container->mainWindow->setWindowTitle(testName);
        container->mainWindow->show();

        l->addWidget(widget);
        l->addStretch(1);
    }
};

UISE_TEST_NAMESPACE_END

#endif
