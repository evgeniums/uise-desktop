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

/** @file uise/desktop/skeleton.hpp
*
*  Declares Skeleton – a shimmer placeholder loading widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SKELETON_HPP
#define UISE_DESKTOP_SKELETON_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractloadingwidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Skeleton_p;

/**
 * @brief Shimmer placeholder loading widget.
 *
 * Draws configurable greyed-out placeholder shapes (avatar circles, text-line
 * bars, fixed blocks) with a diagonal highlight band that sweeps across them
 * on a continuous loop while running.  Colors are driven by QSS via a hidden
 * style-sample child widget (same pattern as Spinner).
 *
 * Usage:
 * @code
 *   auto s = new Skeleton(parent);
 *   s->addCircle(48);
 *   s->newRow();
 *   s->addLine(1.0);
 *   s->addLine(0.7);
 *   s->start();
 * @endcode
 *
 * When no items are added the widget renders a built-in default template
 * (avatar circle + three text-line rows) so it looks good immediately.
 */
class UISE_DESKTOP_EXPORT Skeleton : public AbstractPanelLoadingWidget
{
    Q_OBJECT

    public:

        explicit Skeleton(QWidget* parent=nullptr);
        ~Skeleton() override;

        // ---- content builders ----

        /**
         * @brief Add a rounded text-line bar to the current row.
         * @param widthFactor Fraction of available row width (0.0 – 1.0).
         * @param height      Bar height in pixels; -1 uses the default (12 px).
         */
        void addLine(double widthFactor=1.0, int height=-1);

        /**
         * @brief Add a fixed-size rounded rectangle to the current row.
         */
        void addBlock(int width, int height);

        /**
         * @brief Add a circle (e.g. avatar placeholder) to the current row.
         */
        void addCircle(int diameter);

        /**
         * @brief Begin a new row of items.
         *
         * Items added after this call are placed on the next row.
         */
        void newRow();

        /**
         * @brief Remove all items (reverts to default template on next paint).
         */
        void clearItems();

        // ---- appearance knobs ----

        /** @brief Horizontal/vertical gap between items and rows (default 8 px). */
        void setItemSpacing(int px);
        int  itemSpacing() const noexcept;

        /** @brief Corner radius for line/block placeholders (default 6 px). */
        void setCornerRadius(int px);
        int  cornerRadius() const noexcept;

        /** @brief Duration of one shimmer sweep cycle in milliseconds (default 1200). */
        void setShimmerDurationMs(int ms);
        int  shimmerDurationMs() const noexcept;

        // ---- AbstractLoadingWidget interface ----

        bool isRunning() const noexcept override;

        virtual bool isCenterAligned() const override
        {
            return false;
        }

    public slots:

        void start() override;
        void stop()  override;

    protected:

        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:

        void buildDefaultTemplate();
        void recomputeRects();

        std::unique_ptr<Skeleton_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SKELETON_HPP
