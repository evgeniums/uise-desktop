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

#include <QClipboard>
#include <QApplication>
#include <QCursor>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/directchildwidget.hpp>
#include <uise/desktop/style.hpp>

#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagesview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeFlyweightListItem ******************************/

//--------------------------------------------------------------------------

template <typename BaseMessageT, typename Traits>
void ChatMessagesViewItem<BaseMessageT,Traits>::setDateSeparatorVisible(bool enable, bool withYear)
{
    m_dtSepVisible=enable;

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

        // Clickable only as an affordance for the jump-to-date popup; the connection's context
        // is m_ui (the message widget that owns the section), so Qt drops it when either the
        // section widget or the owning message widget goes away -- the flyweight list may
        // destroy and rebuild both independently of this item.
        dateSection->setClickable(true);
        QObject::connect(
            dateSection,
            &AbstractChatSeparatorSection::clicked,
            m_ui,
            [this,dateSection]()
            {
                if (!m_dateSectionClickedCb)
                {
                    return;
                }
                auto* pill=dateSection->clickableWidget();
                if (pill==nullptr)
                {
                    return;
                }
                m_dateSectionClickedCb(m_msg->dateTime().date(),
                                       pill->mapToGlobal(QPoint{0,pill->height()}));
            }
        );
    }

    dateSection->setVisible(enable);
    auto localDt=m_msg->dateTime();
    auto dt=localDt.date();
    auto curr=QDate::currentDate();
    auto today=dt==curr;
    auto yesterday=curr.addDays(-1)==dt;

    auto str=dateAsMonthAndDay(localDt);
    if (withYear)
    {
        str=QString{"%1, %2"}.arg(str,localDt.date().year());
    }
    else if (today)
    {
        str=QObject::tr("Today","ChatMessagesView");
    }
    else if (yesterday)
    {
        str=QObject::tr("Yesterday","ChatMessagesView");
    }
    dateSection->setText(str);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT, typename Traits>
void ChatMessagesViewItem<BaseMessageT,Traits>::setUnreadSeparatorVisible(bool enable, const QString& text)
{
    m_unreadSepVisible=enable;

    AbstractChatSeparatorSection* section=nullptr;
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

    section=sep->section(AbstractChatSeparatorSection::TypeUnreadMessages);
    if (section==nullptr)
    {
        if (!enable)
        {
            return;
        }

        section=m_msg->template makeWidget<AbstractChatSeparatorSection,ChatSeparatorSection>(m_ui);
        section->setType(AbstractChatSeparatorSection::TypeUnreadMessages);
        sep->insertSection(section,10);
    }

    section->setVisible(enable);
    section->setText(text);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT, typename Traits>
Widget* ChatMessagesViewItem<BaseMessageT,Traits>::doCreateActualWidget(QWidget* parent)
{
    m_ui=m_msg->template makeWidget<AbstractChatMessage,ChatMessage>(parent);
    return m_ui;
}

/************************* ChatMessagesView ******************************/

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
ChatMessagesView<BaseMessageT,Traits>::ChatMessagesView(QWidget* parent)
    : AbstractChatMessagesView(parent)
{
    setObjectName("uiseChatMessagesView");

    // See eventFilter()'s own doc comment: catches this widget's own top-level window losing
    // activation while mouseMoveEvent()'s drag-tracking state was left stuck "pressed".
    qApp->installEventFilter(this);

    m_resizeTimer=new SingleShotTimer(this);
    m_selectionModeTimer=new SingleShotTimer(this);

    m_layout=Layout::vertical(this);

    m_listView=new ChatMessagesViewWidget<BaseMessageT,Traits>(this);
    m_layout->addWidget(m_listView,1);

    m_listView->setItemsAlignment(FlyweightListViewAlignment::Begin);
    m_listView->setPrefetchItemWindowHint(20);
    m_listView->setPrefetchScreensCount(3.0);
    m_listView->setPrefetchItemCount(20);
    m_listView->setFlyweightEnabled(true);
    m_listView->setStickMode(Direction::END);
    m_listView->setVerticalScrollBarPlaceHolder(true);

    m_dateSubtitle=new ChatDateSubtitle(m_listView->viewportFrame());

    connect(
        m_dateSubtitle,
        &ChatDateSubtitle::clicked,
        this,
        [this]()
        {
            auto* section=m_dateSubtitle->section();
            if (section==nullptr)
            {
                return;
            }
            auto* pill=section->clickableWidget();
            if (pill==nullptr)
            {
                return;
            }
            emit dateSectionClicked(m_dateSubtitle->dateTime().date(),
                                    pill->mapToGlobal(QPoint{0,pill->height()}));
        }
    );

    m_listView->setUserScrolledCb(
        [this]()
        {
            onUserScrolled();
        }
    );

    m_listView->setInsertItemCb(
        [this](auto itemW)
        {
            auto chatMsg=itemW->item()->ui();

            connect(
                chatMsg,
                &AbstractChatMessage::clicked,
                this,
                [this,id=itemW->id()]()
                {
                    onMessageClicked(id);
                }
            );

            connect(
                chatMsg,
                &AbstractChatMessage::selectionModeRequested,
                this,
                [this]()
                {
                    setSelectionMode(true);
                }
            );

            connect(
                chatMsg,
                &AbstractChatMessage::selectionUpdated,
                this,
                [this,item=itemW->item()](bool selected)
                {
                    if (selected)
                    {
                        m_selectedMessages.emplace(item->id(),item->data());
                    }
                    else
                    {
                        m_selectedMessages.erase(item->id());
                    }
                    emit selectedCountChanged(m_selectedMessages.size());

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

            if (m_dateSubtitleEnabled)
            {
                updateDateSubtitleText();
            }

            emit viewportUpdated();
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
ChatMessagesView<BaseMessageT,Traits>::~ChatMessagesView()
{
    qApp->removeEventFilter(this);
    m_listView->resetCallbacks();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::setSelectionMode(bool enable)
{
    m_selectionModeTimer->shot(
        150,
        [this]()
        {
            emit selectionModeToggled(m_selectionMode);
        },
        true
    );
    if (!enable)
    {
        emit selectionModeToggled(false);
    }

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

    // update messages' widths
    auto msg=m_listView->firstViewportItem();
    if (msg==nullptr)
    {
        msg=m_listView->lastViewportItem();
    }
    if (msg!=nullptr)
    {
        m_messageBubbleOuterWidth=msg->widget()->bubbleOuterWidth();
    }
    adjustMessagesSizes();

    emit selectedCountChanged(m_selectedMessages.size());
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::clearOtherContentsSelection(const Id& id)
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

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::onMessageClicked(const Id& id)
{
    if (!isSelectionMode())
    {
        clearOtherContentsSelection(id);
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers)
{
    if (forceLongJump || (modifiers & Qt::ControlModifier))
    {
        if (direction==Direction::HOME)
        {
            emit reloadRequested();
        }
        else if (m_onItemsRequested)
        {
            // direction is to home but from the last (nullptr) element
            m_onItemsRequested(Id{},m_listView->prefetchItemWindow(),Direction::HOME);
        }
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::adjustMessageList(std::vector<Message*>& messages)
{
    m_listView->eachItem(
        [&messages](const ChatMessageViewItemWrapper<BaseMessageT,Traits>* msgItem)
        {
            messages.emplace_back(msgItem->item());
            return true;
        }
    );
    std::sort(messages.begin(),messages.end(),[](const auto& l, const auto& r) { return *l<*r;});

    bool hasUnreadSep=false;
    bool prevLastInBatch=true;
    for (size_t i=0;i<messages.size();i++)
    {
        auto msg=messages[i];

        // add date separator
        auto dt=msg->msg()->dateTime();
        bool dateVisible=false;
        bool withYear=false;
        if (i==0)
        {
            auto current=QDateTime::currentDateTime().date();
            dateVisible=dt.date()==current;
            withYear=dt.date().year()!=current.year();
        }
        else
        {
            auto prevDt=messages[i-1]->msg()->dateTime();
            dateVisible=prevDt.date()!=dt.date();
            withYear=prevDt.date().year() != dt.date().year();
        }
        msg->setDateSeparatorVisible(dateVisible,withYear);

        // update unread separator
        if (!hasUnreadSep && msg->isUnread() && i<(messages.size()-1))
        {
            msg->setUnreadSeparatorVisible(true,unreadSeparatorTitle());
            hasUnreadSep=true;
        }
        else if (msg->isUnreadSeparatorVisible())
        {
            msg->setUnreadSeparatorVisible(false);
        }

        // update last in batch for prev msg
        if (msg->isTopSeparatorVisible() && i>0)
        {
            prevLastInBatch=true;
            messages[i-1]->ui()->setLastInBatch(true);
        }

        // set first in batch
        msg->ui()->setFirstInBatch(prevLastInBatch);

        // check if the next message is by same author or last in list
        auto lastInBatch = i==messages.size()-1;
        if (!lastInBatch)
        {
            lastInBatch=!msg->msg()->sameSender(messages[i+1]->msg());
        }
        msg->ui()->setLastInBatch(lastInBatch);

        // set prevLastInBatch
        prevLastInBatch=lastInBatch;
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::insertFetched(bool forLoad, const std::vector<Data>& dbItems, int wasRequestedMaxCount, Direction wasRequestedDirection, bool jumpToEnd)
{
    std::vector<Message*> messages;
    std::vector<ChatMessageViewItemWrapper<BaseMessageT,Traits>> messageItems;

    for (const auto& dbItem : dbItems)
    {
        auto message=makeMessage(dbItem);

        messages.push_back(message);
        messageItems.push_back(message);
    }

    if (forLoad || jumpToEnd)
    {
        m_listView->clear();
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

        adjustMessagesSizes(&messages);
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

            // insert items to the list
            adjustMessagesSizes(&messages);

            m_listView->insertContinuousItems(messageItems,false);

            // A batch shorter than requested means the db had nothing more on that side, so the
            // newly-loaded edge item IS the boundary -- set the marker unconditionally (not
            // widen-only as before) from the list's own post-insert edge. The previous
            // widen-only guard (only raise max / only lower min) could never move a marker past
            // the "unknown end" sentinel (an all-`f` ObjectId, larger than any real value, used
            // by e.g. whitemdesktop's jump-to-date fetch) toward a real value, so a window opened
            // mid-history kept the sentinel forever even once its true tail was loaded here --
            // "jump to end" kept doing a full reload despite the true last message already being
            // on screen. Reading the post-insert list edge (rather than messageItems' own front/
            // back) keeps this correct if a live message arrived meanwhile via doInsertMessage().
            // Must run before endUpdate(), which triggers resizeList()/viewportUpdated() --
            // scroll-driven prefetch reads the marker there.
            //
            // Note: a batch shortened only because the caller's own item-building step silently
            // dropped one malformed item still pins here -- the same pre-existing risk the
            // empty-batch branch above already carries.
            if (messageItems.size()<static_cast<size_t>(wasRequestedMaxCount))
            {
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

            m_listView->endUpdate();
        }
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::loadMessages(const std::vector<Data>& items)
{
    insertFetched(true,items);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::insertContinuousMessages(const std::vector<Data>& items, int wasRequestedMaxCount, Direction wasRequestedDirection, bool wasStratItem)
{
    insertFetched(false,items,wasRequestedMaxCount,wasRequestedDirection,wasStratItem);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::loadMessagesAround(const std::vector<Data>& items, const Id& anchorId,
                                                                const SortValue& minSortValue, const SortValue& maxSortValue,
                                                                int offset)
{
    // Mirrors insertFetched()'s forLoad branch (clear + reload the whole list), but the min/max
    // sort-value markers come from the caller instead of being inferred from an edge load -- a
    // mid-list window can't tell on its own whether there is more data before/after it.
    std::vector<Message*> messages;
    std::vector<ChatMessageViewItemWrapper<BaseMessageT,Traits>> messageItems;

    for (const auto& item : items)
    {
        auto message=makeMessage(item);

        messages.push_back(message);
        messageItems.push_back(message);
    }

    m_listView->clear();
    adjustMessageList(messages);

    m_listView->setMinSortValue(minSortValue);
    m_listView->setMaxSortValue(maxSortValue);

    adjustMessagesSizes(&messages);
    m_listView->loadItems(messageItems);

    // valid synchronously right after loadItems(): endUpdate() (called inside loadItems()) runs
    // resizeList() inline, so the anchor's widget is already laid out.
    m_listView->scrollToItem(anchorId,offset);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
bool ChatMessagesView<BaseMessageT,Traits>::scrollToMessage(const Id& id, int offset)
{
    return m_listView->scrollToItem(id,offset);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
bool ChatMessagesView<BaseMessageT,Traits>::highlightMessage(const Id& id)
{
    auto item=m_listView->item(id);
    if (item==nullptr || item->item()==nullptr)
    {
        return false;
    }
    auto* ui=item->item()->ui();
    if (ui==nullptr)
    {
        return false;
    }

    // At most one row highlighted at a time -- clear whatever a previous jump lit, even if it is
    // still fading. QPointer so this is a no-op if that message has since scrolled out of the
    // loaded window and been destroyed.
    if (m_highlightedMessage && m_highlightedMessage!=ui)
    {
        m_highlightedMessage->clearHighlight();
    }

    m_highlightedMessage=ui;
    ui->startHighlight();
    return true;
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::clear()
{
    m_listView->clear();
    m_dateSubtitle->hideNow();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::jumpToEdge(Direction direction)
{
    m_listView->jumpToEdge(direction);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::adjustCurrentMessagesList()
{
    std::vector<Message*> messages;
    adjustMessageList(messages);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::insertMessage(const Data& dbItem)
{
    m_listView->beginUpdate();

    doInsertMessage(dbItem);
    adjustCurrentMessagesList();

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::readjustList()
{
    m_listView->beginUpdate();

    adjustCurrentMessagesList();

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::doInsertMessage(const Data& dbItem)
{
    auto message=makeMessage(dbItem);
    adjustMesssageSize(message);
    if (m_listView->maxSortValue() < message->msg()->sortValue())
    {
        m_listView->setMaxSortValue(message->msg()->sortValue());
    }
    m_listView->insertItem(message);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::removeMessage(const Id& id)
{
    m_listView->beginUpdate();

    doRemoveMessage(id);
    adjustCurrentMessagesList();

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::doRemoveMessage(const Id& id)
{
    if (isSelectionMode())
    {
        m_selectedMessages.erase(id);
        emit selectedCountChanged(m_selectedMessages.size());
    }
    m_listView->removeItem(id);    
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::reorderMessage(const Id& id)
{
    m_listView->beginUpdate();

    doReorderMessage(id);
    adjustCurrentMessagesList();

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::doReorderMessage(const Id& id)
{
    m_listView->reorderItem(id);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::updateMessage(const Data& dbItem)
{
    auto msg=message(Traits::id(dbItem));
    if (msg==nullptr)
    {
        return;
    }

    auto oldSortValue=msg->sortValue();
    auto newSortValue=Traits::sortValue(dbItem);
    auto reorder=oldSortValue != newSortValue;

    replaceSelectedData(msg);

    m_listView->beginUpdate();

    msg->updateData(dbItem);
    if (reorder)
    {
        doReorderMessage(Traits::id(dbItem));
    }
    adjustMesssageSize(msg);

    adjustCurrentMessagesList();

    m_listView->endUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
typename ChatMessagesView<BaseMessageT,Traits>::Message* ChatMessagesView<BaseMessageT,Traits>::message(const Id& id) const
{
    auto item=m_listView->item(id);
    if (item==nullptr)
    {
        return nullptr;
    }
    return item->item();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::mouseMoveEvent(QMouseEvent* event)
{
    // !m_dragTrustSuspect -- see its own doc comment. Confirmed by log capture: after a
    // fullscreen top-level window steals activation mid-click, buttons() keeps reporting
    // LeftButton down on every subsequent plain move indefinitely, so this flag -- not
    // buttons() -- is what actually gates drag-selection once it is set.
    if (!m_dragTrustSuspect && (event->buttons() & Qt::LeftButton))
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
        if (delta<MouseMoveDetectDelta)
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

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::mouseReleaseEvent(QMouseEvent* event)
{
    resetMouseSelectionState();
    QFrame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::resetMouseSelectionState()
{
    m_chatUnderMouse=nullptr;
    m_lastMousePos=QPoint{};
    m_mouseMoveUp.reset();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
bool ChatMessagesView<BaseMessageT,Traits>::eventFilter(QObject* watched, QEvent* event)
{
    // window(), not a cached pointer -- this widget can be reparented into a different
    // top-level window over its lifetime (e.g. a chat page moved between MainWindows), so it
    // must be re-resolved on every event rather than captured once.
    if (event->type()==QEvent::WindowDeactivate && watched==window())
    {
        resetMouseSelectionState();
        m_dragTrustSuspect=true;
    }
    else if (event->type()==QEvent::MouseButtonPress)
    {
        // Any real press anywhere in the app -- not scoped to this widget or its window --
        // is independent evidence the OS's own mouse-button tracking is sane again (see
        // m_dragTrustSuspect's own doc comment on why that evidence has to come from a fresh
        // press rather than from resetting our own state).
        m_dragTrustSuspect=false;
    }
    return QFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
ChatMessagesViewItem<BaseMessageT,Traits>* ChatMessagesView<BaseMessageT,Traits>::makeMessage(const Data& data)
{
    // Build the message under the widget it will actually be parented to once inserted (the
    // list view's inner LinkedListView), NOT under m_listView. Insertion reparents anything
    // whose parent differs, and with an app-wide stylesheet in effect every reparent runs
    // QWidgetPrivate::inheritStyle() over the bubble's whole descendant subtree -- i.e. the
    // ensurePolished() below would be redone from scratch a moment later. Building under the
    // final parent also makes that polish resolve QSS against the real ancestor chain, which
    // matters because the size hints it produces are measured right after.
    auto message=m_messageBuilder(data,m_listView->itemsParentWidget());
    Assert(message,"Invalid chat message builder in UI factory");

    // Set BEFORE anything can build a date separator: insertFetched() runs
    // adjustMessageList() (-> setDateSeparatorVisible()) before loadItems(), i.e. before
    // setInsertItemCb() would fire, so installing this in that callback would be too late for
    // the initial load. `this` is the view, which outlives every item it owns.
    message->setDateSectionClickedCb(
        [this](const QDate& date, const QPoint& pos)
        {
            emit dateSectionClicked(date,pos);
        }
    );

    // set selection mode
    if (isSelectionMode())
    {
        message->ui()->setSelectionMode(true);
        replaceSelectedData(message);
    }

    // A message's size hint (and every geometry-related QSS rule feeding it -- min/max-width,
    // padding, ...) is only meaningful after QStyle::polish() has run. Every caller of
    // makeMessage() measures the widget (adjustMessagesSizes()/adjustMesssageSize()) right after
    // this call, so it must be fully polished BEFORE that -- not just the first message ever
    // built, which is all the old m_messageBubbleOuterWidth==0-gated repolish below used to cover.
    // ensurePolished() recurses into every descendant built by the message builder above
    // (including the content sections, already attached at this point) and is idempotent, so
    // calling it unconditionally here is cheap for messages built via ChatMessage::construct(),
    // which already polished its own static chrome.
    message->ui()->ensurePolished();

    // keep message widths assuming that all messages have the same minimum and outer bubble width
    if (m_messageBubbleOuterWidth==0)
    {
        m_messageBubbleOuterWidth=message->ui()->bubbleOuterWidth();
        m_messageMinWidth=message->ui()->minimumWidth();
        m_messageMaxWidth=message->ui()->maximumWidth();
    }

    // done
    return message;
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::replaceSelectedData(Message* msg)
{
    if (isSelectionMode())
    {
        auto it=m_selectedMessages.find(msg->id());
        if (it!=m_selectedMessages.end())
        {
            msg->ui()->setSelected(true);
            m_selectedMessages.erase(it);
            m_selectedMessages.emplace(msg->id(),msg->data());
        }
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
std::vector<typename Traits::Data> ChatMessagesView<BaseMessageT,Traits>::selectedMessages() const
{
    std::vector<Data> v;
    if (isSelectionMode())
    {
        v.reserve(m_selectedMessages.size());
        for (const auto& item : m_selectedMessages)
        {
            v.emplace_back(item.second);
        }
        std::sort(v.begin(),v.end(),[](const auto& l, const auto& r)
        {
            return Traits::sortValue(l) < Traits::sortValue(r);
        });
    }
    return v;
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    adjustMessagesSizes();

    // fix resizing artefacts when window is miximized/normalized
    int adjustResizeDelta=30;
    if (qAbs(event->oldSize().width()-event->size().width()) > adjustResizeDelta
        ||
        qAbs(event->oldSize().height()-event->size().height()) > adjustResizeDelta
        )
    {
        m_resizeTimer->shot(50,[this](){adjustMessagesSizes();});
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::adjustMessagesSizes(std::vector<Message*>* messages)
{
    if (messages==nullptr)
    {
        auto maxWidth=messageContentWidth();
        auto handler=[maxWidth](const auto* item)
        {
            item->widget()->content()->updateBubbleWidth(maxWidth);
            return true;
        };
        m_listView->eachItem(handler);
    }
    else
    {
        if (messages!=nullptr)
        {
            for (auto& msg : *messages)
            {
                adjustMesssageSize(msg);
            }
        }
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::adjustMesssageSize(Message* msg)
{
    msg->ui()->content()->updateBubbleWidth(messageContentWidth());
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
int ChatMessagesView<BaseMessageT,Traits>::messageContentWidth() const
{
    auto w=m_listView->viewportSize().width() - horizontalTotalMargin(this) - m_messageBubbleOuterWidth;
    if (w<m_messageMinWidth)
    {
        w=m_messageMinWidth;
    }
    else if (w>m_messageMaxWidth)
    {
        w=m_messageMaxWidth;
    }
    return w;
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy))
    {
        if (isSelectionMode())
        {
            emit copySelectedRequested();
        }
        else
        {
            QString selectedText;
            [[maybe_unused]] bool noneSelected=m_listView->eachItem(
                [&selectedText](const auto* item)
                {
                    selectedText=item->item()->ui()->selectedText();
                    return selectedText.isEmpty();
                }
            );

            if (!selectedText.isEmpty())
            {
                QClipboard *clipboard = QGuiApplication::clipboard();
                if (clipboard!=nullptr)
                {
                    clipboard->setText(selectedText);
                }
            }
        }

        event->ignore();
    }
    return AbstractChatMessagesView::keyPressEvent(event);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
typename ChatMessagesView<BaseMessageT,Traits>::SortValue ChatMessagesView<BaseMessageT,Traits>::lastViewportSortValue() const
{
    auto lastViewportItem=m_listView->lastViewportItem();
    if (lastViewportItem==nullptr)
    {
        return SortValue{};
    }

    return lastViewportItem->sortValue();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
typename ChatMessagesView<BaseMessageT,Traits>::Id ChatMessagesView<BaseMessageT,Traits>::lastViewportSeqId() const
{
    auto lastViewportItem=m_listView->lastViewportItem();
    if (lastViewportItem==nullptr)
    {
        return Id{};
    }

    const auto* item=lastViewportItem->item();
    if (item==nullptr)
    {
        return Id{};
    }
    return item->seqId();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::setUnreadMessageCount(const QString& count)
{
    auto jumpControl=m_listView->jumpEdgeControl();
    if (jumpControl!=nullptr)
    {
        jumpControl->setBadgeText(count);
        m_listView->updateJumpEdgeVisibility();
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
bool ChatMessagesView<BaseMessageT,Traits>::eachMessage(MessageHandler handler)
{
    return m_listView->eachItem(
        [handler](const auto* item)
        {
            return handler(item->item());
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
bool ChatMessagesView<BaseMessageT,Traits>::rEachMessage(MessageHandler handler)
{
    return m_listView->rEachItem(
        [handler](const auto* item)
        {
            return handler(item->item());
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::onUserScrolled()
{
    if (!m_dateSubtitleEnabled)
    {
        return;
    }

    updateDateSubtitleText();
    m_dateSubtitle->notifyScrolled();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::updateDateSubtitleText()
{
    auto item=m_listView->firstViewportItem();
    if (item==nullptr)
    {
        return;
    }

    auto dt=item->item()->msg()->dateTime();
    bool withYear=dt.date().year()!=QDate::currentDate().year();
    m_dateSubtitle->setDateTime(dt,withYear);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_IPP
