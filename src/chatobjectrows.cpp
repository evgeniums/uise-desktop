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

/** @file uise/desktop/src/chatobjectrows.cpp
*
*  Defines the row factories of the stock kinds of ChatObjectListBrowser: files, voice messages,
*  links and mentions.
*
*/

/****************************************************************************/

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QLocale>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStringList>

#include <uise/desktop/chatobjectlistbrowser.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>
#include <uise/desktop/chataudiofileitems.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/utils/filesizeformat.hpp>

// Nothing in this file is a Q_OBJECT class, so no tr() here: the few strings go through
// QCoreApplication::translate() with the "ChatObjectBrowser" context.
namespace uise {

namespace {

//! "Today, 14:05" / "Oct 12, 14:05" / "Oct 12 2024, 14:05". Empty for an invalid date.
QString objectDateText(const QDateTime& dateTime)
{
    if (!dateTime.isValid())
    {
        return QString{};
    }
    const bool withYear=dateTime.date().year()!=QDate::currentDate().year();
    const auto date=chatDateLabel(dateTime,withYear);
    const auto time=QLocale().toString(dateTime.time(),QLocale::ShortFormat);
    return QCoreApplication::translate("ChatObjectBrowser","%1, %2").arg(date,time);
}

//! Join up to three parts with a separator, leaving out the empty ones.
QString joinMeta(const QString& first, const QString& second, const QString& third=QString{})
{
    QStringList parts;
    for (const auto* part : {&first,&second,&third})
    {
        if (!part->isEmpty())
        {
            parts.push_back(*part);
        }
    }

    switch (parts.size())
    {
        case 0:
            return QString{};
        case 1:
            return parts[0];
        case 2:
            return QCoreApplication::translate("ChatObjectBrowser","%1 · %2").arg(parts[0],parts[1]);
        default:
            break;
    }
    return QCoreApplication::translate("ChatObjectBrowser","%1 · %2 · %3").arg(parts[0],parts[1],parts[2]);
}

//! Whitespace and line breaks of a snippet collapsed: the rows show one line of it.
QString oneLine(const QString& text)
{
    return text.simplified();
}

// The hooks are called with a COPY of the row's item: a handler may reload or update the
// browser, and the item must not change under it.

void callActivated(const ChatObjectRowHooks& hooks, const ChatObjectItem& object)
{
    if (hooks.activated)
    {
        const ChatObjectItem copy=object;
        hooks.activated(copy);
    }
}

void callAction(const ChatObjectRowHooks& hooks, const ChatObjectItem& object, const QString& action, const QVariant& argument=QVariant{})
{
    if (hooks.action)
    {
        const ChatObjectItem copy=object;
        hooks.action(copy,action,argument);
    }
}

//--------------------------------------------------------------------------

/**
 * @brief The file row of the chat message, with "size · date · sender" in its info line.
 *
 * Nothing in ChatMessageFileItem depends on a chat bubble; what a list row has to take care of
 * itself is the width (the name must elide to the row, see limitWidth()) and the drag, which is a
 * feature of attachments of a message and is switched off.
 */
class ChatObjectFileRow : public ChatMessageFileItem
{
    public:

        ChatObjectFileRow(const ChatObjectItem& object, const ChatObjectRowHooks& hooks, QWidget* parent)
            : ChatMessageFileItem(parent),
              m_hooks(hooks)
        {
            setObjectName("fileRow");
            setDragEnabled(false);

            connect(this,&ChatMessageFileItem::clicked,this,
                [this]()
                {
                    callActivated(m_hooks,m_object);
                }
            );
            connect(this,&ChatMessageFileItem::loadControlClicked,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::LoadControl);
                }
            );
            connect(this,&ChatMessageFileItem::menuTriggered,this,
                [this](int action)
                {
                    callAction(m_hooks,m_object,ChatObjectAction::FileMenu,action);
                }
            );
            connect(this,&ChatMessageFileItem::pauseRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Pause);
                }
            );
            connect(this,&ChatMessageFileItem::cancelRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Cancel);
                }
            );

            setObject(object);
        }

        void setObject(const ChatObjectItem& object)
        {
            // before setItem(): it calls refresh(), which reads m_object
            m_object=object;
            setItem(object.file,object.incoming);
        }

        void refresh() override
        {
            ChatMessageFileItem::refresh();

            // While a transfer runs the base shows its progress in the info line and rewrites it
            // on every tick; leave it alone then.
            if (m_object.file.state()!=ChatFileTransferState::Running)
            {
                setInfoText(
                    joinMeta(
                        formatFileSize(m_object.file.size()),
                        objectDateText(m_object.dateTime),
                        m_object.senderName
                    )
                );
            }
        }

    protected:

        void resizeEvent(QResizeEvent* event) override
        {
            ChatMessageFileItem::resizeEvent(event);
            limitWidth(width());
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
            ChatMessageFileItem::contextMenuEvent(event);
        }

    private:

        ChatObjectItem m_object;
        ChatObjectRowHooks m_hooks;
};

//--------------------------------------------------------------------------

/**
 * @brief The voice message row of the chat message, titled with the sender and the date.
 *
 * Playing is up to the host: the row reports play, stop and seek as actions, and the host answers
 * with updateItem() (the item's isPlaying()) and setPlaybackProgress()/setPlaybackPosition() of
 * the row, found with ChatObjectListBrowser::rowWidget().
 */
class ChatObjectVoiceRow : public ChatVoiceFileItem
{
    public:

        ChatObjectVoiceRow(const ChatObjectItem& object, const ChatObjectRowHooks& hooks, QWidget* parent)
            : ChatVoiceFileItem(parent),
              m_hooks(hooks)
        {
            setObjectName("voiceRow");
            setDragEnabled(false);

            connect(this,&ChatMessageFileItem::clicked,this,
                [this]()
                {
                    callActivated(m_hooks,m_object);
                }
            );
            connect(this,&ChatMessageFileItem::loadControlClicked,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::LoadControl);
                }
            );
            connect(this,&ChatMessageFileItem::menuTriggered,this,
                [this](int action)
                {
                    callAction(m_hooks,m_object,ChatObjectAction::FileMenu,action);
                }
            );
            connect(this,&ChatMessageFileItem::pauseRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Pause);
                }
            );
            connect(this,&ChatMessageFileItem::cancelRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Cancel);
                }
            );
            connect(this,&ChatMessageFileItem::playRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Play);
                }
            );
            connect(this,&ChatMessageFileItem::stopRequested,this,
                [this]()
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Stop);
                }
            );
            connect(this,&ChatMessageFileItem::seekRequested,this,
                [this](qreal fraction)
                {
                    callAction(m_hooks,m_object,ChatObjectAction::Seek,fraction);
                }
            );

            setObject(object);
        }

        void setObject(const ChatObjectItem& object)
        {
            // before setItem(): it calls refresh(), which reads m_object
            m_object=object;
            setItem(object.file,object.incoming);
        }

        void refresh() override
        {
            ChatVoiceFileItem::refresh();
            applyTitle();
        }

    protected:

        void resizeEvent(QResizeEvent* event) override
        {
            ChatVoiceFileItem::resizeEvent(event);
            limitWidth(width());
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
            ChatVoiceFileItem::contextMenuEvent(event);
        }

        void changeEvent(QEvent* event) override
        {
            ChatVoiceFileItem::changeEvent(event);
            if (event->type()==QEvent::LanguageChange)
            {
                // the base has just put its own "Voice message" title back
                applyTitle();
            }
        }

    private:

        void applyTitle()
        {
            const auto title=joinMeta(m_object.senderName,objectDateText(m_object.dateTime));
            if (!title.isEmpty())
            {
                setNameText(title);
            }
        }

        ChatObjectItem m_object;
        ChatObjectRowHooks m_hooks;
};

//--------------------------------------------------------------------------

/**
 * @brief Row of a link or of a mention: a glyph or an avatar on the left, up to four lines of
 *  elided text on the right.
 *
 * Link:    title (or the url), the url if a title is shown, snippet of the message, "date · sender".
 * Mention: sender, snippet of the message, date.
 *
 * Styled by object names (#linkRow/#mentionRow, #rowIcon, #rowAvatar, #textColumn, #title, #url,
 * #snippet, #meta) under uise--ChatObjectListBrowser, see chatobjectbrowser.qss. Single-line
 * ElidedLabels on purpose: they shrink to the row and elide, which a word-wrapped label would not.
 */
class ChatObjectTextRow : public QFrame
{
    public:

        ChatObjectTextRow(bool mention, const ChatObjectItem& object, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_mention(mention),
              m_hooks(hooks)
        {
            setObjectName(mention ? "mentionRow" : "linkRow");
            setCursor(Qt::PointingHandCursor);

            auto* layout=Layout::horizontal(this);

            if (mention)
            {
                m_avatar=new AvatarWidget(this);
                m_avatar->setObjectName("rowAvatar");
                layout->addWidget(m_avatar,0,Qt::AlignTop);
            }
            else
            {
                m_icon=new RoundedImage(this);
                m_icon->setObjectName("rowIcon");
                m_icon->setSvgIcon(
                    Style::instance().svgIconLocator().icon(QStringLiteral("ChatObjectBrowser::link"),this)
                );
                m_icon->setSvgIconSize(QSize(20,20));
                layout->addWidget(m_icon,0,Qt::AlignTop);
            }

            auto* column=new QFrame(this);
            column->setObjectName("textColumn");
            auto* columnLayout=Layout::vertical(column);
            layout->addWidget(column,1);

            m_title=makeLabel("title",column);
            columnLayout->addWidget(m_title);
            m_url=makeLabel("url",column);
            columnLayout->addWidget(m_url);
            m_snippet=makeLabel("snippet",column);
            columnLayout->addWidget(m_snippet);
            m_meta=makeLabel("meta",column);
            columnLayout->addWidget(m_meta);

            setObject(object);
        }

        void setObject(const ChatObjectItem& object)
        {
            m_object=object;

            if (m_mention)
            {
                m_avatar->setAvatarName(object.senderName.toStdString());
                if (!object.senderAvatarPath.empty())
                {
                    m_avatar->setAvatarPath(object.senderAvatarPath);
                }
                if (object.avatarSource)
                {
                    m_avatar->setAvatarSource(object.avatarSource);
                }

                setText(m_title,object.senderName);
                setText(m_url,QString{});
                setText(m_snippet,oneLine(object.text));
                setText(m_meta,objectDateText(object.dateTime));
            }
            else
            {
                const bool hasTitle=!object.title.isEmpty();
                setText(m_title,hasTitle ? oneLine(object.title) : object.url);
                setText(m_url,hasTitle ? object.url : QString{});
                setText(m_snippet,oneLine(object.text));
                setText(m_meta,joinMeta(objectDateText(object.dateTime),object.senderName));
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
                if (rect().contains(event->position().toPoint()))
                {
                    callActivated(m_hooks,m_object);
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

        static ElidedLabel* makeLabel(const char* name, QWidget* parent)
        {
            auto* label=new ElidedLabel(parent);
            label->setObjectName(name);
            label->setElideMode(Qt::ElideRight);
            // a long text must elide to the row, not widen the list
            label->setIgnoreSizeHint(true);
            return label;
        }

        //! An empty line is hidden so that it takes no height.
        static void setText(ElidedLabel* label, const QString& text)
        {
            label->setText(text);
            label->setVisible(!text.isEmpty());
        }

        bool m_mention;
        ChatObjectRowHooks m_hooks;
        ChatObjectItem m_object;
        bool m_pressed=false;

        AvatarWidget* m_avatar=nullptr;
        RoundedImage* m_icon=nullptr;
        ElidedLabel* m_title=nullptr;
        ElidedLabel* m_url=nullptr;
        ElidedLabel* m_snippet=nullptr;
        ElidedLabel* m_meta=nullptr;
};

} // anonymous namespace

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectFileRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectFileRow(item,hooks,parent);
    };
    factory.update=[](QFrame* row, const ChatObjectItem& item)
    {
        static_cast<ChatObjectFileRow*>(row)->setObject(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectVoiceRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectVoiceRow(item,hooks,parent);
    };
    factory.update=[](QFrame* row, const ChatObjectItem& item)
    {
        static_cast<ChatObjectVoiceRow*>(row)->setObject(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectLinkRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectTextRow(false,item,hooks,parent);
    };
    factory.update=[](QFrame* row, const ChatObjectItem& item)
    {
        static_cast<ChatObjectTextRow*>(row)->setObject(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectMentionRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectTextRow(true,item,hooks,parent);
    };
    factory.update=[](QFrame* row, const ChatObjectItem& item)
    {
        static_cast<ChatObjectTextRow*>(row)->setObject(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectListBrowser* makeChatObjectFileBrowser(QWidget* parent)
{
    return new ChatObjectListBrowser(ChatObjectKind::File,makeChatObjectFileRowFactory(),parent);
}

//--------------------------------------------------------------------------

ChatObjectListBrowser* makeChatObjectVoiceBrowser(QWidget* parent)
{
    return new ChatObjectListBrowser(ChatObjectKind::Voice,makeChatObjectVoiceRowFactory(),parent);
}

//--------------------------------------------------------------------------

ChatObjectListBrowser* makeChatObjectLinkBrowser(QWidget* parent)
{
    return new ChatObjectListBrowser(ChatObjectKind::Link,makeChatObjectLinkRowFactory(),parent);
}

//--------------------------------------------------------------------------

ChatObjectListBrowser* makeChatObjectMentionBrowser(QWidget* parent)
{
    auto* browser=new ChatObjectListBrowser(ChatObjectKind::Mention,makeChatObjectMentionRowFactory(),parent);
    browser->setActivationGoesToMessage(true);
    return browser;
}

} // namespace uise
