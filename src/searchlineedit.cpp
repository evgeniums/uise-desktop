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

/** @file uise/desktop/searchlineedit.cpp
*
*  Defines SearchLineEdit.
*
*/

/****************************************************************************/

#include <QFocusEvent>
#include <QShortcut>

#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/searchlineedit.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

SearchLineEdit::SearchLineEdit(QWidget* parent)
    : LineEdit(parent),
      m_editing(false)
{
    auto searchIcon=Style::instance().svgIconLocator().icon("LineEdit::search");
    m_searchButton=LineEdit::addPushButton(searchIcon,QLineEdit::TrailingPosition);
    m_searchButton->setObjectName("searchButton");
    auto cancelIcon=Style::instance().svgIconLocator().icon("LineEdit::cancel");
    m_cancelButton=LineEdit::addPushButton(cancelIcon,QLineEdit::TrailingPosition);
    m_cancelButton->hide();
    m_cancelButton->setCursor(Qt::ArrowCursor);
    m_cancelButton->setObjectName("cancelButton");

    connect(
        m_cancelButton,
        &PushButton::clicked,
        this,
        &SearchLineEdit::cancel
    );

    m_cancelShortcut=new QShortcut(
        Qt::Key_Escape,
        this
    );
    m_cancelShortcut->setContext(Qt::WidgetShortcut);
    connect(
        m_cancelShortcut,
        &QShortcut::activated,
        this,
        &SearchLineEdit::cancel
    );

    connect(
        this,
        &LineEdit::textChanged,
        this,
        [this](const QString& text)
        {
            if (!text.isEmpty())
            {
                edit();
            }
            else
            {
                doCancel(true);
            }
        }
    );

    setPlaceholderText(tr("Search"));
}

//--------------------------------------------------------------------------

void SearchLineEdit::cancel()
{
    doCancel(false);
}

//--------------------------------------------------------------------------

void SearchLineEdit::doCancel(bool keepFocus)
{
    if (!m_editing)
    {
        return;
    }

    m_editing=false;
    clear();
    m_searchButton->setVisible(true);
    m_cancelButton->setVisible(false);
    updateButtonPositions();
    emit editingModeChanged(m_editing);
    emit cancelled();
    if (!keepFocus)
    {
        clearFocus();
    }
}

//--------------------------------------------------------------------------

void SearchLineEdit::edit()
{
    if (m_editing)
    {
        return;
    }

    m_editing=true;
    m_searchButton->setVisible(false);
    m_cancelButton->setVisible(true);
    updateButtonPositions();
    emit editingModeChanged(m_editing);
}

//--------------------------------------------------------------------------

PushButton* SearchLineEdit::addPushButton(std::shared_ptr<SvgIcon> icon)
{
    return LineEdit::addPushButton(icon,QLineEdit::LeadingPosition);
}

//--------------------------------------------------------------------------

void SearchLineEdit::addPushButton(PushButton* button)
{
    LineEdit::addPushButton(button,QLineEdit::LeadingPosition);
}

//--------------------------------------------------------------------------

void SearchLineEdit::enterEvent(QEnterEvent* event)
{
    m_searchButton->setParentHovered(true);
    LineEdit::enterEvent(event);
}

//--------------------------------------------------------------------------

void SearchLineEdit::leaveEvent(QEvent* event)
{
    m_searchButton->setParentHovered(false);
    LineEdit::leaveEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
