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

/** @file uise/desktop/src/replypreview.cpp
*
*  Defines ReplyPreview.
*
*/

/****************************************************************************/

#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/pixmapscale.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/replypreview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const QSize IconSlotSize{32,32};
const QSize QuoteIconSize{14,14};

// AccentBarWidth/AccentBarSpacing/QuoteIconSpacing mirror QSS geometry (replypreview.qss's
// #accentBar/#quoteIcon rules) that contentWidthHint()/setContentMaxWidth() need to reason about
// BEFORE the widget is ever polished (e.g. during the very first bubble-width negotiation pass)
// -- these must be kept in sync with that QSS by hand, same as IconSlotSize above mirrors
// ChatMessageFileItem's own #iconSlot sizing convention. IconSlotSpacing is different: it is the
// fixed width this file itself gives #iconSlotGap in C++ (see the ctor), not QSS-driven at all,
// so nothing to keep in sync there.
constexpr int AccentBarWidth=3;
constexpr int AccentBarSpacing=10;
constexpr int IconSlotSpacing=6;
constexpr int QuoteIconSpacing=4;

// Mirrors uise--AbstractReplyPreview's min-width in replypreview.qss -- folded into
// contentWidthHint() too, so bubble-width negotiation never settles on a width narrower than
// what this block's own QSS floor enforces visually anyway.
constexpr int MinContentWidth=260;

}

//--------------------------------------------------------------------------

class ReplyPreview_p
{
    public:

        QBoxLayout* layout;

        QFrame* accentBar;
        //! Shown ONLY for a reply to an image message -- see ReplyPreview::isIconSlotVisible().
        QFrame* iconSlot;
        RoundedImage* thumbnail;
        //! Fixed-width empty gap after iconSlot, shown/hidden together with it -- see
        //! ReplyPreview::updateIconSlot()'s own comment on why this is a real widget rather than
        //! a QSS margin on iconSlot.
        QFrame* iconSlotGap;

        QFrame* textColumn;
        QBoxLayout* textColumnLayout;
        QFrame* titleRow;
        ElidedLabel* title;
        RoundedImage* quoteIcon;
        ElidedLabel* text;

        ReplyPreviewData data;

        QString titleFormat;
        QString deletedText;
        QString dateTimeFormat;

        bool pressed=false;
};

//--------------------------------------------------------------------------

ReplyPreview::ReplyPreview(QWidget* parent)
    : AbstractReplyPreview(parent),
      pimpl(std::make_unique<ReplyPreview_p>())
{
    pimpl->titleFormat=tr("Reply to %1, %2");
    pimpl->deletedText=tr("Deleted message");
    pimpl->dateTimeFormat=QStringLiteral("dd.MM.yyyy hh:mm");

    pimpl->layout=Layout::horizontal(this);

    pimpl->accentBar=new QFrame(this);
    pimpl->accentBar->setObjectName("accentBar");
    pimpl->layout->addWidget(pimpl->accentBar);

    pimpl->iconSlot=new QFrame(this);
    pimpl->iconSlot->setObjectName("iconSlot");
    pimpl->iconSlot->setFixedSize(IconSlotSize);
    pimpl->layout->addWidget(pimpl->iconSlot);

    pimpl->thumbnail=new RoundedImage(pimpl->iconSlot);
    pimpl->thumbnail->setObjectName("thumbnail");
    pimpl->thumbnail->setAutoSize(false);
    pimpl->thumbnail->setCornersRadius(6,6);
    pimpl->thumbnail->setImageSize(IconSlotSize);
    pimpl->thumbnail->setGeometry(QRect(QPoint(0,0),IconSlotSize));

    // Reserves IconSlotSpacing as a real layout item, not a QSS margin on iconSlot -- iconSlot
    // is a setFixedSize() widget, and a QBoxLayout clamps a fixed-size item's occupied geometry
    // to its literal minimumSize()/maximumSize(), which are pinned equal by setFixedSize() and
    // so never grow to include a style-sheet margin on top (unlike a widget sized purely from
    // its own sizeHint(), e.g. ReplyBar's configureButton/cancelButton, where margin-right/
    // margin-left do work -- see replypreview.qss's own comment on those). This exact pattern --
    // a dedicated fixed-size gap widget instead of margin on the fixed-size slot -- mirrors
    // ChatMessageFileItem's #iconSlot/#textColumn convention in chatmessagefiles.qss (margin-
    // left on #textColumn there, not margin-right on its own fixed-size #iconSlot).
    pimpl->iconSlotGap=new QFrame(this);
    pimpl->iconSlotGap->setObjectName("iconSlotGap");
    pimpl->iconSlotGap->setFixedWidth(IconSlotSpacing);
    pimpl->layout->addWidget(pimpl->iconSlotGap);

    pimpl->textColumn=new QFrame(this);
    pimpl->textColumn->setObjectName("textColumn");
    pimpl->textColumnLayout=Layout::vertical(pimpl->textColumn);
    pimpl->layout->addWidget(pimpl->textColumn,1);

    pimpl->titleRow=new QFrame(pimpl->textColumn);
    pimpl->titleRow->setObjectName("titleRow");
    auto titleRowLayout=Layout::horizontal(pimpl->titleRow);
    pimpl->textColumnLayout->addWidget(pimpl->titleRow);

    pimpl->title=new ElidedLabel(pimpl->titleRow);
    pimpl->title->setObjectName("title");
    pimpl->title->setElideMode(Qt::ElideRight);
    pimpl->title->setMaxLines(1);
    titleRowLayout->addWidget(pimpl->title,1);

    // Marks "this is a reply" ONLY on an already-sent/received bubble -- see
    // AbstractReplyPreview::quoteIconVisible's own doc comment for why it defaults off here and
    // is turned on per-context via QSS.
    pimpl->quoteIcon=new RoundedImage(pimpl->titleRow);
    pimpl->quoteIcon->setObjectName("quoteIcon");
    pimpl->quoteIcon->setAutoSize(false);
    pimpl->quoteIcon->setImageSize(QuoteIconSize);
    // Without this, RoundedImage::paintEvent() renders the svg into m_size and paints it via a
    // BRUSH TEXTURE FILL sized to the widget's actual on-screen rect() -- if that rect ever
    // differs from m_size by even a device pixel (QSS-polish timing, DPR rounding -- quoteIcon
    // sits in a layout-managed row, unlike the thumbnail slot above which is pinned by an
    // explicit setGeometry()), the texture tiles instead of stretching, showing a sliver of a
    // second copy at one edge. Setting an explicit svg icon size switches to a single centered
    // drawPixmap() instead (see RoundedImage::setSvgIconSize()'s own doc comment), immune to
    // any such size mismatch.
    pimpl->quoteIcon->setSvgIconSize(QuoteIconSize);
    pimpl->quoteIcon->setSvgIcon(Style::instance().svgIconLocator().icon(QStringLiteral("ReplyPreview::quote"),this));
    titleRowLayout->addWidget(pimpl->quoteIcon);

    pimpl->text=new ElidedLabel(pimpl->textColumn);
    pimpl->text->setObjectName("text");
    pimpl->text->setElideMode(Qt::ElideRight);
    pimpl->text->setMaxLines(1);
    pimpl->textColumnLayout->addWidget(pimpl->text);

    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    refresh();
}

//--------------------------------------------------------------------------

ReplyPreview::~ReplyPreview()
{}

//--------------------------------------------------------------------------

void ReplyPreview::setData(ReplyPreviewData data)
{
    pimpl->data=std::move(data);
    refresh();
}

//--------------------------------------------------------------------------

const ReplyPreviewData& ReplyPreview::data() const
{
    return pimpl->data;
}

//--------------------------------------------------------------------------

void ReplyPreview::clear()
{
    setData(ReplyPreviewData{});
}

//--------------------------------------------------------------------------

void ReplyPreview::setTitleFormat(const QString& format)
{
    pimpl->titleFormat=format;
    refresh();
}

//--------------------------------------------------------------------------

QString ReplyPreview::titleFormat() const
{
    return pimpl->titleFormat;
}

//--------------------------------------------------------------------------

void ReplyPreview::setDeletedText(const QString& text)
{
    pimpl->deletedText=text;
    refresh();
}

//--------------------------------------------------------------------------

QString ReplyPreview::deletedText() const
{
    return pimpl->deletedText;
}

//--------------------------------------------------------------------------

void ReplyPreview::setDateTimeFormat(const QString& format)
{
    pimpl->dateTimeFormat=format;
    refresh();
}

//--------------------------------------------------------------------------

QString ReplyPreview::dateTimeFormat() const
{
    return pimpl->dateTimeFormat;
}

//--------------------------------------------------------------------------

int ReplyPreview::contentWidthHint(int forMaxWidth) const
{
    if (maxWidthHint()<=0)
    {
        return 0;
    }

    auto titleRowWidth=pimpl->title->widthHint();
    if (isQuoteIconShown())
    {
        titleRowWidth+=QuoteIconSize.width()+QuoteIconSpacing;
    }
    auto textWidth=qMax(titleRowWidth,pimpl->text->widthHint());

    auto natural=horizontalTotalMargin(this)+AccentBarWidth+AccentBarSpacing+textWidth;
    if (isIconSlotVisible())
    {
        natural+=IconSlotSize.width()+IconSlotSpacing;
    }
    natural=qMax(natural,MinContentWidth);

    return qMin(qMin(natural,maxWidthHint()),forMaxWidth);
}

//--------------------------------------------------------------------------

void ReplyPreview::setContentMaxWidth(int width)
{
    // Only the text column actually needs capping -- accentBar/iconSlot are fixed-size, and the
    // quote icon sits inside the text column's own title row, already covered by this cap. The
    // title/text ElidedLabels re-elide themselves from their own resizeEvent() once the column
    // is relaid out at this width, nothing further is needed here.
    auto reserved=AccentBarWidth+AccentBarSpacing+horizontalTotalMargin(this);
    if (isIconSlotVisible())
    {
        reserved+=IconSlotSize.width()+IconSlotSpacing;
    }
    pimpl->textColumn->setMaximumWidth(qMax(0,width-reserved));
    updateGeometry();
}

//--------------------------------------------------------------------------

void ReplyPreview::updateTextTrimLength()
{
    refresh();
}

//--------------------------------------------------------------------------

void ReplyPreview::updateQuoteTrimLength()
{
    refresh();
}

//--------------------------------------------------------------------------

void ReplyPreview::updateAccentBarVisible()
{
    pimpl->accentBar->setVisible(isAccentBarVisible());
}

//--------------------------------------------------------------------------

void ReplyPreview::updateQuoteIconVisible()
{
    refresh();
}

//--------------------------------------------------------------------------

void ReplyPreview::mousePressEvent(QMouseEvent* event)
{
    // Matches IconTextButton::mousePressEvent()/mouseReleaseEvent(): a press only marks the
    // block down, letting a press dragged out before release cancel the click.
    if (event->button()==Qt::LeftButton)
    {
        pimpl->pressed=true;
    }
    AbstractReplyPreview::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ReplyPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && pimpl->pressed)
    {
        pimpl->pressed=false;
        if (rect().contains(event->pos()))
        {
            emit clicked();
        }
    }
    AbstractReplyPreview::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

void ReplyPreview::refresh()
{
    const auto& d=pimpl->data;
    const bool deleted=d.isDeleted() || d.kind()==ReplyMessageKind::Deleted;
    const bool showTitle=isTitleShown();

    if (showTitle)
    {
        // Only substitute the markers the format actually carries: a caller that does not want
        // the original's datetime shown (a chat bubble's own reply block, vs the editor's reply
        // bar) sets a title format with %1 alone, and QString::arg() warns on any missing marker.
        QString title=pimpl->titleFormat;
        if (title.contains(QLatin1String("%2")))
        {
            title=title.arg(d.senderTitle(),d.dateTime().toString(pimpl->dateTimeFormat));
        }
        else if (title.contains(QLatin1String("%1")))
        {
            title=title.arg(d.senderTitle());
        }
        pimpl->title->setText(title);
    }
    // Hide the whole row, not just the label -- otherwise the (still-visible) quote icon would
    // be left floating next to an empty title.
    pimpl->titleRow->setVisible(showTitle);
    pimpl->quoteIcon->setVisible(isQuoteIconShown());

    // A plain reply's text() is the (potentially long) original message, capped at
    // textTrimLength() characters; a quote (AbstractReplyDialog's "Quote selected") is already
    // the user's own deliberately hand-picked fragment, capped at the separate
    // quoteTrimLength() instead -- see AbstractReplyPreview::setData()'s own doc comment for
    // why these are two distinct limits.
    auto charLimit=d.isQuote() ? quoteTrimLength() : textTrimLength();
    pimpl->text->setText(deleted ? pimpl->deletedText : trimReplyText(d.text(),charLimit));

    // Turning deleted mid-lifetime (a bubble's placeholder resolving to "not found") would
    // otherwise shrink this block by a whole row: buildReplySection() deliberately reserves
    // titleRow's height from construction (its own blank-placeholder seed, see that function's
    // comment) so the bubble never has to renegotiate width/height once the real title arrives
    // -- but a DELETED result hides titleRow just like an empty one, throwing that same
    // reservation away and shrinking the block a moment later, right back into the flicker the
    // placeholder was meant to prevent. Once deleted, pin textColumn to the same total height
    // titleRow+text would occupy together (both sizeHints are effectively constants -- fixed
    // fonts, both labels capped at one line) and center the sole remaining line (deletedText)
    // in the middle of that reserved space, rather than leaving it pinned to the top with a
    // now-invisible gap where titleRow used to be.
    if (deleted)
    {
        auto naturalHeight=pimpl->titleRow->sizeHint().height()+pimpl->text->sizeHint().height();
        pimpl->textColumn->setMinimumHeight(naturalHeight);
        pimpl->textColumnLayout->setAlignment(pimpl->text,Qt::AlignVCenter);
    }
    else
    {
        pimpl->textColumn->setMinimumHeight(0);
        pimpl->textColumnLayout->setAlignment(pimpl->text,Qt::Alignment());
    }

    updateIconSlot();

    Style::setStyleProperty(this,"deleted",deleted);
    Style::setStyleProperty(this,"quote",d.isQuote() && !deleted);

    updateGeometry();
}

//--------------------------------------------------------------------------

void ReplyPreview::updateIconSlot()
{
    if (!isIconSlotVisible())
    {
        pimpl->iconSlot->setVisible(false);
        pimpl->iconSlotGap->setVisible(false);
        return;
    }

    pimpl->iconSlot->setVisible(true);
    pimpl->iconSlotGap->setVisible(true);

    // Scale to PHYSICAL pixels and tag with devicePixelRatio -- same rule as
    // ChatMessageFileItem::updateIconSlot(), required for a sharp brush-texture paint on
    // HiDPI/Retina displays, see scaledAndCropped()'s own doc comment.
    const qreal dpr=devicePixelRatioF();
    QSize physicalSize(qRound(IconSlotSize.width()*dpr),qRound(IconSlotSize.height()*dpr));
    auto px=scaledAndCropped(QPixmap::fromImage(pimpl->data.thumbnail()),physicalSize);
    px.setDevicePixelRatio(dpr);
    pimpl->thumbnail->setPixmap(px);
}

//--------------------------------------------------------------------------

bool ReplyPreview::isIconSlotVisible() const
{
    const auto& d=pimpl->data;
    const bool deleted=d.isDeleted() || d.kind()==ReplyMessageKind::Deleted;
    return !deleted && d.kind()==ReplyMessageKind::Image && !d.thumbnail().isNull();
}

//--------------------------------------------------------------------------

bool ReplyPreview::isTitleShown() const
{
    const auto& d=pimpl->data;
    const bool deleted=d.isDeleted() || d.kind()==ReplyMessageKind::Deleted;
    // Deleted originals show ONLY the deletedText() line -- a "Reply to X, date" header for
    // content that no longer exists reads as misleading, not merely redundant.
    return !deleted && (!d.senderTitle().isEmpty() || d.dateTime().isValid());
}

//--------------------------------------------------------------------------

bool ReplyPreview::isQuoteIconShown() const
{
    return isTitleShown() && isQuoteIconVisible();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
