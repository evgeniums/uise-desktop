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
#include <QList>
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

        // see DropdownFrame::setChainParent() -- links a submenu (or any nested popup) to the
        // frame it opens out of, so the pair behaves as one popup for dismissal purposes
        QPointer<DropdownFrame> chainParent;
        QPointer<DropdownFrame> chainChild;

        // every frame that currently names this one as its chainParent -- NOT just the one that
        // happens to be open (chainChild above). A chainParent link is stable for a cached
        // submenu's whole lifetime (see DropdownFrame::detachFromChainParent()), while this
        // frame's pimpl is destroyed BEFORE ~QWidget deletes its QObject children -- a cached
        // submenu destroyed there would otherwise reach back into this freed pimpl through a
        // QPointer that ~QObject has not cleared yet (QPointer only clears on ~QObject, which
        // runs after QWidget::~QWidget()'s deleteChildren()). ~DropdownFrame severs every link
        // listed here first, while pimpl is still alive, so that invariant never breaks.
        QList<QPointer<DropdownFrame>> chainDependents;

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
        int sideOffsetX=DropdownFrame::DefaultSideOffsetX;
        int sideOffsetY=DropdownFrame::DefaultSideOffsetY;

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

        static QRect globalRect(const QWidget* w)
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
    // A click inside a top-level Qt::Tool window makes it the key window on macOS, which
    // deactivates the host and trips the WindowDeactivate close path in eventFilter() below --
    // WA_ShowWithoutActivating only covers being SHOWN, not being clicked. Nothing in a dropdown
    // ever needs keyboard focus (focus policy is NoFocus below, the Escape shortcut is
    // deliberately Qt::ApplicationShortcut, and restoreFocus assumes focus stays in the host), so
    // simply never accept window activation. Must come after setWindowFlags() above, which would
    // otherwise clear it.
    setWindowFlag(Qt::WindowDoesNotAcceptFocus,true);
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

    // never leave a parent frame with its Escape shortcut stuck disabled because the chained
    // child that was supposed to re-enable it on close got destroyed instead
    detachFromChainParent();

    // unregister from our own chainParent's dependents list, mirroring detachFromChainParent()
    // above but for the stable chainParent link rather than the ephemeral chainChild one
    if (!pimpl->chainParent.isNull())
    {
        pimpl->chainParent->pimpl->chainDependents.removeAll(this);
    }

    // sever every frame chained to this one -- not just chainChild (the one currently open) --
    // while pimpl is still alive. This MUST happen before the base QWidget destructor runs:
    // QWidget::~QWidget() deletes this frame's QObject children (cached submenus among them,
    // see DropdownFrame_p::chainDependents), and each one's own destructor calls
    // detachFromChainParent(), which would otherwise dereference this frame's already-freed
    // pimpl through a chainParent QPointer that is not yet cleared (QPointer only clears on
    // ~QObject, which runs after deleteChildren()).
    //
    // Snapshot the list and clear the member BEFORE iterating: a dependent's chainParent=nullptr
    // assignment below never re-enters this code, but doing it this way keeps the loop safe even
    // if that ever changes, and it costs nothing.
    const auto dependents=pimpl->chainDependents;
    pimpl->chainDependents.clear();
    for (const auto& dep : dependents)
    {
        if (!dep.isNull())
        {
            dep->pimpl->chainParent=nullptr;
        }
    }
    pimpl->chainChild=nullptr;
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

void DropdownFrame::setSideOffsetX(int val) noexcept
{
    pimpl->sideOffsetX=val;
}

int DropdownFrame::sideOffsetX() const noexcept
{
    return pimpl->sideOffsetX;
}

//--------------------------------------------------------------------------

void DropdownFrame::setSideOffsetY(int val) noexcept
{
    pimpl->sideOffsetY=val;
}

int DropdownFrame::sideOffsetY() const noexcept
{
    return pimpl->sideOffsetY;
}

//--------------------------------------------------------------------------

const QRect& DropdownFrame::fullRect() const noexcept
{
    return pimpl->fullRect;
}

//--------------------------------------------------------------------------

void DropdownFrame::setChainParent(DropdownFrame* parent)
{
    if (pimpl->chainParent.data()==parent)
    {
        return;
    }

    // detach from whatever this frame was previously chained to first, so its Escape shortcut
    // is correctly re-enabled and its chainChild pointer is correctly cleared
    detachFromChainParent();

    // keep the old parent's chainDependents (see DropdownFrame_p) in sync with chainParent below
    if (!pimpl->chainParent.isNull())
    {
        pimpl->chainParent->pimpl->chainDependents.removeAll(this);
    }

    pimpl->chainParent=parent;

    if (parent!=nullptr)
    {
        parent->pimpl->chainDependents.append(this);
    }
}

//--------------------------------------------------------------------------

DropdownFrame* DropdownFrame::chainParent() const noexcept
{
    return pimpl->chainParent.data();
}

//--------------------------------------------------------------------------

DropdownFrame* DropdownFrame::chainChild() const noexcept
{
    return pimpl->chainChild.data();
}

//--------------------------------------------------------------------------

DropdownFrame* DropdownFrame::chainRoot() noexcept
{
    auto* root=this;
    while (root->pimpl->chainParent!=nullptr)
    {
        root=root->pimpl->chainParent.data();
    }
    return root;
}

//--------------------------------------------------------------------------

bool DropdownFrame::chainContains(const QPoint& globalPos) const
{
    if (isVisible() && DropdownFrame_p::globalRect(this).contains(globalPos))
    {
        return true;
    }
    if (!pimpl->chainChild.isNull())
    {
        return pimpl->chainChild->chainContains(globalPos);
    }
    return false;
}

//--------------------------------------------------------------------------

QWidget* DropdownFrame::resolveHost(QWidget* anchor) const
{
    // a chained frame's own anchor typically lives INSIDE its parent frame, which is itself a
    // top-level window whose open/close animation repeatedly calls setGeometry() on itself --
    // tracking that as the host would misread the parent's own animation as the host moving/
    // resizing and self-dismiss this frame immediately. Inherit the parent's already-resolved
    // host instead.
    if (!pimpl->chainParent.isNull())
    {
        return pimpl->chainParent->pimpl->hostWindow.data();
    }
    if (anchor!=nullptr)
    {
        return anchor->window();
    }
    // same fallback popupAt() has always used for the anchor-less case
    return !pimpl->triggerWidget.isNull() ? pimpl->triggerWidget->window() : qobject_cast<QWidget*>(QApplication::activeWindow());
}

//--------------------------------------------------------------------------

void DropdownFrame::detachFromChainParent()
{
    // Deliberately does NOT clear pimpl->chainParent: that link is the STABLE, reusable
    // relationship a submenu is given once at creation (DropdownMenu::ensureSubmenu() calls
    // setChainParent() only the first time a given submenu id is opened, never again on later
    // reopens of the same cached instance) -- only the PARENT's chainChild bookkeeping below is
    // ephemeral, set fresh by every beginOpen() and meant to be undone on every close. Clearing
    // chainParent here too used to sever a cached submenu from its parent after its very first
    // close: beginOpen()'s own `if (!pimpl->chainParent.isNull())` guard would then silently skip
    // re-registering it as the parent's chainChild on every later reopen, leaving the parent's
    // chainContains() unable to recurse into a submenu that was, in fact, still open -- a click
    // genuinely inside the submenu's own rows would misclassify as an outside click and close the
    // whole chain before the row's own handler ever ran. setChainParent() (below) reassigns
    // pimpl->chainParent right after calling this anyway when actually switching parents, and the
    // destructor clears a live child's chainParent explicitly -- neither depends on this doing it
    // too.
    auto* parent=pimpl->chainParent.data();
    if (parent==nullptr)
    {
        return;
    }

    if (parent->pimpl->chainChild.data()!=this)
    {
        // setChainParent() was called (e.g. DropdownMenu::ensureSubmenu() sets it right away,
        // on creation) but this frame was never actually opened as the parent's active child --
        // chainChild is only ever set from beginOpen(). Nothing to undo on the parent's side;
        // in particular, the parent's Escape shortcut was never disabled on this frame's
        // account, so re-enabling it here would be wrong if some OTHER chained frame is
        // currently the parent's actual open child.
        return;
    }
    parent->pimpl->chainChild=nullptr;

    // only re-enable the parent's Escape shortcut if the parent itself actually armed it --
    // otherwise this would incorrectly turn it on for a parent that is closed, or that never
    // wanted self-dismissal in the first place (isSelfDismissEnabled()==false)
    if (parent->pimpl->selfDismissEnabled && parent->isVisible())
    {
        parent->pimpl->escShortcut->setEnabled(true);
    }
}

//--------------------------------------------------------------------------

void DropdownFrame::notifyActivated(QWidget* source)
{
    Q_UNUSED(source)
    // an item activated anywhere in a chain (e.g. a leaf row inside a nested submenu) tears
    // down the whole chain, not just the frame it was clicked in -- a no-op walk for an
    // unchained frame, where chainRoot() is just `this`
    chainRoot()->closeDropdown();
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
    // backstop for any path that hides this frame without going through closeDropdown() (e.g.
    // a direct hide() call from outside) -- a still-open chained child would otherwise be left
    // floating with its parent gone
    if (!pimpl->chainChild.isNull())
    {
        pimpl->chainChild->closeDropdown(true);
    }

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

void DropdownFrame::measureBeside(const QRect& anchorGlobalRect)
{
    QMargins m;
    auto natural=measureContentSize(m);
    QSize full(natural.width()+m.left()+m.right(),natural.height()+m.top()+m.bottom());
    // sideOffsetX/Y are QSS qproperty-driven -- read only AFTER measureContentSize(), whose
    // Style::repolishRecursive(this) call is what applies them on this frame's very first
    // opening (same ordering requirement offsetX/Y have in measure() above)

    // bounded by the anchor rect's own screen, not the host window: the frame is a top-level
    // window free to extend past the host window's own edges, like a native submenu
    auto* screen=QGuiApplication::screenAt(anchorGlobalRect.center());
    if (screen==nullptr)
    {
        screen=!pimpl->triggerWidget.isNull() ? pimpl->triggerWidget->screen() : QGuiApplication::primaryScreen();
    }
    auto avail=screen!=nullptr ? screen->availableGeometry() : anchorGlobalRect;

    // horizontal anchor: to the right of the anchor rect by default, flipped to its left only
    // if the frame would overflow the screen's right edge AND there is genuinely more room on
    // the left than on the right (mirrors measure()'s below-left -> below-right policy)
    int x=anchorGlobalRect.right()+1+pimpl->sideOffsetX;
    bool flippedLeft=false;
    if (x+full.width()>avail.right())
    {
        auto roomRight=avail.right()-x;
        auto leftEdge=anchorGlobalRect.left()-pimpl->sideOffsetX;
        auto roomLeft=leftEdge-avail.left();
        if (roomLeft>roomRight)
        {
            x=leftEdge-full.width();
            flippedLeft=true;
        }
    }
    x=qMax(avail.left(),qMin(x,avail.right()+1-full.width()));

    // vertical anchor: top-aligned with the anchor rect by default (like a native submenu,
    // level with the row that opened it); when it would not fit below that top edge, bottom-
    // align with the anchor rect's bottom edge instead, so the frame runs upward from the row
    int y=anchorGlobalRect.top()+pimpl->sideOffsetY;
    bool bottomAnchored=false;
    auto availableBelow=qMax(1,avail.bottom()-y);

    if (full.height()>availableBelow && pimpl->verticalFlipEnabled)
    {
        auto bottomGlobalY=anchorGlobalRect.bottom()+1-pimpl->sideOffsetY;
        auto availableAbove=qMax(1,bottomGlobalY-avail.top());
        if (availableAbove>availableBelow)
        {
            bottomAnchored=true;
            full.setHeight(qMin(full.height(),availableAbove));
            y=bottomGlobalY-full.height();
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

    // NOTE the inversion relative to measure(): applyFrame() pins the RIGHT edge and grows
    // leftwards for a *RightCorner. A submenu sitting to the RIGHT of its anchor must pin its
    // LEFT edge and grow rightwards, so it is *LeftCorner -- the opposite of measure()'s
    // "rightAnchored means right-edge-aligned" convention. Do not rename flippedLeft to
    // rightAnchored, it would silently invert this.
    auto anchor_=bottomAnchored
        ? (flippedLeft ? Qt::BottomRightCorner : Qt::BottomLeftCorner)
        : (flippedLeft ? Qt::TopRightCorner : Qt::TopLeftCorner);

    pimpl->fullRect=QRect(x,y,full.width(),full.height());
    setAnchorCorner(anchor_);
    setFullSize(full);
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

    if (!pimpl->chainParent.isNull())
    {
        // only one child may be open per parent at a time -- e.g. hovering a different submenu
        // row must close whichever submenu is currently open before this one takes its place
        auto* sibling=pimpl->chainParent->pimpl->chainChild.data();
        if (sibling!=nullptr && sibling!=this)
        {
            sibling->closeDropdown(true);
        }
        pimpl->chainParent->pimpl->chainChild=this;

        // two simultaneously enabled Qt::ApplicationShortcut Escape shortcuts fire
        // ambiguously -- only the innermost open frame in a chain should react to Escape
        pimpl->chainParent->pimpl->escShortcut->setEnabled(false);
    }

    if (pimpl->selfDismissEnabled)
    {
        pimpl->escShortcut->setEnabled(true);
        qApp->installEventFilter(this);
    }
    if (pimpl->restoreFocus && host!=nullptr)
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

    auto* host=resolveHost(anchor);
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
    auto* host=resolveHost(nullptr);
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

void DropdownFrame::popupBesideRect(const QRect& anchorGlobalRect)
{
    auto* host=resolveHost(nullptr);
    if (host==nullptr)
    {
        return;
    }
    trackHost(host);

    // see popupBelow()'s comment on the analogous isVisible() check -- reversing an in-flight
    // close animation must not re-fill/re-measure a still-visible frame
    if (!isVisible())
    {
        fillContent();
        measureBeside(anchorGlobalRect);
    }

    beginOpen(host);
}

//--------------------------------------------------------------------------

void DropdownFrame::popupBeside(QWidget* anchor)
{
    if (anchor==nullptr)
    {
        return;
    }
    popupBesideRect(DropdownFrame_p::globalRect(anchor));
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
    // Cascade and detach unconditionally, before the early-out below: notifyActivated() closes
    // from chainRoot(), so an outer frame's closeDropdown() must still tear down an inner frame
    // even if this outer frame's own visibility already looks "closed" here. This must also run
    // BEFORE this frame disables its own escShortcut just below: cascading first closes
    // chainChild, whose own closeDropdown() detaches IT from this frame (as its parent) and
    // re-enables this frame's escShortcut (see detachFromChainParent()) -- doing that after this
    // frame had already disabled its own shortcut would leave it wrongly re-enabled.
    if (!pimpl->chainChild.isNull())
    {
        pimpl->chainChild->closeDropdown(immediate);
    }
    detachFromChainParent();

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

            if (chainContains(g))
            {
                // inside this frame or one of its open chained descendants (see
                // setChainParent()): let it through, the content itself handles it (and may
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
            if (isActiveWindow())
            {
                // the host is deactivating because THIS popup just took activation, not because
                // the user switched away -- fallback for any platform path where the
                // Qt::WindowDoesNotAcceptFocus flag set in the constructor is not honoured. A
                // genuine switch to some other window leaves isActiveWindow() false here and
                // still closes below.
                break;
            }

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

        case (QEvent::NonClientAreaMouseButtonPress): [[fallthrough]];
        case (QEvent::NonClientAreaMouseButtonDblClick):
        {
            // a press on a window's native frame -- title bar, its minimize/zoom/close buttons,
            // a resize border -- never reaches any widget's mousePressEvent()/this filter's own
            // MouseButtonPress case above, because it is handled entirely outside Qt's widget
            // event delivery. Qt still surfaces it though: QWidgetPrivate::create() turns on
            // frame-strut events for every top-level widget unconditionally
            // (QPlatformWindow::setFrameStrutEventsEnabled(true)), and each platform plugin
            // funnels a frame press into these NonClientArea* events (macOS:
            // QNSWindow::sendEvent + handleFrameStrutMouseEvent; Windows: the WM_NC* handlers in
            // qwindowspointerhandler.cpp). Treat it exactly like an outside click: whichever
            // window the user actually clicked, they did not click inside this dropdown.
            //
            // isVisible() guards against a double emit: QWidgetWindow::handleNonClientAreaMouseEvent
            // forwards the same press to the top-level QWidget via QApplication::forwardEvent(),
            // so a qApp-wide filter observes it twice (once per QObject in the chain) for a
            // single physical click.
            //
            // No obj==hostWindow check, unlike WindowDeactivate/Move/Resize above: this frame is
            // Qt::FramelessWindowHint and never generates a NonClientArea event of its own, so a
            // frame press on any window in the application is unambiguously outside this
            // dropdown. A frame press in a different application is already covered by
            // WindowDeactivate.
            //
            // Known gap: on X11 window decorations are drawn and owned by the window manager, not
            // by the Qt window, so no frame-strut events are generated there and a bare title-bar
            // click is not observed by this filter -- Move/Resize (dragging), WindowDeactivate
            // (switching away) and ApplicationStateChange (hiding the app) below still dismiss
            // the dropdown on Linux.
            if (isVisible())
            {
                emit closeRequested(CloseReason::OutsideClick);
                closeDropdown(true);
            }
        }
        break;

        case (QEvent::Hide): [[fallthrough]];
        case (QEvent::Close):
        {
            // the host window disappeared outright (hidden, or closed without necessarily being
            // destroyed) -- with no host left to anchor to or to reactivate, leaving the frame up
            // would strand it floating on screen with no parent window at all. Same insurance
            // FloatingDialogFrame and FileDropOverlay already carry for their own top-level
            // frames; DropdownFrame never had it.
            if (obj==pimpl->hostWindow.data())
            {
                emit closeRequested(CloseReason::WindowChanged);
                closeDropdown(true);
            }
        }
        break;

        case (QEvent::WindowStateChange):
        {
            // minimizing leaves the host isVisible()==true (no Hide event above) and, since the
            // frame never takes activation, does not necessarily deactivate it either -- check
            // the state explicitly.
            if (obj==pimpl->hostWindow.data() &&
                pimpl->hostWindow->windowState().testFlag(Qt::WindowMinimized))
            {
                emit closeRequested(CloseReason::WindowChanged);
                closeDropdown(true);
            }
        }
        break;

        case (QEvent::ApplicationStateChange):
        {
            // delivered to qApp itself (obj==qApp here, not a per-window event), so this is a
            // safety net for platform paths where the host's own WindowDeactivate/Hide never
            // fires as the application as a whole loses activation -- e.g. macOS Cmd+H / Hide
            // Application, Mission Control, or the app losing focus without any single window
            // reporting it. Safe unconditionally: the frame never accepts activation itself
            // (Qt::WindowDoesNotAcceptFocus), so opening it can never flip the application state
            // and cause a spurious self-close here.
            auto* se=static_cast<QApplicationStateChangeEvent*>(event);
            if (se->applicationState()!=Qt::ApplicationActive)
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
