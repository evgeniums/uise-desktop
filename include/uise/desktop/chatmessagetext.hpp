/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chatmessagetext.hpp
*
*  Declares ChatMessageText.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGETEXT_HPP
#define UISE_DESKTOP_CHATMESSAGETEXT_HPP

#include <QTextBrowser>
#include <QColor>
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT ChatMessageTextBrowser : public QTextBrowser
{
    Q_OBJECT

    // task-urls-and characters-in-messages.md, Stage 1: QSS can't reach an inline <a>'s color --
    // it comes from the document's char formats, not the widget's own palette/stylesheet -- so
    // these are exposed as qproperty- settable from QSS instead (same idiom as maxBubbleWidth on
    // AbstractChatMessageText, see chat.qss).
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor)
    Q_PROPERTY(bool linkUnderline READ linkUnderline WRITE setLinkUnderline)

    public:

        explicit ChatMessageTextBrowser(QWidget *parent = nullptr);

        void setMessageTextWidget(AbstractChatMessageText* widget);

        AbstractChatMessageText* messageTextWidget() const
        {
            return m_messageTextWidget;
        }

        QSize sizeHint() const override;

        void setWrapWidth(int w);

        int textWidthHint() const;

        /**
         * @brief Toggle focusability + a minimal Copy/Select All context menu.
         * @param enable Off by default -- see AbstractChatMessageBody::setCopyable()'s own doc
         *  comment for why (live chat page vs. a static preview like AbstractReplyDialog's).
         *  Deliberately NOT createStandardContextMenu(): that pulls in Qt's full standard
         *  action set (Cut/Paste, irrelevant since this is read-only, and "Copy Link Location"
         *  for any markdown-rendered link, which a chat message has no meaningful separate
         *  "location" to expose) -- this shows only what actually applies here.
         */
        void setCopyable(bool enable);

        bool isCopyable() const noexcept
        {
            return m_copyable;
        }

        /**
         * @brief Suppress this widget's own built-in Copy/Select All context menu.
         * @param enable On by default -- a host showing this widget's own message-level context
         *  menu instead (e.g. a static preview bubble embedded in a dialog) turns this off so the
         *  two menus don't compete over the same right-click. Independent of setCopyable(): with
         *  this off, the widget stays focusable/selectable (Ctrl+C still works), it just never
         *  pops its own menu -- see updateContextMenuPolicy().
         */
        void setOwnContextMenuEnabled(bool enable);

        bool isOwnContextMenuEnabled() const noexcept
        {
            return m_ownContextMenu;
        }

        //! Like QTextBrowser::setHtml(), but also remembers `html` so linkColor/linkUnderline can
        //! REAPPLY it after rebuilding the document's stylesheet (setDefaultStyleSheet() only
        //! affects content set afterwards -- see applyLinkStyle()). ChatMessageText::loadText()'s
        //! Html branch calls this instead of setHtml() directly; setPlainText()/setMarkdown() are
        //! unaffected -- Stage 1 never produces a link outside the Html path (see chattextrender.h),
        //! so plain/markdown content has nothing to re-style on a later color/theme change.
        void setHtmlContent(const QString& html);

        QColor linkColor() const noexcept
        {
            return m_linkColor;
        }
        void setLinkColor(const QColor& color);

        //! The NON-hovered baseline underline state -- a hovered anchor is always underlined
        //! regardless of this property (see updateHoveredAnchor()), reverting to this value once
        //! the mouse leaves it.
        bool linkUnderline() const noexcept
        {
            return m_linkUnderline;
        }
        void setLinkUnderline(bool enable);

    public slots:

        void updateSize();

    signals:

        //! See AbstractChatMessageBody::linkActivated() -- ChatMessageText relays this signal
        //! there. Emitted from anchorClicked(), not from a mouse-press handler, so link
        //! activation always uses Qt's own hit-testing (hyperlinks can wrap across lines, etc.).
        void linkActivated(const QUrl& url);

    protected:

        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private slots:

        void showCopyMenu(const QPoint& pos);

    private:

        void applyLinkStyle();

        //! Recomputes contextMenuPolicy() from m_copyable/m_ownContextMenu -- the two setters
        //! share this instead of each duplicating the combination.
        void updateContextMenuPolicy();

        //! Underlines/un-underlines the hovered anchor at `pos` (task-urls-and characters-in-
        //! messages.md follow-up: hover feedback on hyperlinks). Cursor shape (arrow vs. pointing
        //! hand) is NOT handled here -- QTextEdit's own base mouseMoveEvent() already does that
        //! automatically for any anchor once LinksAccessibleByMouse is set (the QTextBrowser
        //! default), so duplicating it here would only risk fighting Qt's own IBeam/pointing-hand
        //! switching over plain text vs. a link.
        void updateHoveredAnchor(const QPoint& pos);

        //! Reverts the currently-hovered anchor (if any) back to the base linkUnderline() state.
        //! Called on leaveEvent() and whenever mouseMoveEvent() can't forward to Qt's own anchor
        //! hit-testing (selection mode / outside the widget rect) -- both cases would otherwise
        //! leave a stale hover-underline behind with no further mouse-move event to clear it.
        void clearHoveredAnchor();

        //! Sets fontUnderline on every char-format run in the document whose anchorHref() equals
        //! `href` (a chat bubble's text is short, so a full block/fragment walk per call is cheap).
        void setAnchorUnderline(const QString& href, bool enable);

        AbstractChatMessageText* m_messageTextWidget=nullptr;
        bool m_copyable=false;
        bool m_ownContextMenu=true;
        QColor m_linkColor;
        bool m_linkUnderline=false;
        QString m_lastHtml;
        QString m_hoveredAnchor;
};

class ChatMessageText_p;

class UISE_DESKTOP_EXPORT ChatMessageText : public AbstractChatMessageText
{
    Q_OBJECT

    public:

        ChatMessageText(QWidget* parent=nullptr);

        ~ChatMessageText();
        ChatMessageText(const ChatMessageText&)=delete;
        ChatMessageText& operator=(const ChatMessageText&)=delete;
        ChatMessageText(ChatMessageText&&)=delete;
        ChatMessageText& operator=(ChatMessageText&&)=delete;

        void loadText(const QString& text, TextFormat format=TextFormat::Markdown) override;

        void clearText() override;

        void clearContentSelection() override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

        QString selectedText() const override;

        bool hasSelectableText() const override;

        void setCopyable(bool enable) override;

        void setOwnContextMenuEnabled(bool enable) override;

        void selectText(const QString& text) override;

        QString linkAt(const QPoint& pos) const override;

    protected:

        void updateChatMessage() override;

    private:

        void adjustWrapWidth(int& value, bool add);
        std::unique_ptr<ChatMessageText_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGETEXT_HPP
