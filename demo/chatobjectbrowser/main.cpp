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

/** @file uise/desktop/demo/chatobjectbrowser/main.cpp
*
*  Demo application of the chat object browsers: the thumbnail grid of images and the lists of
*  files, voice messages, links and mentions in a ChatObjectBrowserPanel, fed by a fake owner that
*  answers the browsers' keyset requests from generated data.
*
*  What to try:
*   - scroll down (requests "before" the oldest loaded object) and, after "Open at middle", up
*     (requests "after" the newest loaded one);
*   - the object count 0 (the empty state) and 5000 (a long list);
*   - the answer delay of 2000 ms (the log must show one request per side at a time);
*   - the thumbnail delay (placeholders first, thumbnails later) in the images;
*   - the months: headings above the first object of a month (a heading is a row of its own in
*     the grid), "Group by month" switches them off and on; after "Open at the middle" scroll up
*     through the month you landed in -- the lines of the grid are cut anew, the place stays;
*   - the search line (the owner filters by the text in the request; try "alice", "12");
*   - the "search" kind (messages found by a text search), see "The search kind" below;
*   - play a voice message (the fake player drives the row);
*   - the "video" kind, registered here and not in the library.
*
*  The search kind
*  ---------------
*  Choose the kind "search" in "Kind of objects". The list stays empty and shows the hint "Type to search messages", and the
*  log shows NO request: a search browser asks the owner for nothing until a text is typed. The objects are fake MESSAGES
*  (the number in "Objects in the chat", 500 by default; the newest is on top). Each is a few English and Russian words
*  from a small vocabulary, see messageText() below; the owner answers by the words you type, and every word you type must
*  be the BEGINNING of some word of the message (all words must be found, upper or lower case does not matter).
*  What to type and what you must see:
*
*   "rep"            many hits: report, reply, repository. Every word that BEGINS with "rep" is highlighted
*                    (the prefix only, "rep" in "report"). Scroll down: more are loaded, the log shows one request at a time.
*   "report"         the same words, the whole of "report" is highlighted.
*   "REPORT"         the same result as "report": the case does not matter.
*   "отч"            Russian: the words "отчёт" and "отчет" (with and without the dotted ё) are found.
*   "отчёт"          only the words with the dotted ё; "отчет" finds only the ones without it. Different letters, different
*                    words.
*   "report tomorrow"  two words: only the messages that have BOTH (a word beginning with "report" and a word beginning
*                    with "tomorrow"), both highlighted. A message with only one of them is not in the list.
*   "and"            a very common word: some messages have it several times, ALL of them are highlighted. Good for scrolling.
*   "the", "и"       the same in English and in Russian.
*   "multiline"      the messages whose text has line breaks and a tab in it (every 7th message, "многострочное" in
*                    every 14th: type "многостр"). They are shown as ONE flowing text, the breaks are spaces, and the
*                    highlight sits exactly on the word, which proves the offsets were not shifted by the replacement.
*   "needle"         a word at the END of a long message (every 13th message has 40 or more words). The snippet
*                    then begins with "…" and some 20 characters before the word, not with the beginning of the message,
*                    and is cut to about 120 characters; at most 3 lines of it are shown, the rest is clipped.
*   "bold"           the messages that end in the literal text  <b>bold</b> & a < b && c > d  (every 11th). It must be
*                    shown AS TYPED, not as bold text and not eaten as markup; "bold" is highlighted inside the tags.
*   "b"              very short query: highlights "b" in words like "budget" and in "<b>" and "</b>".
*   "xyzzy"          nothing is found: the text "Nothing found" (not the hint).
*   "report,"        punctuation is ignored: the same as "report".
*
*  What the owner can tell about the answer (ChatObjectsPage::emptyText and notice). Every answer of the search kind carries
*  the line "Recent messages only (last 90 days)" under the search line. Three texts start with "!" and are a script of the
*  demo, not words of the messages:
*
*   "!unavailable"   no result, and instead of "Nothing found": "Search is not available in this chat".
*   "!short"         no result, and instead of "Nothing found": "Type at least 3 characters to search".
*   "!wait rep"      the result of "rep" and a first line "Searching..." over the period line (the owner's index is still
*                    being built); "!wait" alone has no result and says "Searching..." where "Nothing found" would be.
*
*  Type another text after each: the line and the words of the previous answer are gone at once, and the next answer brings
*  its own. Clearing the text hides the line and brings the hint back.
*
*  Then: clear the text (the cancel icon on the right, or select all and delete). The hint comes back at once, the log
*  shows no new request, and answers that are still on their way are not shown. To see that, choose "Answer in 2000 ms",
*  type "rep", and clear the text before the answer comes (within 2 seconds): the log shows the request and, later, its
*  answer, but the list stays empty and the hint stays. Also with 2000 ms: type "rep", wait until the log shows the request
*  for 'rep' (the search starts about half a second after the last key), then add "o" (the line is now "repo") before the
*  answer to "rep" arrives: only the result of "repo" is shown (the stale answer is dropped).
*  Click a row: the log shows "go to message" with the id of the message. Right click: the menu has "Go to message" and the
*  demo's own entry.
*  Switch "Dark theme" on and off to see the highlight colours of both themes. "Group by month" and "Show the search line"
*  do not apply to this kind: it has no month headings and always has its own search line.
*
*/

/****************************************************************************/

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QChar>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/chataudiofileitems.hpp>
#include <uise/desktop/chatobjectbrowserpanel.hpp>
#include <uise/desktop/chatobjectlistbrowser.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

namespace {

//! Images come in albums of this many objects per message, so that the ordinal tie-break of the
//! keys is exercised; the other kinds have one object per message.
constexpr size_t ImagesPerMessage=3;

size_t perMessage(const QString& kind)
{
    return kind==ChatObjectKind::Image ? ImagesPerMessage : 1;
}

//! Key of the n-th oldest object of the kind: zero padded decimal, so that bytewise order is
//! numeric order.
ChatObjectKey keyOf(const QString& kind, size_t n)
{
    const auto per=perMessage(kind);
    ChatObjectKey key;
    key.sortId=QByteArray::number(static_cast<qulonglong>(n/per)).rightJustified(8,'0');
    key.ordinal=static_cast<quint32>(n%per);
    return key;
}

size_t indexOfKey(const QString& kind, const ChatObjectKey& key)
{
    return static_cast<size_t>(key.sortId.toULongLong())*perMessage(kind)+key.ordinal;
}

//--------------------------------------------------------------------------

//! Thumbnails drawn on demand, after a delay, so that the placeholders can be seen.
class DemoThumbnailSource : public RoundedImageSource
{
    public:

        void setDelayMs(int value) noexcept
        {
            m_delayMs=value;
        }

    protected:

        void doLoadPixmap(const PixmapKey& key) override
        {
            // Always deferred, even for 0 ms: a source that answers from inside doLoadPixmap()
            // answers before the consumer has connected.
            std::weak_ptr<PixmapSource> weak=weak_from_this();
            QTimer::singleShot(m_delayMs,qApp,
                [weak,key]()
                {
                    if (auto source=weak.lock())
                    {
                        static_cast<DemoThumbnailSource*>(source.get())->deliver(key);
                    }
                }
            );
        }

    private:

        void deliver(const PixmapKey& key)
        {
            const auto size=key.size();
            if (!size.isValid() || key.path().empty())
            {
                return;
            }

            const auto n=std::stoull(key.path().front());

            QPixmap pixmap(size);
            pixmap.fill(QColor::fromHsv(static_cast<int>((n*37)%360),140,200));

            QPainter painter(&pixmap);
            painter.setPen(Qt::white);
            QFont font=painter.font();
            font.setPixelSize(std::max(10,size.height()/4));
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(pixmap.rect(),Qt::AlignCenter,QString::number(n));
            painter.end();

            updatePixmap(key,pixmap);
        }

        int m_delayMs=0;
};

struct DemoAvatarBuilder
{
    std::shared_ptr<Avatar> operator() (const WithPath& path) const
    {
        auto avatar=std::make_shared<Avatar>();
        if (!path.path().empty())
        {
            avatar->setAvatarName(path.path().front());
        }
        return avatar;
    }
};

//--------------------------------------------------------------------------

//! Words of the fake messages of the "search" kind: English and Russian, some with a common beginning (rep...) to
//! try the prefix match.
const QStringList& wordPool()
{
    static const QStringList words{
        QStringLiteral("report"), QStringLiteral("reply"), QStringLiteral("repository"), QStringLiteral("release"),
        QStringLiteral("meeting"), QStringLiteral("tomorrow"), QStringLiteral("deploy"), QStringLiteral("server"),
        QStringLiteral("please"), QStringLiteral("check"), QStringLiteral("review"), QStringLiteral("message"),
        QStringLiteral("the"), QStringLiteral("and"), QStringLiteral("with"), QStringLiteral("about"),
        QStringLiteral("Monday"), QStringLiteral("budget"), QStringLiteral("photo"), QStringLiteral("invoice"),
        QStringLiteral("файл"), QStringLiteral("отчёт"), QStringLiteral("отчет"), QStringLiteral("встреча"),
        QStringLiteral("завтра"), QStringLiteral("сервер"), QStringLiteral("релиз"), QStringLiteral("проверь"),
        QStringLiteral("пожалуйста"), QStringLiteral("сообщение"), QStringLiteral("ответ"), QStringLiteral("репозиторий"),
        QStringLiteral("и"), QStringLiteral("для"), QStringLiteral("понедельник"), QStringLiteral("договор")
    };
    return words;
}

//! A fake message of the "search" kind: from one word to about sixty (most are short), English and Russian words of wordPool()
//! mixed. Deterministic, so that the same index is the same text. On top of the random words there are LANDMARKS, so that a
//! person who tries the demo has something certain to type (the list is in the header of this file):
//!  - every 7th message (index n, n%7==0) has at least 7 words, line breaks after its 2nd and 5th word, a tab after the 3rd
//!    and the word "multiline" ("многострочное" if n%14==0) as its 2nd word: type "multiline";
//!  - every 11th has the text " <b>bold</b> & a < b && c > d" at its end: type "bold";
//!  - every 13th has at least 40 words and the word "needle" at its end (before the " <b>bold</b>..." tail if it has one):
//!    type "needle".
QString messageText(size_t n)
{
    quint32 state=static_cast<quint32>(n)*2654435761u+12345u;
    auto next=[&state](quint32 modulo)
    {
        state=state*1664525u+1013904223u;
        return (state>>8)%modulo;
    };

    const auto& pool=wordPool();
    int count=1+static_cast<int>(next(4)==0 ? next(60) : next(14));
    if (n%7==0)
    {
        count=std::max(count,7);
    }
    if (n%13==0)
    {
        count=std::max(count,40);
    }

    QString text;
    for (int i=0; i<count; ++i)
    {
        if (i>0)
        {
            if (n%7==0 && (i==2 || i==5))
            {
                text+=(i==2) ? QStringLiteral("\r\n") : QStringLiteral("\n");
            }
            else if (n%7==0 && i==3)
            {
                text+=QLatin1Char('\t');
            }
            else
            {
                text+=QLatin1Char(' ');
            }
        }
        const auto& word=pool[static_cast<int>(next(static_cast<quint32>(pool.size())))];
        // the random draw above is made in any case, so that the other words of a message do not depend on the landmarks
        if (n%7==0 && i==1)
        {
            text+=(n%14==0) ? QStringLiteral("многострочное") : QStringLiteral("multiline");
        }
        else
        {
            text+=word;
        }
        if (next(9)==0)
        {
            text+=QLatin1Char(',');
        }
    }
    if (n%13==0)
    {
        text+=QStringLiteral(" needle");
    }
    if (n%11==0)
    {
        text+=QStringLiteral(" <b>bold</b> & a < b && c > d");
    }
    return text;
}

//! The words of a text, as the search takes them: runs of letters and digits.
std::vector<std::pair<int,int>> wordSpans(const QString& text)
{
    std::vector<std::pair<int,int>> spans;
    int i=0;
    while (i<text.size())
    {
        if (!text.at(i).isLetterOrNumber())
        {
            ++i;
            continue;
        }
        const int start=i;
        while (i<text.size() && text.at(i).isLetterOrNumber())
        {
            ++i;
        }
        spans.emplace_back(start,i);
    }
    return spans;
}

//! The words of the query, lower case.
QStringList queryWords(const QString& query)
{
    QStringList words;
    for (const auto& span : wordSpans(query))
    {
        words.push_back(query.mid(span.first,span.second-span.first).toLower());
    }
    return words;
}

//! Whether the word of the text [start,end) begins with the (lower case) word, ignoring case. By character: case folding of
//! the whole text could change its length and with it the offsets.
bool hasPrefix(const QString& text, int start, int end, const QString& word)
{
    if (word.size()>end-start)
    {
        return false;
    }
    for (int i=0; i<word.size(); ++i)
    {
        if (text.at(start+i).toLower()!=word.at(i))
        {
            return false;
        }
    }
    return true;
}

//! The found words of a message: every word of the query must be the beginning of some word of the text (AND). All words
//! of the text that begin with a word of the query are returned as ranges of the beginning, in UTF-16 units. Empty if the
//! message does not match.
std::vector<ChatObjectTextRange> findMatches(const QString& text, const QStringList& words)
{
    std::vector<ChatObjectTextRange> hits;
    if (words.isEmpty())
    {
        return hits;
    }

    std::vector<bool> found(static_cast<size_t>(words.size()),false);
    for (const auto& span : wordSpans(text))
    {
        int longest=0;
        for (int w=0; w<words.size(); ++w)
        {
            if (hasPrefix(text,span.first,span.second,words[w]))
            {
                found[static_cast<size_t>(w)]=true;
                longest=std::max(longest,static_cast<int>(words[w].size()));
            }
        }
        if (longest>0)
        {
            hits.push_back(ChatObjectTextRange{span.first,longest});
        }
    }

    if (std::find(found.begin(),found.end(),false)!=found.end())
    {
        hits.clear();
    }
    return hits;
}

//! What a real owner has to do for a search result: a window of about 120 characters that begins about 20 characters before the
//! first match, at a word; "..." in front of it when the text is cut there; the ranges of ALL the matches inside the window.
void fillSnippet(ChatObjectItem& item, const QString& text, const std::vector<ChatObjectTextRange>& hits)
{
    constexpr int Before=20;
    constexpr int Length=120;

    if (hits.empty())
    {
        item.text=text.left(Length);
        return;
    }

    auto isWordChar=[&text](int pos)
    {
        return pos>=0 && pos<text.size() && text.at(pos).isLetterOrNumber();
    };

    const int first=hits.front().start;
    int firstWordEnd=first+hits.front().length;
    while (isWordChar(firstWordEnd))
    {
        ++firstWordEnd;
    }

    // the beginning: not in the middle of a word, and not after the first match
    int start=std::max(0,first-Before);
    if (start>0)
    {
        while (start<first && isWordChar(start-1) && isWordChar(start))
        {
            ++start;
        }
        while (start<first && !isWordChar(start))
        {
            ++start;
        }
    }

    // the end: not in the middle of a word, and not before the end of the first match's word
    int end=std::min(static_cast<int>(text.size()),start+Length);
    if (end<text.size())
    {
        while (end>firstWordEnd && isWordChar(end-1) && isWordChar(end))
        {
            --end;
        }
        while (end>firstWordEnd && !isWordChar(end-1))
        {
            --end;
        }
    }

    const QString prefix=start>0 ? QString(QChar(0x2026)) : QString{};
    item.text=prefix+text.mid(start,end-start);
    for (const auto& hit : hits)
    {
        const int from=std::max(hit.start,start);
        const int to=std::min(hit.start+hit.length,end);
        if (from<to)
        {
            item.matches.push_back(ChatObjectTextRange{from-start+static_cast<int>(prefix.size()),to-from});
        }
    }
}

//--------------------------------------------------------------------------

//! The fake owner of the data: generates the objects of every kind from their index and answers
//! the keyset requests the way a real owner has to.
struct DemoOwner
{
    size_t count=500;
    std::shared_ptr<DemoThumbnailSource> thumbnails;
    std::shared_ptr<AvatarSource> avatars;
    std::vector<QString> senders{QStringLiteral("Alice"),QStringLiteral("Bob"),QStringLiteral("Carol"),QStringLiteral("Dan"),QStringLiteral("Erin")};

    ChatObjectItem makeItem(const QString& kind, size_t n, const QString& query=QString{}) const
    {
        static const QDateTime base=QDateTime::currentDateTime();

        ChatObjectItem item;
        item.kind=kind;
        item.key=keyOf(kind,n);
        item.messageId=QStringLiteral("msg-%1").arg(n/perMessage(kind));
        item.dateTime=base.addSecs(-static_cast<qint64>(count-1-n)*28800);
        item.incoming=(n%3)!=0;
        item.senderName=senders[n%senders.size()];
        item.senderColorKey=item.senderName;
        item.file.setFileId(QString::number(n));
        item.file.setState(ChatFileTransferState::Ready);

        if (kind==ChatObjectKind::Image)
        {
            item.thumbnailPath=WithPath(std::to_string(n));
            item.thumbnailSource=thumbnails;
            item.file.setFileName(QStringLiteral("IMG_%1.jpg").arg(n));
            item.file.setMimeType(QStringLiteral("image/jpeg"));
        }
        else if (kind==ChatObjectKind::File)
        {
            static const char* extensions[]={"pdf","zip","txt","docx","xlsx"};
            item.file.setFileName(QStringLiteral("document-%1.%2").arg(n).arg(QLatin1String(extensions[n%5])));
            item.file.setSize(static_cast<qint64>((n*7919)%5000+1)*1024);
        }
        else if (kind==ChatObjectKind::Voice)
        {
            QByteArray waveform(100,0);
            for (int i=0; i<waveform.size(); ++i)
            {
                waveform[i]=static_cast<char>((n*31+static_cast<size_t>(i)*17)%256);
            }
            item.file.setFileName(QStringLiteral("voice-%1.ogg").arg(n));
            item.file.setVoice(true);
            item.file.setVoiceWaveform(waveform);
            item.file.setVoiceDurationMs(static_cast<quint32>(3000+(n*1731)%90000));
            item.file.setSize(static_cast<qint64>((n*977)%400+20)*1024);
            item.file.setListenedByPeer(n%2==0);
        }
        else if (kind==ChatObjectKind::Link)
        {
            item.url=QStringLiteral("https://example.com/articles/%1").arg(n);
            if (n%3!=0)
            {
                item.title=QStringLiteral("Article number %1: a rather long title that has to be elided somewhere on the way").arg(n);
            }
            item.text=QStringLiteral("Look at this, it is really interesting: %1 and some more words after the link").arg(item.url);
        }
        else if (kind==ChatObjectKind::Mention)
        {
            item.avatarSource=avatars;
            item.senderAvatarPath=WithPath(item.senderName.toStdString());
            item.text=QStringLiteral("@me please have a look at number %1, it is long enough to be elided at the end of the line").arg(n);
        }
        else if (kind==ChatObjectKind::Search)
        {
            item.avatarSource=avatars;
            item.senderAvatarPath=WithPath(item.senderName.toStdString());
            fillSnippet(item,messageText(n),findMatches(messageText(n),queryWords(query)));
        }

        return item;
    }

    //! What the demo searches in: the sender and the number.
    QString labelOf(size_t n) const
    {
        return QStringLiteral("%1 %2").arg(senders[n%senders.size()]).arg(n);
    }

    //! Indices of the objects that match the request's search text, ascending.
    std::vector<size_t> matching(const ChatObjectsRequest& request) const
    {
        std::vector<size_t> ids;
        ids.reserve(count);

        if (request.kind==ChatObjectKind::Search)
        {
            // the words of the query, all of them, as the beginnings of words of the message text; no text, no result
            const auto words=queryWords(request.query);
            if (words.isEmpty())
            {
                return ids;
            }
            for (size_t n=0; n<count; ++n)
            {
                if (!findMatches(messageText(n),words).empty())
                {
                    ids.push_back(n);
                }
            }
            return ids;
        }

        for (size_t n=0; n<count; ++n)
        {
            if (request.query.isEmpty() || labelOf(n).contains(request.query,Qt::CaseInsensitive))
            {
                ids.push_back(n);
            }
        }
        return ids;
    }

    ChatObjectsPage answer(const ChatObjectsRequest& request) const
    {
        if (request.kind!=ChatObjectKind::Search)
        {
            return answerObjects(request);
        }

        // The states of the search that the owner tells beside the objects: why there are none, and a line over the list.
        const QString text=request.query.trimmed();
        if (text==QLatin1String("!unavailable") || text==QLatin1String("!short"))
        {
            ChatObjectsPage page;
            page.requestId=request.requestId;
            page.endBefore=true;
            page.endAfter=true;
            page.emptyText=(text==QLatin1String("!unavailable"))
                               ? QStringLiteral("Search is not available in this chat")
                               : QStringLiteral("Type at least 3 characters to search");
            return page;
        }

        auto effective=request;
        QStringList notice;
        if (text.startsWith(QLatin1String("!wait")))
        {
            effective.query=text.mid(5).trimmed();
            notice.push_back(QStringLiteral("Searching..."));
        }
        auto page=answerObjects(effective);
        if (page.items.empty() && !notice.isEmpty())
        {
            page.emptyText=QStringLiteral("Searching...");
        }
        notice.push_back(QStringLiteral("Recent messages only (last 90 days)"));
        page.notice=notice.join(QLatin1Char('\n'));
        return page;
    }

    //! The objects of the request, by its search text.
    ChatObjectsPage answerObjects(const ChatObjectsRequest& request) const
    {
        ChatObjectsPage page;
        page.requestId=request.requestId;

        // the keyset order is over the objects that match; the end flags are about them
        const auto ids=matching(request);
        const size_t total=ids.size();

        if (total==0)
        {
            page.endBefore=true;
            page.endAfter=true;
            return page;
        }

        if (request.anchor.isNull())
        {
            // the newest end
            const size_t first=total>request.countBefore ? total-request.countBefore : 0;
            for (size_t i=first; i<total; ++i)
            {
                page.items.push_back(makeItem(request.kind,ids[i],request.query));
            }
            page.endBefore=(first==0);
            page.endAfter=true;
            return page;
        }

        // the anchor may not match (the search text changed, the data became shorter): its place
        // among the matching objects is what counts
        const size_t anchorIdx=indexOfKey(request.kind,request.anchor);
        const size_t pos=static_cast<size_t>(std::lower_bound(ids.begin(),ids.end(),anchorIdx)-ids.begin());
        const bool exact=pos<total && ids[pos]==anchorIdx;

        const size_t lo=pos>request.countBefore ? pos-request.countBefore : 0;
        const size_t afterStart=exact ? pos+1 : pos;
        const size_t hi=std::min(total,afterStart+request.countAfter);

        for (size_t i=lo; i<pos; ++i)
        {
            page.items.push_back(makeItem(request.kind,ids[i],request.query));
        }
        if (request.anchorInclusive && exact)
        {
            page.items.push_back(makeItem(request.kind,ids[pos],request.query));
        }
        for (size_t i=afterStart; i<hi; ++i)
        {
            page.items.push_back(makeItem(request.kind,ids[i],request.query));
        }

        page.endBefore=(request.countBefore>0 && lo==0);
        page.endAfter=(request.countAfter>0 && hi==total);
        return page;
    }
};

//--------------------------------------------------------------------------

//! A row of a kind that exists only in this demo, to show that a kind is registered, not wired in.
class DemoVideoRow : public QFrame
{
    public:

        DemoVideoRow(const ChatObjectItem& item, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_item(item),
              m_hooks(hooks)
        {
            auto* layout=Layout::horizontal(this,false);
            layout->addWidget(
                new QLabel(
                    QStringLiteral("Video clip of %1: a kind registered by the demo, not by the library").arg(item.messageId),
                    this
                )
            );
            setCursor(Qt::PointingHandCursor);
        }

    protected:

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            QFrame::mouseReleaseEvent(event);
            if (m_hooks.activated)
            {
                const ChatObjectItem copy=m_item;
                m_hooks.activated(copy);
            }
        }

    private:

        ChatObjectItem m_item;
        ChatObjectRowHooks m_hooks;
};

//! State of the fake voice player.
struct DemoPlayer
{
    ChatObjectItem item;
    qint64 positionMs=0;
    bool active=false;
};

} // anonymous namespace

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    DemoOwner owner;
    owner.thumbnails=std::make_shared<DemoThumbnailSource>();
    owner.avatars=std::make_shared<AvatarSource>();
    owner.avatars->setAvatarBuilder(DemoAvatarBuilder{});

    QMainWindow w;
    w.setWindowTitle("Chat Object Browsers Demo");

    auto* root=new QFrame(&w);
    root->setObjectName("demoRoot");
    auto* rootLayout=Layout::horizontal(root,false);

    // --- the panel with the stock kinds plus one of the demo's own ---

    auto* panel=new ChatObjectBrowserPanel(root);
    panel->setMinimumSize(420,520);

    auto registry=ChatObjectBrowserRegistry::defaults();
    registry.registerKind(
        QStringLiteral("video"),
        ChatObjectBrowserKindInfo{
            []()
            {
                return QStringLiteral("Videos");
            },
            []()
            {
                return QStringLiteral("No videos in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                ChatObjectRowFactory factory;
                // no updater on purpose: updateItem() then replaces the row's widget
                factory.build=[](const ChatObjectItem& item, QWidget* rowParent, const ChatObjectRowHooks& hooks) -> QFrame*
                {
                    return new DemoVideoRow(item,hooks,rowParent);
                };
                return new ChatObjectListBrowser(QStringLiteral("video"),std::move(factory),parent);
            }
        }
    );
    const auto kinds=registry.kinds();
    panel->setRegistry(std::move(registry));
    rootLayout->addWidget(panel,1);

    // --- controls and the log ---

    auto* side=new QFrame(root);
    side->setFixedWidth(340);
    auto* sideLayout=Layout::vertical(side,false);
    rootLayout->addWidget(side);

    auto* log=new QPlainTextEdit(side);
    log->setReadOnly(true);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    auto* kindBox=new QComboBox(side);
    for (const auto& kind : kinds)
    {
        kindBox->addItem(kind,kind);
    }
    sideLayout->addWidget(new QLabel("Kind of objects",side));
    sideLayout->addWidget(kindBox);

    auto* countBox=new QSpinBox(side);
    countBox->setRange(0,20000);
    countBox->setValue(static_cast<int>(owner.count));
    countBox->setKeyboardTracking(false);
    sideLayout->addWidget(new QLabel("Objects in the chat (0 shows the empty state)",side));
    sideLayout->addWidget(countBox);

    auto* presets=new QFrame(side);
    auto* presetsLayout=Layout::horizontal(presets,false);
    for (int preset : {0,5,40,500,5000})
    {
        auto* button=new QPushButton(QString::number(preset),presets);
        QObject::connect(button,&QPushButton::clicked,countBox,
            [countBox,preset]()
            {
                countBox->setValue(preset);
            }
        );
        presetsLayout->addWidget(button);
    }
    sideLayout->addWidget(presets);

    auto* delayBox=new QComboBox(side);
    delayBox->addItem("Answer at once",0);
    delayBox->addItem("Answer in 300 ms",300);
    delayBox->addItem("Answer in 2000 ms",2000);
    delayBox->setCurrentIndex(1);
    sideLayout->addWidget(delayBox);

    auto* thumbBox=new QComboBox(side);
    thumbBox->addItem("Thumbnails at once",0);
    thumbBox->addItem("Thumbnails in 500 ms",500);
    thumbBox->addItem("Thumbnails in 3000 ms",3000);
    thumbBox->setCurrentIndex(1);
    sideLayout->addWidget(thumbBox);

    auto* reloadButton=new QPushButton("Reload (from the newest)",side);
    auto* middleButton=new QPushButton("Open at the middle",side);
    auto* deleteButton=new QPushButton("Delete a message (patches the browser)",side);
    auto* groupBox=new QCheckBox("Group by month",side);
    groupBox->setChecked(true);
    auto* searchBox=new QCheckBox("Show the search line (matches the sender and the number; the search kind has its own)",side);
    auto* darkBox=new QCheckBox("Dark theme",side);
    sideLayout->addWidget(reloadButton);
    sideLayout->addWidget(middleButton);
    sideLayout->addWidget(deleteButton);
    sideLayout->addWidget(groupBox);
    sideLayout->addWidget(searchBox);
    sideLayout->addWidget(darkBox);
    sideLayout->addWidget(log,1);

    // --- the fake voice player ---

    DemoPlayer player;
    auto* playTimer=new QTimer(&w);
    playTimer->setInterval(100);

    auto voiceRow=[panel,&player]() -> ChatVoiceFileItem*
    {
        auto* browser=qobject_cast<ChatObjectListBrowser*>(panel->currentBrowser());
        if (browser==nullptr)
        {
            return nullptr;
        }
        return qobject_cast<ChatVoiceFileItem*>(browser->rowWidget(player.item.key));
    };

    auto showProgress=[&player,voiceRow]()
    {
        auto* row=voiceRow();
        if (row==nullptr)
        {
            return;
        }
        const qint64 duration=player.item.file.voiceDurationMs();
        row->setPlaybackProgress(duration>0 ? static_cast<qreal>(player.positionMs)/static_cast<qreal>(duration) : 0.0);
        row->setPlaybackPosition(player.positionMs,duration);
    };

    auto stopPlayer=[&player,playTimer,panel,voiceRow]()
    {
        if (!player.active)
        {
            return;
        }
        player.active=false;
        playTimer->stop();
        player.item.file.setPlaying(false);
        if (auto* browser=panel->currentBrowser())
        {
            browser->updateItem(player.item);
        }
        if (auto* row=voiceRow())
        {
            row->setPlaybackProgress(0.0);
            row->setPlaybackPosition(-1,player.item.file.voiceDurationMs());
        }
    };

    QObject::connect(playTimer,&QTimer::timeout,&w,
        [&player,showProgress,stopPlayer]()
        {
            player.positionMs+=100;
            if (player.positionMs>=static_cast<qint64>(player.item.file.voiceDurationMs()))
            {
                stopPlayer();
                return;
            }
            showProgress();
        }
    );

    // --- the owner's side of the requests ---

    panel->setRequestCb(
        [&owner,panel,delayBox,logMsg](const ChatObjectsRequest& request)
        {
            logMsg(
                QStringLiteral("request #%1 %2: anchor=%3%4 before=%5 after=%6 query='%7'")
                    .arg(request.requestId)
                    .arg(request.kind)
                    .arg(request.anchor.isNull() ? QStringLiteral("(newest end)") : QString::fromLatin1(request.anchor.sortId)+QLatin1Char('.')+QString::number(request.anchor.ordinal))
                    .arg(request.anchorInclusive ? QStringLiteral(" inclusive") : QString{})
                    .arg(request.countBefore)
                    .arg(request.countAfter)
                    .arg(request.query)
            );

            QPointer<ChatObjectBrowserPanel> guard(panel);
            QTimer::singleShot(delayBox->currentData().toInt(),panel,
                [&owner,guard,request,logMsg]()
                {
                    if (guard.isNull() || guard->currentBrowser()==nullptr)
                    {
                        return;
                    }
                    const auto page=owner.answer(request);
                    logMsg(
                        QStringLiteral("  answer #%1: %2 items, endBefore=%3 endAfter=%4")
                            .arg(page.requestId)
                            .arg(page.items.size())
                            .arg(page.endBefore ? QStringLiteral("yes") : QStringLiteral("no"))
                            .arg(page.endAfter ? QStringLiteral("yes") : QStringLiteral("no"))
                    );
                    // an answer to a browser that is gone is dropped by the id
                    guard->currentBrowser()->applyPage(page);
                }
            );
        }
    );

    panel->setContextMenuBuilder(
        [](const ChatObjectItem&)
        {
            std::vector<MenuItem> items;
            items.emplace_back(AbstractChatObjectBrowser::MenuFirstCustomId,QStringLiteral("Copy object id (the demo's own entry)"));
            return items;
        }
    );

    // --- what the browsers say ---

    QObject::connect(panel,&ChatObjectBrowserPanel::itemActivated,&w,
        [logMsg](const ChatObjectItem& item)
        {
            logMsg(QStringLiteral("activated: %1 %2 (%3)").arg(item.kind,item.uid(),item.messageId));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::goToMessageRequested,&w,
        [logMsg](const QString& messageId)
        {
            logMsg(QStringLiteral("go to message: %1").arg(messageId));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::contextMenuTriggered,&w,
        [logMsg](const ChatObjectItem& item, int id)
        {
            logMsg(QStringLiteral("menu entry %1 for %2").arg(id).arg(item.uid()));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::backRequested,&w,
        [logMsg]()
        {
            logMsg(QStringLiteral("back requested (a real owner leaves the browser here)"));
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::itemActionRequested,&w,
        [&player,playTimer,panel,logMsg,stopPlayer,showProgress](const ChatObjectItem& item, const QString& action, const QVariant& argument)
        {
            logMsg(QStringLiteral("action %1 for %2").arg(action,item.uid()));

            if (action==ChatObjectAction::Play)
            {
                stopPlayer();
                player.item=item;
                player.item.file.setPlaying(true);
                player.positionMs=0;
                player.active=true;
                if (auto* browser=panel->currentBrowser())
                {
                    browser->updateItem(player.item);
                }
                playTimer->start();
            }
            else if (action==ChatObjectAction::Stop)
            {
                stopPlayer();
            }
            else if (action==ChatObjectAction::Seek)
            {
                if (player.active && item.key==player.item.key)
                {
                    player.positionMs=static_cast<qint64>(argument.toDouble()*player.item.file.voiceDurationMs());
                    showProgress();
                }
            }
        }
    );

    // --- the controls ---

    auto openKind=[&owner,panel,kindBox,thumbBox,groupBox,searchBox,stopPlayer,logMsg](const ChatObjectKey& anchor)
    {
        stopPlayer();
        owner.thumbnails->setDelayMs(thumbBox->currentData().toInt());
        logMsg(QStringLiteral("--- open %1 ---").arg(kindBox->currentData().toString()));
        panel->open(kindBox->currentData().toString(),anchor);
        // the options are the demo's, a new browser starts with the defaults
        if (auto* browser=panel->currentBrowser())
        {
            if (browser->kind()==ChatObjectKind::Search)
            {
                // its search line and its (missing) month headings are the kind's own; the options are for the others
                browser->focusSearch();
            }
            else
            {
                browser->setGroupByMonth(groupBox->isChecked());
                browser->setSearchVisible(searchBox->isChecked());
            }
        }
    };

    QObject::connect(kindBox,&QComboBox::currentIndexChanged,&w,
        [openKind](int)
        {
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(countBox,&QSpinBox::valueChanged,&w,
        [&owner,openKind](int value)
        {
            owner.count=static_cast<size_t>(value);
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(reloadButton,&QPushButton::clicked,&w,
        [openKind]()
        {
            openKind(ChatObjectKey{});
        }
    );
    QObject::connect(middleButton,&QPushButton::clicked,&w,
        [&owner,kindBox,openKind]()
        {
            if (owner.count==0)
            {
                return;
            }
            openKind(keyOf(kindBox->currentData().toString(),owner.count/2));
        }
    );
    QObject::connect(deleteButton,&QPushButton::clicked,&w,
        [panel,logMsg]()
        {
            auto* browser=panel->currentBrowser();
            if (browser==nullptr || browser->model().empty())
            {
                return;
            }
            const auto& items=browser->model().items();
            const auto messageId=items[items.size()/2].messageId;
            logMsg(QStringLiteral("removing the objects of %1 from the browser").arg(messageId));
            browser->removeMessageObjects(messageId);
        }
    );
    QObject::connect(groupBox,&QCheckBox::toggled,&w,
        [panel](bool enable)
        {
            auto* browser=panel->currentBrowser();
            if (browser!=nullptr && browser->kind()!=ChatObjectKind::Search)
            {
                browser->setGroupByMonth(enable);
            }
        }
    );
    QObject::connect(searchBox,&QCheckBox::toggled,&w,
        [panel](bool enable)
        {
            auto* browser=panel->currentBrowser();
            if (browser!=nullptr && browser->kind()!=ChatObjectKind::Search)
            {
                browser->setSearchVisible(enable);
                if (enable)
                {
                    browser->focusSearch();
                }
            }
        }
    );
    QObject::connect(panel,&ChatObjectBrowserPanel::searchTextChanged,&w,
        [logMsg](const QString& text)
        {
            logMsg(QStringLiteral("search text in effect: '%1'").arg(text));
        }
    );
    QObject::connect(darkBox,&QCheckBox::toggled,&w,
        [](bool dark)
        {
            Style::instance().setBaseQss(
                dark ? QStringLiteral("QMainWindow, QFrame#demoRoot { background-color: #1E1E1E; }")
                     : QStringLiteral("QMainWindow, QFrame#demoRoot { background-color: #FFFFFF; }")
            );
            Style::instance().setColorTheme(dark ? Style::DarkTheme : Style::LightTheme);
            Style::instance().applyStyleSheet(true);
        }
    );

    w.setCentralWidget(root);
    w.resize(900,720);
    w.show();

    // the first kind of the registry is open from the start
    openKind(ChatObjectKey{});

    return app.exec();
}
