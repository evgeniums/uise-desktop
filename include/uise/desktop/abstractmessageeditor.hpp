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
#include <uise/desktop/textformat.hpp>
#include <uise/desktop/messageeditingmode.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/dropdownmenu.hpp>

class QMimeData;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

// TextFormat now lives in its own header (task-message-formatting-plan.md, Stage 2) so a plain
// value type like ReplyPreviewData can see it without pulling in frame.hpp/dropdownmenu.hpp --
// re-exported here unchanged (same namespace, same name) via the #include above.

// MessageEditingMode lives in its own header for the same reason (task-message-formatting-plan.md,
// Stage 5a) -- MessageEditorToolbar needs the enum and must not depend on this interface --
// re-exported here unchanged via the #include above.

/**
 * @brief Ids of the standard rows AbstractMessageEditor's own context menu builds and handles
 *  itself (see AbstractMessageEditor::setContextMenuHandler()).
 *
 * Ids 10-49 are the Stage 5a formatting submenu (Formatting=10 is the submenu row itself); 50-51
 * and 60 are reserved for the Stage 5b hyperlink and Stage 6 mention rows respectively, so no
 * future id assignment can collide with a consumer's own id ( >= UserAction ).
 */
enum class MessageEditorMenuAction
{
    Cut=1,
    Copy=2,
    Paste=3,
    SelectAll=4,
    Clear=5,

    //! Submenu row grouping every formatting action below -- added to the menu only in
    //! MessageEditingMode::Wysiwyg (see MessageEditor::showContextMenu()). A ContextMenuHandler
    //! that wants formatting gone erases this single id rather than fifteen individual ones.
    Formatting=10,

    Bold=11,
    Italic=12,
    Underline=13,
    Strikethrough=14,
    InlineCode=15,

    BulletList=20,
    NumberedList=21,
    Blockquote=22,
    CodeBlock=23,
    Table=24,
    HorizontalRule=25,

    Heading1=30,
    Heading2=31,
    Heading3=32,
    HeadingNormal=33,

    ClearFormatting=40,

    //! Reserved for Stage 5b's hyperlink insert/remove -- never added to the menu in Stage 5a.
    Link=50,
    RemoveLink=51,

    //! Reserved for Stage 6's mention insertion -- never added to the menu in Stage 5a.
    Mention=60,

    //! First id free for a consumer's own items -- the editor never acts on an id >= this
    //! itself, it only relays it through contextMenuItemTriggered()/contextMenuItemToggled().
    UserAction=1000
};

/**
 * @brief Map a legacy Qt::TextFormat editing mode onto the new three-value MessageEditingMode.
 *
 * @deprecated Used only to implement AbstractMessageEditor::setEditingMode()'s forwarding onto
 *  setMessageEditingMode(). Qt::AutoText and Qt::MarkdownText both map to
 *  MessageEditingMode::Markdown -- MessageEditingMode is the single source of truth going
 *  forward, not Qt::TextFormat, so a caller should migrate to setMessageEditingMode() directly.
 */
inline MessageEditingMode messageEditingModeFromTextFormat(Qt::TextFormat format) noexcept
{
    switch (format)
    {
        case (Qt::PlainText): return MessageEditingMode::Plaintext;
        case (Qt::RichText): return MessageEditingMode::Wysiwyg;
        default: break;
    }
    return MessageEditingMode::Markdown;
}

/**
 * @brief Map a MessageEditingMode back onto the legacy Qt::TextFormat, for editingMode()'s
 *  deprecated forwarding.
 *
 * @deprecated Lossy in one leg: Qt::MarkdownText (in, via setEditingMode()) comes back out as
 *  Qt::AutoText -- no caller in either tree uses Qt::MarkdownText, so this is not a live
 *  regression, only a documented round-trip limitation of the deprecated API.
 */
inline Qt::TextFormat textFormatFromMessageEditingMode(MessageEditingMode mode) noexcept
{
    switch (mode)
    {
        case (MessageEditingMode::Plaintext): return Qt::PlainText;
        case (MessageEditingMode::Markdown): return Qt::AutoText;
        case (MessageEditingMode::Wysiwyg): break;
    }
    return Qt::RichText;
}

class UISE_DESKTOP_EXPORT AbstractMessageEditor : public WidgetQFrame
{
    Q_OBJECT

    //! QSS: qproperty-expandButtonVisible: true; -- whether the checkable icon-only button in
    //! the editor's bottom-left corner is shown at all (task-message-formatting-plan.md, Stage
    //! 5a). Default false: an existing host (whitemdesktop's composer included) that never opts
    //! in renders exactly as it did before this property existed -- no button, no toolbar, no
    //! extra height. A host that wants the expand/toolbar feature sets this true once.
    Q_PROPERTY(bool expandButtonVisible READ isExpandButtonVisible WRITE setExpandButtonVisible)

    //! QSS: qproperty-expanded: true; -- whether the editor is currently showing its formatting
    //! toolbar and clamped to its maximum height (true) or auto-resizing with the toolbar hidden
    //! (false, the default). Toggled by the user via the expand button, or set programmatically
    //! by a host that wants to open the editor already expanded.
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)

    public:

        using WidgetQFrame::WidgetQFrame;

        /**
         * @brief Callback invoked right before the context menu opens.
         * @param items The menu's items, already populated with the standard Cut/Copy/Paste/
         *  Select all/Clear rows (and, in MessageEditingMode::Wysiwyg, the Stage 5a Formatting
         *  submenu) with their isEnabled/isChecked already computed from live editor state.
         *  The handler may append, remove, reorder, relabel or re-enable anything in place;
         *  leaving the vector empty suppresses the menu for that click. An id the handler adds
         *  is never acted on by the editor itself -- it comes back via contextMenuItemTriggered()/
         *  contextMenuItemToggled().
         */
        using ContextMenuHandler=std::function<void(std::vector<MenuItem>& items)>;

        virtual void loadText(const QString& text, TextFormat format=TextFormat::Markdown) =0;

        virtual QString text(TextFormat format=TextFormat::Markdown) const =0;

        virtual QString selectedText(TextFormat format=TextFormat::Markdown) const =0;

        /**
         * @brief Set the three-value editing mode (Stage 5a) -- the single source of truth for
         *  how loadText()/text()/selectedText() interpret their document and how the toolbar/
         *  context menu behave. Superseded setEditingMode()/editingMode() below.
         */
        void setMessageEditingMode(MessageEditingMode mode)
        {
            auto changed=(m_mode!=mode);
            m_mode=mode;
            updateMessageEditingMode();
            if (changed)
            {
                emit messageEditingModeChanged(m_mode);
            }
        }

        MessageEditingMode messageEditingMode() const noexcept
        {
            return m_mode;
        }

        /**
         * @deprecated Use setMessageEditingMode(MessageEditingMode) instead. Forwards onto it via
         *  messageEditingModeFromTextFormat() -- Qt::PlainText/Qt::RichText map onto
         *  Plaintext/Wysiwyg exactly as before, and every other Qt::TextFormat value (AutoText,
         *  MarkdownText) maps onto Markdown. Kept only so an existing caller (e.g.
         *  whitemdesktop's ChatPageBottom::construct()) keeps compiling and behaving unchanged.
         */
        void setEditingMode(Qt::TextFormat mode)
        {
            setMessageEditingMode(messageEditingModeFromTextFormat(mode));
        }

        /**
         * @deprecated Use messageEditingMode() instead. See textFormatFromMessageEditingMode()
         *  for the (one-leg-lossy) mapping back.
         */
        Qt::TextFormat editingMode() const noexcept
        {
            return textFormatFromMessageEditingMode(m_mode);
        }

        //! Show/hide the bottom-left expand button. See the expandButtonVisible property.
        void setExpandButtonVisible(bool enable)
        {
            m_expandButtonVisible=enable;
            updateExpandButtonVisible();
        }

        bool isExpandButtonVisible() const noexcept
        {
            return m_expandButtonVisible;
        }

        //! Expand/collapse the toolbar + max-height clamp. See the expanded property.
        void setExpanded(bool enable)
        {
            auto changed=(m_expanded!=enable);
            m_expanded=enable;
            updateExpanded();
            if (changed)
            {
                emit expandedChanged(m_expanded);
            }
        }

        bool isExpanded() const noexcept
        {
            return m_expanded;
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

        /**
         * @brief Add a widget INSIDE the editor, before the text area (and before the expand
         *  button, which is always the last leading item).
         * @param widget Taken over by the editor's own layout, which reparents it.
         *
         * For a host whose composer puts controls beside the text -- an attach button on the left,
         * a send button on the right, the usual chat-composer shape. Those controls have to live
         * inside the editor once it has chrome of its own: a formatting toolbar that spanned only
         * the text area while the host's own buttons sat outside it would visibly cut the composer
         * in two. Added here, they sit under the same toolbar and take part in the same
         * stacked/inline rearrangement (see isStackedArrangement()).
         *
         * Widgets appear in the order added.
         */
        virtual void addLeadingWidget(QWidget* widget) =0;

        //! Add a widget INSIDE the editor, after the text area. See addLeadingWidget().
        virtual void addTrailingWidget(QWidget* widget) =0;

        /**
         * @brief Whether the leading/trailing widgets are currently stacked into COLUMNS (true)
         *  beside the text area, rather than laid out as rows (false).
         *
         * The leading frame, the text area and the trailing frame always sit in one horizontal
         * row; that row never changes. What changes is the direction of each frame's own layout:
         * a row of buttons beside a one-line text area becomes a column of buttons beside a text
         * area several lines tall, where there is vertical room for them and none to spare
         * horizontally. The group's first widget ends up lowest in the column, nearest the text
         * area's bottom edge, where it already was.
         *
         * Driven by content, not by the host. Going back is deliberately NOT symmetric -- it
         * happens only when the editor is empty again, so that a composer being edited around the
         * one/two-line boundary cannot flip its own layout back and forth under the user's hands.
         *
         * isExpanded() forces this on for the same reason: an expanded editor is pinned to its
         * maximum height whatever it contains, so its side widgets would otherwise be stranded
         * beside a text area several hundred pixels tall.
         */
        bool isStackedArrangement() const noexcept
        {
            return m_stackedArrangement;
        }

        //! Default for insertedTableWidthPercent(). Deliberately not 100: measured against a
        //! real QTextEdit, a 100% table comes out at EXACTLY the viewport width (412px of 412),
        //! leaving no slack at all -- so anything that costs a pixel afterwards, a vertical
        //! scrollbar appearing as the text grows or the host's own border/padding, tips it into
        //! overflow and raises a horizontal scrollbar under the table. A few percent of margin
        //! keeps it clear.
        constexpr static const int DefaultInsertedTableWidthPercent=95;

        /**
         * @brief Width given to a table INSERTED from the toolbar or context menu, as a
         *  percentage of the text area. 95 by default (see
         *  DefaultInsertedTableWidthPercent); 100 fills the text area exactly, with the
         *  scrollbar caveat noted there; 0 sizes the table to its own content instead.
         *
         * Affects newly inserted tables only -- a table already in the document keeps whatever
         * width it was created with, since this is a per-table QTextTableFormat property rather
         * than a document-wide setting.
         *
         * Expressed as a percentage rather than in pixels on purpose: a message is re-rendered at
         * the recipient's own bubble width, so a fixed pixel width would not survive the trip. It
         * is in any case an authoring-time convenience only -- markdown has no notion of table
         * width, so text(TextFormat::Markdown) drops it entirely.
         *
         * Beware 0: an EMPTY content-sized table is only as wide as its cell padding and borders
         * (measured: 32px for three empty columns). Legible and clickable, but easy to mistake
         * for a rendering fault -- which is why 100 is the default.
         */
        void setInsertedTableWidthPercent(int percent) noexcept
        {
            m_insertedTableWidthPercent=percent;
        }

        int insertedTableWidthPercent() const noexcept
        {
            return m_insertedTableWidthPercent;
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

        /**
         * @brief A CHECKABLE context-menu item was toggled whose id is not one of the standard
         *  MessageEditorMenuAction rows the editor already handles itself.
         *
         * Added alongside contextMenuItemTriggered() in Stage 5a: DropdownMenu emits
         * itemToggled(id,checked), not itemTriggered(id), for a checkable row (see
         * DropdownMenu::onItemToggled()), so a ContextMenuHandler that adds its own checkable
         * item previously got no callback at all for it.
         */
        void contextMenuItemToggled(int id, bool checked);

        void messageEditingModeChanged(MessageEditingMode mode);

        void expandedChanged(bool expanded);

        //! See isStackedArrangement(). A host may use this to adapt its own leading/trailing
        //! widgets to the row they have just moved into.
        void stackedArrangementChanged(bool stacked);

        /**
         * @brief The toolbar's Link button (or the context menu's "Insert link" row) was
         *  activated -- task-message-formatting-plan.md, Stage 5b.
         *
         * The editor has no dialog of its own (same reasoning as attachmentsPasted() handing an
         * attachment off to the host rather than showing a file picker itself): a HOST connects
         * this, opens its own AbstractHyperlinkDialog-family dialog pre-filled with the two
         * arguments below, and calls insertLink() from the dialog's own acceptance signal.
         *
         * @param defaultTitle Text to pre-fill the dialog's title field with -- the caret's
         *  current selection if there is one (plain text, indent-preserving), otherwise empty.
         * @param existingUrl Empty for a fresh link ("Insert link"); the link's current href if
         *  the caret was already inside one ("Edit link") -- the editor has already extended the
         *  selection to the whole link run in that case, so a host's insertLink() call replaces
         *  exactly that run.
         */
        void linkRequested(const QString& defaultTitle, const QString& existingUrl);

    protected:

        //! Called by the implementation when the content crosses the one-line boundary (or
        //! empties again). Not public: the arrangement is derived from content, not something a
        //! host sets -- see isStackedArrangement().
        void setStackedArrangement(bool stacked)
        {
            auto changed=(m_stackedArrangement!=stacked);
            m_stackedArrangement=stacked;
            updateStackedArrangement();
            if (changed)
            {
                emit stackedArrangementChanged(m_stackedArrangement);
            }
        }

        //! Reacts to setStackedArrangement().
        virtual void updateStackedArrangement() {}


        /**
         * @brief Called after setMessageEditingMode(). The default implementation calls the
         *  deprecated updateEditingMode() below, so an out-of-tree AbstractMessageEditor subclass
         *  that overrode only THAT hook keeps working unchanged. A new subclass overrides THIS
         *  hook instead and must NOT chain to the base implementation -- doing so would run the
         *  deprecated hook a second time.
         */
        virtual void updateMessageEditingMode() { updateEditingMode(); }

        /**
         * @deprecated Override updateMessageEditingMode() instead. Still invoked by its default
         *  implementation above, for source compatibility with an out-of-tree subclass that
         *  overrode only this hook. MessageEditor itself no longer implements this.
         */
        virtual void updateEditingMode() {}

        virtual void updateFinishOnEnter() {}
        virtual void updateEditingFinished() {}

        //! Reacts to setExpanded(). No default behaviour -- MessageEditor is the only
        //! implementation and it is the one that actually owns the toolbar/text-edit widgets.
        virtual void updateExpanded() {}

        //! Reacts to setExpandButtonVisible().
        virtual void updateExpandButtonVisible() {}

        const ContextMenuHandler& contextMenuHandler() const
        {
            return m_contextMenuHandler;
        }

    private:

        MessageEditingMode m_mode=MessageEditingMode::Wysiwyg;
        bool m_expandButtonVisible=false;
        bool m_expanded=false;
        bool m_stackedArrangement=false;
        int m_insertedTableWidthPercent=DefaultInsertedTableWidthPercent;
        bool m_finishOnEnter=true;
        bool m_contextMenuEnabled=true;
        ContextMenuHandler m_contextMenuHandler;
};

}

#endif // UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP
