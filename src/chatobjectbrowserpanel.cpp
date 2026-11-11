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

/** @file uise/desktop/src/chatobjectbrowserpanel.cpp
*
*  Defines ChatObjectBrowserRegistry and ChatObjectBrowserPanel.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QLabel>

#include <uise/desktop/chatobjectbrowserpanel.hpp>
#include <uise/desktop/chatobjectlistbrowser.hpp>
#include <uise/desktop/chatobjectgallerybrowser.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

void ChatObjectBrowserRegistry::registerKind(const QString& kind, ChatObjectBrowserKindInfo info)
{
    for (auto& entry : m_kinds)
    {
        if (entry.first==kind)
        {
            entry.second=std::move(info);
            return;
        }
    }
    m_kinds.emplace_back(kind,std::move(info));
}

//--------------------------------------------------------------------------

bool ChatObjectBrowserRegistry::unregisterKind(const QString& kind)
{
    const auto it=std::find_if(
        m_kinds.begin(),
        m_kinds.end(),
        [&kind](const std::pair<QString,ChatObjectBrowserKindInfo>& entry)
        {
            return entry.first==kind;
        }
    );
    if (it==m_kinds.end())
    {
        return false;
    }
    m_kinds.erase(it);
    return true;
}

//--------------------------------------------------------------------------

const ChatObjectBrowserKindInfo* ChatObjectBrowserRegistry::find(const QString& kind) const
{
    for (const auto& entry : m_kinds)
    {
        if (entry.first==kind)
        {
            return &entry.second;
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

std::vector<QString> ChatObjectBrowserRegistry::kinds() const
{
    std::vector<QString> result;
    result.reserve(m_kinds.size());
    for (const auto& entry : m_kinds)
    {
        result.push_back(entry.first);
    }
    return result;
}

//--------------------------------------------------------------------------

ChatObjectBrowserRegistry ChatObjectBrowserRegistry::defaults()
{
    ChatObjectBrowserRegistry registry;

    // The strings are written out at each call, not through a helper: lupdate only extracts
    // QCoreApplication::translate() calls whose arguments are string literals.

    registry.registerKind(
        ChatObjectKind::Image,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Images");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","No images in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectImageBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::File,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Files");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","No files in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectFileBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::Voice,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Voice messages");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","No voice messages in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectVoiceBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::Link,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Links");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","No links in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectLinkBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::Mention,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Mentions");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","You were not mentioned in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectMentionBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::Bookmark,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Bookmarks");
            },
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","No bookmarks in this chat");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectBookmarkBrowser(parent);
            }
        }
    );

    registry.registerKind(
        ChatObjectKind::Search,
        ChatObjectBrowserKindInfo{
            []()
            {
                return QCoreApplication::translate("ChatObjectBrowser","Search");
            },
            []()
            {
                // the hint: shown while there is no text to search for
                return QCoreApplication::translate("ChatObjectBrowser","Type to search messages");
            },
            [](QWidget* parent) -> AbstractChatObjectBrowser*
            {
                return makeChatObjectSearchBrowser(parent);
            }
        }
    );

    return registry;
}

//--------------------------------------------------------------------------

ChatObjectBrowserPanel::ChatObjectBrowserPanel(QWidget* parent)
    : FrameWithRefresh(parent),
      m_registry(ChatObjectBrowserRegistry::defaults()),
      m_headerLayout(nullptr),
      m_bodyLayout(nullptr),
      m_backButton(nullptr),
      m_titleLabel(nullptr),
      m_unknownLabel(nullptr)
{
    setObjectName("chatObjectBrowserPanel");

    auto* layout=Layout::vertical(this);

    auto* header=new QFrame(this);
    header->setObjectName("header");
    m_headerLayout=Layout::horizontal(header);

    m_backButton=new PushButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ChatObjectBrowser::back"),this),
        header
    );
    m_backButton->setObjectName("backButton");
    m_backButton->setCursor(Qt::PointingHandCursor);
    connect(m_backButton,&PushButton::clicked,this,&ChatObjectBrowserPanel::backRequested);
    m_headerLayout->addWidget(m_backButton);

    m_titleLabel=new ElidedLabel(header);
    m_titleLabel->setObjectName("title");
    m_titleLabel->setElideMode(Qt::ElideRight);
    // a long title must elide, not widen the panel
    m_titleLabel->setIgnoreSizeHint(true);
    m_headerLayout->addWidget(m_titleLabel,1);

    layout->addWidget(header);

    auto* body=new QFrame(this);
    body->setObjectName("body");
    m_bodyLayout=Layout::vertical(body);
    layout->addWidget(body,1);

    m_unknownLabel=new QLabel(body);
    m_unknownLabel->setObjectName("unknownLabel");
    m_unknownLabel->setTextFormat(Qt::PlainText);
    m_unknownLabel->setAlignment(Qt::AlignCenter);
    m_unknownLabel->setWordWrap(true);
    m_unknownLabel->setVisible(false);
    m_bodyLayout->addWidget(m_unknownLabel,1);
}

//--------------------------------------------------------------------------

ChatObjectBrowserPanel::~ChatObjectBrowserPanel()
{
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::setRegistry(ChatObjectBrowserRegistry registry)
{
    m_registry=std::move(registry);
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::setRequestCb(AbstractChatObjectBrowser::RequestCb cb)
{
    m_requestCb=std::move(cb);
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::setContextMenuBuilder(AbstractChatObjectBrowser::ContextMenuBuilder builder)
{
    m_menuBuilder=std::move(builder);
}

//--------------------------------------------------------------------------

AbstractChatObjectBrowser* ChatObjectBrowserPanel::currentBrowser() const noexcept
{
    return m_browser.data();
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::closeBrowser()
{
    if (!m_browser.isNull())
    {
        destroyWidget(m_browser.data());
    }
    m_browser.clear();
    m_kind.clear();
    m_unknownLabel->setVisible(false);
}

//--------------------------------------------------------------------------

AbstractChatObjectBrowser* ChatObjectBrowserPanel::open(const QString& kind, const ChatObjectKey& anchor)
{
    closeBrowser();
    m_kind=kind;

    const auto* info=m_registry.find(kind);
    AbstractChatObjectBrowser* browser=nullptr;
    if (info!=nullptr && info->factory)
    {
        browser=info->factory(m_bodyLayout->parentWidget());
    }

    if (browser==nullptr)
    {
        qWarning() << "ChatObjectBrowserPanel: no browser is registered for the kind" << kind;
        m_titleLabel->setText(kind);
        m_unknownLabel->setText(QCoreApplication::translate("ChatObjectBrowser","These objects cannot be shown"));
        m_unknownLabel->setVisible(true);
        return nullptr;
    }

    m_browser=browser;
    browser->setRequestCb(m_requestCb);
    if (m_menuBuilder)
    {
        browser->setContextMenuBuilder(m_menuBuilder);
    }

    connect(browser,&AbstractChatObjectBrowser::itemActivated,this,&ChatObjectBrowserPanel::itemActivated);
    connect(browser,&AbstractChatObjectBrowser::goToMessageRequested,this,&ChatObjectBrowserPanel::goToMessageRequested);
    connect(browser,&AbstractChatObjectBrowser::contextMenuTriggered,this,&ChatObjectBrowserPanel::contextMenuTriggered);
    connect(browser,&AbstractChatObjectBrowser::itemActionRequested,this,&ChatObjectBrowserPanel::itemActionRequested);
    connect(browser,&AbstractChatObjectBrowser::searchTextChanged,this,&ChatObjectBrowserPanel::searchTextChanged);

    m_bodyLayout->addWidget(browser,1);
    applyKindTexts();

    // The request callback is set, the browser is in place: now it may ask for its data.
    if (anchor.isNull())
    {
        browser->reload();
    }
    else
    {
        browser->openAt(anchor);
    }

    return browser;
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::applyKindTexts()
{
    const auto* info=m_registry.find(m_kind);
    if (info==nullptr)
    {
        return;
    }
    if (info->title)
    {
        m_titleLabel->setText(info->title());
    }
    if (info->emptyText && !m_browser.isNull())
    {
        m_browser->setEmptyText(info->emptyText());
    }
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

//--------------------------------------------------------------------------

QString ChatObjectBrowserPanel::title() const
{
    return m_titleLabel->text();
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::setBackButtonVisible(bool enable)
{
    m_backButton->setVisible(enable);
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::addTrailingWidget(QWidget* widget)
{
    m_headerLayout->addWidget(widget);
}

//--------------------------------------------------------------------------

void ChatObjectBrowserPanel::changeEvent(QEvent* event)
{
    FrameWithRefresh::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        // a title set with setTitle() is the owner's to translate; the registry's follow
        applyKindTexts();
    }
}

} // namespace uise
