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

/** @file uise/desktop/src/filenamevalidator.cpp
*
*  Defines FileNameValidator.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QCoreApplication>
#include <QRegularExpression>

#include <uise/desktop/utils/filenamevalidator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

constexpr int MaxNameBytes=255;

bool isForbiddenChar(QChar c)
{
    if (c.unicode()<0x20 || c.unicode()==0x7F)
    {
        return true;
    }
    switch (c.unicode())
    {
        case '/': case '\\': case ':': case '*': case '?': case '"': case '<': case '>': case '|':
            return true;
        default:
            break;
    }
    return false;
}

bool isReservedName(const QString& name)
{
    auto dot=name.indexOf(QLatin1Char('.'));
    auto stem=(dot<0 ? name : name.left(dot)).trimmed();
    static const QRegularExpression re(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$"),
        QRegularExpression::CaseInsensitiveOption
    );
    return re.match(stem).hasMatch();
}

}

//--------------------------------------------------------------------------

FileNameValidator::Problem FileNameValidator::check(const QString& name)
{
    for (const auto& c : name)
    {
        if (isForbiddenChar(c))
        {
            return Problem::ForbiddenChar;
        }
    }
    if (name.toUtf8().size()>MaxNameBytes)
    {
        return Problem::TooLong;
    }
    if (name.trimmed().isEmpty())
    {
        return Problem::Empty;
    }
    if (name.front().isSpace() || name.back().isSpace())
    {
        return Problem::EdgeWhitespace;
    }
    if (name.front()==QLatin1Char('.'))
    {
        return Problem::LeadingDot;
    }
    if (name.back()==QLatin1Char('.'))
    {
        return Problem::TrailingDot;
    }
    if (name.contains(QLatin1String("..")))
    {
        return Problem::ConsecutiveDots;
    }
    if (isReservedName(name))
    {
        return Problem::ReservedName;
    }
    return Problem::None;
}

//--------------------------------------------------------------------------

FileNameValidator::State FileNameValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos)
    switch (check(input))
    {
        case Problem::None:
            return Acceptable;
        case Problem::ForbiddenChar:
        case Problem::TooLong:
            return Invalid;
        default:
            break;
    }
    return Intermediate;
}

//--------------------------------------------------------------------------

QString FileNameValidator::problemText(Problem problem)
{
    // translate() with an explicit context: no Q_OBJECT needed and immune to the lupdate
    // macro-namespace issue
    auto t=[](const char* text)
    {
        return QCoreApplication::translate("uise::FileNameValidator",text);
    };
    switch (problem)
    {
        case Problem::ForbiddenChar:
            return t("A file name cannot contain / \\ : * ? \" < > | or control characters");
        case Problem::TooLong:
            return t("The file name is too long");
        case Problem::Empty:
            return t("The file name cannot be empty");
        case Problem::EdgeWhitespace:
            return t("The file name cannot start or end with a space");
        case Problem::LeadingDot:
            return t("The file name cannot start with a dot");
        case Problem::TrailingDot:
            return t("The file name cannot end with a dot");
        case Problem::ConsecutiveDots:
            return t("Dots in the file name must be separated by other symbols");
        case Problem::ReservedName:
            return t("This file name is reserved by the system");
        case Problem::None:
            break;
    }
    return QString();
}

//--------------------------------------------------------------------------

QString FileNameValidator::sanitize(const QString& name, const QString& fallback)
{
    if (check(name)==Problem::None)
    {
        return name;
    }

    auto result=name;

    auto sep=std::max(result.lastIndexOf(QLatin1Char('/')),result.lastIndexOf(QLatin1Char('\\')));
    if (sep>=0)
    {
        result=result.mid(sep+1);
    }

    for (auto& c : result)
    {
        if (isForbiddenChar(c))
        {
            c=QLatin1Char('_');
        }
    }

    static const QRegularExpression dots(QStringLiteral("\\.{2,}"));
    result.replace(dots,QStringLiteral("."));

    auto trim=[](QString& str)
    {
        auto isEdge=[](QChar c)
        {
            return c.isSpace() || c==QLatin1Char('.');
        };
        while (!str.isEmpty() && isEdge(str.front()))
        {
            str.remove(0,1);
        }
        while (!str.isEmpty() && isEdge(str.back()))
        {
            str.chop(1);
        }
    };
    trim(result);

    if (result.toUtf8().size()>MaxNameBytes)
    {
        auto dot=result.lastIndexOf(QLatin1Char('.'));
        auto ext=dot>0 ? result.mid(dot) : QString();
        // a huge "extension" is not worth preserving
        if (ext.toUtf8().size()>32)
        {
            ext.clear();
        }
        auto stem=ext.isEmpty() ? result : result.left(dot);
        while (!stem.isEmpty() && (stem.toUtf8().size()+ext.toUtf8().size())>MaxNameBytes)
        {
            stem.chop(1);
        }
        if (!stem.isEmpty() && stem.back().isHighSurrogate())
        {
            stem.chop(1);
        }
        trim(stem);
        result=stem+ext;
    }

    if (isReservedName(result))
    {
        result.prepend(QLatin1Char('_'));
    }

    if (check(result)!=Problem::None)
    {
        return fallback;
    }
    return result;
}

UISE_DESKTOP_NAMESPACE_END
