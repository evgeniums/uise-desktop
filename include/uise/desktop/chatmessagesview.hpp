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
#include <QShortcut>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

template <typename BaseMessageT, typename Traits>
class ChatMessagesViewItem : public BaseMessageT
{
    public:

        using Id=typename Traits::Id;
        using SortValue=typename Traits::SortValue;

        explicit ChatMessagesViewItem(QWidget* parent=nullptr) :
            BaseMessageT(parent),
            m_msg(this),
            m_ui(nullptr)
        {}

        AbstractChatMessage* ui() override
        {
            return m_ui;
        }

        BaseMessageT* msg()
        {
            return m_msg;
        }

        const BaseMessageT* msg() const
        {
            return m_msg;
        }

        void setDateSeparatorVisible(bool enable, bool withYear=false);

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override;

    private:

        AbstractChatMessage* m_ui;
        BaseMessageT* m_msg;
};

template <typename BaseMessageT, typename Traits>
struct ChatMessagesViewItemTraits : public FlyweightListItemTraits<ChatMessagesViewItem<BaseMessageT,Traits>*,
                                                              AbstractChatMessage,
                                                              typename BaseMessageT::SortValue,
                                                              typename BaseMessageT::Id>
{
    using Item=ChatMessagesViewItem<BaseMessageT,Traits>;

    static auto sortValue(const Item* item) noexcept
    {
        return item->sortValue();
    }

    static auto widget(Item* item) noexcept
    {
        return item->ui();
    }

    static auto id(const Item* item)
    {
        return item->id();
    }
};

template <typename BaseMessageT, typename Traits>
using ChatMessageViewItemWrapper=FlyweightListItem<ChatMessagesViewItemTraits<BaseMessageT,Traits>>;

template <typename BaseMessageT, typename Traits>
using ChatMessagesViewWidget=FlyweightListView<ChatMessageViewItemWrapper<BaseMessageT,Traits>>;

class UISE_DESKTOP_EXPORT ChatMessagesViewQ : public QObject
{
    Q_OBJECT

    public:

        constexpr static const int MouseMoveDetectDelta=10;

        using QObject::QObject;

    signals:

        void reloadRequested();
};

template <typename BaseMessageT, typename Traits>
class ChatMessagesView : public MouseMoveEventFilter
{
    public:

        using Data=typename Traits::Data;

        using Id=typename BaseMessageT::Id;
        using SortValue=typename BaseMessageT::SortValue;
        using Message=ChatMessagesViewItem<BaseMessageT,Traits>;

        using FuncById=std::function<void (const Id&)>;
        using FuncBySortValue=std::function<void (const SortValue&)>;
        using MessageBuilder=std::function<Message* (const Data& data, QWidget* parent)>;
        using FuncItemsRequested=std::function<void (const SortValue& start, size_t maxCount, Direction direction)>;

        explicit ChatMessagesView(QWidget* parent=nullptr);

        ~ChatMessagesView();

        ChatMessagesView(const ChatMessagesView&)=delete;
        ChatMessagesView(ChatMessagesView&&)=delete;
        ChatMessagesView& operator=(const ChatMessagesView&)=delete;
        ChatMessagesView& operator=(ChatMessagesView&&)=delete;

        ChatMessagesViewQ* qObject()
        {
            return m_qobj;
        }

        ChatMessagesViewWidget<BaseMessageT,Traits>* listView()
        {
            return m_listView;
        }

        const ChatMessagesViewWidget<BaseMessageT,Traits>* listView() const
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

        void loadMessages(const std::vector<Data>& items);

        void insertContinuousMessages(const std::vector<Data>& items, int wasRequestedMaxCountoverride, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=true);

        void clear();

        void insertMessage(Data item);

        void jumpToEdge(Direction direction);

        Message* message(const Id& id) const;

        std::vector<Data> selectedMessages() const;

    protected:

        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        Message* makeMessage(const Data& data);

        ChatMessagesViewQ* m_qobj=nullptr;

        QBoxLayout* m_layout=nullptr;
        ChatMessagesViewWidget<BaseMessageT,Traits>* m_listView;
        bool m_selectionMode=false;

        FuncItemsRequested m_onItemsRequested;
        MessageBuilder m_messageBuilder;

        QPointer<AbstractChatMessage> m_chatUnderMouse;
        QPoint m_lastMousePos;
        std::map<Id,Data> m_selectedMessages;
        std::optional<bool> m_mouseMoveUp;

        void onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers);

        void insertFetched(bool forLoad, const std::vector<Data>& items, int wasRequestedMaxCount=0, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=false);

        void replaceSelectedData(Message* msg);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_HPP
