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

/** @file uise/desktop/utils/filesizeformat.hpp
*
*  Declares formatFileSize().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILESIZEFORMAT_HPP
#define UISE_DESKTOP_FILESIZEFORMAT_HPP

#include <QString>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Format a byte count as a human-readable size, e.g. "245 kb".
 * @param bytes Size in bytes. Negative values are treated as 0.
 * @param decimals Number of fractional digits, default 0 (whole units, matching how file
 *  sizes are shown throughout the file upload widgets).
 * @return Formatted size with a lower-case unit suffix ("kb"/"mb"/"gb").
 *
 * Below 1 mb the value is shown in kb; a non-empty file with decimals==0 never rounds down to
 * "0 kb" -- it shows "1 kb" instead.
 */
inline QString formatFileSize(qint64 bytes, int decimals=0)
{
    if (bytes<0)
    {
        bytes=0;
    }

    constexpr double Kb=1024.0;
    constexpr double Mb=Kb*1024.0;
    constexpr double Gb=Mb*1024.0;

    double value=0.0;
    QString unit;
    if (bytes>=static_cast<qint64>(Gb))
    {
        value=static_cast<double>(bytes)/Gb;
        unit=QStringLiteral("gb");
    }
    else if (bytes>=static_cast<qint64>(Mb))
    {
        value=static_cast<double>(bytes)/Mb;
        unit=QStringLiteral("mb");
    }
    else
    {
        value=static_cast<double>(bytes)/Kb;
        unit=QStringLiteral("kb");
    }

    auto text=QString::number(value,'f',decimals);
    if (decimals==0 && bytes>0 && text.toInt()==0)
    {
        text=QStringLiteral("1");
    }
    return QStringLiteral("%1 %2").arg(text,unit);
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILESIZEFORMAT_HPP
