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

/** @file uise/desktop/colorpalette.cpp
*
*  Defines ColorPaletteTheme and the palette index helpers.
*
*/

/****************************************************************************/

#include <cstring>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include <uise/desktop/style.hpp>
#include <uise/desktop/colorpalette.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

size_t paletteIndex(const QByteArray& sha1Digest, size_t count)
{
    if (count==0 || static_cast<size_t>(sha1Digest.size())<sizeof(size_t))
    {
        return 0;
    }

    size_t idx=0;
    memcpy(&idx,sha1Digest.constData(),sizeof(idx));
    return idx%count;
}

//--------------------------------------------------------------------------

size_t paletteIndexForKey(QByteArrayView key, size_t count)
{
    QCryptographicHash hash{QCryptographicHash::Sha1};
    hash.addData(key);
    return paletteIndex(hash.result(),count);
}

//--------------------------------------------------------------------------

bool ColorPaletteTheme::loadFromJson(const QString& json, QString* errorMessage)
{
    // The error text is deliberately NOT translated: it only ever reaches qWarning() (see
    // Style::reloadStyleSheet()), addressed to whoever authors the palette JSON, never to an
    // end user -- so it stays out of the .ts catalogs (and out of whitemdesktop's i18n journals).
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
            *errorMessage=QStringLiteral("json theme must be a JSON object");
        }
        return false;
    }

    auto formatError=[errorMessage](const QString& msg, const QStringList& path)
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=QStringLiteral("%1 at path %2").arg(msg,path.join("."));
        }
        return false;
    };

    auto obj=doc.object();

    // extract theme name
    QString themeField{"theme"};
    if (obj.contains(themeField))
    {
        auto nameEl=obj.value(themeField);
        if (!nameEl.isString())
        {
            return formatError(QStringLiteral("must be string"),QStringList(themeField));
        }
        m_name=nameEl.toString();
    }
    else
    {
        m_name=Style::AnyColorTheme;
    }

    // extract palettes
    auto palettesField=obj.value("palettes");
    if (!palettesField.isObject())
    {
        return formatError(QStringLiteral("must be JSON object"),{"palettes"});
    }

    auto palettesObj=palettesField.toObject();
    for (auto it=palettesObj.begin();it!=palettesObj.end();++it)
    {
        QStringList path{"palettes",it.key()};

        auto val=it.value();
        if (!val.isArray())
        {
            return formatError(QStringLiteral("must be JSON array"),path);
        }

        std::vector<QColor> colors;
        auto arr=val.toArray();
        colors.reserve(static_cast<size_t>(arr.size()));
        for (qsizetype i=0;i<arr.size();i++)
        {
            auto el=arr.at(i);
            QColor color;
            if (el.isString())
            {
                color=QColor(el.toString());
            }
            if (!color.isValid())
            {
                // skipped, not fatal: see this class's own doc comment
                qWarning() << "Invalid colour at palettes." << it.key() << "[" << i << "], skipped";
                continue;
            }
            colors.emplace_back(color);
        }

        m_palettes[it.key()]=std::move(colors);
    }

    return true;
}

//--------------------------------------------------------------------------

}
