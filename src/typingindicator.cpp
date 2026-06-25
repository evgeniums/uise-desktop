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

/** @file uise/desktop/src/typingindicator.cpp
*
*  Defines TypingIndicator – an animated triple-dots + text label widget.
*
*/

/****************************************************************************/

#include <cmath>
#include <algorithm>

#include <QPainter>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QVariantAnimation>

#include <uise/desktop/typingindicator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//==========================================================================
// DotsWidget – forward declaration (defined after TypingIndicator_p)
//==========================================================================

class DotsWidget;

//==========================================================================
// Private data
//==========================================================================

class TypingIndicator_p
{
    public:

        TypingIndicator_p()
            : label(nullptr),
              dots(nullptr),
              layout(nullptr),
              anim(nullptr),
              phase(0.0),
              dotColor(0x7f, 0xb2, 0xe8),
              activeDotColor(0x2f, 0x7f, 0xd1),
              dotRadius(2),
              dotSpacing(3),
              dotCount(3),
              activeScale(1.6),
              animationDurationMs(1000),
              spacing(1),
              dotsPosition(TypingIndicator::DotsPosition::Left)
        {}

        QLabel*            label;
        DotsWidget*        dots;
        QHBoxLayout*       layout;
        QVariantAnimation* anim;
        double             phase;

        QColor dotColor;
        QColor activeDotColor;
        int    dotRadius;
        int    dotSpacing;
        int    dotCount;
        double activeScale;
        int    animationDurationMs;
        int    spacing;
        TypingIndicator::DotsPosition dotsPosition;
};

//==========================================================================
// DotsWidget – paints the animated dot row, no Q_OBJECT needed
//==========================================================================

static constexpr int DotMargin = 2;

class DotsWidget : public QWidget
{
    public:

        explicit DotsWidget(TypingIndicator_p* d, QWidget* parent)
            : QWidget(parent), d(d)
        {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            setAttribute(Qt::WA_TranslucentBackground);
        }

        QSize sizeHint() const override
        {
            const double maxR  = d->dotRadius * d->activeScale;
            const int    stride = 2 * d->dotRadius + d->dotSpacing;
            const int    w = static_cast<int>(std::ceil(2.0 * maxR))
                             + (d->dotCount - 1) * stride
                             + 2 * DotMargin;
            const int    h = static_cast<int>(std::ceil(2.0 * maxR)) + 2 * DotMargin;
            return {w, h};
        }

        QSize minimumSizeHint() const override { return sizeHint(); }

    protected:

        void paintEvent(QPaintEvent*) override
        {
            const double maxR   = d->dotRadius * d->activeScale;
            const int    stride = 2 * d->dotRadius + d->dotSpacing;
            const double firstCX = maxR + DotMargin;
            const int    centerY = height() / 2;

            // Animation phase 0..1 maps to a bump position travelling from
            // dot 0 (pos=0) through to just past the last dot (pos=dotCount).
            // The brief "pause" between cycles (no dot lit) is intentional.
            const double pos = d->phase * d->dotCount;

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);

            for (int i = 0; i < d->dotCount; ++i)
            {
                const double cx   = firstCX + i * stride;
                const double dist = std::abs(pos - static_cast<double>(i));
                // Triangular bump clipped to [0,1]; squared for smoother roll-off.
                const double bump = std::max(0.0, 1.0 - dist);
                const double t    = bump * bump;

                const double r = d->dotRadius + (maxR - d->dotRadius) * t;

                // Blend base colour toward active colour proportionally to t.
                const QColor& c0 = d->dotColor;
                const QColor& c1 = d->activeDotColor;
                const QColor dotC(
                    static_cast<int>(c0.red()   + (c1.red()   - c0.red())   * t),
                    static_cast<int>(c0.green() + (c1.green() - c0.green()) * t),
                    static_cast<int>(c0.blue()  + (c1.blue()  - c0.blue())  * t)
                );

                painter.setBrush(dotC);
                painter.drawEllipse(QPointF(cx, centerY), r, r);
            }
        }

    private:

        TypingIndicator_p* d;
};

//==========================================================================
// Constructor / destructor
//==========================================================================

TypingIndicator::TypingIndicator(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<TypingIndicator_p>())
{
    setObjectName("typingIndicator");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    pimpl->label = new QLabel(this);
    pimpl->label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    pimpl->dots = new DotsWidget(pimpl.get(), this);

    pimpl->layout = new QHBoxLayout(this);
    pimpl->layout->setContentsMargins(4, 4, 4, 4);
    rebuildLayout();

    pimpl->anim = new QVariantAnimation(this);
    pimpl->anim->setStartValue(0.0);
    pimpl->anim->setEndValue(1.0);
    pimpl->anim->setLoopCount(-1);
    pimpl->anim->setDuration(pimpl->animationDurationMs);
    connect(pimpl->anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v)
            {
                pimpl->phase = v.toDouble();
                pimpl->dots->update();
            });
}

TypingIndicator::~TypingIndicator() = default;

//==========================================================================
// Layout helpers
//==========================================================================

void TypingIndicator::rebuildLayout()
{
    // Remove all items from the layout without deleting the widgets.
    QLayoutItem* item = nullptr;
    while ((item = pimpl->layout->takeAt(0)) != nullptr)
    {
        delete item;
    }

    pimpl->layout->setSpacing(pimpl->spacing);

    if (pimpl->dotsPosition == DotsPosition::Left)
    {
        pimpl->layout->addWidget(pimpl->dots,  0, Qt::AlignBottom);
        pimpl->layout->addWidget(pimpl->label, 0, Qt::AlignVCenter);
    }
    else
    {
        pimpl->layout->addWidget(pimpl->label, 0, Qt::AlignVCenter);
        pimpl->layout->addWidget(pimpl->dots,  0, Qt::AlignBottom);
    }
    pimpl->layout->addStretch();
}

//==========================================================================
// Text
//==========================================================================

void TypingIndicator::setText(const QString& text)
{
    pimpl->label->setText(text);
}

QString TypingIndicator::text() const
{
    return pimpl->label->text();
}

//==========================================================================
// Dots position
//==========================================================================

void TypingIndicator::setDotsPosition(DotsPosition pos)
{
    if (pimpl->dotsPosition == pos) { return; }
    pimpl->dotsPosition = pos;
    rebuildLayout();
}

TypingIndicator::DotsPosition TypingIndicator::dotsPosition() const noexcept
{
    return pimpl->dotsPosition;
}

//==========================================================================
// Animation control
//==========================================================================

bool TypingIndicator::isRunning() const noexcept
{
    return pimpl->anim->state() == QAbstractAnimation::Running;
}

void TypingIndicator::start()
{
    if (!isRunning())
    {
        pimpl->anim->start();
    }
}

void TypingIndicator::stop()
{
    pimpl->anim->stop();
    pimpl->phase = 0.0;
    pimpl->dots->update();
}

//==========================================================================
// Colour knobs
//==========================================================================

void TypingIndicator::setDotColor(const QColor& color)
{
    pimpl->dotColor = color;
    pimpl->dots->update();
}

QColor TypingIndicator::dotColor() const noexcept { return pimpl->dotColor; }

void TypingIndicator::setActiveDotColor(const QColor& color)
{
    pimpl->activeDotColor = color;
    pimpl->dots->update();
}

QColor TypingIndicator::activeDotColor() const noexcept { return pimpl->activeDotColor; }

//==========================================================================
// Geometry knobs
//==========================================================================

void TypingIndicator::setDotRadius(int px)
{
    pimpl->dotRadius = std::max(1, px);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
}

int TypingIndicator::dotRadius() const noexcept { return pimpl->dotRadius; }

void TypingIndicator::setDotSpacing(int px)
{
    pimpl->dotSpacing = std::max(0, px);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
}

int TypingIndicator::dotSpacing() const noexcept { return pimpl->dotSpacing; }

void TypingIndicator::setDotCount(int n)
{
    pimpl->dotCount = std::max(1, n);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
}

int TypingIndicator::dotCount() const noexcept { return pimpl->dotCount; }

void TypingIndicator::setActiveScale(double factor)
{
    pimpl->activeScale = std::max(1.0, factor);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
}

double TypingIndicator::activeScale() const noexcept { return pimpl->activeScale; }

void TypingIndicator::setAnimationDurationMs(int ms)
{
    const bool wasRunning = isRunning();
    if (wasRunning) { pimpl->anim->stop(); }
    pimpl->animationDurationMs = std::max(100, ms);
    pimpl->anim->setDuration(pimpl->animationDurationMs);
    if (wasRunning) { pimpl->anim->start(); }
}

int TypingIndicator::animationDurationMs() const noexcept { return pimpl->animationDurationMs; }

void TypingIndicator::setSpacing(int px)
{
    pimpl->spacing = std::max(0, px);
    pimpl->layout->setSpacing(pimpl->spacing);
}

int TypingIndicator::spacing() const noexcept { return pimpl->spacing; }

UISE_DESKTOP_NAMESPACE_END
