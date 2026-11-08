# Chat object browsers

Widgets that browse the objects of one chat, one browser per kind of object: a thumbnail grid for
images, lists for files, voice messages, links, mentions and found messages (search results). UI only and data driven: nothing here
reads a database, computes a count or knows the client library. The owner (in the desktop app, a thin
adapter over the bridge) answers the browsers' requests.

Status: **written, not compiled** (Stage 3 of the chat message counters task; the search kind is Stage 4 of
task-search-chat-messages, also written, not compiled).

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
| `chatobjectrows.cpp` | row factories and browser makers of the file, voice, link, mention and search kinds |
| `chatobjectsearchsnippet.hpp/.cpp` | `ChatObjectSearchSnippet` (the highlighted snippet label of a search row) and `chatObjectHighlightHtml()` |
| `chatobjectgallerybrowser.hpp/.cpp` | `ChatObjectGalleryBrowser`, the thumbnail grid |
| `chatobjectbrowserpanel.hpp/.cpp` | `ChatObjectBrowserRegistry`, `ChatObjectBrowserPanel` (back and title bar) |
| `style/chatobjectbrowser.{qss,json}` (+ `light/`, `dark/`) | styles and icon context `ChatObjectBrowser` |

Demo: `demo/chatobjectbrowser` (`chatobjectbrowser-demo`). Tests: `test/chatobjectbrowser`
(`chatobjectbrowser-test`).

## 2. Data types

```cpp
namespace ChatObjectKind { Image="image", File="file", Voice="voice", Link="link", Mention="mention", Search="search" }
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
    QString senderColorKey;     // key in the chat-sender-title palette (sender uid hash), colours the name
    WithPath senderAvatarPath;                    // mention rows
    std::shared_ptr<AvatarSource> avatarSource;   // mention rows
    ChatFileItem file;          // image/file/voice: name, size, mime, state, voice waveform+duration
    WithPath thumbnailPath;     // image cells
    std::shared_ptr<RoundedImageSource> thumbnailSource;   // may resolve later
    QString url, title;         // link
    QString text;               // link, mention and search: snippet of the message
    std::vector<ChatObjectTextRange> matches;   // search: the found words in text, UTF-16 units
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
    QString emptyText;          // why there is nothing to show, instead of "Nothing found"; empty: the browser's own
    QString notice;             // one line under the search line; empty: none
};
```

```cpp
struct ChatObjectTextRange { int start = 0; int length = 0; };   // UTF-16 code units = QString indices
```

**`matches`** (search kind). Ranges into `ChatObjectItem::text`, **in UTF-16 code units, i.e. indices of the
`QString`** (not bytes, not code points: a character outside the BMP counts 2). `text` is the snippet and it is cut by
the **host**: it begins a few characters before the first match, may start with an ellipsis ("…", one unit, which the
ranges count) and is about 120 characters long; the widget cuts nothing. The ranges are taken defensively, so a
slightly wrong list does no harm: an empty list is no highlight; a range with a length of 0 or less or one that begins
after the end of the text is ignored; the rest is clamped to the text; a range never cuts a surrogate pair in two;
overlapping and adjacent ranges are merged, and the order does not matter. Line breaks and tabs of `text` are shown as
spaces **one to one** (never collapsed), so the offsets stay valid.

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
void setQueryRequired(bool);                  // no text, no request: the search kind, see "Query required"
bool isQueryRequired() const;

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

The cancel button of the line (the cross at its right end) appears with the first typed character and goes with the text.
`setSearchCancelAlwaysVisible(true)` keeps it there with no text too: for a browser that is a page of its own (the search
kind in a sidebar), where the owner closes the page when the line is cancelled. A cancel of an **empty** line then emits
`SearchLineEdit::cancelled()` as well (a click on the button, or Escape while the line's own Escape shortcut is on), so a
single click closes the page; with the default (off) a cancel of an empty line does nothing, as before.

#### What the owner tells about an answer

The two texts of `ChatObjectsPage` are for a search whose answer needs more than a list. Both belong to the answer, are taken
only from an **accepted** one (an answer to a superseded request is dropped with its texts) and are forgotten when the
browser is reset (`reload()`, `openAt()`, a change of the text), so they never outlive the answer.

* `emptyText` replaces "Nothing found" (and the hint) **while the browser shows no object**: "Search is not available in this
  chat", "Type at least 3 characters to search", "Searching...". With objects it is not shown.
* `notice` is one line (`#noticeLabel`, plain text, wrapped) under the search line, with or without objects: "Searching..."
  over the results found so far, "Recent messages only (last 90 days)". An empty one hides the line. `noticeText()` returns the
  text shown. Every accepted answer replaces it, so an owner that has a notice sets it on **every** page, edge pages included.

### Query required (the search kind)

`setQueryRequired(true)` makes the browser a "search results" one: it has nothing to show without a text. With an empty
text (trimmed) `reload()` and `openAt()` send **no request**; both ends count as known, so the browser shows its
**empty text** as a hint ("Type to search messages" in the search kind) instead of "Nothing here yet". Typing a text
reloads as usual (after the 300 ms pause). Removing the text, in the line or with `setSearchText("")`, brings the hint
back **at once** (the pause is skipped), forgets the requests in flight (their answers are dropped like any stale
answer) and clears the results. A text that finds nothing still shows "Nothing found", not the hint. Switching the flag
after the first load reloads.

### Search results

`makeChatObjectSearchBrowser()` is a `ChatObjectListBrowser` of the kind "search": no month headings, a click means "go
to message" (as mentions), the search line is shown with the placeholder "Search messages", the query is required.
A row (`makeChatObjectSearchRowFactory()`) is the sender's avatar, a title line with the sender and the date (as the
mention row) and the snippet: a `ChatObjectSearchSnippet`, a `QLabel` with rich text, word wrap and **at most three
lines** (its maximum height; the rest is cut off, nothing is elided) with the ranges of `matches` highlighted.
`chatObjectHighlightHtml(text, ranges, color, background)` builds its HTML and is a pure function (tested on its own):
whitespace replaced one to one, every piece escaped with `toHtmlEscaped()`, a matched piece in a `<span>` with the two
colours. The colours are the QSS properties `highlightColor` and `highlightBackground` of `ChatObjectSearchSnippet`
(`qproperty-` in `light/` and `dark/`); changing either renders the snippet again. With no valid colour a match is
bold.

The owner of the kind answers like for any other: it filters by `request.query` and returns the messages in the keyset
order with the item's `text` cut and `matches` filled. `demo/chatobjectbrowser` has such a fake owner.

## 5. Panel and registry

`ChatObjectBrowserRegistry` maps a kind to `{ title(), emptyText(), factory(parent) }`. The texts are
functions so that they are translated when asked. `ChatObjectBrowserRegistry::defaults()` has image
(the grid), file, voice, link, mention and search (lists). The search kind is titled "Search" and its empty text is the
hint "Type to search messages".

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
  `searchBar` (padding around the line: the line itself must have **no margin**, `LineEdit` puts its icons by the whole rect of the widget), `searchEdit`, `monthHeader` (above a list row, and a heading row `monthRow` of the grid), `emptyLabel`, `linkRow`, `mentionRow`, `searchRow`, `rowIcon`, `rowAvatar`, `textColumn`, `title`, `url`,
  `snippet` (an `ElidedLabel` in a mention row, a `ChatObjectSearchSnippet` in a search row), `meta`, `galleryRow`, `galleryCell` (its background is the **placeholder** shown until
  a thumbnail resolves), `galleryImage`; panel: `header`, `backButton`, `title`, `body`,
  `unknownLabel`. Reused tints only: the chat file row's hover, the reaction chip's tint.
* `uise--ChatObjectSearchSnippet` carries the QSS properties `highlightColor` and `highlightBackground` (`qproperty-`
  in `light/` and `dark/`): the accent of the own-reaction chip and a low-alpha tint of it, no new hue. The search row
  has the hover tint of the mention row.
* Icon context `ChatObjectBrowser` (back, image, file, voice, link, mention, search), all outline tabler
  icons that were already in the curated qrc.
* The file and voice rows are `ChatMessageFileItem` / `ChatVoiceFileItem` and keep their
  `chatmessagefiles.qss` look.
* Strings: `tr()` in the `Q_OBJECT` classes (context `uise::AbstractChatObjectBrowser`; new in the search kind, all in
  `ChatObjectBrowser`: "Search", "Search messages", "Type to search messages");
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
* **Search kind.** One object per found message, `key` from the message's sort position (ordinal 0). The owner cuts the
  snippet (a window of about 120 characters that begins about 20 characters before the first match, at a word, with a
  leading "…" when cut) and fills `matches` for **all** the matches inside the window. If the index works in UTF-8
  byte offsets, convert them to UTF-16 units of the **cut** snippet (count the "…" too) before they are given to the
  widget. No request for an empty text: the browser sends none (`setQueryRequired`), so the owner need not handle it.
* **Items.** Build `ChatObjectItem` from the object row plus the message: thumbnails through a
  `RoundedImageSource` of the app, `ChatFileItem` from the message's file item (state, size, name,
  voice data), `senderName` and the avatar from the message's sender, `senderColorKey` = the sender uid hash (the key the chat bubble
  uses for the same palette), `text` as a snippet. Keep it
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
   *Answered for the search kind:* the message text, and locally (the on-device index of task-search-chat-messages:
   prefix and stem match, all words must match); the server search comes later and is merged by the owner, the widgets
   do not care. The other kinds keep this question open (their search is the adapter's call).
6. Month headings use the local time zone and the locale's month name; is that what the chat's own
   date separators do?
