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

/** @file uise/desktop/stylecontext.hpp
*
*  Declares StyleCOntext.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STYLE_CONTEXT_HPP
#define UISE_DESKTOP_STYLE_CONTEXT_HPP

#include <QObject>
#include <QSet>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class StyleContext
{
    public:

        StyleContext(QObject* = nullptr)
        {}

        const QSet<QString>& tags() const
        {
            return m_tags;
        }

    private:

        QSet<QString> m_tags;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STYLE_CONTEXT_HPP
