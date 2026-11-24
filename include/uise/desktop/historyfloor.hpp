/*
    Copyright (c) 2024 - current, Evgeny Sidorov (decfile.com), All rights reserved.

    {{LICENSE}}
*/

/****************************************************************************/
/*

*/
/** @file uise/desktop/historyfloor.hpp
  *
  * Where the line of the history floor goes in a chat view ("messages before <date> are not available on this device").
  * Pure and header-only, so that the rule is tested without a window (test/chatmessagesview).
  */

/****************************************************************************/

#ifndef UISEDESKTOPHISTORYFLOOR_HPP
#define UISEDESKTOPHISTORYFLOOR_HPP

#include <cstddef>

namespace uise {

/**
 * @brief The index of the row the history floor line stands above, or -1 when it stands above none.
 *
 * The floor is a sort value: every message at or below it is history from before the device and is never in the list. The list is
 * sorted ascending, so the line stands above the FIRST row whose sort value is above the floor. When that row is the first of the
 * list the line shows only when the start of the chat is known to be loaded (`startReached`): otherwise older rows may still come and
 * the line would stand in the middle of the history. A floor above every row, or none, gives -1.
 *
 * @param count Number of rows.
 * @param sortValueAt Callable `sortValueAt(i)` returning the sort value of the row i, ascending in i.
 * @param floor The floor.
 * @param floorSet False when the chat has no floor.
 * @param startReached The first row is the first message of the chat.
 */
template <typename SortValueT, typename AtT>
int historyFloorIndex(size_t count, const AtT& sortValueAt, const SortValueT& floor, bool floorSet, bool startReached)
{
    if (!floorSet)
    {
        return -1;
    }

    for (size_t i=0;i<count;i++)
    {
        if (floor<sortValueAt(i))
        {
            if (i==0 && !startReached)
            {
                return -1;
            }
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace uise

#endif // UISEDESKTOPHISTORYFLOOR_HPP
