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

/** @file uise/desktop/htreelist.hpp
*
*  Declares branch node of list type for horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_HPP
#define UISE_DESKTOP_HTREE_LIST_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/flyweightlistview.hpp>

#include <uise/desktop/htreelistitem.hpp>
#include <uise/desktop/htreebranch.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeList;
class HTreeList_p;

template <typename ItemT, typename BaseT=QFrame>
class HTreeListView : public WithFlyweightListView<ItemT,BaseT>,
                      public WithRefreshRequested
{
    public:

        using WithFlyweightListView<ItemT,BaseT>::WithFlyweightListView;

        ~HTreeListView()
        {
            if (this->listView()!=nullptr)
            {
                this->listView()->resetCallbacks();
            }
        }

        void setHTreeList(HTreeList* list) noexcept
        {
            m_list=list;
        }

        HTreeList* hTreeList() const noexcept
        {
            return m_list;
        }

    private:

        HTreeList* m_list=nullptr;
};

class UISE_DESKTOP_EXPORT HTreeList : public HTreeBranch
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param tree The tree this node belongs to.
         * @param parent Parent widget.
         */
        HTreeList(HTreeTab* treeTab, QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeList(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeList();

        HTreeList(const HTreeList&)=delete;
        HTreeList(HTreeList&&)=delete;
        HTreeList& operator=(const HTreeList&)=delete;
        HTreeList& operator=(HTreeList&&)=delete;

        template <typename ItemT, typename BaseT>
        void setView(HTreeListView<ItemT,BaseT>* view)
        {
            view->setHTreeList(this);

            view->listView()->setInsertItemCb(
                [this](auto item)
                {
                    onItemInsert(item);
                }
            );
            view->listView()->setRemoveItemCb(
                [this](auto item)
                {
                    onItemRemove(item);
                }
            );
            setViewWidget(view);

            view->setRefreshRequestedCb(
                [this]()
                {
                    refresh();
                }
            );

            connect(
                this,
                &HTreeList::refreshRequested,
                this,
                [view]()
                {
                    view->reload();
                }
            );
        }

    public slots:

        void setNextNodeId(const std::string& id) override;

    private:

        void onItemInsert(HTreeListItem* item);
        void onItemRemove(HTreeListItem* item);

        void setViewWidget(QWidget* widget);

        std::unique_ptr<HTreeList_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_HPP
