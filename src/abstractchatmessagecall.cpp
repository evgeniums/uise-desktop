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

UISE_DESKTOP_NAMESPACE_BEGIN

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

    appendPart(days,tr("d"));
    appendPart(hours,tr("h"));
    appendPart(minutes,tr("m"));
    appendPart(seconds,tr("s"));

    return result;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
