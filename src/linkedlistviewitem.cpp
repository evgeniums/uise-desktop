/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/src/linkedlistviewitem.cpp
*
*  Contains implementation of LinkedListViewItem.
*
*/

/****************************************************************************/

#include <QVariant>

#include <uise/desktop/linkedlistviewitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
void LinkedListViewItem::keepInWidgetProperty()
{
    auto self=shared_from_this();
    Q_ASSERT(self);
    QVariant prop;
    prop.setValue(self);
    self->m_widget->setProperty(LinkedListViewItem::Property,prop);
}

//--------------------------------------------------------------------------
std::shared_ptr<LinkedListViewItem> LinkedListViewItem::getFromWidgetProperty(QObject *widget)
{
    if (!widget)
    {
        return std::shared_ptr<LinkedListViewItem>();
    }
    return widget->property(Property).value<LinkedListViewItemSharedPtr>();
}

//--------------------------------------------------------------------------
void LinkedListViewItem::clearWidgetProperty(QObject *widget)
{
    widget->setProperty(LinkedListViewItem::Property,QVariant());
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
