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

/** @file uise/desktop/textviewer.hpp
*
*  Declares TextViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEXTVIEWER_HPP
#define UISE_DESKTOP_TEXTVIEWER_HPP

#include <memory>

#include <QFrame>
#include <QString>
#include <QSize>
#include <QEvent>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class Toast;
class ChatMessageTextBrowser;
class FloatingDialogFrame;
class TextViewer_p;

/**
 * @brief Expanded viewer for a piece of text: a code block lifted out of a chat message, or a
 *  whole text / markdown / source file.
 *
 * One header bar on top -- file-type icon, an elided title (a file name, or a code block's
 * language) with an optional muted subtitle under it (a size), and a row of icon buttons: Find,
 * Copy, Wrap lines, Markdown Source/Rendered (markdown content only), Full screen, an optional
 * "..." menu (see setFileActionsVisible()) and Close -- and the content below it.
 *
 * Find (the button, or Cmd/Ctrl+F) opens a bar under the header: an input, previous, next and
 * close. Every match is painted while typing, the current one is selected, and closing the bar
 * (its X, the Find button, or Escape -- which closes the bar before the viewer) removes it all.
 *
 * The content is shown by the SAME ChatMessageTextBrowser a chat bubble renders through, in
 * viewer mode, so the syntax highlighting, the painted code slab and the markdown rendering are
 * identical to the bubble's rather than approximated by a second browser. It is always shown
 * through markdownToHtml(): code and plain text are wrapped in a fence long enough to survive any
 * backtick run they contain, markdown is rendered as is.
 *
 * Presentational and signal-only, like ChatImageViewerControls: nothing here knows about files,
 * settings or the clipboard-of-record. The "..." menu's actions are emitted as *Requested()
 * signals for the host to implement.
 *
 * Meant to be shown through open(), which puts it into a FloatingDialogFrame with the header as
 * the drag handle. A TextViewer that has not been opened that way still works as an embedded
 * widget, but Full screen and Close then have no frame to act on and do nothing.
 */
class UISE_DESKTOP_EXPORT TextViewer : public QFrame
{
    Q_OBJECT

    public:

        explicit TextViewer(QWidget* parent=nullptr);

        ~TextViewer();

        TextViewer(const TextViewer&)=delete;
        TextViewer(TextViewer&&)=delete;
        TextViewer& operator=(const TextViewer&)=delete;
        TextViewer& operator=(TextViewer&&)=delete;

        /**
         * @brief Show `text` as source code.
         * @param language Fence info string, resolved by SyntaxLanguageRegistry::find() -- an
         *  alias or a file extension such as "py" or ".py". Empty: monospace slab, no highlighting.
         *
         * Wrapping starts off, as it does for every code block.
         */
        void setCode(const QString& text, const QString& language);

        /**
         * @brief Show `text` as markdown, rendered.
         *
         * Wrapping starts on, since rendered prose has to. The Markdown Source/Rendered button
         * appears, and switches between this rendering and the source in a highlight-free slab.
         */
        void setMarkdown(const QString& text);

        //! Show `text` verbatim in a monospace slab, no highlighting. Same as setCode(text,{}).
        void setPlainText(const QString& text);

        //! The text last passed to setCode()/setMarkdown()/setPlainText(), as given.
        const QString& text() const noexcept;

        /**
         * @brief The browser doing the rendering, for a host that wants to tune it (code slab
         *  padding and radius, syntax highlighting on/off).
         *
         * Settings changed on it BEFORE the setCode()/setMarkdown()/setPlainText() call take
         * effect on that content; the content setters do not reset them.
         */
        ChatMessageTextBrowser* browser() const noexcept;

        /**
         * @brief Header title -- a file name, or a code block's language.
         *
         * Elided in the middle so a file name keeps its extension, with the full text as the
         * tooltip, and mirrored into the frame's window title (visible in a task switcher even
         * though the frame has no title bar).
         */
        void setTitle(const QString& title);

        //! Muted line under the title, e.g. a file size. Hidden while empty.
        void setSubtitle(const QString& subtitle);

        /**
         * @brief Show the "..." menu: Open in system app, Save as, and the "Always" row.
         *
         * Off by default -- a code block lifted out of a message has no file behind it. Every
         * row is only a signal, see openExternallyRequested()/saveAsRequested()/
         * alwaysExternalToggled().
         */
        void setFileActionsVisible(bool visible);

        bool isFileActionsVisible() const noexcept;

        //! Text of the checkable "Always ..." menu row, e.g. "Always open .md files in system app".
        //! Empty removes the row (and the separator above it).
        void setAlwaysExternalText(const QString& text);

        //! Check state of that row. Does not emit alwaysExternalToggled().
        void setAlwaysExternalChecked(bool checked);

        /**
         * @brief Toast for the "Copied" confirmation, also used by the browser's own copy menu.
         *
         * Copy puts text() on the clipboard as given -- for markdown that is the source, not the
         * rendered text. Not owned; without one Copy confirms nothing.
         */
        void setToast(Toast* toast);

        /**
         * @brief Initial size for an expanded viewer, in window coordinates.
         *
         * At least `scale` times HALF the window `anchor` lives in, in each direction (and `scale`
         * times the 400px height floor): the point of expanding is to see more than a bubble
         * showed. `contentWidth` still wins where it is wider, and the window itself is the
         * ceiling. Shared by every expanded viewer (this one and ChatMessageTextBrowser's table
         * viewer), which is why the scale is a parameter: 1 is the size they were given first,
         * open() asks for TextViewer::DefaultScale.
         */
        static QSize expandedSize(const QWidget* anchor, int contentWidth, qreal scale=1.0);

        //! How much larger than the table viewer's size a TextViewer opens: reading a file or a
        //! long code block wants more room than glancing at a table does.
        constexpr static qreal DefaultScale=1.5;

        /**
         * @brief Pop `frame` up at `size`, then let the user shrink it again.
         *
         * Sizing a FloatingDialogFrame is not a resize() away: popup() calls adjustSize(), which
         * takes the frame's size hint and throws away any geometry set beforehand. minimumSize is
         * the one channel adjustSize() must honour, so the size is imposed that way and relaxed to
         * a usable floor once the frame has taken it. Relaxing afterwards resizes nothing; it only
         * stops the initial size from becoming a lower bound the user cannot drag back.
         */
        static void popupSized(QWidget* container, FloatingDialogFrame* frame, const QSize& size);

        /**
         * @brief Put `viewer` into a new FloatingDialogFrame parented to `anchor` and pop it up.
         * @param contentWidth Preferred width of the content, see expandedSize().
         * @return The frame, already shown. It deletes itself, and the viewer with it, when closed.
         */
        static FloatingDialogFrame* open(TextViewer* viewer, QWidget* anchor, int contentWidth);

    signals:

        //! "Open in system app" was chosen in the "..." menu.
        void openExternallyRequested();

        //! "Save as" was chosen in the "..." menu.
        void saveAsRequested();

        //! The checkable "Always ..." row was toggled, `checked` being its new state.
        void alwaysExternalToggled(bool checked);

    protected:

        //! Enter / Shift+Enter in the find bar's input step to the next / previous match.
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        void toggleFullScreen();

        std::unique_ptr<TextViewer_p> pimpl;

        friend class TextViewer_p;
};

}

#endif // UISE_DESKTOP_TEXTVIEWER_HPP
