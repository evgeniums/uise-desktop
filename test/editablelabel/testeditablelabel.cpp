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
    auto ret=TestThread::instance()->execTest(15000);
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

        auto actualLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->text());

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

        auto actualLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,actualLabel->widget()->text());

        // set value
        actualLabel->setValue(updatedValue);
    };

    auto checkSetValueMethod=[checkValueSet,&updatedValue,&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
        checkValueSet(container);
        UISE_TEST_CHECK_EQUAL(checkCount,1);
        UISE_TEST_CHECK_EQUAL(checkValueCount,0);
    };

    auto setValueGui=[&initialValue,&updatedValue](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto actualLabel = qobject_cast<EditableLabelText*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->text());

        auto prevValue = updatedValue;

        // set editable mode
        container->label->edit();

        // set value
        updatedValue="New updated text";
        actualLabel->widget()->setText(updatedValue);

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->text());

        // cancel editable mode
        container->label->cancel();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,actualLabel->widget()->text());

        // set editable mode
        container->label->edit();
        UISE_TEST_CHECK(container->label->isEditable());

        // set value
        updatedValue="New updated text";
        actualLabel->widget()->setText(updatedValue);

        // apply changes
        container->label->apply();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->text());
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

template <EditableLabel::Type TypeId, typename ObjectT=typename EditableLabelHelper<TypeId>::type, typename ValueT=typename EditableLabelHelper<TypeId>::valueT>
void testLabelWithValue(const ValueT& initialValue, const ValueT& setValue, const ValueT& editedValue,
                        const QString& initialValueStr, const QString& setValueStr, const QString& editedValueStr,
                        const QString& title,
                        std::function<void (ObjectT*)> configLabel
                        )
{
    auto updatedValue=setValue;
    auto updatedValueStr=setValueStr;

    auto checkCount=0;
    auto checkValueSet=[&updatedValue,&updatedValueStr,&checkCount](std::shared_ptr<SampleContainer> container){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValueStr,container->label->text());

        auto actualLabel = qobject_cast<ObjectT*>(container->label);
        UISE_TEST_REQUIRE(actualLabel !=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK(updatedValue==actualLabel->value());

        ++checkCount;
    };

    auto checkValueCount=0;
    auto checkValueChanged=[&updatedValue,&checkValueCount](std::shared_ptr<SampleContainer> container, const ValueT& value){
        UISE_TEST_CHECK(updatedValue==value);

        ++checkValueCount;
    };

    auto init=[&initialValue,checkValueSet,checkValueChanged,&title,&configLabel](std::shared_ptr<SampleContainer> container){
        auto label = new ObjectT();
        configLabel(label);
        label->setValue(initialValue);

        QObject::connect(label,&EditableLabel::valueSet,[container,checkValueSet](){checkValueSet(container);});

        beginTestCase(container,label,title);

        UISE_TEST_CHECK(!container->label->isEditable());
        UISE_TEST_CHECK(!container->label->isInGroup());
        UISE_TEST_CHECK(container->label->formatter()==nullptr);
        UISE_TEST_CHECK(container->label->type()==TypeId);

        QObject::connect(label,&ObjectT::valueChanged,[container,checkValueChanged](const ValueT& value){checkValueChanged(container,value);});
    };

    auto setValueMethod=[&initialValue,&initialValueStr,&updatedValue,&updatedValueStr](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(initialValueStr,container->label->text());

        auto actualLabel = qobject_cast<ObjectT*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK(initialValue==actualLabel->value());

        // set value
        actualLabel->setValue(updatedValue);

        UISE_TEST_CHECK_EQUAL_QSTR(updatedValueStr,container->label->text());
        UISE_TEST_CHECK(updatedValue==actualLabel->value());
    };

    auto checkSetValueMethod=[checkValueSet,&updatedValue,&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
        checkValueSet(container);
        UISE_TEST_CHECK_EQUAL(checkCount,1);
        UISE_TEST_CHECK_EQUAL(checkValueCount,0);
    };

    auto setValueGui=[&initialValue,&updatedValue,&editedValue,&updatedValueStr,&editedValueStr](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValueStr,container->label->text());

        auto actualLabel = qobject_cast<ObjectT*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK(updatedValue==actualLabel->value());

        auto prevValue = updatedValue;
        auto prevValueStr = updatedValueStr;

        // set editable mode
        container->label->edit();

        // set value
        updatedValue=editedValue;
        updatedValueStr=editedValueStr;
        EditableLabelHelper<TypeId>::setValue(actualLabel->widget(),updatedValue);

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValueStr,container->label->text());
        // editor label changed
        UISE_TEST_CHECK(updatedValue==actualLabel->value());

        // cancel editable mode
        container->label->cancel();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValueStr,container->label->text());
        // editor label didn't change
        UISE_TEST_CHECK(prevValue==actualLabel->value());

        // set editable mode
        container->label->edit();
        UISE_TEST_CHECK(container->label->isEditable());

        // set value
        EditableLabelHelper<TypeId>::setValue(actualLabel->widget(),updatedValue);

        // apply changes
        container->label->apply();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValueStr,container->label->text());
        // editor label changed
        UISE_TEST_CHECK(updatedValue==actualLabel->value());
    };

    auto checkSetValueGui=[&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
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

BOOST_AUTO_TEST_CASE(TestInt)
{
    int initialValue=100;
    int setValue=-50;
    int editValue=12;

    auto initialValueStr=QString::number(initialValue);
    auto setValueStr=QString::number(setValue);
    auto editValueStr=QString::number(editValue);

    std::function<void (EditableLabelInt*)> configLabel=[](EditableLabelInt* label) {
        label->widget()->setRange(-1000,1000);
    };

    testLabelWithValue<EditableLabel::Type::Int>(initialValue,setValue,editValue,initialValueStr,setValueStr,editValueStr,"Test editable int label", configLabel);
}

BOOST_AUTO_TEST_CASE(TestDouble)
{
    double initialValue=100.21;
    double setValue=-50.32;
    double editValue=12.00;

    auto initialValueStr=QString::number(initialValue);
    auto setValueStr=QString::number(setValue);
    auto editValueStr=QString::number(editValue);

    std::function<void (EditableLabelDouble*)> configLabel=[](EditableLabelDouble* label) {
        label->widget()->setRange(-1000,1000);
    };

    testLabelWithValue<EditableLabel::Type::Double>(initialValue,setValue,editValue,initialValueStr,setValueStr,editValueStr,"Test editable double label", configLabel);
}

BOOST_AUTO_TEST_CASE(TestTime)
{
    QString initialValueStr="21:35";
    QString setValueStr="15:03";
    QString editValueStr="00:01";

    QTime initialValue=QTime::fromString(initialValueStr);
    QTime setValue=QTime::fromString(setValueStr);
    QTime editValue=QTime::fromString(editValueStr);

    std::function<void (EditableLabelTime*)> configLabel=[](EditableLabelTime*) {
    };

    testLabelWithValue<EditableLabel::Type::Time>(initialValue,setValue,editValue,initialValueStr,setValueStr,editValueStr,"Test editable time label", configLabel);
}

BOOST_AUTO_TEST_CASE(TestDate)
{
    QString initialValueStr="2022-02-22";
    QString setValueStr="2000-01-11";
    QString editValueStr="2023-05-15";

    QDate initialValue=QDate::fromString(initialValueStr,Qt::ISODate);
    QDate setValue=QDate::fromString(setValueStr,Qt::ISODate);
    QDate editValue=QDate::fromString(editValueStr,Qt::ISODate);

    auto format = QLocale().dateFormat(QLocale::ShortFormat);
    initialValueStr=initialValue.toString(format);
    setValueStr=setValue.toString(format);
    editValueStr=editValue.toString(format);

    std::function<void (EditableLabelDate*)> configLabel=[](EditableLabelDate*) {
    };

    testLabelWithValue<EditableLabel::Type::Date>(initialValue,setValue,editValue,initialValueStr,setValueStr,editValueStr,"Test editable date label", configLabel);
}

BOOST_AUTO_TEST_CASE(TestDateTime)
{
    QString initialDateStr="2022-02-22";
    QString setDateStr="2000-01-11";
    QString editDateStr="2023-05-15";

    QString initialTimeStr="21:35";
    QString setTimeStr="15:03";
    QString editTimeStr="00:01";

    QTime initialTime=QTime::fromString(initialTimeStr);
    QTime setTime=QTime::fromString(setTimeStr);
    QTime editTime=QTime::fromString(editTimeStr);

    QDate initialDate=QDate::fromString(initialDateStr,Qt::ISODate);
    QDate setDate=QDate::fromString(setDateStr,Qt::ISODate);
    QDate editDate=QDate::fromString(editDateStr,Qt::ISODate);

    QDateTime initialValue(initialDate,initialTime);
    QDateTime setValue(setDate,setTime);
    QDateTime editValue(editDate,editTime);

    auto format = QLocale().dateTimeFormat(QLocale::ShortFormat);
    auto initialValueStr=initialValue.toString(format);
    auto setValueStr=setValue.toString(format);
    auto editValueStr=editValue.toString(format);

    std::function<void (EditableLabelDateTime*)> configLabel=[](EditableLabelDateTime*) {
    };

    testLabelWithValue<EditableLabel::Type::DateTime>(initialValue,setValue,editValue,initialValueStr,setValueStr,editValueStr,"Test editable date time label", configLabel);
}

BOOST_AUTO_TEST_CASE(TestList)
{
    QString initialValue("three");
    QString updatedValue("one");
    QString editedValue("four");
    int editedIndex=3;

    auto checkCount=0;
    auto checkValueSet=[&updatedValue,&checkCount](std::shared_ptr<SampleContainer> container){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto actualLabel = qobject_cast<EditableLabelList*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->text());

        ++checkCount;
    };

    auto checkValueCount=0;
    auto checkValueChanged=[&updatedValue,&checkValueCount](std::shared_ptr<SampleContainer> container, const QString& value){
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,value);

        ++checkValueCount;
    };

    auto countIndexChanged=0;
    auto checkIndexChanged=[&updatedValue,&countIndexChanged,&editedIndex](std::shared_ptr<SampleContainer> container, int index){
        UISE_TEST_CHECK_EQUAL(index,editedIndex);
        ++countIndexChanged;
    };

    auto init=[&initialValue,checkValueSet,checkValueChanged,checkIndexChanged](std::shared_ptr<SampleContainer> container){
        auto label = new EditableLabelList();
        label->widget()->addItems({"one","two","three","four","five"});
        label->setValue(initialValue);

        QObject::connect(label,&EditableLabel::valueSet,[container,checkValueSet](){checkValueSet(container);});

        beginTestCase(container,label,"Test editable text label");

        UISE_TEST_CHECK(!container->label->isEditable());
        UISE_TEST_CHECK(!container->label->isInGroup());
        UISE_TEST_CHECK(container->label->formatter()==nullptr);
        UISE_TEST_CHECK(container->label->type()==EditableLabel::Type::List);

        QObject::connect(label,&EditableLabelList::textChanged,[container,checkValueChanged](const QString& value){checkValueChanged(container,value);});
        QObject::connect(label,&EditableLabelList::indexChanged,[container,checkIndexChanged](int value){checkIndexChanged(container,value);});
    };

    auto setValueMethod=[&initialValue,&updatedValue](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,container->label->text());

        auto actualLabel = qobject_cast<EditableLabelList*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(initialValue,actualLabel->text());

        // set value
        actualLabel->setValue(updatedValue);
    };

    auto checkSetValueMethod=[checkValueSet,&updatedValue,&checkCount,&checkValueCount](std::shared_ptr<SampleContainer> container){
        checkValueSet(container);
        UISE_TEST_CHECK_EQUAL(checkCount,1);
        UISE_TEST_CHECK_EQUAL(checkValueCount,0);
    };

    auto setValueGui=[&initialValue,&updatedValue,&editedValue](std::shared_ptr<SampleContainer> container){

        // initial check
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());

        auto actualLabel = qobject_cast<EditableLabelList*>(container->label);
        UISE_TEST_REQUIRE(actualLabel!=nullptr);
        UISE_TEST_REQUIRE(actualLabel->widget()!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->currentText());

        auto prevValue = updatedValue;

        // set editable mode
        container->label->edit();

        // set value
        updatedValue=editedValue;
        actualLabel->widget()->setCurrentText(updatedValue);

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->currentText());

        // cancel editable mode
        container->label->cancel();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text didn't change
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(prevValue,actualLabel->widget()->currentText());

        // set editable mode
        container->label->edit();
        UISE_TEST_CHECK(container->label->isEditable());

        // set value
        actualLabel->widget()->setCurrentText(updatedValue);

        // apply changes
        container->label->apply();
        UISE_TEST_CHECK(!container->label->isEditable());

        // label text changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,container->label->text());
        // editor label changed
        UISE_TEST_CHECK_EQUAL_QSTR(updatedValue,actualLabel->widget()->currentText());
    };

    auto checkSetValueGui=[checkValueSet,&updatedValue,&checkCount,&checkValueCount,&countIndexChanged](std::shared_ptr<SampleContainer> container){
        UISE_TEST_CHECK_EQUAL(checkCount,2);
        UISE_TEST_CHECK_EQUAL(checkValueCount,1);
        UISE_TEST_CHECK_EQUAL(countIndexChanged,1);
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
