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
    ) : EditablePanel(parent),
        m_rowIndex(0)
{
    m_gridFrame=new QFrame();
    m_gridFrame->setObjectName("gridFrame");
    m_layout=Layout::grid(m_gridFrame,false);
    setWidget(m_gridFrame);
}

//--------------------------------------------------------------------------

int EditablePanelGrid::addRow(const QString& label, std::vector<Item> items, const QString& comment)
{
    std::vector<QPointer<QWidget>> widgets;

    auto count=m_layout->count();

    int column=0;
    QLabel* labelWidget=nullptr;
    if (!label.isEmpty())
    {
        labelWidget=new QLabel(label,m_gridFrame);
        labelWidget->setObjectName("panelLabel");
        labelWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_layout->addWidget(labelWidget,count,column,labelAlignment());
        ++column;
    }

    for (auto&& item:items)
    {
        m_layout->addWidget(item.widget,count,column,item.rowSpan,item.columnSpan,item.alignment);
        column+=item.columnSpan;
        widgets.push_back(item.widget);        
    }
    ++count;

    QLabel* commentWidget=nullptr;
    if (!comment.isEmpty())
    {
        commentWidget=new QLabel(comment);
        commentWidget->setObjectName("panelComment");
        commentWidget->setWordWrap(true);
        commentWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (label.isEmpty())
        {
            m_layout->addWidget(commentWidget,count,0,1,items.size());
        }
        else
        {
            m_layout->addWidget(commentWidget,count,1,1,items.size());
        }
        ++count;
    }

    m_rows.emplace(m_rowIndex,Row{std::move(widgets),true,labelWidget,commentWidget});
    auto index=m_rowIndex++;
    return index;
}

//--------------------------------------------------------------------------

void EditablePanelGrid::setRowVisible(int index, bool enable)
{
    auto it=m_rows.find(index);
    if (it==m_rows.end())
    {
        return;
    }

    it->second.visible=enable;
    for (auto& widget : it->second.widgets)
    {
        if (widget)
        {
            widget->setVisible(enable);
        }
    }
    if (it->second.label)
    {
        it->second.label->setVisible(enable);
    }
    if (it->second.comment)
    {
        it->second.comment->setVisible(enable);
    }
}

//--------------------------------------------------------------------------

bool EditablePanelGrid::isRowVisible(int index) const
{
    auto it=m_rows.find(index);
    if (it==m_rows.end())
    {
        return false;
    }

    return it->second.visible;
}

//--------------------------------------------------------------------------

void EditablePanelGrid::setComment(int index, const QString& comment)
{
    auto it=m_rows.find(index);
    if (it==m_rows.end())
    {
        return;
    }

    if (it->second.comment)
    {
        it->second.comment->setText(comment);
    }
}

//--------------------------------------------------------------------------

void EditablePanelGrid::setCommentStatus(int index, Status::Type status)
{
    auto it=m_rows.find(index);
    if (it==m_rows.end())
    {
        return;
    }

    auto helper=statusHelper();
    QString str;
    if (helper)
    {
        str=helper->statusString(status);
    }
    else
    {
        str=StatusBase::statusString(status);
    }

    if (it->second.comment)
    {
        if (str.isEmpty())
        {
            it->second.comment->setProperty("status",QVariant{});
        }
        else
        {
            it->second.comment->setProperty("status",str);
        }
        Style::updateWidgetStyle(it->second.comment);
    }
}

//--------------------------------------------------------------------------

void EditablePanelGrid::setLabel(int index, const QString& label)
{
    auto it=m_rows.find(index);
    if (it==m_rows.end())
    {
        return;
    }

    if (it->second.label)
    {
        it->second.label->setText(label);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
