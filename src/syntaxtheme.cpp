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

/** @file uise/desktop/syntaxtheme.cpp
*
*  Defines SyntaxTheme.
*
*/

/****************************************************************************/

#include <QJsonDocument>
#include <QJsonObject>

#include <uise/desktop/style.hpp>
#include <uise/desktop/syntaxtheme.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

QString syntaxBucketName(SyntaxBucket bucket)
{
    // Not translated -- these are JSON keys read back by SyntaxTheme::loadFromJson(), not
    // user-facing text, so they stay stable identifiers regardless of locale.
    switch (bucket)
    {
        case (SyntaxBucket::Keyword): return QStringLiteral("keyword");
        case (SyntaxBucket::Type): return QStringLiteral("type");
        case (SyntaxBucket::Literal): return QStringLiteral("literal");
        case (SyntaxBucket::Callable): return QStringLiteral("callable");
        case (SyntaxBucket::Comment): return QStringLiteral("comment");
        case (SyntaxBucket::Text): break;
    }
    // SyntaxBucket::Text has no JSON bucket by design -- see this function's own doc comment.
    return QString{};
}

//--------------------------------------------------------------------------

bool SyntaxTheme::loadFromJson(const QString& json, QString* errorMessage)
{
    QJsonParseError ec;
    auto src=json.toUtf8();

    auto doc=QJsonDocument::fromJson(src, &ec);
    if (doc.isNull())
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=ec.errorString();
        }
        return false;
    }

    if (!doc.isObject())
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=QObject::tr("json theme must be a JSON object","SyntaxTheme");
        }
        return false;
    }

    auto formatError=[errorMessage](const QString& msg, const QStringList& path)
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=QString(QObject::tr("%1 at path %2","SyntaxTheme").arg(msg,path.join(".")));
        }
        return false;
    };

    auto mustBeObject=[&formatError](const QStringList& path)
    {
        return formatError(QObject::tr("must be JSON object","SyntaxTheme"),path);
    };

    auto mustBeString=[&formatError](const QStringList& path)
    {
        return formatError(QObject::tr("must be string","SyntaxTheme"),path);
    };

    auto mustBeValidColor=[&formatError](const QStringList& path)
    {
        return formatError(QObject::tr("must be a valid colour","SyntaxTheme"),path);
    };

    auto obj=doc.object();

    // extract theme name
    QString themeField{"theme"};
    if (obj.contains(themeField))
    {
        auto nameEl=obj.value(themeField);
        if (!nameEl.isString())
        {
            return mustBeString(QStringList(themeField));
        }
        m_name=nameEl.toString();
    }
    else
    {
        m_name=Style::AnyColorTheme;
    }

    // extract bucket colours
    auto bucketsField=obj.value("buckets");
    if (!bucketsField.isObject())
    {
        return mustBeObject({"buckets"});
    }

    auto bucketsObj=bucketsField.toObject();
    for (auto it=bucketsObj.begin();it!=bucketsObj.end();++it)
    {
        QStringList path{"buckets",it.key()};

        auto val=it.value();
        if (!val.isString())
        {
            return mustBeString(path);
        }

        auto str=val.toString();
        QColor color(str);
        if (!color.isValid())
        {
            return mustBeValidColor(path);
        }

        m_colors[it.key()]=color;
    }

    return true;
}

}
