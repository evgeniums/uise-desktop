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

/** @file uise/desktop/textformat.hpp
*
*  Declares TextFormat.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEXT_FORMAT_HPP
#define UISE_DESKTOP_TEXT_FORMAT_HPP

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief How a piece of message/comment/reply-preview text is encoded.
 *
 * Split out of abstractmessageeditor.hpp (task-message-formatting-plan.md, Stage 2) so that a
 * plain value type such as ReplyPreviewData can see it without dragging in frame.hpp/
 * dropdownmenu.hpp -- abstractmessageeditor.hpp now includes this header instead of defining the
 * enum itself, so the enum's name/values/namespace are unchanged for every existing caller.
 */
enum class TextFormat
{
    Plain,
    Markdown,
    Html
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_TEXT_FORMAT_HPP
