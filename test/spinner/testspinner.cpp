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

BOOST_AUTO_TEST_CASE(TestDisabledItemsLinear)
{
    // non-circular section: scrolling past a masked-out range must hard-clamp to the boundary
    // of the enabled range, deterministically, regardless of scroll magnitude -- this is the
    // regression test for the unstable month-wheel bounce this feature replaces.

    auto init=[](SpinnerContainerPtr container){
        auto spinner=createSpinner();
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner disabled items (linear)");
    };

    int section=0;

    auto applyMask=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        // settled state before masking, same precondition createSpinner() already asserts
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);

        // enable items [5,10] (values "6".."11"), disabling everything else. The section is
        // already loaded, so use the Spinner mirror API rather than SpinnerSection::
        // setEnabledRange() directly -- it re-clamps the current selection (item 0, outside the
        // new range) into the range.
        spinner->setEnabledRange(section,5,10);
    };

    auto checkClampedOnMask=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(section),5);
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(section),10);
        // item 0 was outside [5,10] -- setEnabledRange() must have snapped it to the nearer
        // boundary (5)
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),5);
        UISE_TEST_CHECK(!spinner->itemEnabled(section,4));
        UISE_TEST_CHECK(spinner->itemEnabled(section,5));
        UISE_TEST_CHECK(spinner->itemEnabled(section,10));
        UISE_TEST_CHECK(!spinner->itemEnabled(section,11));

        // scroll far forward (past the end of the whole 31-item list) -- must stop exactly at
        // the last enabled item, not the last item of the underlying list
        spinner->scroll(section,50*itemHeight);
    };

    auto checkClampedForward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),10);

        // scroll far backward -- must stop exactly at the first enabled item
        spinner->scroll(section,-50*itemHeight);
    };

    auto checkClampedBackward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),5);

        // selectItem() targeting a disabled index must also land on the nearer boundary
        // instead of throwing or selecting the disabled item
        spinner->selectItem(section,20);
    };

    auto checkSelectItemClampedForward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),10);

        spinner->selectItem(section,0);
    };

    auto checkSelectItemClampedBackward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),5);

        // widen the mask back to everything -- selection must stay put (widening never snaps)
        spinner->resetEnabledItems(section);
    };

    auto checkReset=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),5);
        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(section),0);
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(section),spinner->section(section)->itemsCount()-1);
        UISE_TEST_CHECK(spinner->itemEnabled(section,4));

        // and scrolling past the end works again, unmasked
        spinner->scroll(section,50*itemHeight);
    };

    auto checkUnmaskedScrollEnd=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),spinner->section(section)->itemsCount()-1);
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            applyMask,
            checkClampedOnMask,
            checkClampedForward,
            checkClampedBackward,
            checkSelectItemClampedForward,
            checkSelectItemClampedBackward,
            checkReset,
            checkUnmaskedScrollEnd
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestDisabledItemsCircular)
{
    // circular section: this is the exact shape of DateTimePicker's month wheel. Masking a
    // circular section must make it stop at the boundary instead of wrapping past it -- a
    // single large step (the PageDown/fast-flick case) must not cross the forbidden gap and
    // land on the opposite boundary.

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
        // set the mask BEFORE loading -- setSections() must select firstEnabledIndex(), not 0
        section->setEnabledRange(3,7);

        std::vector<std::shared_ptr<SpinnerSection>> sections{section};
        auto spinner=createSpinner(sections);

        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner disabled items (circular)");
    };

    int section=1; // createSpinner() prepends its own linear section at index 0

    auto checkInitialSelection=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        // a mask set before setSections() must already be honoured for the initial selection
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);

        // scroll far forward -- must stop exactly at the last enabled item, not wrap around
        spinner->scroll(section,50*itemHeight);
    };

    auto checkClampedForward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),7);

        // a further PageDown-sized step from the boundary must NOT cross the forbidden gap and
        // wrap to the other boundary -- this is the regression test for per-call modular
        // clamping (see Spinner::clampOffset()'s comment)
        spinner->scroll(section,5*itemHeight);
    };

    auto checkStillAtForwardBoundary=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),7);

        // scroll far backward -- must stop exactly at the first enabled item
        spinner->scroll(section,-50*itemHeight);
    };

    auto checkClampedBackward=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);

        // and the same PageDown-sized guard in the other direction
        spinner->scroll(section,-5*itemHeight);
    };

    auto checkStillAtBackwardBoundary=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            checkInitialSelection,
            checkClampedForward,
            checkStillAtForwardBoundary,
            checkClampedBackward,
            checkStillAtBackwardBoundary
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestEnabledRangeAppliedAfterDrift)
{
    // a circular section's currentOffset can drift by an arbitrary number of whole periods while
    // unmasked (rendering is exactly period-periodic, so the drift is never visible). Applying a
    // mask afterwards must fold that drift away and land inside the new range in one step --
    // this exercises enforceEnabledItems()'s floor-division normalisation, including its
    // negative-dividend path.

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

        std::vector<std::shared_ptr<SpinnerSection>> sections{section};
        auto spinner=createSpinner(sections);

        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner enabled range after drift");
    };

    int section=1;

    auto driftManyPeriods=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);

        // scroll forward by exactly 5 full periods -- unmasked, so this wraps back to the same
        // item (index 0), but currentOffset itself is now 5*31*itemHeight away from where it
        // started
        spinner->scroll(section,5*31*itemHeight);
    };

    auto applyMaskAfterDrift=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);

        // item 0 is outside [3,7]; the nearest boundary (3 steps forward) is item 3, versus
        // wrapping all the way around to reach item 7 the other way -- must land on 3
        spinner->setEnabledRange(section,3,7);
    };

    auto checkLandedInsideRange=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),3);
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            driftManyPeriods,
            applyMaskAfterDrift,
            checkLandedInsideRange
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestDisabledItemsResize)
{
    // appendItems()/removeLastItems() must keep the enabled mask parallel to the item list, and
    // re-clamp the current selection if the mask's bounds would otherwise outlive a shrunk list
    // -- this is the shape of DateTimePicker's day column shrinking under an active mask (e.g.
    // navigating from a 31-day month to a 30-day one while near the max-date boundary).

    auto init=[](SpinnerContainerPtr container){
        auto spinner=createSpinner();
        SpinnerContainer::beginTestCase(container,spinner,"Test Spinner disabled items resize");
    };

    int section=0;

    auto applyMask=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),0);

        // enable items [15,30] of the initial 31-item list (indices 0..30)
        spinner->setEnabledRange(section,15,30);
    };

    auto checkInitialMask=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        // item 0 was outside [15,30] -- clamped to the nearer boundary (15)
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),15);

        // shrink the list from 31 to 20 items (indices 0..19) -- this removes indices 20..30,
        // i.e. the tail of the enabled range. The mask's lastEnabled (30) now points past the
        // end of the list and must be re-clamped to what actually survives (19).
        spinner->removeLastItems(section,11);
    };

    auto checkMaskReclampedAfterShrink=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->section(section)->itemsCount(),20);
        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(section),15);
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(section),19);
        // selection (15) was already within the re-clamped range -- unaffected by the shrink
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(section),15);

        // grow back to 25 items -- appended items must default to enabled, and their widgets
        // must not carry a stale "itemDisabled" state from a previous load
        for (auto i=0;i<5;++i)
        {
            auto item=new QLabel(QString::number(21+i));
            item->setAlignment(Qt::AlignCenter);
            item->setFixedWidth(spinner->section(section)->itemsWidth());
            item->setFixedHeight(itemHeight);
            spinner->appendItem(section,item);
        }
    };

    auto checkAppendedItemsEnabled=[section](SpinnerContainerPtr container){
        auto spinner=container->testWidget;

        UISE_TEST_CHECK_EQUAL(spinner->section(section)->itemsCount(),25);
        // appended items (20..24) join the enabled range, extending its upper bound
        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(section),15);
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(section),24);
        UISE_TEST_CHECK(spinner->itemEnabled(section,20));
        UISE_TEST_CHECK(spinner->itemEnabled(section,24));
        // the pre-existing mask still holds for indexes that were already loaded
        UISE_TEST_CHECK(!spinner->itemEnabled(section,14));
        UISE_TEST_CHECK(spinner->itemEnabled(section,19));
    };

    std::vector<std::function<void (SpinnerContainerPtr container)>> steps={
            init,
            applyMask,
            checkInitialMask,
            checkMaskReclampedAfterShrink,
            checkAppendedItemsEnabled
    };
    SpinnerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
