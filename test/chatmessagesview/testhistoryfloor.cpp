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

/** @file uise/test/chatmessagesview/testhistoryfloor.cpp
*
*  Test of historyFloorIndex(): where the history floor line of a chat view stands.
*
*/

/****************************************************************************/

#include <vector>

#include <boost/test/unit_test.hpp>

#include <uise/desktop/historyfloor.hpp>

namespace {

//! The index the floor line stands above, for rows given as plain sort values.
int floorIndex(const std::vector<int>& rows, int floor, bool floorSet, bool startReached)
{
    return uise::historyFloorIndex(
        rows.size(),
        [&rows](size_t i)
        {
            return rows[i];
        },
        floor,
        floorSet,
        startReached
    );
}

}

BOOST_AUTO_TEST_SUITE(TestHistoryFloor)

BOOST_AUTO_TEST_CASE(TheLineStandsAboveTheOldestRowWhenTheStartIsLoaded)
{
    // the floor is 10: everything at or below it is hidden, so the oldest visible row is the first above it
    BOOST_CHECK_EQUAL(floorIndex({11,12,13},10,true,true),0);
}

BOOST_AUTO_TEST_CASE(TheLineWaitsForTheStartOfTheChat)
{
    // older rows may still come above the first loaded one: no line in the middle of the history
    BOOST_CHECK_EQUAL(floorIndex({11,12,13},10,true,false),-1);
}

BOOST_AUTO_TEST_CASE(NoFloorNoLine)
{
    BOOST_CHECK_EQUAL(floorIndex({11,12,13},0,false,true),-1);
    BOOST_CHECK_EQUAL(floorIndex({},0,false,true),-1);
}

BOOST_AUTO_TEST_CASE(AFloorAboveEveryRowGivesNoLine)
{
    // the rows are older than the floor (a late arrival of a stale list): nothing to stand above
    BOOST_CHECK_EQUAL(floorIndex({1,2,3},10,true,true),-1);
    BOOST_CHECK_EQUAL(floorIndex({},10,true,true),-1);
}

BOOST_AUTO_TEST_CASE(ARowAtTheFloorIsNotAboveIt)
{
    // a row whose sort value equals the floor is part of the hidden history
    BOOST_CHECK_EQUAL(floorIndex({10,11,12},10,true,true),1);
    BOOST_CHECK_EQUAL(floorIndex({10},10,true,true),-1);
}

BOOST_AUTO_TEST_CASE(TheLineStandsInTheMiddleWhenOlderRowsAreLoaded)
{
    // rows older than the floor are loaded (a message that came late with an old id): the line is above the first row above the floor,
    // and the start of the chat does not matter for it
    BOOST_CHECK_EQUAL(floorIndex({5,6,11,12},10,true,false),2);
    BOOST_CHECK_EQUAL(floorIndex({5,6,11,12},10,true,true),2);
}

BOOST_AUTO_TEST_CASE(AMovedFloorMovesTheLine)
{
    const std::vector<int> rows{11,12,13,14};
    BOOST_CHECK_EQUAL(floorIndex(rows,10,true,true),0);
    BOOST_CHECK_EQUAL(floorIndex(rows,12,true,true),2);
    BOOST_CHECK_EQUAL(floorIndex(rows,14,true,true),-1);
}

BOOST_AUTO_TEST_SUITE_END()
