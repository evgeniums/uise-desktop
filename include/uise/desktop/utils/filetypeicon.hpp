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

/** @file uise/desktop/utils/filetypeicon.hpp
*
*  Declares fileTypeIcon() and fileTypeIconName().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILETYPEICON_HPP
#define UISE_DESKTOP_FILETYPEICON_HPP

#include <memory>

#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>

class QWidget;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Map a lowercased file extension to a curated tabler "file-type-*" icon name (see
 *  thirdparty/tabler-icons/outline/file-type-*.svg).
 * @param suffix Lowercased file extension, without the dot.
 * @return The icon name, or an empty string if suffix has no close match -- callers should fall
 *  back to a generic file icon rather than guess at a wrong file type.
 */
UISE_DESKTOP_EXPORT QString fileTypeIconName(const QString& suffix);

/**
 * @brief Get an icon representing a file's type, from its extension.
 * @param suffix Lowercased file extension, without the dot.
 * @param context Widget the icon will be painted in (for theme/mode resolution).
 * @param fallbackAlias Icon alias (resolved via Style::instance().svgIconLocator()) used when
 *  suffix has no curated file-type icon, or the corresponding resource is missing.
 * @return Never null.
 */
UISE_DESKTOP_EXPORT std::shared_ptr<SvgIcon> fileTypeIcon(
    const QString& suffix,
    QWidget* context,
    const QString& fallbackAlias=QStringLiteral("FileUpload::file")
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILETYPEICON_HPP
