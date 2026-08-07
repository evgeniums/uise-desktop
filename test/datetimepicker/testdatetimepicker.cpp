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

/** @file uise/test/datetimepicker/testdatetimepicker.cpp
*
*  Test DateTimePicker.
*
*/

/****************************************************************************/

#include <QLocale>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>

#include <uise/desktop/spinner.hpp>
#include <uise/desktop/spinnersection.hpp>
#include <uise/desktop/datetimepicker.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

using DateTimePickerContainer=TestWidgetContainer<DateTimePicker>;
using DateTimePickerContainerPtr=std::shared_ptr<DateTimePickerContainer>;

namespace {

// column indices below assume the en_US short date format "M/d/yyyy" (order: Month, Day,
// Year) and short time format "h:mm AP" (order: Hour, Minute, AM/PM) -- both explicitly
// pinned via setLocale() in every test case that relies on them.
const QLocale EnUsLocale(QLocale::English,QLocale::UnitedStates);

}

BOOST_AUTO_TEST_SUITE(TestDateTimePicker)

BOOST_AUTO_TEST_CASE(TestValueRoundTrip)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDateTime(QDateTime(QDate(2026,8,5),QTime(0,0)));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker value round trip");
    };

    auto check=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,5));

        auto spinner=picker->spinner();
        UISE_TEST_REQUIRE_EQUAL(spinner->sectionCount(),static_cast<size_t>(3));

        // Month(0), Day(1), Year(2) -- this is the regression check for the first-run index
        // clobber in Spinner::updateCurrentIndex(): without DateTimePicker's verifyColumns()
        // follow-up pass, these would land on whatever index the middle visible row happens
        // to be, not on the value that was set.
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(0),7);   // August
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(1),4);   // day 5
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(2),126); // year 2026, min 1900
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        check
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestDayTruncation)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDate(QDate(2026,1,31));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker day truncation");
    };

    auto spinToFebruary=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,1,31));

        picker->spinner()->selectItem(0,1); // February
    };

    auto checkFebruary=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,2,28));
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(1)->itemsCount(),28);

        picker->spinner()->selectItem(1,27); // last day of February
    };

    auto checkLastDayOfFeb=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,2,28));

        picker->setDate(QDate(2024,2,10)); // leap year
    };

    auto checkLeapYear=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2024,2,10));
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(1)->itemsCount(),29);
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        spinToFebruary,
        checkFebruary,
        checkLastDayOfFeb,
        checkLeapYear
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestRangeClamp)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDateRange(QDate(2020,6,15),QDate(2030,3,10));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker range clamp");
    };

    auto checkYearCount=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(2)->itemsCount(),11); // 2020..2030

        picker->setDate(QDate(2020,1,1));
    };

    auto checkClampedLow=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2020,6,15));

        // at the min-date year the month wheel (circular, section 0 for en_US) is masked to
        // [June,December] -- this is the regression check for the unstable bounce this feature
        // replaces: scrolling backward past June must stop exactly there, not wrap to December
        auto spinner=picker->spinner();
        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(0),5);  // June
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(0),11);  // December, unrestricted
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(0),5);

        spinner->scroll(0,-50*spinner->itemHeight());
    };

    auto checkMonthClampedAtMinYear=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        auto spinner=picker->spinner();

        // still June -- a big backward scroll must not wrap a masked circular wheel past the
        // boundary
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(0),5);
        UISE_TEST_CHECK(picker->date()==QDate(2020,6,15));

        picker->setDate(QDate(2030,12,31));
    };

    auto checkClampedHigh=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2030,3,10));

        // at the max-date year the month wheel is masked to [January,March] -- scrolling
        // forward past March must stop exactly there, not wrap to January
        auto spinner=picker->spinner();
        UISE_TEST_CHECK_EQUAL(spinner->firstEnabledIndex(0),0);  // January, unrestricted
        UISE_TEST_CHECK_EQUAL(spinner->lastEnabledIndex(0),2);   // March
        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(0),2);

        spinner->scroll(0,50*spinner->itemHeight());
    };

    auto checkMonthClampedAtMaxYear=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        auto spinner=picker->spinner();

        UISE_TEST_CHECK_EQUAL(spinner->selectedItemIndex(0),2);
        UISE_TEST_CHECK(picker->date()==QDate(2030,3,10));
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        checkYearCount,
        checkClampedLow,
        checkMonthClampedAtMinYear,
        checkClampedHigh,
        checkMonthClampedAtMaxYear
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestSignalNotSpurious)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDate(QDate(2026,1,31));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker signal not spurious");
    };

    auto changeCount=std::make_shared<int>(0);

    auto connectAndNoOpSet=[changeCount](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        QObject::connect(picker,&DateTimePicker::dateTimeChanged,[changeCount](const QDateTime&)
        {
            ++(*changeCount);
        });

        picker->setDate(QDate(2026,1,31)); // same value -- must not emit
    };

    auto checkNoOpDidNotEmit=[changeCount](DateTimePickerContainerPtr container)
    {
        UISE_TEST_CHECK_EQUAL(*changeCount,0);

        auto picker=container->testWidget;
        picker->spinner()->selectItem(0,1); // February -- truncates day 31 -> 28
    };

    auto checkTruncationEmittedOnce=[changeCount](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,2,28));
        UISE_TEST_CHECK_EQUAL(*changeCount,1);
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        connectAndNoOpSet,
        checkNoOpDidNotEmit,
        checkTruncationEmittedOnce
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestLocaleOrder)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDate(QDate(2026,8,5));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker locale order");
    };

    auto checkEnUs=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        // en_US "M/d/yyyy": Month, Day, Year -- month (12 items) is section 0
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(0)->itemsCount(),12);

        picker->setLocale(QLocale(QLocale::German,QLocale::Germany));
    };

    auto checkDeDe=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,5));
        // de_DE "dd.MM.yyyy": Day, Month, Year -- month (12 items) is section 1
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(1)->itemsCount(),12);
        UISE_TEST_CHECK(picker->spinner()->section(0)->itemsCount()<=31);

        picker->setLocale(QLocale(QLocale::Japanese,QLocale::Japan));
    };

    auto checkJaJp=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,5));
        // ja_JP "yyyy/MM/dd": Year, Month, Day -- year (full 1900..2100 range) is section 0
        UISE_TEST_CHECK_EQUAL(picker->spinner()->section(0)->itemsCount(),201);
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        checkEnUs,
        checkDeDe,
        checkJaJp
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestFieldsRebuild)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new DateTimePicker(DateTimeField::Date);
        picker->setLocale(EnUsLocale);
        picker->setDate(QDate(2026,8,5));

        DateTimePickerContainer::beginTestCase(container,picker,"Test DateTimePicker fields rebuild");
    };

    auto checkDateFields=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK_EQUAL(picker->spinner()->sectionCount(),static_cast<size_t>(3));
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,5));

        picker->setFields(DateTimeField::DateTime);
    };

    auto checkDateTimeFields=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        // Year,Month,Day + Hour,Minute + AM/PM (en_US is 12-hour) = 6 sections
        UISE_TEST_CHECK_EQUAL(picker->spinner()->sectionCount(),static_cast<size_t>(6));
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,5));

        picker->setFields(DateTimeField::YearMonth);
    };

    auto checkYearMonthFields=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK_EQUAL(picker->spinner()->sectionCount(),static_cast<size_t>(2));
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,1));
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        checkDateFields,
        checkDateTimeFields,
        checkYearMonthFields
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestMonthMode)
{
    auto init=[](DateTimePickerContainerPtr container)
    {
        DateTimePickerContainer::PlayStepPeriod=500;

        auto picker=new MonthPicker();
        picker->setLocale(EnUsLocale);

        DateTimePickerContainer::beginTestCase(container,picker,"Test MonthPicker");
    };

    auto checkSectionCount=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK_EQUAL(picker->spinner()->sectionCount(),static_cast<size_t>(2));

        picker->setDate(QDate(2026,8,17));
    };

    auto checkPinnedToFirstDay=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,1));

        picker->setDateRange(QDate(2020,6,15),QDate(2030,3,10));
    };

    auto checkRangeUnaffected=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        // 2026-08 is well within the range -- setting the range must not move the value
        UISE_TEST_CHECK(picker->date()==QDate(2026,8,1));

        picker->setDate(QDate(2020,6,1));
    };

    auto checkNotClampedForward=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        // 2020-06-01 is exactly the range's starting month -- must NOT be pushed forward to
        // the 15th, which would be a day-granularity clamp that month mode must not apply
        UISE_TEST_CHECK(picker->date()==QDate(2020,6,1));

        picker->setDate(QDate(2020,5,1));
    };

    auto checkClampedToMinMonth=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2020,6,1));

        picker->setDate(QDate(2026,1,1));
    };

    auto checkJanuarySet=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,1,1));

        // en_US column order for YearMonth is Month(0), Year(1)
        picker->spinner()->selectItem(0,3); // April (30 days)
    };

    auto checkMonthOnlyChanged=[](DateTimePickerContainerPtr container)
    {
        auto picker=container->testWidget;
        UISE_TEST_CHECK(picker->date()==QDate(2026,4,1));
    };

    std::vector<std::function<void (DateTimePickerContainerPtr container)>> steps={
        init,
        checkSectionCount,
        checkPinnedToFirstDay,
        checkRangeUnaffected,
        checkNotClampedForward,
        checkClampedToMinMonth,
        checkJanuarySet,
        checkMonthOnlyChanged
    };
    DateTimePickerContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
