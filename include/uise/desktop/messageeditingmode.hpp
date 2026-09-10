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

/** @file uise/desktop/messageeditingmode.hpp
*
*  Declares MessageEditingMode.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGEEDITINGMODE_HPP
#define UISE_DESKTOP_MESSAGEEDITINGMODE_HPP

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief The three editing modes AbstractMessageEditor's toolbar/context menu can switch between
 *  (task-message-formatting-plan.md, Stage 5a).
 *
 * Kept in its own header, the same way TextFormat was split out of abstractmessageeditor.hpp in
 * Stage 2 -- MessageEditorToolbar needs this enum and must not depend on the editor interface it
 * is embedded in.
 */
enum class MessageEditingMode
{
    Wysiwyg,     //!< rich document; the toolbar edits QTextCharFormat/QTextBlockFormat directly
    Markdown,    //!< the document holds markdown SOURCE, edited as plain text
    Plaintext    //!< literal text, no markup meaning at all
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MESSAGEEDITINGMODE_HPP
