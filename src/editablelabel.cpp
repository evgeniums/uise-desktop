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

/** @file uise/desktop/src/editablelabel.cpp
*
*  Defines EditableLabel.
*
*/

/****************************************************************************/

#include <QKeyEvent>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QApplication>
#include <QSizePolicy>
#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/autoresizingtextedit.hpp>
#include <uise/desktop/editablelabel.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

/**
 * @brief Whether a Return/Enter key event, delivered to this label's current editor, inserts a
 *        newline rather than requesting apply().
 *
 * An AutoResizingTextEdit inserts a newline on a plain Return/Enter unless
 * setReturnInsertsNewLine(false) was called, and always does on Shift+Return/Enter. Any other
 * QTextEdit (a host may still install its own) keeps the previous behaviour of always inserting a
 * newline. Everything else (QLineEdit, QSpinBox, ...) has no newline to insert.
 *
 * Cannot compare exact metatypes (as this used to) once the editor may be an AutoResizingTextEdit
 * -- a Q_OBJECT subclass never compares equal to QTextEdit::staticMetaObject.metaType(), which
 * would silently make Return always apply instead of inserting a line.
 */
bool editorInsertsNewLineOnReturn(QWidget* editor, const QKeyEvent* keyEvent)
{
    if (auto* autoResizing=qobject_cast<AutoResizingTextEdit*>(editor))
    {
        return autoResizing->isReturnInsertingNewLine() || (keyEvent->modifiers() & Qt::ShiftModifier);
    }

    return qobject_cast<QTextEdit*>(editor)!=nullptr;
}

}

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(
        Type type,
        QWidget* parent,
        bool inGroup
    ) : AbstractValueWidget(parent),
        m_type(type),
        m_label(new Label(this)),
        m_formatter(nullptr),
        m_editing(false),
        m_inGroup(inGroup),
        m_panel(nullptr),
        m_editable(true),
        m_editButtonAlwaysHidden(false),
        m_trailingWidget(nullptr)
{
    m_mainLayout=Layout::vertical(this);

    auto mainFrame=new QFrame(this);
    m_mainLayout->addWidget(mainFrame);

    m_layout=Layout::horizontal(mainFrame);
    m_layout->addWidget(m_label,100);
    m_label->setObjectName("label");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_label->installEventFilter(this);
    m_label->setWordWrap(true);
    // No implicit indent: with indent() left at -1, a QLabel whose frameWidth() is non-zero --
    // which any QSS border or padding makes it -- indents its text by half an 'x' width on top of
    // that padding. None of the editors that replace the label in editing mode have an equivalent,
    // so the text jumped ~3px left on entering edit mode. With 0 the text sits exactly at the QSS
    // padding, the same inset the editors' own padding/margins produce.
    m_label->setIndent(0);

    m_editorFrame=new QFrame(this);
    m_editorFrame->setObjectName("labelEditorFrame");
    m_editorLayout=Layout::horizontal(m_editorFrame);
    m_layout->addWidget(m_editorFrame,100);
    m_editorFrame->setVisible(false);

    m_buttonsFrame=new QFrame(this);
    m_buttonsFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    m_buttonsFrame->setObjectName("labelButtonsFrame");
    m_buttonsLayout=Layout::horizontal(m_buttonsFrame);
    m_layout->addWidget(m_buttonsFrame,Qt::AlignBaseline | Qt::AlignLeft);
    m_buttonsFrame->setVisible(!m_inGroup);

    m_editButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::edit",this),this);
    m_editButton->setProperty("labelButton",true);
    m_editButton->setToolTip(tr("Edit"));
    m_editButton->setObjectName("editButton");
    connect(m_editButton,&PushButton::clicked,this,&EditableLabel::edit);
    m_buttonsLayout->addWidget(m_editButton,Qt::AlignBaseline | Qt::AlignLeft);

    m_applyButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::apply",this),this);
    m_applyButton->setProperty("labelButton",true);
    m_applyButton->setToolTip(tr("Apply"));
    m_applyButton->setObjectName("applyButton");
    connect(m_applyButton,&PushButton::clicked,this,&EditableLabel::apply);
    m_buttonsLayout->addWidget(m_applyButton,Qt::AlignBaseline | Qt::AlignLeft);
    m_applyButton->setVisible(false);

    m_cancelButton = new PushButton(Style::instance().svgIconLocator().icon("EditableLabel::cancel",this),this);
    m_cancelButton->setProperty("labelButton",true);
    m_cancelButton->setToolTip(tr("Cancel"));
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setVisible(false);
    connect(m_cancelButton,&PushButton::clicked,this,&EditableLabel::cancel);
    m_buttonsLayout->addWidget(m_cancelButton,Qt::AlignBaseline | Qt::AlignLeft);

    m_comment=new QLabel(mainFrame);
    m_comment->setObjectName("comment");
    m_comment->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_comment->setWordWrap(true);
    // Same as m_label above, so the comment text stays aligned under the label/editor text.
    m_comment->setIndent(0);
    m_mainLayout->addWidget(m_comment);
    m_comment->setVisible(false);
}

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(Type type, AbstractEditablePanel* panel)
    : EditableLabel(type,panel,true)
{
    setEditablePanel(panel);
}

//--------------------------------------------------------------------------

void EditableLabel::setTrailingWidget(QWidget* widget)
{
    if (m_trailingWidget==widget)
    {
        return;
    }

    if (m_trailingWidget!=nullptr)
    {
        m_layout->removeWidget(m_trailingWidget);
        m_trailingWidget->deleteLater();
    }

    m_trailingWidget=widget;
    if (m_trailingWidget!=nullptr)
    {
        // Appended, so it lands after m_buttonsFrame - the trailing edge of the row. Stretch 0:
        // the label (and the editor that replaces it) keep all the slack, exactly as they do
        // against the buttons frame. addWidget() reparents into m_layout's own widget (the inner
        // mainFrame, not this), which is where every other row element lives too.
        m_layout->addWidget(m_trailingWidget,0);
    }
}

//--------------------------------------------------------------------------

void EditableLabel::setEditing(bool enable)
{
    m_editing=m_editable && enable;

    // Hand focus across explicitly, and show the incoming side BEFORE updateControls() hides the
    // outgoing one. When a widget that holds focus is hidden, Qt moves focus on by calling its
    // focusNextPrevChild(true) (QWidgetPrivate::hide_helper()), which bubbles up the parent chain
    // to any enclosing QScrollArea -- and QScrollArea::focusNextPrevChild() then
    // ensureWidgetVisible()s whatever widget is next in the tab chain, scrolling the whole page.
    // That is what made a form jump when a double-clicked (hence focused) label was swapped for
    // its editor, or a focused editor back for its label on apply/cancel.
    auto* focused=QApplication::focusWidget();
    const bool focusInside=focused!=nullptr && isAncestorOf(focused);
    if (m_editing)
    {
        setChildVisible(m_editorFrame,true);
        if (focusInside)
        {
            editor()->setFocus(Qt::OtherFocusReason);
        }
    }
    else
    {
        // Not a plain setVisible(true): see setChildVisible() -- this runs from
        // setEditablePanel() while the label may still be parentless.
        setChildVisible(m_label,true);
        if (focusInside)
        {
            m_label->setFocus(Qt::OtherFocusReason);
        }
    }

    updateControls();
    m_editorFrame->setVisible(m_editing);
    if (m_editing)
    {
        if (!m_inGroup || config().property(ValueWidgetProperty::EditFocus).toBool())
        {
            editor()->setFocus();
        }
    }
}

//--------------------------------------------------------------------------

bool EditableLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && watched == editor())
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key()==Qt::Key_Escape)
        {
            if (m_inGroup)
            {
                emit groupCancelRequested();
            }
            else
            {
                cancel();
            }
        }
        if ((keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter) && !m_inGroup)
        {
            if (!editorInsertsNewLineOnReturn(editor(),keyEvent))
            {
                apply();
            }
        }
    }
    else if (watched == m_label)
    {
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            if (m_editable)
            {
                if (m_inGroup)
                {
                    if (isGroupEditingRequestEnabled())
                    {
                        emit groupEditingRequested();
                        QTimer::singleShot(
                            10,
                            this,
                            [this]()
                            {
                                editor()->setFocus();
                            }
                        );
                        return true;
                    }
                    return false;
                }

                setEditing(true);
                return true;
            }
            return false;
        }
        else if (event->type() == QEvent::ContextMenu)
        {
            auto menu=new QMenu(m_label);

            if (!m_inGroup)
            {
                auto edit = menu->addAction(tr("Edit"),this,[this](){setEditing(true);});
                menu->setDefaultAction(edit);
            }

            menu->addAction(tr("Copy"),this,[this](){
                auto selected = m_label->selectedText();
                if (selected.isEmpty())
                {
                    selected = m_label->text();
                }
                QGuiApplication::clipboard()->setText(selected);
            });            
            menu->exec(QCursor::pos());
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------

void EditableLabel::setEditablePanel(AbstractEditablePanel* panel)
{
    setInGroup(config().property(ValueWidgetProperty::InGroup,true).toBool());
    m_panel=panel;
    updateControls();
    if (m_inGroup)
    {
        connect(
            panel,
            &AbstractEditablePanel::editRequested,
            this,
            &EditableLabel::edit
        );
        connect(
            panel,
            &AbstractEditablePanel::cancelRequested,
            this,
            &EditableLabel::cancel
        );
        connect(
            panel,
            &AbstractEditablePanel::applyCommited,
            this,
            &EditableLabel::apply
        );
        connect(
            this,
            &AbstractValueWidget::valueEdited,
            panel,
            &AbstractEditablePanel::contentEdited
        );
    }
}

//--------------------------------------------------------------------------

void EditableLabel::setComment(const QString& comment)
{
    m_comment->setText(comment);
    // addValueWidget() calls this before addRow() places the label, see setChildVisible().
    setChildVisible(m_comment,!comment.isEmpty());
}

QString EditableLabel::comment() const
{
    return m_comment->text();
}

//--------------------------------------------------------------------------

void EditableLabelText::setValidator(const QValidator* validator, std::function<QString (const QString&)> hint)
{
    m_hint=std::move(hint);
    editorWidget()->setValidator(validator);
    if (!m_validationWatched)
    {
        m_validationWatched=true;
        connect(
            editorWidget(),
            &QLineEdit::textChanged,
            this,
            [this](const QString&)
            {
                updateValidationState();
            }
        );
    }
    updateValidationState();
}

//--------------------------------------------------------------------------

void EditableLabelText::updateValidationState()
{
    auto* edit=editorWidget();
    auto acceptable=edit->hasAcceptableInput();

    edit->setProperty("state",acceptable ? "acceptable" : "intermediate");
    edit->setToolTip((acceptable || !m_hint) ? QString() : m_hint(edit->text()));
    applyButton()->setEnabled(acceptable);
    Style::updateWidgetStyle(edit);
}

//--------------------------------------------------------------------------

void EditableLabelText::apply()
{
    if (!editorWidget()->hasAcceptableInput())
    {
        updateValidationState();
        return;
    }
    baseType::apply();
}

//--------------------------------------------------------------------------

void EditableLabelText::restoreWidgetValue()
{
    baseType::restoreWidgetValue();
    updateValidationState();
}

}
