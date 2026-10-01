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
#include <numeric>
#include <vector>

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QVariantAnimation>

#include <uise/desktop/typingindicator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//==========================================================================
// DotsWidget – forward declaration (defined after TypingIndicator_p)
//==========================================================================

class DotsWidget;

//==========================================================================
// Label that can be squeezed
//==========================================================================

namespace {

// A QLabel never gets narrower than its text, which would keep the indicator from shrinking
// with the room it is given. The indicator elides the text itself to the room that is left,
// so the label may be squeezed down to nothing; its size hint is still the width of the text.
class TypingLabel : public QLabel
{
    public:

        using QLabel::QLabel;

        QSize minimumSizeHint() const override
        {
            return {0, QLabel::minimumSizeHint().height()};
        }
};

//! Replaces %1..%N of @a pattern by @a names in a single pass. A placeholder that has no name
//! is left as it is.
QString substituteNames(const QString& pattern, const QStringList& names)
{
    QString result;
    result.reserve(pattern.size());

    const auto size=pattern.size();
    for (qsizetype i=0;i<size;++i)
    {
        const QChar ch=pattern.at(i);
        if (ch==QLatin1Char('%') && i+1<size && pattern.at(i+1).isDigit())
        {
            qsizetype end=i+1;
            int index=0;
            while (end<size && end<i+3 && pattern.at(end).isDigit())
            {
                index=index*10+pattern.at(end).digitValue();
                ++end;
            }
            if (index>=1 && index<=names.size())
            {
                result+=names.at(index-1);
                i=end-1;
                continue;
            }
        }
        result+=ch;
    }
    return result;
}

} // namespace

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

        int         maxNameWidth=0;
        QString     pattern;
        QStringList names;
        //! the text with every name in full
        QString     fullText;
        //! the text with every name capped at maxNameWidth: what the widget would like to be wide for
        QString     wantedText;
        bool        updatingText=false;
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
            const auto   stride = 2 * d->dotRadius + d->dotSpacing;
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
            const auto   stride = 2 * d->dotRadius + d->dotSpacing;
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
                painter.drawEllipse(QPointF(cx, centerY - r), r, r);
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

    pimpl->label = new TypingLabel(this);
    pimpl->label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // names of people are in the text: whatever they contain is not markup
    pimpl->label->setTextFormat(Qt::PlainText);

    pimpl->dots = new DotsWidget(pimpl.get(), this);

    pimpl->layout = new QHBoxLayout(this);
    pimpl->layout->setContentsMargins(4, 4, 4, 4);
    rebuildLayout();

    // only now: the handler needs the dots and the layout
    pimpl->label->installEventFilter(this);

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
    setElidedText(text, {});
}

QString TypingIndicator::text() const
{
    return pimpl->fullText;
}

void TypingIndicator::setElidedText(const QString& pattern, const QStringList& names)
{
    if (pimpl->pattern == pattern && pimpl->names == names)
    {
        return;
    }
    pimpl->pattern = pattern;
    pimpl->names = names;
    updateText();
}

//==========================================================================
// Eliding
//==========================================================================

int TypingIndicator::availableTextWidth() const
{
    if (width() <= 0)
    {
        return -1;
    }

    const auto own = contentsMargins();
    const auto lay = pimpl->layout->contentsMargins();

    // what the label adds around its text (margins, and the rounding of its own measuring)
    const int labelOverhead = std::max(0,
        pimpl->label->sizeHint().width() - pimpl->label->fontMetrics().horizontalAdvance(pimpl->label->text()));

    const int chrome = own.left() + own.right()
                       + lay.left() + lay.right()
                       + labelOverhead
                       + pimpl->dots->sizeHint().width()
                       + std::max(0, pimpl->layout->spacing());
    return std::max(0, width() - chrome);
}

void TypingIndicator::updateText()
{
    // setText() of the label asks for a new geometry, and that must not come back here
    if (pimpl->updatingText)
    {
        return;
    }
    pimpl->updatingText = true;

    const QFontMetrics fm = pimpl->label->fontMetrics();
    const auto& names     = pimpl->names;
    const int   count     = static_cast<int>(names.size());

    // what every name would take: in full, and capped
    std::vector<int> natural(count);
    std::vector<int> cap(count);
    QStringList cappedNames = names;
    for (int i = 0; i < count; ++i)
    {
        natural[i] = fm.horizontalAdvance(names.at(i));
        cap[i]     = pimpl->maxNameWidth > 0 ? std::min(natural[i], pimpl->maxNameWidth) : natural[i];
        if (cap[i] < natural[i])
        {
            cappedNames[i] = fm.elidedText(names.at(i), Qt::ElideRight, cap[i]);
        }
    }

    const QString fullText   = substituteNames(pimpl->pattern, names);
    const QString wantedText = substituteNames(pimpl->pattern, cappedNames);

    QString shown = wantedText;
    const int avail = availableTextWidth();
    if (avail >= 0 && fm.horizontalAdvance(wantedText) > avail)
    {
        QStringList elidedNames = cappedNames;
        if (count > 0)
        {
            // the room left after the words of the pattern is shared between the names: the
            // shortest ones first, so that what they do not need goes to the longer ones
            const int fixed  = fm.horizontalAdvance(substituteNames(pimpl->pattern, QStringList(count, QString())));
            int remaining    = std::max(0, avail - fixed);

            std::vector<int> order(count);
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&cap](int a, int b){return cap[a] < cap[b];});

            for (int k = 0; k < count; ++k)
            {
                const int i     = order[k];
                const int share = remaining / (count - k);
                const int alloc = std::min(cap[i], share);
                remaining -= alloc;
                if (alloc < cap[i])
                {
                    elidedNames[i] = fm.elidedText(names.at(i), Qt::ElideRight, alloc);
                }
            }
        }

        shown = substituteNames(pimpl->pattern, elidedNames);
        // nothing but the words of the pattern may be left too wide for the room: elide the whole
        if (fm.horizontalAdvance(shown) > avail)
        {
            shown = fm.elidedText(shown, Qt::ElideRight, avail);
        }
    }

    const bool hintChanged = pimpl->wantedText != wantedText;
    pimpl->fullText   = fullText;
    pimpl->wantedText = wantedText;

    if (pimpl->label->text() != shown)
    {
        pimpl->label->setText(shown);
    }
    // the full text is worth a look while something is cut off
    setToolTip(shown != fullText ? fullText : QString());

    if (hintChanged)
    {
        updateGeometry();
    }

    pimpl->updatingText = false;
}

QSize TypingIndicator::sizeHint() const
{
    // The label shows what fits, which is not what the widget wants: it asks for the width of the
    // whole text, so that a layout gives it as much room as there is. The elided text is part of
    // the layout's own hint, so only the difference is added -- this does not change with eliding.
    auto size = QFrame::sizeHint();
    const QFontMetrics fm = pimpl->label->fontMetrics();
    const int extra = fm.horizontalAdvance(pimpl->wantedText) - fm.horizontalAdvance(pimpl->label->text());
    if (extra > 0)
    {
        size.setWidth(size.width() + extra);
    }
    return size;
}

void TypingIndicator::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    updateText();
}

bool TypingIndicator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == pimpl->label)
    {
        // the font of the label comes from the style sheet, which applies after the text is set
        switch (event->type())
        {
            case QEvent::FontChange:
            case QEvent::StyleChange:
            case QEvent::Polish:
                updateText();
                break;
            default:
                break;
        }
    }
    return QFrame::eventFilter(watched, event);
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

void TypingIndicator::setDotRadius(double px)
{
    pimpl->dotRadius = std::max(1.0, px);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
    updateText(); // the room left for the text changes with the dots
}

double TypingIndicator::dotRadius() const noexcept { return pimpl->dotRadius; }

void TypingIndicator::setDotSpacing(int px)
{
    pimpl->dotSpacing = std::max(0, px);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
    updateText(); // the room left for the text changes with the dots
}

int TypingIndicator::dotSpacing() const noexcept { return pimpl->dotSpacing; }

void TypingIndicator::setDotCount(int n)
{
    pimpl->dotCount = std::max(1, n);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
    updateText(); // the room left for the text changes with the dots
}

int TypingIndicator::dotCount() const noexcept { return pimpl->dotCount; }

void TypingIndicator::setActiveScale(double factor)
{
    pimpl->activeScale = std::max(1.0, factor);
    pimpl->dots->updateGeometry();
    pimpl->dots->update();
    updateText(); // the room left for the text changes with the dots
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
    updateText();
}

int TypingIndicator::spacing() const noexcept { return pimpl->spacing; }

void TypingIndicator::setMaxNameWidth(int px)
{
    px = std::max(0, px);
    if (pimpl->maxNameWidth == px)
    {
        return;
    }
    pimpl->maxNameWidth = px;
    updateText();
}

int TypingIndicator::maxNameWidth() const noexcept { return pimpl->maxNameWidth; }

UISE_DESKTOP_NAMESPACE_END
