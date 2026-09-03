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

/** @file uise/desktop/chatfloatingavatar.hpp
*
*  Declares ChatFloatingAvatar – a floating sender avatar shown over a scrolled chat messages view.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATFLOATINGAVATAR_HPP
#define UISE_DESKTOP_CHATFLOATINGAVATAR_HPP

#include <memory>
#include <optional>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AbstractChatMessage;
class AvatarWidget;

class ChatFloatingAvatar_p;

/**
 * @brief Floating sender avatar overlaid on a scrolled chat messages view.
 *
 * A batch's sender avatar is normally anchored beside the tail of the batch's LAST message (see
 * ChatMessageAvatar/ChatMessage::updateAvatarForced()). Once that message scrolls below the
 * viewport, so does its avatar, leaving a long batch on screen with no visible indication of who
 * sent it. This widget is a second, independent copy of that avatar, pinned near the bottom of
 * the viewport for as long as setWanted(true) holds.
 *
 * The widget is intended to be a child of the viewport of the chat messages view's
 * FlyweightListView (see FlyweightListView::viewportFrame()), the same way ChatDateSubtitle is,
 * so that it floats over the actually scrolling content and gets clipped by the viewport's own
 * bottom edge rather than spilling past it.
 *
 * Unlike ChatDateSubtitle, this widget is not tied to a scroll session: it stays on screen for as
 * long as setWanted(true) holds -- there is no scroll-driven fade-out timer, and (unlike an
 * earlier design of this class) it does NOT itself fade out again once the row it represents
 * scrolls back into view carrying its own anchored avatar. That handoff is resolved the other
 * way around: ChatMessagesView::updateFloatingAvatar() instantly (no fade)
 * AbstractChatMessage::setAvatarObscured()s the row's own avatar the moment this widget would
 * sit over it, so only ever one of the two is rendered at a given spot, and this widget itself
 * just keeps tracking smoothly with no animation of its own at that handoff.
 */
class UISE_DESKTOP_EXPORT ChatFloatingAvatar : public WidgetQFrame
{
    Q_OBJECT

    Q_PROPERTY(int showDelayMs READ showDelayMs WRITE setShowDelayMs)
    Q_PROPERTY(int hideDelayMs READ hideDelayMs WRITE setHideDelayMs)
    Q_PROPERTY(int fadeInDurationMs READ fadeInDurationMs WRITE setFadeInDurationMs)
    Q_PROPERTY(int fadeOutDurationMs READ fadeOutDurationMs WRITE setFadeOutDurationMs)
    Q_PROPERTY(int bottomOffset READ bottomOffset WRITE setBottomOffset)
    Q_PROPERTY(qreal maxOpacity READ maxOpacity WRITE setMaxOpacity)
    Q_PROPERTY(qreal avatarOpacity READ avatarOpacity WRITE setAvatarOpacity)
    Q_PROPERTY(int occlusionMargin READ occlusionMargin WRITE setOcclusionMargin)
    Q_PROPERTY(int modeChangeBlockMs READ modeChangeBlockMs WRITE setModeChangeBlockMs)

    public:

        constexpr static const int DefaultShowDelayMs=0;
        constexpr static const int DefaultHideDelayMs=0;
        //! 0, i.e. no animation at all: this copy appears exactly where a row's own avatar is
        //! hidden in the same pass (and vice versa), so anything but a straight swap reads as a
        //! blink with neither of them on screen. Kept as knobs only so a host can opt back in.
        constexpr static const int DefaultFadeInDurationMs=0;
        constexpr static const int DefaultFadeOutDurationMs=0;
        constexpr static const int DefaultBottomOffset=8;
        constexpr static const qreal DefaultMaxOpacity=1.0;
        constexpr static const int DefaultOcclusionMargin=4;
        constexpr static const int DefaultModeChangeBlockMs=400;

        explicit ChatFloatingAvatar(QWidget* parent=nullptr);

        ~ChatFloatingAvatar();
        ChatFloatingAvatar(const ChatFloatingAvatar&)=delete;
        ChatFloatingAvatar& operator=(const ChatFloatingAvatar&)=delete;
        ChatFloatingAvatar(ChatFloatingAvatar&&)=delete;
        ChatFloatingAvatar& operator=(ChatFloatingAvatar&&)=delete;

        /**
         * @brief Get the inner avatar widget that actually renders the image.
         */
        AvatarWidget* avatar() const;

        /**
         * @brief Bind to the message this floating avatar currently represents and copy its
         * avatar source/path/name/size onto the inner avatar widget.
         *
         * No-op on every piece of data that did not change since the last call -- this is invoked
         * on every scroll frame, and re-applying an unchanged image path/source would re-trigger
         * that source's own (potentially costly) pixmap fetch for nothing.
         *
         * @param msg Message to represent, or nullptr to unbind (does not by itself hide the
         *  widget -- use hideNow()/setWanted(false) for that).
         */
        void setMessage(AbstractChatMessage* msg);

        //! Currently represented message, or nullptr. May go stale (a flyweight-dropped row) --
        //! reads back null rather than dangling.
        AbstractChatMessage* message() const;

        //! Horizontal anchor: the avatar COLUMN of the row this copy stands in for, in the
        //! parent's coordinates. The image is centred in it exactly the way a row centres its own
        //! avatar (Qt::AlignHCenter, i.e. left+(width-imageWidth)/2), so the two line up to the
        //! pixel. Passing a centre point instead does NOT -- QRect::center() rounds as
        //! left+(width-1)/2, which lands a pixel off on an even-width column.
        void setTargetColumn(int left, int width);

        //! Lower bound clamp on this widget's top edge, in parent coordinates -- see this class's
        //! own doc comment and ChatMessagesView::updateFloatingAvatar()'s geometry algorithm.
        void setClampTopY(int y);

        //! Where the row's OWN anchored avatar -- the one this copy stands in for -- currently
        //! sits, in parent coordinates, or nullopt when that is unknown. This copy never parks
        //! BELOW that spot: once the batch's tail has scrolled far enough into view that its own
        //! avatar is above the natural bottom position, the two coincide exactly, which is what
        //! makes the handoff (that row going setAvatarObscured()) invisible. Without it the copy
        //! would keep sinking to the viewport's bottom edge and detach from its batch -- most
        //! visibly in a chat too short to fill the viewport, where it would sit alone in the
        //! empty space below the last message.
        void setAnchoredTopY(std::optional<int> y);

        int showDelayMs() const;
        void setShowDelayMs(int value);

        int hideDelayMs() const;
        void setHideDelayMs(int value);

        int fadeInDurationMs() const;
        void setFadeInDurationMs(int value);

        int fadeOutDurationMs() const;
        void setFadeOutDurationMs(int value);

        //! Gap between the viewport's bottom edge and this widget's own, in the widget's natural
        //! (unclamped) position.
        int bottomOffset() const;
        void setBottomOffset(int value);

        //! Maximum (fully visible) opacity to fade in to.
        qreal maxOpacity() const;
        void setMaxOpacity(qreal value);

        //! Current animated opacity. Not meant to be set directly by client code.
        qreal avatarOpacity() const;
        void setAvatarOpacity(qreal value);

        //! Extra vertical slack (px), subtracted from the viewport's bottom edge, the embedding
        //! view adds when testing whether a row's own anchored avatar has come far enough into
        //! view to be the one this floating copy is covering -- so the handoff (which row gets
        //! setAvatarObscured()) does not chatter right at the boundary.
        int occlusionMargin() const;
        void setOcclusionMargin(int value);

        //! How long the embedding view suppresses this copy entirely after the chat switches
        //! between showing and hiding per-message avatars. That switch relayouts every row (the
        //! avatar column appears or disappears, bubbles change side), so the geometry this copy
        //! is positioned from is in flux for a moment; parking it and letting the rows carry
        //! their own avatars until things settle avoids placing it against half-applied
        //! geometry. Read by ChatMessagesView, which owns the timer.
        int modeChangeBlockMs() const;
        void setModeChangeBlockMs(int value);

        //! Whether the embedding view currently wants a floating avatar shown at all.
        bool isWanted() const;

    public slots:

        //! The embedding view decided whether a floating avatar should be shown at all (avatars
        //! enabled and there is a bottom-most visible message with an avatar column). Fades in or
        //! out accordingly. Nothing else toggles this widget's visibility -- in particular a row's
        //! own anchored avatar coming back into view does not, see the class doc comment.
        void setWanted(bool enable);

        /**
         * @brief Cancel any pending timers/animation, hide immediately and unbind the represented
         * message.
         */
        void hideNow();

    signals:

        //! The avatar was clicked. ChatMessagesView forwards this to the represented message's own
        //! AbstractChatMessage::avatarClicked(), so an embedder's existing per-message handling
        //! applies unchanged.
        void clicked();

    protected:

        bool event(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        void updatePosition();
        void fadeIn();
        void fadeOut();

        std::unique_ptr<ChatFloatingAvatar_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATFLOATINGAVATAR_HPP
