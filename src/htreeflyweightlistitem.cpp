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

/** @file uise/desktop/htreeflyweightlistitem.cpp
*
*  Defines HTreeFlyWeighListItem.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/htreeflyweightlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeFlyweightListItem ******************************/

//--------------------------------------------------------------------------

HTreeFlyweightListItem::HTreeFlyweightListItem(HTreePathElement el, QWidget* parent)
    : HTreeListItem(std::move(el),parent)
{
    m_id.append(pathElement().id()).append("_").append(itemType());
    m_layout=Layout::horizontal(this);
    setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void HTreeFlyweightListItem::setItemWidgets(QWidget* icon, QWidget* content, int contentStretch, int nextStrech)
{
    m_layout->addWidget(icon);
    m_layout->addWidget(content,contentStretch);
    if (nextStrech!=0)
    {
        m_layout->addStretch(nextStrech);
    }
}

UISE_DESKTOP_NAMESPACE_END
