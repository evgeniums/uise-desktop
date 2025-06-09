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
#include <QSizePolicy>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/editablelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(
        Type type,
        QWidget* parent,
        bool inGroup
    ) : QFrame(parent),
        m_type(type),
        m_label(new QLabel(this)),
        m_formatter(nullptr),
        m_editing(false),
        m_inGroup(inGroup),
        m_panel(nullptr)
{
    m_layout=Layout::horizontal(this);
    m_layout->addWidget(m_label,100);
    m_label->setObjectName("label");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_label->installEventFilter(this);

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
}

//--------------------------------------------------------------------------

EditableLabel::EditableLabel(Type type, EditablePanel* panel)
    : EditableLabel(type,panel,true)
{
    setEditablePanel(panel);
}

//--------------------------------------------------------------------------

bool EditableLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && !m_inGroup && watched == editor())
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key()==Qt::Key_Escape)
        {
            cancel();
            return true;
        }
        if (keyEvent->key()==Qt::Key_Return)
        {
            apply();
            return true;
        }
    }
    else if (watched == m_label)
    {
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            if (m_inGroup)
            {
                return false;
            }

            setEditing(true);
            return true;
        }
        if (event->type() == QEvent::ContextMenu)
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

void EditableLabel::setEditablePanel(EditablePanel* panel)
{
    m_panel=panel;
    updateControls();
    connect(
        panel,
        &EditablePanel::editRequested,
        this,
        &EditableLabel::edit
    );
    connect(
        panel,
        &EditablePanel::cancelRequested,
        this,
        &EditableLabel::cancel
    );
    connect(
        panel,
        &EditablePanel::applyRequested,
        this,
        &EditableLabel::apply
    );
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
