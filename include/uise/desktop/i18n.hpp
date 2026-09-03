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

/** @file uise/desktop/i18n.hpp
*
*  Declares uise-desktop's own translation catalog installer.
*
*  uise-desktop ships its own .ts/.qm catalogs (translations/uise_<lang>.ts, embedded under
*  :/uise/desktop/translations/ by the CMake i18n block) so that any application embedding this
*  library gets translated dialog buttons, file-upload prompts, chat message controls, etc.
*  without having to know uise-desktop's internal tr() contexts. A consuming application installs
*  its own translator(s) after calling installTranslator() here, so its own catalog takes
*  precedence (Qt consults translators in reverse install order).
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_I18N_HPP
#define UISE_DESKTOP_I18N_HPP

#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief List the language codes for which this build actually embeds a uise-desktop catalog.
 *
 * Read from the embedded :/uise/desktop/translations/manifest.json, generated at configure time
 * from whatever translations/uise_*.ts files existed in the source tree. Never throws; an absent
 * or malformed manifest yields an empty list (uise-desktop then simply contributes no translator,
 * which is a safe no-op -- every uise-desktop string still falls through to its English source).
 */
UISE_DESKTOP_EXPORT QStringList availableTranslations();

/**
 * @brief Install uise-desktop's own translator for @b language onto qApp.
 *
 * @param language Language code (e.g. "ru"). A regional code (e.g. "pt_BR") falls back to its
 *                 neutral prefix ("pt") if no exact catalog is embedded.
 *
 * @return true if a translator was found and installed, false otherwise (no catalog for this
 *         language was embedded in this build -- not an error, uise-desktop's strings simply stay
 *         in their English source form, which is always safe).
 *
 * Idempotent to call from a static-library consumer: internally calls Q_INIT_RESOURCE on
 * uise-desktop's translations resource before loading, so the caller never has to remember that
 * anchor. Safe to call multiple times; each call installs a fresh QTranslator (parented on qApp)
 * without removing a previously-installed one -- callers that want to switch language at runtime
 * should not rely on this (uise-desktop, like the rest of the app, expects a restart to change
 * language, see whitemdesktop's Language settings node).
 */
UISE_DESKTOP_EXPORT bool installTranslator(const QString& language);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_I18N_HPP
