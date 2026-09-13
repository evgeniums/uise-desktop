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

/** @file demo/chatreactions/main.cpp
*
*  Demo application of chat message reactions: the bubble section (chip row, wrap, overflow,
*  avatars-vs-count) and the reaction gallery popup (collapsed quick bar / expanded search+grid),
*  closing the loop end to end -- picking a reaction in the gallery updates the bubble.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessagereactions.hpp>
#include <uise/desktop/chatreactiongallery.hpp>
#include <uise/desktop/reactioniconpack.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

namespace {

constexpr int DemoBubbleWidth=380;

// Builds a real ChatMessage/ChatMessageContent bubble around a one-line text body, a time/status
// bottom row, and an (initially empty) reactions section -- mirrors demo/messageformatting/
// main.cpp's own makeMessage(), extended with the 6th setWidgets() slot.
AbstractChatMessage* makeMessage(QWidget* parent, const QString& text, ChatMessageReactions** outReactions)
{
    AbstractChatMessage* msg=new ChatMessage(parent);
    msg->construct();
    msg->setDirection(AbstractChatMessage::Direction::Received);
    msg->setDateTime(QDateTime::currentDateTime());

    auto* content=new ChatMessageContent(msg);
    content->setChatMessage(msg);

    auto* body=new ChatMessageText(content);
    body->loadText(text,TextFormat::Plain);

    auto* bottom=new ChatMessageBottom(content);
    bottom->setTimeString(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));

    auto* reactions=new ChatMessageReactions(content);
    if (outReactions!=nullptr)
    {
        *outReactions=reactions;
    }

    content->setWidgets(body,nullptr,bottom,nullptr,nullptr,reactions);
    msg->setContent(content);

    content->updateBubbleWidth(DemoBubbleWidth);

    return msg;
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;

    auto* mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);
    auto* central=new QFrame(mainFrame);
    auto* rootLayout=Layout::vertical(central,false);
    mainFrame->setWidget(central);

    auto* log=new QPlainTextEdit();
    log->setReadOnly(true);
    log->setMaximumHeight(140);
    auto logMsg=[log](const QString& text)
    {
        log->appendPlainText(text);
    };

    // --- state: the bubble's current reactions, kept as a plain in-memory model the demo owns
    // (a real host would own this per-message, backed by the server -- see the task spec) ---

    ChatReactions currentReactions;
    ChatMessageReactions* reactionsSection=nullptr;
    AbstractChatMessage* msg=nullptr;

    auto applyReactions=[&]()
    {
        // ChatMessageReactions::setReactions() re-shows itself when it stops being empty and
        // re-runs the bubble's own negotiation on every call -- the caller does not need to
        // (and should not) do either one itself.
        reactionsSection->setReactions(currentReactions);
    };

    auto toggleReaction=[&](const QString& iconId)
    {
        auto id=ChatReactionId::make(iconId,QString{});
        auto it=std::find_if(currentReactions.begin(),currentReactions.end(),[&id](const ChatReaction& r)
            {
                return r.id()==id;
            });

        if (it!=currentReactions.end() && it->isOwn())
        {
            logMsg(QStringLiteral("toggleReaction: removing own reaction '%1'").arg(iconId));
            if (it->count()<=1)
            {
                currentReactions.erase(it);
            }
            else
            {
                it->setOwn(false);
                it->setCount(it->count()-1);
            }
        }
        else if (it!=currentReactions.end())
        {
            logMsg(QStringLiteral("toggleReaction: adding own vote to existing reaction '%1'").arg(iconId));
            it->setOwn(true);
            it->setCount(it->count()+1);
        }
        else
        {
            logMsg(QStringLiteral("toggleReaction: setting new reaction '%1'").arg(iconId));
            ChatReaction r;
            r.setId(id);
            r.setCount(1);
            r.setOwn(true);
            currentReactions.push_back(r);
        }

        applyReactions();
    };

    // --- the bubble itself ---

    auto* bubbleRow=new QFrame(central);
    Layout::horizontal(bubbleRow);
    rootLayout->addWidget(bubbleRow);

    msg=makeMessage(central,QStringLiteral("Reactions demo -- react below, or drag the window "
                                            "narrow to see the chip row wrap."),&reactionsSection);
    bubbleRow->layout()->addWidget(msg);
    static_cast<QHBoxLayout*>(bubbleRow->layout())->addStretch(1);

    QObject::connect(reactionsSection,&AbstractChatMessageReactions::toggleRequested,log,
        [&,logMsg](const QString& reactionId, bool currentlyOwn)
        {
            logMsg(QStringLiteral("toggleRequested: '%1' currentlyOwn=%2")
                   .arg(ChatReactionId::iconId(reactionId)).arg(currentlyOwn));
            toggleReaction(ChatReactionId::iconId(reactionId));
        }
    );
    QObject::connect(reactionsSection,&AbstractChatMessageReactions::moreRequested,log,
        [logMsg]()
        {
            logMsg(QStringLiteral("moreRequested: host would show a who-reacted-with-what list here"));
        }
    );

    // --- quick manual toggles, bypassing the gallery, for fast iteration on wrap/overflow/
    // avatars-vs-count without having to click through the popup each time ---

    auto* quickRow=new QFrame(central);
    auto* quickRowL=Layout::horizontal(quickRow);
    rootLayout->addWidget(quickRow);
    quickRowL->addWidget(new QLabel(QStringLiteral("Quick toggle:")));
    for (const auto& iconId : {QStringLiteral("thumbsup"),QStringLiteral("heart"),QStringLiteral("joy"),
                               QStringLiteral("fire"),QStringLiteral("clap"),QStringLiteral("tada"),
                               QStringLiteral("rocket"),QStringLiteral("eyes"),QStringLiteral("check")})
    {
        auto* button=new QPushButton(iconId,quickRow);
        quickRowL->addWidget(button);
        QObject::connect(button,&QPushButton::clicked,log,[iconId,&toggleReaction]()
            {
                toggleReaction(iconId);
            }
        );
    }
    quickRowL->addStretch(1);

    // --- avatar-mode sample: the quick-toggle buttons above only ever set the CURRENT user's own
    // reaction (count 1, no avatar data), which never satisfies ChatMessageReactionsRow::
    // setReactions()'s Auto policy (<=2 reaction types, each count<=3, EVERY reaction carrying
    // avatars() -- see its own doc comment), so the row never has a reason to pick Avatars over
    // Counts display. This button loads reaction data shaped the way a real multi-user chat would
    // hand it in, to actually exercise that path. ---

    auto* avatarRow=new QFrame(central);
    auto* avatarRowL=Layout::horizontal(avatarRow);
    rootLayout->addWidget(avatarRow);

    auto* avatarSampleButton=new QPushButton(QStringLiteral("Show avatar-mode sample"),avatarRow);
    avatarRowL->addWidget(avatarSampleButton);

    auto* clearButton=new QPushButton(QStringLiteral("Clear reactions"),avatarRow);
    avatarRowL->addWidget(clearButton);
    avatarRowL->addStretch(1);

    // AvatarWidget renders its initials-circle fallback whenever setAvatarSource()/setAvatarPath()
    // is never called -- exactly ChatReactionAvatar's own "no source, no path" default, so plain
    // names are enough here without needing any actual image files.
    auto makeAvatar=[](const QString& name)
    {
        ChatReactionAvatar a;
        a.name=name;
        return a;
    };

    QObject::connect(avatarSampleButton,&QPushButton::clicked,log,
        [&,makeAvatar,logMsg]()
        {
            logMsg(QStringLiteral("loading avatar-mode sample (2 reaction types, named users)"));

            ChatReaction heart;
            heart.setId(ChatReactionId::make(QStringLiteral("heart"),QString{}));
            heart.setCount(2);
            heart.setOwn(true);
            heart.setAvatars({makeAvatar(QStringLiteral("You")),makeAvatar(QStringLiteral("Alice"))});

            ChatReaction thumbsup;
            thumbsup.setId(ChatReactionId::make(QStringLiteral("thumbsup"),QString{}));
            thumbsup.setCount(3);
            thumbsup.setOwn(false);
            thumbsup.setAvatars({makeAvatar(QStringLiteral("Bob")),makeAvatar(QStringLiteral("Carol")),
                                 makeAvatar(QStringLiteral("Dave"))});

            currentReactions={heart,thumbsup};
            applyReactions();
        }
    );
    QObject::connect(clearButton,&QPushButton::clicked,log,
        [&,logMsg]()
        {
            logMsg(QStringLiteral("clearing all reactions"));
            currentReactions.clear();
            applyReactions();
        }
    );

    // --- max visible chips control, to exercise the "..." overflow chip ---

    auto* limitRow=new QFrame(central);
    auto* limitRowL=Layout::horizontal(limitRow);
    rootLayout->addWidget(limitRow);
    limitRowL->addWidget(new QLabel(QStringLiteral("Max visible chips (0 = unlimited):")));
    auto* limitSpin=new QSpinBox(limitRow);
    limitSpin->setRange(0,20);
    limitSpin->setValue(reactionsSection->row()->maxVisibleChips());
    limitRowL->addWidget(limitSpin);
    QObject::connect(limitSpin,QOverload<int>::of(&QSpinBox::valueChanged),log,
        [reactionsSection,logMsg](int value)
        {
            reactionsSection->row()->setMaxVisibleChips(value);
            logMsg(QStringLiteral("maxVisibleChips -> %1").arg(value));
        }
    );
    QObject::connect(reactionsSection->row(),&ChatMessageReactionsRow::moreRequested,log,
        [logMsg]()
        {
            logMsg(QStringLiteral("row moreRequested (\"...\" chip clicked)"));
        }
    );
    limitRowL->addStretch(1);

    // --- context-menu-like trigger for the gallery: a plain DropdownMenu with one "React..."
    // entry, exactly the shape whitemdesktop's own per-message context menu will use. "React..."
    // is CHECKABLE rather than a plain item specifically so it does NOT close the menu on click --
    // DropdownMenu::closeOnCheckableActivation() defaults to false, so toggling a checkable row
    // only emits itemToggled(), never notifyActivated()/chainRoot()->closeDropdown() the way a
    // plain item's itemTriggered() always does (see dropdownmenu.cpp). That is what makes it safe
    // to setChainParent() the gallery to a STILL-OPEN menu here, matching the reactions task
    // spec's "popups above chat message context menu" -- both stay visible together, like a
    // submenu flyout, instead of the menu closing first.

    auto* menuRow=new QFrame(central);
    auto* menuRowL=Layout::horizontal(menuRow);
    rootLayout->addWidget(menuRow);
    menuRowL->addWidget(new QLabel(QStringLiteral("Context menu (gallery chained above it):")));

    auto* menuButton=new IconTextButton(QStringLiteral("..."),menuRow);
    menuRowL->addWidget(menuButton);

    auto* autoOpenCheck=new QCheckBox(QStringLiteral("Open gallery immediately with menu"),menuRow);
    menuRowL->addWidget(autoOpenCheck);
    menuRowL->addStretch(1);

    auto* contextMenu=new DropdownMenu(menuRow);
    contextMenu->setItems({
        MenuItem::checkable(1,QStringLiteral("React..."),false),
        MenuItem::separator(),
        MenuItem(2,QStringLiteral("Reply")),
        MenuItem(3,QStringLiteral("Forward"))
    });
    contextMenu->attachTo(menuButton);

    auto* gallery=new ChatReactionGalleryDropdown(menuRow);
    gallery->setPack(ReactionIconPacks::instance().defaultPack());

    // Shared by the "React..." row's own toggle and, when autoOpenCheck is on, by the menu's own
    // aboutToShow() below -- both just need the gallery chained and popped up above the CURRENT
    // menu opening's fullRect(). setItemChecked(1,true) keeps the row's own checkmark in sync
    // when this runs from aboutToShow() (id 1 was never actually clicked in that case, so
    // DropdownMenu never set it itself), without re-triggering itemToggled() (setItemChecked()
    // goes through a QSignalBlocker -- see its own doc comment).
    auto openGallery=[&,gallery,contextMenu]()
    {
        QStringList ownIds;
        for (const auto& r : currentReactions)
        {
            if (r.isOwn())
            {
                ownIds << ChatReactionId::iconId(r.id());
            }
        }
        gallery->setOwnReactionIds(ownIds);
        gallery->setExpanded(false);

        gallery->setChainParent(contextMenu);
        gallery->popupAboveRect(contextMenu->fullRect());
        contextMenu->setItemChecked(1,true);
    };

    QObject::connect(contextMenu,&DropdownMenu::itemTriggered,log,
        [logMsg](int id)
        {
            logMsg(QStringLiteral("context menu: item %1 triggered").arg(id));
        }
    );
    QObject::connect(contextMenu,&DropdownMenu::itemToggled,log,
        [gallery,openGallery](int id, bool checked)
        {
            if (id!=1)
            {
                return;
            }

            if (!checked)
            {
                // the row was clicked again to toggle it back off -- close the gallery to match
                gallery->closeDropdown();
                return;
            }

            openGallery();
        }
    );
    QObject::connect(contextMenu,&DropdownMenu::aboutToShow,log,
        [autoOpenCheck,openGallery]()
        {
            // Fires after measure() has already computed this opening's fullRect() (see
            // DropdownFrame::popupBelow()), so it is already valid to anchor the gallery to here
            // -- both frames then animate open together instead of the gallery visibly lagging
            // a beat behind the menu's own open animation (which shown() would wait out first).
            if (autoOpenCheck->isChecked())
            {
                openGallery();
            }
        }
    );
    QObject::connect(gallery,&ChatReactionGalleryDropdown::hidden,log,
        [contextMenu]()
        {
            // Keep the checkable row's own visual state in sync however the gallery closed
            // (picking a reaction below, Escape, an outside click, or the menu itself closing
            // and cascading the close down to its chained child) -- setItemChecked() updates the
            // row via a QSignalBlocker, so this can never re-trigger itemToggled() itself.
            contextMenu->setItemChecked(1,false);
        }
    );
    QObject::connect(gallery,&ChatReactionGalleryDropdown::reactionPicked,log,
        [&,logMsg](const QString& reactionId)
        {
            // ChatReactionGalleryDropdown closes itself (and, via chaining, the context menu
            // above -- see its reactionPicked() doc comment) right after this signal -- no
            // closeDropdown() call needed here.
            logMsg(QStringLiteral("gallery: picked '%1'").arg(ChatReactionId::iconId(reactionId)));
            toggleReaction(ChatReactionId::iconId(reactionId));
        }
    );

    // --- log pane ---

    rootLayout->addWidget(new QLabel(QStringLiteral("Signal log:")));
    rootLayout->addWidget(log,1);

    w.setCentralWidget(mainFrame);
    w.resize(560,720);
    w.show();

    return app.exec();
}
