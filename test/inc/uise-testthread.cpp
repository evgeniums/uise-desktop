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

/** @file uise/test/inc/uise-testtread.cpp
*
*  Test thread.
*
*/

/****************************************************************************/

#include <atomic>

#include <QDebug>
#include <QMetaObject>
#include <QApplication>
#include <QEventLoop>
#include <QMutex>
#include <QWaitCondition>

#include <uise/test/uise-testthread.hpp>

UISE_TEST_NAMESPACE_BEGIN

class TestThread_p
{
    public:

        TestThread_p() : started(false)
        {}

        std::atomic<bool> started;
        QMutex mutex;
        QWaitCondition wc;

        QMutex testMutex;
};

static TestThread* Instance=nullptr;

//--------------------------------------------------------------------------
TestThread::TestThread(
    ) : pimpl(std::make_unique<TestThread_p>())
{
    setObjectName("Test Thread");

    start();
    while(!pimpl->started.load())
    {}

    moveToThread(this);
}

//--------------------------------------------------------------------------
TestThread::~TestThread()
{
    pimpl->wc.wakeAll();
    quit();
    if (!wait(15000))
    {
        qWarning()<<"Failed to stop test thread";
    }
}

//--------------------------------------------------------------------------
void TestThread::run()
{
    pimpl->started.store(true);
    exec();

    moveToThread(qApp->thread());
}

//--------------------------------------------------------------------------
bool TestThread::execTest(uint32_t timeout)
{
    QMutexLocker l(&pimpl->mutex);
    if (timeout!=0)
    {
        return pimpl->wc.wait(&pimpl->mutex,timeout);
    }
    return pimpl->wc.wait(&pimpl->mutex);
}

//--------------------------------------------------------------------------
void TestThread::continueTest()
{
    pimpl->wc.wakeOne();
}

//--------------------------------------------------------------------------
void TestThread::postTestThread(std::function<void ()> handler)
{
    continueTest();
    QMetaObject::invokeMethod(this,std::move(handler));
}

//--------------------------------------------------------------------------
void TestThread::postGuiThread(std::function<void ()> handler)
{
    QMetaObject::invokeMethod(qApp->thread(),std::move(handler));
}

//--------------------------------------------------------------------------
void TestThread::execGuiThread(std::function<void ()> handler)
{
    QMetaObject::invokeMethod(qApp->thread(),std::move(handler),Qt::BlockingQueuedConnection);
}

//--------------------------------------------------------------------------
TestThread* TestThread::instance()
{
    if (!Instance)
    {
        Instance=new TestThread();
    }
    return Instance;
}

//--------------------------------------------------------------------------
void TestThread::free()
{
    delete Instance;
    Instance=nullptr;
}

//--------------------------------------------------------------------------
QMutex& TestThread::testMutex() noexcept
{
    return pimpl->testMutex;
}

//--------------------------------------------------------------------------

UISE_TEST_NAMESPACE_END
