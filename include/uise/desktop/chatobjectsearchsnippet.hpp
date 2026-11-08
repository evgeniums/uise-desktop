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

/** @file uise/desktop/chatobjectsearchsnippet.hpp
*
*  Declares ChatObjectSearchSnippet, the label of a search result row of the chat object browsers that shows a snippet
*  of a message with the found words highlighted, and chatObjectHighlightHtml(), the function that builds its text.
*  See docs/chat-object-browsers.md.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTSEARCHSNIPPET_HPP
#define UISE_DESKTOP_CHATOBJECTSEARCHSNIPPET_HPP

#include <vector>

#include <QColor>
#include <QLabel>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatobjectitem.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not match what moc (a real
// preprocessor) resolves at runtime. See task-localization-framework.md.
namespace uise {

/**
 * @brief Rich text (HTML) of a snippet with the ranges highlighted.
 *
 * - Whitespace characters (\\n \\r \\t, \\v, \\f, U+2028, U+2029) are replaced with a space ONE TO ONE, so that the offsets
 *   of the ranges stay valid. (QString::simplified() would change them.)
 * - Ranges are in UTF-16 code units of \a text. They are taken defensively: empty ones are dropped, the rest is clamped to
 *   the text, a range never splits a surrogate pair, overlapping and adjacent ranges are merged, the order does not matter.
 *   No valid range means no highlight at all.
 * - Every piece of the text is escaped with toHtmlEscaped(); a matched piece is wrapped in a span with the colours. An
 *   invalid colour is left out; with no valid colour at all a match is told by its weight.
 *
 * Pure function, so that it is tested on its own.
 */
UISE_DESKTOP_EXPORT QString chatObjectHighlightHtml(
        const QString& text,
        const std::vector<ChatObjectTextRange>& ranges,
        const QColor& color,
        const QColor& background
    );

/**
 * @brief A label for a snippet of a message with the found words highlighted.
 *
 * Rich text, word wrapped, not taller than three lines whatever the width (the rest is cut off at the bottom, nothing is
 * elided), never wider than the room it is given. Not selectable: a click goes to the row it is in.
 *
 * Styled by QSS, the colours of the highlight with qproperty- (see light/ and dark/ chatobjectbrowser.qss).
 */
class UISE_DESKTOP_EXPORT ChatObjectSearchSnippet : public QLabel
{
    Q_OBJECT

    /**
     * QSS: qproperty-highlightColor: #0A66C2; -- colour of the found words. An invalid colour keeps the colour of the text.
     * Changing it renders the snippet again.
     */
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor)

    /**
     * QSS: qproperty-highlightBackground: rgba(10,102,194,28); -- background of the found words. An invalid colour leaves the
     * background alone. Changing it renders the snippet again.
     */
    Q_PROPERTY(QColor highlightBackground READ highlightBackground WRITE setHighlightBackground)

    public:

        //! The most lines the label shows.
        constexpr static const int MaxLines=3;

        explicit ChatObjectSearchSnippet(QWidget* parent=nullptr);

        /**
         * @brief Set the text and the ranges to highlight in it.
         * @param text Snippet, already cut by the host.
         * @param ranges UTF-16 ranges in \a text, see chatObjectHighlightHtml().
         */
        void setSnippet(const QString& text, std::vector<ChatObjectTextRange> ranges);

        void setHighlightColor(const QColor& color);

        QColor highlightColor() const noexcept
        {
            return m_highlightColor;
        }

        void setHighlightBackground(const QColor& color);

        QColor highlightBackground() const noexcept
        {
            return m_highlightBackground;
        }

        //! The snippet as it was set (not its HTML).
        const QString& snippetText() const noexcept
        {
            return m_snippet;
        }

    protected:

        void changeEvent(QEvent* event) override;

    private:

        void render();
        void updateMaxHeight();

        QString m_snippet;
        std::vector<ChatObjectTextRange> m_ranges;
        QColor m_highlightColor;
        QColor m_highlightBackground;
};

} // namespace uise

#endif // UISE_DESKTOP_CHATOBJECTSEARCHSNIPPET_HPP
