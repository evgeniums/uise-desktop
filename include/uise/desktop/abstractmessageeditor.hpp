/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractmessageeditor.hpp
*
*  Declares AbstractMessageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP
#define UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP

#include <functional>
#include <vector>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/dropdownmenu.hpp>

class QMimeData;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

enum class TextFormat
{
    Plain,
    Markdown,
    Html
};

/**
 * @brief Ids of the standard rows AbstractMessageEditor's own context menu builds and handles
 *  itself (see AbstractMessageEditor::setContextMenuHandler()).
 */
enum class MessageEditorMenuAction
{
    Cut=1,
    Copy=2,
    Paste=3,
    SelectAll=4,
    Clear=5,

    //! First id free for a consumer's own items -- the editor never acts on an id >= this
    //! itself, it only relays it through contextMenuItemTriggered().
    UserAction=1000
};

class UISE_DESKTOP_EXPORT AbstractMessageEditor : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        /**
         * @brief Callback invoked right before the context menu opens.
         * @param items The menu's items, already populated with the standard Cut/Copy/Paste/
         *  Select all/Clear rows and their isEnabled already computed from live editor state.
         *  The handler may append, remove, reorder, relabel or re-enable anything in place;
         *  leaving the vector empty suppresses the menu for that click. An id the handler adds
         *  is never acted on by the editor itself -- it comes back via contextMenuItemTriggered().
         */
        using ContextMenuHandler=std::function<void(std::vector<MenuItem>& items)>;

        virtual void loadText(const QString& text, TextFormat format=TextFormat::Markdown) =0;

        virtual QString text(TextFormat format=TextFormat::Markdown) const =0;

        virtual QString selectedText(TextFormat format=TextFormat::Markdown) const =0;

        void setEditingMode(Qt::TextFormat mode)
        {
            m_editingMode=mode;
            updateEditingMode();
        }

        Qt::TextFormat editingMode() const noexcept
        {
            return m_editingMode;
        }

        void setFinishOnEnter(bool enable)
        {
            m_finishOnEnter=enable;
            updateFinishOnEnter();
        }

        bool isFinishOnEnter() const noexcept
        {
            return m_finishOnEnter;
        }

        virtual void setFocusIn() =0;

        virtual void setPlaceHolderText(const QString& text) =0;

        /**
         * @brief Set the handler consulted right before the context menu opens.
         * @param handler See ContextMenuHandler.
         */
        void setContextMenuHandler(ContextMenuHandler handler)
        {
            m_contextMenuHandler=std::move(handler);
        }

        void setContextMenuEnabled(bool enable) noexcept
        {
            m_contextMenuEnabled=enable;
        }

        bool isContextMenuEnabled() const noexcept
        {
            return m_contextMenuEnabled;
        }

        virtual bool hasSelection() const =0;

        virtual bool isEmpty() const =0;

        virtual bool canPasteFromClipboard() const =0;

    public slots:

        void finishEditing()
        {
            updateEditingFinished();
            emit editingFinished();
        }

        virtual void selectAll() =0;

        virtual void clearSelection() =0;

        virtual void clear() =0;

        virtual void cut() =0;

        virtual void copy() =0;

        virtual void paste() =0;

    signals:

        void textChanged();
        void editingFinished();
        void activated();

        /**
         * @brief A paste/drop the editor recognized as an attachment payload (mimeDataHasAttachments())
         *  rather than text, and therefore did not insert into the document.
         * @param mimeData Payload. Emitted synchronously from inside the triggering event handler, so
         *  mimeData is only valid for the duration of the slot -- consume it there (e.g. via
         *  AbstractFileUploadWidget::addFromMimeData(), which copies everything it needs) or copy it
         *  yourself, same contract as FileDropOverlay::dropped().
         */
        void attachmentsPasted(const QMimeData* mimeData);

        /**
         * @brief A plain Up-arrow was pressed while the editor was EMPTY.
         *
         * Purely a gesture report -- this widget does nothing else with it. A chat host typically
         * uses it to reopen the most recent message for editing, the usual messenger shortcut.
         * Emitted only while the editor is empty, so acting on it can never clobber a draft the
         * user is part-way through typing; with any text present, Up keeps its normal
         * caret-movement meaning and this is not emitted at all.
         */
        void editPreviousRequested();

        /**
         * @brief A context-menu item was triggered whose id is not one of the standard
         *  MessageEditorMenuAction rows the editor already handles itself -- i.e. an item a
         *  ContextMenuHandler added.
         */
        void contextMenuItemTriggered(int id);

    protected:

        virtual void updateEditingMode() {}
        virtual void updateFinishOnEnter() {}
        virtual void updateEditingFinished() {}

        const ContextMenuHandler& contextMenuHandler() const
        {
            return m_contextMenuHandler;
        }

    private:

        Qt::TextFormat m_editingMode=Qt::RichText;
        bool m_finishOnEnter=true;
        bool m_contextMenuEnabled=true;
        ContextMenuHandler m_contextMenuHandler;
};

}

#endif // UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP
