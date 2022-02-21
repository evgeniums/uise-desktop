/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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
        m_editable(false),
        m_inGroup(inGroup)
{
    m_layout=Layout::horizontal(this);

    m_layout->addWidget(m_label);

    m_editButton = new QPushButton(this);
    auto editIcon = QIcon::fromTheme("document-edit",QIcon(":/uise/desktop/document-edit.svg"));
    m_editButton->setIcon(editIcon);
    m_editButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    m_editButton->setFlat(true);
    m_editButton->setProperty("labelButton",true);
    m_editButton->setToolTip(tr("Edit"));
    m_editButton->setObjectName("editButton");
    connect(m_editButton,&QPushButton::clicked,this,&EditableLabel::edit);
    m_layout->addWidget(m_editButton);
    m_editButton->setVisible(!m_inGroup);

    m_applyButton = new QPushButton(this);
    auto applyIcon = QIcon::fromTheme("dialog-ok-apply",QIcon(":/uise/desktop/dialog-ok-apply.svg"));
    m_applyButton->setIcon(applyIcon);
    m_applyButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    m_applyButton->setFlat(true);
    m_applyButton->setProperty("labelButton",true);
    m_applyButton->setToolTip(tr("Apply"));
    m_applyButton->setObjectName("applyButton");
    connect(m_applyButton,&QPushButton::clicked,this,&EditableLabel::apply);
    m_layout->addWidget(m_applyButton);
    m_applyButton->setVisible(false);

    m_cancelButton = new QPushButton(this);
    auto cancelIcon = QIcon::fromTheme("dialog-cancel",QIcon(":/uise/desktop/dialog-cancel.svg"));
    m_cancelButton->setIcon(cancelIcon);
    m_cancelButton->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    m_cancelButton->setFlat(true);
    m_cancelButton->setProperty("labelButton",true);
    m_cancelButton->setToolTip(tr("Cancel"));
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setVisible(false);
    connect(m_cancelButton,&QPushButton::clicked,this,&EditableLabel::cancel);
    m_layout->addWidget(m_cancelButton);

    m_label->setObjectName("label");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_label->installEventFilter(this);

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
}

//--------------------------------------------------------------------------
bool EditableLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == editor() && event->type() == QEvent::KeyPress)
    {
        if (m_inGroup)
        {
            return false;
        }

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

            setEditable(true);
            return true;
        }
        if (event->type() == QEvent::ContextMenu)
        {
            auto menu=new QMenu(m_label);

            if (!m_inGroup)
            {
                auto edit = menu->addAction(tr("Edit"),this,[this](){setEditable(true);});
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

UISE_DESKTOP_NAMESPACE_END
