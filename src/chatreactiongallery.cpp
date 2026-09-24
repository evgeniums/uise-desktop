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

/** @file uise/desktop/src/chatreactiongallery.cpp
*
*  Defines ChatReactionQuickBar, ChatReactionGallery and ChatReactionGalleryDropdown.
*
*/

/****************************************************************************/

#include <algorithm>
#include <cstddef>
#include <iostream>

#include <QLabel>
#include <QBoxLayout>
#include <QEvent>
#include <QShowEvent>

#include <uise/desktop/chatreactiongallery.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/searchlineedit.hpp>
#include <uise/desktop/reactioniconpack.hpp>
// The expanded gallery is virtualized over these -- see ChatReactionGalleryGrid below. The .ipp
// is included, not just the declaration header, because this file INSTANTIATES the template (the
// convention every other in-tree instantiation site follows, e.g. htreeflyweightlistitem.hpp).
#include <uise/desktop/flyweightlistitem.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

namespace {

/**
 * @brief Gap between adjacent icon cells, shared by the quick bar (the collapsed row, and the
 *  gallery's own "recent" row) and by every row of the expanded grid.
 *
 * One constant rather than a number at each site because the two are read side by side: the
 * recent row sits directly above the grid in the expanded gallery, so any difference between
 * them shows up as the grid looking more tightly packed than the row heading it. Not a QSS
 * property: QLayout spacing is not stylable, and the cells' own box model (icon-size, padding,
 * border) already is -- see chatreactions.qss's #quickButton/#galleryButton rules.
 */
constexpr const int ReactionCellSpacing=4;

/**
 * @brief Compose a gallery cell's hover tooltip: just ":shortcode:".
 *
 * Originally two lines (shortcode + description), dropped to one: the description is almost
 * always the same word or a close paraphrase of the shortcode itself ("star" / ":star:"), so the
 * second line was rarely telling the user anything the first had not already. The shortcode
 * alone is also the more ACTIONABLE half -- it is literally what to type -- so keeping it and
 * dropping the description is not a loss even where the two do differ.
 *
 * Falls back to the (still available) description only for an entry with no shortcode at all,
 * which nothing in the shipped pack has today but a future host pack might.
 */
QString reactionTooltipText(const ReactionIconInfo* info)
{
    if (info==nullptr)
    {
        return QString{};
    }
    if (info->shortcode.isEmpty())
    {
        return info->description;
    }
    return QStringLiteral(":%1:").arg(info->shortcode);
}

} // anonymous namespace

//--------------------------------------------------------------------------

ChatReactionQuickBar::ChatReactionQuickBar(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatReactionQuickBar");

    auto* layout=Layout::horizontal(this);
    layout->setSpacing(ReactionCellSpacing);

    m_expandButton=new PushButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ChatReactions::expand"),this),
        this
    );
    m_expandButton->setObjectName("expandButton");
    connect(m_expandButton,&PushButton::clicked,this,&ChatReactionQuickBar::expandRequested);
    layout->addWidget(m_expandButton);
    // Moved to its final (trailing) position once the icon buttons are built -- see rebuild().
}

//--------------------------------------------------------------------------

ChatReactionQuickBar::~ChatReactionQuickBar()
{
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_pack=std::move(pack);
    rebuild();
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setOwnReactionIds(QStringList ids)
{
    m_ownReactionIds=std::move(ids);
    for (auto* button : m_buttons)
    {
        auto id=button->property("reactionId").toString();
        button->setChecked(m_ownReactionIds.contains(id));
    }
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setLeadingIconIds(QStringList ids)
{
    if (m_leadingIconIds==ids)
    {
        // rebuild() destroys and recreates every button, so an unchanged list must not reach it:
        // the recents row is re-pushed on every gallery open, and rebuilding under the pointer
        // would drop the hover state (and, with it, a click already in progress).
        return;
    }
    m_leadingIconIds=std::move(ids);
    rebuild();
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setChevronVisible(bool enable)
{
    m_expandButton->setVisible(enable);
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::changeEvent(QEvent* event)
{
    Frame::changeEvent(event);
    if (event->type()==QEvent::LanguageChange && m_pack)
    {
        m_pack->retranslate();
        rebuild();
    }
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::rebuild()
{
    for (auto* button : m_buttons)
    {
        destroyWidget(button);
    }
    m_buttons.clear();

    if (!m_pack)
    {
        return;
    }

    auto* layout=static_cast<QHBoxLayout*>(this->layout());
    // Drop the trailing stretch/expand button from the layout without destroying them -- they
    // are re-added below, after the fresh icon buttons.
    layout->removeWidget(m_expandButton);

    // Leading ids first, then the pack's own basics -- PREPENDED, never a replacement, so the row
    // keeps its default icons and a single pick cannot collapse it to one button. See
    // setLeadingIconIds(). Deduped, because a picked emoji is very often one of the basics
    // already; capped, so the row cannot outgrow the grid below it.
    std::vector<QString> iconIds;
    iconIds.reserve(static_cast<size_t>(MaxRowIcons));
    auto appendIconId=[&iconIds](const QString& id)
    {
        if (static_cast<int>(iconIds.size())>=MaxRowIcons
            || std::find(iconIds.begin(),iconIds.end(),id)!=iconIds.end())
        {
            return;
        }
        iconIds.push_back(id);
    };
    for (const auto& id : m_leadingIconIds)
    {
        appendIconId(id);
    }
    for (const auto& id : m_pack->basicIconIds())
    {
        appendIconId(id);
    }

    for (const auto& iconId : iconIds)
    {
        const auto* info=m_pack->find(iconId);
        if (info==nullptr)
        {
            continue;
        }

        auto* button=new PushButton(info->icon,this);
        button->setObjectName("quickButton");
        button->setProperty("reactionId",iconId);
        // Also covers ChatReactionGallery's own recents row -- m_recentBar there IS a
        // ChatReactionQuickBar, not a separate widget with its own tooltip site.
        const auto tip=reactionTooltipText(info);
        if (!tip.isEmpty())
        {
            button->setToolTip(tip);
        }
        button->setCheckable(true);
        button->setChecked(m_ownReactionIds.contains(iconId));
        connect(button,&PushButton::clicked,this,
                [this,iconId]()
                {
                    Q_EMIT reactionPicked(iconId);
                });

        layout->addWidget(button);
        m_buttons.push_back(button);
    }

    layout->addWidget(m_expandButton);
}

//--------------------------------------------------------------------------
// The virtualized expanded gallery: a FlyweightListView whose ITEMS ARE ROWS.
//
// See ChatReactionGallery's own class doc comment for why the icons themselves are not the
// items (FlyweightListView is 1-D) and why a section header lives INSIDE the first row of its
// category rather than being an item of its own.
//
// Everything here is deliberately file-local in the sense that matters: nothing outside this
// file names any of it except through the opaque ChatReactionGalleryGrid* forward declaration in
// the header, which is what keeps the flyweight templates (and their .ipp) out of every
// translation unit that merely uses a gallery. Not put in an anonymous namespace only because
// ChatReactionGalleryGrid IS named in the header and so needs external linkage.
//--------------------------------------------------------------------------

//! One row's worth of PLAN -- which icons it shows, and whether it opens a category. Holds no
//! widgets and resolves no icons, so keeping the plan for the whole pack costs nothing and can
//! be recomputed on every keystroke.
struct ChatReactionGalleryRowPlan
{
    //! Non-empty only for the FIRST row of a category; that row draws it as a header above its
    //! icons. Empty on every other row, including every row in SEARCH mode.
    QString headerTitle;

    //! Indices into the pack's at(), at most ChatReactionGallery::galleryColumns() of them.
    std::vector<size_t> iconIndices;
};

/**
 * @brief One row widget: an optional section header, then a horizontal strip of icon buttons.
 *
 * No Q_OBJECT, deliberately: a Q_OBJECT class defined in a .cpp needs its own moc include, which
 * this project avoids (see the demo helpers' own "needs its own header" rule). Nothing here has
 * to emit -- a pick is reported through the std::function the grid hands down, and the buttons'
 * own clicked() connections take this row as their context object, which works for any
 * QObject-derived class whether or not it declares signals of its own.
 */
class ChatReactionGalleryRow : public QFrame
{
    public:

        using PickHandler=std::function<void (const QString&)>;

        ChatReactionGalleryRow(
                size_t index,
                const ChatReactionGalleryRowPlan& plan,
                AbstractReactionIconPack* pack,
                const QStringList& ownReactionIds,
                PickHandler pick,
                QWidget* parent=nullptr
            ) : QFrame(parent),
                m_index(index)
        {
            setObjectName("galleryRow");
            auto* layout=Layout::vertical(this);

            if (!plan.headerTitle.isEmpty())
            {
                auto* header=new QLabel(plan.headerTitle,this);
                header->setObjectName("sectionHeader");
                // Marks the very first row of the whole view, whose header sits directly under
                // the tab strip and wants no extra top margin -- see chatreactions.qss's
                // #sectionHeader[firstSection="true"] rule. Set before the row is ever shown, so
                // the initial polish already sees it.
                header->setProperty("firstSection",index==0);
                layout->addWidget(header);
            }

            auto* cellsFrame=new QFrame(this);
            cellsFrame->setObjectName("rowCells");
            auto* cellsLayout=Layout::horizontal(cellsFrame);
            // Layout::horizontal() resets spacing to 0; the quick bar directly above this grid
            // uses ReactionCellSpacing, and the two rows are read side by side, so a zero-spacing
            // grid looked noticeably tighter than the recent row heading it.
            cellsLayout->setSpacing(ReactionCellSpacing);

            for (auto iconIndex : plan.iconIndices)
            {
                const auto* info=(pack!=nullptr) ? pack->at(iconIndex) : nullptr;
                if (info==nullptr)
                {
                    // EMOJI-DEBUG: temporary diagnostic for the intermittent blank-emoji-icon
                    // bug -- remove once the root cause is confirmed.
                    std::cerr << "EMOJI-DEBUG ChatReactionGalleryRow: no info for iconIndex="
                               << iconIndex << " row=" << index
                               << " pack=" << (pack!=nullptr ? "set" : "null") << std::endl;
                    continue;
                }
                if (!info->icon)
                {
                    std::cerr << "EMOJI-DEBUG ChatReactionGalleryRow: null icon for iconId="
                               << info->iconId.toStdString() << " row=" << index << std::endl;
                }

                // Same objectName the gallery's cells have always used, so chatreactions.qss's
                // "#galleryButton QPushButton" rule styles these identically without a change.
                auto* cell=new PushButton(info->icon,cellsFrame);
                cell->setObjectName("galleryButton");
                cell->setCheckable(true);
                cell->setToolTip(reactionTooltipText(info));
                cell->setProperty("reactionId",info->iconId);
                cell->setChecked(ownReactionIds.contains(info->iconId));
                const auto iconId=info->iconId;
                connect(cell,&PushButton::clicked,this,
                        [pick,iconId]()
                        {
                            if (pick)
                            {
                                pick(iconId);
                            }
                        });
                cellsLayout->addWidget(cell);
                m_cells.push_back(cell);
            }

            // A short last row (the tail of a category) keeps its icons left-aligned under the
            // full rows above it instead of spreading across the width.
            cellsLayout->addStretch(1);
            layout->addWidget(cellsFrame);
        }

        //! Both the sort value and the id -- see ChatReactionGalleryRowTraits. Row order IS plan
        //! order, and a plan index is unique within the plan, so one number serves as both.
        size_t index() const noexcept
        {
            return m_index;
        }

        //! Whether this row draws a section header, i.e. is the first row of its category. Read
        //! by the grid when it looks for a row to measure a TYPICAL row height from.
        bool hasHeader() const noexcept
        {
            return layout()!=nullptr && layout()->count()>1;
        }

        void setOwnReactionIds(const QStringList& ids)
        {
            for (auto* cell : m_cells)
            {
                cell->setChecked(ids.contains(cell->property("reactionId").toString()));
            }
        }

    private:

        size_t m_index;
        std::vector<PushButton*> m_cells;
};

struct ChatReactionGalleryRowTraits : public FlyweightListItemTraits<ChatReactionGalleryRow*,QFrame,size_t,size_t>
{
    static size_t sortValue(const ChatReactionGalleryRow* item) noexcept
    {
        return item->index();
    }

    static QFrame* widget(ChatReactionGalleryRow* item) noexcept
    {
        return item;
    }

    static size_t id(const ChatReactionGalleryRow* item)
    {
        return item->index();
    }
};

using ChatReactionGalleryRowItem=FlyweightListItem<ChatReactionGalleryRowTraits>;

/**
 * @brief The expanded gallery's scrolling area: a FlyweightListView over row widgets built on
 *  demand from a plan.
 *
 * Only the rows near the viewport exist as widgets. Everything else is a
 * ChatReactionGalleryRowPlan entry -- a header string and a handful of pack indices -- which is
 * what makes a ~1374-icon pack cost the same to show as a 54-icon one.
 */
class ChatReactionGalleryGrid : public QFrame
{
    public:

        using PickHandler=std::function<void (const QString&)>;

        /**
         * @brief Rows built in one go when the plan is (re)loaded or jumped to.
         *
         * Deliberately small, and the single biggest lever on how long OPENING the gallery
         * takes: every row built resolves up to galleryColumns() icons, and resolving an icon
         * parses an SVG (see DefaultReactionIconPack::Pimpl::resolveIcon()), so this number
         * times the column count is the SVG parse count an open pays on the GUI thread. A
         * gallery shows ~5-6 rows, so this covers the viewport with a little to spare and the
         * view's own prefetch -- which, unlike this constant, knows the real viewport size --
         * takes it from there.
         */
        constexpr static const size_t InitialRowWindow=8;

        //! Rows the view keeps ready beyond each edge, and the size of one prefetch request. See
        //! the prefetch tuning block in the constructor for the arithmetic this feeds.
        constexpr static const size_t PrefetchRowWindow=6;

        /**
         * @brief Rows loaded ABOVE a jump target, so a jump does not land flush against the
         *  begin edge.
         *
         * Load-bearing, not cosmetic. Landing exactly at the begin edge with rows still
         * unfetched above is a trap: the view sees hiddenBefore==0 with more rows available, so
         * it prepends a batch, and FlyweightListView_p::compensateSizeChange() then hits its
         * "m_atBegin && stick==HOME" branch and scrollToEdge(HOME)s onto the row it just
         * prepended. That repeats, and the jump crawls all the way back to row 0 -- which looks
         * exactly like "it jumps to the group and then returns to Most common".
         *
         * Keeping a margin above the target means hiddenBefore starts above the prefetch
         * threshold AND m_atBegin is false, so neither half of that loop can start. Sized to the
         * prefetch window so it always clears the threshold (which is a fraction of that window).
         */
        constexpr static const size_t LeadingRowMargin=PrefetchRowWindow;

        explicit ChatReactionGalleryGrid(QWidget* parent=nullptr)
            : QFrame(parent)
        {
            setObjectName("galleryGrid");
            auto* layout=Layout::vertical(this);

            // --- prefetch tuning: the stock defaults are wrong for THESE rows ------------------
            //
            // Everything the view prefetches and evicts scales off prefetchItemWindow(), and with
            // the defaults that works out, per side, to:
            //
            //   window    = max(PrefetchItemWindowHint=20, round(visible * ScreensCount=2)) = 20
            //   threshold = window * ThresholdRatio(0.75) = 15   <- keep >=15 hidden rows ready
            //   request   = window                        = 20   <- rows asked for per request
            //   maxHidden = window * MaxHiddenRatio(5)    = 100  <- evict only past 100 hidden
            //
            // i.e. a steady state of up to ~100 + visible + ~100 = over 200 ROWS resident, which
            // at galleryColumns() icons each is ~1800 icon widgets -- MORE than the whole pack,
            // and worse than the unvirtualized version this replaced. Those numbers are sane for
            // a chat, where a row is some text; they are not sane here, where every row resolves
            // up to nine icons and resolving one parses an SVG.
            //
            // Retuned to about one screen of slack per side: ~6 hidden rows ready, requests of
            // ~6, eviction past ~12. MaxHiddenRatio is an inline static on the class TEMPLATE, so
            // assigning it touches only this instantiation -- ChatMessagesView instantiates
            // FlyweightListView over its own item type and keeps its own copy of these statics --
            // and it must be set BEFORE the view is constructed, because the pimpl copies it into
            // m_maxHiddenRatio in its own constructor.
            FlyweightListView<ChatReactionGalleryRowItem>::MaxHiddenRatio=2.0;

            m_view=new FlyweightListView<ChatReactionGalleryRowItem>(this,PrefetchRowWindow);
            m_view->setObjectName("galleryRows");
            m_view->setOrientation(Qt::Vertical);
            // One screen, not two: a screen of rows here is a screen of SVG parses.
            m_view->setPrefetchScreensCount(1.0);
            // Start refilling when half a window of hidden rows is left, rather than 3/4 -- with
            // a window this small the difference is a row or two, and refilling later means
            // fewer, larger bursts of icon resolution rather than a trickle on every scroll tick.
            m_view->setPrefetchThresholdRatio(0.5);
            // Explicit, rather than letting it default to the whole window: this is the number of
            // rows ONE request builds, and it is the single number that decides how much work a
            // scroll that crosses the threshold does at once.
            m_view->setPrefetchItemCount(PrefetchRowWindow);
            // HOME, not the Direction::END default. That default is the CHAT one -- a message
            // list wants to sit on the newest item at the bottom -- and it is actively wrong
            // here: sticking to the end means the viewport is at the end, being at the end
            // requests the rows past it, inserting those sticks to the NEW end, and the view
            // walks itself all the way down loading the entire plan, which is the exact opposite
            // of what virtualizing it was for. A gallery opens at its first row and stays there
            // until the user scrolls.
            m_view->setStickMode(Direction::HOME);
            layout->addWidget(m_view);

            m_view->setRequestItemsCb(
                [this](const ChatReactionGalleryRowItem* item, size_t count, Direction direction)
                {
                    onItemsRequested(item,count,direction);
                }
            );

            // The jump-edge control ("back to the top" button the view floats over itself once
            // enough rows are hidden above the viewport). It points HOME because setStickMode()
            // above points it there, and clicking it lands in
            // FlyweightListView_p::jumpToEdge(Direction::HOME), which handles the easy case
            // ITSELF -- if row 0 is still among the loaded rows it simply scrolls there. It only
            // calls out to this callback for the case it cannot handle alone: row 0 long since
            // evicted, so there is nothing in the view to scroll TO and the rows around the top
            // have to be built again. Without this the button was silently dead exactly when a
            // user most wants it, after scrolling a long way down.
            //
            // No matching end callback: the control is HOME-directed here, and jumpToEdge() only
            // dispatches END when the icon points down/right, which it never does under this
            // stick mode.
            m_view->setRequestHomeCb(
                [this](bool forceLongJump, Qt::KeyboardModifiers modifiers)
                {
                    Q_UNUSED(forceLongJump)
                    Q_UNUSED(modifiers)
                    // Same path a category tab jump takes -- rebuild the window at the target
                    // row and put it at the top. reloadAround() does the scrolling itself.
                    reloadAround(0);
                }
            );
        }

        void setPickHandler(PickHandler pick)
        {
            m_pick=std::move(pick);
        }

        //! Stored only -- rows resolve their icons through it as they are built, so a pack swap
        //! takes effect on the next setPlan(), which the gallery always follows it with.
        void setPack(std::shared_ptr<AbstractReactionIconPack> pack)
        {
            m_pack=std::move(pack);
        }

        void setOwnReactionIds(QStringList ids)
        {
            m_ownReactionIds=std::move(ids);
            // Only the rows that currently EXIST need updating; every row built later picks the
            // new list up at construction. eachItem() walks exactly the live ones.
            m_view->eachItem(
                [this](const ChatReactionGalleryRowItem* item)
                {
                    item->item()->setOwnReactionIds(m_ownReactionIds);
                    return true;
                }
            );
        }

        //! Replace the plan and reload the view from its first row.
        void setPlan(std::vector<ChatReactionGalleryRowPlan> plan)
        {
            m_plan=std::move(plan);
            reloadAround(0);
        }

        size_t rowCount() const noexcept
        {
            return m_plan.size();
        }

        //! Put the given row at the top of the viewport, reloading around it when it is not
        //! currently built with enough rows above it to jump to safely.
        void scrollToRow(size_t row)
        {
            if (row>=m_plan.size())
            {
                return;
            }

            // The cheap path needs BOTH that the row is loaded and that enough rows are loaded
            // ABOVE it -- see LeadingRowMargin for why scrolling flush against the begin edge
            // with more rows available above starts a prepend/stick loop back to row 0. Row 0
            // itself is exempt: there is nothing above it to fetch, so the loop cannot start.
            const auto* first=m_view->firstItem();
            const auto hasMarginAbove=(row==0)
                || (first!=nullptr && first->sortValue()+LeadingRowMargin<=row);
            if (m_view->hasItem(row) && hasMarginAbove)
            {
                m_view->scrollToItemEdge(row,Direction::HOME);
                return;
            }
            reloadAround(row);
        }

        /**
         * @brief Height of one TYPICAL (header-less) row, for the gallery's own visible-rows
         *  clamp, or 0 when nothing is built yet to measure.
         *
         * Prefers a row WITHOUT a header: in BROWSE mode the very first row always has one, and
         * clamping "N visible rows" to N header-carrying rows would make the gallery noticeably
         * taller than the N rows of icons it is meant to show.
         */
        int measuredRowHeight() const
        {
            int fallback=0;
            int plain=0;
            m_view->eachItem(
                [&fallback,&plain](const ChatReactionGalleryRowItem* item)
                {
                    const auto* row=item->item();
                    const auto height=row->sizeHint().height();
                    if (height<=0)
                    {
                        return true;
                    }
                    if (fallback==0)
                    {
                        fallback=height;
                    }
                    if (!row->hasHeader())
                    {
                        plain=height;
                        return false;
                    }
                    return true;
                }
            );
            return plain>0 ? plain : fallback;
        }

    private:

        //! Rebuild the loaded window around `targetRow` and put that row at the top of the
        //! viewport, with LeadingRowMargin rows kept loaded above it.
        void reloadAround(size_t targetRow)
        {
            m_view->clear();
            if (targetRow>=m_plan.size())
            {
                return;
            }

            // BEFORE loadItems(), as setMaxSortValue()'s own doc comment requires -- and
            // load-bearing for far more than sorting. checkItemCount()'s fetch guards read
            //
            //     canFetchBefore = first && (!m_minSortValueSet || cmp(m_minSortValue,first))
            //     canFetchAfter  = last  && (!m_maxSortValueSet || cmp(last,m_maxSortValue))
            //
            // so a view that has never been TOLD the range has both flags false, both guards
            // unconditionally true, and therefore believes there is always more to fetch in both
            // directions. It then keeps re-arming its prefetch batch (m_currentBatchCount) and
            // requesting, and since a request past either end of the plan returns nothing, the
            // "not enough hidden items" condition never clears -- so it walks the whole pack in
            // rather than stopping at the end. Row indices ARE the sort values, so the bounds are
            // simply the first and last plan index.
            m_view->setMinSortValue(0);
            m_view->setMaxSortValue(m_plan.size()-1);

            // A margin of rows ABOVE the target, so the jump does not land flush against the
            // begin edge -- see LeadingRowMargin for the prepend/stick loop that causes. Clamped
            // at row 0, where there is nothing above to load and no loop to avoid.
            const auto before=std::min(targetRow,LeadingRowMargin);
            const auto firstRow=targetRow-before;
            const auto last=std::min(m_plan.size(),targetRow+InitialRowWindow);

            std::vector<ChatReactionGalleryRowItem> items;
            items.reserve(last-firstRow);
            for (size_t i=firstRow; i<last; ++i)
            {
                items.emplace_back(makeRow(i));
            }
            m_view->loadItems(items);

            // scrollToItemEdge(target), NOT scrollToEdge(HOME): those coincide only when the
            // target IS the first loaded row, which is exactly the case the margin above exists
            // to avoid. Explicit rather than left to the stick mode either way -- sticking only
            // acts when the viewport already sits on an edge, so relying on it would leave a jump
            // wherever the previous window happened to be scrolled.
            m_view->scrollToItemEdge(targetRow,Direction::HOME);
        }

        void onItemsRequested(const ChatReactionGalleryRowItem* item, size_t count, Direction direction)
        {
            if (m_plan.empty())
            {
                return;
            }

            std::vector<ChatReactionGalleryRowItem> items;
            if (item==nullptr)
            {
                const auto last=std::min(m_plan.size(),count);
                for (size_t i=0; i<last; ++i)
                {
                    items.emplace_back(makeRow(i));
                }
            }
            else if (direction==Direction::END)
            {
                const auto from=item->sortValue()+1;
                const auto last=std::min(m_plan.size(),from+count);
                for (size_t i=from; i<last; ++i)
                {
                    items.emplace_back(makeRow(i));
                }
            }
            else
            {
                const auto idx=item->sortValue();
                const auto span=std::min(count,idx);
                for (size_t i=idx-span; i<idx; ++i)
                {
                    items.emplace_back(makeRow(i));
                }
            }

            if (!items.empty())
            {
                m_view->insertContinuousItems(items);
            }
        }

        ChatReactionGalleryRowItem makeRow(size_t index)
        {
            if (!m_pack)
            {
                // EMOJI-DEBUG: temporary diagnostic for the intermittent blank-emoji-icon bug --
                // catches a row built while m_pack has been swapped out from under an in-flight
                // prefetch request (a claim between two composers, or a mode change). Remove once
                // the root cause is confirmed.
                std::cerr << "EMOJI-DEBUG ChatReactionGalleryGrid::makeRow: m_pack is null, row="
                           << index << std::endl;
            }
            return ChatReactionGalleryRowItem(
                new ChatReactionGalleryRow(index,m_plan[index],m_pack.get(),m_ownReactionIds,m_pick)
            );
        }

        FlyweightListView<ChatReactionGalleryRowItem>* m_view;
        std::vector<ChatReactionGalleryRowPlan> m_plan;
        std::shared_ptr<AbstractReactionIconPack> m_pack;
        QStringList m_ownReactionIds;
        PickHandler m_pick;
};

//--------------------------------------------------------------------------

ChatReactionGallery::ChatReactionGallery(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatReactionGallery");

    auto* layout=Layout::vertical(this);

    m_recentFrame=new QFrame(this);
    m_recentFrame->setObjectName("recentFrame");
    auto* recentLayout=Layout::vertical(m_recentFrame);

    m_recentTitle=new QLabel(tr("Recently used"),m_recentFrame);
    m_recentTitle->setObjectName("recentTitle");
    recentLayout->addWidget(m_recentTitle);

    m_recentBar=new ChatReactionQuickBar(m_recentFrame);
    m_recentBar->setObjectName("recentBar");
    m_recentBar->setChevronVisible(false);
    connect(m_recentBar,&ChatReactionQuickBar::reactionPicked,this,&ChatReactionGallery::reactionPicked);
    recentLayout->addWidget(m_recentBar);

    layout->addWidget(m_recentFrame);

    m_searchEdit=new SearchLineEdit(this);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(tr("Search reactions"));
    // The gallery's own dismissal (via the chained DropdownFrame it lives in) must win over the
    // search box's own Qt::WidgetShortcut Escape -- see SearchLineEdit::setCancelShortcutEnabled()'s
    // own doc comment. Without this, pressing Escape while the search box has focus would only
    // clear the search text instead of closing the gallery.
    m_searchEdit->setCancelShortcutEnabled(false);
    connect(m_searchEdit,&QLineEdit::textChanged,this,&ChatReactionGallery::onSearchTextChanged);
    layout->addWidget(m_searchEdit);

    // Category tab strip -- a sibling of the view, not inside it, so it stays pinned above the
    // rows regardless of scroll position. Hidden by rebuildCategoryTabs() when the pack offers
    // no categories() (every pack did, before categories existed).
    m_categoryTabs=new QFrame(this);
    m_categoryTabs->setObjectName("categoryTabs");
    Layout::horizontal(m_categoryTabs);
    m_categoryTabs->setVisible(false);
    layout->addWidget(m_categoryTabs);

    m_emptyLabel=new QLabel(tr("No matching reactions"),this);
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setVisible(false);
    layout->addWidget(m_emptyLabel);

    m_grid=new ChatReactionGalleryGrid(this);
    // A row reports a pick straight through, exactly as the old per-cell connection did -- the
    // grid deliberately has no signals of its own (no Q_OBJECT, see ChatReactionGalleryRow).
    m_grid->setPickHandler(
        [this](const QString& iconId)
        {
            Q_EMIT reactionPicked(iconId);
        }
    );
    layout->addWidget(m_grid,1);
}

//--------------------------------------------------------------------------

ChatReactionGallery::~ChatReactionGallery()
{
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_pack=std::move(pack);
    m_recentBar->setPack(m_pack);
    m_grid->setPack(m_pack);
    m_grid->setOwnReactionIds(m_ownReactionIds);
    // The rows now describe the wrong pack. Marked stale rather than rebuilt here: while this
    // gallery is hidden (which, inside a collapsed ChatReactionGalleryDropdown, is every time a
    // context menu opens) the work waits for the first show -- see rebuildRows().
    m_rowsDirty=true;
    // The tab strip is cheap by comparison (one button and one sample icon per category, ~10 of
    // them) and its buttons must exist before rebuildRows() can decide whether to show the
    // strip, so it is not deferred.
    rebuildCategoryTabs();
    resetSearch();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setOwnReactionIds(QStringList ids)
{
    m_ownReactionIds=std::move(ids);
    m_recentBar->setOwnReactionIds(m_ownReactionIds);
    // Only the rows that currently exist are updated in place; rows built later (as the user
    // scrolls) read the stored list at construction -- see ChatReactionGalleryGrid.
    m_grid->setOwnReactionIds(m_ownReactionIds);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::resetSearch()
{
    m_searchEdit->clear();
    // Explicit, rather than relying on the clear()-triggered textChanged signal: clear() emits
    // nothing when the box was ALREADY empty (e.g. resetSearch() right after setPack(), on a
    // freshly-constructed gallery), and resetSearch() must put the browse rows on screen
    // regardless of the search box's prior state.
    rebuildRows(QString{});
}

//--------------------------------------------------------------------------

void ChatReactionGallery::focusSearch()
{
    m_searchEdit->setFocus(Qt::OtherFocusReason);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::onSearchTextChanged(const QString& text)
{
    rebuildRows(text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::rebuildRows(const QString& searchPrefix)
{
    if (searchPrefix!=m_appliedSearchPrefix)
    {
        m_rowsDirty=true;
    }
    m_pendingSearchPrefix=searchPrefix;

    if (!isVisible())
    {
        // Deferred until showEvent(). isVisible() is false both for an explicitly hidden gallery
        // (the collapsed dropdown's own setVisible(false)) and for one whose ancestors are not
        // on screen yet (a dropdown built but never popped up), which is exactly the set of
        // cases where building rows would be work for something nobody is looking at.
        return;
    }
    applyRows();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::showEvent(QShowEvent* event)
{
    // EMOJI-DEBUG: temporary diagnostic for the intermittent blank-emoji-icon bug -- orders this
    // (every) show relative to DefaultReactionIconPack's ctor in the log. Remove once the root
    // cause is confirmed.
    std::cerr << "EMOJI-DEBUG ChatReactionGallery::showEvent objectName="
               << objectName().toStdString() << std::endl;
    Frame::showEvent(event);
    applyRows();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::applyRows()
{
    if (!m_rowsDirty)
    {
        return;
    }
    m_rowsDirty=false;

    const auto searchPrefix=m_pendingSearchPrefix;
    m_appliedSearchPrefix=searchPrefix;

    std::vector<ChatReactionGalleryRowPlan> plan;
    m_categoryFirstRow.clear();

    const auto columns=std::max(1,m_galleryColumns);

    // Chops one run of pack indices into rows of `columns`, giving the FIRST row of the run the
    // section title (if any) so it draws a header -- see ChatReactionGalleryRow. Shared by the
    // sectioned browse pass and the flat search pass below, which differ only in whether they
    // pass a title at all.
    auto appendRun=[&plan,columns](const std::vector<size_t>& indices, const QString& title)
    {
        for (size_t offset=0; offset<indices.size(); offset+=static_cast<size_t>(columns))
        {
            ChatReactionGalleryRowPlan row;
            if (offset==0)
            {
                row.headerTitle=title;
            }
            const auto last=std::min(indices.size(),offset+static_cast<size_t>(columns));
            // ptrdiff_t, not long: long is 32-bit on Windows (clang-cl is a supported target
            // here), and iterator arithmetic wants the iterator's own difference_type.
            row.iconIndices.assign(indices.begin()+static_cast<std::ptrdiff_t>(offset),
                                   indices.begin()+static_cast<std::ptrdiff_t>(last));
            plan.push_back(std::move(row));
        }
    };

    if (m_pack)
    {
        if (searchPrefix.isEmpty())
        {
            const auto categories=m_pack->categories();
            if (categories.empty())
            {
                // A pack with no categories lays out as one unsectioned run -- exactly what
                // every pack looked like before categories existed. search("") returns every
                // index in pack order.
                appendRun(m_pack->search(QString{}),QString{});
            }
            else
            {
                for (const auto& category : categories)
                {
                    const auto indices=m_pack->categoryIcons(category.id);
                    if (indices.empty())
                    {
                        // No rows means no tab target either -- see m_categoryFirstRow.
                        continue;
                    }
                    m_categoryFirstRow.emplace(category.id,plan.size());
                    appendRun(indices,category.title);
                }
            }
        }
        else
        {
            // SEARCH mode: one flat run, no headers. A prefix ranks matches across every
            // category at once, so per-category headers over the results would be noise.
            appendRun(m_pack->search(searchPrefix),QString{});
        }
    }

    const auto empty=plan.empty();
    m_grid->setPlan(std::move(plan));
    m_grid->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
    // Tabs are a browse affordance: with a search prefix on screen there are no sections to jump
    // between, and with no categories at all there never were any.
    m_categoryTabs->setVisible(searchPrefix.isEmpty() && !m_categoryTabButtons.empty() && !empty);

    applyVisibleRowsHeight();

    Q_EMIT sizeChanged();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::applyVisibleRowsHeight()
{
    // galleryVisibleRows used to be stored and never read by anything: the gallery was as tall as
    // its whole pack and grew without bound with a bigger one. chatreactions.qss has always
    // documented this property as sizing the viewport; this is what makes that true.
    //
    // Measured from a live row rather than from a QSS number: the row's own size hint already
    // carries whatever icon size and padding the current theme gives it, so the clamp tracks the
    // theme instead of duplicating its numbers here. 0 means nothing is built yet to measure --
    // leave the clamp off rather than guess, the next rebuild measures again.
    const auto rowHeight=m_grid->measuredRowHeight();
    const auto rows=m_grid->rowCount();
    if (m_galleryVisibleRows<=0 || rowHeight<=0 || rows==0)
    {
        m_grid->setMinimumHeight(0);
        m_grid->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    // FIXED, not merely a maximum. A FlyweightListView holds only the rows near the viewport, so
    // its own size hint reflects that handful rather than the whole list -- a bare maximum would
    // let the layout settle on whatever those few rows happen to want, which is not the same
    // thing as "show N rows". The old ScrollArea could be left to its content's natural height
    // precisely because that content WAS the whole pack.
    //
    // Capped at the number of rows that actually exist, so a search matching two rows still
    // gives a two-row popup instead of reserving the full N.
    const auto shown=std::min(static_cast<size_t>(m_galleryVisibleRows),rows);
    m_grid->setFixedHeight(static_cast<int>(shown)*rowHeight);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::rebuildCategoryTabs()
{
    for (auto* tab : m_categoryTabButtons)
    {
        destroyWidget(tab);
    }
    m_categoryTabButtons.clear();

    if (!m_pack)
    {
        m_categoryTabs->setVisible(false);
        return;
    }

    auto* tabsLayout=static_cast<QHBoxLayout*>(m_categoryTabs->layout());
    for (const auto& category : m_pack->categories())
    {
        const auto* sampleInfo=m_pack->find(category.sampleIconId);
        auto* tab=new PushButton(sampleInfo!=nullptr ? sampleInfo->icon : nullptr,m_categoryTabs);
        tab->setObjectName("categoryTab");
        tab->setToolTip(category.title);
        const auto categoryId=category.id;
        connect(tab,&PushButton::clicked,this,
                [this,categoryId]()
                {
                    scrollToSection(categoryId);
                });
        tabsLayout->addWidget(tab);
        m_categoryTabButtons.push_back(tab);
    }

    // Final visibility is rebuildRows()' call to make -- it alone knows whether the CURRENT plan
    // is sectioned (browse) or flat (search), and whether it has any rows at all.
    m_categoryTabs->setVisible(false);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::scrollToSection(const QString& categoryId)
{
    auto it=m_categoryFirstRow.find(categoryId);
    if (it==m_categoryFirstRow.end())
    {
        // Not in the CURRENT plan: either a search is on screen (no sections at all) or this
        // category matched nothing. Nothing to scroll to, rather than an error.
        return;
    }
    m_grid->scrollToRow(it->second);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setSearchPlaceholderText(const QString& text)
{
    m_searchPlaceholderText=text;
    m_searchEdit->setPlaceholderText(text.isEmpty() ? tr("Search reactions") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setEmptyText(const QString& text)
{
    m_emptyText=text;
    m_emptyLabel->setText(text.isEmpty() ? tr("No matching reactions") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setRecentIds(QStringList ids)
{
    m_recentBar->setLeadingIconIds(std::move(ids));
}

//--------------------------------------------------------------------------

QStringList ChatReactionGallery::recentIds() const
{
    return m_recentBar->leadingIconIds();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setRecentTitleText(const QString& text)
{
    m_recentTitleText=text;
    m_recentTitle->setText(text.isEmpty() ? tr("Recently used") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setGalleryColumns(int value)
{
    if (m_galleryColumns==value) return;
    m_galleryColumns=value;
    // The column count is what chops each run of icons into rows, so the whole plan changes --
    // see applyRows()'s own appendRun(). Same prefix as before, so the dirty flag has to be set
    // explicitly; rebuildRows() only infers it from a CHANGED prefix.
    m_rowsDirty=true;
    rebuildRows(m_searchEdit->text());
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setGalleryVisibleRows(int value)
{
    if (m_galleryVisibleRows==value) return;
    m_galleryVisibleRows=value;
    applyVisibleRowsHeight();
    Q_EMIT sizeChanged();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::changeEvent(QEvent* event)
{
    Frame::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        // Through the setters, not straight to the widgets: a host that supplied its own wording
        // (an emoji picker says "Search emoji", not "Search reactions") must not have it silently
        // reverted to the reaction defaults by a language switch. Re-translating the override
        // itself is the host's job, from its own changeEvent -- see EmojiGalleryDialog.
        setRecentTitleText(m_recentTitleText);
        setSearchPlaceholderText(m_searchPlaceholderText);
        setEmptyText(m_emptyText);
        if (m_pack)
        {
            m_pack->retranslate();
        }
        // Tabs before rows: the pack's retranslate() above may have rebuilt its entries
        // wholesale (DefaultReactionIconPack does), so category titles AND the underlying icon
        // indices are both stale until these run, and rebuildRows() is what decides the tab
        // strip's final visibility. m_recentBar (a ChatReactionQuickBar) re-translates and
        // rebuilds itself independently, via its own changeEvent() override -- Qt delivers
        // LanguageChange to it directly as a child widget.
        //
        // Explicit dirty flag for the same reason setGalleryColumns() needs one: the search
        // prefix has not changed, only what the rows built from it would contain.
        m_rowsDirty=true;
        rebuildCategoryTabs();
        rebuildRows(m_searchEdit->text());
    }
}

//--------------------------------------------------------------------------

ChatReactionGalleryDropdown::ChatReactionGalleryDropdown(QWidget* parent)
    : DropdownFrame(parent)
{
    setObjectName("chatReactionGalleryDropdown");

    // The expanded page carries a search box, and a DropdownFrame is a non-activating Qt::Tool
    // window -- without this the box takes focus and blinks its caret while every typed character
    // still goes to whatever the host window had focused. See setKeyboardInputEnabled().
    setKeyboardInputEnabled(true);

    auto* content=new QFrame();
    auto* layout=Layout::vertical(content);

    // Picking a reaction (either page) is a genuine "activation" of this popup, exactly like a
    // DropdownMenu row's own click handler (emit itemTriggered(id); notifyActivated(...);) --
    // notifyActivated() closes chainRoot() (see its own doc comment), so a gallery chained above
    // a context menu takes that menu down with it too, instead of leaving it open behind an
    // already-closed gallery. Forwarding reactionPicked() first, so the host's own handler still
    // sees it before anything starts closing.
    auto onReactionPicked=[this](const QString& reactionId)
    {
        Q_EMIT reactionPicked(reactionId);
        notifyActivated();
    };

    m_quickBar=new ChatReactionQuickBar(content);
    connect(m_quickBar,&ChatReactionQuickBar::reactionPicked,this,onReactionPicked);
    connect(m_quickBar,&ChatReactionQuickBar::expandRequested,this,
            [this]()
            {
                setExpanded(true);
            });
    layout->addWidget(m_quickBar);

    m_gallery=new ChatReactionGallery(content);
    m_gallery->setVisible(false);
    connect(m_gallery,&ChatReactionGallery::reactionPicked,this,onReactionPicked);
    connect(m_gallery,&ChatReactionGallery::sizeChanged,this,
            [this,content]()
            {
                content->updateGeometry();
                // Keep the collapsed bar's own on-screen position fixed and grow/shrink the
                // panel below it -- see setExpanded()'s identical reasoning. A search result
                // count change is exactly the same "content changed shape while already
                // expanded" case setExpanded() itself handles.
                remeasureKeepingTopLeft(false);
            });
    layout->addWidget(m_gallery);

    setContent(content);
}

//--------------------------------------------------------------------------

ChatReactionGalleryDropdown::~ChatReactionGalleryDropdown()
{
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_quickBar->setPack(pack);
    m_gallery->setPack(std::move(pack));
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setOwnReactionIds(QStringList ids)
{
    m_quickBar->setOwnReactionIds(ids);
    m_gallery->setOwnReactionIds(std::move(ids));
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setRecentIds(QStringList ids)
{
    m_quickBar->setLeadingIconIds(ids);
    m_gallery->setRecentIds(std::move(ids));
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setExpanded(bool enable)
{
    if (m_expanded==enable)
    {
        return;
    }
    m_expanded=enable;

    m_quickBar->setVisible(!m_expanded);
    m_gallery->setVisible(m_expanded);
    if (m_expanded)
    {
        m_gallery->resetSearch();
        // Expanding is a deliberate "show me everything" gesture, and the pack is ~1374 icons --
        // typing a name is the fast route through it, so the caret starts there rather than
        // making the user click the box first. Safe to do while the panel is still being laid
        // out: setFocus() only records the focus child, and DropdownFrame's key forwarding reads
        // that child at delivery time, not now.
        m_gallery->focusSearch();
    }

    content()->updateGeometry();
    // Keeps the collapsed quick bar's own on-screen position fixed and unfolds the gallery
    // downward beneath it (or folds it back away) -- see remeasureKeepingTopLeft()'s own doc
    // comment for why remeasure() alone is the wrong tool here: it would re-derive position from
    // the ORIGINAL anchor (popupAboveRect()'s upward growth), moving the header itself on every
    // expand/collapse instead of leaving it in place. A no-op if the frame is not currently open
    // (e.g. setExpanded() called before the first popupX()); the NEXT opening measures whichever
    // page is visible at that time from scratch.
    remeasureKeepingTopLeft(false);
}

//--------------------------------------------------------------------------

}
