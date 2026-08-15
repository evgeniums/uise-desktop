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
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/chatdatesubtitle.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

template <typename BaseMessageT, typename Traits>
class ChatMessagesViewItem : public BaseMessageT
{
    public:

        using Id=typename Traits::Id;
        using SortValue=typename Traits::SortValue;

        explicit ChatMessagesViewItem(QWidget* parent=nullptr) :
            BaseMessageT(parent),
            m_ui(nullptr),
            m_msg(this)
        {}

        AbstractChatMessage* ui() override
        {
            return m_ui;
        }

        AbstractChatMessage* widget() const
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

        bool isDateSeparatorVisible() const
        {
            return m_dtSepVisible;
        }

        void setUnreadSeparatorVisible(bool enable, const QString& text={});

        bool isUnreadSeparatorVisible() const
        {
            return m_unreadSepVisible;
        }

        bool isTopSeparatorVisible() const
        {
            return isDateSeparatorVisible() || isUnreadSeparatorVisible();
        }

        //! Invoked when the date separator's pill is clicked. Args: the message's local date,
        //! and the global position of the pill's bottom-left corner (an anchor point for a
        //! popup, see FloatingDialogFrame::popupAt()).
        using DateSectionClickedCb=std::function<void (const QDate& date, const QPoint& globalAnchorPos)>;

        void setDateSectionClickedCb(DateSectionClickedCb cb)
        {
            m_dateSectionClickedCb=std::move(cb);
        }

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override;

    private:

        AbstractChatMessage* m_ui;
        BaseMessageT* m_msg;
        bool m_dtSepVisible=false;
        bool m_unreadSepVisible=false;
        DateSectionClickedCb m_dateSectionClickedCb;
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

class UISE_DESKTOP_EXPORT AbstractChatMessagesView : public QFrame
{
    Q_OBJECT

    public:

        constexpr static const int MouseMoveDetectDelta=10;

        using QFrame::QFrame;

        QString unreadSeparatorTitle() const { return tr("Unread messages"); }

    signals:

        void reloadRequested();
        void selectionModeToggled(bool enable);
        void selectedCountChanged(size_t count);
        void copySelectedRequested();
        void viewportUpdated();

        /**
         * @brief A date pill was clicked -- either an inline separator's date section or the
         *  floating ChatDateSubtitle.
         * @param date Local date the pill displays.
         * @param globalAnchorPos Global position of the pill's bottom-left corner.
         *
         * The view itself does nothing with this; an embedder is expected to open a date
         * picker anchored at globalAnchorPos and jump the history to the chosen date.
         */
        void dateSectionClicked(const QDate& date, const QPoint& globalAnchorPos);
};

template <typename BaseMessageT, typename Traits>
class ChatMessagesView : public AbstractChatMessagesView
{
    public:

        using Data=typename Traits::Data;
        using Message=ChatMessagesViewItem<BaseMessageT,Traits>;

        using Id=typename Traits::Id;
        using SortValue=typename Traits::SortValue;

        using MessageBuilder=std::function<Message* (const Data& data, QWidget* parent)>;
        using FuncItemsRequested=std::function<void (const SortValue& start, size_t maxCount, Direction direction)>;

        using MessageHandler=std::function<bool (Message*)>;

        explicit ChatMessagesView(QWidget* parent=nullptr);

        ~ChatMessagesView();

        ChatMessagesView(const ChatMessagesView&)=delete;
        ChatMessagesView(ChatMessagesView&&)=delete;
        ChatMessagesView& operator=(const ChatMessagesView&)=delete;
        ChatMessagesView& operator=(ChatMessagesView&&)=delete;

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

        void clearOtherContentsSelection(const Id& excludeId={});

        void setOnItemsRequested(FuncItemsRequested handler)
        {
            m_onItemsRequested=handler;
        }

        void setMessageBuilder(MessageBuilder messageBuilder)
        {
            m_messageBuilder=messageBuilder;
        }

        void adjustMessageList(std::vector<Message*>& messages);

        void loadMessages(const std::vector<Data>& items);

        void insertContinuousMessages(const std::vector<Data>& items, int wasRequestedMaxCountoverride, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=true);

        //! Replace the whole list with a window centred on an arbitrary anchor (not necessarily
        //! an edge), then scroll to it. `minSortValue`/`maxSortValue` are the "is there more data
        //! beyond this window" markers the flyweight list uses for prefetch/jumpToEdge -- unlike
        //! loadMessages() (always an edge-anchored load), a mid-list window can't infer them from
        //! `items` alone, so the caller must supply them explicitly. `offset` is forwarded to
        //! scrollToItem() as-is (0 aligns anchorId's leading edge with the viewport's top).
        void loadMessagesAround(const std::vector<Data>& items, const Id& anchorId,
                                const SortValue& minSortValue, const SortValue& maxSortValue,
                                int offset=0);

        //! Scroll to a message already present in the loaded window. False if `id` isn't loaded
        //! (this never fetches -- the caller must have loaded a window containing it first, e.g.
        //! via loadMessagesAround()).
        bool scrollToMessage(const Id& id, int offset=0);

        void clear();

        void insertMessage(const Data& item);
        void updateMessage(const Data& item);
        void replaceMessage(const Id& replaceId, const Data& newItem);
        void removeMessage(const Id& id);
        void reorderMessage(const Id& id);

        void jumpToEdge(Direction direction);

        Message* message(const Id& id) const;

        std::vector<Data> selectedMessages() const;

        SortValue lastViewportSortValue() const;

        Id lastViewportSeqId() const;

        void setUnreadMessageCount(const QString& count);

        bool eachMessage(MessageHandler handler);

        bool rEachMessage(MessageHandler handler);

        void readjustList();

        ChatDateSubtitle* dateSubtitle() const
        {
            return m_dateSubtitle;
        }

        void setDateSubtitleEnabled(bool enable)
        {
            m_dateSubtitleEnabled=enable;
            if (!enable)
            {
                m_dateSubtitle->hideNow();
            }
        }

        bool isDateSubtitleEnabled() const noexcept
        {
            return m_dateSubtitleEnabled;
        }

    protected:

        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        //! App-wide filter, watching only this widget's own top-level window losing activation
        //! -- see resetMouseSelectionState()'s doc comment for why that alone is what actually
        //! catches the case a real mouseReleaseEvent() misses.
        bool eventFilter(QObject* watched, QEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:

        QBoxLayout* m_layout=nullptr;
        ChatMessagesViewWidget<BaseMessageT,Traits>* m_listView;
        bool m_selectionMode=false;

        ChatDateSubtitle* m_dateSubtitle=nullptr;
        bool m_dateSubtitleEnabled=true;

        FuncItemsRequested m_onItemsRequested;
        MessageBuilder m_messageBuilder;

        QPointer<AbstractChatMessage> m_chatUnderMouse;
        QPoint m_lastMousePos;
        std::map<Id,Data> m_selectedMessages;
        std::optional<bool> m_mouseMoveUp;

        int m_messageBubbleOuterWidth=0;
        int m_messageMinWidth=0;
        int m_messageMaxWidth=QWIDGETSIZE_MAX;

        SingleShotTimer* m_resizeTimer=nullptr;
        SingleShotTimer* m_selectionModeTimer=nullptr;

    private:

        //! Clears the drag-tracking state mouseMoveEvent() above reads. Normally only
        //! mouseReleaseEvent() needs this, but a mousePressEvent delivered to a bubble (e.g.
        //! ImageLabel, which fires its own clicked() on press rather than release -- see
        //! AbstractChatMessage::detectMouseSelection()) can open a new top-level window (the
        //! fullscreen image viewer) that steals activation before the matching
        //! mouseReleaseEvent ever reaches this widget. Qt's per-widget event()->buttons()
        //! tracking then stays "left button down" forever, and mouseMoveEvent() -- gated only
        //! on that flag -- starts drag-selecting messages on every subsequent plain mouse move,
        //! until an explicit fresh click resets it. eventFilter() above calls this on
        //! WindowDeactivate of this widget's own top-level window to close that gap; a plain
        //! changeEvent() override was not used because window-activation events are not
        //! reliably delivered to descendant widgets across Qt versions/platforms (see
        //! ImageLabel::changeEvent()'s own comment).
        void resetMouseSelectionState();

        Message* makeMessage(const Data& data);

        void onJumpRequested(Direction direction, bool forceLongJump, Qt::KeyboardModifiers modifiers);

        void insertFetched(bool forLoad, const std::vector<Data>& items, int wasRequestedMaxCount=0, Direction wasRequestedDirection=Direction::END, bool jumpToEnd=false);

        void replaceSelectedData(Message* msg);

        void doInsertMessage(const Data& item);
        void doRemoveMessage(const Id& id);
        void doReorderMessage(const Id& id);
        void adjustCurrentMessagesList();

        void onMessageClicked(const Id& id);

        void adjustMessagesSizes(std::vector<Message*>* messages=nullptr);
        int messageContentWidth() const;
        int defaultMessageContentWidth() const;
        void adjustMesssageSize(Message* msg);

        void onUserScrolled();
        void updateDateSubtitleText();
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGESVIEW_HPP
