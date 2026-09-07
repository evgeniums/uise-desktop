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

/** @file uise/desktop/countbadge.hpp
*
*  Declares CountBadge.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_COUNTBADGE_HPP
#define UISE_DESKTOP_COUNTBADGE_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class CountBadge_p;

/**
 * @brief Small counter badge that paints itself as a circle for short text, growing into a
 * rounded pill as the text lengthens -- e.g. for an unread-message count.
 *
 * Unlike a QLabel styled with QSS `border-radius`, whose size (and therefore apparent
 * roundness) is driven by the bounding box of whatever text it happens to hold, CountBadge's
 * height is derived from the font's metrics alone and never changes with the text -- only the
 * width grows once the text no longer fits inside a circle of that height. This keeps the
 * badge's silhouette constant across "1", "42", "999", "1.2K", etc.
 *
 * The badge paints nothing (and reports a zero sizeHint) when its text is empty, so it can be
 * added straight into a layout without a separate visibility toggle for the "no count" case.
 *
 * Colours, font and padding are picked up from an internal QSS style-carrier (a hidden child
 * QLabel with objectName "badge"), following the same pattern used by JumpEdge's own inline
 * badge painting -- see `uise--CountBadge > QLabel#badge` in the bundled style sheets. A "mute"
 * dynamic property on that carrier lets QSS switch to a muted colour, toggled via setMuted().
 *
 * A QSS `margin` set directly on CountBadge itself (as opposed to `padding` on the carrier
 * above) is folded into sizeHint(), and the badge shape is painted inset from it -- unlike a
 * QBoxLayout, a manual/custom layout that positions children purely from sizeHint() (e.g.
 * ElidedContainer) has no other way to learn that space was requested around the badge.
 *
 * Usage:
 * @code
 *   auto badge = new CountBadge(parent);
 *   badge->setCount(7);
 *   layout->addWidget(badge);
 * @endcode
 */
class UISE_DESKTOP_EXPORT CountBadge : public QFrame
{
    Q_OBJECT

    public:

        explicit CountBadge(QWidget* parent=nullptr);
        ~CountBadge() override;

        CountBadge(const CountBadge&)=delete;
        CountBadge& operator=(const CountBadge&)=delete;

        /** @brief Set pre-formatted badge text, e.g. "42" or "1.2K". Empty text paints nothing. */
        void setText(const QString& text);
        QString text() const;

        /** @brief Equivalent to setText({}). */
        void clear();

        /** @brief Convenience for a plain integer count; callers that abbreviate large counts
         * (e.g. "1.2K", "99K", "12M") should format the string themselves and use setText(). */
        void setCount(size_t count);

        /** @brief Switch between the normal accent colour and a muted colour, both defined in
         * QSS via `uise--CountBadge > QLabel#badge` and its `[mute="true"]` variant. */
        void setMuted(bool enable);
        bool isMuted() const noexcept;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:

        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:

        void updateGeometryAndRepaint();

        std::unique_ptr<CountBadge_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_COUNTBADGE_HPP
