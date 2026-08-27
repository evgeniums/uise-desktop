/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/messageeditor.cpp
*
*  Defines MessageEditor.
*
*/

/****************************************************************************/

#include <QKeyEvent>
#include <QTextEdit>
#include <QTextDocumentFragment>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/messageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Rows of MessageEditor's own Cut/Copy/Paste/Select all/Clear context menu -- ordinary
//! drop-down menu rows on the menu background, so they resolve against the generic
//! DropdownMenu context rather than owning one of their own (compare LoadControlMenu's
//! private context, which exists because those rows sit on a different background).
std::shared_ptr<SvgIcon> menuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("DropdownMenu::%1").arg(alias),context);
}

}

/******************************EnhancedTextEdit********************************/

//--------------------------------------------------------------------------

EnhancedTextEdit::EnhancedTextEdit(QWidget* parent) : QTextEdit(parent),
    m_autoResize(true),
    m_newLineOnEnter(false)
{
    // Right-click is handled by MessageEditor's own DropdownMenu (see showContextMenu()),
    // not Qt's stock createStandardContextMenu() -- its Paste entry is driven by canPaste(),
    // which would report nothing-to-paste for an attachment payload (see
    // canInsertFromMimeData() below).
    setContextMenuPolicy(Qt::CustomContextMenu);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::setAutoResizingEnabled(bool enable)
{
    m_autoResize=enable;
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (enable)
    {
        connect(this,
                &QTextEdit::textChanged,
                this,
                &EnhancedTextEdit::updateSize
            );
    }
    else
    {
        disconnect(this,
                &QTextEdit::textChanged,
                this,
                &EnhancedTextEdit::updateSize
            );
    }
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::updateSize()
{
    updateGeometry();
}

//--------------------------------------------------------------------------

QSize EnhancedTextEdit::sizeHint() const
{
    if (m_autoResize)
    {
        QSizeF size = document()->size();

        // add a small margin for the frame/margins
        int height = static_cast<int>(size.height()) + frameWidth() * 2;

        return QSize(width(), height);
    }
    return QTextEdit::sizeHint();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::keyPressEvent(QKeyEvent* event)
{
    // Up-arrow in an EMPTY editor is a free gesture: there is no line above the caret to move to,
    // so the default handling is a visible no-op and claiming the key here costs nothing. Gated
    // strictly on emptiness -- with any text present, Up must keep moving the caret, and a host
    // acting on this signal would clobber an in-progress draft. Unmodified Up only: a modified Up
    // (Shift-select, Ctrl-scroll, ...) keeps its standard meaning.
    //
    // Tested as a MASK over the modifiers that actually change the gesture's meaning, never as
    // `modifiers()==Qt::NoModifier`: macOS sets NSEventModifierFlagNumericPad on the arrow keys,
    // which Qt surfaces as Qt::KeypadModifier, so an equality test silently never matches there.
    // (Same reason the Key_Return branch just below masks instead of comparing, and why every
    // other arrow-key handler in this library -- Calendar, Spinner, DateTimeInput -- ignores
    // modifiers altogether.)
    if (event->key() == Qt::Key_Up
        && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier))
        && document()->isEmpty())
    {
        emit editPreviousRequested();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (m_newLineOnEnter)
        {
            if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
            {
                emit returnPressed();
                return;
            }
        }
        else
        {
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            {
                emit returnPressed();
                return;
            }
        }
        event->setModifiers(event->modifiers() & ~(Qt::ControlModifier | Qt::ShiftModifier));
    }

    QTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::focusInEvent(QFocusEvent* event)
{
    QTextEdit::focusInEvent(event);
    emit activated();
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::canInsertFromMimeData(const QMimeData* source) const
{
    if (mimeDataHasAttachments(source))
    {
        return false;
    }

    return QTextEdit::canInsertFromMimeData(source);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::insertFromMimeData(const QMimeData* source)
{
    if (mimeDataHasAttachments(source))
    {
        emit attachmentsPasted(source);
        return;
    }

    QTextEdit::insertFromMimeData(source);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::pasteFromClipboard()
{
    QTextEdit::paste();
}

//--------------------------------------------------------------------------

bool EnhancedTextEdit::canPasteFromClipboard() const
{
    if (isReadOnly())
    {
        return false;
    }

    const auto* mimeData=QApplication::clipboard()->mimeData();
    return mimeDataHasAttachments(mimeData) || canPaste();
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::clearUndoable()
{
    auto cursor=textCursor();
    cursor.select(QTextCursor::Document);
    cursor.removeSelectedText();
}

/********************************MessageEditor*********************************/

//--------------------------------------------------------------------------

class MessageEditor_p
{
    public:

        QBoxLayout* layout;

        EnhancedTextEdit* editor;

        QPointer<DropdownMenu> contextMenu;
};

//--------------------------------------------------------------------------

MessageEditor::MessageEditor(QWidget* parent)
    : AbstractMessageEditor(parent),
      pimpl(std::make_unique<MessageEditor_p>())
{
    pimpl->layout=Layout::vertical(this);

    pimpl->editor=new EnhancedTextEdit(this);
    pimpl->layout->addWidget(pimpl->editor);
    pimpl->editor->setAutoResizingEnabled(true);

    setupReturnPressed();
    connect(
        pimpl->editor,
        &EnhancedTextEdit::returnPressed,
        this,
        &AbstractMessageEditor::finishEditing
    );

    connect(
        pimpl->editor,
        &QTextEdit::textChanged,
        this,
        [this]()
        {
            emit textChanged();
        }
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::activated,
        this,
        &AbstractMessageEditor::activated
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::attachmentsPasted,
        this,
        &AbstractMessageEditor::attachmentsPasted
    );

    connect(
        pimpl->editor,
        &EnhancedTextEdit::editPreviousRequested,
        this,
        &AbstractMessageEditor::editPreviousRequested
    );

    connect(
        pimpl->editor,
        &QWidget::customContextMenuRequested,
        this,
        &MessageEditor::showContextMenu
    );
}

//--------------------------------------------------------------------------

MessageEditor::~MessageEditor()
{
    if (!pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu->closeDropdown(true);
        destroyWidget(pimpl->contextMenu);
    }
}

//--------------------------------------------------------------------------

void MessageEditor::loadText(const QString& text, TextFormat format)
{
    if (editingMode()!=Qt::RichText)
    {
        pimpl->editor->setPlainText(text);
        return;
    }

    switch (format)
    {
        case (TextFormat::Markdown):
        {
            pimpl->editor->setMarkdown(text);
            break;
        }

        case (TextFormat::Plain):
        {
            pimpl->editor->setPlainText(text);
            break;
        }

        case (TextFormat::Html):
        {
            pimpl->editor->setHtml(text);
            break;
        }
    }
}

//--------------------------------------------------------------------------

QString MessageEditor::text(TextFormat format) const
{
    switch (format)
    {
        case (TextFormat::Markdown):
        {
            return pimpl->editor->toMarkdown();
            break;
        }

        case (TextFormat::Plain):
        {
            return pimpl->editor->toPlainText();
            break;
        }

        case (TextFormat::Html):
        {
            return pimpl->editor->toHtml();
            break;
        }
    }

    return QString{};
}

//--------------------------------------------------------------------------

QString MessageEditor::selectedText(TextFormat format) const
{
    auto cursor = pimpl->editor->textCursor();
    auto fragment = cursor.selection();

    switch (format)
    {
        case (TextFormat::Markdown):
        {
            return fragment.toMarkdown();
            break;
        }

        case (TextFormat::Plain):
        {
            return fragment.toPlainText();
            break;
        }

        case (TextFormat::Html):
        {
            return fragment.toHtml();
            break;
        }
    }

    return QString{};
}

//--------------------------------------------------------------------------

void MessageEditor::clear()
{
    pimpl->editor->clear();
}

//--------------------------------------------------------------------------

void MessageEditor::clearSelection()
{
    auto cursor = pimpl->editor->textCursor();
    cursor.clearSelection();
    pimpl->editor->setTextCursor(cursor);
}

//--------------------------------------------------------------------------

void MessageEditor::selectAll()
{
    pimpl->editor->selectAll();
}

//--------------------------------------------------------------------------

void MessageEditor::cut()
{
    // QTextEdit's own clipboard behavior, matching the composer's stock text-edit actions --
    // deliberately not selectedText(TextFormat::Markdown) plus a manual delete.
    pimpl->editor->cut();
}

//--------------------------------------------------------------------------

void MessageEditor::copy()
{
    pimpl->editor->copy();
}

//--------------------------------------------------------------------------

void MessageEditor::paste()
{
    pimpl->editor->pasteFromClipboard();
}

//--------------------------------------------------------------------------

bool MessageEditor::hasSelection() const
{
    return pimpl->editor->textCursor().hasSelection();
}

//--------------------------------------------------------------------------

bool MessageEditor::isEmpty() const
{
    return pimpl->editor->document()->isEmpty();
}

//--------------------------------------------------------------------------

bool MessageEditor::canPasteFromClipboard() const
{
    return pimpl->editor->canPasteFromClipboard();
}

//--------------------------------------------------------------------------

void MessageEditor::setFocusIn()
{
    pimpl->editor->setFocus();
}

//--------------------------------------------------------------------------

void MessageEditor::updateEditingMode()
{
    switch (editingMode())
    {
        case (Qt::PlainText):
        {
            loadText(pimpl->editor->toPlainText(),TextFormat::Plain);
            pimpl->editor->setAcceptRichText(false);
            break;
        }

        case (Qt::RichText):
        {
            loadText(pimpl->editor->toMarkdown(),TextFormat::Markdown);
            pimpl->editor->setAcceptRichText(true);
            break;
        }

        default:
        {
            loadText(pimpl->editor->toMarkdown(),TextFormat::Markdown);
            pimpl->editor->setAcceptRichText(false);
            break;
        }
    }
}

//--------------------------------------------------------------------------

void MessageEditor::updateFinishOnEnter()
{
    setupReturnPressed();
}

//--------------------------------------------------------------------------

void MessageEditor::updateEditingFinished()
{
}

//--------------------------------------------------------------------------

void MessageEditor::setupReturnPressed()
{
    pimpl->editor->setNewLineOnEnter(!isFinishOnEnter());
}

//--------------------------------------------------------------------------

void MessageEditor::setPlaceHolderText(const QString& text)
{
    pimpl->editor->setPlaceholderText(text);
}

//--------------------------------------------------------------------------

void MessageEditor::showContextMenu(const QPoint& pos)
{
    if (!isContextMenuEnabled())
    {
        return;
    }

    if (pimpl->contextMenu.isNull())
    {
        pimpl->contextMenu=new DropdownMenu();
        // a context menu is expected to pop up instantly, like the native QTextEdit menu it
        // replaces, rather than visibly grow -- same reasoning as ChatMessage's own context
        // menu (uichatmessage.cpp).
        pimpl->contextMenu->setAnimationDurationMs(0);
        // deliberately no setTriggerWidget(): DropdownFrame::eventFilter() consumes a press on
        // the trigger widget to turn it into a toggle-close, which would swallow a *second*
        // right-click instead of letting the menu reopen at the new cursor position. A genuine
        // outside click already closes the frame and passes the press through, which is what
        // moves the caret on a plain left-click elsewhere in the editor.
        connect(pimpl->contextMenu,&DropdownMenu::itemTriggered,this,&MessageEditor::onContextMenuItemTriggered);
    }
    else if (pimpl->contextMenu->isOpen())
    {
        pimpl->contextMenu->closeDropdown(true);
    }

    std::vector<MenuItem> items;

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Cut),
        tr("Cut"),
        menuIcon(QStringLiteral("cut"),pimpl->editor)
    ));
    items.back().isEnabled=hasSelection() && !pimpl->editor->isReadOnly();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Copy),
        tr("Copy"),
        menuIcon(QStringLiteral("copy"),pimpl->editor)
    ));
    items.back().isEnabled=hasSelection();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Paste),
        tr("Paste"),
        menuIcon(QStringLiteral("paste"),pimpl->editor)
    ));
    items.back().isEnabled=canPasteFromClipboard();

    items.push_back(MenuItem::separator());

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::SelectAll),
        tr("Select all"),
        menuIcon(QStringLiteral("selectAll"),pimpl->editor)
    ));
    items.back().isEnabled=!isEmpty();

    items.push_back(MenuItem(
        static_cast<int>(MessageEditorMenuAction::Clear),
        tr("Clear"),
        menuIcon(QStringLiteral("clear"),pimpl->editor)
    ));
    items.back().isEnabled=!isEmpty();

    if (contextMenuHandler())
    {
        contextMenuHandler()(items);
    }

    if (items.empty())
    {
        return;
    }

    pimpl->contextMenu->setItems(std::move(items));
    pimpl->contextMenu->popupAt(pimpl->editor->mapToGlobal(pos));
}

//--------------------------------------------------------------------------

void MessageEditor::onContextMenuItemTriggered(int id)
{
    switch (static_cast<MessageEditorMenuAction>(id))
    {
        case (MessageEditorMenuAction::Cut):
        {
            cut();
            break;
        }

        case (MessageEditorMenuAction::Copy):
        {
            copy();
            break;
        }

        case (MessageEditorMenuAction::Paste):
        {
            paste();
            break;
        }

        case (MessageEditorMenuAction::SelectAll):
        {
            selectAll();
            break;
        }

        case (MessageEditorMenuAction::Clear):
        {
            // Not clear(): that also purges the undo/redo history (QTextEdit::clear()'s
            // documented behavior), which is right for the "message sent, wipe everything"
            // call sites (ChatPageBottom::acceptOperation(), FileUploadWidget::reset()) but
            // wrong for this menu action, which the user expects to be undoable like any other
            // edit.
            pimpl->editor->clearUndoable();
            break;
        }

        default:
        {
            emit contextMenuItemTriggered(id);
            break;
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
