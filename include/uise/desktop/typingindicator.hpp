/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/typingindicator.hpp
*
*  Declares TypingIndicator – an animated triple-dots + text label widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TYPINGINDICATOR_HPP
#define UISE_DESKTOP_TYPINGINDICATOR_HPP

#include <memory>

#include <QColor>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class TypingIndicator_p;

/**
 * @brief Animated triple-dots + text label widget.
 *
 * Draws three dots (circles) next to a text label. A highlight bump travels
 * continuously from left to right, momentarily enlarging the radius of each
 * dot as it passes – producing a smooth "typing…" animation.
 *
 * Dots can be placed to the left or right of the text label via
 * setDotsPosition(). All visual parameters (dot radius, spacing, colours,
 * animation speed) are configurable via QSS @c qproperty-* declarations or
 * the corresponding setters.
 *
 * Default QSS (light and dark variants) uses a light-blue colour palette and
 * is bundled into the library's Qt resource file; it is applied automatically
 * when the uise-desktop Style singleton is active.
 *
 * Usage:
 * @code
 *   auto ti = new TypingIndicator(parent);
 *   ti->setText("User is typing");
 *   ti->setDotsPosition(TypingIndicator::DotsPosition::Left);
 *   ti->start();
 * @endcode
 */
class UISE_DESKTOP_EXPORT TypingIndicator : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(QColor dotColor           READ dotColor           WRITE setDotColor)
    Q_PROPERTY(QColor activeDotColor     READ activeDotColor     WRITE setActiveDotColor)
    Q_PROPERTY(int    dotRadius          READ dotRadius          WRITE setDotRadius)
    Q_PROPERTY(int    dotSpacing         READ dotSpacing         WRITE setDotSpacing)
    Q_PROPERTY(int    dotCount           READ dotCount           WRITE setDotCount)
    Q_PROPERTY(double activeScale        READ activeScale        WRITE setActiveScale)
    Q_PROPERTY(int    animationDurationMs READ animationDurationMs WRITE setAnimationDurationMs)
    Q_PROPERTY(int    spacing            READ spacing            WRITE setSpacing)

    public:

        /** @brief Position of the animated dots relative to the text label. */
        enum class DotsPosition { Left, Right };
        Q_ENUM(DotsPosition)

        explicit TypingIndicator(QWidget* parent = nullptr);
        ~TypingIndicator() override;

        TypingIndicator(const TypingIndicator&) = delete;
        TypingIndicator& operator=(const TypingIndicator&) = delete;

        // ---- text ----

        void    setText(const QString& text);
        QString text() const;

        // ---- dots position ----

        void         setDotsPosition(DotsPosition pos);
        DotsPosition dotsPosition() const noexcept;

        // ---- animation control ----

        bool isRunning() const noexcept;

        // ---- colour knobs ----

        /** @brief Base colour of the dots when not highlighted. */
        void   setDotColor(const QColor& color);
        QColor dotColor() const noexcept;

        /** @brief Colour of a dot at the peak of its highlight bump. */
        void   setActiveDotColor(const QColor& color);
        QColor activeDotColor() const noexcept;

        // ---- geometry knobs ----

        /** @brief Base radius of each dot in pixels (default 4). */
        void setDotRadius(int px);
        int  dotRadius() const noexcept;

        /** @brief Gap between adjacent dot centres minus their base diameters (default 10 px). */
        void setDotSpacing(int px);
        int  dotSpacing() const noexcept;

        /** @brief Number of dots (default 3). */
        void setDotCount(int n);
        int  dotCount() const noexcept;

        /** @brief Peak scale factor applied to the highlighted dot's radius (default 1.8). */
        void   setActiveScale(double factor);
        double activeScale() const noexcept;

        /** @brief Duration of one full left-to-right animation cycle in ms (default 1000). */
        void setAnimationDurationMs(int ms);
        int  animationDurationMs() const noexcept;

        /** @brief Gap in pixels between the dots area and the text label (default 8). */
        void setSpacing(int px);
        int  spacing() const noexcept;

    public slots:

        void start();
        void stop();

    private:

        void rebuildLayout();

        std::unique_ptr<TypingIndicator_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_TYPINGINDICATOR_HPP
