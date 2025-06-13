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
#include <QPointer>
#include <QGridLayout>
#include <QLabel>

#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT EditablePanelGrid : public EditablePanel
{
    Q_OBJECT

    public:

        EditablePanelGrid(QWidget* parent=nullptr);

        using EditablePanel::addRow;

        virtual int addRow(const QString& label, std::vector<Item> items, const QString& comment={}) override;

        virtual void setRowVisible(int index, bool enable) override;
        virtual bool isRowVisible(int index) const override;

        virtual void setComment(int index, const QString& comment) override;
        virtual void setLabel(int index, const QString& label) override;

    private:

        QFrame* m_gridFrame;
        QGridLayout* m_layout;

        int m_rowIndex;

        struct Row
        {
            std::vector<QPointer<QWidget>> widgets;
            bool visible;

            QPointer<QLabel> label;
            QPointer<QLabel> comment;
        };

        std::map<int,Row> m_rows;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANELGRID_HPP
