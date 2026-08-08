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

/** @file uise/desktop/utils/mimedatautils.hpp
*
*  Declares helpers for inspecting QMimeData carried by a drag-and-drop or paste operation,
*  shared between FileUploadWidget and FileDropOverlay so both agree on what counts as an
*  image and what counts as an acceptable payload.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MIMEDATAUTILS_HPP
#define UISE_DESKTOP_MIMEDATAUTILS_HPP

#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

class QMimeData;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Get the list of raw image MIME formats accepted as pasted/dropped payload data.
 * @return A format list such as "image/png"/"image/jpeg"/"image/webp" -- content actually
 *  carried inline in the QMimeData, as opposed to a local file's own MIME type.
 */
UISE_DESKTOP_EXPORT const QStringList& acceptedImageMimeFormats();

/**
 * @brief Get local filesystem paths carried by mimeData's URL list.
 * @param mimeData Payload to inspect; nullptr is treated as empty.
 * @return Paths of every URL that is a local file, in list order; non-local URLs are skipped.
 */
UISE_DESKTOP_EXPORT QStringList mimeDataLocalFilePaths(const QMimeData* mimeData);

/**
 * @brief Check whether mimeData carries at least one image.
 * @param mimeData Payload to inspect; nullptr is treated as not containing images.
 * @return true if mimeData->hasImage(), or it carries one of acceptedImageMimeFormats(), or
 *  any of its local-file URLs sniffs (by extension, via QMimeDatabase) as an "image" mime type.
 *
 * The last check matters more than it looks: an image file dragged from a file manager
 * (Finder/Explorer/most Linux file managers) is delivered as a text/uri-list with no "image"
 * format at all -- checking only hasImage()/acceptedImageMimeFormats() would misclassify the
 * single most common "drag an image" gesture as "no images".
 */
UISE_DESKTOP_EXPORT bool mimeDataHasImages(const QMimeData* mimeData);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MIMEDATAUTILS_HPP
