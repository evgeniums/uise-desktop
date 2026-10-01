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

/** @file uise/test/typingindicator/testtypingindicator.cpp
*
*  Test TypingIndicator text eliding.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QFontMetrics>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>

#include <uise/desktop/typingindicator.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

using TypingIndicatorContainer=TestWidgetContainer<TypingIndicator>;
using TypingIndicatorContainerPtr=std::shared_ptr<TypingIndicatorContainer>;

namespace {

const QChar Ellipsis(0x2026);

// the text the indicator really shows: the label inside it
QLabel* labelOf(TypingIndicator* indicator)
{
    return indicator->findChild<QLabel*>();
}

int textWidth(TypingIndicator* indicator, const QString& text)
{
    return labelOf(indicator)->fontMetrics().horizontalAdvance(text);
}

// everything of the widget that is not text: dots, spacing, margins. Only valid while nothing is
// elided and no name is capped, as then the size hint is exactly this plus the width of the text.
int chromeOf(TypingIndicator* indicator)
{
    return indicator->sizeHint().width()-textWidth(indicator,indicator->text());
}

}

BOOST_AUTO_TEST_SUITE(TestTypingIndicator)

BOOST_AUTO_TEST_CASE(TestTextThatFitsIsNotChanged)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator fitting text");
    };

    auto setText=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        indicator->setElidedText("%1 and %2 are typing",{"Alice","Bob"});
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;

        UISE_TEST_CHECK(indicator->text()=="Alice and Bob are typing");
        UISE_TEST_CHECK(labelOf(indicator)->text()=="Alice and Bob are typing");
        UISE_TEST_CHECK(indicator->toolTip().isEmpty());

        // plain text goes the same way
        indicator->setText("typing");
    };

    auto checkPlain=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        UISE_TEST_CHECK(indicator->text()=="typing");
        UISE_TEST_CHECK(labelOf(indicator)->text()=="typing");
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        setText,
        check,
        checkPlain
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestLongNameIsElidedShortOneKept)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator fair eliding");
    };

    auto narrow=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        indicator->setElidedText("%1 and %2 are typing",{"Alexandra Bartholomew Longname-Smythe","Bob"});

        // room for the words, the short name in full and part of the long one: the long name has
        // to give way, and what the short one does not need is what it gets
        const auto width=chromeOf(indicator)
                         +textWidth(indicator," and  are typing")
                         +textWidth(indicator,"Bob")
                         +textWidth(indicator,"Alexandra Bart");
        indicator->setFixedWidth(width);
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        auto shown=labelOf(indicator)->text();

        UISE_TEST_CHECK(shown.contains("Bob"));
        UISE_TEST_CHECK(shown.contains(" and "));
        UISE_TEST_CHECK(shown.endsWith(" are typing"));
        UISE_TEST_CHECK(shown.contains(Ellipsis));
        UISE_TEST_CHECK(shown.startsWith("A"));
        UISE_TEST_CHECK(!shown.contains("Longname"));

        // the whole text is still there, and shows when pointing at the elided one
        UISE_TEST_CHECK(indicator->text()=="Alexandra Bartholomew Longname-Smythe and Bob are typing");
        UISE_TEST_CHECK(indicator->toolTip()==indicator->text());

        // widening gives the name its letters back
        indicator->setFixedWidth(800);
    };

    auto checkWide=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        UISE_TEST_CHECK(labelOf(indicator)->text()==indicator->text());
        UISE_TEST_CHECK(indicator->toolTip().isEmpty());
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        narrow,
        check,
        checkWide
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestEveryNameKeepsItsShareWhenAllAreLong)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator equal shares");
    };

    auto narrow=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        indicator->setElidedText("%1, %2 and %3 are typing",
                                 {"Alexandra Bartholomew","Bartholomew Alexandra","Christopher Montgomery"});
        // the words around the names, and about a third of what the three names would take
        const auto width=chromeOf(indicator)
                         +textWidth(indicator,",  and  are typing")
                         +textWidth(indicator,"Alexandra Bartholomew")*3/2;
        indicator->setFixedWidth(width);
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        auto shown=labelOf(indicator)->text();

        // none of the three is dropped for the benefit of another: each one starts its own part
        UISE_TEST_CHECK(shown.contains("A"));
        UISE_TEST_CHECK(shown.contains("B"));
        UISE_TEST_CHECK(shown.contains("C"));
        UISE_TEST_CHECK(shown.endsWith(" are typing"));
        UISE_TEST_CHECK(shown.count(Ellipsis)>=1);
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        narrow,
        check
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestMaxNameWidthCapsEvenWithRoomToSpare)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator name cap");
    };

    auto cap=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        indicator->setFixedWidth(800);
        indicator->setMaxNameWidth(textWidth(indicator,"Alexandra"));
        UISE_TEST_CHECK_EQUAL(indicator->maxNameWidth(),textWidth(indicator,"Alexandra"));
        indicator->setElidedText("%1 is typing",{"Alexandra Bartholomew Longname-Smythe"});
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        auto shown=labelOf(indicator)->text();

        UISE_TEST_CHECK(shown.startsWith("Al"));
        UISE_TEST_CHECK(shown.contains(Ellipsis));
        UISE_TEST_CHECK(shown.endsWith(" is typing"));
        UISE_TEST_CHECK(!shown.contains("Longname"));
        UISE_TEST_CHECK(indicator->text()=="Alexandra Bartholomew Longname-Smythe is typing");

        // without the cap the same text fits
        indicator->setMaxNameWidth(0);
    };

    auto checkUncapped=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        UISE_TEST_CHECK(labelOf(indicator)->text()==indicator->text());
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        cap,
        check,
        checkUncapped
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestTooNarrowForEvenTheWordsElidesTheWholeText)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator whole text eliding");
    };

    auto narrow=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        indicator->setElidedText("%1 is recording a voice message",{"Alexandra"});
        indicator->setFixedWidth(chromeOf(indicator)+textWidth(indicator,"is recording"));
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        auto shown=labelOf(indicator)->text();

        UISE_TEST_CHECK(shown.contains(Ellipsis));
        UISE_TEST_CHECK(textWidth(indicator,shown)<=indicator->width());

        // a widget that is given little room must not insist on the width of its text
        UISE_TEST_CHECK(indicator->minimumSizeHint().width()<indicator->sizeHint().width());
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        narrow,
        check
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestNamesAreSubstitutedInOnePass)
{
    auto init=[](TypingIndicatorContainerPtr container){
        auto indicator=new TypingIndicator();
        TypingIndicatorContainer::beginTestCase(container,indicator,"Test TypingIndicator substitution");
    };

    auto setText=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        // a title that looks like a placeholder is a title, not a placeholder
        indicator->setElidedText("%1 and %2 are typing",{"%2","Bob"});
    };

    auto check=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        UISE_TEST_CHECK(indicator->text()=="%2 and Bob are typing");
        UISE_TEST_CHECK(labelOf(indicator)->text()=="%2 and Bob are typing");

        // a placeholder without a name stays as it is
        indicator->setElidedText("%1 and %3",{"Alice"});
    };

    auto checkMissing=[](TypingIndicatorContainerPtr container){
        auto indicator=container->testWidget;
        UISE_TEST_CHECK(indicator->text()=="Alice and %3");
    };

    std::vector<std::function<void (TypingIndicatorContainerPtr container)>> steps={
        init,
        setText,
        check,
        checkMissing
    };
    TypingIndicatorContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
