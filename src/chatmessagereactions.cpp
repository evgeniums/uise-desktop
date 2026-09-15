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

/** @file uise/desktop/src/chatmessagereactions.cpp
*
*  Defines ChatMessageReactionChip, ChatMessageReactionsRow and ChatMessageReactions.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QLabel>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QResizeEvent>

#include <uise/desktop/chatmessagereactions.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/flowpack.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// Matches #reactionIcon/#moreIcon/#avatar's min/max-width/height in chatreactions.qss -- shared
// by the icon glyph and the small user avatars so they read as one consistent size (deliberately
// the SAME constant, not just two equal numbers, so they cannot drift apart later). RoundedImage
// does not pick this up from QSS on its own -- see the constructor's own comment on why every
// RoundedImage-based icon here needs setSvgIconSize()/setAvatarSize() explicitly, and
// updateAvatars()'s own comment on why AvatarWidget's raster path needs it just as much as the
// SVG path does.
const QSize ReactionChipIconSize(16,16);

//! reaction.icon() if set, else resolved through the process-wide pack registry -- see
//! ChatReaction::icon()'s own doc comment.
std::shared_ptr<SvgIcon> resolveIcon(const ChatReaction& reaction)
{
    if (reaction.icon())
    {
        return reaction.icon();
    }
    return ReactionIconPacks::instance().icon(reaction.id());
}

}

//--------------------------------------------------------------------------

ChatMessageReactionChip::ChatMessageReactionChip(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatMessageReactionChip");
    setCursor(Qt::PointingHandCursor);

    auto layout=Layout::horizontal(this);

    m_icon=new RoundedImage(this);
    m_icon->setObjectName("reactionIcon");
    // RoundedImage has no size of its own until told one -- without this, m_size stays the
    // default-constructed, invalid QSize(-1,-1) and SvgIcon::pixmap() (paintEvent()'s fallback
    // for a null pixmap()) is asked to rasterize at that invalid size, so nothing ever paints
    // even though setSvgIcon() below succeeds. setSvgIconSize() also switches paintEvent() to a
    // single centered drawPixmap() instead of a brush-texture fill sized to the widget's actual
    // (layout-managed) rect() -- see replypreview.cpp's quoteIcon for the identical pattern and
    // the tiling artifact setImageSize() alone would otherwise risk.
    m_icon->setAutoSize(false);
    m_icon->setImageSize(ReactionChipIconSize);
    m_icon->setSvgIconSize(ReactionChipIconSize);
    layout->addWidget(m_icon);

    m_count=new QLabel(this);
    m_count->setObjectName("count");
    layout->addWidget(m_count);

    m_avatarsFrame=new QFrame(this);
    m_avatarsFrame->setObjectName("avatars");
    // Deliberately NO QLayout here -- overlap needs a genuine negative gap between items, and
    // neither Qt mechanism for that turned out to give one: a QSS negative margin goes through
    // QStyleSheetStyle's box-model recomputation and distorted a fixed-size, custom-painted
    // RoundedImage/AvatarWidget into an ellipse; QLayout::setSpacing() with a negative value is
    // not "overlap by that many pixels" at all -- Qt treats negative spacing as a sentinel for
    // "use the style's default", which is why it came out as an ordinary (positive) gap instead.
    // updateAvatars() below positions each AvatarWidget with a manual setGeometry() instead,
    // matching how ChatMessageReactionsRow itself already avoids a QLayout for the same reason
    // (see utils/flowpack.hpp) -- the only way to get an exact, guaranteed overlap in Qt.
    layout->addWidget(m_avatarsFrame);

    m_moreIcon=new RoundedImage(this);
    m_moreIcon->setObjectName("moreIcon");
    m_moreIcon->setAutoSize(false);
    m_moreIcon->setImageSize(ReactionChipIconSize);
    m_moreIcon->setSvgIconSize(ReactionChipIconSize);
    m_moreIcon->setSvgIcon(Style::instance().svgIconLocator().icon(QStringLiteral("ChatReactions::more"),this));
    layout->addWidget(m_moreIcon);

    updateVisibility();
}

//--------------------------------------------------------------------------

ChatMessageReactionChip::~ChatMessageReactionChip()
{
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setReaction(const ChatReaction& reaction)
{
    m_reaction=reaction;

    m_icon->setSvgIcon(resolveIcon(m_reaction));
    m_count->setText(QString::number(m_reaction.count()));
    updateAvatars();

    Style::setStyleProperty(this,"own",m_reaction.isOwn());

    updateVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::updateAvatars()
{
    const auto& avatars=m_reaction.avatars();

    while (m_avatarWidgets.size()<avatars.size() && m_avatarWidgets.size()<3)
    {
        // Each ring sits directly behind its OWN avatar and, in the final Z-order this function
        // asserts explicitly below (see the raise() pass after the positioning loop), ABOVE the
        // NEXT avatar in the stack -- so it paints a same-colour "notch" into THAT neighbour
        // wherever they overlap, and its own avatar (right on top of it) covers the ring again
        // except for the exposed rim. That is what makes overlapping same-coloured avatars read
        // as a stack instead of one merged blob (see the task feedback this responds to). Pool
        // creation order here has no bearing on the FINAL stacking -- only which pair a ring and
        // an avatar belong to.
        auto* ring=new QFrame(m_avatarsFrame);
        ring->setObjectName("avatarRing");
        // Plain QFrame does paint its own QSS background/border-radius by default, but this
        // guarantees it regardless -- the standard incantation for a custom-painted-adjacent
        // widget that must not be mistaken for a transparent pass-through. background-color
        // itself still comes from the app-level per-theme chatreactions.qss (selector-matched by
        // objectName); only border-radius is set per-instance below, in the positioning loop --
        // see that loop's own comment on why.
        ring->setAttribute(Qt::WA_StyledBackground,true);
        m_avatarRings.push_back(ring);

        auto* avatar=new AvatarWidget(m_avatarsFrame);
        avatar->setObjectName("avatar");
        avatar->setClickable(false);
        // See the constructor's identical comment on m_icon/m_moreIcon -- without an explicit
        // size, RoundedImage's pixmap-consumer/SVG paint paths both silently produce nothing.
        avatar->setAutoSize(false);
        avatar->setAvatarSize(ReactionChipIconSize);
        // RoundedImage renders a PLAIN SQUARE by default -- xRadius()/yRadius() only become
        // width()/2 / height()/2 (a true circle for this square size) once autoFitEllipse is on
        // (see RoundedImage::xRadius()/yRadius() -- setCornersRadius() would be a fixed pixel
        // radius instead, wrong for a widget whose size is a qproperty rather than a constant).
        // Only takes effect for the name-only initials fallback used here (no avatar source set)
        // -- evalXRadius()/evalYRadius() on a REAL image source, once a host supplies one, take
        // priority and are that source's own responsibility to shape.
        avatar->setAutoFitEllipse(true);
        m_avatarWidgets.push_back(avatar);
    }

    // Overlap by manual geometry -- see the constructor's own comment on why neither a QSS
    // margin nor QLayout::setSpacing() can express a genuine negative gap in Qt. Every avatar is
    // offset by m_avatarRingBorderWidth on both axes so its own ring -- which is larger than the
    // avatar on every side -- never needs a NEGATIVE coordinate: ring[0]'s top-left is exactly
    // (0,0), not (-border,-border), which would otherwise be clipped by m_avatarsFrame's own
    // bounds (Qt clips child painting to the parent's rect).
    const auto step=ReactionChipIconSize.width()-ReactionChipIconSize.width()/3;
    const auto ringSize=ReactionChipIconSize.width()+2*m_avatarRingBorderWidth;
    // A QSS border-radius rule for #avatarRing would need to be kept in sync BY HAND with
    // m_avatarRingBorderWidth (ringSize/2 changes whenever it does) -- computed here instead and
    // applied per-instance below, so the ring is always an exact circle regardless of the
    // property's current value. setStyleSheet() on the widget itself merges with (and, for this
    // one property, overrides) the app-level per-theme rule that still supplies background-color.
    const auto ringRadiusStyle=QStringLiteral("border-radius:%1px;").arg(ringSize/2);
    auto visibleCount=std::min({avatars.size(),m_avatarWidgets.size(),size_t{3}});

    for (size_t i=0; i<m_avatarWidgets.size(); ++i)
    {
        auto* widget=m_avatarWidgets[i];
        auto* ring=m_avatarRings[i];
        if (i<visibleCount)
        {
            const auto& a=avatars[i];
            // Source and path are NOT alternatives: the source is the fetcher, the path is what
            // it is asked to fetch (and what seeds the generated fallback background color), so
            // both are applied. Applied unconditionally, including when null/empty, because these
            // widgets are reused across setReactions() calls -- skipping an unset field would
            // leave the previous reactor's avatar showing under the new one's name.
            widget->setAvatarSource(a.source);
            widget->setAvatarPath(a.path);
            widget->setAvatarName(a.name.toStdString());

            auto ringX=static_cast<int>(i)*step;
            ring->setGeometry(ringX,0,ringSize,ringSize);
            ring->setStyleSheet(ringRadiusStyle);
            ring->setVisible(true);

            // Centred within its own ring, i.e. inset by m_avatarRingBorderWidth on every side.
            widget->setGeometry(ringX+m_avatarRingBorderWidth,m_avatarRingBorderWidth,
                                 ReactionChipIconSize.width(),ReactionChipIconSize.height());
            widget->setVisible(true);
        }
        else
        {
            widget->setVisible(false);
            ring->setVisible(false);
        }
    }

    // Explicit Z-order: the LEFTMOST (first) avatar on top, each avatar's own ring exposed as a
    // rim against the NEXT (further-right) avatar -- see this function's own doc comment above.
    // raise() moves a widget to the top of its parent's stack every time it is called, so
    // raising (ring,avatar) pairs from the RIGHTMOST towards the leftmost builds the whole
    // desired stack bottom-to-top: the last pair raised (index 0) ends up truly on top. Cheap
    // and safe to redo unconditionally on every call, including when nothing actually changed.
    for (size_t i=0; i<visibleCount; ++i)
    {
        auto k=visibleCount-1-i;
        m_avatarRings[k]->raise();
        m_avatarWidgets[k]->raise();
    }

    // m_avatarsFrame has no QLayout of its own (see the constructor's comment), so nothing else
    // reports its packed width to the chip's own outer layout -- set it explicitly to match
    // exactly what was just laid out above (the LAST ring's own right/bottom edge, which is where
    // the whole cluster actually ends now that rings extend past their avatars). setFixedSize()
    // itself notifies that outer layout of the change (standard QWidget behaviour), no extra
    // updateGeometry() call needed here.
    auto totalWidth=(visibleCount>0)
        ? static_cast<int>(visibleCount-1)*step+ringSize
        : 0;
    m_avatarsFrame->setFixedSize(totalWidth,ringSize);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setDisplayMode(ChatReactionDisplayMode mode)
{
    if (m_mode==mode)
    {
        return;
    }
    m_mode=mode;

    Style::setStyleProperty(this,"mode",m_mode==ChatReactionDisplayMode::Avatars
                                         ? QStringLiteral("avatars")
                                         : QStringLiteral("count"));

    updateVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setEllipsis(bool enable)
{
    if (m_ellipsis==enable)
    {
        return;
    }
    m_ellipsis=enable;

    Style::setStyleProperty(this,"ellipsis",m_ellipsis);

    updateVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::updateVisibility()
{
    if (m_ellipsis)
    {
        m_icon->setVisible(false);
        m_count->setVisible(false);
        m_avatarsFrame->setVisible(false);
        m_moreIcon->setVisible(true);
        return;
    }

    m_icon->setVisible(true);
    m_moreIcon->setVisible(false);

    bool avatars=(m_mode==ChatReactionDisplayMode::Avatars);
    m_count->setVisible(!avatars);
    m_avatarsFrame->setVisible(avatars);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setInteractive(bool enable)
{
    m_interactive=enable;
    // A press in selection mode must reach the message bubble underneath instead of being
    // consumed (and ignored) here -- see AbstractChatMessage::mousePressEvent().
    setAttribute(Qt::WA_TransparentForMouseEvents,!enable);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setAvatarRingBorderWidth(int value)
{
    if (m_avatarRingBorderWidth==value)
    {
        return;
    }
    m_avatarRingBorderWidth=value;
    // Re-lays out and re-styles every EXISTING ring/avatar pair at the new size -- see
    // updateAvatars()'s own comment on why the ring's border-radius is computed and applied here
    // rather than through a QSS rule that would need to track this property by hand.
    updateAvatars();
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::setHovered(bool enable)
{
    Style::setStyleProperty(this,"hovered",enable);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::enterEvent(QEnterEvent* event)
{
    setHovered(true);
    Frame::enterEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::leaveEvent(QEvent* event)
{
    setHovered(false);
    Frame::leaveEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::mousePressEvent(QMouseEvent* event)
{
    // Matches IconTextButton's own press-then-release-inside pattern (icontextbutton.cpp) -- a
    // press alone never fires clicked(), so a press dragged out of the chip before release
    // cancels it.
    if (event->button()==Qt::LeftButton)
    {
        m_pressed=true;
        event->accept();
        return;
    }
    Frame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageReactionChip::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && m_pressed)
    {
        m_pressed=false;
        if (rect().contains(event->pos()))
        {
            Q_EMIT clicked();
        }
        event->accept();
        return;
    }
    Frame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

ChatMessageReactionsRow::ChatMessageReactionsRow(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatMessageReactionsRow");
}

//--------------------------------------------------------------------------

ChatMessageReactionsRow::~ChatMessageReactionsRow()
{
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setReactions(ChatReactions reactions)
{
    m_reactions=std::move(reactions);
    ++m_reactionsRevision;

    if (m_displayModePolicy==DisplayModePolicy::ForceAvatars)
    {
        m_displayMode=ChatReactionDisplayMode::Avatars;
    }
    else if (m_displayModePolicy==DisplayModePolicy::ForceCounts)
    {
        m_displayMode=ChatReactionDisplayMode::Counts;
    }
    else
    {
        bool fitsAvatarForm=static_cast<int>(m_reactions.size())<=m_maxAvatarChipTypes
            && std::all_of(m_reactions.begin(),m_reactions.end(),[this](const ChatReaction& r)
                {
                    return static_cast<int>(r.count())<=m_maxAvatarsPerChip && !r.avatars().empty();
                });
        m_displayMode=fitsAvatarForm ? ChatReactionDisplayMode::Avatars : ChatReactionDisplayMode::Counts;
    }

    ensureChipCount(m_reactions.size());

    for (size_t i=0; i<m_reactions.size(); ++i)
    {
        auto* chip=m_chips[i];
        chip->setEllipsis(false);
        chip->setDisplayMode(m_displayMode);
        chip->setReaction(m_reactions[i]);
        chip->setVisible(true);
    }

    // Hide any chip left over from a previous, larger reaction set -- ensureChipCount() only
    // grows the pool, it never shrinks it (chips are cheap to keep around and reuse).
    for (size_t i=m_reactions.size(); i<m_chips.size(); ++i)
    {
        m_chips[i]->setVisible(false);
    }
    m_ellipsisShown=false;

    // Repack immediately, at the CURRENT width -- do not wait for a resizeEvent() that may never
    // come (this widget's on-screen size need not change just because its reaction DATA did).
    // m_packedForWidth is invalidated first so the repack()->packForWidth() call below cannot be
    // short-circuited by the memoization guard for a width that happens to match the last pack.
    // packForWidth() calls updateGeometry() itself once the repack is done.
    m_packedForWidth=-1;
    repack();
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::ensureChipCount(size_t count)
{
    // +1 spare slot reused as the ellipsis chip when the row needs to truncate -- see repack().
    auto needed=count+1;
    while (m_chips.size()<needed)
    {
        auto* chip=new ChatMessageReactionChip(this);
        chip->setInteractive(m_interactive);
        connect(chip,&ChatMessageReactionChip::clicked,this,
                [this,chip]()
                {
                    onChipClicked(chip);
                });
        m_chips.push_back(chip);
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::onChipClicked(ChatMessageReactionChip* chip)
{
    if (chip->isEllipsis())
    {
        Q_EMIT moreRequested();
        return;
    }

    auto reactionId=chip->reaction().id();
    bool currentlyOwn=chip->reaction().isOwn();

    applyOptimisticToggle(reactionId);
    Q_EMIT toggleRequested(reactionId,currentlyOwn);
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::applyOptimisticToggle(const QString& reactionId)
{
    // Flip the rendered copy immediately (optimistic UI) -- setReactions() from the host later
    // either confirms it (a matching push, no visible change) or overwrites it wholesale (a
    // contradicting push rolls this back automatically, since setReactions() replaces
    // m_reactions outright rather than merging into it).
    auto it=std::find_if(m_reactions.begin(),m_reactions.end(),[&reactionId](const ChatReaction& r)
        {
            return r.id()==reactionId;
        });

    if (it==m_reactions.end())
    {
        return;
    }

    if (it->isOwn())
    {
        it->setOwn(false);
        it->setCount(it->count()>0 ? it->count()-1 : 0);
        if (it->count()==0)
        {
            m_reactions.erase(it);
        }
    }
    else
    {
        it->setOwn(true);
        it->setCount(it->count()+1);
    }

    // Re-run the same setReactions() the host would push, on the optimistically mutated copy --
    // this keeps chip assignment, display-mode selection and repacking all in the single place
    // that already knows how to do it correctly.
    setReactions(m_reactions);
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setDisplayModePolicy(DisplayModePolicy policy)
{
    if (m_displayModePolicy==policy)
    {
        return;
    }
    m_displayModePolicy=policy;
    if (!m_reactions.empty())
    {
        setReactions(m_reactions);
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setInteractive(bool enable)
{
    m_interactive=enable;
    for (auto* chip : m_chips)
    {
        chip->setInteractive(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setMaxVisibleChips(int value)
{
    if (m_maxVisibleChips==value) return;
    m_maxVisibleChips=value;
    // Repack immediately at the current width -- see setReactions()'s identical reasoning: a
    // property change here does not guarantee updateGeometry() will actually trigger a
    // resizeEvent() (this widget's on-screen SIZE need not change), so resizeEvent()->repack()
    // cannot be relied on alone.
    // packForWidth() (via repack()) calls updateGeometry() itself once the repack is done.
    m_packedForWidth=-1;
    repack();
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setMaxAvatarChipTypes(int value)
{
    if (m_maxAvatarChipTypes==value) return;
    m_maxAvatarChipTypes=value;
    if (!m_reactions.empty())
    {
        setReactions(m_reactions);
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setMaxAvatarsPerChip(int value)
{
    if (m_maxAvatarsPerChip==value) return;
    m_maxAvatarsPerChip=value;
    if (!m_reactions.empty())
    {
        setReactions(m_reactions);
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setChipSpacing(int value)
{
    if (m_chipSpacing==value) return;
    m_chipSpacing=value;
    // Repack immediately at the current width -- see setReactions()'s identical reasoning: a
    // property change here does not guarantee updateGeometry() will actually trigger a
    // resizeEvent() (this widget's on-screen SIZE need not change), so resizeEvent()->repack()
    // cannot be relied on alone.
    // packForWidth() (via repack()) calls updateGeometry() itself once the repack is done.
    m_packedForWidth=-1;
    repack();
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setRowSpacing(int value)
{
    if (m_rowSpacing==value) return;
    m_rowSpacing=value;
    // Repack immediately at the current width -- see setReactions()'s identical reasoning: a
    // property change here does not guarantee updateGeometry() will actually trigger a
    // resizeEvent() (this widget's on-screen SIZE need not change), so resizeEvent()->repack()
    // cannot be relied on alone.
    // packForWidth() (via repack()) calls updateGeometry() itself once the repack is done.
    m_packedForWidth=-1;
    repack();
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::setTailGap(int value)
{
    if (m_tailGap==value) return;
    m_tailGap=value;
    // Repack immediately at the current width -- see setReactions()'s identical reasoning: a
    // property change here does not guarantee updateGeometry() will actually trigger a
    // resizeEvent() (this widget's on-screen SIZE need not change), so resizeEvent()->repack()
    // cannot be relied on alone.
    // packForWidth() (via repack()) calls updateGeometry() itself once the repack is done.
    m_packedForWidth=-1;
    repack();
}

//--------------------------------------------------------------------------

int ChatMessageReactionsRow::packForWidth(int maxWidth, int reservedTailWidth)
{
    auto dpr=devicePixelRatioF();

    // Memoized against everything the packing actually depends on -- a repeat call at the exact
    // same width/reservation/reactions/DPR is a no-op, mirroring ChatMessageImages' own
    // lastLayoutForMaxWidth/lastLayoutDpr pin (see chatmessagereactions.hpp's own comment).
    if (m_packedForWidth==maxWidth && m_packedReservedTailWidth==reservedTailWidth
        && m_packedReactionsRevision==m_reactionsRevision && m_packedDpr==dpr)
    {
        return m_packedSize.width();
    }

    m_packedForWidth=maxWidth;
    m_packedReservedTailWidth=reservedTailWidth;
    m_packedReactionsRevision=m_reactionsRevision;
    m_packedDpr=dpr;

    auto contentWidth=maxWidth-horizontalTotalMargin(this);

    // Decide up front how many chips are visible, so an "..." overflow chip past the limit
    // reuses the spare slot ensureChipCount() always keeps around instead of growing the pool.
    int visibleCount=static_cast<int>(m_reactions.size());
    bool overflow=m_maxVisibleChips>0 && visibleCount>m_maxVisibleChips;

    std::vector<QSize> sizes;
    sizes.reserve(m_reactions.size());
    for (size_t i=0; i<m_reactions.size(); ++i)
    {
        sizes.push_back(m_chips[i]->sizeHint());
    }

    FlowPackOptions options;
    options.maxWidth=contentWidth;
    options.hSpacing=m_chipSpacing;
    options.vSpacing=m_rowSpacing;
    // tailGap is EXTRA room on top of a genuine reservation, not a reservation of its own -- see
    // its own doc comment. ChatMessageReactions::bubbleWidthHint()/updateMaximumBubbleWidth()
    // always pass reservedTailWidth=0 (making room for the bubble's inline time/status row is
    // ChatMessageBottom's own job, via evaluateInlineBottom() measuring the NATURAL trailing
    // space left on the last packed row after the fact -- see lastTextLineRect()'s contract).
    // Adding tailGap unconditionally here reserved 10px for content that was never actually
    // going to occupy it, tipping the correction pass into peeling off the last chip on EVERY
    // pack once the row was wide enough to almost-but-not-quite need it.
    options.reservedTailWidth=(reservedTailWidth>0) ? (reservedTailWidth+m_tailGap) : 0;
    if (overflow)
    {
        options.maxItems=m_maxVisibleChips;
        // the ellipsis chip's own natural width, measured with a representative glyph already
        // set (see the constructor's setSvgIcon() call on m_moreIcon)
        m_chips[m_chips.size()-1]->setEllipsis(true);
        options.tailItemWidth=m_chips.back()->sizeHint().width();
    }

    auto result=flowPack(sizes,options);

    // overflow (item count > the limit) and result.truncated (flowPack()'s own, purely
    // count-based truncation decision) are always in lockstep here -- options.maxItems is set to
    // m_maxVisibleChips precisely when overflow is true, and to 0 (unlimited) otherwise. When
    // truncated, the LAST entry of result.rects is the tail (ellipsis) item's rect, NOT a real
    // reaction's -- see flowPack()'s own contract -- so only the entries before it correspond to
    // real chips.
    m_ellipsisShown=overflow;
    auto realPlaced=m_ellipsisShown ? (result.placedCount>0 ? result.placedCount-1 : 0) : result.placedCount;

    for (size_t i=0; i<realPlaced; ++i)
    {
        // setVisible(true) is needed here, not just setGeometry() -- a previous, narrower
        // packForWidth() call may have hidden this very chip as part of ITS OWN truncation (see
        // the hide loop below), and a later, wider repack must be able to bring it back.
        m_chips[i]->setVisible(true);
        m_chips[i]->setGeometry(result.rects[i].translated(contentsMargins().left(),contentsMargins().top()));
    }
    // Real reactions that lost the packing race to the "..." chip must not keep stale geometry
    // from a previous, less-truncated pack -- hide them explicitly.
    for (size_t i=realPlaced; i<m_reactions.size(); ++i)
    {
        m_chips[i]->setVisible(false);
    }

    if (m_ellipsisShown)
    {
        auto* ellipsisChip=m_chips.back();
        ellipsisChip->setGeometry(result.rects.back().translated(contentsMargins().left(),contentsMargins().top()));
        ellipsisChip->setVisible(true);
    }
    else if (!m_chips.empty())
    {
        // The spare slot's index (m_chips.size()-1) is never one of the "real" chips placed
        // above (ensureChipCount() guarantees m_chips.size()>=m_reactions.size()+1), so it is
        // only ever the ellipsis or idle -- explicitly hide it here in case an EARLIER
        // packForWidth() call left it visible (overflow is width-independent, so this can flip
        // false->true->false purely from setMaxVisibleChips()/setReactions() without an
        // intervening setReactions() hide-pass to already cover it). m_chips is empty only when
        // packForWidth() runs (e.g. via resizeEvent()) before any setReactions() call has ever
        // happened -- nothing to hide yet.
        m_chips.back()->setVisible(false);
    }

    m_lastRowRect=result.lastRowRect.translated(contentsMargins().left(),contentsMargins().top());
    m_packedSize=QSize(result.totalSize.width()+horizontalTotalMargin(this),
                        result.totalSize.height()+contentsMargins().top()+contentsMargins().bottom());
    m_tailFits=result.tailFits;

    // sizeHint() just (potentially) changed. This widget has no QLayout of its own, but a HOST
    // that wraps it in one (ChatMessageReactions does exactly that, around exactly this row) has
    // its own QWidgetItemV2 cache for this widget as a child item -- that cache is keyed on
    // widget identity, not on anything this method touched, so only updateGeometry() invalidates
    // it. Safe/cheap to call unconditionally, including when nothing actually changed.
    updateGeometry();

    return m_packedSize.width();
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::repack()
{
    packForWidth(width());
}

//--------------------------------------------------------------------------

void ChatMessageReactionsRow::resizeEvent(QResizeEvent* event)
{
    Frame::resizeEvent(event);
    repack();
}

//--------------------------------------------------------------------------

QSize ChatMessageReactionsRow::sizeHint() const
{
    return m_packedSize;
}

//--------------------------------------------------------------------------

ChatMessageReactions::ChatMessageReactions(QWidget* parent)
    : AbstractChatMessageReactions(parent)
{
    setObjectName("chatMessageReactions");

    m_row=new ChatMessageReactionsRow(this);
    m_row->setObjectName("row");
    Layout::vertical(this)->addWidget(m_row);

    connect(m_row,&ChatMessageReactionsRow::toggleRequested,this,&AbstractChatMessageReactions::toggleRequested);
    connect(m_row,&ChatMessageReactionsRow::moreRequested,this,&AbstractChatMessageReactions::moreRequested);
}

//--------------------------------------------------------------------------

ChatMessageReactions::~ChatMessageReactions()
{
}

//--------------------------------------------------------------------------

void ChatMessageReactions::setReactions(ChatReactions reactions)
{
    m_row->setReactions(std::move(reactions));

    // The row can flip empty<->non-empty right here, well after this section was first attached
    // -- updateWidgets() only sets setVisible(!isEmpty()) ONCE, at attach time (see its own
    // comment), and every OTHER section normally has its content set before it is ever attached,
    // so that one-shot decision was never revisited. Reactions are different: a message starts
    // with none and gains its first one interactively while already on screen, so this section
    // must keep its own visibility in sync on every data change, not just the first one.
    setVisible(!isEmpty());

    // Mirrors ChatMessageBottom::refreshPlacement()'s identical reasoning: this section's own
    // natural size just changed, which can also flip trailingSection() to/from this section (see
    // AbstractChatMessageContent::trailingSection()) and therefore the inline-vs-row bottom
    // placement -- re-running the full negotiation is what recomputes all of that. Safe and cheap
    // before the first real negotiation pass (renegotiateBubbleWidth() is a no-op then).
    if (chatContent()!=nullptr)
    {
        chatContent()->renegotiateBubbleWidth();
    }
}

//--------------------------------------------------------------------------

void ChatMessageReactions::setInteractive(bool enable)
{
    m_row->setInteractive(enable);
}

//--------------------------------------------------------------------------

int ChatMessageReactions::bubbleWidthHint(int forMaxWidth)
{
    // No tail reservation here -- making room for an INLINE time/status row seated beside the
    // last chip is entirely ChatMessageBottom's own job (its bubbleWidthHint() returns
    // chatContent()->inlineBubbleWidth() while isBottomInline(), which is what actually widens
    // the bubble via the max() reduction in AbstractChatMessageContent::updateBubbleWidth()'s
    // section loop). This section only reports its OWN plain packed width at forMaxWidth.
    return m_row->packForWidth(forMaxWidth,0);
}

//--------------------------------------------------------------------------

void ChatMessageReactions::updateMaximumBubbleWidth()
{
    // Re-pack at the FINAL committed width -- which can be wider than the forMaxWidth passed to
    // bubbleWidthHint() above (e.g. ChatMessageBottom's own inline width won the section loop's
    // max()). Unlike ChatMessageImages' own pin (only re-pack when narrower -- see
    // chatmessageimages.cpp), flowPack() is a pure function of its inputs, so repacking at ANY
    // width (wider or narrower) is always correct; ChatMessageReactionsRow::packForWidth()'s own
    // memo already makes an unchanged width a no-op.
    m_row->packForWidth(chatContent()->maximumBubbleWidth(),0);
}

//--------------------------------------------------------------------------

QRect ChatMessageReactions::lastTextLineRect() const
{
    return m_row->lastRowRect();
}

//--------------------------------------------------------------------------

int ChatMessageReactions::ownWidthCeiling() const
{
    return m_maxBubbleWidth;
}

//--------------------------------------------------------------------------

void ChatMessageReactions::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageReactions::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
}

UISE_DESKTOP_NAMESPACE_END
