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

/** @file uise/desktop/fileuploadwidget.hpp
*
*  Declares FileUploadWidget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILEUPLOADWIDGET_HPP
#define UISE_DESKTOP_FILEUPLOADWIDGET_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractfileuploadwidget.hpp>

class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QKeyEvent;
class QShowEvent;

UISE_DESKTOP_NAMESPACE_BEGIN

class FileUploadListItem;

class FileUploadWidget_p;

/**
 * @brief Concrete file upload widget: preview list, send options, comment and buttons, with
 *  drag-and-drop and clipboard paste support. See AbstractFileUploadWidget for the interface
 *  and tasks/fileupload-dialog.md for the full spec.
 *
 * Self-contained: works standalone (headerVisible/buttonsVisible/commentsVisible all default
 * true) or embedded in FileUploadDialog (which hides the header and buttons and drives them via
 * its own title bar and Dialog<> chrome instead).
 */
class UISE_DESKTOP_EXPORT FileUploadWidget : public AbstractFileUploadWidget
{
    Q_OBJECT

    Q_PROPERTY(int maxListAreaHeight READ maxListAreaHeight WRITE setMaxListAreaHeight)
    Q_PROPERTY(int minListAreaHeight READ minListAreaHeight WRITE setMinListAreaHeight)
    Q_PROPERTY(int maxCommentsHeight READ maxCommentsHeight WRITE setMaxCommentsHeight)
    Q_PROPERTY(int maxCommentLength READ maxCommentLength WRITE setMaxCommentLength)

    public:

        constexpr static const int DefaultMaxListAreaHeight=360;
        constexpr static const int DefaultMinListAreaHeight=88;
        constexpr static const int DefaultMaxCommentsHeight=110;
        constexpr static const int DefaultMaxCommentLength=1000;
        //! Out-of-the-box extreme-aspect-ratio limit, see
        //! AbstractFileUploadWidget::setMaxImageAspectRatio(). A caller that genuinely wants no
        //! check at all can still setMaxImageAspectRatio(0) explicitly.
        constexpr static const uint32_t DefaultMaxImageAspectRatio=10;

        explicit FileUploadWidget(QWidget* parent=nullptr);

        ~FileUploadWidget();

        FileUploadWidget(const FileUploadWidget&)=delete;
        FileUploadWidget(FileUploadWidget&&)=delete;
        FileUploadWidget& operator=(const FileUploadWidget&)=delete;
        FileUploadWidget& operator=(FileUploadWidget&&)=delete;

        const FileUploadItems& items() const override;
        void setItems(FileUploadItems items) override;
        int addFiles(const QStringList& paths) override;
        int addItems(FileUploadItems items) override;
        void removeItem(int index) override;
        void clearItems() override;

        QImage itemImage(int index) const override;
        void setItemImage(int index, QImage image) override;
        void setItemFileName(int index, const QString& name) override;

        bool isHighQuality() const override;
        void setHighQuality(bool enable) override;
        bool isSendAsDocuments() const override;
        void setSendAsDocuments(bool enable) override;
        bool isGroupItems() const override;
        void setGroupItems(bool enable) override;
        bool isRememberChoice() const override;
        void setRememberChoice(bool enable) override;

        void setMaxImageAspectRatio(uint32_t ratio) override;
        uint32_t maxImageAspectRatio() const noexcept override;

        FileUploadOptions options() const override;

        AbstractMessageEditor* messageEditor() const override;
        IconTextButton* menuButton() const override;

        void setHeaderVisible(bool visible) override;
        void setButtonsVisible(bool visible) override;
        void setCommentsVisible(bool visible) override;

        /**
         * @brief Set the tallest the preview list is allowed to grow to before it starts
         *  scrolling instead of growing further.
         *
         * The list otherwise tracks its content's natural height (see updateListAreaHeight()),
         * so a single item does not stretch to fill whatever space a host layout happens to
         * give this widget.
         */
        void setMaxListAreaHeight(int height);
        int maxListAreaHeight() const noexcept;

        /**
         * @brief Set the shortest the preview list is allowed to be, so the widget does not
         *  collapse to a sliver when empty.
         */
        void setMinListAreaHeight(int height);
        int minListAreaHeight() const noexcept;

        /**
         * @brief Set the tallest the comments editor is allowed to grow to.
         *
         * Beyond this the editor stops growing and its content scrolls (via mouse wheel/
         * keyboard -- AbstractMessageEditor does not expose its own scrollbar) instead of
         * pushing the rest of the dialog down indefinitely as the user keeps typing.
         */
        void setMaxCommentsHeight(int height);
        int maxCommentsHeight() const noexcept;

        /**
         * @brief Set the maximum comment length in characters, or <=0 for no limit.
         *
         * Enforced live: typing past the limit truncates the comment back down to it.
         */
        void setMaxCommentLength(int length);
        int maxCommentLength() const noexcept;

    public slots:

        void reset() override;
        void pasteFromClipboard() override;
        void addFromMimeData(const QMimeData* mimeData) override;
        void requestAddFiles() override;

    protected:

        void keyPressEvent(QKeyEvent* event) override;
        void showEvent(QShowEvent* event) override;

        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dragLeaveEvent(QDragLeaveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    private:

        bool acceptsMimeData(const QMimeData* mimeData) const;
        bool isAtMaxFileCount() const;
        void addRowFor(const FileUploadItem& item);
        /**
         * @brief Choose Image vs Row view for a row from its item's current isImage()/
         *  sendAsDocuments state, and -- when the outcome is Row (presented as a plain
         *  document, whatever the reason: the extreme-aspect-ratio guard or the user's own
         *  "send as documents" choice) -- ensure the underlying item has a fileName() to
         *  display, generating one via FileUploadItem::ensureFileName() if it doesn't (a
         *  pasted/generated buffer-sourced image has none until named).
         * @param index This row's position in items()/pimpl->items, i.e. the same index
         *  ensureFileName() itself takes -- callers already know it (it's how they found `row`).
         */
        void applyViewToRow(FileUploadListItem* row, int index);
        int indexOfRow(FileUploadListItem* row) const;

        void clearItemsInternal(bool notifyEmptied);
        void updateItemsState(bool wasEmpty, bool notifyEmptied);
        void updateListAreaHeight();
        void doUpdateListAreaHeight();
        void deferListAreaHeightUpdate();
        void updateCommentsAreaHeight();

        void updateCaption();
        void updateSendEnabled();
        void updateAddEnabled();
        void updateMenuVisibility();
        void enforceMaxCommentLength();

        std::unique_ptr<FileUploadWidget_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILEUPLOADWIDGET_HPP
