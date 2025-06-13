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

/** @file uise/desktop/src/editablepanels.cpp
*
*  Defines EditablePanels.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/editablepanels.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

EditablePanels::EditablePanels(
        QWidget* parent
    ) : AbstractEditablePanels(parent)
{
    m_layout=Layout::vertical(this);
}

//--------------------------------------------------------------------------

void EditablePanels::addPanel(QWidget* panel, int stretch, Qt::Alignment alignment)
{
    m_layout->addWidget(panel,stretch,alignment);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
