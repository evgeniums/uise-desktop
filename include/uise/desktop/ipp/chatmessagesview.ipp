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

/** @file uise/desktop/ipp/chatmessagesview.ipp
*
*  Defines ChatMessagesView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGESVIEW_IPP
#define UISE_DESKTOP_CHATMESSAGESVIEW_IPP

#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagesview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeFlyweightListItem ******************************/

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesViewItem<DataT>::setDateSeparatorVisible(bool enable, bool withYear)
{
    AbstractChatSeparatorSection* dateSection=nullptr;
    auto sep=m_ui->topSeparator();
    if (sep==nullptr)
    {
        if (!enable)
        {
            return;
        }

        sep=makeWidget<AbstractChatSeparator,ChatSeparator>(m_ui);
        m_ui->setTopSeparator(sep);
    }

    dateSection=sep->section(AbstractChatSeparatorSection::TypeDate);
    if (dateSection==nullptr)
    {
        if (!enable)
        {
            return;
        }

        dateSection=makeWidget<AbstractChatSeparatorSection,ChatSeparatorSection>(m_ui);
        dateSection->setType(AbstractChatSeparatorSection::TypeDate);
        sep->insertSection(dateSection,0);
    }

    dateSection->setVisible(enable);
    auto localDt=dateTime();

    auto str=dateAsMonthAndDay(localDt);
    if (withYear)
    {
        str=QString{"%1, %2"}.arg(str,localDt.date().year());
    }
    dateSection->setText(str);
}

//--------------------------------------------------------------------------

template <typename DataT>
Widget* ChatMessagesViewItem<DataT>::doCreateActualWidget(QWidget* parent)
{
    return makeWidget<AbstractChatMessage>(parent);
}

/************************* ChatMessagesView ******************************/

//--------------------------------------------------------------------------

template <typename DataT>
ChatMessagesView<DataT>::ChatMessagesView(QWidget* parent)
    : QFrame(parent),
      m_qobj(new ChatMessagesViewQ(this))
{
    m_layout=Layout::vertical(this);

    m_listView=new ChatMessagesViewWidget<DataT>(this);
    m_layout->addWidget(m_listView);

    m_listView->setPrefetchItemWindowHint(20);
    m_listView->setPrefetchScreensCount(2.0);
    m_listView->setPrefetchItemCount(10);
    m_listView->setFlyweightEnabled(true);
    m_listView->setStickMode(Direction::END);

    m_listView->setInsertItemCb(
        [this](auto itemW)
        {
            auto chatMsg=itemW->ui();

            connect(
                chatMsg,
                &AbstractChatMessage::clicked,
                m_qobj,
                [this,id=itemW->id()]()
                {
                    onMessageClicked(id);
                }
            );

            connect(
                chatMsg,
                &AbstractChatMessage::selectionModeRequested,
                m_qobj,
                [this]()
                {
                    setSelectionMode(true);
                }
            );
        }
    );

    m_listView->setRequestItemsCb(
        [this](const auto* startItem, size_t maxCount, Direction direction)
        {
#if 0
            const auto* item=startItem->item();
            std::cerr << common::DateTime::currentUtc().toIsoString() << ": ChatMessages requested items "
                      << maxCount << " direction="<<int(direction) << " id " << item->id().toString() << std::endl;
#endif
            m_onItemsRequested(startItem->sortValue(),maxCount,direction);
        }
    );

    m_listView->setRequestHomeCb(
        [this](bool forceLongJump, Qt::KeyboardModifiers modifiers)
        {
            onJumpRequested(Direction::HOME,forceLongJump,modifiers);
        }
        );

    m_listView->setRequestEndCb(
        [this](bool forceLongJump, Qt::KeyboardModifiers modifiers)
        {
            onJumpRequested(Direction::END,forceLongJump,modifiers);
        }
    );
}

//--------------------------------------------------------------------------

template <typename DataT>
ChatMessagesView<DataT>::~ChatMessagesView()
{
    m_listView->resetCallbacks();
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::setSelectionMode(bool enable)
{
    m_selectionMode=enable;
    m_listView->eachItem(
        [enable](const auto* item)
        {
            item->ui()->setSelectionMode(enable);
            return true;
        }
    );
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::clearOtherContentsSelection(const Id& id)
{
    m_listView->eachItem(
        [&id](const auto* item)
        {
            if (item->id()!=id)
            {
                item->ui()->clearContentSelection();
            }
            return true;
        }
    );
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::onMessageClicked(const Id& id)
{
    if (!isSelectionMode())
    {
        clearOtherContentsSelection(id);
    }
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers)
{
    if (forceLongJump || (modifiers & Qt::ControlModifier))
    {
        if (direction==Direction::HOME)
        {
            emit m_qobj->reloadRequested();
        }
        else if (m_onItemsRequested)
        {
            // direction is to home but from the last (nullptr) element
            m_onItemsRequested(Id{},m_listView->prefetchItemWindow(),Direction::HOME);
        }
    }
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::adjustMessageList(std::vector<Message*>& messages)
{
    m_listView->eachItem(
        [&messages](const ChatMessageViewItemWrapper<DataT>* msgItem)
        {
            messages.emplace_back(msgItem->item());
            return true;
        }
        );
    std::sort(messages.begin(),messages.end(),[](const auto& l, const auto& r) { return *l<*r;});

    for (size_t i=0;i<messages.size();i++)
    {
        auto msg=messages[i];
        auto dt=msg->dateTime();

        bool dateVisible=false;
        bool withYear=false;
        if (i==0)
        {
            dateVisible=true;
            withYear=dt.year()!=QDateTime::currentDateTime().date().year();
        }
        else
        {
            auto prevDt=messages[i-1]->dateTime();

            dateVisible=prevDt.date()!=dt.date();

            withYear=prevDt.date().year() != dt.date().year();
        }

        msg->setDateSeparatorVisible(dateVisible,withYear);
    }
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::removeMessage(const Id& id)
{
    m_listView->beginUpdate();
    m_listView->removeItem(id);
    std::vector<Message*> messages;
    adjustMessageList(messages);
    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::reorderMessage(const Id& id)
{
    m_listView->beginUpdate();
    m_listView->reorderItem(id);
    std::vector<Message*> messages;
    adjustMessageList(messages);
    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::insertFetched(bool forLoad, const std::vector<DataT>& dbItems, int wasRequestedMaxCount, Direction wasRequestedDirection, bool jumpToEnd)
{
    std::vector<Message*> messages;
    std::vector<ChatMessageViewItemWrapper<DataT>> messageItems;

    for (const auto& dbItem : dbItems)
    {
        auto message=m_messageBuilder(dbItem,m_listView);
        Assert(message,"Invalid chat message builder in UI factory");

        messages.push_back(message);
        messageItems.push_back(message);
    }

    if (forLoad || jumpToEnd)
    {
        adjustMessageList(messages);

        // process initial loading or jump-to-end
        m_listView->setMinSortValue({});
        if (messageItems.empty())
        {
            m_listView->setMaxSortValue({});
        }
        else
        {
            m_listView->setMaxSortValue(messageItems.back().sortValue());
        }
        m_listView->loadItems(messageItems);
        if (!forLoad)
        {
            // special case for jump-to-end
            m_listView->scrollToEdge(Direction::END);
        }
    }
    else
    {
        if (messageItems.empty())
        {
            // adjust min and max sort values
            if (wasRequestedDirection==Direction::END)
            {
                auto last=m_listView->lastItem();
                if (last!=nullptr)
                {
                    m_listView->setMaxSortValue(last->sortValue());
                }
            }
            else
            {
                auto first=m_listView->firstItem();
                if (first!=nullptr)
                {
                    m_listView->setMinSortValue(first->sortValue());
                }
            }
        }
        else
        {
            m_listView->beginUpdate();

            // preprocess list with merged existing and new messages
            adjustMessageList(messages);

            // adjust min and max sort values
            if (messageItems.size()<wasRequestedMaxCount)
            {
                if (wasRequestedDirection==Direction::END)
                {
                    auto maxSortValue=messageItems.back().sortValue();
                    if (m_listView->maxSortValue().isNull() || m_listView->maxSortValue()<maxSortValue)
                    {
                        m_listView->setMaxSortValue(maxSortValue);
                    }
                }
                else
                {
                    auto minSortValue=messageItems.front().sortValue();
                    if (m_listView->minSortValue().isNull() || minSortValue<m_listView->minSortValue())
                    {
                        m_listView->setMinSortValue(minSortValue);
                    }
                }
            }

            // insert items to the list
            m_listView->insertContinuousItems(messageItems,false);

            m_listView->endUpdate();
        }
    }
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::loadMessages(const std::vector<DataT>& items)
{
    insertFetched(true,items);
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::insertContinuousMessages(const std::vector<DataT>& items, int wasRequestedMaxCount, Direction wasRequestedDirection, bool wasStratItem)
{
    insertFetched(false,items,wasRequestedMaxCount,wasRequestedDirection,wasStratItem);
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::clear()
{
    m_listView->clear();
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::jumpToEdge(Direction direction)
{
    m_listView->scrollToEdge(direction);
}

//--------------------------------------------------------------------------

template <typename DataT>
void ChatMessagesView<DataT>::insertMessage(DataT dbItem)
{
    auto message=m_messageBuilder(dbItem,m_listView);
    Assert(message,"Invalid chat message builder in UI factory");

    m_listView->beginUpdate();

    if (m_listView->maxSortValue()<message->data()->sortValue())
    {
        m_listView->setMaxSortValue(message->data()->sortValue());
    }

    m_listView->insertItem(message);
    std::vector<Message*> messages;
    adjustMessageList(messages);
    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_IPP
