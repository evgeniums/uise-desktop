/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/spinner/testspinner.cpp
*
*  Test Spinner.
*
*/

/****************************************************************************/

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

    auto styleSample = new QLineEdit();
    spinner->setStyleSample(styleSample);

    spinner->setFixedSize(500,500);

    QString lightTheme="*{font-size: 20px;} \n uise--desktop--Spinner *{color: black;} \n uise--desktop--Spinner QLabel {background-color: transparent;} \n uise--desktop--Spinner *[style-sample=\"true\"] {background-color: white; selection-background-color: lightgray;}";
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

    auto checkFirstItemSelected=[section,selectItem,&handleItemChanged](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);
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
            checkFirstItemSelected,
            checkSelectedItem
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestScroll)
{
    auto init=[](SpinnerContainerPtr container){
        auto spinner=createSpinner();
        SpinnerContainer::PlayStepPeriod=300;
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner item selection");
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
        SpinnerContainer::PlayStepPeriod=300;
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner item selection");
    };

    int scrollDelta=20;
    int section=1;
    int changedCount=0;
    auto handleItemChanged=[&changedCount,section](int sectionIndex, int itemIndex)
    {
        UISE_TEST_CHECK_EQUAL(sectionIndex,section);
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

    auto checkScroll03Delay=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);
    };

    auto checkScroll03=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
        UISE_TEST_CHECK_EQUAL(changedCount,2);

        spinner->scroll(section,-4*itemHeight);
    };

    auto checkScrollBegin=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),30);
        UISE_TEST_CHECK_EQUAL(changedCount,3);

        spinner->scroll(section,50*itemHeight);
    };

    auto checkScrollEnd=[&changedCount,section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),18);
        UISE_TEST_CHECK_EQUAL(changedCount,4);
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
