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

/** @file uise/desktop/src/countbadge.cpp
*
*  Defines CountBadge.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QFontMetrics>

#include <uise/desktop/style.hpp>
#include <uise/desktop/countbadge.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** CountBadge_p *********************************/

class CountBadge_p
{
    public:

        CountBadge_p() : badge(nullptr)
        {}

        // Hidden QSS style-carrier -- colour, font and padding for the badge are read from
        // this widget's palette/font/contentsMargins at paint time, the same trick JumpEdge
        // uses for its own inline badge painting. It is never shown; only its style matters.
        QLabel* badge;
};

/**************************** CountBadge ************************************/

CountBadge::CountBadge(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<CountBadge_p>())
{
    setObjectName("countBadge");
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setAttribute(Qt::WA_TranslucentBackground);

    pimpl->badge=new QLabel(this);
    pimpl->badge->setObjectName("badge");
    pimpl->badge->setVisible(false);
}

CountBadge::~CountBadge()=default;

//--------------------------------------------------------------------------

void CountBadge::setText(const QString& text)
{
    if (pimpl->badge->text()==text)
    {
        return;
    }
    pimpl->badge->setText(text);
    updateGeometryAndRepaint();
}

QString CountBadge::text() const
{
    return pimpl->badge->text();
}

//--------------------------------------------------------------------------

void CountBadge::clear()
{
    setText(QString());
}

//--------------------------------------------------------------------------

void CountBadge::setCount(size_t count)
{
    setText(QString::number(count));
}

//--------------------------------------------------------------------------

void CountBadge::setMuted(bool enable)
{
    if (Style::setStyleProperty(pimpl->badge,"mute",enable))
    {
        updateGeometryAndRepaint();
    }
}

bool CountBadge::isMuted() const noexcept
{
    return pimpl->badge->property("mute").toBool();
}

//--------------------------------------------------------------------------

void CountBadge::updateGeometryAndRepaint()
{
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------

QSize CountBadge::sizeHint() const
{
    auto text=pimpl->badge->text();
    if (text.isEmpty())
    {
        return QSize(0,0);
    }

    QFontMetrics metrics(pimpl->badge->font());
    auto m=pimpl->badge->contentsMargins();

    // Height comes from the FONT only, never from the glyphs actually present -- this is what
    // keeps the badge's silhouette (a perfect circle up to its diameter, a constant-height
    // capsule beyond it) identical whether the text is "1" or "9", unlike JumpEdge's own badge
    // which derives its circle from the tight bounding rect of the current text and therefore
    // wobbles in both height and roundness as the text changes.
    //
    // minimumWidth()/minimumHeight() already resolve Qt's QSS min-width/min-height as border-box
    // sizes -- i.e. they already include contentsMargins() -- so they are compared directly
    // against other border-box candidates below, never with m.left()/m.right()/m.top()/m.bottom()
    // added on top a second time. Adding them again was exactly the bug that made small counts
    // like "1"/"9" paint as a wide oval instead of a circle: minimumWidth() (already 10px content
    // + 12px padding = 22px) plus another +12px padding came out to 34px against a ~20px height.
    auto h=std::max(metrics.height()+m.top()+m.bottom(),pimpl->badge->minimumHeight());

    auto fw=metrics.horizontalAdvance(text);
    auto w=std::max({h,fw+m.left()+m.right(),pimpl->badge->minimumWidth()});

    // A QSS "margin" on the CountBadge itself (as opposed to "padding" on the hidden #badge
    // carrier, handled above) lands in THIS frame's own contentsMargins() -- Qt folds margin and
    // padding into the same accessor, and since CountBadge never sets any padding on itself
    // (only on the carrier), this is purely margin. It must be added to the reported size, not
    // just used for painting: a container that positions children from sizeHint() alone (e.g.
    // ElidedContainer's manual layout, unlike a QBoxLayout) has no other way to learn that extra
    // space around the badge was requested.
    auto outer=contentsMargins();
    return QSize(w+outer.left()+outer.right(),h+outer.top()+outer.bottom());
}

QSize CountBadge::minimumSizeHint() const
{
    return sizeHint();
}

//--------------------------------------------------------------------------

void CountBadge::paintEvent(QPaintEvent* /*event*/)
{
    auto text=pimpl->badge->text();
    if (text.isEmpty())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

    // Inset by the outer margin (see sizeHint()) before drawing anything, so the badge shape
    // itself sits clear of the reserved margin strip rather than spanning the full widget rect.
    auto r=rect().marginsRemoved(contentsMargins());

    // Background: a rounded rect whose corner radius is half the height draws an exact circle
    // when width==height, and a constant-height, round-ended capsule once the width grows past
    // that -- the circle-then-pill shape, with no separate code path for either case.
    auto radius=r.height()/2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(pimpl->badge->palette().color(QPalette::Base));
    painter.drawRoundedRect(QRectF(r),radius,radius);

    // Text: centered by Qt against the font's own ascent/descent rather than the visible
    // glyphs' tight bounding box, so it never drifts vertically or horizontally as the text
    // changes -- unlike JumpEdge::renderBadgeText()'s manual bearing/baseline correction.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pimpl->badge->palette().color(QPalette::Text));
    painter.setFont(pimpl->badge->font());
    painter.drawText(r,Qt::AlignCenter,text);
}

//--------------------------------------------------------------------------

void CountBadge::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);

    switch (event->type())
    {
        case (QEvent::StyleChange):
        case (QEvent::FontChange):
        case (QEvent::PaletteChange):
        {
            updateGeometryAndRepaint();
        }
        break;

        default:
        break;
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
