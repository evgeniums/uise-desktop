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

/** @file uise/desktop/utils/folderupload.hpp
*
*  Declares helpers for a folder dropped where files are expected: telling that the payload is
*  exactly one folder, and whether the folder's files can be offered one by one.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FOLDERUPLOAD_HPP
#define UISE_DESKTOP_FOLDERUPLOAD_HPP

#include <QString>
#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

class QMimeData;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Get the folder carried by mimeData, when the payload is exactly one folder.
 * @param mimeData Payload to inspect; nullptr is treated as empty.
 * @return Cleaned absolute path of the folder when mimeData's URL list holds a single URL and it
 *  is a local folder (a symlink to a folder counts); an empty string otherwise -- no URLs, a
 *  file, several URLs of any kind, a non-local URL. A drag of a folder together with anything
 *  else is deliberately not "a folder": such a payload keeps its ordinary file handling.
 */
UISE_DESKTOP_EXPORT QString mimeDataSingleLocalFolder(const QMimeData* mimeData);

/**
 * @brief Result of scanFolderForUpload().
 */
struct FolderUploadScan
{
    /**
     * Absolute paths of the visible files directly inside the folder, in natural name order
     * ("img2" before "img10"). INCOMPLETE once hasSubfolders or exceeded is set -- the scan
     * stops at that point, so it never costs more than the limit asks for.
     */
    QStringList files;

    bool hasSubfolders=false; //!< a visible subfolder (or a link to one) was found
    bool exceeded=false;      //!< more files than the limit allows were found

    /**
     * @brief Whether the folder's files can be sent one by one: no subfolders, at least one
     *  file, no more than the limit.
     */
    bool filesEligible() const noexcept
    {
        return !hasSubfolders && !exceeded && !files.isEmpty();
    }
};

/**
 * @brief Look at the top level of a folder to decide whether its files can be offered one by one.
 * @param folderPath Folder to look at.
 * @param maxFiles Largest number of files that still counts as eligible; <=0 means no limit.
 *
 * Not recursive. Hidden entries (dot-files, and files with the hidden attribute on Windows) are
 * ignored: they neither count against the limit nor appear in the result. A folder that cannot
 * be read gives an empty, therefore non-eligible, result.
 *
 * Meant to be cheap enough for a drag-enter handler: it stops at the first visible subfolder, and
 * at the first file past maxFiles.
 */
UISE_DESKTOP_EXPORT FolderUploadScan scanFolderForUpload(const QString& folderPath, int maxFiles);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FOLDERUPLOAD_HPP
