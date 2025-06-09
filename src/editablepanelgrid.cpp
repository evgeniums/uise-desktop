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

/** @file uise/desktop/src/editablepanelgrid.cpp
*
*  Defines EditablePanelGrid.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QGridLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/editablepanelgrid.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

EditablePanelGrid::EditablePanelGrid(
        QWidget* parent
    ) : EditablePanel(parent)
{
    m_gridFrame=new QFrame();
    m_layout=Layout::grid(m_gridFrame,false);
    setWidget(m_gridFrame);
}

//--------------------------------------------------------------------------

void EditablePanelGrid::addWidget(const QString& label, QWidget* widget, int columnSpan, Qt::Alignment alignment)
{
    auto l=new QLabel(label,m_gridFrame);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto count=m_layout->count();
    m_layout->addWidget(l,count,0,Qt::AlignRight|Qt::AlignVCenter);
    m_layout->addWidget(widget,count,1,1,columnSpan,alignment);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
