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

/** @file uise/desktop/iconbadge.hpp
*
*  Declares IconBadge.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ICONBADGE_HPP
#define UISE_DESKTOP_ICONBADGE_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/countbadge.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class IconBadge_p;

/**
 * @brief CountBadge that floats over the top-right corner of an "anchor" widget (typically an
 * icon or avatar) without taking part in any layout.
 *
 * Because the badge is a free-floating child of a host widget, showing, hiding or changing its
 * text never changes the size of the anchor or of anything laid out around it -- the point of
 * using it instead of decorating a title with "(N)".
 *
 * The badge tracks the anchor's position inside the host by watching the anchor, every ancestor
 * between the anchor and the host, and the host itself (Move/Resize/Show/Hide/ParentChange), and
 * is visible only while its text is non-empty and the anchor is visible relative to the host.
 * It is transparent for mouse events, so clicks and hovers reach whatever is underneath.
 *
 * Placement is computed against a "virtual icon" square of anchorSize() centred on the anchor,
 * not against the anchor's own rect: a small anchor (e.g. a 12px status dot) can thereby carry a
 * badge positioned as if the anchor were a larger icon. The badge's right edge is pinned at the
 * virtual square's right edge plus offsetX(), so a longer count grows to the left, never towards
 * whatever follows the icon. The result is clamped into the host's rect.
 *
 * anchorSize, offsetX and offsetY are QSS-tunable, e.g.
 * @code
 *   uise--IconBadge { qproperty-anchorSize: 22; qproperty-offsetX: 2; qproperty-offsetY: 3; }
 * @endcode
 * Colours, font and padding come from CountBadge's own QSS rules.
 */
class UISE_DESKTOP_EXPORT IconBadge : public CountBadge
{
    Q_OBJECT

    Q_PROPERTY(int anchorSize READ anchorSize WRITE setAnchorSize)
    Q_PROPERTY(int offsetX READ offsetX WRITE setOffsetX)
    Q_PROPERTY(int offsetY READ offsetY WRITE setOffsetY)

    public:

        constexpr static const int DefaultAnchorSize=22;
        constexpr static const int DefaultOffsetX=2;
        constexpr static const int DefaultOffsetY=3;

        /**
         * @param anchor Widget whose top-right corner the badge decorates.
         * @param host Widget the badge is a child of, i.e. the coordinate space it floats in.
         * Must be \p anchor itself or one of its ancestors to be meaningful, otherwise the badge
         * stays hidden.
         */
        IconBadge(QWidget* anchor, QWidget* host);
        ~IconBadge() override;

        IconBadge(const IconBadge&)=delete;
        IconBadge& operator=(const IconBadge&)=delete;

        //! Re-parent the badge to another host, e.g. when the widget owning the anchor is
        //! adopted by a container that must carry the badge instead.
        void setHost(QWidget* host);
        QWidget* host() const;

        void setAnchor(QWidget* anchor);
        QWidget* anchor() const;

        void setAnchorSize(int val);
        int anchorSize() const noexcept;

        void setOffsetX(int val);
        int offsetX() const noexcept;

        void setOffsetY(int val);
        int offsetY() const noexcept;

        //! Recompute geometry and visibility now. Called automatically on every relevant event.
        void reposition();

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;
        void contentChanged() override;

    private:

        void rewatch();

        std::unique_ptr<IconBadge_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ICONBADGE_HPP
