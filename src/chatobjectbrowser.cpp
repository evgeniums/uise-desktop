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

/** @file uise/desktop/src/chatobjectbrowser.cpp
*
*  Defines ChatObjectBrowserModel and AbstractChatObjectBrowser.
*
*/

/****************************************************************************/

#include <algorithm>
#include <iterator>

#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <uise/desktop/chatobjectbrowser.hpp>
#include <uise/desktop/searchlineedit.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/utils/layout.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

namespace {

bool itemKeyLess(const ChatObjectItem& left, const ChatObjectItem& right)
{
    return left.key<right.key;
}

//! Pause after the last keystroke before the typed text becomes a search.
constexpr int SearchDelayMs=300;

} // anonymous namespace

//--------------------------------------------------------------------------

int chatObjectMonthKey(const QDateTime& dateTime)
{
    if (!dateTime.isValid())
    {
        return -1;
    }
    const auto date=dateTime.toLocalTime().date();
    return date.year()*12+date.month()-1;
}

//--------------------------------------------------------------------------

QString chatObjectMonthTitle(const QDateTime& dateTime)
{
    if (!dateTime.isValid())
    {
        return QString{};
    }
    return dateAsMonthAndYear(dateTime.toLocalTime().date());
}

//--------------------------------------------------------------------------

bool ChatObjectBrowserModel::startsMonth(const ChatObjectKey& key) const
{
    const auto idx=lowerBound(key);
    if (idx>=m_items.size() || m_items[idx].key!=key)
    {
        return false;
    }
    if (idx+1==m_items.size())
    {
        return true;
    }
    return chatObjectMonthKey(m_items[idx+1].dateTime)!=chatObjectMonthKey(m_items[idx].dateTime);
}

//--------------------------------------------------------------------------

void ChatObjectBrowserModel::clear()
{
    m_items.clear();
    m_endOlder=false;
    m_endNewer=false;
}

//--------------------------------------------------------------------------

const ChatObjectItem* ChatObjectBrowserModel::oldest() const noexcept
{
    return m_items.empty() ? nullptr : &m_items.front();
}

//--------------------------------------------------------------------------

const ChatObjectItem* ChatObjectBrowserModel::newest() const noexcept
{
    return m_items.empty() ? nullptr : &m_items.back();
}

//--------------------------------------------------------------------------

size_t ChatObjectBrowserModel::lowerBound(const ChatObjectKey& key) const
{
    const auto it=std::lower_bound(
        m_items.begin(),
        m_items.end(),
        key,
        [](const ChatObjectItem& item, const ChatObjectKey& value)
        {
            return item.key<value;
        }
    );
    return static_cast<size_t>(it-m_items.begin());
}

//--------------------------------------------------------------------------

const ChatObjectItem* ChatObjectBrowserModel::find(const ChatObjectKey& key) const
{
    const auto idx=lowerBound(key);
    if (idx<m_items.size() && m_items[idx].key==key)
    {
        return &m_items[idx];
    }
    return nullptr;
}

//--------------------------------------------------------------------------

ChatObjectBrowserModel::MergeResult ChatObjectBrowserModel::merge(std::vector<ChatObjectItem> incoming)
{
    MergeResult result;

    std::stable_sort(incoming.begin(),incoming.end(),itemKeyLess);
    incoming.erase(
        std::unique(
            incoming.begin(),
            incoming.end(),
            [](const ChatObjectItem& left, const ChatObjectItem& right)
            {
                return left.key==right.key;
            }
        ),
        incoming.end()
    );

    std::vector<ChatObjectItem> fresh;
    for (auto& item : incoming)
    {
        const auto idx=lowerBound(item.key);
        if (idx<m_items.size() && m_items[idx].key==item.key)
        {
            m_items[idx]=item;
            result.updated.push_back(std::move(item));
        }
        else
        {
            fresh.push_back(std::move(item));
        }
    }

    if (!fresh.empty())
    {
        result.added=fresh;

        const auto middle=m_items.size();
        m_items.insert(
            m_items.end(),
            std::make_move_iterator(fresh.begin()),
            std::make_move_iterator(fresh.end())
        );
        std::inplace_merge(
            m_items.begin(),
            m_items.begin()+static_cast<std::ptrdiff_t>(middle),
            m_items.end(),
            itemKeyLess
        );
    }

    return result;
}

//--------------------------------------------------------------------------

bool ChatObjectBrowserModel::replace(const ChatObjectItem& item)
{
    const auto idx=lowerBound(item.key);
    if (idx<m_items.size() && m_items[idx].key==item.key)
    {
        m_items[idx]=item;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------

bool ChatObjectBrowserModel::remove(const ChatObjectKey& key)
{
    const auto idx=lowerBound(key);
    if (idx<m_items.size() && m_items[idx].key==key)
    {
        m_items.erase(m_items.begin()+static_cast<std::ptrdiff_t>(idx));
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------

std::vector<ChatObjectKey> ChatObjectBrowserModel::removeMessage(const QString& messageId)
{
    std::vector<ChatObjectKey> removed;
    for (const auto& item : m_items)
    {
        if (item.messageId==messageId)
        {
            removed.push_back(item.key);
        }
    }
    if (!removed.empty())
    {
        m_items.erase(
            std::remove_if(
                m_items.begin(),
                m_items.end(),
                [&messageId](const ChatObjectItem& item)
                {
                    return item.messageId==messageId;
                }
            ),
            m_items.end()
        );
    }
    return removed;
}

//--------------------------------------------------------------------------

std::vector<ChatObjectItem> ChatObjectBrowserModel::olderThan(const ChatObjectKey& key, size_t count) const
{
    std::vector<ChatObjectItem> result;
    const auto idx=lowerBound(key);
    const auto take=std::min(count,idx);
    result.reserve(take);
    for (size_t i=1; i<=take; ++i)
    {
        result.push_back(m_items[idx-i]);
    }
    return result;
}

//--------------------------------------------------------------------------

std::vector<ChatObjectItem> ChatObjectBrowserModel::newerThan(const ChatObjectKey& key, size_t count) const
{
    std::vector<ChatObjectItem> result;
    auto idx=lowerBound(key);
    if (idx<m_items.size() && m_items[idx].key==key)
    {
        ++idx;
    }
    const auto last=std::min(m_items.size(),idx+count);
    result.reserve(last>idx ? last-idx : 0);
    for (size_t i=idx; i<last; ++i)
    {
        result.push_back(m_items[i]);
    }
    return result;
}

//--------------------------------------------------------------------------

AbstractChatObjectBrowser::AbstractChatObjectBrowser(QString kind, QWidget* parent)
    : FrameWithRefresh(parent),
      m_kind(std::move(kind)),
      m_pageCount(DefaultPageCount),
      m_started(false),
      m_activationGoesToMessage(false),
      m_groupByMonth(true),
      m_layout(nullptr),
      m_searchEdit(nullptr),
      m_searchTimer(nullptr),
      m_emptyLabel(nullptr),
      m_view(nullptr)
{
    setObjectName("chatObjectBrowser");

    m_emptyText=tr("Nothing here yet");

    m_layout=Layout::vertical(this);

    m_searchEdit=new SearchLineEdit(this);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(tr("Search"));
    // The host (a dialog) owns Escape; the line has its own cancel button.
    m_searchEdit->setCancelShortcutEnabled(false);
    m_searchEdit->setVisible(false);
    m_layout->addWidget(m_searchEdit);

    m_searchTimer=new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SearchDelayMs);
    connect(m_searchTimer,&QTimer::timeout,this,&AbstractChatObjectBrowser::applySearch);
    connect(m_searchEdit,&QLineEdit::textChanged,m_searchTimer,
        [this]()
        {
            m_searchTimer->start();
        }
    );

    m_emptyLabel=new QLabel(m_emptyText,this);
    m_emptyLabel->setObjectName("emptyLabel");
    // Plain text: the text is set by the host and may be anything.
    m_emptyLabel->setTextFormat(Qt::PlainText);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setVisible(false);
    m_layout->addWidget(m_emptyLabel,1);
}

//--------------------------------------------------------------------------

AbstractChatObjectBrowser::~AbstractChatObjectBrowser()
{
    // The menu is created parentless, like the other per-item menus of this library, so nothing
    // else owns it.
    delete m_menu.data();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setRequestCb(RequestCb cb)
{
    m_requestCb=std::move(cb);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setContextMenuBuilder(ContextMenuBuilder builder)
{
    m_menuBuilder=std::move(builder);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setEmptyText(const QString& text)
{
    m_emptyText=text;
    updateEmptyState();
}

//--------------------------------------------------------------------------

QString AbstractChatObjectBrowser::emptyText() const
{
    return m_emptyText;
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setGroupByMonth(bool enable)
{
    if (m_groupByMonth==enable)
    {
        return;
    }
    m_groupByMonth=enable;
    doGroupingChanged();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::doGroupingChanged()
{
    if (m_model.empty())
    {
        return;
    }
    const std::vector<ChatObjectItem> viewItems(m_model.items().rbegin(),m_model.items().rend());
    doLoad(viewItems,ChatObjectKey{});
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setSearchVisible(bool enable)
{
    if (!enable)
    {
        // a hidden search must not keep filtering
        setSearchText(QString{});
    }
    m_searchEdit->setVisible(enable);
}

//--------------------------------------------------------------------------

bool AbstractChatObjectBrowser::isSearchVisible() const noexcept
{
    return !m_searchEdit->isHidden();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setSearchText(const QString& text)
{
    // Signals are not blocked: the line has its own state (the cancel button) that follows its
    // text. The typing pause that setText() starts is cancelled, the text is applied here.
    m_searchEdit->setText(text);
    m_searchTimer->stop();
    applySearch();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::applySearch()
{
    const QString query=m_searchEdit->text().trimmed();
    if (query==m_query)
    {
        return;
    }

    m_query=query;
    emit searchTextChanged(m_query);

    // before the first reload() there is nothing to redo; the text goes with the first request
    if (m_started)
    {
        reload();
    }
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setSearchPlaceholderText(const QString& text)
{
    m_searchEdit->setPlaceholderText(text.isEmpty() ? tr("Search") : text);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::focusSearch()
{
    if (isSearchVisible())
    {
        m_searchEdit->setFocus(Qt::OtherFocusReason);
    }
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setPageCount(size_t count) noexcept
{
    m_pageCount=count==0 ? DefaultPageCount : count;
}

//--------------------------------------------------------------------------

bool AbstractChatObjectBrowser::isLoading() const noexcept
{
    return m_pendingReset.id!=0 || m_pendingOlder.id!=0 || m_pendingNewer.id!=0;
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setView(QWidget* view)
{
    m_view=view;
    // below the search line
    m_layout->insertWidget(1,view,1);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::setActivationGoesToMessage(bool enable) noexcept
{
    m_activationGoesToMessage=enable;
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::doRefresh()
{
    reload();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::updateEmptyState()
{
    const bool empty=m_started
                     && m_model.empty()
                     && m_model.isEndOlder()
                     && m_model.isEndNewer()
                     && m_pendingReset.id==0;
    // a search that finds nothing is not an empty chat
    m_emptyLabel->setText(m_query.isEmpty() ? m_emptyText : tr("Nothing found"));
    m_emptyLabel->setVisible(empty);
    if (m_view!=nullptr)
    {
        m_view->setVisible(!empty);
    }
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::resetState()
{
    m_model.clear();
    m_pendingReset=Pending{};
    m_pendingOlder=Pending{};
    m_pendingNewer=Pending{};
    doReset();
    updateEmptyState();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::sendRequest(ChatObjectsRequest request, Pending& slot)
{
    // Process-wide, not per browser: the owner may answer after the browser is gone and another
    // one of the same kind is open, and that one must not take the stale answer for its own.
    static quint64 lastRequestId=0;
    request.requestId=++lastRequestId;
    request.kind=m_kind;
    request.query=m_query;

    // The slot is taken before the callback runs: the owner is free to answer synchronously, and
    // the answer is only accepted for a request that is registered.
    slot.id=request.requestId;

    if (m_requestCb)
    {
        m_requestCb(request);
    }
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::reload()
{
    resetState();
    m_started=true;

    ChatObjectsRequest request;
    request.countBefore=m_pageCount*2;

    m_pendingReset.nullAnchor=true;
    sendRequest(std::move(request),m_pendingReset);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::openAt(const ChatObjectKey& key)
{
    if (key.isNull())
    {
        reload();
        return;
    }

    resetState();
    m_started=true;

    ChatObjectsRequest request;
    request.anchor=key;
    request.anchorInclusive=true;
    request.countBefore=m_pageCount*2;
    request.countAfter=m_pageCount;

    m_pendingReset.focus=key;
    sendRequest(std::move(request),m_pendingReset);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::applyPage(const ChatObjectsPage& page)
{
    if (page.requestId==0)
    {
        return;
    }

    if (page.requestId==m_pendingReset.id)
    {
        handleResetPage(page);
    }
    else if (page.requestId==m_pendingOlder.id)
    {
        handleEdgePage(page,true);
    }
    else if (page.requestId==m_pendingNewer.id)
    {
        handleEdgePage(page,false);
    }
    // else: unknown or superseded by reload()/openAt(), dropped
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::handleResetPage(const ChatObjectsPage& page)
{
    const Pending pending=m_pendingReset;
    m_pendingReset=Pending{};

    m_model.merge(page.items);

    // An empty answer is the end in both directions whatever it says: there is nothing to scroll
    // to, and a broken owner must not make the view ask again forever.
    const bool noItems=m_model.empty();
    m_model.setEndOlder(page.endBefore || noItems);
    m_model.setEndNewer(pending.nullAnchor || page.endAfter || noItems);

    const std::vector<ChatObjectItem> viewItems(m_model.items().rbegin(),m_model.items().rend());
    doLoad(viewItems,pending.focus);

    updateEmptyState();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::handleEdgePage(const ChatObjectsPage& page, bool older)
{
    if (older)
    {
        m_pendingOlder=Pending{};
    }
    else
    {
        m_pendingNewer=Pending{};
    }

    const auto result=m_model.merge(page.items);

    // Same rule as for the first answer: nothing new means nothing more.
    if (older)
    {
        if (page.endBefore || result.added.empty())
        {
            m_model.setEndOlder(true);
        }
    }
    else
    {
        if (page.endAfter || result.added.empty())
        {
            m_model.setEndNewer(true);
        }
    }

    for (const auto& item : result.updated)
    {
        doUpdate(item);
    }

    doEdgeArrived(older);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::requestEdge(bool older)
{
    if (m_pendingReset.id!=0 || m_model.empty())
    {
        return;
    }

    auto& slot=older ? m_pendingOlder : m_pendingNewer;
    if (slot.id!=0)
    {
        return;
    }

    ChatObjectsRequest request;
    if (older)
    {
        request.anchor=m_model.oldest()->key;
        request.countBefore=m_pageCount;
    }
    else
    {
        request.anchor=m_model.newest()->key;
        request.countAfter=m_pageCount;
    }
    sendRequest(std::move(request),slot);
}

//--------------------------------------------------------------------------

std::vector<ChatObjectItem> AbstractChatObjectBrowser::serveEdge(const ChatObjectKey& anchor, size_t count, bool older)
{
    std::vector<ChatObjectItem> items;
    if (older)
    {
        // nearest first == descending keys == the widgets' order
        items=m_model.olderThan(anchor,count);
    }
    else
    {
        items=m_model.newerThan(anchor,count);
        std::reverse(items.begin(),items.end());
    }

    if (items.size()<count)
    {
        const bool exhausted=older ? m_model.isEndOlder() : m_model.isEndNewer();
        if (!exhausted)
        {
            requestEdge(older);
        }
    }

    return items;
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::removeMessageObjects(const QString& messageId)
{
    const auto removed=m_model.removeMessage(messageId);
    if (removed.empty())
    {
        return;
    }

    doRemove(removed);

    if (m_model.empty() && !(m_model.isEndOlder() && m_model.isEndNewer()))
    {
        // everything that was loaded is gone but there is more behind it
        reload();
        return;
    }
    updateEmptyState();
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::updateItem(const ChatObjectItem& item)
{
    if (m_model.replace(item))
    {
        doUpdate(item);
    }
}

//--------------------------------------------------------------------------

ChatObjectRowHooks AbstractChatObjectBrowser::rowHooks()
{
    ChatObjectRowHooks hooks;
    hooks.activated=[this](const ChatObjectItem& item)
    {
        activate(item);
    };
    hooks.goToMessage=[this](const ChatObjectItem& item)
    {
        emit goToMessageRequested(item.messageId);
    };
    hooks.contextMenu=[this](const ChatObjectItem& item, const QPoint& globalPos)
    {
        showContextMenu(item,globalPos);
    };
    hooks.action=[this](const ChatObjectItem& item, const QString& action, const QVariant& argument)
    {
        emit itemActionRequested(item,action,argument);
    };
    return hooks;
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::activate(const ChatObjectItem& item)
{
    if (m_activationGoesToMessage)
    {
        emit goToMessageRequested(item.messageId);
    }
    else
    {
        emit itemActivated(item);
    }
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::showContextMenu(const ChatObjectItem& item, const QPoint& globalPos)
{
    if (m_menu.isNull())
    {
        m_menu=new DropdownMenu();
        connect(m_menu.data(),&DropdownMenu::itemTriggered,this,&AbstractChatObjectBrowser::onMenuTriggered);
    }

    m_menuItem=item;

    std::vector<MenuItem> items;
    items.emplace_back(MenuOpen,tr("Open"));
    if (!m_activationGoesToMessage)
    {
        items.emplace_back(MenuShowInChat,tr("Show in chat"));
    }
    if (m_menuBuilder)
    {
        auto extra=m_menuBuilder(item);
        if (!extra.empty())
        {
            items.push_back(MenuItem::separator());
            for (auto& entry : extra)
            {
                items.push_back(std::move(entry));
            }
        }
    }
    m_menu->setItems(std::move(items));
    m_menu->popupAt(globalPos);
}

//--------------------------------------------------------------------------

void AbstractChatObjectBrowser::onMenuTriggered(int id)
{
    // copy: a handler may reload the browser, and m_menuItem must not change under the signal
    const ChatObjectItem item=m_menuItem;
    if (id==MenuOpen)
    {
        activate(item);
    }
    else if (id==MenuShowInChat)
    {
        emit goToMessageRequested(item.messageId);
    }
    else
    {
        emit contextMenuTriggered(item,id);
    }
}

} // namespace uise
