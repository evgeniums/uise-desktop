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

/** @file uise/desktop/abstractchatmessagecall.cpp
*
*  Defines AbstractChatMessageCall.
*
*/

/****************************************************************************/

#include <uise/desktop/abstractchatmessagecall.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

QString AbstractChatMessageCall::selectedText() const
{
    return formatText();
}

//--------------------------------------------------------------------------

QString AbstractChatMessageCall::formatText() const
{
    if (chatMessage()==nullptr)
    {
        return QString{};
    }

    switch (m_status)
    {
    case Status::Missed:
        if (chatMessage()->isIncoming())
        {
            return tr("Missed incoming call");
        }
        else
        {
            return tr("Unanswered outgoing call");
        }
    case Status::Complete:
        if (chatMessage()->isIncoming())
        {
            return tr("Incoming call %1").arg(formatDuration());
        }
        else
        {
            return tr("Outgoing call %1").arg(formatDuration());
        }
        break;
    case Status::Declined:
        if (chatMessage()->isIncoming())
        {
            return tr("Declined incoming call");
        }
        else
        {
            return tr("Declined outgoing call");
        }
        break;
    case Status::Failed:
        if (chatMessage()->isIncoming())
        {
            return tr("Failed incoming call");
        }
        else
        {
            return tr("Failed outgoing call");
        }
        break;
    }

    return QString{};
}

//--------------------------------------------------------------------------

QString AbstractChatMessageCall::formatDuration() const
{
    uint32_t remaining=m_duration;

    if (remaining<60)
    {
        //: Abbreviation appended directly after a number of seconds in a call duration
        //: (e.g. "42s"), no separator. Keep as short as the source in every language.
        return QString::number(remaining)+tr("s");
    }

    const uint32_t days=remaining/86400;
    remaining%=86400;
    const uint32_t hours=remaining/3600;
    remaining%=3600;
    const uint32_t minutes=remaining/60;
    const uint32_t seconds=remaining%60;

    QString result;
    const auto appendPart=[&result](uint32_t value, const QString& suffix)
    {
        if (value==0)
        {
            return;
        }
        if (!result.isEmpty())
        {
            result+=QLatin1Char(' ');
        }
        result+=QString::number(value)+suffix;
    };

    //: Abbreviation appended directly after a number of days in a call duration (e.g. "2d 3h"),
    //: no separator. Keep as short as the source in every language.
    appendPart(days,tr("d"));
    //: Same idiom as "d" above, but for hours (e.g. "3h 5m").
    appendPart(hours,tr("h"));
    //: Same idiom as "d" above, but for minutes -- NOT months (e.g. "5m 12s").
    appendPart(minutes,tr("m"));
    //: Same idiom as "d" above, but for seconds (e.g. "12s").
    appendPart(seconds,tr("s"));

    return result;
}

//--------------------------------------------------------------------------

}
