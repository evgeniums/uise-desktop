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

/** @file uise/desktop/src/dropdownframe.cpp
*
*  Defines DropdownFrame.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QVariantAnimation>
#include <QPointer>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QLayout>
#include <QPainter>
#include <QStyleOption>
#include <QStyle>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/dropdownframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class DropdownFrame_p
{
    public:

        QPointer<QWidget> content;
        Qt::Corner anchor=Qt::TopLeftCorner;
        QSize fullSize;
        QRect fullRect;

        QPointer<QWidget> triggerWidget;

        // the window whose deactivate/move/resize should auto-dismiss the frame -- tracked
        // explicitly because the frame is its own top-level window (window()==this), it is no
        // longer a child of this window the way it would be if it were embedded
        QPointer<QWidget> hostWindow;

        QVariantAnimation* anim=nullptr;
        qreal t=0.0;
        bool animForward=false;

        QShortcut* escShortcut=nullptr;
        QPointer<QWidget> focusBefore;

        bool selfDismissEnabled=true;
        bool verticalFlipEnabled=true;
        bool restoreFocus=true;

        // QWidget::mousePressEvent ignores unhandled presses by default, which makes Qt
        // redeliver the SAME press event to the parent chain. Since the click that opens the
        // frame runs synchronously inside the trigger's own mousePressEvent, that redelivery
        // reaches this eventFilter with the frame already open, and would otherwise be mistaken
        // for a second, separate click meant to close it. This flag suppresses exactly that one
        // propagated redelivery.
        bool suppressNextOwnPressClose=false;

        // QAbstractAnimation::stop() emits finished() when the animation is still running. The
        // animateFrame() helper stops a possibly-running animation before restarting it in a new
        // direction; without this guard that stop() would synchronously run the finished handler
        // with the STALE direction flag (e.g. hiding and clearing content right in the middle of
        // reopening, and leaving t stuck at 1 so the next opening animates 1 -> 1 and never
        // becomes visible).
        bool animStopGuard=false;

        void stopAnimation()
        {
            animStopGuard=true;
            anim->stop();
            animStopGuard=false;
        }

        int animationDurationMs=DropdownFrame::DefaultAnimationDurationMs;
        int easingCurveType=static_cast<int>(DropdownFrame::DefaultEasingCurve);
        int offsetX=DropdownFrame::DefaultOffsetX;
        int offsetY=DropdownFrame::DefaultOffsetY;

        void repositionContent(QWidget* self)
        {
            if (content.isNull())
            {
                return;
            }

            auto m=self->contentsMargins();
            auto cw=content->width();
            auto rightAnchored=(anchor==Qt::TopRightCorner || anchor==Qt::BottomRightCorner);
            auto x=rightAnchored ? self->width()-cw-m.right() : m.left();
            content->move(x,m.top());
        }

        static QRect globalRect(QWidget* w)
        {
            if (w==nullptr)
            {
                return QRect();
            }
            return QRect(w->mapToGlobal(QPoint(0,0)),w->size());
        }
};

//--------------------------------------------------------------------------

DropdownFrame::DropdownFrame(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<DropdownFrame_p>())
{
    // a real top-level window, not embedded into the host: Qt::Tool keeps it out of the
    // taskbar/window switcher and (with WA_ShowWithoutActivating below) out of the way of the
    // host window's own activation, FramelessWindowHint drops the native title bar/border, and
    // WA_TranslucentBackground lets the QSS-drawn rounded corners show whatever is actually
    // behind the frame instead of an opaque square -- all necessary because, unlike an embedded
    // child widget, a top-level window no longer shares the host's backing store
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_NoMousePropagation,true);
    setVisible(false);

    pimpl->anim=new QVariantAnimation(this);
    connect(
        pimpl->anim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& val)
        {
            pimpl->t=val.toReal();
            applyFrame(pimpl->t);
        }
    );
    connect(
        pimpl->anim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->animStopGuard)
            {
                // spurious finished() from an explicit stop() before a restart, not a natural
                // completion -- see DropdownFrame_p::stopAnimation()
                return;
            }
            finishAnimation(pimpl->animForward);
        }
    );

    pimpl->escShortcut=new QShortcut(Qt::Key_Escape,this);
    // Qt::WindowShortcut would require this frame's own window to be the active one, which
    // WA_ShowWithoutActivating above deliberately prevents (the host window stays active) --
    // Qt::ApplicationShortcut fires regardless of which of the app's windows is active
    pimpl->escShortcut->setContext(Qt::ApplicationShortcut);
    pimpl->escShortcut->setEnabled(false);
    connect(
        pimpl->escShortcut,
        &QShortcut::activated,
        this,
        [this]()
        {
            emit closeRequested(CloseReason::Escape);
            closeDropdown();
        }
    );
    connect(
        pimpl->escShortcut,
        &QShortcut::activatedAmbiguously,
        this,
        [this]()
        {
            emit closeRequested(CloseReason::Escape);
            closeDropdown();
        }
    );
}

//--------------------------------------------------------------------------

DropdownFrame::~DropdownFrame()
{
    qApp->removeEventFilter(this);
}

//--------------------------------------------------------------------------

void DropdownFrame::setContent(QWidget* content)
{
    destroyWidget(pimpl->content);

    pimpl->content=content;
    if (content!=nullptr)
    {
        // QWidget::setParent() makes the widget invisible as a side effect of reparenting,
        // even when the new parent is the SAME as the current one -- callers normally pass a
        // widget already constructed with this frame as its parent, so calling setParent()
        // unconditionally here would redundantly hide/reshow a widget that was already
        // correctly parented, right as its geometry is about to be measured for the very
        // first time
        if (content->parentWidget()!=this)
        {
            content->setParent(this);
        }
        content->setVisible(true);
    }
}

//--------------------------------------------------------------------------

QWidget* DropdownFrame::content() const
{
    return pimpl->content;
}

//--------------------------------------------------------------------------

QWidget* DropdownFrame::takeContent()
{
    auto w=pimpl->content.data();
    if (w!=nullptr)
    {
        w->setParent(nullptr);
        pimpl->content=nullptr;
    }
    return w;
}

//--------------------------------------------------------------------------

void DropdownFrame::setAnchorCorner(Qt::Corner corner)
{
    if (pimpl->anchor==corner)
    {
        return;
    }
    pimpl->anchor=corner;
    pimpl->repositionContent(this);
}

//--------------------------------------------------------------------------

Qt::Corner DropdownFrame::anchorCorner() const noexcept
{
    return pimpl->anchor;
}

//--------------------------------------------------------------------------

void DropdownFrame::setFullSize(const QSize& size)
{
    pimpl->fullSize=size;
}

//--------------------------------------------------------------------------

QSize DropdownFrame::fullSize() const noexcept
{
    return pimpl->fullSize;
}

//--------------------------------------------------------------------------

void DropdownFrame::setTriggerWidget(QWidget* widget)
{
    pimpl->triggerWidget=widget;
}

//--------------------------------------------------------------------------

QWidget* DropdownFrame::triggerWidget() const
{
    return pimpl->triggerWidget;
}

//--------------------------------------------------------------------------

void DropdownFrame::setSelfDismissEnabled(bool enable) noexcept
{
    pimpl->selfDismissEnabled=enable;
}

bool DropdownFrame::isSelfDismissEnabled() const noexcept
{
    return pimpl->selfDismissEnabled;
}

//--------------------------------------------------------------------------

void DropdownFrame::setVerticalFlipEnabled(bool enable) noexcept
{
    pimpl->verticalFlipEnabled=enable;
}

bool DropdownFrame::isVerticalFlipEnabled() const noexcept
{
    return pimpl->verticalFlipEnabled;
}

//--------------------------------------------------------------------------

void DropdownFrame::setRestoreFocus(bool enable) noexcept
{
    pimpl->restoreFocus=enable;
}

bool DropdownFrame::isRestoreFocus() const noexcept
{
    return pimpl->restoreFocus;
}

//--------------------------------------------------------------------------

bool DropdownFrame::isOpen() const noexcept
{
    return isVisible();
}

//--------------------------------------------------------------------------

void DropdownFrame::setAnimationDurationMs(int val) noexcept
{
    pimpl->animationDurationMs=val;
}

int DropdownFrame::animationDurationMs() const noexcept
{
    return pimpl->animationDurationMs;
}

//--------------------------------------------------------------------------

void DropdownFrame::setEasingCurveType(int val) noexcept
{
    pimpl->easingCurveType=val;
}

int DropdownFrame::easingCurveType() const noexcept
{
    return pimpl->easingCurveType;
}

//--------------------------------------------------------------------------

void DropdownFrame::setOffsetX(int val) noexcept
{
    pimpl->offsetX=val;
}

int DropdownFrame::offsetX() const noexcept
{
    return pimpl->offsetX;
}

//--------------------------------------------------------------------------

void DropdownFrame::setOffsetY(int val) noexcept
{
    pimpl->offsetY=val;
}

int DropdownFrame::offsetY() const noexcept
{
    return pimpl->offsetY;
}

//--------------------------------------------------------------------------

void DropdownFrame::notifyActivated(QWidget* source)
{
    Q_UNUSED(source)
    closeDropdown();
}

//--------------------------------------------------------------------------

void DropdownFrame::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    pimpl->repositionContent(this);
}

//--------------------------------------------------------------------------

void DropdownFrame::hideEvent(QHideEvent* event)
{
    emit aboutToHide();
    QFrame::hideEvent(event);
}

//--------------------------------------------------------------------------

void DropdownFrame::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&painter,this);
}

//--------------------------------------------------------------------------

void DropdownFrame::trackHost(QWidget* host)
{
    pimpl->hostWindow=host;
}

//--------------------------------------------------------------------------

QSize DropdownFrame::measureContentSize(QMargins& outMargins)
{
    auto* c=pimpl->content.data();
    if (c==nullptr)
    {
        outMargins=QMargins();
        return QSize(1,1);
    }

    // repolish this frame itself (which recurses into content and all of its descendants too,
    // so a separate call for content is not needed): contentsMargins() (read below, from the
    // QSS "padding" rule) belongs to `this`, freshly inserted into the tree on the very first
    // open, and would otherwise still report its pre-QSS default margins the first time this
    // is measured
    Style::repolishRecursive(this);

    // QWidget::ensurePolished() (the QEvent::Polish/font-resolution path) is a separate
    // mechanism from QStyle::polish() invoked by repolishRecursive() above; sizeHint() of
    // labels and buttons depends on resolved fonts, which are only guaranteed after
    // ensurePolished(). Run it over the whole subtree before measuring -- including this frame
    // itself, whose QSS padding feeds contentsMargins() read below.
    ensurePolished();
    c->ensurePolished();
    const auto contentDescendants=c->findChildren<QWidget*>();
    for (auto* w : contentDescendants)
    {
        w->ensurePolished();
    }

    // Bust every layout's cached sizeHint in the whole subtree, not just content's own
    // top-level one: a nested widget's own layout (e.g. rows a subclass just filled via
    // fillContent()) propagates its invalidation upwards via a posted, asynchronous
    // QEvent::LayoutRequest, so on a synchronous first measurement content->sizeHint() can
    // still be answered from a stale cache. Then lay out and measure in a second pass: pass 1
    // primes geometry with the initial hint, pass 2 re-measures after the subtree has gone
    // through a real layout cycle -- QSS box-model metrics that only settle once the widgets
    // have been laid out (fresh, never-shown widgets) are then reflected in the final size,
    // matching what a second opening would measure.
    auto invalidateAll=[c,&contentDescendants]()
    {
        if (c->layout()!=nullptr)
        {
            c->layout()->invalidate();
        }
        for (auto* w : contentDescendants)
        {
            if (w->layout()!=nullptr)
            {
                w->layout()->invalidate();
            }
        }
    };

    QSize natural;
    QMargins m;
    for (int pass=0;pass<2;++pass)
    {
        invalidateAll();

        // read this frame's QSS padding fresh on every pass: like the size hints below, the
        // stylesheet-driven contentsMargins() of a freshly created frame only settle after the
        // subtree has gone through a first real resize/layout cycle, so the pass-1 reading can
        // still be stale on the very first opening
        auto margins=contentsMargins();

        auto hint=c->sizeHint();
        if (!hint.isValid())
        {
            hint=c->size();
        }

        if (pass>0 && hint==natural && margins==m)
        {
            // second measurement agrees with the first -- geometry is already correct
            break;
        }
        natural=hint;
        m=margins;

        // lay children out synchronously against this size (a plain resize would only post a
        // deferred QEvent::Resize for the next event loop pass, which is too late: the frame
        // becomes visible synchronously right after this function returns)
        c->setGeometry(m.left(),m.top(),natural.width(),natural.height());
        if (c->layout()!=nullptr)
        {
            c->layout()->activate();
        }
        for (auto* w : contentDescendants)
        {
            if (w->layout()!=nullptr)
            {
                w->layout()->activate();
            }
        }
    }

    outMargins=m;
    return natural;
}

//--------------------------------------------------------------------------

void DropdownFrame::measure(QWidget* anchor)
{
    QMargins m;
    auto natural=measureContentSize(m);
    QSize full(natural.width()+m.left()+m.right(),natural.height()+m.top()+m.bottom());

    // bounded by the anchor's screen, not the host window: the frame is a top-level window free
    // to extend past the host window's own edges, like a native menu
    auto* screen=anchor->screen();
    auto avail=screen!=nullptr ? screen->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();

    // horizontal anchor: below-left by default, flip to below-right if it would overflow the
    // screen's right edge
    auto belowLeftGlobal=anchor->mapToGlobal(QPoint(pimpl->offsetX,anchor->height()+pimpl->offsetY));

    int x=belowLeftGlobal.x();
    bool rightAnchored=false;
    if (x+full.width()>avail.right())
    {
        auto rightGlobalX=anchor->mapToGlobal(QPoint(anchor->width(),0)).x();
        x=rightGlobalX-full.width();
        rightAnchored=true;
    }
    x=qMax(avail.left(),x);

    // vertical anchor: below by default; flip above the anchor when there is not enough room
    // below but there is more room above (e.g. a per-item menu button near the bottom of a
    // scrolling list)
    int y=belowLeftGlobal.y();
    bool bottomAnchored=false;
    auto availableBelow=qMax(1,avail.bottom()-y);

    if (full.height()>availableBelow && pimpl->verticalFlipEnabled)
    {
        auto aboveGlobalY=anchor->mapToGlobal(QPoint(0,-pimpl->offsetY)).y();
        auto availableAbove=qMax(1,aboveGlobalY-avail.top());
        if (availableAbove>availableBelow)
        {
            bottomAnchored=true;
            full.setHeight(qMin(full.height(),availableAbove));
            y=aboveGlobalY-full.height();
        }
        else
        {
            full.setHeight(qMin(full.height(),availableBelow));
        }
    }
    else
    {
        full.setHeight(qMin(full.height(),availableBelow));
    }

    auto anchor_=bottomAnchored
        ? (rightAnchored ? Qt::BottomRightCorner : Qt::BottomLeftCorner)
        : (rightAnchored ? Qt::TopRightCorner : Qt::TopLeftCorner);

    pimpl->fullRect=QRect(x,y,full.width(),full.height());
    setAnchorCorner(anchor_);
    setFullSize(full);
    // content geometry and synchronous child layout were already applied inside
    // measureContentSize()'s two-pass measurement loop
}

//--------------------------------------------------------------------------

void DropdownFrame::measureAt(const QPoint& globalPos)
{
    QMargins m;
    auto natural=measureContentSize(m);
    QSize full(natural.width()+m.left()+m.right(),natural.height()+m.top()+m.bottom());

    // bounded by globalPos's own screen, not the host window: the frame is a top-level window
    // free to extend past the host window's own edges, like a native context menu
    auto* screen=QGuiApplication::screenAt(globalPos);
    if (screen==nullptr)
    {
        screen=!pimpl->triggerWidget.isNull() ? pimpl->triggerWidget->screen() : QGuiApplication::primaryScreen();
    }
    auto avail=screen!=nullptr ? screen->availableGeometry() : QRect(globalPos,QSize(1,1));

    int x=globalPos.x();
    bool rightAnchored=false;
    if (x+full.width()>avail.right())
    {
        x=avail.right()-full.width();
        rightAnchored=true;
    }
    x=qMax(avail.left(),x);

    int y=globalPos.y();
    bool bottomAnchored=false;
    auto availableBelow=qMax(1,avail.bottom()-y);

    if (full.height()>availableBelow && pimpl->verticalFlipEnabled)
    {
        auto availableAbove=qMax(1,y-avail.top());
        if (availableAbove>availableBelow)
        {
            bottomAnchored=true;
            full.setHeight(qMin(full.height(),availableAbove));
            y-=full.height();
        }
        else
        {
            full.setHeight(qMin(full.height(),availableBelow));
        }
    }
    else
    {
        full.setHeight(qMin(full.height(),availableBelow));
    }

    auto anchor_=bottomAnchored
        ? (rightAnchored ? Qt::BottomRightCorner : Qt::BottomLeftCorner)
        : (rightAnchored ? Qt::TopRightCorner : Qt::TopLeftCorner);

    pimpl->fullRect=QRect(x,y,full.width(),full.height());
    setAnchorCorner(anchor_);
    setFullSize(full);
}

//--------------------------------------------------------------------------

void DropdownFrame::applyFrame(qreal t)
{
    // never let a visible widget sit at exactly zero size: some platforms don't reliably
    // repaint a widget that grows from a literal 0x0 starting geometry
    auto w=qMax(1,qRound(t*pimpl->fullRect.width()));
    auto h=qMax(1,qRound(t*pimpl->fullRect.height()));
    auto rightAnchored=(pimpl->anchor==Qt::TopRightCorner || pimpl->anchor==Qt::BottomRightCorner);
    auto bottomAnchored=(pimpl->anchor==Qt::BottomLeftCorner || pimpl->anchor==Qt::BottomRightCorner);
    auto x=rightAnchored ? pimpl->fullRect.right()+1-w : pimpl->fullRect.left();
    auto y=bottomAnchored ? pimpl->fullRect.bottom()+1-h : pimpl->fullRect.top();
    setGeometry(x,y,w,h);
}

//--------------------------------------------------------------------------

void DropdownFrame::animateFrame(bool forward, bool immediate)
{
    if (!forward && !isVisible() && qFuzzyIsNull(pimpl->t))
    {
        return;
    }

    if (forward)
    {
        raise();
        if (!isVisible())
        {
            // only reset to the tiny starting geometry for a genuinely fresh open. If already
            // visible, this call is reversing a close animation that a quick toggle interrupted
            // mid-flight -- snapping geometry back down to (1,1) here would throw away that
            // in-flight size and jump-cut it small before growing again, instead of smoothly
            // reversing from wherever it currently is (which setStartValue(pimpl->t) below
            // already does correctly)
            setGeometry(pimpl->fullRect.x(),pimpl->fullRect.y(),1,1);
            show();
        }
        raise();
    }

    const qreal target=forward ? 1.0 : 0.0;

    if (immediate)
    {
        pimpl->stopAnimation();
        pimpl->t=target;
        applyFrame(target);
        finishAnimation(forward);
        return;
    }

    pimpl->stopAnimation();
    pimpl->anim->setDuration(pimpl->animationDurationMs);
    pimpl->anim->setEasingCurve(static_cast<QEasingCurve::Type>(pimpl->easingCurveType));
    pimpl->anim->setStartValue(pimpl->t);
    pimpl->anim->setEndValue(target);
    pimpl->animForward=forward;
    pimpl->anim->start();
}

//--------------------------------------------------------------------------

void DropdownFrame::finishAnimation(bool forward)
{
    if (forward)
    {
        emit shown();
        return;
    }

    hide();
    clearContent();
    emit hidden();
}

//--------------------------------------------------------------------------

void DropdownFrame::beginOpen(QWidget* host)
{
    // arm the redelivery guard (see suppressNextOwnPressClose) and disarm it once the
    // synchronous propagation of the opening click has fully finished
    pimpl->suppressNextOwnPressClose=true;
    QTimer::singleShot(0,this,[this](){ pimpl->suppressNextOwnPressClose=false; });

    if (pimpl->selfDismissEnabled)
    {
        pimpl->escShortcut->setEnabled(true);
        qApp->installEventFilter(this);
    }
    if (pimpl->restoreFocus)
    {
        pimpl->focusBefore=host->focusWidget();
    }

    emit aboutToShow();
    animateFrame(true,false);
}

//--------------------------------------------------------------------------

void DropdownFrame::popupBelow(QWidget* anchor)
{
    if (anchor==nullptr)
    {
        return;
    }

    auto* host=anchor->window();
    trackHost(host);

    // If the frame is still visible here, this open is reversing a close animation that a
    // previous, very quick toggle interrupted mid-flight (see the animStopGuard comment).
    // Re-filling/re-measuring in that state would tear down and rebuild content while it is
    // still on screen and mid-transition, which is exactly what leaves the popup with a wrong,
    // "stuck" size for several activations afterwards -- content and fullRect from the
    // previous, already fully-settled measurement are still valid, so just keep them and let
    // the animation reverse back towards open instead of measuring again.
    if (!isVisible())
    {
        fillContent();
        measure(anchor);
    }

    beginOpen(host);
}

//--------------------------------------------------------------------------

void DropdownFrame::popupAt(const QPoint& globalPos)
{
    auto* host=!pimpl->triggerWidget.isNull() ? pimpl->triggerWidget->window() : qobject_cast<QWidget*>(QApplication::activeWindow());
    if (host==nullptr)
    {
        return;
    }
    trackHost(host);

    if (!isVisible())
    {
        fillContent();
        measureAt(globalPos);
    }

    beginOpen(host);
}

//--------------------------------------------------------------------------

void DropdownFrame::toggleBelow(QWidget* anchor)
{
    if (isVisible())
    {
        closeDropdown();
    }
    else
    {
        popupBelow(anchor);
    }
}

//--------------------------------------------------------------------------

void DropdownFrame::closeDropdown(bool immediate)
{
    if (!isVisible() && qFuzzyIsNull(pimpl->t))
    {
        return;
    }

    if (pimpl->selfDismissEnabled)
    {
        qApp->removeEventFilter(this);
        pimpl->escShortcut->setEnabled(false);
    }

    if (pimpl->restoreFocus && pimpl->focusBefore)
    {
        pimpl->focusBefore->setFocus();
    }
    pimpl->focusBefore=nullptr;

    animateFrame(false,immediate);
}

//--------------------------------------------------------------------------

bool DropdownFrame::eventFilter(QObject* obj, QEvent* event)
{
    switch (event->type())
    {
        case (QEvent::MouseButtonPress):
        {
            if (pimpl->suppressNextOwnPressClose)
            {
                // this is a propagated redelivery of the very click that just opened the
                // frame (see suppressNextOwnPressClose) -- an unhandled press is ignored by
                // QWidget::mousePressEvent by default, so Qt keeps redelivering the SAME event
                // to each ancestor up the parent chain until something accepts it or it reaches
                // the top-level widget, re-invoking this filter once per ancestor. Only the
                // deferred reset armed in beginOpen() clears this flag, so every one of those
                // redeliveries is suppressed, not just the first.
                break;
            }

            auto* me=static_cast<QMouseEvent*>(event);
            auto g=me->globalPosition().toPoint();

            if (isVisible() && DropdownFrame_p::globalRect(this).contains(g))
            {
                // inside the frame: let it through, the content itself handles it (and may
                // call notifyActivated())
                break;
            }

            if (!pimpl->triggerWidget.isNull() && DropdownFrame_p::globalRect(pimpl->triggerWidget).contains(g))
            {
                // consume the press so the trigger's own mousePressEvent never runs and cannot
                // re-open what this press is meant to close
                emit closeRequested(CloseReason::TriggerClick);
                closeDropdown();
                return true;
            }

            // genuine outside click: close, but pass the press through so it can still
            // activate whatever else it landed on
            emit closeRequested(CloseReason::OutsideClick);
            closeDropdown();
            break;
        }

        case (QEvent::WindowDeactivate):
        {
            // the frame is its own top-level window (window()==this), no longer a child of the
            // host the way an embedded popup would be -- watch the tracked host explicitly
            // instead. WA_ShowWithoutActivating (see the constructor) keeps the host active
            // while the frame itself opens, so this does not fire as a side effect of that.
            if (obj==pimpl->hostWindow.data())
            {
                emit closeRequested(CloseReason::WindowChanged);
                closeDropdown();
            }
        }
        break;

        case (QEvent::Move): [[fallthrough]];
        case (QEvent::Resize):
        {
            if (obj==pimpl->hostWindow.data())
            {
                emit closeRequested(CloseReason::WindowChanged);
                closeDropdown(true);
            }
        }
        break;

        default:
            break;
    }

    return QFrame::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
