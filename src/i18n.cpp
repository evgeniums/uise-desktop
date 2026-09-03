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

/** @file uise/desktop/i18n.cpp
*
*  Implements uise-desktop's own translation catalog installer.
*
*/

/****************************************************************************/

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTranslator>

#include <uise/desktop/i18n.hpp>

// Force-link the Qt resource generated from the CMake i18n block
// (<build>/uise_translations.qrc, embedding translations/uise_<lang>.qm under
// :/uise/desktop/translations/). uise-desktop can be built STATIC
// (UISE_DESKTOP_STATIC), in which case the linker would otherwise strip this qrc's
// object file since nothing references a symbol in it. Anchoring the call here --
// rather than requiring every consumer to remember it -- means installTranslator()
// always works regardless of how uise-desktop was linked. Must live in the global
// namespace: Q_INIT_RESOURCE declares an extern symbol that would not match the
// generated one if mangled into a C++ namespace. Safe and idempotent in shared
// builds too -- qRegisterResourceData deduplicates a repeat registration.
static void uise_initTranslationsResource()
{
    Q_INIT_RESOURCE(uise_translations);
}

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

constexpr const char* TranslationsPrefix = ":/uise/desktop/translations";
constexpr const char* CatalogBase = "uise";

} // anonymous namespace

//--------------------------------------------------------------------------

QStringList availableTranslations()
{
    uise_initTranslationsResource();

    QFile f(QString::fromLatin1(TranslationsPrefix) + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly))
    {
        return {};
    }
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
    {
        return {};
    }
    auto arr = doc.object().value(QStringLiteral("languages")).toArray();
    QStringList result;
    result.reserve(arr.size());
    for (const auto& v : arr)
    {
        auto s = v.toString();
        if (!s.isEmpty())
        {
            result.append(s);
        }
    }
    return result;
}

//--------------------------------------------------------------------------

bool installTranslator(const QString& language)
{
    uise_initTranslationsResource();

    QStringList candidates{language};
    auto sep = language.indexOf(QLatin1Char('_'));
    if (sep > 0)
    {
        // Regional code (pt_BR) falls back to its neutral prefix (pt) if this build
        // only embeds the neutral catalog.
        candidates << language.left(sep);
    }

    for (const auto& candidate : candidates)
    {
        auto* translator = new QTranslator(qApp);
        if (translator->load(
                QString::fromLatin1(CatalogBase) + QLatin1Char('_') + candidate,
                QString::fromLatin1(TranslationsPrefix)))
        {
            QCoreApplication::installTranslator(translator);
            return true;
        }
        delete translator;
    }
    return false;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
