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

/** @file uise/test/utils/testfilenamevalidator.cpp
*
*  Test FileNameValidator.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/desktop/utils/filenamevalidator.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

using Problem=FileNameValidator::Problem;

BOOST_AUTO_TEST_SUITE(TestFileNameValidator)

BOOST_AUTO_TEST_CASE(Check)
{
    struct Case
    {
        QString name;
        Problem expected;
    };

    const std::vector<Case> cases{
        {QStringLiteral("a.txt"),Problem::None},
        {QStringLiteral("a.b.c"),Problem::None},
        {QStringLiteral("my file (1).tar.gz"),Problem::None},
        {QStringLiteral("console.txt"),Problem::None},
        {QStringLiteral("COM10"),Problem::None},
        {QString::fromUtf8("\xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB.txt"),Problem::None},
        {QString::fromUtf8("\xF0\x9F\x98\x80.png"),Problem::None},
        {QStringLiteral("a/b"),Problem::ForbiddenChar},
        {QStringLiteral("a\\b"),Problem::ForbiddenChar},
        {QStringLiteral("a:b"),Problem::ForbiddenChar},
        {QStringLiteral("a*b"),Problem::ForbiddenChar},
        {QStringLiteral("a?b"),Problem::ForbiddenChar},
        {QStringLiteral("a\"b"),Problem::ForbiddenChar},
        {QStringLiteral("a<b>"),Problem::ForbiddenChar},
        {QStringLiteral("a|b"),Problem::ForbiddenChar},
        {QStringLiteral("a\tb"),Problem::ForbiddenChar},
        {QString(256,QLatin1Char('a')),Problem::TooLong},
        {QString(128,QChar(0x0444)),Problem::TooLong},
        {QString(),Problem::Empty},
        {QStringLiteral("   "),Problem::Empty},
        {QStringLiteral(" a"),Problem::EdgeWhitespace},
        {QStringLiteral("a "),Problem::EdgeWhitespace},
        {QStringLiteral("."),Problem::LeadingDot},
        {QStringLiteral(".."),Problem::LeadingDot},
        {QStringLiteral(".hidden"),Problem::LeadingDot},
        {QStringLiteral("a."),Problem::TrailingDot},
        {QStringLiteral("a..b"),Problem::ConsecutiveDots},
        {QStringLiteral("CON"),Problem::ReservedName},
        {QStringLiteral("con.txt"),Problem::ReservedName},
        {QStringLiteral("Lpt1.tar.gz"),Problem::ReservedName},
        {QStringLiteral("nul"),Problem::ReservedName},
    };

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("name: " << c.name.toStdString())
        {
            BOOST_CHECK(FileNameValidator::check(c.name)==c.expected);
            BOOST_CHECK_EQUAL(FileNameValidator::isValid(c.name),c.expected==Problem::None);
        }
    }
}

BOOST_AUTO_TEST_CASE(ValidatorStates)
{
    FileNameValidator validator;
    auto state=[&validator](QString text)
    {
        int pos=0;
        return validator.validate(text,pos);
    };

    BOOST_CHECK(state(QStringLiteral("a.txt"))==QValidator::Acceptable);
    BOOST_CHECK(state(QStringLiteral("a/b"))==QValidator::Invalid);
    BOOST_CHECK(state(QStringLiteral("a..b"))==QValidator::Intermediate);
    BOOST_CHECK(state(QString())==QValidator::Intermediate);
    BOOST_CHECK(state(QStringLiteral("con"))==QValidator::Intermediate);
}

BOOST_AUTO_TEST_SUITE_END()
