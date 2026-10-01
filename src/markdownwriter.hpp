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

/** @file src/markdownwriter.hpp
*
*  Private, non-wrapping replacement for QTextDocument::toMarkdown().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MARKDOWN_WRITER_HPP
#define UISE_DESKTOP_MARKDOWN_WRITER_HPP

#include <QString>

class QTextDocument;

namespace uise {

namespace detail {

/**
 * @brief QTextDocument::toMarkdown() without its hard wrap at 80 columns.
 *
 * Qt's writer breaks prose into 80-column lines (`ColumnLimit` is a local constant inside
 * QTextMarkdownWriter::writeBlock(), no API reaches it). In a chat those breaks are
 * indistinguishable from line breaks the user typed -- the renderer keeps every newline -- and a
 * wrap between an emphasis marker and its text even writes the marker BEFORE the newline, which no
 * parser reads as emphasis. A paragraph here is therefore exactly one line, and every newline in
 * the output is one the user typed (a U+2028 inside a block).
 *
 * Otherwise a transcription of Qt 6.9.3's qtextmarkdownwriter.cpp built on public API only, so the
 * output for everything that is not long prose (lists, quotes, tables, code, links, images,
 * escaping) is byte-identical to Qt's. Front matter is not written.
 */
QString documentToMarkdown(const QTextDocument* document);

} // namespace detail

} // namespace uise

#endif // UISE_DESKTOP_MARKDOWN_WRITER_HPP
