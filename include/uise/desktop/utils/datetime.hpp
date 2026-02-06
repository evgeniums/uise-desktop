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

/** @file uise/desktop/utils/atetime.hpp
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DATETIME_HPP
#define UISE_DESKTOP_DATETIME_HPP

#include <QDateTime>
#include <QRegularExpression>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

inline QString printCurrentDateTime()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

inline QString dateAsMonthAndDay(const QDateTime& dt, const QLocale& locale=QLocale{})
{
    static QRegularExpression rx("[,./-]*\\s?y+\\s?[,./-]*",QRegularExpression::CaseInsensitiveOption);
    QString localeFormat = locale.dateFormat(QLocale::LongFormat);
    QString dateFormat = localeFormat.remove(rx).trimmed();
    return locale.toString(dt,dateFormat);
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DATETIME_HPP
