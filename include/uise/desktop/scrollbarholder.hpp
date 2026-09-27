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

/** @file uise/desktop/scrollbarholder.hpp
*
*  Declares a scrollbar holder that can keep its place in a layout when its scrollbar is
*  invisible, and can auto-hide/fade the scrollbar handle when the mouse is not over a given
*  hover target.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SCROLLBARHOLDER_HPP
#define UISE_DESKTOP_SCROLLBARHOLDER_HPP

#include <memory>

#include <QScrollBar>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ScrollBarHolder_p;

/**
 * @brief Frame holding a QScrollBar (vertical or horizontal), with two independent behaviours:
 *
 *  - "Hold place": when the scrollbar is not needed (setVisible(false), i.e. content fits), the
 *    holder can either collapse (default) or keep occupying its layout cell (setHoldPlace(true)),
 *    e.g. so surrounding content does not shift width/height as items are loaded.
 *
 *  - "Auto-hide": when enabled, the scrollbar handle is only opaque while the mouse is over a
 *    configured hover target (see setHoverTarget()) or shortly after notifyUserScrolled() is
 *    called; otherwise it fades to fully transparent. The holder itself (and thus the space it
 *    reserves when the bar is needed) is unaffected -- only the inner QScrollBar's opacity
 *    changes, via a QGraphicsOpacityEffect kept installed for as long as auto-hide is enabled.
 *    Unlike the per-item QGraphicsOpacityEffect that caused stale/offset composites inside a
 *    FlyweightListView (see checkbox-opacity-effect-scroll-eclipse-fix.md), a permanently
 *    installed effect is safe here: that defect came from the effect's ancestor being the
 *    scrolled item container itself (relocated via QWidget::move() on every scroll), whereas this
 *    holder is a stationary sibling of the scrolled content, only ever resized -- the same
 *    scroll-adjacent-overlay pattern ChatDateSubtitle already uses safely with a permanent effect.
 */
class UISE_DESKTOP_EXPORT ScrollBarHolder : public QFrame
{
    Q_OBJECT

    public:

        constexpr static int DefaultFadeInMs=150;
        constexpr static int DefaultFadeOutMs=400;
        constexpr static int DefaultHideDelayMs=1000;

        explicit ScrollBarHolder(Qt::Orientation orientation, QWidget* parent=nullptr);

        ~ScrollBarHolder();

        ScrollBarHolder(const ScrollBarHolder&)=delete;
        ScrollBarHolder(ScrollBarHolder&&)=delete;
        ScrollBarHolder& operator=(const ScrollBarHolder&)=delete;
        ScrollBarHolder& operator=(ScrollBarHolder&&)=delete;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        QScrollBar* bar() const;

        //! Whether the scrollbar is needed (content overflows). Distinct from the holder's own
        //! QWidget::isVisible() when isHoldPlace() is set.
        void setVisible(bool enable) override;
        bool isVisible() const;

        //! Keep occupying the layout cell even when the scrollbar is not needed.
        void setHoldPlace(bool enable);
        bool isHoldPlace() const;

        //! Enable/disable auto-hide/fade of the scrollbar handle. Disabled by default (a plain
        //! ScrollBarHolder behaves exactly as VerticalScrollBar always did); FlyweightListView
        //! turns it on.
        void setAutoHide(bool enable);
        bool isAutoHide() const;

        //! Widget whose Enter/Leave/Hide events drive the auto-hide fade -- typically the
        //! FlyweightListView itself, so hovering anywhere over the list (not just the bar) keeps
        //! the handle visible.
        void setHoverTarget(QWidget* target);
        QWidget* hoverTarget() const;

        //! Call when the user actively scrolled (wheel, keys, drag, jump) so the handle flashes
        //! visible even while the mouse is elsewhere, then fades out again after the hide delay.
        //! No-op when auto-hide is disabled or the bar is not needed.
        void notifyUserScrolled();

        void setFadeInDurationMs(int value);
        int fadeInDurationMs() const;

        void setFadeOutDurationMs(int value);
        int fadeOutDurationMs() const;

        void setHideDelayMs(int value);
        int hideDelayMs() const;

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        std::unique_ptr<ScrollBarHolder_p> pimpl;
};

}

#endif // UISE_DESKTOP_SCROLLBARHOLDER_HPP
