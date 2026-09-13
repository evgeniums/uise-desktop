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

/** @file uise/test/chatreactions/testchatmessagereactions.cpp
*
*  Test of the ChatMessageReactions bubble section -- attach/detach, trailingSection()
*  flipping on isEmpty(), and the empty-attached-section non-regression (see
*  AbstractChatMessageContent::trailingSection()'s own doc comment).
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessagereactions.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

// Fixed width so bubble-width negotiation is deterministic -- same reasoning as
// demo/messageformatting/main.cpp's own DemoBubbleWidth.
constexpr int TestBubbleWidth=380;

//! Builds a real ChatMessage/ChatMessageContent bubble around a short one-line text body plus a
//! time/status bottom row, so the inline-vs-row negotiation is genuinely exercised rather than
//! stubbed -- mirrors demo/messageformatting/main.cpp's makeMessage().
AbstractChatMessage* makeMessage(QWidget* parent, ChatMessageReactions* reactions=nullptr)
{
    // Declared as the BASE pointer, not auto* -- ChatMessage::construct() is protected (only
    // AbstractChatMessage::construct() is accessible here), same as
    // demo/messageformatting/main.cpp's own makeMessage().
    AbstractChatMessage* msg=new ChatMessage(parent);
    msg->construct();
    msg->setDirection(AbstractChatMessage::Direction::Received);
    msg->setDateTime(QDateTime::currentDateTime());

    auto* content=new ChatMessageContent(msg);
    content->setChatMessage(msg);

    auto* body=new ChatMessageText(content);
    body->loadText(QStringLiteral("hi"),TextFormat::Plain);

    auto* bottom=new ChatMessageBottom(content);
    bottom->setTimeString(QStringLiteral("12:00"));

    content->setWidgets(body,nullptr,bottom,nullptr,nullptr,reactions);
    msg->setContent(content);

    content->updateBubbleWidth(TestBubbleWidth);

    return msg;
}

ChatReaction makeReaction(const QString& iconId, size_t count, bool own)
{
    ChatReaction r;
    r.setId(ChatReactionId::make(iconId,QString{}));
    r.setCount(count);
    r.setOwn(own);
    return r;
}

}

BOOST_AUTO_TEST_SUITE(TestChatMessageReactionsSection)

BOOST_AUTO_TEST_CASE(TestAttachDetach)
{
    auto handler=[]()
    {
        auto* reactions=new ChatMessageReactions();
        auto* msg=makeMessage(nullptr,reactions);
        auto* content=msg->content();

        UISE_TEST_REQUIRE(content->reactions()!=nullptr);
        UISE_TEST_CHECK(content->reactions()==reactions);

        content->clearReactions();
        UISE_TEST_CHECK(content->reactions()==nullptr);

        delete msg;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestEmptyAttachedReactionsDoesNotForceRowMode)
{
    // The regression this section's trailingSection() override must never reintroduce: a short
    // one-line message with an ATTACHED BUT EMPTY reactions section (the common case -- most
    // messages carry the slot whether or not any reaction was ever set) must seat its
    // time/status row INLINE, exactly as it would with no reactions section at all.
    auto handler=[]()
    {
        auto* baselineMsg=makeMessage(nullptr,nullptr);
        UISE_TEST_REQUIRE(baselineMsg->content()->isBottomInline());

        auto* reactions=new ChatMessageReactions();
        UISE_TEST_CHECK(reactions->isEmpty());
        auto* msg=makeMessage(nullptr,reactions);

        UISE_TEST_CHECK(msg->content()->isBottomInline());
        UISE_TEST_CHECK_EQUAL(msg->content()->sizeHint().height(),baselineMsg->content()->sizeHint().height());

        delete msg;
        delete baselineMsg;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestNonEmptyReactionsBecomesTrailingSection)
{
    auto handler=[]()
    {
        auto* reactions=new ChatMessageReactions();
        auto* msg=makeMessage(nullptr,reactions);
        auto* content=msg->content();

        ChatReactions data{makeReaction(QStringLiteral("thumbsup"),3,false)};
        reactions->setReactions(data);

        // Re-negotiate now that the section actually has content -- setReactions() alone only
        // repacks the row at its OWN current width, it does not re-run the bubble's own
        // negotiation pass.
        content->updateBubbleWidth(TestBubbleWidth);

        UISE_TEST_CHECK(!reactions->isEmpty());
        UISE_TEST_CHECK(content->trailingSection()==static_cast<ChatMessageContentSection*>(reactions));
        UISE_TEST_CHECK(reactions->lastTextLineRect().isValid());

        delete msg;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestBubbleGrowsWhenReactionsAreSet)
{
    auto handler=[]()
    {
        auto* reactions=new ChatMessageReactions();
        auto* msg=makeMessage(nullptr,reactions);
        auto* content=msg->content();

        auto heightBefore=content->sizeHint().height();

        ChatReactions data{makeReaction(QStringLiteral("thumbsup"),3,false),
                           makeReaction(QStringLiteral("heart"),1,true)};
        reactions->setReactions(data);
        content->updateBubbleWidth(TestBubbleWidth);

        // NOT UISE_TEST_CHECK_GT -- that macro is actually BOOST_CHECK_GE (a known naming bug in
        // this test framework, see uise-testthread.hpp), which would silently accept an
        // unchanged height. A raw strict comparison is what "bubble height growth" needs here.
        auto heightAfter=content->sizeHint().height();
        UISE_TEST_CHECK(heightAfter>heightBefore);

        delete msg;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_SUITE_END()
