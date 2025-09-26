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

/** @file uise/desktop/htreelistflyweightview.hpp
*
*  Declares flyweight list view for htree list branch node.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_FLYWEIGHT_VIEW_HPP
#define UISE_DESKTOP_HTREE_LIST_FLYWEIGHT_VIEW_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/framewithrefresh.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeList;

template <typename ItemT, typename BaseT=QFrame>
class HTreeListFlyweightView : public WithFlyweightListView<ItemT,BaseT>,
                               public WithRefreshRequested
{
    public:

        using WithFlyweightListView<ItemT,BaseT>::WithFlyweightListView;

        ~HTreeListFlyweightView()
        {
            if (this->listView()!=nullptr)
            {
                this->listView()->resetCallbacks();
            }
        }

        void setListNode(HTreeList* listNode) noexcept
        {
            m_node=listNode;
        }

        HTreeList* listNode() const noexcept
        {
            return m_node;
        }

    private:

        HTreeList* m_node=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_FLYWEIGHT_VIEW_HPP
