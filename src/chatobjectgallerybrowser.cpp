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

/** @file uise/desktop/src/chatobjectgallerybrowser.cpp
*
*  Defines ChatObjectGalleryBrowser and the flyweight grid behind it.
*
*/

/****************************************************************************/

#include <algorithm>
#include <optional>

#include <QContextMenuEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>

#include <uise/desktop/chatobjectgallerybrowser.hpp>
#include <uise/desktop/roundedimage.hpp>
// The grid is virtualized over these. The .ipp is included, not just the declaration header,
// because this file INSTANTIATES the template.
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/utils/layout.hpp>

namespace uise {

//--------------------------------------------------------------------------

/**
 * @brief A row of the grid: either a cells row or a month heading. What the grid needs to ask of
 *  a row without knowing which it is. No Q_OBJECT, see ChatObjectListView.
 */
class ChatObjectGalleryRowBase : public QFrame
{
    public:

        using QFrame::QFrame;

        //! The size of the cells and the gap between them changed.
        virtual void setGeometryParams(int cellSize, int spacing)
        {
            Q_UNUSED(cellSize)
            Q_UNUSED(spacing)
        }

        //! Refresh the cell of the object, if this row has it.
        virtual bool updateObject(const ChatObjectItem& object)
        {
            Q_UNUSED(object)
            return false;
        }
};

/**
 * @brief One thumbnail: a placeholder frame with the image on top.
 *
 * The placeholder is the frame's own QSS background (#galleryCell), visible through the
 * transparent image until the thumbnail source delivers a pixmap.
 */
class ChatObjectGalleryCell : public QFrame
{
    public:

        constexpr static const int CornerRadius=6;

        ChatObjectGalleryCell(const ChatObjectItem& object, int size, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_hooks(hooks)
        {
            setObjectName("galleryCell");
            setCursor(Qt::PointingHandCursor);

            auto* layout=Layout::vertical(this);
            m_image=new RoundedImage(this);
            m_image->setObjectName("galleryImage");
            m_image->setCornersRadius(CornerRadius,CornerRadius);
            layout->addWidget(m_image);

            setCellSize(size);
            setObject(object);
        }

        const ChatObjectItem& object() const noexcept
        {
            return m_object;
        }

        void setCellSize(int size)
        {
            setFixedSize(size,size);
            // fixes the image's size as well; a request for a new pixmap follows only if it changed
            m_image->setImageSize(QSize(size,size));
        }

        void setObject(const ChatObjectItem& object)
        {
            const bool thumbnailChanged=object.thumbnailSource!=m_object.thumbnailSource
                                        || object.thumbnailPath.path()!=m_object.thumbnailPath.path();
            m_object=object;
            if (thumbnailChanged && m_object.thumbnailSource && !m_object.thumbnailPath.empty())
            {
                m_image->setImageSource(m_object.thumbnailSource,m_object.thumbnailPath);
            }
        }

    protected:

        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button()==Qt::LeftButton)
            {
                m_pressed=true;
                event->accept();
                return;
            }
            QFrame::mousePressEvent(event);
        }

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            if (m_pressed && event->button()==Qt::LeftButton)
            {
                m_pressed=false;
                event->accept();
                if (rect().contains(event->position().toPoint()) && m_hooks.activated)
                {
                    const ChatObjectItem copy=m_object;
                    m_hooks.activated(copy);
                }
                return;
            }
            QFrame::mouseReleaseEvent(event);
        }

        void contextMenuEvent(QContextMenuEvent* event) override
        {
            if (m_hooks.contextMenu)
            {
                const ChatObjectItem copy=m_object;
                m_hooks.contextMenu(copy,event->globalPos());
                event->accept();
                return;
            }
            QFrame::contextMenuEvent(event);
        }

    private:

        ChatObjectRowHooks m_hooks;
        ChatObjectItem m_object;
        RoundedImage* m_image=nullptr;
        bool m_pressed=false;
};

//! A line of cells, newest at the left.
class ChatObjectGalleryRow : public ChatObjectGalleryRowBase
{
    public:

        ChatObjectGalleryRow(
                const std::vector<ChatObjectItem>& objects,
                int cellSize,
                int spacing,
                const ChatObjectRowHooks& hooks,
                QWidget* parent
            ) : ChatObjectGalleryRowBase(parent)
        {
            setObjectName("galleryRow");
            m_layout=Layout::horizontal(this);

            for (const auto& object : objects)
            {
                auto* cell=new ChatObjectGalleryCell(object,cellSize,hooks,this);
                m_layout->addWidget(cell);
                m_cells.push_back(cell);
            }
            // a short row keeps its cells at the left, under the full rows
            m_layout->addStretch(1);

            setGeometryParams(cellSize,spacing);
        }

        //! Gaps are layout geometry that the number of columns is computed from, so they are
        //! set here from the same numbers (the browser's QSS properties), not from the style.
        void setGeometryParams(int cellSize, int spacing) override
        {
            m_layout->setSpacing(spacing);
            m_layout->setContentsMargins(0,0,0,spacing);
            for (auto* cell : m_cells)
            {
                cell->setCellSize(cellSize);
            }
            setFixedHeight(cellSize+spacing);
        }

        bool updateObject(const ChatObjectItem& object) override
        {
            for (auto* cell : m_cells)
            {
                if (cell->object().key==object.key)
                {
                    cell->setObject(object);
                    return true;
                }
            }
            return false;
        }

    private:

        QHBoxLayout* m_layout=nullptr;
        std::vector<ChatObjectGalleryCell*> m_cells;
};

//! The heading of a month, a row of its own that spans the width.
class ChatObjectGalleryHeaderRow : public ChatObjectGalleryRowBase
{
    public:

        ChatObjectGalleryHeaderRow(const QString& title, QWidget* parent)
            : ChatObjectGalleryRowBase(parent)
        {
            setObjectName("monthRow");
            auto* layout=Layout::vertical(this);

            auto* label=new QLabel(title,this);
            label->setObjectName("monthHeader");
            // plain text: month names are not markup
            label->setTextFormat(Qt::PlainText);
            layout->addWidget(label);
        }
};

//--------------------------------------------------------------------------

//! Place of a row in the grid, newest first: the key of its first object, and heading before cells.
struct ChatObjectGalleryPos
{
    ChatObjectKey key;
    //! 0 for a month heading, 1 for a line of cells.
    int type=1;

    friend bool operator<(const ChatObjectGalleryPos& left, const ChatObjectGalleryPos& right) noexcept
    {
        if (left.key!=right.key)
        {
            return right.key<left.key;
        }
        return left.type<right.type;
    }

    friend bool operator==(const ChatObjectGalleryPos& left, const ChatObjectGalleryPos& right) noexcept
    {
        return left.type==right.type && left.key==right.key;
    }

    friend bool operator!=(const ChatObjectGalleryPos& left, const ChatObjectGalleryPos& right) noexcept
    {
        return !(left==right);
    }

    QString id() const
    {
        return (type==0 ? QStringLiteral("h") : QStringLiteral("c"))+key.toString();
    }
};

//! What the flyweight grid holds per row.
struct ChatObjectGalleryRowHandle
{
    QFrame* widget=nullptr;
    ChatObjectGalleryPos pos;
    QString id;
};

struct ChatObjectGalleryRowTraits : public FlyweightListItemTraits<ChatObjectGalleryRowHandle,QFrame,ChatObjectGalleryPos,QString>
{
    static QFrame* widget(const ChatObjectGalleryRowHandle& handle) noexcept
    {
        return handle.widget;
    }

    // By value on purpose, see ChatObjectListItemTraits.
    static ChatObjectGalleryPos sortValue(const ChatObjectGalleryRowHandle& handle) noexcept
    {
        return handle.pos;
    }

    static QString id(const ChatObjectGalleryRowHandle& handle)
    {
        return handle.id;
    }
};

using ChatObjectGalleryRowItem=FlyweightListItem<ChatObjectGalleryRowTraits>;

/**
 * @brief The grid's scrolling area: a FlyweightListView over rows built on demand from the
 *  browser's model.
 *
 * The rows are a function of the model. The loaded objects, newest first, are cut into GROUPS: one
 * per month when grouping by month, else one. A group is a heading row (month grouping only)
 * followed by lines of N cells, cut from the group's NEWEST object, so a line is full except the
 * last one of the group. A row is identified by the key of its first object, and the heading by the
 * key of the group's newest object.
 *
 * Cutting from the newest object means that objects arriving on the OLDER side never move a line.
 * Objects arriving on the newer side can: when they belong to the group of the topmost row, its
 * lines are cut anew, and the grid is rebuilt around the first visible row, keeping that row where
 * it is on the screen. A line is built only when it is full or when its group is known to end after
 * it, so a built line never has to grow.
 *
 * The grid never receives objects from anywhere but the model (through ServeFn it only asks the
 * owner for more), so it is gap-free by construction.
 */
class ChatObjectGalleryView : public QFrame
{
    private:

        //! Objects of one month among the loaded ones: items()[begin,end), ascending.
        struct Group
        {
            size_t begin=0;
            size_t end=0;
            int month=0;
        };

        //! A row by its place in the groups.
        struct RowRef
        {
            size_t group=0;
            size_t chunk=0;
            bool header=false;
            bool valid=false;
        };

        //! Where the user is, to come back to the same place after a rebuild.
        struct Focus
        {
            ChatObjectKey key;
            int type=1;
            bool valid=false;
            std::optional<int> offset;
        };

    public:

        using View=FlyweightListView<ChatObjectGalleryRowItem>;
        using ServeFn=std::function<std::vector<ChatObjectItem> (const ChatObjectKey& anchor, size_t count, bool older)>;

        //! Rows the view keeps ready beyond each edge and the size of one prefetch request.
        constexpr static const size_t PrefetchRows=5;

        //! Rows built in one go on a (re)load.
        constexpr static const size_t InitialRows=10;

        //! Rows loaded NEWER than a jump target, see ChatObjectListView::LeadingMargin.
        constexpr static const size_t LeadingRows=PrefetchRows;

        //! Width assumed before the first layout.
        constexpr static const int FallbackWidth=480;

        ChatObjectGalleryView(
                const ChatObjectBrowserModel& model,
                ChatObjectRowHooks hooks,
                ServeFn serve,
                std::function<void ()> jumpHome,
                QWidget* parent=nullptr
            ) : QFrame(parent),
                m_model(model),
                m_hooks(std::move(hooks)),
                m_serve(std::move(serve)),
                m_jumpHome(std::move(jumpHome))
        {
            setObjectName("galleryGrid");
            auto* layout=Layout::vertical(this);

            // About one screen of slack per side, see ChatObjectListView for the arithmetic.
            // Static of the class template, so it touches only this instantiation, and must be
            // set before the view is constructed.
            View::MaxHiddenRatio=2.0;

            m_view=new View(this,PrefetchRows);
            m_view->setObjectName("galleryRows");
            m_view->setOrientation(Qt::Vertical);
            m_view->setPrefetchScreensCount(1.0);
            m_view->setPrefetchThresholdRatio(0.5);
            m_view->setPrefetchItemCount(PrefetchRows);
            // HOME, not the END default of the chat, see ChatObjectListView.
            m_view->setStickMode(Direction::HOME);
            layout->addWidget(m_view);

            m_view->setRequestItemsCb(
                [this](const ChatObjectGalleryRowItem* item, size_t count, Direction direction)
                {
                    if (item==nullptr || direction==Direction::NONE)
                    {
                        return;
                    }
                    fetchRows(item->sortValue(),count,direction==Direction::END);
                }
            );
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

        int minCellSize() const noexcept
        {
            return m_minCell;
        }

        int cellSpacing() const noexcept
        {
            return m_spacing;
        }

        int columnCount() const noexcept
        {
            return m_columns;
        }

        void setGeometryParams(int minCell, int spacing)
        {
            // keep the column computation away from a division by zero
            m_minCell=std::max(1,minCell);
            m_spacing=std::max(0,spacing);
            relayout(true);
        }

        void setGrouped(bool enable)
        {
            if (m_group==enable)
            {
                return;
            }
            m_group=enable;
            if (m_view->itemCount()!=0)
            {
                rebuild(currentFocus());
            }
        }

        void reset()
        {
            m_view->clear();
            m_topGroupHead=ChatObjectKey{};
        }

        /**
         * @brief Replace the content.
         * @param items All items of the model, newest first.
         * @param focus Key to put at the top, or a null key to start at the newest item.
         */
        void load(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus)
        {
            if (items.empty())
            {
                reset();
                return;
            }

            Focus where;
            where.key=focus;
            where.valid=!focus.isNull();
            rebuild(where);
        }

        void remove()
        {
            if (m_model.empty())
            {
                reset();
                return;
            }
            // Positions shift when objects go, so the lines are cut anew. Where the user is is
            // read from the widgets, which still hold the removed objects.
            rebuild(currentFocus());
        }

        void updateObject(const ChatObjectItem& object)
        {
            m_view->eachItem(
                [&object](const ChatObjectGalleryRowItem* item)
                {
                    auto* row=static_cast<ChatObjectGalleryRowBase*>(item->widget());
                    // keep going until the row that has the object
                    return !row->updateObject(object);
                }
            );
        }

        //! The model grew on a side: take from it what the view was waiting for.
        void edgeArrived(bool older)
        {
            const auto* edge=older ? m_view->lastItem() : m_view->firstItem();
            if (edge==nullptr)
            {
                // nothing could be built at the first load (the data was shorter than a line
                // while the end was not known yet); try again with what there is now
                rebuild(Focus{});
                return;
            }
            fetchRows(edge->sortValue(),PrefetchRows,older);
        }

    protected:

        void resizeEvent(QResizeEvent* event) override
        {
            QFrame::resizeEvent(event);
            relayout(false);
        }

    private:

        //--------------------------------------------------------------
        // groups and rows
        //--------------------------------------------------------------

        void refreshGroups()
        {
            const auto& items=m_model.items();
            const bool same=m_groupsValid
                            && m_groupsSize==items.size()
                            && m_groupsGrouped==m_group
                            && (items.empty()
                                || (m_groupsOldest==items.front().key && m_groupsNewest==items.back().key));
            if (same)
            {
                return;
            }

            m_groups.clear();
            for (size_t i=0; i<items.size(); ++i)
            {
                const int month=m_group ? chatObjectMonthKey(items[i].dateTime) : 0;
                if (m_groups.empty() || m_groups.back().month!=month)
                {
                    Group group;
                    group.begin=i;
                    group.end=i+1;
                    group.month=month;
                    m_groups.push_back(group);
                }
                else
                {
                    m_groups.back().end=i+1;
                }
            }

            m_groupsValid=true;
            m_groupsSize=items.size();
            m_groupsGrouped=m_group;
            if (!items.empty())
            {
                m_groupsOldest=items.front().key;
                m_groupsNewest=items.back().key;
            }
        }

        size_t columns() const noexcept
        {
            return static_cast<size_t>(std::max(1,m_columns));
        }

        size_t groupSize(size_t group) const
        {
            return m_groups[group].end-m_groups[group].begin;
        }

        size_t chunkCount(size_t group) const
        {
            return (groupSize(group)+columns()-1)/columns();
        }

        //! Whether the group has a heading row. A group of objects without a date has none.
        bool hasHeader(size_t group) const
        {
            return m_group && m_groups[group].month>=0;
        }

        //! The newest object of the group.
        const ChatObjectItem& headItem(size_t group) const
        {
            return m_model.items()[m_groups[group].end-1];
        }

        //! The first row of the grid: the heading of the newest group, or its first line.
        RowRef firstRef() const
        {
            RowRef ref;
            if (m_groups.empty())
            {
                return ref;
            }
            ref.valid=true;
            ref.group=m_groups.size()-1;
            ref.header=hasHeader(ref.group);
            ref.chunk=0;
            return ref;
        }

        //! The last row of the grid: the last line of the oldest group.
        RowRef lastRef() const
        {
            RowRef ref;
            if (m_groups.empty())
            {
                return ref;
            }
            ref.valid=true;
            ref.group=0;
            ref.chunk=chunkCount(0)-1;
            return ref;
        }

        //! The row below: older.
        RowRef nextRef(const RowRef& ref) const
        {
            RowRef next;
            if (ref.header)
            {
                next=ref;
                next.header=false;
                next.chunk=0;
                return next;
            }
            if (ref.chunk+1<chunkCount(ref.group))
            {
                next=ref;
                ++next.chunk;
                return next;
            }
            if (ref.group==0)
            {
                return next;
            }
            next.valid=true;
            next.group=ref.group-1;
            next.chunk=0;
            next.header=hasHeader(next.group);
            return next;
        }

        //! The row above: newer.
        RowRef prevRef(const RowRef& ref) const
        {
            RowRef prev;
            if (!ref.header && ref.chunk>0)
            {
                prev=ref;
                --prev.chunk;
                return prev;
            }
            if (!ref.header && hasHeader(ref.group))
            {
                prev=ref;
                prev.header=true;
                prev.chunk=0;
                return prev;
            }
            if (ref.group+1>=m_groups.size())
            {
                return prev;
            }
            prev.valid=true;
            prev.group=ref.group+1;
            prev.chunk=chunkCount(prev.group)-1;
            prev.header=false;
            return prev;
        }

        /**
         * @brief Whether the row may be built now.
         *
         * A heading always may. A line may if it is full, or if its group is known to end after
         * it: there is an older group in the model, or the owner said the older end is reached.
         * Otherwise more objects of the group may still arrive and fill it.
         */
        bool buildable(const RowRef& ref) const
        {
            if (ref.header)
            {
                return true;
            }
            const size_t first=ref.chunk*columns();
            const size_t cells=std::min(columns(),groupSize(ref.group)-first);
            return cells==columns() || ref.group>0 || m_model.isEndOlder();
        }

        ChatObjectGalleryPos posOf(const RowRef& ref) const
        {
            ChatObjectGalleryPos pos;
            if (ref.header)
            {
                pos.key=headItem(ref.group).key;
                pos.type=0;
            }
            else
            {
                pos.key=m_model.items()[m_groups[ref.group].end-1-ref.chunk*columns()].key;
                pos.type=1;
            }
            return pos;
        }

        //! The row for a place in the view. False if the model has no such row any more.
        bool refFromPos(const ChatObjectGalleryPos& pos, RowRef& ref) const
        {
            const auto& items=m_model.items();
            const size_t idx=m_model.lowerBound(pos.key);
            if (idx>=items.size() || items[idx].key!=pos.key)
            {
                return false;
            }

            size_t group=0;
            while (group<m_groups.size() && m_groups[group].end<=idx)
            {
                ++group;
            }
            if (group>=m_groups.size())
            {
                return false;
            }

            const size_t offset=m_groups[group].end-1-idx;
            ref.valid=true;
            ref.group=group;
            if (pos.type==0)
            {
                if (!hasHeader(group) || offset!=0)
                {
                    return false;
                }
                ref.header=true;
                ref.chunk=0;
                return true;
            }
            if (offset%columns()!=0)
            {
                return false;
            }
            ref.header=false;
            ref.chunk=offset/columns();
            return true;
        }

        ChatObjectGalleryRowItem makeRow(const RowRef& ref)
        {
            ChatObjectGalleryRowHandle handle;
            handle.pos=posOf(ref);
            handle.id=handle.pos.id();

            auto* parent=m_view->itemsParentWidget();
            if (ref.header)
            {
                handle.widget=new ChatObjectGalleryHeaderRow(chatObjectMonthTitle(headItem(ref.group).dateTime),parent);
            }
            else
            {
                const auto& items=m_model.items();
                const auto& group=m_groups[ref.group];
                std::vector<ChatObjectItem> cells;
                cells.reserve(columns());
                for (size_t j=0; j<columns(); ++j)
                {
                    const size_t offset=ref.chunk*columns()+j;
                    if (offset>=groupSize(ref.group))
                    {
                        break;
                    }
                    cells.push_back(items[group.end-1-offset]);
                }
                handle.widget=new ChatObjectGalleryRow(cells,m_cellSize,m_spacing,m_hooks,parent);
            }
            return ChatObjectGalleryRowItem(handle);
        }

        //--------------------------------------------------------------
        // geometry
        //--------------------------------------------------------------

        void computeGeometry()
        {
            const int totalWidth=width()>0 ? width() : FallbackWidth;

            // The scrollbar is counted even while it is hidden: the data is nearly always taller
            // than the viewport, and a grid that re-flowed when the bar appeared would jump.
            auto* bar=m_view->verticalScrollBar();
            bar->ensurePolished();
            const auto margins=contentsMargins()+m_view->contentsMargins();

            int available=totalWidth-margins.left()-margins.right()-bar->sizeHint().width();
            available=std::max(available,m_minCell);

            m_columns=std::max(1,(available+m_spacing)/(m_minCell+m_spacing));
            m_cellSize=std::max(1,(available-(m_columns-1)*m_spacing)/m_columns);
        }

        //! Apply a changed width or changed gaps to what is built.
        void relayout(bool force)
        {
            if (m_columns==0 || m_view->itemCount()==0)
            {
                // nothing is built yet; the first load computes the geometry
                return;
            }

            const int oldColumns=m_columns;
            const int oldCell=m_cellSize;
            computeGeometry();

            if (m_columns!=oldColumns)
            {
                rebuild(currentFocus());
                return;
            }
            if (force || m_cellSize!=oldCell)
            {
                m_view->eachItem(
                    [this](const ChatObjectGalleryRowItem* item)
                    {
                        static_cast<ChatObjectGalleryRowBase*>(item->widget())->setGeometryParams(m_cellSize,m_spacing);
                        return true;
                    }
                );
            }
        }

        //--------------------------------------------------------------
        // building
        //--------------------------------------------------------------

        //! Where the user is: the first visible row and how far it is from the top of the view.
        Focus currentFocus() const
        {
            Focus focus;
            const auto* item=m_view->firstViewportItem();
            if (item==nullptr)
            {
                item=m_view->firstItem();
            }
            if (item==nullptr)
            {
                return focus;
            }

            const auto pos=item->sortValue();
            focus.valid=true;
            focus.key=pos.key;
            focus.type=pos.type;
            focus.offset=m_view->itemViewOffset(item->id());
            return focus;
        }

        /**
         * @brief Build the view from scratch, from the model.
         * @param focus Row to bring back: with an offset to the same place on the screen, without
         *  one to the top. An invalid focus starts at the newest row.
         */
        void rebuild(const Focus& focus)
        {
            if (m_model.empty())
            {
                reset();
                return;
            }

            computeGeometry();
            refreshGroups();

            RowRef target;
            RowRef start=firstRef();
            if (focus.valid)
            {
                const auto& items=m_model.items();
                // a key that is gone maps to the object next to it
                const size_t idx=std::min(m_model.lowerBound(focus.key),items.size()-1);

                size_t group=0;
                while (group+1<m_groups.size() && m_groups[group].end<=idx)
                {
                    ++group;
                }
                const size_t offset=m_groups[group].end-1-idx;

                target.valid=true;
                target.group=group;
                target.chunk=offset/columns();
                target.header=(focus.type==0 && hasHeader(group) && offset==0);

                // rows above the target, so that it does not land flush against the begin edge
                start=target;
                for (size_t i=0; i<LeadingRows; ++i)
                {
                    const auto prev=prevRef(start);
                    if (!prev.valid)
                    {
                        break;
                    }
                    start=prev;
                }
            }

            std::vector<ChatObjectGalleryRowItem> rows;
            for (RowRef ref=start; ref.valid && rows.size()<InitialRows && buildable(ref); ref=nextRef(ref))
            {
                rows.push_back(makeRow(ref));
            }

            if (rows.empty())
            {
                m_view->clear();
                // the data is shorter than a line and its end is not known: ask for more
                askForMore(true);
                askForMore(false);
                return;
            }

            // loadItems() clears the view, and clearing resets the "end reached" markers --
            // they are set AFTER it, never before.
            m_view->loadItems(rows);
            noteTop();
            pinEdges();

            if (target.valid)
            {
                const auto id=posOf(target).id();
                if (m_view->hasItem(id))
                {
                    if (focus.offset)
                    {
                        m_view->scrollToItem(id,*focus.offset);
                    }
                    else
                    {
                        m_view->scrollToItemEdge(id,Direction::HOME);
                    }
                }
            }
        }

        //! Ask the owner for more on a side, if that side is not known to be exhausted.
        void askForMore(bool older)
        {
            const bool exhausted=older ? m_model.isEndOlder() : m_model.isEndNewer();
            if (exhausted || m_model.empty())
            {
                return;
            }
            const auto* edge=older ? m_model.oldest() : m_model.newest();
            // Nothing is older than the oldest (newer than the newest) object, so this returns
            // nothing; its point is the request to the owner it makes on the way.
            m_serve(edge->key,1,older);
        }

        /**
         * @brief Remember the newest object of the group of the topmost row.
         *
         * The lines of a group are cut from that object, so when a newer one appears the lines in
         * the view are out of date.
         */
        void noteTop()
        {
            m_topGroupHead=ChatObjectKey{};
            const auto* first=m_view->firstItem();
            RowRef ref;
            if (first!=nullptr && refFromPos(first->sortValue(),ref))
            {
                m_topGroupHead=headItem(ref.group).key;
            }
        }

        /**
         * @brief Tell the view which of its ends are the real ends of the data.
         *
         * Without it the view believes there is always more and keeps asking. Set only when the
         * owner has said the side is exhausted AND the view's edge row is the model's edge row.
         */
        void pinEdges()
        {
            refreshGroups();
            const auto* first=m_view->firstItem();
            const auto* last=m_view->lastItem();
            if (first==nullptr || last==nullptr || m_groups.empty())
            {
                return;
            }

            if (m_model.isEndNewer() && first->sortValue()==posOf(firstRef()))
            {
                m_view->setMinSortValue(first->sortValue());
            }
            if (m_model.isEndOlder() && last->sortValue()==posOf(lastRef()))
            {
                m_view->setMaxSortValue(last->sortValue());
            }
        }

        void fetchRows(const ChatObjectGalleryPos& edge, size_t count, bool older)
        {
            refreshGroups();

            RowRef current;
            if (!refFromPos(edge,current))
            {
                // the model changed under the view (a removal, a regrouping)
                rebuild(currentFocus());
                return;
            }
            if (!older && headItem(current.group).key!=m_topGroupHead)
            {
                // newer objects of the topmost group arrived: its lines are cut anew
                rebuild(currentFocus());
                return;
            }

            std::vector<ChatObjectGalleryRowItem> rows;
            bool stopped=false;
            while (rows.size()<count)
            {
                const RowRef next=older ? nextRef(current) : prevRef(current);
                if (!next.valid || !buildable(next))
                {
                    stopped=true;
                    break;
                }
                rows.push_back(makeRow(next));
                current=next;
            }
            if (!older)
            {
                // insertContinuousItems() wants the view's order
                std::reverse(rows.begin(),rows.end());
            }

            if (stopped)
            {
                askForMore(older);
            }

            if (rows.empty())
            {
                pinEdges();
                return;
            }

            m_view->beginUpdate();
            m_view->insertContinuousItems(rows,false);
            // before endUpdate(): that is what schedules the check that reads the markers
            pinEdges();
            m_view->endUpdate();

            if (!older)
            {
                noteTop();
            }
        }

        const ChatObjectBrowserModel& m_model;
        ChatObjectRowHooks m_hooks;
        ServeFn m_serve;
        std::function<void ()> m_jumpHome;
        View* m_view=nullptr;

        bool m_group=true;
        std::vector<Group> m_groups;
        bool m_groupsValid=false;
        bool m_groupsGrouped=true;
        size_t m_groupsSize=0;
        ChatObjectKey m_groupsOldest;
        ChatObjectKey m_groupsNewest;
        ChatObjectKey m_topGroupHead;

        int m_minCell=ChatObjectGalleryBrowser::DefaultMinCellSize;
        int m_spacing=ChatObjectGalleryBrowser::DefaultCellSpacing;
        int m_columns=0;
        int m_cellSize=0;
};

//--------------------------------------------------------------------------

ChatObjectGalleryBrowser::ChatObjectGalleryBrowser(QString kind, QWidget* parent)
    : AbstractChatObjectBrowser(std::move(kind),parent),
      m_gallery(nullptr)
{
    setObjectName("chatObjectGalleryBrowser");

    m_gallery=new ChatObjectGalleryView(
        model(),
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
    m_gallery->setGrouped(isGroupByMonth());
    setView(m_gallery);
}

//--------------------------------------------------------------------------

int ChatObjectGalleryBrowser::minCellSize() const noexcept
{
    return m_gallery->minCellSize();
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::setMinCellSize(int value)
{
    m_gallery->setGeometryParams(value,m_gallery->cellSpacing());
}

//--------------------------------------------------------------------------

int ChatObjectGalleryBrowser::cellSpacing() const noexcept
{
    return m_gallery->cellSpacing();
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::setCellSpacing(int value)
{
    m_gallery->setGeometryParams(m_gallery->minCellSize(),value);
}

//--------------------------------------------------------------------------

int ChatObjectGalleryBrowser::columnCount() const noexcept
{
    return m_gallery->columnCount();
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doReset()
{
    m_gallery->reset();
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus)
{
    m_gallery->load(items,focus);
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doRemove(const std::vector<ChatObjectKey>& keys)
{
    Q_UNUSED(keys)
    m_gallery->remove();
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doUpdate(const ChatObjectItem& item)
{
    m_gallery->updateObject(item);
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doEdgeArrived(bool older)
{
    m_gallery->edgeArrived(older);
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::doGroupingChanged()
{
    // the headings come and go and the lines are cut anew, the user stays where they are
    m_gallery->setGrouped(isGroupByMonth());
}

//--------------------------------------------------------------------------

void ChatObjectGalleryBrowser::jumpToNewest()
{
    if (model().isEndNewer() && !model().empty())
    {
        // the newest end is known and loaded: no need to ask the owner again
        const std::vector<ChatObjectItem> viewItems(model().items().rbegin(),model().items().rend());
        m_gallery->load(viewItems,ChatObjectKey{});
        return;
    }
    reload();
}

//--------------------------------------------------------------------------

ChatObjectGalleryBrowser* makeChatObjectImageBrowser(QWidget* parent)
{
    return new ChatObjectGalleryBrowser(ChatObjectKind::Image,parent);
}

} // namespace uise
