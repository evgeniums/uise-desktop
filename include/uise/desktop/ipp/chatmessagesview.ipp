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

#include <QPushButton>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/directchildwidget.hpp>

#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagesview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeFlyweightListItem ******************************/

//--------------------------------------------------------------------------

template <typename BaseMessageT>
void ChatMessagesViewItem<BaseMessageT>::setDateSeparatorVisible(bool enable, bool withYear)
{
    AbstractChatSeparatorSection* dateSection=nullptr;
    auto sep=m_ui->topSeparator();
    if (sep==nullptr)
    {
        if (!enable)
        {
            return;
        }

        sep=m_msg->template makeWidget<AbstractChatSeparator,ChatSeparator>(m_ui);
        m_ui->setTopSeparator(sep);
    }

    dateSection=sep->section(AbstractChatSeparatorSection::TypeDate);
    if (dateSection==nullptr)
    {
        if (!enable)
        {
            return;
        }

        dateSection=m_msg->template makeWidget<AbstractChatSeparatorSection,ChatSeparatorSection>(m_ui);
        dateSection->setType(AbstractChatSeparatorSection::TypeDate);
        sep->insertSection(dateSection,0);
    }

    dateSection->setVisible(enable);
    auto localDt=m_msg->dateTime();

    auto str=dateAsMonthAndDay(localDt);
    if (withYear)
    {
        str=QString{"%1, %2"}.arg(str,localDt.date().year());
    }
    dateSection->setText(str);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT>
Widget* ChatMessagesViewItem<BaseMessageT>::doCreateActualWidget(QWidget* parent)
{
    m_ui=m_msg->template makeWidget<AbstractChatMessage,ChatMessage>(parent);
    return m_ui;
}

/************************* ChatMessagesView ******************************/

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
ChatMessagesView<BaseMessageT,DataT>::ChatMessagesView(QWidget* parent)
    : MouseMoveEventFilter(parent),
      m_qobj(new ChatMessagesViewQ(this))
{
    setObjectName("uiseChatMessagesView");

    m_layout=Layout::vertical(this);

    m_listView=new ChatMessagesViewWidget<BaseMessageT>(this);
    m_layout->addWidget(m_listView,1);

    m_listView->setPrefetchItemWindowHint(20);
    m_listView->setPrefetchScreensCount(2.0);
    m_listView->setPrefetchItemCount(10);
    m_listView->setFlyweightEnabled(true);
    m_listView->setStickMode(Direction::END);

    m_listView->setInsertItemCb(
        [this](auto itemW)
        {
            auto chatMsg=itemW->item()->ui();

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

            connect(
                chatMsg,
                &AbstractChatMessage::selectionUpdated,
                m_qobj,
                [this,id=itemW->id()](bool selected)
                {
                    if (selected)
                    {
                        m_selectedMessages.insert(id);
                    }
                    else
                    {
                        m_selectedMessages.erase(id);
                    }

                    bool noneSelected=m_listView->eachItem(
                        [](const auto* item)
                        {
                            return !item->item()->ui()->isSelected();
                        }
                    );

                    if (noneSelected)
                    {
                        QTimer::singleShot(
                            300,
                            this,
                            [this]()
                            {
                                bool noneSelected=m_listView->eachItem(
                                    [](const auto* item)
                                    {
                                        return !item->item()->ui()->isSelected();
                                    }
                                );
                                if (noneSelected)
                                {
                                    setSelectionMode(false);
                                }
                            }
                        );
                    }
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

    m_listView->setViewportChangedCb(
        [this](const auto* startItem, const auto* endItem)
        {
            if (isSelectionMode() && QGuiApplication::mouseButtons() & Qt::LeftButton)
            {
                auto globalCursorPos = QCursor::pos();
                QPoint localCursorPos = this->mapFromGlobal(globalCursorPos);
                auto widgetTopLeft = mapToGlobal(QPoint(0, 0));

                if (globalCursorPos.y() < widgetTopLeft.y() && startItem)
                {
                    if (m_chatUnderMouse)
                    {
                        startItem->widget()->setSelected(m_chatUnderMouse->isSelected());
                    }
                    else
                    {
                        startItem->widget()->setSelected(true);
                    }
                    m_chatUnderMouse=startItem->widget();
                }
                else if (localCursorPos.y() > height() && endItem)
                {
                    if (m_chatUnderMouse)
                    {
                        endItem->widget()->setSelected(m_chatUnderMouse->isSelected());
                    }
                    else
                    {
                        endItem->widget()->setSelected(true);
                    }
                    m_chatUnderMouse=endItem->widget();
                }
            }
#if 0
            if (startItem)
            {
                std::cerr << "ChatMessagesView::ChatMessagesView viewPortChanged"
                          << " start=" << startItem->sortValue().toString()
                          << " maxSortValue= "<<m_listView->maxSortValue().toString()
                          << std::endl;
            }
            if (endItem)
            {
                std::cerr << "ChatMessagesView::ChatMessagesView viewPortChanged"
                          << " end=" << endItem->sortValue().toString()
                          << " maxSortValue= "<<m_listView->maxSortValue().toString()
                          << std::endl;
            }

            if (!startItem && !endItem)
            {
                std::cerr << "ChatMessagesView::ChatMessagesView viewPortChanged no items";
            }
#endif
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
ChatMessagesView<BaseMessageT,DataT>::~ChatMessagesView()
{
    m_listView->resetCallbacks();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::setSelectionMode(bool enable)
{
    if (m_selectionMode==enable)
    {
        return;
    }

    m_selectionMode=enable;
    m_listView->eachItem(
        [enable](const auto* item)
        {
            item->item()->ui()->setSelectionMode(enable);
            return true;
        }
    );
    if (!m_selectionMode)
    {
        m_chatUnderMouse=nullptr;
        m_lastMousePos=QPoint{};
        m_selectedMessages.clear();
        m_mouseMoveUp.reset();
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::clearOtherContentsSelection(const Id& id)
{
    m_listView->eachItem(
        [&id](const auto* item)
        {
            if (item->id()!=id)
            {
                item->item()->ui()->clearContentSelection();
            }
            return true;
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::onMessageClicked(const Id& id)
{
    if (!isSelectionMode())
    {
        clearOtherContentsSelection(id);
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers)
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

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::adjustMessageList(std::vector<Message*>& messages)
{
    m_listView->eachItem(
        [&messages](const ChatMessageViewItemWrapper<BaseMessageT>* msgItem)
        {
            messages.emplace_back(msgItem->item());
            return true;
        }
    );
    std::sort(messages.begin(),messages.end(),[](const auto& l, const auto& r) { return *l<*r;});

    for (size_t i=0;i<messages.size();i++)
    {
        auto msg=messages[i];
        auto dt=msg->msg()->dateTime();

        bool dateVisible=false;
        bool withYear=false;
        if (i==0)
        {
            dateVisible=true;
            withYear=dt.date().year()!=QDateTime::currentDateTime().date().year();
        }
        else
        {
            auto prevDt=messages[i-1]->msg()->dateTime();

            dateVisible=prevDt.date()!=dt.date();

            withYear=prevDt.date().year() != dt.date().year();
        }

        msg->setDateSeparatorVisible(dateVisible,withYear);
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::removeMessage(const Id& id)
{
    m_listView->beginUpdate();
    m_listView->removeItem(id);
    std::vector<Message*> messages;
    adjustMessageList(messages);
    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::reorderMessage(const Id& id)
{
    m_listView->beginUpdate();
    m_listView->reorderItem(id);
    std::vector<Message*> messages;
    adjustMessageList(messages);
    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::insertFetched(bool forLoad, const std::vector<DataT>& dbItems, int wasRequestedMaxCount, Direction wasRequestedDirection, bool jumpToEnd)
{
    std::vector<Message*> messages;
    std::vector<ChatMessageViewItemWrapper<BaseMessageT>> messageItems;

    for (const auto& dbItem : dbItems)
    {
        auto message=makeMessage(dbItem);

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

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::loadMessages(const std::vector<DataT>& items)
{
    insertFetched(true,items);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::insertContinuousMessages(const std::vector<DataT>& items, int wasRequestedMaxCount, Direction wasRequestedDirection, bool wasStratItem)
{
    insertFetched(false,items,wasRequestedMaxCount,wasRequestedDirection,wasStratItem);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::clear()
{
    m_listView->clear();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::jumpToEdge(Direction direction)
{
    m_listView->scrollToEdge(direction);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::insertMessage(DataT dbItem)
{
    auto message=makeMessage(dbItem);

    m_listView->beginUpdate();

    if (m_listView->maxSortValue()<message->msg()->sortValue())
    {
        m_listView->setMaxSortValue(message->msg()->sortValue());
    }

    m_listView->insertItem(message);
    std::vector<Message*> messages;
    adjustMessageList(messages);

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
typename ChatMessagesView<BaseMessageT,DataT>::Message* ChatMessagesView<BaseMessageT,DataT>::message(const Id& id) const
{
    return m_listView->item(id)->item();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        auto newPos=event->pos();

        if (newPos.y()<0)
        {
            m_listView->scroll(-10);
            return;
        }
        if (newPos.y()>height())
        {
            m_listView->scroll(10);
            return;
        }

        if (m_lastMousePos.isNull())
        {
            m_lastMousePos=newPos;
            return;
        }
        auto delta=qAbs(m_lastMousePos.y()-newPos.y());
        if (delta<ChatMessagesViewQ::MouseMoveDetectDelta)
        {
            return;
        }

        std::optional<bool> mouseMoveUp;
        mouseMoveUp=m_lastMousePos.y() > newPos.y();
        m_lastMousePos=newPos;

        auto chatMsg=childWidgetAt<AbstractChatMessage>(this,event->pos());
        if (chatMsg)
        {
            std::optional<bool> forceSelect;
            if (m_chatUnderMouse)
            {
                if (chatMsg!=m_chatUnderMouse.get())
                {
                    m_chatUnderMouse->setSelectDetectionBlocked(false);
                    forceSelect=m_chatUnderMouse->isSelected();
                }
            }

            if (m_mouseMoveUp && mouseMoveUp)
            {
                if (m_mouseMoveUp.value()!=mouseMoveUp.value())
                {
                    forceSelect=!chatMsg->isSelected();
                }
            }

            chatMsg->detectMouseSelection(forceSelect);
            m_chatUnderMouse=chatMsg;
        }
        else
        {
            m_chatUnderMouse=nullptr;
        }

        m_mouseMoveUp=mouseMoveUp;
    }
    else
    {
        m_chatUnderMouse=nullptr;
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
void ChatMessagesView<BaseMessageT,DataT>::mouseReleaseEvent(QMouseEvent* event)
{
    m_chatUnderMouse=nullptr;
    m_lastMousePos=QPoint{};
    m_mouseMoveUp.reset();
    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename DataT>
ChatMessagesViewItem<BaseMessageT>* ChatMessagesView<BaseMessageT,DataT>::makeMessage(DataT data)
{
    auto message=m_messageBuilder(data,m_listView);
    Assert(message,"Invalid chat message builder in UI factory");
    if (isSelectionMode())
    {
        message->ui()->setSelectionMode(true);
        auto it=m_selectedMessages.find(message->id());
        if (it!=m_selectedMessages.end())
        {
            message->ui()->setSelected(true);
        }
    }
    return message;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_IPP
