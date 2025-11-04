/**
@copyright Evgeny Sidorov 2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/buttonslist.cpp
*
*  Defines ButtonsList.
*
*/

/****************************************************************************/

#include <QScreen>
#include <QApplication>
#include <QGuiApplication>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/buttonslist.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

ButtonsList::ButtonsList(QWidget *parent)
    : QFrame(parent)
{
    m_layout=Layout::vertical(this);
    m_layout->addStretch(1);
}

//--------------------------------------------------------------------------

void ButtonsList::addWidget(QWidget* widget)
{
    m_layout->insertWidget(m_layout->count()-1,widget,0,Qt::AlignLeft);
}

//--------------------------------------------------------------------------

IconTextButton* ButtonsList::addButton(const QString& text, std::shared_ptr<SvgIcon> icon)
{
    auto button=new IconTextButton(text,std::move(icon),this);
    addWidget(button);
    return button;
}

//--------------------------------------------------------------------------

PopupButtonsList::PopupButtonsList(QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    auto l=Layout::vertical(this);
    m_buttonsList=new ButtonsList(this);
    l->addWidget(m_buttonsList);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void PopupButtonsList::show()
{
    auto pos=QCursor::pos();
    move(pos);
    QFrame::show();
    setFocus();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
