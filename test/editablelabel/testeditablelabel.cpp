/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/alignedstretchingwidget/testeditablelabel.cpp
*
*  Test EditableLabel.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QFrame>
#include <QMainWindow>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/editablelabel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestEditableLabel)

namespace {

constexpr static const int PlayStepPeriod=100;

struct SampleContainer
{
    QMainWindow* mainWindow=nullptr;
    QFrame* content=nullptr;
    EditableLabel* label=nullptr;
};

void endTestCase(
        SampleContainer* container
    )
{
    container->mainWindow->hide();
    container->mainWindow->deleteLater();

    delete container;
    TestThread::instance()->continueTest();
}

void runTestCase(
        std::function<void (SampleContainer*)> init,
        std::function<void (SampleContainer*)> action,
        std::function<void (SampleContainer*)> check
    )
{
    auto container=new SampleContainer();
    auto run=[container,init,action,check]()
    {
        init(container);
        QTimer::singleShot(PlayStepPeriod,container->content,[action,container,check](){
            action(container);
            QTimer::singleShot(PlayStepPeriod,container->content,[check,container](){
                check(container);
                endTestCase(container);
            });
        });
    };

    TestThread::instance()->postGuiThread(run);
    auto ret=TestThread::instance()->execTest(3000);
    UISE_TEST_CHECK(ret);    
}

void beginTestCase(SampleContainer* container, EditableLabel* label, const QString& testName)
{
    container->label=label;
    container->mainWindow=new QMainWindow();
    container->content=new QFrame(container->mainWindow);
    auto l = Layout::vertical(container->content);

    container->mainWindow->setCentralWidget(container->content);
    container->mainWindow->resize(600,500);
    container->mainWindow->setWindowTitle(testName);
    container->mainWindow->show();

    l->addWidget(label);
}

}

BOOST_AUTO_TEST_CASE(TestText)
{
    QString initialValue("Hello world");
    QString updatedValue("New text");

    auto checkCount=0;
    auto checkValueSet=[&updatedValue,&checkCount](SampleContainer* container){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto textLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(textLabel!=nullptr);
        UISE_TEST_REQUIRE(textLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,textLabel->widget()->text());

        ++checkCount;
    };

    auto init=[&initialValue,checkValueSet](SampleContainer* container){
        auto label = new EditableLabelText();
        label->setValue(initialValue);

        QObject::connect(label,&EditableLabel::valueSet,[container,checkValueSet](){checkValueSet(container);});

        beginTestCase(container,label,"Test editable text label");
    };

    auto action=[&initialValue,&updatedValue](SampleContainer* container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,container->label->text());

        auto textLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(textLabel!=nullptr);
        UISE_TEST_REQUIRE(textLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,textLabel->widget()->text());

        // set value
        textLabel->setValue(updatedValue);
    };

    auto check=[checkValueSet,&updatedValue,&checkCount](SampleContainer* container){
        checkValueSet(container);
        UISE_TEST_CHECK_EQUAL(checkCount,1);
    };

    runTestCase(init,action,check);
}

BOOST_AUTO_TEST_SUITE_END()
