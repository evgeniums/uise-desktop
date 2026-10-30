# Chat object browsers

Widgets that browse the objects of one chat, one browser per kind of object: a thumbnail grid for
images, lists for files, voice messages, links and mentions. UI only and data driven: nothing here
reads a database, computes a count or knows the client library. The owner (in the desktop app, a thin
adapter over the bridge) answers the browsers' requests.

Status: **written, not compiled** (Stage 3 of the chat message counters task).

Contents: 1 Files, 2 Data types, 3 Keyset semantics, 4 Browser API, 5 Panel and registry,
6 Styling and strings, 7 Behaviour worth knowing, 8 For the bridge and the adapter,
9 Open questions.

## 1. Files

All under `include/uise/desktop`, `src`, `resources/style` of uise-desktop.

| File | What |
|---|---|
| `chatobjectitem.hpp` | `ChatObjectKey`, `ChatObjectItem`, `ChatObjectsRequest`, `ChatObjectsPage`, `ChatObjectKind` (header only) |
| `chatobjectbrowser.hpp/.cpp` | `ChatObjectBrowserModel`, `ChatObjectRowHooks`, `ChatObjectAction`, `AbstractChatObjectBrowser` |
| `chatobjectlistbrowser.hpp/.cpp` | `ChatObjectListBrowser` (a row per object), `ChatObjectRowFactory` |
| `chatobjectrows.cpp` | row factories and browser makers of the file, voice, link and mention kinds |
| `chatobjectgallerybrowser.hpp/.cpp` | `ChatObjectGalleryBrowser`, the thumbnail grid |
| `chatobjectbrowserpanel.hpp/.cpp` | `ChatObjectBrowserRegistry`, `ChatObjectBrowserPanel` (back and title bar) |
| `style/chatobjectbrowser.{qss,json}` (+ `light/`, `dark/`) | styles and icon context `ChatObjectBrowser` |

Demo: `demo/chatobjectbrowser` (`chatobjectbrowser-demo`). Tests: `test/chatobjectbrowser`
(`chatobjectbrowser-test`).

## 2. Data types

```cpp
namespace ChatObjectKind { Image="image", File="file", Voice="voice", Link="link", Mention="mention" }
```

A kind is **any string**. The set is open; a new kind (video) is registered, see 5.

```cpp
struct ChatObjectKey            // keyset position AND identity of an object within a kind
{
    QByteArray sortId;          // opaque, compared bytewise; empty = null key
    quint32 ordinal = 0;        // tie-break among the objects of one message and kind
};                              // operator< : sortId bytes, then ordinal. Ascending = older -> newer.

struct ChatObjectItem
{
    QString kind;
    ChatObjectKey key;
    QString messageId;          // the message the object belongs to (go to message, removal)
    QDateTime dateTime;
    bool incoming = true;
    QString senderName;
    WithPath senderAvatarPath;                    // mention rows
    std::shared_ptr<AvatarSource> avatarSource;   // mention rows
    ChatFileItem file;          // image/file/voice: name, size, mime, state, voice waveform+duration
    WithPath thumbnailPath;     // image cells
    std::shared_ptr<RoundedImageSource> thumbnailSource;   // may resolve later
    QString url, title;         // link
    QString text;               // link and mention: snippet of the message
    QVariantMap extra;          // kinds added later
    QString uid() const;        // == key.toString()
};

struct ChatObjectsRequest       // browser -> owner
{
    quint64 requestId;          // process-wide unique
    QString kind;
    ChatObjectKey anchor;       // null = the newest end
    bool anchorInclusive;
    size_t countBefore;         // how many OLDER than the anchor
    size_t countAfter;          // how many NEWER than the anchor
    QString query;              // search text, empty when not searching
};

struct ChatObjectsPage          // owner -> browser
{
    quint64 requestId;          // echoed
    std::vector<ChatObjectItem> items;   // any order, duplicates tolerated
    bool endBefore;             // nothing older than the oldest of items
    bool endAfter;              // nothing newer than the newest of items
};
```

The key is the identity. If the owner changes the sort position of a message (a resend restamps it),
it removes the message's objects with `removeMessageObjects(messageId)` and delivers them again; the
browser does not match old and new keys.

## 3. Keyset semantics

There are no pages, offsets or totals. The browser asks for objects around a key:

* **Anchor.** A null anchor means the newest end: the answer is the newest `countBefore` objects
  (`countAfter` is 0). A real anchor is the key of the oldest or newest loaded object (scrolling) or
  of an object to open at.
* **Before / after.** *Before* is older (smaller keys), *after* is newer. The answer holds at most
  `countBefore` objects older than the anchor and at most `countAfter` newer ones, the *nearest* ones.
  The anchor itself is in the answer only if `anchorInclusive`.
* **Three shapes of request** the browser makes:

  | When | anchor | inclusive | before | after |
  |---|---|---|---|---|
  | `reload()` and a first open | null | no | 2 x page | 0 |
  | `openAt(key)` | key | **yes** | 2 x page | page |
  | scrolling down (older) | the browser's **oldest** loaded key | no | page | 0 |
  | scrolling up (newer) | the browser's **newest** loaded key | no | 0 | page |

  `page` is `AbstractChatObjectBrowser::pageCount()`, 30 by default.
* **"No more" is explicit.** The owner sets `endBefore` when nothing older exists, `endAfter` when
  nothing newer exists. A *short* answer is not proof of the end: the owner may drop rows (cleared
  ranges, see 8) after its database limit. The flags are only read for the side that was asked about.
  A null anchor implies `endAfter`.
* **An empty answer is the end** of the side that was asked about (of both sides for the first
  answer), whatever the flags say. So the owner must not return an empty page "to try again": a
  browser with an empty answer stops asking.
* **One request per side in flight.** A browser never has two outstanding requests for the older
  side (or for the newer one). A first-load request (`reload`, `openAt`) supersedes every other.
* **Stale answers are dropped.** An answer whose `requestId` the browser does not have outstanding is
  ignored: one superseded by `reload()` or `openAt()`, one answered twice, one meant for a browser
  that is gone. Ids are unique in the process, so a new browser of the same kind never takes the
  answer to an old one. The owner may therefore answer *any* current browser with *any* page and let
  the id decide (the demo does that).
* **Search.** While the user searches, every request carries the text in `query`. The owner returns
  only the matching objects, in the same keyset order, and the end flags are about the matching set.
  The widgets never filter: they hold a window of the data, so only the owner can say what matches.
  What matches is up to the owner and the kind (file names, urls and titles, message texts,
  senders). A change of the text reloads the browser from the newest end; answers to requests of
  the old text are dropped like any stale answer.
* **The answer may come at any time and from anywhere**, including synchronously from inside the
  request callback.

The browser keeps what it loaded (`ChatObjectBrowserModel`, sorted by key). Scrolling back over loaded
ground is served from it and does not ask the owner; only an edge that is not known to be exhausted
does. This makes the browser's memory proportional to what the user has scrolled through (small
structs; the widgets stay flyweight).

## 4. Browser API

`AbstractChatObjectBrowser` (a `FrameWithRefresh`, so it can be a page of `StackWithNavigationBar`;
`refresh()` reloads).

```cpp
using RequestCb = std::function<void (const ChatObjectsRequest&)>;
using ContextMenuBuilder = std::function<std::vector<MenuItem> (const ChatObjectItem&)>;

void setRequestCb(RequestCb);                 // before reload()/openAt()
void setContextMenuBuilder(ContextMenuBuilder);   // extra entries, ids >= MenuFirstCustomId (100)
void setEmptyText(const QString&);
void setPageCount(size_t);
void setActivationGoesToMessage(bool);        // mentions: a click means "show the message"
void setGroupByMonth(bool);                   // month headings, on by default
void setSearchVisible(bool);                  // the search line, hidden by default
void setSearchText(const QString&);           // as if typed, applied at once
void setSearchPlaceholderText(const QString&);
void focusSearch();

void reload();                                // from the newest
void openAt(const ChatObjectKey&);            // around a key; the key is brought to the top
void applyPage(const ChatObjectsPage&);       // the answer
void removeMessageObjects(const QString& messageId);   // a deleted message
void updateItem(const ChatObjectItem&);       // same key: refresh the loaded object and its row
const ChatObjectBrowserModel& model() const;
bool isLoading() const;
bool isGroupByMonth() const;
bool isSearchVisible() const;
const QString& searchText() const;           // the text in effect
```

Signals out. **Counts are never computed by the widgets.**

```cpp
void itemActivated(const ChatObjectItem&);                  // click, or "Open"
void goToMessageRequested(const QString& messageId);        // "Show in chat"; mention click
void contextMenuTriggered(const ChatObjectItem&, int id);   // an entry added by the builder
void itemActionRequested(const ChatObjectItem&, const QString& action, const QVariant& argument);
void searchTextChanged(const QString&);                     // the text in effect; the browser reloads
```

`itemActionRequested` carries the actions of the rich rows (names in `ChatObjectAction`):

| action | from | argument |
|---|---|---|
| `loadControl`, `pause`, `cancel` | file, voice rows | - |
| `fileMenu` | file, voice rows' own menu button | `int`, a `ChatFileMenuAction` |
| `play`, `stop` | voice row | - |
| `seek` | voice row | `qreal` fraction in [0,1] |

The built-in context menu (right click) has "Open" (-> `itemActivated`) and "Show in chat" (->
`goToMessageRequested`), then the builder's entries.

`ChatObjectListBrowser::rowWidget(key)` returns the live row widget, or null (rows far from the
viewport do not exist). The host drives a playing voice message with it: `updateItem()` with
`file.setPlaying(true)` for the glyph, then on every tick
`qobject_cast<ChatVoiceFileItem*>(rowWidget(key))->setPlaybackProgress()/setPlaybackPosition()`.

`ChatObjectGalleryBrowser` has `minCellSize` and `cellSpacing` (QSS properties) and `columnCount()`.

### Month headings

With `setGroupByMonth(true)` (the default) the first object of every month is preceded by a heading
("October 2026", in the current locale). The month is the month of `ChatObjectItem::dateTime` in the
local time zone; an object without a valid date has no month and no heading. Months are taken in the
order of the keys, so keys and dates are assumed to run the same way (a date out of order gives a
second heading for its month).

* **Lists.** Every row is a container with an optional heading above the kind's own row; the row
  factories know nothing about it. When a newer object of the month arrives, or the first object of a
  month is removed, the headings of the rows that exist are redone.
* **Grid.** A heading is a row of its own. The objects of a month are cut into lines of N cells **from
  the month's newest object**, so a line is full except the last one of the month, and objects that
  arrive on the older side never move a line. Objects that arrive on the newer side and belong to the
  month of the topmost row cut its lines anew: the grid is rebuilt around the first visible row and
  keeps that row at the same place on the screen (the cells of that month may shift sideways by a
  few). That happens while scrolling up through the month a window was opened in, and not at all
  when the browser started from the newest end. A line that could still grow (its month may have
  more older objects) is not built until it is full or the month is known to end.
* Switching grouping on or off: the lists redo their headings, the grid cuts its lines anew.

### Search line

`setSearchVisible(true)` shows a `SearchLineEdit` above the objects. Typing is applied after a pause of
300 ms; `setSearchText()` applies at once. Either way a changed text (trimmed) reloads the browser and
the owner gets it as `ChatObjectsRequest::query`. `setSearchVisible(false)` also ends the search (the
text is cleared and the browser reloads without it). A search with no result shows "Nothing found"
instead of the kind's empty text. The line is outside the view, so it stays when there is nothing to
show. The panel forwards `searchTextChanged`.

## 5. Panel and registry

`ChatObjectBrowserRegistry` maps a kind to `{ title(), emptyText(), factory(parent) }`. The texts are
functions so that they are translated when asked. `ChatObjectBrowserRegistry::defaults()` has image
(the grid), file, voice, link and mention (lists).

`ChatObjectBrowserPanel` shows one browser at a time under a bar with a back button and a title:

```cpp
void setRegistry(ChatObjectBrowserRegistry);
void setRequestCb(RequestCb);                 // given to every browser it creates
void setContextMenuBuilder(ContextMenuBuilder);
AbstractChatObjectBrowser* open(const QString& kind, const ChatObjectKey& anchor = {});
void closeBrowser();
AbstractChatObjectBrowser* currentBrowser() const;
void setTitle(const QString&);                // e.g. "Images (123)"
void setBackButtonVisible(bool);
void addTrailingWidget(QWidget*);
// signals: backRequested(), and itemActivated / goToMessageRequested / contextMenuTriggered /
// itemActionRequested of the current browser
```

The panel does not navigate; `backRequested()` tells the owner. Used as a page of
`StackWithNavigationBar` the owner connects it to the stack's `truncate()`.

**Adding a kind** (e.g. video):

```cpp
auto registry = ChatObjectBrowserRegistry::defaults();
registry.registerKind("video", {
    [] { return QCoreApplication::translate("ChatInfo", "Videos"); },
    [] { return QCoreApplication::translate("ChatInfo", "No videos in this chat"); },
    [](QWidget* parent) -> AbstractChatObjectBrowser*
    {
        ChatObjectRowFactory rows;
        rows.build = [](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame* { ... };
        // rows.update is optional
        return new ChatObjectListBrowser("video", std::move(rows), parent);
        // or: new ChatObjectGalleryBrowser("video", parent) for a grid of thumbnails
    } });
panel->setRegistry(std::move(registry));
```

## 6. Styling and strings

* Styles are in `style/chatobjectbrowser.qss` with colours in `light/` and `dark/`. Object names:
  `searchEdit`, `monthHeader` (above a list row, and a heading row `monthRow` of the grid), `emptyLabel`, `linkRow`, `mentionRow`, `rowIcon`, `rowAvatar`, `textColumn`, `title`, `url`,
  `snippet`, `meta`, `galleryRow`, `galleryCell` (its background is the **placeholder** shown until
  a thumbnail resolves), `galleryImage`; panel: `header`, `backButton`, `title`, `body`,
  `unknownLabel`. Reused tints only: the chat file row's hover, the reaction chip's tint.
* Icon context `ChatObjectBrowser` (back, image, file, voice, link, mention), all outline tabler
  icons that were already in the curated qrc.
* The file and voice rows are `ChatMessageFileItem` / `ChatVoiceFileItem` and keep their
  `chatmessagefiles.qss` look.
* Strings: `tr()` in the `Q_OBJECT` classes (context `uise::AbstractChatObjectBrowser`);
  `QCoreApplication::translate("ChatObjectBrowser", ...)` for the rest (registry titles, date and
  separator formats, the rows). After building run `scripts/i18n-update.sh` and the per-language
  journal step of the `whitemdesktop-i18n` skill.

## 7. Behaviour worth knowing

* **Order.** Newest first. The top of every browser is the newest object.
* **The view is gap-free by construction.** A flyweight view only ever gets items from
  `serveEdge()`, i.e. the model's items beyond the view's own edge. Never insert answers around the
  model.
* **Markers.** FlyweightListView believes there is always more until it is told otherwise. The
  browsers set its min/max sort values *after* `loadItems()` (which clears them) and before
  `endUpdate()`, and only when the owner has said that side is exhausted and the view's edge is the
  model's edge.
* **Grid.** Columns follow the width (`minCellSize`, `cellSpacing`); a change of the column count
  rebuilds the loaded rows around the first visible one, which stays at the same place on the
  screen. How the lines are cut is described under "Month headings". Every resize step of the same
  column count re-requests thumbnails at the new size.
* **Thumbnails** arrive through the `RoundedImageSource`; the cell shows its QSS background until
  then. A source that answers synchronously from `doLoadPixmap()` answers before the consumer is
  connected -- defer the answer (the demo's source uses a 0 ms timer).
* **Updates.** `updateItem()` uses the kind's `ChatObjectRowFactory::update` when it has one (the
  stock rows do); otherwise the row's widget is replaced, which can disturb the scroll position.
* **The model is memory resident** for the life of the browser (see 3). `reload()` drops it.

## 8. For the bridge and the adapter

Nothing in the widgets depends on the bridge. What an adapter has to do:

* **Request -> bridge.** `fetch_chat_objects{chat, kind, anchor, count_before, count_after}` maps one
  to one on `ChatObjectsRequest`: the anchor is `sort_oid` + `ordinal` of `chat_msg_object`
  (`(kind, sort_oid, ordinal)` is the index order), `anchorInclusive` is the bridge's "include the
  anchor". Remember `request.requestId` and answer with it.
* **Keys.** `sortId` must compare bytewise like the index does: hex (or raw bytes) of the ObjectId
  of `sort_oid`, lowercase and fixed width. `ordinal` is `chat_msg_object::ordinal`.
* **End flags.** Set `endBefore` / `endAfter` from the bridge's knowledge ("no more rows in that
  direction"), not from the page being short.
* **Cleared messages.** Architecture doc 3b: a cleared message counts until its cleanup batch
  commits. The object rows of such messages must be **filtered out by the owner** (the message
  behind an object row is read anyway; `isClearedMessage()` is the test) *before* the page is built,
  and the owner must keep reading until it has `count` objects or the end. The browser does not
  filter.
* **Search.** Filter by `request.query` before the keyset logic: the anchor, the counts and the end
  flags all refer to the matching objects. The anchor may be an object that does not match (the text
  changed between two requests); its position among the matching ones is what counts. Fields to
  match are the adapter's call; the sender's name and the date are not in the object rows.
* **Items.** Build `ChatObjectItem` from the object row plus the message: thumbnails through a
  `RoundedImageSource` of the app, `ChatFileItem` from the message's file item (state, size, name,
  voice data), `senderName` and the avatar from the message's sender, `text` as a snippet. Keep it
  short (the rows show one line).
* **Events -> browser.** On `chat_counters_updated`: `reload()` the open browser, or finer:
  `removeMessageObjects(messageId)` for a deleted message and `updateItem()` for a changed file
  state. On `message_objects_indexed` of the chat: `reload()`.
* **Signals -> app.** `itemActivated` of an image/file: open the viewer / the file; of a link: open
  the url; `goToMessageRequested(messageId)`: open the chat at the message;
  `itemActionRequested`: the transfer actions and the voice player.
* **Image viewer "n of N".** Not part of this: the viewer needs the counter of the kind and the
  position of the open image, which the bridge gives.

## 9. Open questions

For Stage 2 (the bridge):

1. Is `sortId` available to the adapter as bytes that compare like the index? If the ObjectId is
   only available as a string with another collation, say so, the key type may need a comparator.
2. Does the bridge filter cleared messages itself (8)? If not, how will the adapter read on until it
   has `count` objects without a second round trip per batch?
3. Can the bridge answer "around a key" (`count_before` and `count_after` in one call), as `openAt`
   needs? If not, the adapter issues two calls and merges them into one `ChatObjectsPage`.
4. How are file states (download progress, local availability) pushed for the file kinds? The
   browsers take them with `updateItem()`; is there a per-file event or must the adapter poll?

For Stage 4 (the dialog and the adapter):

1. The panel emits `backRequested()` only. The dialog decides what is under it (the counters).
2. Titles with a count ("Images (123)") are `ChatObjectBrowserPanel::setTitle()`; the panel never
   learns counts.
3. Voice playback: the stock voice row only reports play/stop/seek. Which player drives it (the one
   of the chat, or a separate one)?
4. Where do the link rows' "copy url" and the file rows' "forward" go -- the context menu builder?
5. Which fields does a search match per kind (file name; url, title and text; message text and sender)?
   Is a search of the message texts local, or does it need the server?
6. Month headings use the local time zone and the locale's month name; is that what the chat's own
   date separators do?
