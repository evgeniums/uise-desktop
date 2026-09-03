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

/** @file uise/desktop/fileuploadlistitem.hpp
*
*  Declares FileUploadListItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILEUPLOADLISTITEM_HPP
#define UISE_DESKTOP_FILEUPLOADLISTITEM_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/fileuploaditem.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RoundedImage;
class IconTextButton;

class FileUploadListItem_p;

/**
 * @brief One row of a FileUploadWidget's preview list.
 *
 * A single class rendering two views of the same item, swapped by show/hide rather than
 * destroy/recreate (a QStackedLayout would size to the tallest page, and rebuilding ~10 rows
 * on every "Send as documents" toggle would lose any in-flight preview decoding):
 *
 * - View::Image: a large preview, its pixel size and content size below it, with the menu and
 *   remove buttons floating over its top-right corner. Used for images not in send-as-documents
 *   mode.
 * - View::Row: a small thumbnail (or a generic/per-extension file icon for non-images) beside
 *   an elided-in-the-middle, renameable file name and its content size, with the menu and
 *   remove buttons at the end of the row. Used for everything else, including images while
 *   send-as-documents mode is on.
 *
 * setItem()/refresh() do NOT choose a view on their own -- the owning list decides, from both
 * the item's type and the list's current send-as-documents mode, and calls setView()
 * explicitly. A freshly constructed item defaults to View::Row.
 */
class UISE_DESKTOP_EXPORT FileUploadListItem : public QFrame
{
    Q_OBJECT

    public:

        enum class View : uint8_t
        {
            Image,
            Row
        };

        FileUploadListItem(QWidget* parent=nullptr);

        ~FileUploadListItem();

        FileUploadListItem(const FileUploadListItem&)=delete;
        FileUploadListItem(FileUploadListItem&&)=delete;
        FileUploadListItem& operator=(const FileUploadListItem&)=delete;
        FileUploadListItem& operator=(FileUploadListItem&&)=delete;

        void setItem(const FileUploadItem& item);
        const FileUploadItem& item() const;

        /**
         * @brief Re-read item() into the preview/name/info/menu widgets.
         *
         * Called by setItem(); also useful after mutating item() in place (e.g. via a
         * non-const reference obtained elsewhere) without replacing it wholesale.
         */
        void refresh();

        void setView(View view);

        View view() const noexcept;

        /**
         * @brief Get the preview widget of the currently active view.
         */
        RoundedImage* preview() const;

        IconTextButton* menuButton() const;

        /**
         * @brief Put the file name into inline editing mode.
         *
         * The name field only exists in View::Row's layout; calling this while View::Image is
         * active switches to View::Row first (there is nothing to show the edit in otherwise).
         */
        void beginRename();

        /**
         * @brief Apply an in-progress inline rename, if one is active.
         *
         * If the file name field is currently in editing mode (beginRename() was called but
         * the user hasn't pressed Enter or clicked the apply control yet), this applies the
         * pending edit, which synchronously emits renameRequested() with the new name.
         * Does nothing if the name field isn't being edited.
         */
        void commitPendingRename();

        /**
         * @brief Close the per-item drop-down menu if open, without animation.
         *
         * Meant for a host that embeds this item in a scrolling list to call whenever the
         * list scrolls: the menu is parented to window(), not to this item, so it would
         * otherwise float detached from its (moving) menu button.
         */
        void closeMenu();

    signals:

        void editRequested();

        /**
         * @brief Emitted once an inline rename is committed (Enter/the accept control).
         * @param newName The new file name.
         */
        void renameRequested(const QString& newName);

        void removeRequested();

        /**
         * @brief Emitted when the preview image is clicked (View::Image only).
         */
        void previewClicked();

    protected:

        void resizeEvent(QResizeEvent* event) override;
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        void rebuildMenu();
        void updatePreview();
        void updateInfoLabels();
        void updateNameLabel();
        void repositionButtonsBlock();

    private slots:

        void onMenuItemTriggered(int id);

    private:

        std::unique_ptr<FileUploadListItem_p> pimpl;
};

}

#endif // UISE_DESKTOP_FILEUPLOADLISTITEM_HPP
