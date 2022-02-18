/**
@copyright Evgeny Sidorov 2021

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

#include <QMainWindow>
#include <QKeyEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/editablelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
EditableLabel::EditableLabel(
        Type type,
        QWidget* parent
    ) : QFrame(parent),
        m_type(type),
        m_label(new QLabel(this)),
        m_formatter(nullptr)
{
    m_layout=Layout::vertical(this);
    m_layout->addWidget(m_label);

    m_label->setObjectName("label");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_label->installEventFilter(this);
}

//--------------------------------------------------------------------------
bool EditableLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == editableWidget() && event->type() == QEvent::KeyPress)
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
    else if (watched == m_label && event->type() == QEvent::MouseButtonDblClick)
    {
        setEditable(true);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
