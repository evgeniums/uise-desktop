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

/** @file uise/desktop/chatobjectitem.hpp
*
*  Declares the data contract of the chat object browsers: ChatObjectKey, ChatObjectItem,
*  ChatObjectsRequest and ChatObjectsPage. See docs/chat-object-browsers.md.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTITEM_HPP
#define UISE_DESKTOP_CHATOBJECTITEM_HPP

#include <cstddef>
#include <memory>
#include <vector>

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatfileitem.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarSource;
class RoundedImageSource;

/**
 * @brief Kinds of chat objects known to the stock browsers.
 *
 * The set of kinds is OPEN: a kind is just a string, a new one (e.g. "video") needs no change
 * here -- only a browser registered for it, see ChatObjectBrowserRegistry.
 */
namespace ChatObjectKind
{
    inline const QString Image=QStringLiteral("image");
    inline const QString File=QStringLiteral("file");
    inline const QString Voice=QStringLiteral("voice");
    inline const QString Link=QStringLiteral("link");
    inline const QString Mention=QStringLiteral("mention");
}

/**
 * @brief Position of an object in the keyset order of its kind. Ascending means older to newer.
 *
 * The key is the object's IDENTITY within a browser and the keyset cursor at the same time: two
 * objects with equal keys are the same object. It is built by the owner of the data from the
 * sort position of the message the object belongs to and the object's ordinal within that
 * message, so the order is stable and total. The widgets never interpret sortId; they only
 * compare it bytewise.
 */
struct ChatObjectKey
{
    //! Opaque, compared bytewise. Typically the hex of the message's sort object id, so that
    //! bytewise order is chronological order. Empty means "no position", see isNull().
    QByteArray sortId;

    //! Tie-break among the objects of one message and kind, 0-based.
    quint32 ordinal=0;

    bool isNull() const noexcept
    {
        return sortId.isEmpty();
    }

    friend bool operator==(const ChatObjectKey& left, const ChatObjectKey& right) noexcept
    {
        return left.ordinal==right.ordinal && left.sortId==right.sortId;
    }

    friend bool operator!=(const ChatObjectKey& left, const ChatObjectKey& right) noexcept
    {
        return !(left==right);
    }

    friend bool operator<(const ChatObjectKey& left, const ChatObjectKey& right) noexcept
    {
        if (left.sortId!=right.sortId)
        {
            return left.sortId<right.sortId;
        }
        return left.ordinal<right.ordinal;
    }

    //! Printable form that is unique per key; used as the widgets' item id.
    QString toString() const
    {
        return QString::fromLatin1(sortId.toHex())+QLatin1Char('.')+QString::number(ordinal);
    }
};

/**
 * @brief One chat object to show: a thumbnail, a file, a voice message, a link, a mention.
 *
 * A plain, cheaply copyable value type (everything heavy is implicitly shared or a shared_ptr),
 * UI-only: it carries what is needed to draw a row or a cell and to report back what was
 * clicked, nothing about how the data is stored. Fields that a kind does not use stay empty.
 */
struct ChatObjectItem
{
    //! One of ChatObjectKind or a kind of the host.
    QString kind;

    //! Keyset position and identity, see ChatObjectKey.
    ChatObjectKey key;

    //! Id of the message the object belongs to, in the host's terms. Reported back with
    //! AbstractChatObjectBrowser::goToMessageRequested() and used to drop all objects of a
    //! message with AbstractChatObjectBrowser::removeMessageObjects().
    QString messageId;

    //! When the message was sent.
    QDateTime dateTime;

    //! Direction of the message; selects Download vs Upload glyph of a not yet transferred file.
    bool incoming=true;

    //! Display name of the sender and the avatar to show for it (mention rows).
    QString senderName;
    WithPath senderAvatarPath;
    std::shared_ptr<AvatarSource> avatarSource;

    //! Image, file and voice kinds: name, size, mime type, transfer state, voice waveform and
    //! duration, ... exactly as the chat message widgets take them.
    ChatFileItem file;

    //! Image kind: thumbnail of the cell. The source may resolve it later; the cell shows a
    //! placeholder until then.
    WithPath thumbnailPath;
    std::shared_ptr<RoundedImageSource> thumbnailSource;

    //! Link kind: the url and, if known, the title of the page.
    QString url;
    QString title;

    //! Link and mention kinds: snippet of the message text the object comes from.
    QString text;

    //! Room for kinds added later, so that they need no change of this struct.
    QVariantMap extra;

    //! Id of the object in the widgets; equals key.toString().
    QString uid() const
    {
        return key.toString();
    }
};

/**
 * @brief Request of a browser for objects around a key. Keyset, never offset.
 *
 * "Before" means older (smaller keys), "after" means newer. The answer must contain at most
 * countBefore objects older than the anchor, nearest first in terms of key, and at most
 * countAfter newer ones. The anchor itself is included only if anchorInclusive is set.
 * A null anchor means "the newest end": the answer is the newest countBefore objects.
 */
struct ChatObjectsRequest
{
    //! Echoed back in ChatObjectsPage::requestId.
    quint64 requestId=0;

    QString kind;

    ChatObjectKey anchor;
    bool anchorInclusive=false;

    size_t countBefore=0;
    size_t countAfter=0;

    /**
     * Search text of the browser, empty when it is not searching. The owner returns only the
     * objects that match it, in the same keyset order; what "match" means is up to the owner and
     * the kind (file names, urls and titles, message texts, senders). The end flags of the answer
     * then refer to the filtered set. Every request of a browser carries the current text.
     */
    QString query;
};

/**
 * @brief Answer to a ChatObjectsRequest, given to AbstractChatObjectBrowser::applyPage().
 */
struct ChatObjectsPage
{
    //! requestId of the request being answered. An unknown or superseded id is dropped.
    quint64 requestId=0;

    //! The objects, in any order; the browser sorts them and drops duplicates by key.
    std::vector<ChatObjectItem> items;

    //! The owner has nothing older than the oldest of items, i.e. the end of history is reached.
    //! Said explicitly because a short answer is not proof of the end: the owner may filter
    //! rows after its database limit.
    bool endBefore=false;

    //! The owner has nothing newer than the newest of items. Implied for a null anchor.
    bool endAfter=false;
};

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::ChatObjectItem)

#endif // UISE_DESKTOP_CHATOBJECTITEM_HPP
