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

/** @file uise/desktop/src/replypreviewdata.cpp
*
*  Defines ReplyPreviewData.
*
*/

/****************************************************************************/

#include <uise/desktop/replypreviewdata.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

QString trimReplyText(const QString& text, int maxLength)
{
    // simplified() collapses every run of whitespace (including newlines) to a single space
    // and trims the ends -- exactly the "one line" normalization a reply preview needs.
    auto collapsed=text.simplified();

    if (maxLength<=0 || collapsed.length()<=maxLength)
    {
        return collapsed;
    }

    // Cut at the last word boundary at or before maxLength, so a long word is not split
    // mid-character -- unless there is no boundary in range (a single word longer than the
    // limit), in which case cutting at exactly maxLength is the only option.
    auto cut=collapsed.lastIndexOf(QChar(' '),maxLength);
    if (cut<=0)
    {
        cut=maxLength;
    }
    return collapsed.left(cut);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
