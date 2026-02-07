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

/** @file uise/desktop/chatmessagesview.hpp
*
*  Declares ChatMessagesView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGESVIEW_HPP
#define UISE_DESKTOP_CHATMESSAGESVIEW_HPP

#include <QBoxLayout>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

template <class DataT>
class ChatMessagesViewItem : public WidgetController
{
    public:

        using Id=typename DataT::Id;
        using SortValue=typename DataT::SortValue;

        using WidgetController::WidgetController;

        const AbstractChatMessage* ui() const
        {
            return m_ui;
        }

        AbstractChatMessage* ui()
        {
            return m_ui;
        }

        virtual QDateTime dateTime() const =0;

        void setData(DataT data)
        {
            m_data=std::move(data);
            m_data->setParent(this);
        }

        const DataT& data() const
        {
            return m_data;
        }

        DataT& data()
        {
            return m_data;
        }

        bool operator < (const ChatMessagesViewItem<DataT>& other) const noexcept
        {
            return m_data->sortValue() < other.m_data->sortValue();
        }

        void setDateSeparatorVisible(bool enable, bool withYear=false);

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override;

    private:

        AbstractChatMessage* m_ui;
        DataT m_data;
};

template <class DataT>
struct ChatMessagesViewItemTraits : public FlyweightListItemTraits<ChatMessagesViewItem<DataT>*,
                                                              AbstractChatMessage,
                                                              typename DataT::SortValue,
                                                              typename DataT::Id>
{
    using Item=ChatMessagesViewItem<DataT>;

    static auto sortValue(const Item* item) noexcept
    {
        return item->data()->sortValue();
    }

    static auto widget(Item* item) noexcept
    {
        return item->ui();
    }

    static auto id(const Item* item)
    {
        return item->data()->id();
    }
};

template <class DataT>
using ChatMessageViewItemWrapper=FlyweightListItem<ChatMessagesViewItemTraits<DataT>>;

template <class DataT>
using ChatMessagesViewWidget=FlyweightListView<ChatMessageViewItemWrapper<DataT>>;

class UISE_DESKTOP_EXPORT ChatMessagesViewQ : public QObject
{
    Q_OBJECT

    public:

        using QObject::QObject;

    signals:

        void reloadRequested();
};

template <class DataT>
class ChatMessagesView : public QFrame
{
    public:

        using Id=typename DataT::Id;
        using SortValue=typename DataT::SortValue;
        using Message=ChatMessagesViewItem<DataT>;

        using MessageBuilder=std::function<Message* (DataT data, QWidget* parent)>;

        using FuncById=std::function<void (const Id&)>;
        using FuncBySortValue=std::function<void (const SortValue&)>;
        using FuncItemsRequested=std::function<void (const SortValue& start, size_t maxCount, Direction direction)>;


        explicit ChatMessagesView(QWidget* parent=nullptr);

        ~ChatMessagesView();

        ChatMessagesView(const ChatMessagesView&)=delete;
        ChatMessagesView(ChatMessagesView&&)=delete;
        ChatMessagesView& operator=(const ChatMessagesView&)=delete;
        ChatMessagesView& operator=(ChatMessagesView&&)=delete;

        QObject* qObject()
        {
            return m_qobj;
        }

        ChatMessagesViewWidget<DataT>* listView()
        {
            return m_listView;
        }

        const ChatMessagesViewWidget<DataT>* listView() const
        {
            return m_listView;
        }

        void setSelectionMode(bool enable);

        bool isSelectionMode() const noexcept
        {
            return m_selectionMode;
        }

        void clearOtherContentsSelection(const Id& excludeId);

        void onMessageClicked(const Id& id);

        void setOnItemsRequested(FuncItemsRequested handler)
        {
            m_onItemsRequested=handler;
        }

        void setMessageBuilder(MessageBuilder messageBuilder)
        {
            m_messageBuilder=messageBuilder;
        }

        void adjustMessageList(std::vector<Message*>& messages);

        void removeMessage(const Id& id);
        void reorderMessage(const Id& id);

        void loadMessages(const std::vector<DataT>& items);

        void insertContinuousMessages(const std::vector<DataT>& items, int wasRequestedMaxCountoverride, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=true);

        void clear();

        void insertMessage(DataT item);

        void jumpToEdge(Direction direction);

    private:

        ChatMessagesViewQ* m_qobj=nullptr;

        QBoxLayout* m_layout=nullptr;
        ChatMessagesViewWidget<DataT>* m_listView;
        bool m_selectionMode=false;

        FuncBySortValue m_onItemsRequested;
        MessageBuilder m_messageBuilder;

        void onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers);

        void insertFetched(bool forLoad, const std::vector<DataT>& items, int wasRequestedMaxCount=0, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=false);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_HPP
