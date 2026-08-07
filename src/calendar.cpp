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

/** @file uise/desktop/src/calendar.cpp
*
*  Defines Calendar and CalendarDay.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QApplication>
#include <QLabel>
#include <QFrame>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QGridLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/datetime.hpp>

#include <uise/desktop/style.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/datetimepicker.hpp>
#include <uise/desktop/datetimeinput.hpp>

#include <uise/desktop/ripple.hpp>
#include <uise/desktop/calendar.hpp>
#include <uise/desktop/detail/calendar_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

constexpr const char* PropAdjacent="adjacent";
constexpr const char* PropOutOfRange="outOfRange";
constexpr const char* PropToday="today";
constexpr const char* PropWeekend="weekend";
constexpr const char* PropHovered="hovered";
constexpr const char* PropBand="band";
constexpr const char* PropBandEdge="bandEdge";
constexpr const char* PropMarked="marked";
constexpr const char* PropChecked="checked";

// QWheelEvent::angleDelta() reports eighths of a degree; a standard mouse wheel "notch" is
// always exactly this many units, per Qt's own documentation.
constexpr int WheelNotch=120;

//--------------------------------------------------------------------------

bool hasControlModifier(Qt::KeyboardModifiers mods) noexcept
{
    // Qt maps the macOS Command key onto Qt::ControlModifier by default, so a single
    // ControlModifier check covers Ctrl on Windows/Linux and Cmd on macOS. MetaModifier is
    // accepted too so the widget keeps working in an application that sets
    // Qt::AA_MacDontSwapCtrlAndMeta (which routes Cmd to MetaModifier instead).
    return mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
}

//--------------------------------------------------------------------------

// Auto starts collapsed to Activation (nothing kept selected until the user escalates);
// ExtendedSelection starts at SingleSelection instead, since a plain click there always keeps
// exactly one date selected -- see CalendarMode::ExtendedSelection. Every other mode is already
// its own effective mode.
CalendarMode initialEffectiveMode(CalendarMode mode) noexcept
{
    switch (mode)
    {
        case (CalendarMode::Auto):
            return CalendarMode::Activation;
        case (CalendarMode::ExtendedSelection):
            return CalendarMode::SingleSelection;
        default:
            break;
    }
    return mode;
}

//--------------------------------------------------------------------------

const char* bandEdgeName(CalendarDay::BandEdge edge) noexcept
{
    switch (edge)
    {
        case (CalendarDay::BandEdge::Left):
            return "left";
        case (CalendarDay::BandEdge::Right):
            return "right";
        case (CalendarDay::BandEdge::Both):
            return "both";
        default:
            break;
    }
    return "none";
}

//--------------------------------------------------------------------------

const char* markedName(CalendarDay::Marked marked) noexcept
{
    switch (marked)
    {
        case (CalendarDay::Marked::Point):
            return "point";
        case (CalendarDay::Marked::Endpoint):
            return "endpoint";
        default:
            break;
    }
    return "none";
}

//--------------------------------------------------------------------------

QString longDateText(const QDate& date, const QLocale& locale)
{
    // Literal "Month D, YYYY" composition, per spec -- QLocale::LongFormat includes the
    // weekday name in most locales, which the dates-list rows deliberately omit.
    return locale.toString(date,QStringLiteral("MMMM d, yyyy"));
}

//--------------------------------------------------------------------------

CalendarDay* calendarDayAt(const QPoint& globalPos)
{
    // Used while dragging: the widget that received the press keeps all subsequent move/release
    // events regardless of where the cursor actually is (Qt's implicit mouse grab), so finding
    // "which cell is the cursor over right now" has to go through the actual widget under that
    // global point -- which may be the day's #dayLabel child, hence the walk up the parent chain.
    auto w=QApplication::widgetAt(globalPos);
    while (w!=nullptr)
    {
        if (auto day=qobject_cast<CalendarDay*>(w))
        {
            return day;
        }
        w=w->parentWidget();
    }
    return nullptr;
}

//--------------------------------------------------------------------------

void setPropertyRepolish(QWidget* widget, const char* name, const QVariant& value)
{
    if (widget==nullptr)
    {
        return;
    }

    auto current=widget->property(name);
    if (current.isValid() && current==value)
    {
        return;
    }

    widget->setProperty(name,value);
    Style::updateWidgetStyle(widget);
}

} // anonymous namespace

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

CalendarDay::CalendarDay(QWidget* parent)
    : Frame(parent)
{
    auto l=Layout::vertical(this);

    m_label=new QLabel(this);
    m_label->setObjectName(QStringLiteral("dayLabel"));
    m_label->setAlignment(Qt::AlignCenter);
    l->addWidget(m_label,0,Qt::AlignCenter);

    // Installed on the label, not on the cell itself: dayLabel is the round 28x28 marker (see
    // calendar.qss), while the outer CalendarDay frame is the whole 34x34 grid cell including
    // the band background -- an ellipse ripple belongs on the marker, not the square cell.
    // Manually triggered (see mousePressEvent()/mouseReleaseEvent()/leaveEvent() below) rather
    // than auto-triggered, because a ripple must only ever appear on a selectable day.
    m_ripple=RippleOverlay::install(m_label);
    m_ripple->setAutoTrigger(false);

    applyState();
}

//--------------------------------------------------------------------------

void CalendarDay::setDate(const QDate& date)
{
    if (m_date==date)
    {
        return;
    }

    m_date=date;
    m_label->setText(date.isValid() ? QString::number(date.day()) : QString{});
}

//--------------------------------------------------------------------------

QDate CalendarDay::date() const noexcept
{
    return m_date;
}

//--------------------------------------------------------------------------

void CalendarDay::setAdjacent(bool enable) noexcept
{
    if (m_adjacent==enable)
    {
        return;
    }
    m_adjacent=enable;
    m_dirty=true;
}

//--------------------------------------------------------------------------

bool CalendarDay::isAdjacent() const noexcept
{
    return m_adjacent;
}

//--------------------------------------------------------------------------

void CalendarDay::setOutOfRange(bool enable) noexcept
{
    if (m_outOfRange==enable)
    {
        return;
    }
    m_outOfRange=enable;
    m_dirty=true;
}

//--------------------------------------------------------------------------

bool CalendarDay::isOutOfRange() const noexcept
{
    return m_outOfRange;
}

//--------------------------------------------------------------------------

void CalendarDay::setToday(bool enable) noexcept
{
    if (m_today==enable)
    {
        return;
    }
    m_today=enable;
    m_dirty=true;
}

//--------------------------------------------------------------------------

bool CalendarDay::isToday() const noexcept
{
    return m_today;
}

//--------------------------------------------------------------------------

void CalendarDay::setWeekend(bool enable) noexcept
{
    if (m_weekend==enable)
    {
        return;
    }
    m_weekend=enable;
    m_dirty=true;
}

//--------------------------------------------------------------------------

bool CalendarDay::isWeekend() const noexcept
{
    return m_weekend;
}

//--------------------------------------------------------------------------

void CalendarDay::setMarked(Marked marked) noexcept
{
    if (m_marked==marked)
    {
        return;
    }
    m_marked=marked;
    m_dirty=true;
}

//--------------------------------------------------------------------------

CalendarDay::Marked CalendarDay::marked() const noexcept
{
    return m_marked;
}

//--------------------------------------------------------------------------

void CalendarDay::setBand(bool enable, BandEdge edge) noexcept
{
    if (m_band==enable && m_bandEdge==edge)
    {
        return;
    }
    m_band=enable;
    m_bandEdge=edge;
    m_dirty=true;
}

//--------------------------------------------------------------------------

bool CalendarDay::isBand() const noexcept
{
    return m_band;
}

//--------------------------------------------------------------------------

CalendarDay::BandEdge CalendarDay::bandEdge() const noexcept
{
    return m_bandEdge;
}

//--------------------------------------------------------------------------

bool CalendarDay::isSelectable() const noexcept
{
    return m_date.isValid() && !m_adjacent && !m_outOfRange;
}

//--------------------------------------------------------------------------

void CalendarDay::applyState()
{
    if (!m_dirty)
    {
        return;
    }
    m_dirty=false;

    setProperty(PropAdjacent,m_adjacent);
    setProperty(PropOutOfRange,m_outOfRange);
    setProperty(PropToday,m_today);
    setProperty(PropWeekend,m_weekend);
    setProperty(PropHovered,m_hovered);
    setProperty(PropBand,m_band ? QStringLiteral("band") : QStringLiteral("none"));
    setProperty(PropBandEdge,QString::fromLatin1(bandEdgeName(m_bandEdge)));
    m_label->setProperty(PropMarked,QString::fromLatin1(markedName(m_marked)));

    // Only touch the cursor for the cell actually under the mouse right now (m_hovered), not
    // for the other ~41 during a full-page rebuild (month navigation, incl. the month picker's
    // Apply). Cells persist across page changes (see the class doc comment) and keep real
    // screen geometry even while completely covered by an open dropdown popup; unconditionally
    // calling setCursor() here on macOS was observed to bleed a PointingHandCursor through to
    // the popup on top of them, even though the mouse was never over these cells at all.
    // leaveEvent() below resets the cursor explicitly on the one genuine hover-exit transition.
    if (m_hovered)
    {
        setCursor(isSelectable() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    Style::repolishRecursive(this);
}

//--------------------------------------------------------------------------

QLabel* CalendarDay::dayLabel() const noexcept
{
    return m_label;
}

//--------------------------------------------------------------------------

void CalendarDay::enterEvent(QEnterEvent* event)
{
    if (isSelectable() && !m_hovered)
    {
        m_hovered=true;
        m_dirty=true;
        applyState();
        emit hovered(m_date,true);
    }
    Frame::enterEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDay::leaveEvent(QEvent* event)
{
    if (m_hovered)
    {
        m_hovered=false;
        m_dirty=true;
        applyState();
        // applyState() above only updates the cursor while m_hovered is true (see its own
        // comment), so the transition back to not-hovered needs an explicit reset here.
        setCursor(Qt::ArrowCursor);
        emit hovered(m_date,false);
    }
    // A no-op when nothing is held -- but a held ripple must not survive the cursor leaving the
    // cell mid-press (e.g. a press that turns into a drag out of this cell without a matching
    // release ever reaching it).
    m_ripple->release();
    Frame::leaveEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDay::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && isSelectable())
    {
        event->accept();
        m_pressed=true;
        m_dragged=false;
        m_lastDragDate=m_date;
        m_ripple->start(m_label->mapFromParent(event->pos()));
        emit dragStarted(m_date,event->modifiers());
        return;
    }
    Frame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressed)
    {
        auto day=calendarDayAt(event->globalPosition().toPoint());
        if (day!=nullptr && day->isSelectable())
        {
            if (day!=this)
            {
                m_dragged=true;
            }
            if (day->date()!=m_lastDragDate)
            {
                m_lastDragDate=day->date();
                emit dragMoved(m_lastDragDate);
            }
        }
    }
    Frame::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && m_pressed)
    {
        m_pressed=false;
        m_ripple->release();

        auto day=calendarDayAt(event->globalPosition().toPoint());
        auto finalDate=(day!=nullptr && day->isSelectable()) ? day->date() : m_date;

        emit dragFinished(finalDate,event->modifiers(),m_dragged);
        if (!m_dragged && rect().contains(event->pos()))
        {
            emit clicked(m_date,event->modifiers());
        }
    }
    Frame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

void ClickableFrame::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        event->accept();
        emit clicked();
        return;
    }
    Frame::mousePressEvent(event);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

void CalendarHeaderFrame::mousePressEvent(QMouseEvent* event)
{
    auto child=childAt(event->pos());
    while (child!=nullptr && child!=this)
    {
        bool isNonTrigger=false;
        for (auto&& w: m_nonTrigger)
        {
            if (!w.isNull() && w.data()==child)
            {
                isNonTrigger=true;
                break;
            }
        }
        if (isNonTrigger)
        {
            Frame::mousePressEvent(event);
            return;
        }
        child=child->parentWidget();
    }

    Frame::mousePressEvent(event);
    if (event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

CalendarDateRow::CalendarDateRow(const QDate& date, const QString& text, QWidget* parent)
    : Frame(parent),
      m_date(date)
{
    auto l=Layout::horizontal(this);

    m_label=new Label(text,this);
    m_label->setObjectName(QStringLiteral("dateRowLabel"));
    // Same fix as the header's weekday/month labels: Label defaults to
    // Qt::TextSelectableByMouse, which would make clicking the row's text start a text
    // selection instead of reaching CalendarDateRow::mousePressEvent (navigate-to-month).
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    l->addWidget(m_label,1);

    m_removeButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::clear"),this),this);
    m_removeButton->setObjectName(QStringLiteral("dateRowRemoveButton"));
    m_removeButton->setVisible(false);
    l->addWidget(m_removeButton);

    setCursor(Qt::PointingHandCursor);

    // Auto-trigger stays on (unlike CalendarDay's ripple): every row here already represents an
    // existing selected date, so there is no isSelectable()-style condition to gate on -- the
    // whole row is always clickable, matching mouseReleaseEvent()'s own unconditional handling
    // below. m_removeButton (a PushButton) already gets its own ripple independently, see
    // pushbutton.cpp.
    m_ripple=RippleOverlay::install(this);

    connect(m_removeButton,&PushButton::clicked,this,[this]()
    {
        emit removeRequested(m_date);
    });
}

//--------------------------------------------------------------------------

void CalendarDateRow::enterEvent(QEnterEvent* event)
{
    m_removeButton->setVisible(true);
    setProperty(PropHovered,true);
    Style::repolishRecursive(this);
    Frame::enterEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDateRow::leaveEvent(QEvent* event)
{
    m_removeButton->setVisible(false);
    setProperty(PropHovered,false);
    Style::repolishRecursive(this);
    Frame::leaveEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDateRow::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        event->accept();
        return;
    }
    Frame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDateRow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && rect().contains(event->pos()))
    {
        emit clicked(m_date);
    }
    Frame::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

CalendarDatesDropdown::CalendarDatesDropdown(Calendar* owner, QWidget* parent)
    : DropdownFrame(parent),
      m_owner(owner)
{
    auto content=new QFrame(this);
    content->setObjectName(QStringLiteral("calendarDatesDropdownContent"));
    auto l=Layout::vertical(content);

    m_titleLabel=new Label(content);
    m_titleLabel->setObjectName(QStringLiteral("datesTitleLabel"));
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    l->addWidget(m_titleLabel);

    m_separator=new QFrame(content);
    m_separator->setObjectName(QStringLiteral("datesSeparator"));
    m_separator->setFixedHeight(1);
    l->addWidget(m_separator);

    m_scrollArea=new ScrollArea(content);
    m_scrollArea->setObjectName(QStringLiteral("datesScrollArea"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    // QAbstractScrollArea's viewport is a separate internal child that paints its own opaque
    // QPalette::Base background by default, regardless of "background-color: transparent" QSS
    // on the QScrollArea itself -- a classic Qt gotcha that otherwise leaves a mismatched,
    // undertheme-colored block sitting on top of DropdownFrame's own translucent-window
    // background (painted in one pass across the whole popup by DropdownFrame::paintEvent()).
    m_scrollArea->viewport()->setAutoFillBackground(false);
    // ScrollArea overrides minimumSizeHint() to zero (see its own docs) and QScrollArea's own
    // sizeHint() is otherwise derived from the contained widget's sizeHint -- for a handful of
    // freshly-created rows that can come out effectively empty by the time DropdownFrame
    // measures content (see DropdownFrame::measureContentSize()), leaving the whole popup sized
    // at (or close to) zero and therefore invisible. Pin an explicit, deterministic minimum
    // instead of relying on that computed chain; the maximum is capped dynamically (see
    // updateMaxHeight()) relative to the calendar's own current height.
    m_scrollArea->setMinimumSize(220,60);
    l->addWidget(m_scrollArea);

    auto listFrame=new QFrame(m_scrollArea);
    listFrame->setObjectName(QStringLiteral("datesListFrame"));
    m_listLayout=Layout::vertical(listFrame);
    m_listFrame=listFrame;
    m_scrollArea->setWidget(listFrame);

    setContent(content);

    // Qt::QueuedConnection: removeRequested (below) ends up back here via
    // Calendar::setSelectedDates(), synchronously from INSIDE the remove button's own click
    // handling -- rebuilding rows synchronously at that point would destroy (see
    // destroyWidget()) the very row/button whose click is still being processed. Deferring to
    // the next event-loop turn lets that click finish first. Calls rebuildRows(), NOT
    // refreshRows() -- see refreshRows()'s own comment for why resizing here, while already
    // open, is not safe.
    connect(m_owner,&Calendar::selectedDatesChanged,this,[this](const QList<QDate>& dates)
    {
        if (!isOpen())
        {
            return;
        }
        if (dates.isEmpty())
        {
            closeDropdown();
        }
        else
        {
            rebuildRows();
        }
    },Qt::QueuedConnection);
}

//--------------------------------------------------------------------------

void CalendarDatesDropdown::fillContent()
{
    refreshRows();
}

//--------------------------------------------------------------------------

void CalendarDatesDropdown::clearContent()
{
    // Remove every layout item, not just the CalendarDateRow widgets -- also drops the trailing
    // stretch added at the end of refreshRows() below, so it doesn't accumulate on every
    // refresh (QBoxLayout::addWidget() always appends after whatever is already there).
    while (auto item=m_listLayout->takeAt(0))
    {
        if (auto w=item->widget())
        {
            destroyWidget(w);
        }
        delete item;
    }
}

//--------------------------------------------------------------------------

void CalendarDatesDropdown::updateMaxHeight()
{
    // Cap relative to the calendar's own current height, but never below a sane floor -- a
    // small/default-sized calendar shouldn't squeeze the popup smaller than that.
    constexpr int MinHeight=60;
    auto cap=qMax(MinHeight,qRound(m_owner->height()*0.9));

    // QScrollArea::sizeHint() (and therefore content->sizeHint(), which is what
    // DropdownFrame::measureContentSize() actually measures the whole popup from) does not
    // reliably reflect the rows just (re)built into listFrame by the time of that measurement --
    // empirically the popup was landing at whatever minimum size we pinned below, regardless of
    // how tall the row list actually was or what setMaximumHeight() said. Sidestep that by
    // activating listFrame's own layout right here and reading ITS sizeHint() directly, then
    // pinning the scroll area's minimum to that (clamped to the cap) so the measured popup size
    // actually tracks the real content instead of an unrelated computed value.
    m_listLayout->activate();
    auto contentHeight=m_listFrame->sizeHint().height();

    auto desired=qBound(MinHeight,contentHeight,cap);
    m_scrollArea->setMinimumHeight(desired);
    m_scrollArea->setMaximumHeight(cap);
}

//--------------------------------------------------------------------------

void CalendarDatesDropdown::rebuildRows()
{
    clearContent();

    const auto dates=m_owner->selectedDates();
    m_titleLabel->setText(dates.size()==1
                           ? Calendar::tr("Selected 1 day")
                           : Calendar::tr("Selected %1 days").arg(dates.size()));

    for (auto&& date: dates)
    {
        auto row=new CalendarDateRow(date,longDateText(date,m_owner->locale()),m_listFrame);
        m_listLayout->addWidget(row);

        connect(row,&CalendarDateRow::clicked,this,[this](const QDate& d)
        {
            m_owner->setDisplayedMonth(QDate{d.year(),d.month(),1});
        });
        connect(row,&CalendarDateRow::removeRequested,this,[this](const QDate& d)
        {
            auto dates=m_owner->selectedDates();
            dates.removeOne(d);
            m_owner->setSelectedDates(dates);
        });
    }

    // Without a trailing stretch, QBoxLayout distributes any leftover height (the scroll area
    // is often taller than a short list needs) evenly around every item instead of leaving it
    // at the bottom -- which is what was reading as the rows being vertically centered.
    m_listLayout->addStretch(1);
}

//--------------------------------------------------------------------------

void CalendarDatesDropdown::refreshRows()
{
    rebuildRows();

    // Must run AFTER the rows above exist -- it measures them directly. Only safe here because
    // this is the fresh-opening path (see fillContent()): DropdownFrame measures/sizes content
    // once per opening and does not expect it to change size afterwards (see its fullSize()
    // docs). A LIVE update (e.g. from a row's own remove button, via the selectedDatesChanged
    // connection below) uses rebuildRows() alone instead, deliberately never touching the scroll
    // area's size while already open.
    updateMaxHeight();
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

void Calendar_p::buildHeader()
{
    header=new CalendarHeaderFrame(self);
    header->setObjectName(QStringLiteral("headerFrame"));
    // KNOWN ISSUE (macOS, confirmed 2026-08-08): this cursor -- and titleFrame's own below --
    // shows correctly for a standalone Calendar, but stays a plain arrow (not the pointing hand)
    // for a Calendar hosted inside a CalendarDropdown popup, even while hovering/moving the
    // mouse over the header; the click itself still works (opens the month picker). Not caused
    // by headerClickable (defaults true, never disabled by CalendarDropdown/CalendarInput) or by
    // any QSS rule -- most likely a Qt/Cocoa limitation where a non-activating popup window
    // (DropdownFrame opens Qt::Tool + Qt::WA_ShowWithoutActivating, deliberately, to avoid
    // stealing window activation) never becomes "key" and so never gets its cursor icon
    // recomputed, regardless of QWidget::setCursor(). Unconfirmed on Windows/Linux. If this needs
    // fixing later, the standard workaround is QGuiApplication::setOverrideCursor()/
    // restoreOverrideCursor() in enterEvent()/leaveEvent() instead of a plain setCursor(), since
    // an application-level override cursor bypasses whatever per-window mechanism is failing here
    // -- but every setOverrideCursor() must be exactly paired with a restoreOverrideCursor(), or
    // the cursor gets stuck overridden app-wide.
    header->setCursor(Qt::PointingHandCursor);
    auto hl=Layout::vertical(header);

    headerTop=new QFrame(header);
    headerTop->setObjectName(QStringLiteral("headerTopFrame"));
    auto htl=Layout::horizontal(headerTop);
    hl->addWidget(headerTop);

    headerTopLeft=new QFrame(headerTop);
    headerTopLeft->setObjectName(QStringLiteral("headerTopLeftFrame"));
    auto htll=Layout::horizontal(headerTopLeft);
    htl->addWidget(headerTopLeft,1);

    titleFrame=new ClickableFrame(headerTopLeft);
    titleFrame->setObjectName(QStringLiteral("titleFrame"));
    titleFrame->setCursor(Qt::PointingHandCursor);   // see the known cursor issue noted on header's own setCursor() above
    auto tfl=Layout::horizontal(titleFrame);
    titleLabel=new ElidedLabel(titleFrame);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setElideMode(Qt::ElideRight);
    titleLabel->setMaxLines(1);
    // Without this, ElidedLabel::sizeHint() reports the width of the FULL, un-elided text (see
    // elidedlabel.cpp), which would make the whole header -- and therefore the whole Calendar --
    // grow to fit a long multiple-selection date list instead of eliding it. setIgnoreSizeHint()
    // makes it shrink freely and elide to whatever width the layout actually gives it.
    titleLabel->setIgnoreSizeHint(true);
    tfl->addWidget(titleLabel);
    // Stretch factor well above the trailing spacer's (see addStretch() below) so titleFrame
    // claims almost all of the row's leftover width instead of splitting it evenly with that
    // spacer -- the spacer only matters for pushing clearButton to the edge when titleFrame is
    // hidden (range chips showing instead).
    htll->addWidget(titleFrame,8);

    rangeFromButton=new PushButton(QString{},headerTopLeft);
    rangeFromButton->setObjectName(QStringLiteral("rangeFromButton"));
    htll->addWidget(rangeFromButton);

    rangeDashLabel=new Label(QStringLiteral("–"),headerTopLeft);
    rangeDashLabel->setObjectName(QStringLiteral("rangeDashLabel"));
    rangeDashLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    htll->addWidget(rangeDashLabel);

    rangeToButton=new PushButton(QString{},headerTopLeft);
    rangeToButton->setObjectName(QStringLiteral("rangeToButton"));
    htll->addWidget(rangeToButton);

    htll->addStretch(1);

    clearButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::clear"),self),headerTopLeft);
    clearButton->setObjectName(QStringLiteral("clearButton"));
    htll->addWidget(clearButton);

    headerTopRight=new QFrame(headerTop);
    headerTopRight->setObjectName(QStringLiteral("headerTopRightFrame"));
    auto htrl=Layout::horizontal(headerTopRight);
    htl->addWidget(headerTopRight);

    prevButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::prev"),self),headerTopRight);
    prevButton->setObjectName(QStringLiteral("prevButton"));
    htrl->addWidget(prevButton);

    nextButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::next"),self),headerTopRight);
    nextButton->setObjectName(QStringLiteral("nextButton"));
    htrl->addWidget(nextButton);

    auto headerBottom=new QFrame(header);
    headerBottom->setObjectName(QStringLiteral("headerBottomFrame"));
    auto hbl=Layout::vertical(headerBottom);
    hl->addWidget(headerBottom);

    weekDaysFrame=new QFrame(headerBottom);
    weekDaysFrame->setObjectName(QStringLiteral("weekDaysFrame"));
    auto wdl=Layout::horizontal(weekDaysFrame);
    for (int i=0;i<Calendar::GridColumns;++i)
    {
        auto lbl=new Label(weekDaysFrame);
        lbl->setObjectName(QStringLiteral("weekDayLabel"));
        lbl->setAlignment(Qt::AlignCenter);
        // Label defaults to Qt::TextSelectableByMouse, which makes QLabel actively consume the
        // press itself (for text selection) instead of ignoring it -- clear it back to
        // Qt::NoTextInteraction so a click here falls through to the header like a plain,
        // non-interactive widget and opens the month picker.
        lbl->setTextInteractionFlags(Qt::NoTextInteraction);
        wdl->addWidget(lbl,1);
        weekDayLabels[i]=lbl;
    }
    hbl->addWidget(weekDaysFrame);

    monthLabel=new Label(headerBottom);
    monthLabel->setObjectName(QStringLiteral("monthLabel"));
    monthLabel->setAlignment(Qt::AlignCenter);
    monthLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    hbl->addWidget(monthLabel);

    header->addNonTriggerWidget(prevButton);
    header->addNonTriggerWidget(nextButton);
    header->addNonTriggerWidget(clearButton);
    header->addNonTriggerWidget(rangeFromButton);
    header->addNonTriggerWidget(rangeToButton);
    header->addNonTriggerWidget(titleFrame);

    QObject::connect(header,&CalendarHeaderFrame::clicked,self,&Calendar::openMonthPicker);
    QObject::connect(titleFrame,&ClickableFrame::clicked,self,[this]()
    {
        if (!headerClickable)
        {
            return;
        }
        if (effective==CalendarMode::MultipleSelection)
        {
            if (!multiple.empty())
            {
                datesDropdown->toggleBelow(titleFrame);
            }
        }
        else if (effective==CalendarMode::SingleSelection && single.isValid())
        {
            self->setDisplayedMonth(QDate{single.year(),single.month(),1});
        }
        else
        {
            self->openMonthPicker();
        }
    });
    QObject::connect(prevButton,&PushButton::clicked,self,&Calendar::showPreviousMonth);
    QObject::connect(nextButton,&PushButton::clicked,self,&Calendar::showNextMonth);
    QObject::connect(clearButton,&PushButton::clicked,self,&Calendar::clearSelection);
    QObject::connect(rangeFromButton,&PushButton::clicked,self,[this]()
    {
        self->setDisplayedMonth(QDate{rangeFrom.year(),rangeFrom.month(),1});
    });
    QObject::connect(rangeToButton,&PushButton::clicked,self,[this]()
    {
        self->setDisplayedMonth(QDate{rangeTo.year(),rangeTo.month(),1});
    });
}

//--------------------------------------------------------------------------

void Calendar_p::buildGrid()
{
    auto g=Layout::grid(daysFrame);
    for (int r=0;r<Calendar::GridRows;++r)
    {
        for (int c=0;c<Calendar::GridColumns;++c)
        {
            auto cell=new CalendarDay(daysFrame);
            cells[r*Calendar::GridColumns+c]=cell;
            g->addWidget(cell,r,c);
            QObject::connect(cell,&CalendarDay::clicked,self,[this](const QDate& d, Qt::KeyboardModifiers m)
            {
                onDayClicked(d,m);
            });
            QObject::connect(cell,&CalendarDay::dragStarted,self,[this](const QDate& d, Qt::KeyboardModifiers m)
            {
                onDayDragStarted(d,m);
            });
            QObject::connect(cell,&CalendarDay::dragMoved,self,[this](const QDate& d)
            {
                onDayDragMoved(d);
            });
            QObject::connect(cell,&CalendarDay::dragFinished,self,[this](const QDate& d, Qt::KeyboardModifiers m, bool dragged)
            {
                onDayDragFinished(d,m,dragged);
            });
        }
    }
}

//--------------------------------------------------------------------------

void Calendar_p::applyMetrics()
{
    for (auto&& cell: cells)
    {
        if (cell!=nullptr)
        {
            cell->setFixedSize(cellWidth,cellHeight);
        }
    }
    if (weekDaysFrame!=nullptr)
    {
        weekDaysFrame->setFixedHeight(weekDayRowHeight);
    }
    if (monthLabel!=nullptr)
    {
        monthLabel->setFixedHeight(monthLabelHeight);
    }
    if (headerTop!=nullptr && headerTop->layout()!=nullptr)
    {
        headerTop->layout()->setSpacing(headerSpacing);
    }
}

//--------------------------------------------------------------------------

Qt::DayOfWeek Calendar_p::resolvedFirstDayOfWeek() const
{
    switch (weekStart)
    {
        case (CalendarWeekStart::Monday):
            return Qt::Monday;
        case (CalendarWeekStart::Sunday):
            return Qt::Sunday;
        default:
            break;
    }
    return locale.firstDayOfWeek();
}

//--------------------------------------------------------------------------

void Calendar_p::rebuildWeekDays()
{
    const auto first=static_cast<int>(resolvedFirstDayOfWeek());
    const auto workingDays=locale.weekdays();
    for (int i=0;i<Calendar::GridColumns;++i)
    {
        const auto dow=static_cast<Qt::DayOfWeek>(((first-1+i)%7)+1);
        auto label=weekDayLabels[i];
        if (label==nullptr)
        {
            continue;
        }
        label->setText(locale.dayName(static_cast<int>(dow),QLocale::ShortFormat));
        setPropertyRepolish(label,PropWeekend,!workingDays.contains(dow));
    }
}

//--------------------------------------------------------------------------

bool Calendar_p::withinLimits(const QDate& date) const
{
    if (!date.isValid())
    {
        return false;
    }
    if (minDate.isValid() && date<minDate)
    {
        return false;
    }
    if (maxDate.isValid() && date>maxDate)
    {
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------

QDate Calendar_p::clampMonth(const QDate& month) const
{
    auto result=month;
    if (minDate.isValid())
    {
        auto minMonth=QDate{minDate.year(),minDate.month(),1};
        if (result<minMonth)
        {
            result=minMonth;
        }
    }
    if (maxDate.isValid())
    {
        auto maxMonth=QDate{maxDate.year(),maxDate.month(),1};
        if (result>maxMonth)
        {
            result=maxMonth;
        }
    }
    return result;
}

//--------------------------------------------------------------------------

void Calendar_p::rebuildPage()
{
    const auto first=displayed;
    const auto firstDow=static_cast<int>(first.dayOfWeek());
    const auto weekStartDow=static_cast<int>(resolvedFirstDayOfWeek());
    const auto lead=(firstDow-weekStartDow+7)%7;
    const auto start=first.addDays(-lead);
    const auto workingDays=locale.weekdays();

    for (int i=0;i<Calendar::GridRows*Calendar::GridColumns;++i)
    {
        const auto d=start.addDays(i);
        auto cell=cells[i];
        if (cell==nullptr)
        {
            continue;
        }
        cell->setDate(d);
        cell->setAdjacent(d.month()!=first.month() || d.year()!=first.year());
        cell->setOutOfRange(!withinLimits(d));
        cell->setToday(d==currentDate);
        cell->setWeekend(!workingDays.contains(static_cast<Qt::DayOfWeek>(d.dayOfWeek())));
    }

    restyleSelection();
    updateTitle();
    updateMonthLabel();
    updateNavButtons();
}

//--------------------------------------------------------------------------

void Calendar_p::restyleSelection()
{
    const bool rtl=(self->layoutDirection()==Qt::RightToLeft);

    auto inBand=[this](int idx) -> bool
    {
        if (idx<0 || idx>=Calendar::GridRows*Calendar::GridColumns)
        {
            return false;
        }
        auto cell=cells[idx];
        if (cell==nullptr || !cell->isSelectable() || effective!=CalendarMode::RangeSelection)
        {
            return false;
        }
        return rangeFrom.isValid() && cell->date()>=rangeFrom && cell->date()<=rangeTo;
    };

    for (int r=0;r<Calendar::GridRows;++r)
    {
        for (int c=0;c<Calendar::GridColumns;++c)
        {
            const auto idx=r*Calendar::GridColumns+c;
            auto cell=cells[idx];
            if (cell==nullptr)
            {
                continue;
            }

            const bool band=inBand(idx);
            auto edge=CalendarDay::BandEdge::None;
            if (band)
            {
                const auto prevIdx=rtl ? idx+1 : idx-1;
                const auto nextIdx=rtl ? idx-1 : idx+1;
                const bool atRowStart=rtl ? (c==Calendar::GridColumns-1) : (c==0);
                const bool atRowEnd=rtl ? (c==0) : (c==Calendar::GridColumns-1);
                const bool roundLeft=atRowStart || !inBand(prevIdx);
                const bool roundRight=atRowEnd || !inBand(nextIdx);
                if (roundLeft && roundRight)
                {
                    edge=CalendarDay::BandEdge::Both;
                }
                else if (roundLeft)
                {
                    edge=CalendarDay::BandEdge::Left;
                }
                else if (roundRight)
                {
                    edge=CalendarDay::BandEdge::Right;
                }
            }
            cell->setBand(band,edge);

            auto marked=CalendarDay::Marked::None;
            if (cell->isSelectable())
            {
                switch (effective)
                {
                    case (CalendarMode::SingleSelection):
                        if (cell->date()==single)
                        {
                            marked=CalendarDay::Marked::Point;
                        }
                        break;

                    case (CalendarMode::MultipleSelection):
                        if (multiple.count(cell->date())!=0)
                        {
                            marked=CalendarDay::Marked::Point;
                        }
                        break;

                    case (CalendarMode::RangeSelection):
                        if (cell->date()==rangeFrom || cell->date()==rangeTo)
                        {
                            marked=CalendarDay::Marked::Endpoint;
                        }
                        break;

                    default:
                        break;
                }
            }
            cell->setMarked(marked);

            cell->applyState();
        }
    }
}

//--------------------------------------------------------------------------

QString Calendar_p::shortDate(const QDate& d) const
{
    return locale.toString(d,QLocale::ShortFormat);
}

//--------------------------------------------------------------------------

void Calendar_p::updateTitle()
{
    QString text;
    switch (effective)
    {
        case (CalendarMode::Activation):
            text=dateAsMonthAndYear(displayed,locale);
            break;

        case (CalendarMode::SingleSelection):
            text=single.isValid() ? shortDate(single) : Calendar::tr("Select day");
            break;

        case (CalendarMode::RangeSelection):
            if (!rangeFrom.isValid())
            {
                text=Calendar::tr("Select days");
            }
            break;

        case (CalendarMode::MultipleSelection):
        {
            if (multiple.empty())
            {
                text=Calendar::tr("Select days");
                break;
            }
            QStringList parts;
            int n=0;
            for (auto&& d: multiple)
            {
                if (maxTitleDates>0 && n>=maxTitleDates)
                {
                    parts.append(QStringLiteral("..."));
                    break;
                }
                parts.append(shortDate(d));
                ++n;
            }
            text=parts.join(QStringLiteral(", "));
            break;
        }

        default:
            break;
    }

    titleLabel->setText(text);
    titleLabel->setToolTip(text);

    updateRangeChips();
}

//--------------------------------------------------------------------------

void Calendar_p::updateRangeChips()
{
    const bool active=(effective==CalendarMode::RangeSelection) && rangeFrom.isValid();
    titleFrame->setVisible(!active);
    rangeFromButton->setVisible(active);

    const bool showTo=active && rangeTo!=rangeFrom;
    rangeDashLabel->setVisible(showTo);
    rangeToButton->setVisible(showTo);

    if (active)
    {
        rangeFromButton->setText(shortDate(rangeFrom));
        rangeToButton->setText(shortDate(rangeTo));
    }
}

//--------------------------------------------------------------------------

void Calendar_p::updateMonthLabel()
{
    monthLabel->setText(effective==CalendarMode::Activation ? QString{} : dateAsMonthAndYear(displayed,locale));
}

//--------------------------------------------------------------------------

void Calendar_p::updateNavButtons()
{
    bool prevEnabled=true;
    bool nextEnabled=true;
    if (minDate.isValid())
    {
        prevEnabled=QDate{minDate.year(),minDate.month(),1}<displayed;
    }
    if (maxDate.isValid())
    {
        nextEnabled=QDate{maxDate.year(),maxDate.month(),1}>displayed;
    }
    prevButton->setEnabled(prevEnabled);
    nextButton->setEnabled(nextEnabled);
}

//--------------------------------------------------------------------------

void Calendar_p::updateClearButton()
{
    const bool hasSel=single.isValid() || rangeFrom.isValid() || !multiple.empty();
    clearButton->setVisible(effective!=CalendarMode::Activation && hasSel);
}

//--------------------------------------------------------------------------

void Calendar_p::applyLimits()
{
    monthDropdown->picker()->setDateRange(
        minDate.isValid() ? minDate : QDate(1900,1,1),
        maxDate.isValid() ? maxDate : QDate(2999,12,31));

    bool changed=false;
    bool multipleChanged=false;

    if (single.isValid() && !withinLimits(single))
    {
        single=QDate{};
        changed=true;
    }

    for (auto it=multiple.begin();it!=multiple.end();)
    {
        if (!withinLimits(*it))
        {
            it=multiple.erase(it);
            changed=true;
            multipleChanged=true;
        }
        else
        {
            ++it;
        }
    }

    if (rangeFrom.isValid())
    {
        auto f=rangeFrom;
        auto t=rangeTo;
        if (minDate.isValid() && f<minDate)
        {
            f=minDate;
        }
        if (maxDate.isValid() && t>maxDate)
        {
            t=maxDate;
        }
        if (f>t)
        {
            f=QDate{};
            t=QDate{};
            rangePending=false;
        }
        if (f!=rangeFrom || t!=rangeTo)
        {
            rangeFrom=f;
            rangeTo=t;
            changed=true;
        }
    }

    displayed=clampMonth(displayed);
    updateNavButtons();

    if (multipleChanged)
    {
        emit self->selectedDatesChanged(QList<QDate>(multiple.begin(),multiple.end()));
    }
    if (changed)
    {
        emitSelectionChanged();
    }
}

//--------------------------------------------------------------------------

void Calendar_p::setEffectiveMode(CalendarMode m)
{
    if (effective==m)
    {
        return;
    }
    effective=m;

    restyleSelection();
    updateTitle();
    updateMonthLabel();
    updateClearButton();

    if (mode==CalendarMode::Auto || mode==CalendarMode::ExtendedSelection)
    {
        emit self->effectiveModeChanged(effective);
    }
}

//--------------------------------------------------------------------------

void Calendar_p::toggleMultiple(const QDate& date)
{
    auto it=multiple.find(date);
    if (it!=multiple.end())
    {
        multiple.erase(it);
    }
    else
    {
        multiple.insert(date);
    }

    emit self->selectedDatesChanged(QList<QDate>(multiple.begin(),multiple.end()));
    emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar_p::clickRange(const QDate& date)
{
    if (!rangePending)
    {
        rangeAnchor=date;
        rangeFrom=date;
        rangeTo=date;
        rangePending=true;
        emitSelectionChanged();
    }
    else
    {
        rangePending=false;
        if (date==rangeAnchor)
        {
            // second click landed back on the anchor day -- a degenerate one-day "range" is
            // rarely what the user meant to pick, so treat it as cancelling the gesture instead
            // of committing it; emitSelectionChanged() below detects the now-empty selection and
            // handles selectionCleared()/Auto-mode revert itself.
            rangeFrom=QDate{};
            rangeTo=QDate{};
            emitSelectionChanged();
            return;
        }
        rangeFrom=std::min(rangeAnchor,date);
        rangeTo=std::max(rangeAnchor,date);
        emit self->rangeSelected(rangeFrom,rangeTo);
        emitSelectionChanged();
    }
}

//--------------------------------------------------------------------------

bool Calendar_p::isRangeDragEligible() const
{
    // Dragging is meaningful in RangeSelection itself (explicit or already-escalated Auto), as
    // the gesture that escalates Auto's Activation into RangeSelection in the first place --
    // mirroring what Shift+click already does for a plain click -- and unconditionally in
    // ExtendedSelection, which (like Shift+click there) always replaces the whole selection with
    // a fresh range regardless of what effectiveMode() currently is. Multiple/Single/explicit
    // Activation have no drag semantics of their own, so a drag there is simply ignored (the
    // release still falls through to a plain click if it never left the pressed cell).
    return effective==CalendarMode::RangeSelection
           || mode==CalendarMode::ExtendedSelection
           || (mode==CalendarMode::Auto && effective==CalendarMode::Activation);
}

//--------------------------------------------------------------------------

void Calendar_p::onDayDragStarted(const QDate& date, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    dragAnchor=date;
    dragActive=false;
}

//--------------------------------------------------------------------------

void Calendar_p::onDayDragMoved(const QDate& date)
{
    if (!isRangeDragEligible())
    {
        return;
    }

    if (!dragActive)
    {
        dragActive=true;
        if (mode==CalendarMode::Auto && effective==CalendarMode::Activation)
        {
            setEffectiveMode(CalendarMode::RangeSelection);
        }
        else if (mode==CalendarMode::ExtendedSelection)
        {
            single=QDate{};
            multiple.clear();
            setEffectiveMode(CalendarMode::RangeSelection);
        }
    }

    rangeAnchor=dragAnchor;
    rangeFrom=std::min(dragAnchor,date);
    rangeTo=std::max(dragAnchor,date);
    rangePending=true;
    restyleSelection();
    updateTitle();
    updateClearButton();
}

//--------------------------------------------------------------------------

void Calendar_p::onDayDragFinished(const QDate& date, Qt::KeyboardModifiers modifiers, bool dragged)
{
    Q_UNUSED(modifiers)

    const bool wasDragActive=dragActive;
    dragActive=false;

    if (!dragged || !wasDragActive || !isRangeDragEligible())
    {
        // Not a real cross-cell drag (or drag wasn't eligible in this mode) -- CalendarDay
        // already emits its separate clicked() signal for this case, handled by onDayClicked().
        return;
    }

    rangeFrom=std::min(dragAnchor,date);
    rangeTo=std::max(dragAnchor,date);
    rangePending=false;
    // rangeAnchor is already dragAnchor (set throughout by onDayDragMoved) -- a drag is a single
    // gesture with one fixed anchor (its start), same as a Shift+click never moving the anchor
    // (see onDayClicked()'s ExtendedSelection handling), so it is deliberately left untouched
    // here rather than moved to the drag's release point.
    emit self->rangeSelected(rangeFrom,rangeTo);
    emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar_p::emitSelectionChanged()
{
    restyleSelection();
    updateTitle();
    updateClearButton();

    emit self->selectionChanged();

    const bool empty=!single.isValid() && !rangeFrom.isValid() && multiple.empty();
    if (empty)
    {
        emit self->selectionCleared();
        if (mode==CalendarMode::Auto && effective!=CalendarMode::Activation)
        {
            setEffectiveMode(CalendarMode::Activation);
        }
        else if (mode==CalendarMode::ExtendedSelection && effective!=CalendarMode::SingleSelection)
        {
            // ExtendedSelection has no Activation-like empty state of its own -- it always rests
            // at SingleSelection (with single left invalid), see CalendarMode::ExtendedSelection.
            setEffectiveMode(CalendarMode::SingleSelection);
        }
    }
}

//--------------------------------------------------------------------------

void Calendar_p::onDayClicked(const QDate& date, Qt::KeyboardModifiers modifiers)
{
    emit self->dateClicked(date,modifiers);

    const bool ctrl=hasControlModifier(modifiers);
    const bool shift=modifiers.testFlag(Qt::ShiftModifier);

    if (mode==CalendarMode::ExtendedSelection)
    {
        if (shift)
        {
            // Range from the anchor -- deliberately NOT updated to date below, so repeated
            // Shift+clicks keep re-deriving the range from the same fixed point (the last plain
            // or Ctrl+click) instead of "walking" to wherever the previous Shift+click landed.
            // This is the same anchor convention file browsers and list views use (Explorer,
            // Finder, Qt's own QAbstractItemView::ExtendedSelection): Shift+click alone never
            // moves it, only a plain or Ctrl+click does.
            const auto anchor=rangeAnchor.isValid() ? rangeAnchor : date;
            single=QDate{};
            multiple.clear();
            rangeFrom=std::min(anchor,date);
            rangeTo=std::max(anchor,date);
            rangePending=false;
            setEffectiveMode(CalendarMode::RangeSelection);
            emit self->rangeSelected(rangeFrom,rangeTo);
            emitSelectionChanged();
        }
        else if (ctrl)
        {
            // Seed the multiple set from whatever was selected before -- a range collapses to
            // its individual days (mirroring Auto's own Range-to-Multiple collapse), a single
            // date becomes a one-element set -- then toggle this date in/out of it.
            if (rangeFrom.isValid())
            {
                std::set<QDate> collapsed;
                for (auto d=rangeFrom;d<=rangeTo;d=d.addDays(1))
                {
                    collapsed.insert(d);
                }
                multiple=std::move(collapsed);
                rangeFrom=QDate{};
                rangeTo=QDate{};
                rangePending=false;
            }
            else if (single.isValid())
            {
                multiple.clear();
                multiple.insert(single);
                single=QDate{};
            }
            setEffectiveMode(CalendarMode::MultipleSelection);
            toggleMultiple(date);
            rangeAnchor=date;   // moves the anchor, same as a plain click -- see the shift branch
        }
        else
        {
            // A plain click always collapses back down to exactly this one date, discarding any
            // range or Ctrl-accumulated set -- the defining "basically SingleSelection" behaviour
            // of this mode. Also moves the anchor -- see the shift branch above.
            single=date;
            rangeFrom=QDate{};
            rangeTo=QDate{};
            rangePending=false;
            multiple.clear();
            rangeAnchor=date;
            setEffectiveMode(CalendarMode::SingleSelection);
            emitSelectionChanged();
        }
        return;
    }

    if (mode!=CalendarMode::Auto)
    {
        switch (effective)
        {
            case (CalendarMode::Activation):
                emit self->dateActivated(date);
                break;

            case (CalendarMode::SingleSelection):
                single=date;
                emitSelectionChanged();
                break;

            case (CalendarMode::MultipleSelection):
                toggleMultiple(date);
                break;

            case (CalendarMode::RangeSelection):
                clickRange(date);
                break;

            default:
                break;
        }
        return;
    }

    // mode==Auto
    switch (effective)
    {
        case (CalendarMode::Activation):
            if (shift)
            {
                rangeAnchor=date;
                rangeFrom=date;
                rangeTo=date;
                rangePending=true;
                setEffectiveMode(CalendarMode::RangeSelection);
                emitSelectionChanged();
            }
            else if (ctrl)
            {
                multiple.clear();
                multiple.insert(date);
                setEffectiveMode(CalendarMode::MultipleSelection);
                emit self->selectedDatesChanged(QList<QDate>(multiple.begin(),multiple.end()));
                emitSelectionChanged();
            }
            else
            {
                emit self->dateActivated(date);
            }
            break;

        case (CalendarMode::MultipleSelection):
            if (shift)
            {
                const auto anchor=multiple.empty() ? date : *multiple.begin();
                rangeAnchor=anchor;
                rangeFrom=std::min(anchor,date);
                rangeTo=std::max(anchor,date);
                rangePending=false;
                multiple.clear();
                setEffectiveMode(CalendarMode::RangeSelection);
                emit self->rangeSelected(rangeFrom,rangeTo);
                emitSelectionChanged();
            }
            else if (ctrl)
            {
                toggleMultiple(date);
            }
            else
            {
                multiple.clear();
                multiple.insert(date);
                emit self->selectedDatesChanged(QList<QDate>(multiple.begin(),multiple.end()));
                emitSelectionChanged();
            }
            break;

        case (CalendarMode::RangeSelection):
            if (shift)
            {
                clickRange(date);
            }
            else if (ctrl)
            {
                std::set<QDate> collapsed;
                if (rangeFrom.isValid())
                {
                    for (auto d=rangeFrom;d<=rangeTo;d=d.addDays(1))
                    {
                        collapsed.insert(d);
                    }
                }
                auto it=collapsed.find(date);
                if (it!=collapsed.end())
                {
                    collapsed.erase(it);
                }
                else
                {
                    collapsed.insert(date);
                }
                multiple=std::move(collapsed);
                rangeFrom=QDate{};
                rangeTo=QDate{};
                rangePending=false;
                setEffectiveMode(CalendarMode::MultipleSelection);
                emit self->selectedDatesChanged(QList<QDate>(multiple.begin(),multiple.end()));
                emitSelectionChanged();
            }
            else
            {
                rangeAnchor=date;
                rangeFrom=date;
                rangeTo=date;
                rangePending=true;
                emitSelectionChanged();
            }
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

Calendar::Calendar(QWidget* parent)
    : Calendar(CalendarMode::Activation,parent)
{}

//--------------------------------------------------------------------------

Calendar::Calendar(CalendarMode mode, QWidget* parent)
    : Frame(parent),
      pimpl(std::make_unique<Calendar_p>())
{
    construct(mode);
}

//--------------------------------------------------------------------------

Calendar::~Calendar()
{
    destroyWidget(pimpl->monthDropdown);
    destroyWidget(pimpl->datesDropdown);
}

//--------------------------------------------------------------------------

void Calendar::construct(CalendarMode mode)
{
    setFocusPolicy(Qt::StrongFocus);

    pimpl->self=this;
    pimpl->mode=mode;
    pimpl->effective=initialEffectiveMode(mode);
    pimpl->locale=locale();
    pimpl->currentDate=QDate::currentDate();
    pimpl->maxDate=pimpl->currentDate;
    pimpl->displayed=QDate{pimpl->currentDate.year(),pimpl->currentDate.month(),1};

    auto l=Layout::vertical(this);

    pimpl->buildHeader();
    l->addWidget(pimpl->header);

    pimpl->daysFrame=new QFrame(this);
    pimpl->daysFrame->setObjectName(QStringLiteral("daysFrame"));
    pimpl->buildGrid();
    l->addWidget(pimpl->daysFrame);

    // no parent: DropdownFrame is always a top-level window regardless (see its docs) --
    // lifetime is managed explicitly via destroyWidget() in the destructor, matching the
    // DateTimeInput precedent
    pimpl->monthDropdown=new DateTimePickerDropdown(DateTimeField::YearMonth);
    // Buttons visible rather than a live-updating wheel: the picker's own value follows the
    // wheel as it scrolls (that's intrinsic to DateTimePicker, not something to suppress), but
    // forwarding every intermediate value straight into the Calendar's own displayed month made
    // the grid behind the popup repage continuously while merely scrolling to a destination --
    // jarring, and gave the user no way to back out mid-scroll. Apply/Cancel already exist on
    // DateTimePickerDropdown for exactly this: Cancel restores the wheel to the value it had on
    // open (its own internal snapshot) with no effect on the Calendar at all; Apply is the one
    // moment the picked month is actually committed, via the connection below.
    pimpl->monthDropdown->setButtonsVisible(true);
    pimpl->monthDropdown->setTriggerWidget(pimpl->header);

    pimpl->datesDropdown=new CalendarDatesDropdown(this);
    pimpl->datesDropdown->setTriggerWidget(pimpl->titleFrame);

    connect(pimpl->monthDropdown,&DateTimePickerDropdown::applied,this,[this]()
    {
        auto d=pimpl->monthDropdown->picker()->date();
        setDisplayedMonth(QDate{d.year(),d.month(),1});
    });

    connect(pimpl->datesDropdown,&DropdownFrame::shown,this,[this]()
    {
        setPropertyRepolish(pimpl->titleFrame,PropChecked,true);
    });
    connect(pimpl->datesDropdown,&DropdownFrame::hidden,this,[this]()
    {
        setPropertyRepolish(pimpl->titleFrame,PropChecked,false);
    });

    pimpl->applyMetrics();
    pimpl->applyLimits();
    pimpl->rebuildWeekDays();
    pimpl->rebuildPage();
    pimpl->updateClearButton();
}

//--------------------------------------------------------------------------

void Calendar::setMode(CalendarMode mode)
{
    if (pimpl->mode==mode)
    {
        return;
    }

    // close any popup left open from the previous mode -- e.g. the dates-management dropdown
    // would otherwise linger showing a now-irrelevant list once multiple.clear() below runs
    pimpl->monthDropdown->closeDropdown(true);
    pimpl->datesDropdown->closeDropdown(true);

    pimpl->mode=mode;
    pimpl->rangePending=false;
    pimpl->single=QDate{};
    pimpl->rangeFrom=QDate{};
    pimpl->rangeTo=QDate{};
    pimpl->multiple.clear();

    pimpl->setEffectiveMode(initialEffectiveMode(mode));
    emit selectedDatesChanged(QList<QDate>{});
    pimpl->emitSelectionChanged();
}

//--------------------------------------------------------------------------

CalendarMode Calendar::mode() const noexcept
{
    return pimpl->mode;
}

//--------------------------------------------------------------------------

CalendarMode Calendar::effectiveMode() const noexcept
{
    return pimpl->effective;
}

//--------------------------------------------------------------------------

void Calendar::setWeekStart(CalendarWeekStart value)
{
    if (pimpl->weekStart==value)
    {
        return;
    }
    pimpl->weekStart=value;
    pimpl->rebuildWeekDays();
    pimpl->rebuildPage();
}

//--------------------------------------------------------------------------

CalendarWeekStart Calendar::weekStart() const noexcept
{
    return pimpl->weekStart;
}

//--------------------------------------------------------------------------

Qt::DayOfWeek Calendar::firstDayOfWeek() const
{
    return pimpl->resolvedFirstDayOfWeek();
}

//--------------------------------------------------------------------------

void Calendar::setDateRange(const QDate& min, const QDate& max)
{
    pimpl->minDate=min;
    pimpl->maxDate=max;
    pimpl->applyLimits();
    pimpl->rebuildPage();
}

//--------------------------------------------------------------------------

void Calendar::setMinimumDate(const QDate& date)
{
    setDateRange(date,pimpl->maxDate);
}

//--------------------------------------------------------------------------

void Calendar::setMaximumDate(const QDate& date)
{
    setDateRange(pimpl->minDate,date);
}

//--------------------------------------------------------------------------

QDate Calendar::minimumDate() const noexcept
{
    return pimpl->minDate;
}

//--------------------------------------------------------------------------

QDate Calendar::maximumDate() const noexcept
{
    return pimpl->maxDate;
}

//--------------------------------------------------------------------------

QDate Calendar::displayedMonth() const noexcept
{
    return pimpl->displayed;
}

//--------------------------------------------------------------------------

void Calendar::setCurrentDate(const QDate& date)
{
    if (!date.isValid() || pimpl->currentDate==date)
    {
        return;
    }
    pimpl->currentDate=date;
    pimpl->rebuildPage();
}

//--------------------------------------------------------------------------

QDate Calendar::currentDate() const noexcept
{
    return pimpl->currentDate;
}

//--------------------------------------------------------------------------

QDate Calendar::selectedDate() const noexcept
{
    return pimpl->single;
}

//--------------------------------------------------------------------------

QDate Calendar::rangeFrom() const noexcept
{
    return pimpl->rangeFrom;
}

//--------------------------------------------------------------------------

QDate Calendar::rangeTo() const noexcept
{
    return pimpl->rangeTo;
}

//--------------------------------------------------------------------------

QList<QDate> Calendar::selectedDates() const
{
    return QList<QDate>(pimpl->multiple.begin(),pimpl->multiple.end());
}

//--------------------------------------------------------------------------

bool Calendar::hasSelection() const noexcept
{
    return pimpl->single.isValid() || pimpl->rangeFrom.isValid() || !pimpl->multiple.empty();
}

//--------------------------------------------------------------------------

void Calendar::setSelectedDate(const QDate& date)
{
    pimpl->single=date;
    // Auto/ExtendedSelection keep single/rangeFrom-rangeTo/multiple mutually exclusive and
    // effectiveMode() in sync with whichever of them is actually populated -- every other mode
    // pins effectiveMode() to itself already, so setting the "wrong" field for that mode is
    // (as before) simply inert rather than something to reconcile here.
    if ((pimpl->mode==CalendarMode::Auto || pimpl->mode==CalendarMode::ExtendedSelection) && date.isValid())
    {
        pimpl->rangeFrom=QDate{};
        pimpl->rangeTo=QDate{};
        pimpl->rangePending=false;
        pimpl->multiple.clear();
        pimpl->setEffectiveMode(CalendarMode::SingleSelection);
    }
    pimpl->emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar::setSelectedRange(const QDate& from, const QDate& to)
{
    if (!from.isValid())
    {
        pimpl->rangeFrom=QDate{};
        pimpl->rangeTo=QDate{};
    }
    else
    {
        pimpl->rangeFrom=std::min(from,to);
        pimpl->rangeTo=std::max(from,to);
        if (pimpl->mode==CalendarMode::Auto || pimpl->mode==CalendarMode::ExtendedSelection)
        {
            pimpl->single=QDate{};
            pimpl->multiple.clear();
            pimpl->setEffectiveMode(CalendarMode::RangeSelection);
        }
    }
    pimpl->rangePending=false;
    pimpl->emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar::setSelectedDates(const QList<QDate>& dates)
{
    pimpl->multiple.clear();
    for (auto&& d: dates)
    {
        if (d.isValid())
        {
            pimpl->multiple.insert(d);
        }
    }
    if ((pimpl->mode==CalendarMode::Auto || pimpl->mode==CalendarMode::ExtendedSelection) && !pimpl->multiple.empty())
    {
        pimpl->single=QDate{};
        pimpl->rangeFrom=QDate{};
        pimpl->rangeTo=QDate{};
        pimpl->setEffectiveMode(CalendarMode::MultipleSelection);
    }
    emit selectedDatesChanged(QList<QDate>(pimpl->multiple.begin(),pimpl->multiple.end()));
    pimpl->emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar::setLocale(const QLocale& locale)
{
    QWidget::setLocale(locale);
}

//--------------------------------------------------------------------------

void Calendar::setHeaderClickable(bool enable)
{
    pimpl->headerClickable=enable;
    pimpl->header->setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    // titleFrame handles its own press regardless of what the header body does (see
    // ClickableFrame), so its cursor is not implied by header's -- it must be set explicitly
    // to stay in sync with the same headerClickable gate (see the titleFrame::clicked handler
    // in buildHeader()).
    pimpl->titleFrame->setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

//--------------------------------------------------------------------------

bool Calendar::isHeaderClickable() const noexcept
{
    return pimpl->headerClickable;
}

//--------------------------------------------------------------------------

QString Calendar::title() const
{
    return pimpl->titleLabel->text();
}

//--------------------------------------------------------------------------

CalendarDay* Calendar::dayCell(const QDate& date) const
{
    if (!date.isValid())
    {
        return nullptr;
    }
    for (auto&& cell: pimpl->cells)
    {
        if (cell!=nullptr && cell->date()==date && !cell->isAdjacent())
        {
            return cell;
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

CalendarDay* Calendar::dayCell(int row, int column) const
{
    if (row<0 || row>=GridRows || column<0 || column>=GridColumns)
    {
        return nullptr;
    }
    return pimpl->cells[row*GridColumns+column];
}

//--------------------------------------------------------------------------

QWidget* Calendar::headerFrame() const noexcept
{
    return pimpl->header;
}

//--------------------------------------------------------------------------

QWidget* Calendar::daysFrame() const noexcept
{
    return pimpl->daysFrame;
}

//--------------------------------------------------------------------------

void Calendar::setCellWidth(int val) noexcept
{
    if (pimpl->cellWidth==val)
    {
        return;
    }
    pimpl->cellWidth=val;
    pimpl->applyMetrics();
}

//--------------------------------------------------------------------------

int Calendar::cellWidth() const noexcept
{
    return pimpl->cellWidth;
}

//--------------------------------------------------------------------------

void Calendar::setCellHeight(int val) noexcept
{
    if (pimpl->cellHeight==val)
    {
        return;
    }
    pimpl->cellHeight=val;
    pimpl->applyMetrics();
}

//--------------------------------------------------------------------------

int Calendar::cellHeight() const noexcept
{
    return pimpl->cellHeight;
}

//--------------------------------------------------------------------------

void Calendar::setWeekDayRowHeight(int val) noexcept
{
    if (pimpl->weekDayRowHeight==val)
    {
        return;
    }
    pimpl->weekDayRowHeight=val;
    pimpl->applyMetrics();
}

//--------------------------------------------------------------------------

int Calendar::weekDayRowHeight() const noexcept
{
    return pimpl->weekDayRowHeight;
}

//--------------------------------------------------------------------------

void Calendar::setMonthLabelHeight(int val) noexcept
{
    if (pimpl->monthLabelHeight==val)
    {
        return;
    }
    pimpl->monthLabelHeight=val;
    pimpl->applyMetrics();
}

//--------------------------------------------------------------------------

int Calendar::monthLabelHeight() const noexcept
{
    return pimpl->monthLabelHeight;
}

//--------------------------------------------------------------------------

void Calendar::setHeaderSpacing(int val) noexcept
{
    if (pimpl->headerSpacing==val)
    {
        return;
    }
    pimpl->headerSpacing=val;
    pimpl->applyMetrics();
}

//--------------------------------------------------------------------------

int Calendar::headerSpacing() const noexcept
{
    return pimpl->headerSpacing;
}

//--------------------------------------------------------------------------

void Calendar::setMaxTitleDates(int val) noexcept
{
    if (pimpl->maxTitleDates==val)
    {
        return;
    }
    pimpl->maxTitleDates=val;
    pimpl->updateTitle();
}

//--------------------------------------------------------------------------

int Calendar::maxTitleDates() const noexcept
{
    return pimpl->maxTitleDates;
}

//--------------------------------------------------------------------------

void Calendar::setDisplayedMonth(const QDate& month)
{
    if (!month.isValid())
    {
        return;
    }

    auto normalized=pimpl->clampMonth(QDate{month.year(),month.month(),1});
    if (pimpl->displayed==normalized)
    {
        return;
    }
    pimpl->displayed=normalized;
    pimpl->rebuildPage();

    pimpl->monthDropdown->picker()->setDate(pimpl->displayed);

    emit displayedMonthChanged(pimpl->displayed);
}

//--------------------------------------------------------------------------

void Calendar::showPreviousMonth()
{
    setDisplayedMonth(pimpl->displayed.addMonths(-1));
}

//--------------------------------------------------------------------------

void Calendar::showNextMonth()
{
    setDisplayedMonth(pimpl->displayed.addMonths(1));
}

//--------------------------------------------------------------------------

void Calendar::showToday()
{
    setDisplayedMonth(pimpl->currentDate);
}

//--------------------------------------------------------------------------

void Calendar::clearSelection()
{
    pimpl->single=QDate{};
    pimpl->rangeFrom=QDate{};
    pimpl->rangeTo=QDate{};
    pimpl->rangePending=false;
    pimpl->multiple.clear();
    emit selectedDatesChanged(QList<QDate>{});
    pimpl->emitSelectionChanged();
}

//--------------------------------------------------------------------------

void Calendar::openMonthPicker()
{
    if (!pimpl->headerClickable)
    {
        return;
    }
    pimpl->monthDropdown->toggleBelow(pimpl->header);
}

//--------------------------------------------------------------------------

void Calendar::closeMonthPicker()
{
    pimpl->monthDropdown->closeDropdown();
}

//--------------------------------------------------------------------------

void Calendar::changeEvent(QEvent* event)
{
    Frame::changeEvent(event);
    if (event==nullptr)
    {
        return;
    }

    switch (event->type())
    {
        case (QEvent::LocaleChange):
        case (QEvent::LanguageChange):
            pimpl->locale=locale();
            pimpl->monthDropdown->picker()->setLocale(locale());
            pimpl->rebuildWeekDays();
            pimpl->rebuildPage();
            break;

        case (QEvent::FontChange):
        case (QEvent::StyleChange):
        case (QEvent::ApplicationFontChange):
            pimpl->applyMetrics();
            break;

        case (QEvent::LayoutDirectionChange):
            pimpl->rebuildWeekDays();
            pimpl->rebuildPage();
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------

void Calendar::wheelEvent(QWheelEvent* event)
{
    auto deltaY=event->angleDelta().y();
    if (deltaY==0)
    {
        Frame::wheelEvent(event);
        return;
    }

    // Require a full notch's worth of accumulated rotation before stepping a month, so a light
    // trackpad scroll -- which sends many small fractional-notch events rather than one clean
    // +-120 step -- doesn't blow through several months for what the user felt as one light
    // gesture. A genuine mouse wheel always reports a full +-120 per notch, so it steps
    // immediately, one month per notch, exactly as before.
    pimpl->wheelAccumulated+=static_cast<float>(deltaY);
    while (pimpl->wheelAccumulated>=WheelNotch)
    {
        pimpl->wheelAccumulated-=WheelNotch;
        showPreviousMonth();
    }
    while (pimpl->wheelAccumulated<=-WheelNotch)
    {
        pimpl->wheelAccumulated+=WheelNotch;
        showNextMonth();
    }

    event->accept();
}

//--------------------------------------------------------------------------

void Calendar::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case (Qt::Key_PageUp):
        case (Qt::Key_Up):
            showPreviousMonth();
            event->accept();
            return;

        case (Qt::Key_PageDown):
        case (Qt::Key_Down):
            showNextMonth();
            event->accept();
            return;

        default:
            break;
    }

    Frame::keyPressEvent(event);
}

UISE_DESKTOP_NAMESPACE_END

//--------------------------------------------------------------------------
