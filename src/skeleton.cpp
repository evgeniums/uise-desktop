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

/** @file uise/desktop/src/skeleton.cpp
*
*  Defines Skeleton – a shimmer placeholder loading widget.
*
*/

/****************************************************************************/

#include <vector>
#include <algorithm>

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QVariantAnimation>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QLineEdit>

#include <uise/desktop/skeleton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Item definitions
// ---------------------------------------------------------------------------

enum class SkeletonItemType { Line, Block, Circle };

struct SkeletonItem
{
    SkeletonItemType type        = SkeletonItemType::Line;
    double           widthFactor = 1.0; // Line: fraction of row width (0..1)
    int              fixedWidth  = 0;   // Block / Circle
    int              fixedHeight = 0;   // Block / Circle / Line (0 → DefaultLineHeight)

    QRect rect; // resolved in recomputeRects()
};

// ---------------------------------------------------------------------------
// Private implementation
// ---------------------------------------------------------------------------

class Skeleton_p
{
    public:

        Skeleton_p()
            : styleSample(nullptr),
              itemSpacing(12),
              cornerRadius(6),
              shimmerDurationMs(1200),
              anim(nullptr),
              shimmerProgress(0.0),
              rectsDirty(true)
        {}

        QWidget* styleSample;

        std::vector<std::vector<SkeletonItem>> rows;

        int    itemSpacing;
        int    cornerRadius;
        int    shimmerDurationMs;

        QVariantAnimation* anim;
        double shimmerProgress; // 0..1 – position of the sweep band

        bool rectsDirty; // true whenever rows or size changed since last recomputeRects
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int DefaultLineHeight = 24;
static constexpr int DefaultAvatarSize = 36;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Skeleton::Skeleton(QWidget* parent)
    : AbstractLoadingWidget(parent),
      pimpl(std::make_unique<Skeleton_p>())
{
    setObjectName("skeleton");
    // A minimum size is required so that the popup layout (addWidget with
    // Qt::AlignCenter, no stretch) allocates real space rather than 0×0.
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Hidden style-sample child: QSS writes palette colors into it;
    // paintEvent reads them back. QLineEdit is used (same as Spinner) because
    // Qt only maps `background-color` → QPalette::Base for input widgets.
    pimpl->styleSample = new QLineEdit(this);
    pimpl->styleSample->setObjectName("styleSample");
    pimpl->styleSample->setProperty("style-sample", true);
    pimpl->styleSample->setVisible(false);

    pimpl->anim = new QVariantAnimation(this);
    pimpl->anim->setStartValue(0.0);
    pimpl->anim->setEndValue(1.0);
    pimpl->anim->setLoopCount(-1);
    pimpl->anim->setDuration(pimpl->shimmerDurationMs);
    connect(pimpl->anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v)
            {
                pimpl->shimmerProgress = v.toDouble();
                update();
            });
}

Skeleton::~Skeleton() = default;

// ---------------------------------------------------------------------------
// Content builders
// ---------------------------------------------------------------------------

void Skeleton::addLine(double widthFactor, int height)
{
    if (pimpl->rows.empty())
    {
        pimpl->rows.emplace_back();
    }
    SkeletonItem item;
    item.type        = SkeletonItemType::Line;
    item.widthFactor = std::clamp(widthFactor, 0.0, 1.0);
    item.fixedHeight = (height > 0) ? height : 0;
    pimpl->rows.back().push_back(item);
    pimpl->rectsDirty = true;
    update();
}

void Skeleton::addBlock(int w, int h)
{
    if (pimpl->rows.empty())
    {
        pimpl->rows.emplace_back();
    }
    SkeletonItem item;
    item.type        = SkeletonItemType::Block;
    item.fixedWidth  = w;
    item.fixedHeight = h;
    pimpl->rows.back().push_back(item);
    pimpl->rectsDirty = true;
    update();
}

void Skeleton::addCircle(int diameter)
{
    if (pimpl->rows.empty())
    {
        pimpl->rows.emplace_back();
    }
    SkeletonItem item;
    item.type        = SkeletonItemType::Circle;
    item.fixedWidth  = diameter;
    item.fixedHeight = diameter;
    pimpl->rows.back().push_back(item);
    pimpl->rectsDirty = true;
    update();
}

void Skeleton::newRow()
{
    pimpl->rows.emplace_back();
}

void Skeleton::clearItems()
{
    pimpl->rows.clear();
    pimpl->rectsDirty = true;
    update();
}

// ---------------------------------------------------------------------------
// Knobs
// ---------------------------------------------------------------------------

void Skeleton::setItemSpacing(int px)
{
    pimpl->itemSpacing = px;
    pimpl->rectsDirty  = true;
    update();
}

int Skeleton::itemSpacing() const noexcept { return pimpl->itemSpacing; }

void Skeleton::setCornerRadius(int px)
{
    pimpl->cornerRadius = px;
    update();
}

int Skeleton::cornerRadius() const noexcept { return pimpl->cornerRadius; }

void Skeleton::setShimmerDurationMs(int ms)
{
    pimpl->shimmerDurationMs = ms;
    bool wasRunning = isRunning();
    if (wasRunning) { pimpl->anim->stop(); }
    pimpl->anim->setDuration(ms);
    if (wasRunning) { pimpl->anim->start(); }
}

int Skeleton::shimmerDurationMs() const noexcept { return pimpl->shimmerDurationMs; }

// ---------------------------------------------------------------------------
// AbstractLoadingWidget interface
// ---------------------------------------------------------------------------

bool Skeleton::isRunning() const noexcept
{
    return pimpl->anim->state() == QAbstractAnimation::Running;
}

void Skeleton::start()
{
    if (!isRunning())
    {
        pimpl->anim->start();
    }
}

void Skeleton::stop()
{
    pimpl->anim->stop();
    update();
}

// ---------------------------------------------------------------------------
// Default template (built lazily on first paint when rows are empty)
// ---------------------------------------------------------------------------

void Skeleton::buildDefaultTemplate()
{
    pimpl->rows.clear();

    // row 0: avatar circle
    pimpl->rows.emplace_back();
    SkeletonItem avatar;
    avatar.type        = SkeletonItemType::Circle;
    avatar.fixedWidth  = DefaultAvatarSize;
    avatar.fixedHeight = DefaultAvatarSize;
    pimpl->rows.back().push_back(avatar);

    // rows 1-3: text-line bars
    for (double factor : {1.0, 0.85, 0.60})
    {
        pimpl->rows.emplace_back();
        SkeletonItem line;
        line.type        = SkeletonItemType::Line;
        line.widthFactor = factor;
        pimpl->rows.back().push_back(line);
    }

    pimpl->rectsDirty = true;
}

// ---------------------------------------------------------------------------
// Layout computation
// ---------------------------------------------------------------------------

void Skeleton::recomputeRects()
{
    if (pimpl->rows.empty())
    {
        return;
    }

    const int spacing = pimpl->itemSpacing;
    const int margin  = spacing;
    const int availW  = std::max(1, width()  - 2 * margin);

    int y = margin;
    for (auto& row : pimpl->rows)
    {
        if (row.empty()) { y += spacing; continue; }

        // row height = tallest item
        int rowH = 0;
        for (const auto& item : row)
        {
            int h = (item.fixedHeight > 0) ? item.fixedHeight : DefaultLineHeight;
            rowH  = std::max(rowH, h);
        }

        int x = margin;
        for (auto& item : row)
        {
            int h = (item.fixedHeight > 0) ? item.fixedHeight : DefaultLineHeight;
            int w = 0;
            if (item.type == SkeletonItemType::Line)
            {
                w = static_cast<int>(availW * item.widthFactor);
            }
            else
            {
                w = item.fixedWidth;
            }
            int iy  = y + (rowH - h) / 2;
            item.rect = QRect(x, iy, w, h);
            x += w + spacing;
        }
        y += rowH + spacing;
    }

    pimpl->rectsDirty = false;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void Skeleton::resizeEvent(QResizeEvent* event)
{
    AbstractLoadingWidget::resizeEvent(event);
    pimpl->rectsDirty = true;
}

void Skeleton::paintEvent(QPaintEvent* /*event*/)
{
    // Build the default template lazily on the first paint (when the widget
    // finally has a real size). Subsequent paints skip this branch.
    if (pimpl->rows.empty())
    {
        buildDefaultTemplate(); // sets rectsDirty=true
    }

    // Recompute rects whenever the size changed or items were modified.
    if (pimpl->rectsDirty)
    {
        recomputeRects();
    }

    // Read theme colors from the hidden style-sample child.
    const QColor baseColor = pimpl->styleSample->palette().color(QPalette::Base);
    QColor shimmerColor    = pimpl->styleSample->palette().color(QPalette::Highlight);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const int radius = pimpl->cornerRadius;

    // Shimmer gradient: a soft band sweeping left-to-right across the whole
    // widget, clipped to each item's shape so it's continuous across all rows.
    const int    totalW = width();
    const int    totalH = height();
    const double bandW  = totalW * 0.4;
    const double startX = -bandW + (totalW + bandW) * pimpl->shimmerProgress;
    const double endX   = startX + bandW;

    QLinearGradient shimmer(startX, 0, endX, totalH * 0.3);
    QColor c = shimmerColor;
    c.setAlphaF(0.0);  shimmer.setColorAt(0.0,  c);
    c.setAlphaF(0.85); shimmer.setColorAt(0.45, c);
    c.setAlphaF(0.95); shimmer.setColorAt(0.5,  c);
    c.setAlphaF(0.85); shimmer.setColorAt(0.55, c);
    c.setAlphaF(0.0);  shimmer.setColorAt(1.0,  c);

    for (const auto& row : pimpl->rows)
    {
        for (const auto& item : row)
        {
            if (item.rect.isEmpty()) { continue; }

            QPainterPath path;
            if (item.type == SkeletonItemType::Circle)
            {
                path.addEllipse(item.rect);
            }
            else
            {
                path.addRoundedRect(item.rect, radius, radius);
            }

            painter.save();
            painter.setClipPath(path);
            painter.fillPath(path, baseColor);
            if (isRunning())
            {
                painter.fillPath(path, shimmer);
            }
            painter.restore();
        }
    }
}

UISE_DESKTOP_NAMESPACE_END
