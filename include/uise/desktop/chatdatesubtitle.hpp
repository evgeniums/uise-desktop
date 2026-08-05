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

/** @file uise/desktop/chatdatesubtitle.hpp
*
*  Declares ChatDateSubtitle – a floating date pill shown over a scrolled chat messages view.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATDATESUBTITLE_HPP
#define UISE_DESKTOP_CHATDATESUBTITLE_HPP

#include <memory>

#include <QDateTime>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractChatSeparatorSection;

class ChatDateSubtitle_p;

/**
 * @brief Floating date subtitle overlaid on a scrolled chat messages view.
 *
 * The subtitle is normally invisible. While the user scrolls the chat messages view it fades in
 * showing the date of the topmost visible message, and fades out again shortly after scrolling
 * stops. It looks and behaves exactly as the date section of an inline chat separator
 * (AbstractChatSeparatorSection with type() == AbstractChatSeparatorSection::TypeDate), because it
 * reuses that same section widget internally.
 *
 * The widget is intended to be a child of the viewport of the chat messages view's
 * FlyweightListView (see FlyweightListView::viewportFrame()) so that it floats over the actually
 * scrolling content rather than over the whole view (which may include scrollbars and padding).
 *
 * @note The reused section widget is never attached to a chat message (setChatMessage() is never
 * called on it), so a custom AbstractChatSeparatorSection implementation registered in the widget
 * factory must not dereference chatMessage().
 */
class UISE_DESKTOP_EXPORT ChatDateSubtitle : public WidgetQFrame
{
    Q_OBJECT

    Q_PROPERTY(int showDelayMs READ showDelayMs WRITE setShowDelayMs)
    Q_PROPERTY(int hideDelayMs READ hideDelayMs WRITE setHideDelayMs)
    Q_PROPERTY(int fadeInDurationMs READ fadeInDurationMs WRITE setFadeInDurationMs)
    Q_PROPERTY(int fadeOutDurationMs READ fadeOutDurationMs WRITE setFadeOutDurationMs)
    Q_PROPERTY(int topOffset READ topOffset WRITE setTopOffset)
    Q_PROPERTY(qreal maxOpacity READ maxOpacity WRITE setMaxOpacity)
    Q_PROPERTY(qreal subtitleOpacity READ subtitleOpacity WRITE setSubtitleOpacity)

    public:

        constexpr static const int DefaultShowDelayMs=150;
        constexpr static const int DefaultHideDelayMs=1200;
        constexpr static const int DefaultFadeInDurationMs=150;
        constexpr static const int DefaultFadeOutDurationMs=300;
        constexpr static const int DefaultTopOffset=0;
        constexpr static const qreal DefaultMaxOpacity=1.0;

        explicit ChatDateSubtitle(QWidget* parent=nullptr);

        ~ChatDateSubtitle();
        ChatDateSubtitle(const ChatDateSubtitle&)=delete;
        ChatDateSubtitle& operator=(const ChatDateSubtitle&)=delete;
        ChatDateSubtitle(ChatDateSubtitle&&)=delete;
        ChatDateSubtitle& operator=(ChatDateSubtitle&&)=delete;

        /**
         * @brief Get the reused separator section widget that renders the date pill.
         * @return Section widget.
         */
        AbstractChatSeparatorSection* section() const;

        /**
         * @brief Set date/time to display and update the pill's text.
         * @param dt Date/time of the topmost visible message.
         * @param withYear If true, append the year to the displayed date.
         */
        void setDateTime(const QDateTime& dt, bool withYear=false);

        /**
         * @brief Get date/time last set with setDateTime().
         */
        QDateTime dateTime() const;

        /**
         * @brief Get the text currently displayed in the pill.
         */
        QString text() const;

        int showDelayMs() const;
        void setShowDelayMs(int value);

        int hideDelayMs() const;
        void setHideDelayMs(int value);

        int fadeInDurationMs() const;
        void setFadeInDurationMs(int value);

        int fadeOutDurationMs() const;
        void setFadeOutDurationMs(int value);

        //! @brief Vertical offset from the top of the parent widget.
        int topOffset() const;
        void setTopOffset(int value);

        //! @brief Maximum (fully visible) opacity to fade in to.
        qreal maxOpacity() const;
        void setMaxOpacity(qreal value);

        //! @brief Current animated opacity. Not meant to be set directly by client code.
        qreal subtitleOpacity() const;
        void setSubtitleOpacity(qreal value);

    public slots:

        /**
         * @brief Notify the subtitle that the user scrolled the chat messages view.
         *
         * Schedules a delayed fade-in (if currently hidden) and (re)schedules a delayed fade-out.
         */
        void notifyScrolled();

        /**
         * @brief (Re)schedule the delayed fade-out without a preceding scroll notification.
         */
        void hideDelayed();

        /**
         * @brief Cancel any pending timers/animation and hide immediately.
         */
        void hideNow();

    signals:

        //! @todo Bind to opening a calendar to jump to a date, similar to Telegram Desktop.
        void clicked();

    protected:

        bool event(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        void updatePosition();
        void fadeIn();
        void fadeOut();

        std::unique_ptr<ChatDateSubtitle_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATDATESUBTITLE_HPP
