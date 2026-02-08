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

template <typename BaseMessageT>
class ChatMessagesViewItem : public BaseMessageT
{
    public:

        using Id=typename BaseMessageT::Id;
        using SortValue=typename BaseMessageT::SortValue;

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

template <typename BaseMessageT>
struct ChatMessagesViewItemTraits : public FlyweightListItemTraits<ChatMessagesViewItem<BaseMessageT>*,
                                                              AbstractChatMessage,
                                                              typename BaseMessageT::SortValue,
                                                              typename BaseMessageT::Id>
{
    using Item=ChatMessagesViewItem<BaseMessageT>;

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

template <typename BaseMessageT>
using ChatMessageViewItemWrapper=FlyweightListItem<ChatMessagesViewItemTraits<BaseMessageT>>;

template <typename BaseMessageT>
using ChatMessagesViewWidget=FlyweightListView<ChatMessageViewItemWrapper<BaseMessageT>>;

class UISE_DESKTOP_EXPORT ChatMessagesViewQ : public QObject
{
    Q_OBJECT

    public:

        constexpr static const int MouseMoveDetectDelta=10;

        using QObject::QObject;

    signals:

        void reloadRequested();
};

template <typename BaseMessageT, typename DataT>
class ChatMessagesView : public MouseMoveEventFilter
{
    public:

        using Id=typename BaseMessageT::Id;
        using SortValue=typename BaseMessageT::SortValue;
        using Message=ChatMessagesViewItem<BaseMessageT>;

        using FuncById=std::function<void (const Id&)>;
        using FuncBySortValue=std::function<void (const SortValue&)>;
        using MessageBuilder=std::function<Message* (DataT data, QWidget* parent)>;
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

        ChatMessagesViewWidget<BaseMessageT>* listView()
        {
            return m_listView;
        }

        const ChatMessagesViewWidget<BaseMessageT>* listView() const
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

        Message* message(const Id& id) const;

    protected:

        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        Message* makeMessage(DataT data);

        ChatMessagesViewQ* m_qobj=nullptr;

        QBoxLayout* m_layout=nullptr;
        ChatMessagesViewWidget<BaseMessageT>* m_listView;
        bool m_selectionMode=false;

        FuncItemsRequested m_onItemsRequested;
        MessageBuilder m_messageBuilder;

        QPointer<AbstractChatMessage> m_chatUnderMouse;
        QPoint m_lastMousePos;
        std::set<Id> m_selectedMessages;
        std::optional<bool> m_mouseMoveUp;

        void onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers);

        void insertFetched(bool forLoad, const std::vector<DataT>& items, int wasRequestedMaxCount=0, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=false);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_HPP
