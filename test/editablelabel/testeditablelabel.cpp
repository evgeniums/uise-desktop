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
#include <QPointer>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/editablelabel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestEditableLabel)

namespace {

constexpr static const int PlayStepPeriod=200;

struct SampleContainer
{
    QPointer<QMainWindow> mainWindow;
    QFrame* content=nullptr;
    EditableLabel* label=nullptr;

    ~SampleContainer()
    {
        if (mainWindow)
        {
            mainWindow->hide();
            mainWindow->deleteLater();
        }
    }
};

void runStep(std::shared_ptr<SampleContainer> container, std::vector<std::function<void (std::shared_ptr<SampleContainer>)>> steps, size_t index)
{
    if (index>=steps.size())
    {
        TestThread::instance()->continueTest();
        return;
    }
    steps[index](container);
    QTimer::singleShot(PlayStepPeriod,container->content,[container,steps,index](){
        runStep(container,steps,index+1);
    });
}

void runTestCase(
        std::vector<std::function<void (std::shared_ptr<SampleContainer>)>> steps
    )
{
    auto container=std::make_shared<SampleContainer>();
    auto run=[container,steps]()
    {
        runStep(container,steps,0);
    };

    TestThread::instance()->postGuiThread(run);
    auto ret=TestThread::instance()->execTest(3000);
    UISE_TEST_CHECK(ret);
}

void beginTestCase(std::shared_ptr<SampleContainer> container, EditableLabel* label, const QString& testName)
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
    auto checkValueSet=[&updatedValue,&checkCount](std::shared_ptr<SampleContainer> container){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto textLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(textLabel!=nullptr);
        UISE_TEST_REQUIRE(textLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,textLabel->widget()->text());

        ++checkCount;
    };

    auto checkValueCount=0;
    auto checkValueChanged=[&updatedValue,&checkValueCount](std::shared_ptr<SampleContainer> container, const QString& value){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,value);

        ++checkValueCount;
    };

    auto init=[&initialValue,checkValueSet,checkValueChanged](std::shared_ptr<SampleContainer> container){
        auto label = new EditableLabelText();
        label->setValue(initialValue);

        QObject::connect(label,&EditableLabel::valueSet,[container,checkValueSet](){checkValueSet(container);});

        beginTestCase(container,label,"Test editable text label");

        UISE_TEST_CHECK(!container->label->isEditable());
        UISE_TEST_CHECK(!container->label->isInGroup());
        UISE_TEST_CHECK(container->label->formatter()==nullptr);
        UISE_TEST_CHECK(container->label->type()==EditableLabel::Type::Text);

        QObject::connect(label,&EditableLabelText::valueChanged,[container,checkValueChanged](const QString& value){checkValueChanged(container,value);});
    };

    auto setValueMethod=[&initialValue,&updatedValue](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,container->label->text());

        auto textLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(textLabel!=nullptr);
        UISE_TEST_REQUIRE(textLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,textLabel->widget()->text());

        // set value
        textLabel->setValue(updatedValue);
    };

    auto checkSetValueMethod=[checkValueSet,&updatedValue,&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
        checkValueSet(container);
        UISE_TEST_CHECK_EQUAL(checkCount,1);
        UISE_TEST_CHECK_EQUAL(checkValueCount,0);
    };

    auto setValueGui=[&initialValue,&updatedValue](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto textLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(textLabel!=nullptr);
        UISE_TEST_REQUIRE(textLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,textLabel->widget()->text());

        auto prevValue = updatedValue;

        // set editable mode
        container->label->edit();

        // set value
        updatedValue="New updated text";
        textLabel->widget()->setText(updatedValue);

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,textLabel->widget()->text());

        // cancel editable mode
        container->label->cancel();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,textLabel->widget()->text());

        // set editable mode
        container->label->edit();
        UISE_TEST_CHECK(container->label->isEditable());

        // set value
        updatedValue="New updated text";
        textLabel->widget()->setText(updatedValue);

        // apply changes
        container->label->apply();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,textLabel->widget()->text());
    };

    auto checkSetValueGui=[checkValueSet,&updatedValue,&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
        UISE_TEST_CHECK_EQUAL(checkCount,2);
        UISE_TEST_CHECK_EQUAL(checkValueCount,1);
    };

    std::vector<std::function<void (std::shared_ptr<SampleContainer>)>> steps={
            init,
            setValueMethod,
            checkSetValueMethod,
            setValueGui,
            checkSetValueGui
    };
    runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
