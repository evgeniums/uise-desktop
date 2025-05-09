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

/** @file uise/test/spinner/testspinner.cpp
*
*  Test Spinner.
*
*/

/****************************************************************************/

#include <iostream>

#include <QLabel>
#include <QLineEdit>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>

#include <uise/desktop/spinner.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

using SpinnerContainer=TestWidgetContainer<Spinner>;
using SpinnerContainerPtr=std::shared_ptr<SpinnerContainer>;

namespace {

static const int itemHeight=50;

Spinner *createSpinner(std::vector<std::shared_ptr<SpinnerSection>> sections=std::vector<std::shared_ptr<SpinnerSection>>())
{
    SpinnerContainer::PlayStepPeriod=300;

    auto spinner=new Spinner();

    int itemHeight=50;
    int itemsWidth=30;
    spinner->setItemHeight(itemHeight);

    auto section1=std::make_shared<SpinnerSection>();
    section1->setItemsWidth(30);
    QList<QWidget*> items1;
    for (auto i=1;i<=31;i++)
    {
        auto item=new QLabel(QString::number(i));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section1->itemsWidth());
        item->setFixedHeight(itemHeight);
        items1.append(item);
    }
    section1->setItems(items1);
    sections.insert(sections.begin(),section1);

    spinner->setFixedSize(500,500);

    QString lightTheme="*{font-size: 20px;} \n uise--Spinner *{color: black;} \n uise--Spinner QLabel {background-color: transparent;} \n uise--Spinner *[style-sample=\"true\"] {background-color: white; selection-background-color: lightgray;}";
    qApp->setStyleSheet(lightTheme);

    spinner->setSections(sections);

    UISE_TEST_REQUIRE_EQUAL(spinner->sectionCount(),sections.size());
    UISE_TEST_REQUIRE(spinner->section(0));
    UISE_TEST_CHECK_EQUAL(spinner->section(0)->itemsCount(),items1.size());
    UISE_TEST_CHECK_EQUAL(spinner->section(0)->itemsWidth(),itemsWidth);
    UISE_TEST_CHECK(!spinner->section(0)->circular());
    UISE_TEST_CHECK_EQUAL(spinner->section(0)->width(),itemsWidth);
    UISE_TEST_CHECK_EQUAL(spinner->section(0)->leftBarWidth(),0);
    UISE_TEST_CHECK_EQUAL(spinner->section(0)->rightBarWidth(),0);

    return spinner;
}

}

BOOST_AUTO_TEST_SUITE(TestSpinner)

BOOST_AUTO_TEST_CASE(TestItemSelection)
{
    auto init=[](SpinnerContainerPtr container){
        auto spinner=createSpinner();
        spinner->selectItem(0,5);

        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner item selection");
    };

    int selectItem=20;
    int section=0;
    int changedCount=0;
    auto handleItemChanged=[&changedCount,section,selectItem](int sectionIndex, int itemIndex)
    {
        UISE_TEST_CHECK_EQUAL(sectionIndex,section);
        UISE_TEST_CHECK_EQUAL(itemIndex,selectItem);
        changedCount++;
    };

    auto checkFirstSelectedItem=[section,selectItem,&handleItemChanged](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),5);
        QObject::connect(spinner,&Spinner::itemChanged,handleItemChanged);

        spinner->selectItem(section,selectItem);
    };

    auto checkSelectedItem=[&changedCount,section,selectItem](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),selectItem);
        UISE_TEST_CHECK_EQUAL(changedCount,1);
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            checkFirstSelectedItem,
            checkSelectedItem
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestScroll)
{
    auto init=[](SpinnerContainerPtr container){
        auto spinner=createSpinner();
        SpinnerContainer::PlayStepPeriod=300;
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner scroll");
    };

    int scrollDelta=20;
    int section=0;
    int changedCount=0;
    auto handleItemChanged=[&changedCount,section](int sectionIndex, int itemIndex)
    {
        changedCount++;
    };

    auto scrollTwoItems=[section,&handleItemChanged](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);
        QObject::connect(spinner,&Spinner::itemChanged,handleItemChanged);

        spinner->scroll(section,itemHeight*2);
    };

    auto checkScrollTwoItems=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),2);
        UISE_TEST_CHECK_EQUAL(changedCount,1);

        spinner->scroll(section,itemHeight*0.75);
    };

    auto checkScroll075Delay=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),2);
        UISE_TEST_CHECK_EQUAL(changedCount,1);
    };

    auto checkScroll075=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);

        spinner->scroll(section,itemHeight*0.3);
    };

    auto checkScroll03=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);

        spinner->scroll(section,-4*itemHeight);
    };

    auto checkScrollBegin=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);
        UISE_TEST_CHECK_EQUAL(changedCount,3);

        spinner->scroll(section,50*itemHeight);
    };

    auto checkScrollEnd=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),spinner->section(0)->itemsCount()-1);
        UISE_TEST_CHECK_EQUAL(changedCount,4);
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            scrollTwoItems,
            checkScrollTwoItems,
            checkScroll075Delay,
            checkScroll075,
            checkScroll03,
            checkScrollBegin,
            checkScrollEnd
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestCircular)
{
    auto init=[](SpinnerContainerPtr container){

        UISE_TEST_MESSAGE("init begin");

        auto section=std::make_shared<SpinnerSection>();
        section->setItemsWidth(30);
        QList<QWidget*> items;
        for (auto i=1;i<=31;i++)
        {
            auto item=new QLabel(QString::number(i));
            item->setAlignment(Qt::AlignCenter);
            item->setFixedWidth(section->itemsWidth());
            item->setFixedHeight(itemHeight);
            items.append(item);
        }
        section->setItems(items);
        section->setCircular(true);
        section->setLeftBarWidth(20);
        UISE_TEST_CHECK_EQUAL(section->leftBarWidth(),20);
        section->setRightBarWidth(10);
        UISE_TEST_CHECK_EQUAL(section->rightBarWidth(),10);

        std::vector<std::shared_ptr<SpinnerSection>> sections{section};
        auto spinner=createSpinner(sections);

        auto styleSample = new QLineEdit();
        spinner->setStyleSample(styleSample);

        SpinnerContainer::PlayStepPeriod=300;
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner circular");

        UISE_TEST_MESSAGE("init end");
    };

    int scrollDelta=20;
    int section=1;
    int changedCount=0;
    auto handleItemChanged=[&changedCount,section](int sectionIndex, int itemIndex)
    {
        UISE_TEST_MESSAGE("handleItemChanged begin");

        UISE_TEST_CHECK_EQUAL(sectionIndex,section);
        changedCount++;

        UISE_TEST_MESSAGE("handleItemChanged end");
    };

    auto scrollTwoItems=[section,&handleItemChanged](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("scrollTwoItems begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);
        QObject::connect(spinner,&Spinner::itemChanged,handleItemChanged);

        spinner->scroll(section,itemHeight*2);

        UISE_TEST_MESSAGE("scrollTwoItems end");
    };

    auto checkScrollTwoItems=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScrollTwoItems begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),2);
        UISE_TEST_CHECK_EQUAL(changedCount,1);

        spinner->scroll(section,itemHeight*0.75);

        UISE_TEST_MESSAGE("checkScrollTwoItems end");
    };

    auto checkScroll075Delay=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScroll075Delay begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),2);
        UISE_TEST_CHECK_EQUAL(changedCount,1);
        UISE_TEST_MESSAGE("checkScroll075Delay end");
    };

    auto checkScroll075=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScroll075 begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);

        spinner->scroll(section,itemHeight*0.3);
        UISE_TEST_MESSAGE("checkScroll075 end");
    };

    auto checkScroll03Delay=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScroll03Delay begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);
        UISE_TEST_MESSAGE("checkScroll03Delay end");
    };

    auto checkScroll03=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScroll03 begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);

        spinner->scroll(section,-4*itemHeight);
        UISE_TEST_MESSAGE("checkScroll03 end");
    };

    auto checkScrollBegin=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScrollBegin begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),30);
        UISE_TEST_CHECK_EQUAL(changedCount,3);

        spinner->scroll(section,50*itemHeight);
        UISE_TEST_MESSAGE("checkScrollBegin end");
    };

    auto checkScrollEnd=[&changedCount,section](SpinnerContainerPtr container)
    {
        UISE_TEST_MESSAGE("checkScrollEnd begin");
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),18);
        UISE_TEST_CHECK_EQUAL(changedCount,4);
        UISE_TEST_MESSAGE("checkScrollEnd end");
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            scrollTwoItems,
            checkScrollTwoItems,
            checkScroll075Delay,
            checkScroll075,
            checkScroll03Delay,
            checkScroll03,
            checkScrollBegin,
            checkScrollEnd
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
