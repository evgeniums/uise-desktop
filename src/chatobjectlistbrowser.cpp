/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/chatobjectlistbrowser.cpp
*
*  Defines ChatObjectListBrowser and the flyweight list behind it.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QBoxLayout>
#include <QFrame>
#include <QLabel>

#include <uise/desktop/chatobjectlistbrowser.hpp>
// The list is virtualized over these. The .ipp is included, not just the declaration header,
// because this file INSTANTIATES the template (the convention every other in-tree instantiation
// site follows).
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/utils/layout.hpp>

namespace uise {

//--------------------------------------------------------------------------
// The flyweight list. Declared (opaque) in the header, so it has external linkage; everything
// else here is only reachable through it.
//--------------------------------------------------------------------------

//! Sort value of a row in the list: the object's key with the order turned around, so that the
//! view's ascending order is newest first. Home (the top) is the newer side, End the older one.
struct ChatObjectViewPos
{
    ChatObjectKey key;

    friend bool operator<(const ChatObjectViewPos& left, const ChatObjectViewPos& right) noexcept
    {
        return right.key<left.key;
    }
};

/**
 * @brief The widget the list holds per object: the kind's own row under an optional month heading.
 *
 * A container instead of a heading item of the list: a heading that is a list item of its own would
 * have to be placed between the objects by a sort value of its own, and would move when a newer
 * object of its month arrives. Here the first object of a month just shows the heading and loses it
 * when a newer one of the same month turns up. No Q_OBJECT, see ChatObjectListView.
 */
class ChatObjectListRow : public QFrame
{
    public:

        explicit ChatObjectListRow(QWidget* parent)
            : QFrame(parent)
        {
            setObjectName("listRow");
            m_layout=Layout::vertical(this);

            m_header=new QLabel(this);
            m_header->setObjectName("monthHeader");
            // plain text: month names are not markup
            m_header->setTextFormat(Qt::PlainText);
            // A styled QLabel indents its text by half a character on its own; the rows below do not, and the heading
            // must start where they do.
            m_header->setIndent(0);
            m_header->setMargin(0);
            m_header->setVisible(false);
            m_layout->addWidget(m_header);
        }

        //! A title line between the heading and the kind's row; before setInner().
        void setTitle(QWidget* title)
        {
            m_title=title;
            m_layout->addWidget(title);
        }

        QWidget* title() const noexcept
        {
            return m_title;
        }

        void setInner(QFrame* inner)
        {
            m_inner=inner;
            m_layout->addWidget(inner);
        }

        //! The kind's own row, the one the row factory built.
        QFrame* inner() const noexcept
        {
            return m_inner;
        }

        //! An empty title hides the heading.
        void setHeader(const QString& title)
        {
            if (m_header->text()!=title)
            {
                m_header->setText(title);
            }
            m_header->setVisible(!title.isEmpty());
        }

    private:

        QBoxLayout* m_layout=nullptr;
        QLabel* m_header=nullptr;
        QWidget* m_title=nullptr;
        QFrame* m_inner=nullptr;
};

//! What the flyweight list holds per row: the widget plus what the list needs to place it.
struct ChatObjectRowHandle
{
    QFrame* widget=nullptr;
    ChatObjectViewPos pos;
    ChatObjectKey key;
    QString uid;
};

struct ChatObjectListItemTraits : public FlyweightListItemTraits<ChatObjectRowHandle,QFrame,ChatObjectViewPos,QString>
{
    static QFrame* widget(const ChatObjectRowHandle& handle) noexcept
    {
        return handle.widget;
    }

    // By value on purpose: the list's multi_index keys are const_mem_fun<..., SortValueType, ...>
    // and IdType, which need the exact return type.
    static ChatObjectViewPos sortValue(const ChatObjectRowHandle& handle) noexcept
    {
        return handle.pos;
    }

    static QString id(const ChatObjectRowHandle& handle)
    {
        return handle.uid;
    }
};

using ChatObjectListItem=FlyweightListItem<ChatObjectListItemTraits>;

/**
 * @brief A FlyweightListView of rows of one kind, fed from the browser's model.
 *
 * No Q_OBJECT, deliberately: a Q_OBJECT class defined in a .cpp needs its own moc include, which
 * this project avoids. Nothing here has to emit -- it talks to the browser through std::functions.
 *
 * The list never receives items from anywhere but ServeFn, which answers "the N items beyond this
 * key" from the browser's model, so the list is gap-free by construction.
 */
class ChatObjectListView : public QFrame
{
    public:

        using View=FlyweightListView<ChatObjectListItem>;
        using ServeFn=std::function<std::vector<ChatObjectItem> (const ChatObjectKey& anchor, size_t count, bool older)>;

        //! Rows the list keeps ready beyond each edge, and the size of one prefetch request.
        //! File and voice rows are heavy (each one owns a menu and a load control), so this is
        //! small -- see the tuning block in the constructor.
        constexpr static const size_t PrefetchWindow=12;

        //! Rows built in one go when the list is (re)loaded.
        constexpr static const size_t InitialWindow=20;

        /**
         * @brief Rows loaded NEWER than a jump target, so a jump does not land flush against the
         *  begin edge.
         *
         * Load-bearing, not cosmetic: landing exactly at the begin edge with newer rows still
         * unfetched makes the view prepend a batch and then, because it sticks to HOME, scroll
         * onto the row it just prepended -- repeated, the jump crawls back to the newest row.
         * See the same constant in ChatReactionGalleryGrid.
         */
        constexpr static const size_t LeadingMargin=PrefetchWindow;

        ChatObjectListView(
                const ChatObjectBrowserModel& model,
                ChatObjectRowFactory factory,
                ChatObjectRowHooks hooks,
                ServeFn serve,
                std::function<void ()> jumpHome,
                QWidget* parent=nullptr
            ) : QFrame(parent),
                m_model(model),
                m_factory(std::move(factory)),
                m_hooks(std::move(hooks)),
                m_serve(std::move(serve)),
                m_jumpHome(std::move(jumpHome))
        {
            setObjectName("objectList");
            auto* layout=Layout::vertical(this);

            // --- prefetch tuning: the stock defaults are wrong for THESE rows ---------------
            //
            // The defaults keep ~15 hidden rows ready per side, request 20 at a time and evict
            // only past 100 hidden ones, i.e. over 200 rows resident. A chat row is some text;
            // a file row here owns a drop-down menu and a load control. Retuned to about one
            // screen of slack per side: ~6 hidden rows ready, requests of PrefetchWindow,
            // eviction past twice that. MaxHiddenRatio is a static of the class TEMPLATE, so
            // assigning it touches only this instantiation, and it must be set BEFORE the view
            // is constructed because the pimpl copies it in its own constructor.
            View::MaxHiddenRatio=2.0;

            m_view=new View(this,PrefetchWindow);
            m_view->setObjectName("objectRows");
            m_view->setOrientation(Qt::Vertical);
            m_view->setPrefetchScreensCount(1.0);
            m_view->setPrefetchThresholdRatio(0.5);
            m_view->setPrefetchItemCount(PrefetchWindow);
            // HOME, not the END default of the chat: a browser opens at the newest object, which
            // is the first row, and stays there until the user scrolls. Sticking to the end
            // would walk the view down through the whole history.
            m_view->setStickMode(Direction::HOME);
            layout->addWidget(m_view);

            m_view->setRequestItemsCb(
                [this](const ChatObjectListItem* item, size_t count, Direction direction)
                {
                    onItemsRequested(item,count,direction);
                }
            );

            // The jump-edge control ("back to the newest" button the view floats over itself once
            // enough rows are hidden above the viewport). It calls out only when the newest row is
            // no longer among the loaded rows.
            m_view->setRequestHomeCb(
                [this](bool forceLongJump, Qt::KeyboardModifiers modifiers)
                {
                    Q_UNUSED(forceLongJump)
                    Q_UNUSED(modifiers)
                    if (m_jumpHome)
                    {
                        m_jumpHome();
                    }
                }
            );
        }

        void reset()
        {
            m_view->clear();
        }

        /**
         * @brief Replace the content.
         * @param items All items of the model, newest first.
         * @param focus Key to put at the top, or a null key to start at the newest item.
         */
        void load(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus)
        {
            size_t focusIdx=0;
            bool focusFound=false;
            if (!focus.isNull())
            {
                for (size_t i=0; i<items.size(); ++i)
                {
                    if (items[i].key==focus)
                    {
                        focusIdx=i;
                        focusFound=true;
                        break;
                    }
                }
            }

            const size_t first=(focusFound && focusIdx>LeadingMargin) ? focusIdx-LeadingMargin : 0;
            const size_t last=std::min(items.size(),first+InitialWindow);

            std::vector<ChatObjectListItem> handles;
            handles.reserve(last-first);
            for (size_t i=first; i<last; ++i)
            {
                handles.emplace_back(makeHandle(items[i]));
            }

            // loadItems() clears the view, and clearing resets the "end reached" markers --
            // they are set AFTER it, never before.
            m_view->loadItems(handles);
            pinEdges();

            if (focusFound)
            {
                m_view->scrollToItemEdge(items[focusIdx].uid(),Direction::HOME);
            }
        }

        void remove(const std::vector<ChatObjectKey>& keys)
        {
            std::vector<QString> uids;
            for (const auto& key : keys)
            {
                auto uid=key.toString();
                if (m_view->hasItem(uid))
                {
                    uids.push_back(std::move(uid));
                }
            }
            if (!uids.empty())
            {
                m_view->removeItems(uids);
            }
            // the object after a removed first of a month is the first now
            refreshHeaders();
            pinEdges();
        }

        void setGrouped(bool enable)
        {
            m_group=enable;
            refreshHeaders();
        }

        void updateObject(const ChatObjectItem& object)
        {
            const auto uid=object.uid();
            const auto* current=m_view->item(uid);
            if (current==nullptr)
            {
                return;
            }

            if (m_factory.update)
            {
                auto* row=static_cast<ChatObjectListRow*>(current->widget());
                m_factory.update(row->inner(),object);
                if (m_factory.updateTitle && row->title()!=nullptr)
                {
                    m_factory.updateTitle(row->title(),object);
                }
                return;
            }

            // no updater: replace the widget
            m_view->beginUpdate();
            m_view->removeItem(uid);
            m_view->insertItem(ChatObjectListItem(makeHandle(object)),false);
            pinEdges();
            m_view->endUpdate();
        }

        //! The model grew on a side: take from it what the view was waiting for.
        void edgeArrived(bool older)
        {
            const auto* edge=older ? m_view->lastItem() : m_view->firstItem();
            if (edge==nullptr)
            {
                return;
            }
            fetch(edge->item().key,PrefetchWindow,older);
        }

        QFrame* rowWidget(const ChatObjectKey& key) const
        {
            const auto* current=m_view->item(key.toString());
            return current!=nullptr ? static_cast<ChatObjectListRow*>(current->widget())->inner() : nullptr;
        }

    private:

        //! Month heading of the object's row: its title if the object is the first of its month
        //! among the loaded ones and the list is grouped, else empty.
        QString headerFor(const ChatObjectKey& key) const
        {
            if (!m_group || !m_model.startsMonth(key))
            {
                return QString{};
            }
            const auto* object=m_model.find(key);
            return object!=nullptr ? chatObjectMonthTitle(object->dateTime) : QString{};
        }

        //! Redo the headings of the rows that exist: what the first object of a month is changes
        //! when objects arrive on the newer side or go away.
        void refreshHeaders()
        {
            m_view->eachItem(
                [this](const ChatObjectListItem* item)
                {
                    static_cast<ChatObjectListRow*>(item->widget())->setHeader(headerFor(item->item().key));
                    return true;
                }
            );
        }

        ChatObjectRowHandle makeHandle(const ChatObjectItem& object)
        {
            ChatObjectRowHandle handle;
            handle.key=object.key;
            handle.pos.key=object.key;
            handle.uid=object.uid();
            auto* row=new ChatObjectListRow(m_view->itemsParentWidget());
            if (m_factory.buildTitle)
            {
                if (auto* title=m_factory.buildTitle(row))
                {
                    row->setTitle(title);
                    if (m_factory.updateTitle)
                    {
                        m_factory.updateTitle(title,object);
                    }
                }
            }
            QFrame* inner=nullptr;
            if (m_factory.build)
            {
                inner=m_factory.build(object,row,m_hooks);
            }
            if (inner==nullptr)
            {
                // a broken factory must not crash the list
                inner=new QFrame(row);
            }
            row->setInner(inner);
            row->setHeader(headerFor(object.key));
            handle.widget=row;
            return handle;
        }

        /**
         * @brief Tell the view which of its ends are the real ends of the data.
         *
         * Without it the view believes there is always more and keeps asking. The newest end is
         * the view's first (min) item, the oldest end its last (max) one. A marker is set only
         * when the owner has said that side is exhausted AND the view's edge item is the model's
         * edge item, i.e. nothing is left between them.
         */
        void pinEdges()
        {
            const auto* first=m_view->firstItem();
            const auto* last=m_view->lastItem();
            if (first==nullptr || last==nullptr)
            {
                return;
            }

            const auto* newest=m_model.newest();
            if (m_model.isEndNewer() && newest!=nullptr && first->item().key==newest->key)
            {
                m_view->setMinSortValue(first->sortValue());
            }

            const auto* oldest=m_model.oldest();
            if (m_model.isEndOlder() && oldest!=nullptr && last->item().key==oldest->key)
            {
                m_view->setMaxSortValue(last->sortValue());
            }
        }

        void fetch(const ChatObjectKey& anchor, size_t count, bool older)
        {
            const auto items=m_serve(anchor,count,older);
            if (items.empty())
            {
                pinEdges();
                return;
            }

            std::vector<ChatObjectListItem> handles;
            handles.reserve(items.size());
            for (const auto& object : items)
            {
                handles.emplace_back(makeHandle(object));
            }

            m_view->beginUpdate();
            m_view->insertContinuousItems(handles,false);
            // newer objects may have taken the heading of the row that was the first of a month
            refreshHeaders();
            // before endUpdate(): that is what schedules the check that reads the markers
            pinEdges();
            m_view->endUpdate();
        }

        void onItemsRequested(const ChatObjectListItem* item, size_t count, Direction direction)
        {
            if (item==nullptr || direction==Direction::NONE)
            {
                return;
            }
            fetch(item->item().key,count,direction==Direction::END);
        }

        const ChatObjectBrowserModel& m_model;
        ChatObjectRowFactory m_factory;
        ChatObjectRowHooks m_hooks;
        ServeFn m_serve;
        std::function<void ()> m_jumpHome;
        View* m_view=nullptr;
        bool m_group=true;
};

//--------------------------------------------------------------------------

ChatObjectListBrowser::ChatObjectListBrowser(QString kind, ChatObjectRowFactory factory, QWidget* parent)
    : AbstractChatObjectBrowser(std::move(kind),parent),
      m_listView(nullptr)
{
    setObjectName("chatObjectListBrowser");

    m_listView=new ChatObjectListView(
        model(),
        std::move(factory),
        rowHooks(),
        [this](const ChatObjectKey& anchor, size_t count, bool older)
        {
            return serveEdge(anchor,count,older);
        },
        [this]()
        {
            jumpToNewest();
        },
        this
    );
    m_listView->setGrouped(isGroupByMonth());
    setView(m_listView);
}

//--------------------------------------------------------------------------

QFrame* ChatObjectListBrowser::rowWidget(const ChatObjectKey& key) const
{
    return m_listView->rowWidget(key);
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doReset()
{
    m_listView->reset();
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus)
{
    m_listView->load(items,focus);
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doRemove(const std::vector<ChatObjectKey>& keys)
{
    m_listView->remove(keys);
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doUpdate(const ChatObjectItem& item)
{
    m_listView->updateObject(item);
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doEdgeArrived(bool older)
{
    m_listView->edgeArrived(older);
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::doGroupingChanged()
{
    // only the headings come and go, the rows stay where they are
    m_listView->setGrouped(isGroupByMonth());
}

//--------------------------------------------------------------------------

void ChatObjectListBrowser::jumpToNewest()
{
    if (model().isEndNewer() && !model().empty())
    {
        // the newest end is known and loaded: no need to ask the owner again
        const std::vector<ChatObjectItem> viewItems(model().items().rbegin(),model().items().rend());
        m_listView->load(viewItems,ChatObjectKey{});
        return;
    }
    reload();
}

} // namespace uise
