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

/** @file uise/desktop/messageeditor.hpp
*
*  Declares MessageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGEEDITOR_HPP
#define UISE_DESKTOP_MESSAGEEDITOR_HPP

#include <QTextEdit>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractmessageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT EnhancedTextEdit : public QTextEdit
{
    Q_OBJECT

    public:

        explicit EnhancedTextEdit(QWidget *parent = nullptr);

        QSize sizeHint() const override;

        void setAutoResizingEnabled(bool enable);
        bool isAutoResizingEnabled() const noexcept
        {
            return m_autoResize;
        }

        void setNewLineOnEnter(bool enable)
        {
            m_newLineOnEnter=enable;
        }

        bool isNewLineOnEnter() const noexcept
        {
            return m_newLineOnEnter;
        }

        /**
         * @brief Paste the clipboard's current content, image/file payloads included.
         *
         * Implemented as QTextEdit::paste() rather than e.g. insertPlainText(clipboard->text()):
         * QWidgetTextControl::paste() funnels straight into insertFromMimeData() with no
         * canPaste() gate in front of it, so an attachment payload still reaches
         * insertFromMimeData()'s attachmentsPasted() branch below, exactly as Ctrl+V does.
         */
        void pasteFromClipboard();

        /**
         * @brief Whether pasteFromClipboard() would do anything right now.
         *
         * Deliberately not just canPaste(): canPaste() consults canInsertFromMimeData(), which
         * returns false for an attachment payload by design (see canInsertFromMimeData() below),
         * so it would report nothing-to-paste for exactly the image/file case that has to work.
         */
        bool canPasteFromClipboard() const;

        /**
         * @brief Remove all text as a single undoable edit.
         *
         * Deliberately not QTextEdit::clear(): its documented behavior is to also purge the
         * undo/redo history, which is exactly right for MessageEditor::clear() (e.g. wiping the
         * composer after a message is sent -- an undo there must not resurrect the sent text)
         * but wrong for a user-facing "Clear" context-menu action, which is expected to be
         * undoable like any other edit.
         */
        void clearUndoable();

    signals:

        void returnPressed();
        void activated();

        /**
         * @brief See AbstractMessageEditor::attachmentsPasted() -- relayed there verbatim by
         *  MessageEditor. Emitted for Ctrl+V, context-menu Paste, and middle-click paste alike,
         *  since all three funnel through insertFromMimeData().
         */
        void attachmentsPasted(const QMimeData* mimeData);

    protected:

        void keyPressEvent(QKeyEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;

        /**
         * @brief Refuse a payload mimeDataHasAttachments() recognizes as an attachment, so Qt's
         *  own drag-and-drop machinery lets the drag propagate to an ancestor (e.g. a chat page's
         *  FileDropOverlay) instead of the editor claiming it as the drop target.
         */
        bool canInsertFromMimeData(const QMimeData* source) const override;

        /**
         * @brief For an attachment payload, emit attachmentsPasted() instead of inserting it into
         *  the document. Covers Ctrl+V, context-menu Paste, and middle-click paste uniformly, since
         *  QTextEdit funnels all three through this one override.
         */
        void insertFromMimeData(const QMimeData* source) override;

    private slots:

        void updateSize();

    private:

        bool m_autoResize;
        bool m_newLineOnEnter;
};

class MessageEditor_p;

class UISE_DESKTOP_EXPORT MessageEditor : public AbstractMessageEditor
{
    Q_OBJECT

    public:

        explicit MessageEditor(QWidget* parent=nullptr);

        ~MessageEditor();

        MessageEditor(const MessageEditor&) =delete;
        MessageEditor& operator=(const MessageEditor&) =delete;
        MessageEditor(MessageEditor&&) =delete;
        MessageEditor& operator=(MessageEditor&&) =delete;

        void loadText(const QString& text, TextFormat format=TextFormat::Markdown) override;

        QString text(TextFormat format=TextFormat::Markdown) const override;

        QString selectedText(TextFormat format=TextFormat::Markdown) const override;

        void setFocusIn() override;

        void setPlaceHolderText(const QString& text) override;

        bool hasSelection() const override;

        bool isEmpty() const override;

        bool canPasteFromClipboard() const override;

    public slots:

        void selectAll() override;

        void clearSelection() override;

        void clear() override;

        void cut() override;

        void copy() override;

        void paste() override;

    protected:

        void updateEditingMode() override;
        virtual void updateFinishOnEnter() override;
        virtual void updateEditingFinished() override;

    private:

        void setupReturnPressed();

        std::unique_ptr<MessageEditor_p> pimpl;

    private slots:

        void showContextMenu(const QPoint& pos);
        void onContextMenuItemTriggered(int id);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MESSAGEEDITOR_HPP
