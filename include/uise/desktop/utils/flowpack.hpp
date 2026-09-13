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

/** @file uise/desktop/utils/flowpack.hpp
*
*  Declares flowPack().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLOWPACK_HPP
#define UISE_DESKTOP_FLOWPACK_HPP

#include <vector>

#include <QRect>
#include <QSize>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Parameters bounding a single greedy left-to-right wrapping pass.
 *
 * Deliberately NOT a QLayout -- see flowPack()'s own doc comment for why: the chat bubble
 * negotiation pipeline (AbstractChatMessageContent::updateBubbleWidth()) needs to ask "how wide
 * would this pack be at width W" as a pure function repeatedly, on hypothetical widths, before
 * anything is committed -- a QLayout has no equivalent of that.
 */
struct UISE_DESKTOP_EXPORT FlowPackOptions
{
    int maxWidth=0;         //!< Hard width budget -- no item's rect ever extends past this.
    int hSpacing=6;         //!< Horizontal gap between items on the same row.
    int vSpacing=4;         //!< Vertical gap between rows.

    //! Width kept free at the END of the LAST row, for content the caller positions itself
    //! afterwards (e.g. a chat message's inline time/status row -- see
    //! AbstractChatMessageContent::evaluateInlineBottom()). 0 disables the reservation.
    int reservedTailWidth=0;

    //! Caps how many of the input items are actually placed; 0 means unlimited. When the input
    //! has more items than this, the LAST placed slot is given over to a caller-supplied tail
    //! item (see tailItemWidth) instead of the truncated input item, and FlowPackResult::truncated
    //! is set.
    int maxItems=0;

    //! Natural width of the tail item (e.g. an ellipsis "..." chip) substituted in when
    //! maxItems truncates the input. Height is assumed to not exceed the tallest placed item's
    //! row, matching how a small overflow indicator is normally sized. Ignored unless truncation
    //! actually happens.
    int tailItemWidth=0;
};

/**
 * @brief Result of a single flowPack() call.
 */
struct UISE_DESKTOP_EXPORT FlowPackResult
{
    //! One rect per PLACED item, in the same order as the input (with the caveat below for
    //! truncation). Rects are relative to the packed area's own origin (0,0) -- callers offset by
    //! their own contents margin.
    std::vector<QRect> rects;

    //! Bounding rect of the final row alone -- the anchor for content the caller positions
    //! relative to the last packed item (see reservedTailWidth and
    //! ChatMessageContentSection::lastTextLineRect()).
    QRect lastRowRect;

    //! Bounding size of the whole pack -- the union of all rects in `rects`.
    QSize totalSize;

    //! Number of items actually placed. Equals items.size() unless FlowPackOptions::maxItems
    //! truncated the input, in which case this is maxItems and the LAST entry in `rects`
    //! is the tail item's rect rather than the corresponding input item's.
    size_t placedCount=0;

    //! True when FlowPackOptions::maxItems truncated the input.
    bool truncated=false;

    //! False when honouring reservedTailWidth was not possible even with the last row holding a
    //! single item -- the caller's tail content does not fit next to the pack at this width and
    //! must fall back to a row of its own. Always true when reservedTailWidth is 0.
    bool tailFits=true;
};

/**
 * @brief Pack a sequence of fixed-size items into a wrapping row, greedy left-to-right.
 *
 * There is no FlowLayout (or any wrapping QLayout) anywhere in this library -- this function is
 * the manual-geometry idiom used instead, matching how ChatMessageImages already packs an image
 * album's tiles (see utils/albumlayout.hpp's albumLayout(), the closest sibling). A QLayout is
 * deliberately avoided for the same three reasons that idiom exists:
 *
 *  1. The chat bubble negotiation is PULL, not push: a section's bubbleWidthHint(forMaxWidth)
 *     must answer for a hypothetical width, and updateMaximumBubbleWidth() then commits at
 *     whatever width the bubble actually settled on. A QLayout has no "answer for this width
 *     without committing to it" step.
 *  2. A height-for-width child inside the bubble's QVBoxLayout would fight the QWidgetItemV2
 *     per-child size-hint cache that AbstractChatMessageContent::relayoutSections() and
 *     refreshSectionHints() already work around for the other sections.
 *  3. AbstractChatMessageContent::setMaximumBubbleWidth() reads the layout's aggregate
 *     sizeHint().height() BEFORE activating the layout -- a heightForWidth child's contribution
 *     would be wrong at that point.
 *
 * Algorithm: items are placed left to right; when an item would extend past
 * options.maxWidth, a new row starts instead. Each row's height is the tallest item in it;
 * items are vertically centred within their row. After the greedy placement, if
 * options.reservedTailWidth is set, a correction pass runs: while the last row plus the
 * reservation would overflow maxWidth and that row holds more than one item, the row's last item
 * is moved to a new row of its own and the check repeats. If a single item's row still cannot
 * make room for the reservation, the loop stops and FlowPackResult::tailFits is set to false
 * (see AbstractChatMessageContent::evaluateInlineBottom()'s ownWidthCeiling()/lastTextLineRect()
 * contract for how a caller is expected to react to that).
 *
 * @param items Natural (unspaced) size of each item, in display order.
 * @param options Packing bounds -- see FlowPackOptions.
 * @return Packed geometry -- see FlowPackResult.
 */
UISE_DESKTOP_EXPORT FlowPackResult flowPack(
    const std::vector<QSize>& items,
    const FlowPackOptions& options
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLOWPACK_HPP
