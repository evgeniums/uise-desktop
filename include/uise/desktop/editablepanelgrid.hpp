/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/editablepanelgrid.hpp
*
*  Declares EditablePanelGrid.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITABLEPANELGRID_HPP
#define UISE_DESKTOP_EDITABLEPANELGRID_HPP

#include <QFrame>
#include <QGridLayout>

#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT EditablePanelGrid : public EditablePanel
{
    Q_OBJECT

    public:

        EditablePanelGrid(QWidget* parent=nullptr);

        QGridLayout* panelLayout() const noexcept
        {
            return m_layout;
        }

        void addWidget(const QString& label, QWidget* widget, int columnSpan=1, Qt::Alignment alignment=Qt::AlignLeft|Qt::AlignVCenter);

    private:

        QFrame* m_gridFrame;
        QGridLayout* m_layout;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANELGRID_HPP
