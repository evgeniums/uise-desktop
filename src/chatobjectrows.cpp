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

#include <functional>
#include <memory>

#include <QBoxLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QStringList>

#include <uise/desktop/chatobjectlistbrowser.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>
#include <uise/desktop/chataudiofileitems.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
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

//! Whether \a text has anything in it besides the links: what is left of it without them has a letter or a digit. The links are
//! found as the address, as the row shows it and as the message names it.
bool hasTextBesidesLinks(const QString& text, const std::vector<ChatObjectItemLink>& links)
{
    QString rest=text;
    for (const auto& link : links)
    {
        // the address first: it contains what the row shows of it
        for (const auto* part : {&link.url,&link.text,&link.title})
        {
            if (!part->isEmpty())
            {
                rest.remove(*part,Qt::CaseInsensitive);
            }
        }
    }
    for (const auto ch : rest)
    {
        if (ch.isLetterOrNumber())
        {
            return true;
        }
    }
    return false;
}

//! Whitespace and line breaks of a snippet collapsed: the rows show one line of it.
QString oneLine(const QString& text)
{
    return text.simplified();
}

/**
 * @brief The title line of the row of an object: who and when. The sender is elided to the row, and "You" stands for our own
 *  messages, in a style of its own; the date is at the right.
 *
 * Styled by object names (#objectHeader, #sender, #time) under uise--ChatObjectListBrowser. In a file or a voice row it is
 * the title the list puts above the row (ChatObjectRowFactory::buildTitle), so that it spans the row's whole width, the menu
 * button included; in a link row it is the first line of the card.
 */
class ChatObjectTitle : public QFrame
{
    public:

        explicit ChatObjectTitle(QWidget* parent)
            : QFrame(parent)
        {
            setObjectName("objectHeader");
            auto* layout=Layout::horizontal(this);

            m_sender=new ElidedLabel(this);
            m_sender->setObjectName("sender");
            m_sender->setElideMode(Qt::ElideRight);
            // a long name must elide to the row, not widen the list
            m_sender->setIgnoreSizeHint(true);
            layout->addWidget(m_sender,1);

            // the date is at the right end of the line, whatever the sender takes
            m_time=new QLabel(this);
            m_time->setObjectName("time");
            m_time->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            layout->addWidget(m_time,0,Qt::AlignRight);
        }

        void set(const ChatObjectItem& object)
        {
            const bool self=!object.incoming;
            const auto sender=self ? QCoreApplication::translate("ChatObjectBrowser","You") : object.senderName;
            m_sender->setText(sender);
            m_sender->setVisible(!sender.isEmpty());
            if (m_self!=self)
            {
                m_self=self;
                m_sender->setProperty("self",self);
                // the style is keyed on the property and reaches the label inside the ElidedLabel
                Style::repolishRecursive(m_sender);
            }
            m_time->setText(objectDateText(object.dateTime));
        }

    private:

        ElidedLabel* m_sender=nullptr;
        QLabel* m_time=nullptr;
        bool m_self=false;
};

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

            // The row's own menu is not offered in a browser: what there is to do with the object is in the context menu
            // of the browser (Open, Go to message).
            menuButton()->setVisible(false);

            // The name elides to the row (the base sizes the row to the name, as it does in a chat bubble).
            nameLabel()->setIgnoreSizeHint(true);

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
                // the date is in the title line
                setInfoText(formatFileSize(m_object.file.size()));
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

            // as the file row
            menuButton()->setVisible(false);

            // In a browser the waveform is not a seek bar: a click on it is a click on the row. (Transparent for the mouse
            // whatever refresh() says about seeking: the press goes on to mousePressEvent() below.)
            waveformBar()->setAttribute(Qt::WA_TransparentForMouseEvents,true);
            setCursor(Qt::PointingHandCursor);

            // as the file row
            nameLabel()->setIgnoreSizeHint(true);

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

    protected:

        void resizeEvent(QResizeEvent* event) override
        {
            ChatVoiceFileItem::resizeEvent(event);
            limitWidth(width());
        }

        //! What the base takes for the background of the row, and what falls through the waveform: the row is opened (which
        //! plays it). The icon and the title are clicked through the base's own filter, before this.
        void mousePressEvent(QMouseEvent* event) override
        {
            if (event->button()==Qt::LeftButton)
            {
                callActivated(m_hooks,m_object);
                event->accept();
                return;
            }
            ChatVoiceFileItem::mousePressEvent(event);
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

    private:

        ChatObjectItem m_object;
        ChatObjectRowHooks m_hooks;
};

//--------------------------------------------------------------------------

/**
 * @brief Row of a mention: the avatar of the sender on the left, up to three lines of elided text on the right.
 *
 * Mention: sender, snippet of the message, date.
 *
 * Styled by object names (#mentionRow, #rowAvatar, #textColumn, #title, #snippet, #meta) under
 * uise--ChatObjectListBrowser, see chatobjectbrowser.qss. Single-line ElidedLabels on purpose: they shrink to
 * the row and elide, which a word-wrapped label would not.
 */
class ChatObjectTextRow : public QFrame
{
    public:

        ChatObjectTextRow(const ChatObjectItem& object, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_hooks(hooks)
        {
            setObjectName("mentionRow");
            setCursor(Qt::PointingHandCursor);

            auto* layout=Layout::horizontal(this);

            m_avatar=new AvatarWidget(this);
            m_avatar->setObjectName("rowAvatar");
            layout->addWidget(m_avatar,0,Qt::AlignTop);

            auto* column=new QFrame(this);
            column->setObjectName("textColumn");
            auto* columnLayout=Layout::vertical(column);
            layout->addWidget(column,1);

            m_title=makeLabel("title",column);
            columnLayout->addWidget(m_title);
            m_snippet=makeLabel("snippet",column);
            columnLayout->addWidget(m_snippet);
            m_meta=makeLabel("meta",column);
            columnLayout->addWidget(m_meta);

            setObject(object);
        }

        void setObject(const ChatObjectItem& object)
        {
            m_object=object;

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
            setText(m_snippet,oneLine(object.text));
            setText(m_meta,objectDateText(object.dateTime));
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

        ChatObjectRowHooks m_hooks;
        ChatObjectItem m_object;
        bool m_pressed=false;

        AvatarWidget* m_avatar=nullptr;
        ElidedLabel* m_title=nullptr;
        ElidedLabel* m_snippet=nullptr;
        ElidedLabel* m_meta=nullptr;
};

//--------------------------------------------------------------------------

/**
 * @brief One line of an elided text that can be clicked and asks for a context menu of its own.
 *
 * The click lands on the QLabel inside the ElidedLabel, which does not take it, so it comes up to this widget.
 */
class ChatObjectClickableLabel : public ElidedLabel
{
    public:

        using ElidedLabel::ElidedLabel;

        std::function<void ()> clicked;

        //! The global position of the menu.
        std::function<void (const QPoint&)> menuRequested;

    protected:

        void mousePressEvent(QMouseEvent* event) override
        {
            if (clicked && event->button()==Qt::LeftButton)
            {
                m_pressed=true;
                event->accept();
                return;
            }
            ElidedLabel::mousePressEvent(event);
        }

        void mouseReleaseEvent(QMouseEvent* event) override
        {
            if (m_pressed && event->button()==Qt::LeftButton)
            {
                m_pressed=false;
                event->accept();
                if (rect().contains(event->position().toPoint()) && clicked)
                {
                    clicked();
                }
                return;
            }
            ElidedLabel::mouseReleaseEvent(event);
        }

        void contextMenuEvent(QContextMenuEvent* event) override
        {
            if (menuRequested)
            {
                menuRequested(event->globalPos());
                event->accept();
                return;
            }
            ElidedLabel::contextMenuEvent(event);
        }

    private:

        bool m_pressed=false;
};

//--------------------------------------------------------------------------

/**
 * @brief Row of a message with links: what the message says, the links in it, when and from whom.
 *
 * From the top: the sender (elided; "You" for our own message, in a style of its own) and the date, up to three lines
 * of the text of the message (elided, not clickable; its context menu has "Go to message" and "Copy text"), and a line
 * for every link of the message (elided, clickable, with "Open in browser" and "Copy link" in the context menu).
 *
 * Styled by object names (#linkRow, #header, #sender, #time, #messageText, #linksColumn, #linkItem) under
 * uise--ChatObjectListBrowser, see chatobjectbrowser.qss.
 */
class ChatObjectLinkRow : public QFrame
{
    public:

        ChatObjectLinkRow(const ChatObjectItem& object, const ChatObjectRowHooks& hooks, QWidget* parent)
            : QFrame(parent),
              m_hooks(hooks)
        {
            setObjectName("linkRow");

            auto* layout=Layout::vertical(this);

            // The top line: who, and when.
            m_title=new ChatObjectTitle(this);
            layout->addWidget(m_title);

            m_text=new ChatObjectClickableLabel(this);
            m_text->setObjectName("messageText");
            m_text->setElideMode(Qt::ElideRight);
            m_text->setMaxLines(3);
            // A long text must elide to the row, not widen the list, and must not make it taller than three lines: the
            // list measures a row before the text is elided, and a text of many lines left a row with the room of them.
            m_text->setIgnoreSizeHint(true);
            m_text->setMaximumHeight(3*m_text->fontMetrics().lineSpacing()+2);
            m_text->menuRequested=[this](const QPoint& globalPos)
            {
                showMessageMenu(globalPos);
            };
            layout->addWidget(m_text);

            m_linksFrame=new QFrame(this);
            m_linksFrame->setObjectName("linksColumn");
            m_linksLayout=Layout::vertical(m_linksFrame);
            layout->addWidget(m_linksFrame);

            // Whatever room the list gives the row beyond its content goes below it, not between its lines.
            layout->addStretch(1);

            setObject(object);
        }

        ~ChatObjectLinkRow()
        {
            // a parentless popup of its own, as the other rows' menus
            if (!m_menu.isNull())
            {
                m_menu->closeDropdown(true);
                destroyWidget(m_menu);
            }
        }

        void setObject(const ChatObjectItem& object)
        {
            m_object=object;

            // The links of the message; a lone link given by url and title is the one entry.
            std::vector<ChatObjectItemLink> links=object.links;
            if (links.empty() && !object.url.isEmpty())
            {
                links.push_back(ChatObjectItemLink{object.url,object.title,QString{}});
            }

            // What the message says is shown when it says something besides the links; a message that is only its link
            // (or links) would show them twice.
            const auto text=oneLine(object.text);
            const bool showText=hasTextBesidesLinks(text,links);
            m_text->setText(showText ? text : QString{});
            m_text->setVisible(showText);

            // The labels are kept and reused: a row is updated with other items all the time.
            while (m_linkLabels.size()<links.size())
            {
                m_linkLabels.push_back(makeLinkLabel());
            }
            for (size_t i=0;i<m_linkLabels.size();i++)
            {
                auto* label=m_linkLabels[i];
                if (i<links.size())
                {
                    label->setText(links[i].text.isEmpty() ? links[i].url : links[i].text);
                    label->setToolTip(links[i].title);
                    label->setVisible(true);
                }
                else
                {
                    label->setVisible(false);
                }
            }
            m_links=std::move(links);
            m_linksFrame->setVisible(!m_links.empty());

            m_title->set(object);
        }

    protected:

        //! The background of the row and the empty places: the menu of the message.
        void contextMenuEvent(QContextMenuEvent* event) override
        {
            showMessageMenu(event->globalPos());
            event->accept();
        }

    private:

        // ids of the entries of the menu
        constexpr static const int MenuGoToMessage=1;
        constexpr static const int MenuCopyText=2;
        constexpr static const int MenuOpenLink=3;
        constexpr static const int MenuCopyLink=4;

        ChatObjectClickableLabel* makeLinkLabel()
        {
            auto* label=new ChatObjectClickableLabel(m_linksFrame);
            label->setObjectName("linkItem");
            label->setElideMode(Qt::ElideRight);
            // a long address must elide to the row, not widen the list
            label->setIgnoreSizeHint(true);
            label->setCursor(Qt::PointingHandCursor);
            const size_t index=m_linkLabels.size();
            label->clicked=[this,index]()
            {
                if (index<m_links.size())
                {
                    callActivated(m_hooks,withLink(index));
                }
            };
            label->menuRequested=[this,index](const QPoint& globalPos)
            {
                if (index<m_links.size())
                {
                    showLinkMenu(index,globalPos);
                }
            };
            m_linksLayout->addWidget(label);
            return label;
        }

        //! The object of the message as the object of the one link: what the owner opens is its url.
        ChatObjectItem withLink(size_t index) const
        {
            ChatObjectItem copy=m_object;
            copy.url=m_links[index].url;
            copy.title=m_links[index].title;
            return copy;
        }

        //! The icon of a menu entry, from the browsers' icon context.
        std::shared_ptr<SvgIcon> menuIcon(const char* alias)
        {
            return Style::instance().svgIconLocator().icon(QString("ChatObjectBrowser::%1").arg(alias),this);
        }

        void showMessageMenu(const QPoint& globalPos)
        {
            std::vector<MenuItem> items;
            items.emplace_back(MenuGoToMessage,QCoreApplication::translate("ChatObjectBrowser","Go to message"),menuIcon("goToMessage"));
            if (!m_object.text.isEmpty())
            {
                items.emplace_back(MenuCopyText,QCoreApplication::translate("ChatObjectBrowser","Copy text"),menuIcon("copy"));
            }
            popup(std::move(items),globalPos);
        }

        void showLinkMenu(size_t index, const QPoint& globalPos)
        {
            m_menuLink=index;
            std::vector<MenuItem> items;
            items.emplace_back(MenuOpenLink,QCoreApplication::translate("ChatObjectBrowser","Open in browser"),menuIcon("openInBrowser"));
            items.emplace_back(MenuCopyLink,QCoreApplication::translate("ChatObjectBrowser","Copy link"),menuIcon("copy"));
            // a message of one link shows no text of its own to ask for the message from
            items.emplace_back(MenuGoToMessage,QCoreApplication::translate("ChatObjectBrowser","Go to message"),menuIcon("goToMessage"));
            popup(std::move(items),globalPos);
        }

        void popup(std::vector<MenuItem> items, const QPoint& globalPos)
        {
            if (m_menu.isNull())
            {
                m_menu=new DropdownMenu();
                // a context menu is expected to pop up instantly
                m_menu->setAnimationDurationMs(0);
                connect(m_menu.data(),&DropdownMenu::itemTriggered,this,
                    [this](int id)
                    {
                        onMenu(id);
                    }
                );
            }
            m_menu->setItems(std::move(items));
            m_menu->popupAt(globalPos);
        }

        void onMenu(int id)
        {
            switch (id)
            {
                case MenuGoToMessage:
                    if (m_hooks.goToMessage)
                    {
                        const ChatObjectItem copy=m_object;
                        m_hooks.goToMessage(copy);
                    }
                    break;

                case MenuCopyText:
                    copyToClipboard(m_object.text);
                    break;

                case MenuOpenLink:
                    if (m_menuLink<m_links.size())
                    {
                        callActivated(m_hooks,withLink(m_menuLink));
                    }
                    break;

                case MenuCopyLink:
                    if (m_menuLink<m_links.size())
                    {
                        copyToClipboard(m_links[m_menuLink].url);
                    }
                    break;

                default:
                    break;
            }
        }

        void copyToClipboard(const QString& text)
        {
            auto* clipboard=QGuiApplication::clipboard();
            if (clipboard==nullptr || text.isEmpty())
            {
                return;
            }
            clipboard->setText(text);
            callAction(m_hooks,m_object,ChatObjectAction::Copied,text);
        }

        ChatObjectRowHooks m_hooks;
        ChatObjectItem m_object;
        std::vector<ChatObjectItemLink> m_links;
        size_t m_menuLink=0;

        ChatObjectClickableLabel* m_text=nullptr;
        QFrame* m_linksFrame=nullptr;
        QBoxLayout* m_linksLayout=nullptr;
        std::vector<ChatObjectClickableLabel*> m_linkLabels;
        ChatObjectTitle* m_title=nullptr;
        QPointer<DropdownMenu> m_menu;
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
    factory.buildTitle=[](QWidget* parent) -> QWidget*
    {
        return new ChatObjectTitle(parent);
    };
    factory.updateTitle=[](QWidget* title, const ChatObjectItem& item)
    {
        static_cast<ChatObjectTitle*>(title)->set(item);
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
    factory.buildTitle=[](QWidget* parent) -> QWidget*
    {
        return new ChatObjectTitle(parent);
    };
    factory.updateTitle=[](QWidget* title, const ChatObjectItem& item)
    {
        static_cast<ChatObjectTitle*>(title)->set(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectLinkRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectLinkRow(item,hooks,parent);
    };
    factory.update=[](QFrame* row, const ChatObjectItem& item)
    {
        static_cast<ChatObjectLinkRow*>(row)->setObject(item);
    };
    return factory;
}

//--------------------------------------------------------------------------

ChatObjectRowFactory makeChatObjectMentionRowFactory()
{
    ChatObjectRowFactory factory;
    factory.build=[](const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks) -> QFrame*
    {
        return new ChatObjectTextRow(item,hooks,parent);
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
