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

/** @file uise/desktop/src/chatobjectsearchsnippet.cpp
*
*  Defines ChatObjectSearchSnippet and chatObjectHighlightHtml().
*
*/

/****************************************************************************/

#include <algorithm>
#include <cstdint>
#include <utility>

#include <QEvent>
#include <QFontMetrics>
#include <QSizePolicy>

#include <uise/desktop/chatobjectsearchsnippet.hpp>

namespace uise {

namespace {

//! "rgba(r,g,b,a)" with a in 0..255, the form the style sheets of the library use (the rich text of a label understands it).
QString cssColor(const QColor& color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

bool isHighSurrogate(QChar ch)
{
    return ch.isHighSurrogate();
}

bool isLowSurrogate(QChar ch)
{
    return ch.isLowSurrogate();
}

//! Ranges that are in the text, as [begin,end) pairs: sorted, not overlapping and not adjacent.
std::vector<std::pair<int,int>> normalizeRanges(const QString& text, const std::vector<ChatObjectTextRange>& ranges)
{
    const qint64 size=text.size();

    std::vector<std::pair<int,int>> pairs;
    pairs.reserve(ranges.size());
    for (const auto& range : ranges)
    {
        if (range.length<=0)
        {
            continue;
        }

        // 64 bit: a start and a length that are both big must not wrap around
        qint64 begin=std::max<qint64>(0,range.start);
        qint64 end=std::min<qint64>(size,static_cast<qint64>(range.start)+static_cast<qint64>(range.length));
        if (begin>=end)
        {
            continue;
        }

        // a pair of UTF-16 units is one character: do not cut it in two
        if (begin>0 && isLowSurrogate(text.at(static_cast<int>(begin))) && isHighSurrogate(text.at(static_cast<int>(begin-1))))
        {
            --begin;
        }
        if (end<size && isLowSurrogate(text.at(static_cast<int>(end))) && isHighSurrogate(text.at(static_cast<int>(end-1))))
        {
            ++end;
        }

        pairs.emplace_back(static_cast<int>(begin),static_cast<int>(end));
    }

    std::sort(pairs.begin(),pairs.end());

    std::vector<std::pair<int,int>> merged;
    merged.reserve(pairs.size());
    for (const auto& pair : pairs)
    {
        // touching ranges are one range: no empty gap between two spans
        if (!merged.empty() && pair.first<=merged.back().second)
        {
            merged.back().second=std::max(merged.back().second,pair.second);
        }
        else
        {
            merged.push_back(pair);
        }
    }
    return merged;
}

} // anonymous namespace

//--------------------------------------------------------------------------

QString chatObjectHighlightHtml(
        const QString& text,
        const std::vector<ChatObjectTextRange>& ranges,
        const QColor& color,
        const QColor& background
    )
{
    // One to one, so that the offsets of the ranges stay valid.
    QString plain=text;
    for (QChar& ch : plain)
    {
        const char16_t unit=ch.unicode();
        if (unit==u'\n' || unit==u'\r' || unit==u'\t' || unit==u'\v' || unit==u'\f' || unit==0x2028 || unit==0x2029)
        {
            ch=QLatin1Char(' ');
        }
    }

    const auto merged=normalizeRanges(plain,ranges);
    if (merged.empty())
    {
        return plain.toHtmlEscaped();
    }

    // the style of a match
    QString style;
    if (color.isValid())
    {
        style+=QStringLiteral("color:%1;").arg(cssColor(color));
    }
    if (background.isValid())
    {
        style+=QStringLiteral("background-color:%1;").arg(cssColor(background));
    }
    if (style.isEmpty())
    {
        // no colours (a host without the style sheet): a match must still be told from the rest
        style=QStringLiteral("font-weight:600;");
    }
    const QString open=QStringLiteral("<span style=\"%1\">").arg(style);
    const QString close=QStringLiteral("</span>");

    QString html;
    int pos=0;
    for (const auto& range : merged)
    {
        html+=plain.mid(pos,range.first-pos).toHtmlEscaped();
        html+=open;
        html+=plain.mid(range.first,range.second-range.first).toHtmlEscaped();
        html+=close;
        pos=range.second;
    }
    html+=plain.mid(pos).toHtmlEscaped();
    return html;
}

//--------------------------------------------------------------------------

ChatObjectSearchSnippet::ChatObjectSearchSnippet(QWidget* parent)
    : QLabel(parent)
{
    setTextFormat(Qt::RichText);
    setWordWrap(true);
    // a click is a click on the row, and the row has its own context menu
    setTextInteractionFlags(Qt::NoTextInteraction);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // A long word must not widen the list; the height follows the width.
    setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    updateMaxHeight();
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::setSnippet(const QString& text, std::vector<ChatObjectTextRange> ranges)
{
    m_snippet=text;
    m_ranges=std::move(ranges);
    render();
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::setHighlightColor(const QColor& color)
{
    if (m_highlightColor==color)
    {
        return;
    }
    m_highlightColor=color;
    render();
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::setHighlightBackground(const QColor& color)
{
    if (m_highlightBackground==color)
    {
        return;
    }
    m_highlightBackground=color;
    render();
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::render()
{
    setText(chatObjectHighlightHtml(m_snippet,m_ranges,m_highlightColor,m_highlightBackground));
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::updateMaxHeight()
{
    // three lines of the font in effect, as the other snippets of the rows
    setMaximumHeight(MaxLines*fontMetrics().lineSpacing()+2);
}

//--------------------------------------------------------------------------

void ChatObjectSearchSnippet::changeEvent(QEvent* event)
{
    // the style sheet sets the font after the label is built
    if (event->type()==QEvent::FontChange || event->type()==QEvent::StyleChange)
    {
        updateMaxHeight();
    }
    QLabel::changeEvent(event);
}

} // namespace uise
