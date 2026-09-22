/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/utils/audiotime.hpp
*
*  Formats an audio position or duration as a clock string.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUDIOTIME_HPP
#define UISE_DESKTOP_AUDIOTIME_HPP

#include <QChar>
#include <QString>
#include <QtGlobal>
#include <QFontMetrics>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Format milliseconds as "m:ss", or "h:mm:ss" from one hour up.
 * @param ms Time in milliseconds; negative values are shown as zero.
 *
 * Digits and a colon only, so it is the same in every language and needs no translation.
 */
inline QString formatAudioTime(qint64 ms)
{
    if (ms<0)
    {
        ms=0;
    }
    const qint64 total=ms/1000;
    const qint64 seconds=total%60;
    const qint64 minutes=(total/60)%60;
    const qint64 hours=total/3600;

    if (hours>0)
    {
        return QStringLiteral("%1:%2:%3")
                .arg(hours)
                .arg(minutes,2,10,QLatin1Char('0'))
                .arg(seconds,2,10,QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds,2,10,QLatin1Char('0'));
}

/**
 * @brief Format milliseconds as "m:ss.t", or "h:mm:ss.t" from one hour up, t being tenths of a second.
 * @param ms Time in milliseconds; negative values are shown as zero.
 *
 * Tenths are cut off, not rounded, so that the clock never shows a time that has not come yet.
 * For a clock that moves ten times a second, see the recorder dialog.
 */
inline QString formatAudioTimeTenths(qint64 ms)
{
    if (ms<0)
    {
        ms=0;
    }
    return QStringLiteral("%1.%2").arg(formatAudioTime(ms)).arg((ms/100)%10);
}

/**
 * @brief Replace every digit of \p text with the widest digit of \p metrics's font.
 *
 * A clock is drawn in a proportional font, where "0:01" and "0:02" are not the same number of
 * pixels -- a label showing one after the other has its size hint change on every tick, which (if
 * nothing reserves a stable width for it) reads as the label, and whatever is laid out beside it,
 * flickering in place. Reserving the SIZE HINT OF THIS MASK as the label's minimum width (see
 * AudioPlayerWidget::updateTimeLabelWidth() / VoiceRecorderDialog's own equivalent for the two
 * callers) fixes that: every digit is already at its widest, so no later value of the same digit
 * pattern can be wider.
 */
inline QString widestDigitsOf(const QString& text, const QFontMetrics& metrics)
{
    auto widest=QLatin1Char('0');
    int widestAdvance=-1;
    for (char digit='0';digit<='9';digit++)
    {
        const auto advance=metrics.horizontalAdvance(QLatin1Char(digit));
        if (advance>widestAdvance)
        {
            widestAdvance=advance;
            widest=QLatin1Char(digit);
        }
    }

    auto mask=text;
    for (auto& ch: mask)
    {
        if (ch.isDigit())
        {
            ch=widest;
        }
    }
    return mask;
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_AUDIOTIME_HPP
