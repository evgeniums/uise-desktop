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

/** @file uise/desktop/abstractfileuploadwidget.hpp
*
*  Declares AbstractFileUploadWidget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTFILEUPLOADWIDGET_HPP
#define UISE_DESKTOP_ABSTRACTFILEUPLOADWIDGET_HPP

#include <QStringList>
#include <QImage>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/fileuploaditem.hpp>

class QMimeData;

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractMessageEditor;
class IconTextButton;

/**
 * @brief Ids of the entries in the header drop-down menu, kept in sync with the three
 *  checkboxes of the same meaning (see AbstractFileUploadWidget).
 */
enum class FileUploadMenuAction : int
{
    HighQuality=1,
    SendAsDocuments=2,
    GroupItems=3
};

/**
 * @brief Interface of a file upload widget: a preview list of files/images plus send options
 *  and a comment, as described in tasks/fileupload-dialog.md.
 *
 * The widget owns its own header (caption + a drop-down menu mirroring the checkboxes) and its
 * own button row (Add on the left, Cancel/Send on the right) rather than relying on Dialog<>'s
 * title bar and button frame -- Dialog<> lays out all of its buttons with a single alignment
 * (see ipp/dialog.ipp), so it cannot place Add on the left and Cancel/Send on the right.
 * FileUploadDialog (fileuploaddialog.hpp) hosts this widget with both hidden and relays its
 * addRequested()/cancelled()/sendRequested() as its own button clicks instead.
 */
class UISE_DESKTOP_EXPORT AbstractFileUploadWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual const FileUploadItems& items() const=0;
        virtual void setItems(FileUploadItems items)=0;

        /**
         * @brief Add files by path.
         * @return Number of files actually added (may be less than paths.size() if
         *  maxFileCount() was reached, see maxFileCountExceeded()).
         */
        virtual int addFiles(const QStringList& paths)=0;

        /**
         * @brief Add items, truncating to maxFileCount(). The single choke point every other
         *  add path (addFiles(), drag-and-drop, paste) routes through.
         * @return Number of items actually added.
         */
        virtual int addItems(FileUploadItems items)=0;

        virtual void removeItem(int index)=0;
        virtual void clearItems()=0;

        /**
         * @brief Read an item's image content, for handing off to an external editor.
         */
        virtual QImage itemImage(int index) const=0;

        /**
         * @brief Overwrite an item's image content, from an external editor.
         */
        virtual void setItemImage(int index, QImage image)=0;

        virtual void setItemFileName(int index, const QString& name)=0;

        void setMaxFileCount(int count) noexcept
        {
            m_maxFileCount=count;
        }

        int maxFileCount() const noexcept
        {
            return m_maxFileCount;
        }

        virtual bool isHighQuality() const=0;
        virtual void setHighQuality(bool enable)=0;

        virtual bool isSendAsDocuments() const=0;
        virtual void setSendAsDocuments(bool enable)=0;

        virtual bool isGroupItems() const=0;
        virtual void setGroupItems(bool enable)=0;

        virtual bool isRememberChoice() const=0;
        virtual void setRememberChoice(bool enable)=0;

        /**
         * @brief Get the current send options and comment, as handed to the consumer on Send.
         */
        virtual FileUploadOptions options() const=0;

        virtual AbstractMessageEditor* messageEditor() const=0;

        /**
         * @brief Get the header's drop-down menu button (visible only while items() has at
         *  least one image, per the task spec).
         */
        virtual IconTextButton* menuButton() const=0;

        virtual void setHeaderVisible(bool visible)=0;
        virtual void setButtonsVisible(bool visible)=0;
        virtual void setCommentsVisible(bool visible)=0;

        /**
         * @brief Get the current header caption ("Send as a file" / "3 images selected" / ...).
         */
        QString caption() const noexcept
        {
            return m_caption;
        }

    public slots:

        /**
         * @brief Return the widget to its just-constructed state: clear items() and the
         *  comment, restore the four toggles to their construction defaults, close any open
         *  menu, cancel an in-progress rename, and scroll back to the top.
         *
         * Emits itemsChanged() and, if the caption changes, captionChanged() -- but
         * deliberately NOT emptied(), so resetting a dialog for reuse never trips an
         * auto-close-when-empty policy the host may have wired to that signal.
         */
        virtual void reset()=0;

        virtual void pasteFromClipboard()=0;

        virtual void addFromMimeData(const QMimeData* mimeData)=0;

        /**
         * @brief Open a file picker (QFileDialog::getOpenFileNames) and add the chosen files.
         */
        virtual void requestAddFiles()=0;

    signals:

        void captionChanged(const QString& caption);

        void itemsChanged();

        /**
         * @brief Emitted once items() transitions from non-empty to empty.
         *
         * Fired once per emptying regardless of cause (removeItem()/clearItems()/setItems({})),
         * never for a widget that was already empty, and never from reset() -- see reset().
         */
        void emptied();

        void editImageRequested(int index);

        /**
         * @brief Emitted when addFiles()/addItems()/a drop/a paste would exceed maxFileCount().
         * @param rejectedCount Number of items that were NOT added because of the limit.
         */
        void maxFileCountExceeded(int rejectedCount);

        void addRequested();
        void cancelled();
        void sendRequested();

    protected:

        /**
         * @brief Set the header caption, emitting captionChanged() if it actually changed.
         */
        void setCaption(QString caption)
        {
            if (m_caption==caption)
            {
                return;
            }
            m_caption=std::move(caption);
            emit captionChanged(m_caption);
        }

    private:

        int m_maxFileCount=10;
        QString m_caption;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTFILEUPLOADWIDGET_HPP
