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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/messageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/******************************EnhancedTextEdit********************************/

//--------------------------------------------------------------------------

EnhancedTextEdit::EnhancedTextEdit(QWidget* parent) : QTextEdit(parent),
    m_autoResize(true),
    m_newLineOnEnter(false)
{
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
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (m_newLineOnEnter)
        {
            if (event->modifiers()&Qt::ControlModifier)
            {
                emit returnPressed();
                return;
            }
        }
        else
        {
            if (!(event->modifiers()&Qt::ControlModifier))
            {
                emit returnPressed();
                return;
            }
        }
        event->setModifiers(event->modifiers()&~Qt::ControlModifier);
    }

    QTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void EnhancedTextEdit::focusInEvent(QFocusEvent* event)
{
    QTextEdit::focusInEvent(event);
    emit activated();
}

/********************************MessageEditor*********************************/

//--------------------------------------------------------------------------

class MessageEditor_p
{
    public:

        QBoxLayout* layout;

        EnhancedTextEdit* editor;
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
}

//--------------------------------------------------------------------------

MessageEditor::~MessageEditor()
{}

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

UISE_DESKTOP_NAMESPACE_END
