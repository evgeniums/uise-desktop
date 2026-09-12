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

/** @file uise/desktop/abstractchatmessageerror.cpp
*
*  Defines AbstractChatMessageError.
*
*/

/****************************************************************************/

#include <uise/desktop/abstractchatmessageerror.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

QString AbstractChatMessageError::selectedText() const
{
    auto title=formatTitle();
    auto description=formatDescription();
    if (description.isEmpty())
    {
        return title;
    }
    return title+QChar('\n')+description;
}

//--------------------------------------------------------------------------

QString AbstractChatMessageError::formatTitle() const
{
    return formatMessageErrorTitle(m_reason);
}

//--------------------------------------------------------------------------

QString AbstractChatMessageError::formatDescription() const
{
    return formatMessageErrorDescription(m_reason);
}

//--------------------------------------------------------------------------

// Free-function twins of the bodies above (see the doc comment on the declarations in
// abstractchatmessageerror.hpp): same tr() calls, same AbstractChatMessageError context, so
// existing translations keep resolving, but usable without a widget instance.

QString formatMessageErrorTitle(AbstractChatMessageError::Reason reason)
{
    using Reason=AbstractChatMessageError::Reason;

    switch (reason)
    {
        case (Reason::UnknownType):
            return AbstractChatMessageError::tr("Unsupported message type");

        case (Reason::NeedsNewerVersion):
            return AbstractChatMessageError::tr("This message needs a newer version");

        case (Reason::TooLarge):
            return AbstractChatMessageError::tr("Message is too large");

        case (Reason::Unreadable):
            return AbstractChatMessageError::tr("This message could not be read");
    }

    return QString{};
}

//--------------------------------------------------------------------------

QString formatMessageErrorDescription(AbstractChatMessageError::Reason reason)
{
    using Reason=AbstractChatMessageError::Reason;

    switch (reason)
    {
        case (Reason::UnknownType):
        case (Reason::NeedsNewerVersion):
            return AbstractChatMessageError::tr("Update the application to see this message.");

        case (Reason::TooLarge):
            return AbstractChatMessageError::tr("Ask the sender to send shorter messages.");

        case (Reason::Unreadable):
            return QString{};
    }

    return QString{};
}

//--------------------------------------------------------------------------

}
