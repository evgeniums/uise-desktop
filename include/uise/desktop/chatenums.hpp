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

/** @file uise/desktop/chatenums.hpp
*
*  Defines enums used by the chat messages view framework, kept in a header of their own so that
*  code needing only the enums does not have to include the whole chatmessagesview.hpp template
*  stack.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATENUMS_HPP
#define UISE_DESKTOP_CHATENUMS_HPP

#include <cstdint>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//! Position of own (sent) messages -- app-settings-driven, see whitemdesktop's Appearance node
//! ("Position of my messages"). Auto resolves to Left/Right depending on the view's own width vs.
//! AbstractChatMessagesView::alignSentLeftWidth(); Left/Right are unconditional. Aliased as
//! AbstractChatMessagesView::AlignSentMode.
enum class ChatAlignSentMode : uint8_t
{
    Auto=0,
    Right=1,
    Left=2
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATENUMS_HPP
