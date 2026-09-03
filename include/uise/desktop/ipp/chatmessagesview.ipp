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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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
        str=dateWithoutWeekday(localDt);
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

    m_floatingAvatarTimer=new SingleShotTimer(this);
    m_floatingAvatarBlockTimer=new SingleShotTimer(this);

    m_floatingAvatar=new ChatFloatingAvatar(m_listView->viewportFrame());

    connect(
        m_floatingAvatar,
        &ChatFloatingAvatar::clicked,
        this,
        [this]()
        {
            // Requirement: clicking the floating copy must be indistinguishable from clicking the
            // anchored avatar it stands in for, so it has to arrive on the represented message's
            // own signal -- an embedder's existing per-message connection then applies unchanged,
            // sourceWidget included. Emitting another object's signal is the established idiom in
            // this tree (see e.g. editablelabel.hpp's valueWidget->valueEdited()).
            auto* msg=m_floatingAvatar->message();
            if (msg!=nullptr)
            {
                emit msg->avatarClicked();
            }
        }
    );

    // viewportChangedCb (wired below) only fires when the first/last VIEWPORT ITEM changes
    // (FlyweightListView_p::informViewportUpdated()) -- a tall batch scrolling by without either
    // edge item changing produces none, yet the floating avatar's clamp and hide test are
    // per-pixel quantities. Every scroll of any origin, including the programmatic scrollTo()
    // paths that bypass userScrolledCb, moves this inner list widget, so watch it directly too.
    m_listView->itemsParentWidget()->installEventFilter(this);

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
                updateDateSubtitleOcclusion();
            }

            scheduleFloatingAvatarUpdate();

            emit viewportUpdated();
        }
    );

    connect(
        this,
        &AbstractChatMessagesView::effectiveAlignSentChanged,
        this,
        [this]()
        {
            applyAlignSentToMessages();

            // Must run AFTER applyAlignSentToMessages(): that is what re-derives every row's
            // avatar visibility and column width for the new alignment, both of which the
            // floating copy is positioned and matched against.
            blockFloatingAvatar();
        }
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
ChatMessagesView<BaseMessageT,Traits>::~ChatMessagesView()
{
    qApp->removeEventFilter(this);
    m_listView->itemsParentWidget()->removeEventFilter(this);
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
            dateVisible=true;
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

    // Batch boundaries (first/last-in-batch) and avatar visibility may all have just shifted --
    // covers load/insert/remove/reorder/update, every one of which funnels through here.
    scheduleFloatingAvatarUpdate();
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
    m_floatingAvatar->hideNow();
    setObscuredAvatarMessage(nullptr);
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
template <typename FieldsT>
void ChatMessagesView<BaseMessageT,Traits>::updateMessage(const Data& dbItem, FieldsT fields)
{
    auto msg=message(Traits::id(dbItem));
    if (msg==nullptr)
    {
        return;
    }

    // Traits::sortValue(dbItem) is not necessarily the same key as msg->sortValue() -- e.g.
    // whitemdesktop's ChatMessage::sortValue() reads chat_msg::sort_oid while its
    // ChatMessageTraits::sortValue() reads at_server::server_oid. Comparing the item's own
    // accessor before and after updateData() keeps both sides on the same key, so a change in
    // one but not the other doesn't spuriously (or silently) skip the required reorder.
    auto oldSortValue=msg->sortValue();

    replaceSelectedData(msg);

    m_listView->beginUpdate();

    msg->updateData(dbItem,fields);

    // sortValue() reads the key live from the (now replaced) DU -- see
    // insertItemToContainer()'s own @todo: a live sort-key mutation MUST be paired with
    // reordering the index, or the ordered index silently corrupts.
    if (msg->sortValue() != oldSortValue)
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
    // See scheduleFloatingAvatarUpdate()'s own doc comment: this is the catch-all trigger that
    // covers scrolls originating outside userScrolledCb/viewportChangedCb (programmatic
    // scrollTo() calls, content-height changes that move rows without changing which item sits
    // at either viewport edge).
    //
    // m_listView!=nullptr guard is load-bearing, not defensive: qApp->installEventFilter(this)
    // above runs before m_listView is constructed, and building the very first SingleShotTimer
    // child of `this` (m_resizeTimer, right below) already sends an event through the
    // application-wide filter chain back into this same eventFilter() -- i.e. this branch can
    // run while the constructor is still between those two lines, with m_listView still null.
    if (m_listView!=nullptr && watched==m_listView->itemsParentWidget() &&
        (event->type()==QEvent::Move || event->type()==QEvent::Resize))
    {
        scheduleFloatingAvatarUpdate();
    }
    // window(), not a cached pointer -- this widget can be reparented into a different
    // top-level window over its lifetime (e.g. a chat page moved between MainWindows), so it
    // must be re-resolved on every event rather than captured once.
    else if (event->type()==QEvent::WindowDeactivate && watched==window())
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

    // Own (sent) message alignment, as currently resolved by this view (app setting + Auto-mode
    // width check, see AbstractChatMessagesView::effectiveAlignSent()). A no-op for a Received
    // message (isRight() never depends on alignSent there) and cheap to call unconditionally;
    // must happen before ensurePolished() below since it can reorder the bubble/avatar layout.
    message->ui()->setAlignSent(effectiveAlignSent());

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
        // maxMessageWidth() (qproperty-maxMessageWidth, see chat.qss) takes precedence when set --
        // this view's own width is no longer capped (chat.qss dropped that), so without an
        // explicit bubble cap here a message would grow to fill an arbitrarily wide window.
        // Falls back to sampling the first message's own QSS maximumWidth(), same as before.
        m_messageMaxWidth=maxMessageWidth()>0 ? maxMessageWidth() : message->ui()->maximumWidth();
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

    // Auto mode: this view's own width just changed, so re-check whether own messages should
    // flip side. A flip emits effectiveAlignSentChanged(), which applyAlignSentToMessages() is
    // connected to (ctor) -- runs synchronously here, before adjustMessagesSizes() below.
    updateEffectiveAlignSent();

    adjustMessagesSizes();

    // The viewport's own height just changed, so the floating avatar's natural bottom-anchored Y
    // (and possibly the clamp against it) did too.
    scheduleFloatingAvatarUpdate();

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
void ChatMessagesView<BaseMessageT,Traits>::applyAlignSentToMessages()
{
    auto align=effectiveAlignSent();
    Message* anyMessage=nullptr;
    eachMessage(
        [align,&anyMessage](Message* msg)
        {
            msg->ui()->setAlignSent(align);
            anyMessage=msg;
            return true;
        }
    );
    if (anyMessage!=nullptr)
    {
        // A message's own bubbleOuterWidth() (its avatar column's contribution) can change with
        // alignment -- see ChatMessage::updateAlignment()'s avatar-forcing, which widens the
        // avatar column once own and received messages share the same side. makeMessage() only
        // ever samples m_messageBubbleOuterWidth once, from the very first message ever built, so
        // that cached value can now be stale; refresh it from any already-updated message (they
        // all share the same alignment, so any one will do) before resizing bubbles against it.
        // Covers a plain setAlignSentMode() call too (e.g. the Appearance setting), not just a
        // resize -- resizeEvent()'s own adjustMessagesSizes() call right after this fires
        // (effectiveAlignSentChanged() is a direct, same-thread connection) is otherwise the only
        // path that would pick this up, and it never runs on a setting change alone.
        m_messageBubbleOuterWidth=anyMessage->ui()->bubbleOuterWidth();
        adjustMessagesSizes();
    }
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::onUserScrolled()
{
    // Not gated on m_dateSubtitleEnabled -- the floating avatar has its own independent enable
    // flag (m_floatingAvatarEnabled, checked inside scheduleFloatingAvatarUpdate()/
    // updateFloatingAvatar()).
    scheduleFloatingAvatarUpdate();

    if (!m_dateSubtitleEnabled)
    {
        return;
    }

    updateDateSubtitleText();
    updateDateSubtitleOcclusion();
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

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::updateDateSubtitleOcclusion()
{
    if (!m_dateSubtitleEnabled)
    {
        return;
    }

    // The subtitle's own rect, inflated by its configured slack. Kept current even while the
    // pill itself is hidden -- updateDateSubtitleText() above already repositioned it for the
    // current topmost message before this runs.
    auto* subtitlePill=m_dateSubtitle->section()!=nullptr
                            ? m_dateSubtitle->section()->clickableWidget()
                            : nullptr;
    if (subtitlePill==nullptr)
    {
        return;
    }

    auto margin=m_dateSubtitle->occlusionMargin();
    auto subtitleTopLeft=subtitlePill->mapToGlobal(QPoint{0,0});
    QRect subtitleRect{subtitleTopLeft,subtitlePill->size()};
    subtitleRect.adjust(0,-margin,0,margin);

    bool occluded=false;
    auto* viewportFrame=m_listView->viewportFrame();

    m_listView->eachItem(
        [&occluded,&subtitleRect,viewportFrame](const auto* item) -> bool
        {
            auto* sep=item->item()->ui()->topSeparator();
            if (sep==nullptr)
            {
                return true;
            }

            auto* dateSection=sep->section(AbstractChatSeparatorSection::TypeDate);
            if (dateSection==nullptr || !dateSection->isVisibleTo(viewportFrame))
            {
                return true;
            }

            auto* pill=dateSection->clickableWidget();
            if (pill==nullptr)
            {
                pill=dateSection;
            }

            auto pillTopLeft=pill->mapToGlobal(QPoint{0,0});
            QRect pillRect{pillTopLeft,pill->size()};

            if (pillRect.top()>subtitleRect.bottom())
            {
                // Items are visited top to bottom -- nothing further down can overlap either.
                return false;
            }

            if (pillRect.intersects(subtitleRect))
            {
                occluded=true;
                return false;
            }

            return true;
        }
    );

    m_dateSubtitle->setOccluded(occluded);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    scheduleFloatingAvatarUpdate();
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::scheduleFloatingAvatarUpdate()
{
    // m_floatingAvatarTimer==nullptr guard is load-bearing: this can be reached (via the
    // application-wide event filter installed in the ctor, or via a synchronous
    // setUserScrolledCb()/setViewportChangedCb() callback the flyweight list view's own setup
    // calls trigger) while the constructor is still between creating m_listView and creating
    // m_floatingAvatarTimer/m_floatingAvatar a few lines later.
    if (!m_floatingAvatarEnabled || m_floatingAvatarBlocked || m_floatingAvatarTimer==nullptr)
    {
        return;
    }

    // 0 ms, restart=false: coalesces a burst of triggers describing the same scroll frame
    // (userScrolledCb, viewportChangedCb, the itemsParentWidget() Move/Resize event filter) into
    // one recompute on the next event-loop turn -- same pattern FlyweightListView_p::
    // informViewportUpdated() uses for its own viewportChangedCb. Deferring also means this runs
    // after beginUpdate()/endUpdate() has settled geometry when triggered from
    // adjustMessageList().
    m_floatingAvatarTimer->shot(0,[this](){updateFloatingAvatar();});
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::updateFloatingAvatar()
{
    // Blocked check repeated here, not just in scheduleFloatingAvatarUpdate(): an update queued
    // just before the block started would otherwise still fire inside it.
    if (!m_floatingAvatarEnabled || m_floatingAvatarBlocked)
    {
        return;
    }

    // mapToGlobal() below is meaningless before the window is actually mapped -- e.g.
    // adjustMessageList() can schedule an update while the chat page is still hidden.
    if (!isVisible() || window()==nullptr || !window()->isVisible())
    {
        return;
    }

    // Requirement: the only gate is whether avatars are shown at all under the current
    // avatar-visibility mode -- the view-level mirror of the `leftAligned` half of
    // ChatMessage::updateAvatarForced(), deliberately WITHOUT its isLastInBatch() half: this
    // floats for the bottom-most visible message wherever it sits in its batch.
    if (effectiveAlignSent()!=AbstractChatMessage::AlignSent::Left)
    {
        m_floatingAvatar->setWanted(false);
        setObscuredAvatarMessage(nullptr);
        return;
    }

    auto* bottomItem=m_listView->lastViewportItem();
    if (bottomItem==nullptr)
    {
        m_floatingAvatar->setWanted(false);
        setObscuredAvatarMessage(nullptr);
        return;
    }
    auto* bottomMsg=bottomItem->widget();
    if (bottomMsg==nullptr || bottomMsg->avatarColumnWidget()==nullptr)
    {
        m_floatingAvatar->setWanted(false);
        setObscuredAvatarMessage(nullptr);
        return;
    }

    // Walk the loaded window (bounded, early-exiting) both ways starting from bottomMsg to find
    // its batch's head (isFirstInBatch()) and its own tail (isLastInBatch()). eachItem()/
    // rEachItem() iterate the whole flyweight-loaded window in sort order, not just the visible
    // rows, so both walks skip everything before bottomMsg is reached.
    AbstractChatMessage* headMsg=nullptr;
    AbstractChatMessage* tailMsg=nullptr;

    bool reached=false;
    m_listView->rEachItem(
        [&](const auto* item) -> bool
        {
            auto* m=item->widget();
            if (m==nullptr)
            {
                return true;
            }
            if (!reached)
            {
                if (m!=bottomMsg)
                {
                    return true;
                }
                reached=true;
            }
            headMsg=m;
            return !m->isFirstInBatch();
        }
    );

    reached=false;
    m_listView->eachItem(
        [&](const auto* item) -> bool
        {
            auto* m=item->widget();
            if (m==nullptr)
            {
                return true;
            }
            if (!reached)
            {
                if (m!=bottomMsg)
                {
                    return true;
                }
                reached=true;
            }
            tailMsg=m;
            return !m->isLastInBatch();
        }
    );
    // headMsg not reaching the true batch head (it runs off the top of the loaded window) is
    // harmless: it ends up as the topmost loaded row, whose bubble top is far above the
    // viewport, so the qMax() clamp in ChatFloatingAvatar::updatePosition() picks the natural
    // bottom position anyway. tailMsg==nullptr cannot actually happen -- adjustMessageList()
    // always forces isLastInBatch(true) on the last loaded row -- but is guarded below regardless.

    auto* viewportFrame=m_listView->viewportFrame();

    // A row that actually carries a visible anchored avatar, reported as two rects in viewport
    // coordinates: the avatar image's own, and the whole row's. Both stay valid while the row is
    // setAvatarObscured(), because obscuring works on opacity and leaves the widget in its
    // layout -- see ChatMessageAvatar::setAvatarObscured().
    struct AvatarRects
    {
        QRect avatar;
        QRect row;
    };
    auto avatarRects=[&viewportFrame](AbstractChatMessage* msg) -> std::optional<AvatarRects>
    {
        if (msg==nullptr)
        {
            return std::nullopt;
        }
        auto* anchored=msg->avatarWidget();
        if (anchored==nullptr || !anchored->isVisibleTo(viewportFrame))
        {
            return std::nullopt;
        }
        return AvatarRects{
            QRect{viewportFrame->mapFromGlobal(anchored->mapToGlobal(QPoint{0,0})),
                  anchored->size()},
            QRect{viewportFrame->mapFromGlobal(msg->mapToGlobal(QPoint{0,0})),msg->size()}
        };
    };

    // Horizontal placement: centre of the bottom-most message's avatar COLUMN (laid out on every
    // row regardless of whether that row's own avatar image is shown), not its avatar image
    // (position-less on a row whose avatar is suppressed by isAvatarVisible()).
    auto* column=bottomMsg->avatarColumnWidget();
    QRect columnRect{viewportFrame->mapFromGlobal(column->mapToGlobal(QPoint{0,0})),column->size()};

    // Vertical clamp: the batch head's bubble top, so the floating avatar never rises above it
    // (ChatFloatingAvatar::updatePosition() pushes DOWN to this, never up).
    int clampTopY=0;
    if (headMsg!=nullptr && headMsg->content()!=nullptr)
    {
        auto* bubble=headMsg->content();
        clampTopY=viewportFrame->mapFromGlobal(bubble->mapToGlobal(QPoint{0,0})).y();
    }

    // ...and the opposite bound: never sink below the batch tail's own anchored avatar, so the
    // two land exactly on top of each other once that row is far enough in.
    auto tailRects=avatarRects(tailMsg);
    std::optional<int> anchoredTopY;
    if (tailRects.has_value())
    {
        anchoredTopY=tailRects->avatar.top();
    }

    m_floatingAvatar->setMessage(bottomMsg);
    m_floatingAvatar->setTargetColumn(columnRect.left(),columnRect.width());
    m_floatingAvatar->setClampTopY(clampTopY);
    m_floatingAvatar->setAnchoredTopY(anchoredTopY);
    m_floatingAvatar->setWanted(true);

    // Now that it is positioned, hide the avatar of the row it is standing in for, and release it
    // again once it has moved off -- both instantly, so the swap between the two is not visible.
    //
    // ONLY that row is ever a candidate. An earlier cut also offered the previous batch's tail
    // here, as a guard against drawing over an avatar already on screen at a batch boundary, but
    // that avatar belongs to a different batch -- in a personal chat, to the other party -- so
    // hiding it leaves that batch with no avatar at all while this copy shows someone else's.
    // It is also unreachable by construction: clampTopY below keeps this copy at or under the
    // batch head's bubble top, and the previous batch's row ends above that.
    //
    // Tested against the whole ROW, not against that row's avatar rect: the copy stops at its
    // resting place while the row keeps sliding, so once the row's own avatar has slid below the
    // copy the two rects no longer meet even though the copy is still standing in for exactly
    // that row. Releasing there put both on screen at once, the real one peeking out just below
    // the floating one, until the row finally left the viewport.
    QRect floatingRect=m_floatingAvatar->geometry().adjusted(0,-m_floatingAvatar->occlusionMargin(),
                                                             0,m_floatingAvatar->occlusionMargin());
    AbstractChatMessage* messageToObscure=nullptr;
    if (tailRects.has_value() && tailRects->row.intersects(floatingRect))
    {
        messageToObscure=tailMsg;
    }
    setObscuredAvatarMessage(messageToObscure);
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::blockFloatingAvatar()
{
    // Same construction-order caveat as scheduleFloatingAvatarUpdate(): effectiveAlignSentChanged()
    // can fire from the first resizeEvent(), which may land before these are built.
    if (m_floatingAvatarBlockTimer==nullptr || m_floatingAvatar==nullptr)
    {
        return;
    }

    // Back to the resting state on both sides at once: the copy off screen, every row showing
    // its own avatar again.
    m_floatingAvatar->hideNow();
    setObscuredAvatarMessage(nullptr);

    // Anything already queued for this frame was derived from the PREVIOUS alignment.
    m_floatingAvatarTimer->cancel();

    m_floatingAvatarBlocked=true;
    m_floatingAvatarBlockTimer->shot(
        static_cast<size_t>(m_floatingAvatar->modeChangeBlockMs()),
        [this]()
        {
            m_floatingAvatarBlocked=false;
            scheduleFloatingAvatarUpdate();
        },
        // restart=true: a second flip inside the window (a drag-resize crossing
        // alignSentLeftWidth back and forth) restarts the full settle delay rather than letting
        // the first one's deadline expire mid-relayout.
        true
    );
}

//--------------------------------------------------------------------------

template <typename BaseMessageT,typename Traits>
void ChatMessagesView<BaseMessageT,Traits>::setObscuredAvatarMessage(AbstractChatMessage* msg)
{
    if (m_obscuredAvatarMsg==msg)
    {
        return;
    }

    if (m_obscuredAvatarMsg!=nullptr)
    {
        m_obscuredAvatarMsg->setAvatarObscured(false);
    }
    m_obscuredAvatarMsg=msg;
    if (m_obscuredAvatarMsg!=nullptr)
    {
        m_obscuredAvatarMsg->setAvatarObscured(true);
    }
}

//--------------------------------------------------------------------------

}

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_IPP
