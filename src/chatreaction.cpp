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

/** @file uise/desktop/src/chatreaction.cpp
*
*  Defines ChatReactionId helpers.
*
*/

/****************************************************************************/

#include <uise/desktop/chatreaction.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace ChatReactionId {

//--------------------------------------------------------------------------

QString make(const QString& iconId, const QString& packUri)
{
    if (packUri.isEmpty())
    {
        return iconId;
    }
    return iconId+QStringLiteral("@")+packUri;
}

//--------------------------------------------------------------------------

QString iconId(const QString& reactionId)
{
    // Split on the FIRST '@' -- icon ids are plain slugs and never contain one, while a pack
    // URI legitimately can (e.g. userinfo in a URL), so a naive last-'@' split would be wrong.
    auto pos=reactionId.indexOf(QChar('@'));
    if (pos<0)
    {
        return reactionId;
    }
    return reactionId.left(pos);
}

//--------------------------------------------------------------------------

QString packUri(const QString& reactionId)
{
    auto pos=reactionId.indexOf(QChar('@'));
    if (pos<0)
    {
        return QString{};
    }
    return reactionId.mid(pos+1);
}

//--------------------------------------------------------------------------

} // namespace ChatReactionId

UISE_DESKTOP_NAMESPACE_END
