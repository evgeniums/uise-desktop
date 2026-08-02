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

/** @file uise/desktop/src/fastswitchbutton.cpp
*
*  Defines FastSwitchButton and FastSwitchButtonDropdown.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QVariantAnimation>
#include <QBoxLayout>
#include <QPointer>
#include <QApplication>
#include <QTimer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/fastswitchbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*******************************FastSwitchButtonDropdown***********************/

//--------------------------------------------------------------------------

class FastSwitchButtonDropdown_p
{
    public:

        QPointer<QWidget> content;
        Qt::Corner anchor=Qt::TopLeftCorner;
        QSize fullSize;

        void repositionContent(QWidget* self)
        {
            if (content.isNull())
            {
                return;
            }

            auto m=self->contentsMargins();
            auto cw=content->width();
            auto x=(anchor==Qt::TopLeftCorner) ? m.left() : self->width()-cw-m.right();
            content->move(x,m.top());
        }
};

//--------------------------------------------------------------------------

FastSwitchButtonDropdown::FastSwitchButtonDropdown(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FastSwitchButtonDropdown_p>())
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_NoMousePropagation,true);
    setVisible(false);
}

//--------------------------------------------------------------------------

FastSwitchButtonDropdown::~FastSwitchButtonDropdown()
{}

//--------------------------------------------------------------------------

void FastSwitchButtonDropdown::setContent(QWidget* content)
{
    destroyWidget(pimpl->content);

    pimpl->content=content;
    if (content!=nullptr)
    {
        // QWidget::setParent() makes the widget invisible as a side effect of reparenting,
        // even when the new parent is the SAME as the current one -- and createDropdownContent()
        // is normally passed dropdown() as the constructor parent already (see ensureDropdownContent()),
        // so calling setParent() unconditionally here would redundantly hide/reshow a
        // widget that was already correctly parented, right as its geometry is about to be
        // measured for the very first time
        if (content->parentWidget()!=this)
        {
            content->setParent(this);
        }
        content->setVisible(true);
    }
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButtonDropdown::content() const
{
    return pimpl->content;
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButtonDropdown::takeContent()
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

void FastSwitchButtonDropdown::setAnchorCorner(Qt::Corner corner)
{
    if (pimpl->anchor==corner)
    {
        return;
    }
    pimpl->anchor=corner;
    pimpl->repositionContent(this);
}

//--------------------------------------------------------------------------

Qt::Corner FastSwitchButtonDropdown::anchorCorner() const noexcept
{
    return pimpl->anchor;
}

//--------------------------------------------------------------------------

void FastSwitchButtonDropdown::setFullSize(const QSize& size)
{
    pimpl->fullSize=size;
}

//--------------------------------------------------------------------------

QSize FastSwitchButtonDropdown::fullSize() const noexcept
{
    return pimpl->fullSize;
}

//--------------------------------------------------------------------------

void FastSwitchButtonDropdown::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    pimpl->repositionContent(this);
}

//--------------------------------------------------------------------------

void FastSwitchButtonDropdown::hideEvent(QHideEvent* event)
{
    emit aboutToHide();
    QFrame::hideEvent(event);
}

/*************************************FastSwitchButton*************************/

//--------------------------------------------------------------------------

namespace {

// QWidget::updateGeometry() -- which is what a hidden layout item uses to tell its
// containing layout that its contribution to sizeHint() changed -- does not recompute
// anything synchronously: it posts a QEvent::LayoutRequest, processed on the NEXT event
// loop iteration. So hiding extraClip correctly excludes it from FastSwitchButton's own
// layout eventually, but FastSwitchButton's actual on-screen width (and everything a
// containing navbar/leftFrame lays out relative to it) stays stale for that one deferred
// round-trip, which reads as a brief, visible size glitch. Force every ancestor's layout to
// re-activate synchronously, right now, instead of waiting for that deferred event.
void activateLayoutsUpward(QWidget* widget)
{
    for (auto* w=widget; w!=nullptr; w=w->parentWidget())
    {
        if (w->layout()!=nullptr)
        {
            w->layout()->invalidate();
            w->layout()->activate();
        }
    }
}

}

class FastSwitchButton_p
{
    public:

        IconTextButton* mainButton=nullptr;
        QWidget* extraClip=nullptr;
        QPointer<QWidget> extraWidget;

        QPointer<FastSwitchButtonDropdown> dropdown;
        QPointer<QWidget> dropdownContent;

        QVariantAnimation* extraAnim=nullptr;
        QVariantAnimation* dropAnim=nullptr;
        QShortcut* escShortcut=nullptr;
        SingleShotTimer* hoverTimer=nullptr;

        FastSwitchButton::State state=FastSwitchButton::State::Normal;

        qreal extraT=0.0;
        qreal dropT=0.0;
        bool extraAnimForward=false;
        bool dropAnimForward=false;

        QSize extraFullSize;
        QRect dropFullRect;
        Qt::Corner dropAnchor=Qt::TopLeftCorner;

        bool inTransition=false;

        // QWidget::mousePressEvent ignores unhandled presses by default, which makes Qt
        // redeliver the SAME press event to the parent chain (mainButton -> FastSwitchButton
        // -> ...). Since the click that opens the dropdown runs synchronously inside
        // mainButton's own mousePressEvent, that redelivery reaches this control's eventFilter
        // with state already == Dropdown, and would otherwise be mistaken for a second,
        // separate click on the main button while the dropdown is open (whose job is to
        // close it). This flag suppresses exactly that one propagated redelivery.
        bool suppressNextOwnPressClose=false;

        // QAbstractAnimation::stop() emits finished() when the animation is still running.
        // The animate*() helpers stop a possibly-running animation before restarting it in
        // a new direction; without this guard that stop() would synchronously run the
        // finished handler with the STALE direction flag (e.g. hiding and clearing the
        // dropdown right in the middle of reopening it, and leaving dropT stuck at 1 so
        // the next opening animates 1 -> 1 and never becomes visible).
        bool animStopGuard=false;

        void stopAnimation(QVariantAnimation* a)
        {
            animStopGuard=true;
            a->stop();
            animStopGuard=false;
        }

        QPointer<QWidget> focusBefore;

        int extraSlideDurationMs=FastSwitchButton::DefaultSlideDurationMs;
        int dropdownAnimationDurationMs=FastSwitchButton::DefaultSlideDurationMs;
        int extraEasingCurveType=static_cast<int>(FastSwitchButton::DefaultEasingCurve);
        int dropdownEasingCurveType=static_cast<int>(FastSwitchButton::DefaultEasingCurve);
        int extraSpacing=FastSwitchButton::DefaultExtraSpacing;
        int dropdownOffsetX=FastSwitchButton::DefaultDropdownOffsetX;
        int dropdownOffsetY=FastSwitchButton::DefaultDropdownOffsetY;
        int hoverEnterDelayMs=FastSwitchButton::DefaultHoverEnterDelayMs;
        int hoverLeaveDelayMs=FastSwitchButton::DefaultHoverLeaveDelayMs;

        void applyExtraFrame(qreal t)
        {
            if (extraClip==nullptr)
            {
                return;
            }
            auto w=qRound(t*(extraFullSize.width()+extraSpacing));
            // never let a visible widget sit at exactly zero size: some platforms don't
            // reliably repaint a widget that grows from a literal 0x0 starting geometry
            extraClip->setFixedWidth(qMax(1,w));
        }

        void applyDropFrame(qreal t)
        {
            if (dropdown.isNull())
            {
                return;
            }
            auto w=qMax(1,qRound(t*dropFullRect.width()));
            auto h=qMax(1,qRound(t*dropFullRect.height()));
            auto x=(dropAnchor==Qt::TopLeftCorner) ? dropFullRect.left() : dropFullRect.right()+1-w;
            dropdown->setGeometry(x,dropFullRect.top(),w,h);
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

FastSwitchButton::FastSwitchButton(std::shared_ptr<SvgIcon> icon, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FastSwitchButton_p>())
{
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    auto* l=Layout::horizontal(this);

    pimpl->mainButton=createMainButton();
    l->addWidget(pimpl->mainButton);

    pimpl->extraClip=new QWidget(this);
    pimpl->extraClip->setObjectName("extraClip");
    pimpl->extraClip->setVisible(false);
    l->addWidget(pimpl->extraClip);

    setSvgIcon(std::move(icon));

    pimpl->hoverTimer=new SingleShotTimer(this);

    pimpl->extraAnim=new QVariantAnimation(this);
    connect(
        pimpl->extraAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& val)
        {
            pimpl->extraT=val.toReal();
            pimpl->applyExtraFrame(pimpl->extraT);
        }
    );
    connect(
        pimpl->extraAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->animStopGuard)
            {
                // spurious finished() from an explicit stop() before a restart, not a
                // natural completion -- see FastSwitchButton_p::stopAnimation()
                return;
            }
            finishExtraAnimation(pimpl->extraAnimForward);
        }
    );

    pimpl->dropAnim=new QVariantAnimation(this);
    connect(
        pimpl->dropAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& val)
        {
            pimpl->dropT=val.toReal();
            pimpl->applyDropFrame(pimpl->dropT);
        }
    );
    connect(
        pimpl->dropAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->animStopGuard)
            {
                // spurious finished() from an explicit stop() before a restart, not a
                // natural completion -- see FastSwitchButton_p::stopAnimation()
                return;
            }
            finishDropdownAnimation(pimpl->dropAnimForward);
        }
    );

    pimpl->escShortcut=new QShortcut(Qt::Key_Escape,this);
    pimpl->escShortcut->setContext(Qt::WindowShortcut);
    pimpl->escShortcut->setEnabled(false);
    connect(pimpl->escShortcut,&QShortcut::activated,this,[this](){ returnToNormal(); });
    connect(pimpl->escShortcut,&QShortcut::activatedAmbiguously,this,[this](){ returnToNormal(); });

    connect(
        pimpl->mainButton,
        &IconTextButton::toggled,
        this,
        [this](bool checked)
        {
            if (checked)
            {
                openDropdown();
            }
            else
            {
                closeDropdown();
            }
        }
    );

    qApp->installEventFilter(this);

    // Anchor mainButton (and extraClip) firmly to the left with a trailing stretch item.
    // Without one, QBoxLayout has nothing to absorb transient slack when this widget's own
    // width is briefly stale relative to its children's combined size -- e.g. right after
    // extraClip is hidden, `this` does not shrink synchronously (QWidget::updateGeometry()
    // posts a QEvent::LayoutRequest rather than resizing immediately), so for one frame the
    // layout has more width than mainButton+extraClip need. With no stretch item that slack
    // gets distributed across the Fixed-size items instead of sitting harmlessly at the end,
    // which read as mainButton visibly shifting right for a frame and snapping back once the
    // width settled.
    l->addStretch(1);
}

//--------------------------------------------------------------------------

FastSwitchButton::~FastSwitchButton()
{
    qApp->removeEventFilter(this);
    if (!pimpl->dropdown.isNull())
    {
        destroyWidget(pimpl->dropdown);
    }
}

//--------------------------------------------------------------------------

IconTextButton* FastSwitchButton::mainButton() const
{
    return pimpl->mainButton;
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButton::extraWidget() const
{
    return pimpl->extraWidget;
}

//--------------------------------------------------------------------------

FastSwitchButtonDropdown* FastSwitchButton::dropdown() const
{
    return pimpl->dropdown;
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButton::dropdownContent() const
{
    return pimpl->dropdownContent;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->mainButton->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> FastSwitchButton::svgIcon() const
{
    return pimpl->mainButton->svgIcon();
}

//--------------------------------------------------------------------------

FastSwitchButton::State FastSwitchButton::state() const noexcept
{
    return pimpl->state;
}

//--------------------------------------------------------------------------

bool FastSwitchButton::isDropdownOpen() const noexcept
{
    return pimpl->state==State::Dropdown;
}

//--------------------------------------------------------------------------

bool FastSwitchButton::isExtraWidgetVisible() const noexcept
{
    return pimpl->state!=State::Normal;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setExtraSlideDurationMs(int val) noexcept
{
    pimpl->extraSlideDurationMs=val;
}

int FastSwitchButton::extraSlideDurationMs() const noexcept
{
    return pimpl->extraSlideDurationMs;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setDropdownAnimationDurationMs(int val) noexcept
{
    pimpl->dropdownAnimationDurationMs=val;
}

int FastSwitchButton::dropdownAnimationDurationMs() const noexcept
{
    return pimpl->dropdownAnimationDurationMs;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setExtraEasingCurveType(int val) noexcept
{
    pimpl->extraEasingCurveType=val;
}

int FastSwitchButton::extraEasingCurveType() const noexcept
{
    return pimpl->extraEasingCurveType;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setDropdownEasingCurveType(int val) noexcept
{
    pimpl->dropdownEasingCurveType=val;
}

int FastSwitchButton::dropdownEasingCurveType() const noexcept
{
    return pimpl->dropdownEasingCurveType;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setExtraSpacing(int val) noexcept
{
    pimpl->extraSpacing=val;
}

int FastSwitchButton::extraSpacing() const noexcept
{
    return pimpl->extraSpacing;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setDropdownOffsetX(int val) noexcept
{
    pimpl->dropdownOffsetX=val;
}

int FastSwitchButton::dropdownOffsetX() const noexcept
{
    return pimpl->dropdownOffsetX;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setDropdownOffsetY(int val) noexcept
{
    pimpl->dropdownOffsetY=val;
}

int FastSwitchButton::dropdownOffsetY() const noexcept
{
    return pimpl->dropdownOffsetY;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setHoverEnterDelayMs(int val) noexcept
{
    pimpl->hoverEnterDelayMs=val;
}

int FastSwitchButton::hoverEnterDelayMs() const noexcept
{
    return pimpl->hoverEnterDelayMs;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setHoverLeaveDelayMs(int val) noexcept
{
    pimpl->hoverLeaveDelayMs=val;
}

int FastSwitchButton::hoverLeaveDelayMs() const noexcept
{
    return pimpl->hoverLeaveDelayMs;
}

//--------------------------------------------------------------------------

IconTextButton* FastSwitchButton::createMainButton()
{
    auto* btn=new IconTextButton(this,IconTextButton::IconPosition::BeforeText);
    btn->setObjectName("mainButton");
    btn->setCheckable(true);
    btn->setText(QString());
    return btn;
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButton::createExtraWidget(QWidget* parent)
{
    auto* btn=new IconTextButton(parent,IconTextButton::IconPosition::BeforeText);
    btn->setObjectName("extraWidget");
    return btn;
}

//--------------------------------------------------------------------------

void FastSwitchButton::connectExtraWidget(QWidget* widget)
{
    if (widget==nullptr)
    {
        return;
    }

    if (auto* btn=qobject_cast<IconTextButton*>(widget))
    {
        connect(btn,&IconTextButton::clicked,this,&FastSwitchButton::onExtraWidgetClicked);
        return;
    }
    if (auto* btn=qobject_cast<AvatarButton*>(widget))
    {
        connect(btn,&AvatarButton::clicked,this,&FastSwitchButton::onExtraWidgetClicked);
        return;
    }
    if (widget->metaObject()->indexOfSignal("clicked()")>=0)
    {
        connect(widget,SIGNAL(clicked()),this,SLOT(onExtraWidgetClicked()));
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::fillExtraWidget(QWidget*)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::clearExtraWidget(QWidget*)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::setExtraWidgetHovered(QWidget* widget, bool hovered)
{
    if (widget==nullptr)
    {
        return;
    }

    widget->setProperty("hovered",hovered);
    Style::updateWidgetStyle(widget);
}

//--------------------------------------------------------------------------

QWidget* FastSwitchButton::createDropdownContent(QWidget* parent)
{
    auto* w=new QFrame(parent);
    w->setObjectName("dropdownContent");
    Layout::vertical(w);
    return w;
}

//--------------------------------------------------------------------------

void FastSwitchButton::fillDropdownContent(QWidget*)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::clearDropdownContent(QWidget*)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::onStateChanged(State)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::onActivated(QWidget*)
{}

//--------------------------------------------------------------------------

void FastSwitchButton::notifyActivated(QWidget* source)
{
    onActivated(source);
    emit activated(source);
    returnToNormal();
}

//--------------------------------------------------------------------------

void FastSwitchButton::onExtraWidgetClicked()
{
    emit extraWidgetActivated();
    notifyActivated(pimpl->extraWidget);
}

//--------------------------------------------------------------------------

void FastSwitchButton::showExtraWidget()
{
    if (pimpl->state!=State::Normal)
    {
        return;
    }
    setState(State::Hovered);
    animateExtraWidget(true,false);
}

//--------------------------------------------------------------------------

void FastSwitchButton::hideExtraWidget()
{
    if (pimpl->state!=State::Hovered)
    {
        return;
    }
    setState(State::Normal);
    animateExtraWidget(false,false);
}

//--------------------------------------------------------------------------

void FastSwitchButton::ensureExtraWidget()
{
    if (!pimpl->extraWidget.isNull())
    {
        return;
    }

    pimpl->extraWidget=createExtraWidget(pimpl->extraClip);
    if (pimpl->extraWidget)
    {
        connectExtraWidget(pimpl->extraWidget);
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::ensureDropdown()
{
    auto* host=window();

    if (!pimpl->dropdown.isNull())
    {
        if (pimpl->dropdown->parentWidget()!=host)
        {
            pimpl->dropdown->hide();
            pimpl->dropdown->setParent(host);
        }
        return;
    }

    pimpl->dropdown=new FastSwitchButtonDropdown(host);
    connect(
        pimpl->dropdown,
        &FastSwitchButtonDropdown::aboutToHide,
        this,
        [this]()
        {
            if (pimpl->state==State::Dropdown)
            {
                returnToNormal(true);
            }
        }
    );
}

//--------------------------------------------------------------------------

void FastSwitchButton::ensureDropdownContent()
{
    ensureDropdown();
    if (!pimpl->dropdownContent.isNull())
    {
        return;
    }

    pimpl->dropdownContent=createDropdownContent(pimpl->dropdown);
    if (pimpl->dropdownContent)
    {
        pimpl->dropdown->setContent(pimpl->dropdownContent);
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::measureExtra()
{
    ensureExtraWidget();
    if (pimpl->extraWidget.isNull())
    {
        return;
    }

    fillExtraWidget(pimpl->extraWidget);
    Style::repolishRecursive(pimpl->extraWidget);

    // QStyle::polish() above (what repolishRecursive() calls) does not itself invalidate any
    // nested layout's cached sizeHint, and font resolution (QWidget::ensurePolished()) is a
    // separate mechanism again -- both must run before sizeHint() below, exactly as
    // measureDropdown() already has to (see its comments) for the same reason: a freshly
    // created extra widget with its own internal layout (e.g. an AvatarButton subclass, whose
    // avatar/text spacing comes entirely from a QSS margin on its #text label) can otherwise
    // measure too narrow on the very first fill and never grow to actually show that margin.
    pimpl->extraWidget->ensurePolished();
    const auto extraDescendants=pimpl->extraWidget->findChildren<QWidget*>();
    for (auto* w : extraDescendants)
    {
        w->ensurePolished();
        if (w->layout()!=nullptr)
        {
            w->layout()->invalidate();
        }
    }
    if (pimpl->extraWidget->layout()!=nullptr)
    {
        pimpl->extraWidget->layout()->invalidate();
    }

    pimpl->extraWidget->adjustSize();

    auto hint=pimpl->extraWidget->sizeHint();
    if (!hint.isValid() || hint.isEmpty())
    {
        hint=pimpl->extraWidget->size();
    }
    pimpl->extraFullSize=hint;

    pimpl->extraWidget->setGeometry(pimpl->extraSpacing,0,pimpl->extraFullSize.width(),pimpl->extraFullSize.height());
    if (pimpl->extraWidget->layout()!=nullptr)
    {
        // Lay children out synchronously against the final rect -- the extra widget becomes
        // visible synchronously right after this function returns, so a deferred layout pass
        // (which a plain resize would only schedule) is too late.
        pimpl->extraWidget->layout()->activate();
    }
    pimpl->extraClip->setFixedHeight(pimpl->extraFullSize.height());
}

//--------------------------------------------------------------------------

void FastSwitchButton::measureDropdown()
{
    ensureDropdownContent();
    auto* content=pimpl->dropdownContent.data();
    if (content==nullptr)
    {
        return;
    }

    fillDropdownContent(content);
    // repolish the dropdown frame itself (which recurses into content and all of its
    // descendants too, so a separate call for content is not needed): contentsMargins()
    // (read below, from the QSS "padding" rule) belongs to pimpl->dropdown, a widget
    // freshly inserted into the tree on the very first open, and would otherwise still
    // report its pre-QSS default margins the first time this is measured
    Style::repolishRecursive(pimpl->dropdown);

    // QWidget::ensurePolished() (the QEvent::Polish/font-resolution path) is a separate
    // mechanism from QStyle::polish() invoked by repolishRecursive() above; sizeHint() of
    // labels and buttons depends on resolved fonts, which are only guaranteed after
    // ensurePolished(). Run it over the whole subtree before measuring -- including the
    // dropdown frame itself, whose QSS padding feeds contentsMargins() read below.
    pimpl->dropdown->ensurePolished();
    content->ensurePolished();
    const auto contentDescendants=content->findChildren<QWidget*>();
    for (auto* w : contentDescendants)
    {
        w->ensurePolished();
    }

    // Bust every layout's cached sizeHint in the whole subtree, not just content's own
    // top-level one: a nested widget's own layout (e.g. the rows host just filled by
    // fillDropdownContent()) propagates its invalidation upwards via a posted, asynchronous
    // QEvent::LayoutRequest, so on a synchronous first measurement content->sizeHint() can
    // still be answered from a stale cache. Then lay out and measure in a second pass:
    // pass 1 primes geometry with the initial hint, pass 2 re-measures after the subtree
    // has gone through a real layout cycle -- QSS box-model metrics that only settle once
    // the widgets have been laid out (fresh, never-shown widgets) are then reflected in
    // the final size, matching what a second opening of the dropdown would measure.
    auto invalidateAll=[content,&contentDescendants]()
    {
        if (content->layout()!=nullptr)
        {
            content->layout()->invalidate();
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

        // read the frame's QSS padding fresh on every pass: like the size hints below, the
        // stylesheet-driven contentsMargins() of the freshly created dropdown frame only
        // settle after the subtree has gone through a first real resize/layout cycle, so
        // the pass-1 reading can still be stale on the very first opening
        auto margins=pimpl->dropdown->contentsMargins();

        auto hint=content->sizeHint();
        if (!hint.isValid())
        {
            hint=content->size();
        }

        if (pass>0 && hint==natural && margins==m)
        {
            // second measurement agrees with the first -- geometry is already correct
            break;
        }
        natural=hint;
        m=margins;

        // lay children out synchronously against this size (a plain resize would only post
        // a deferred QEvent::Resize for the next event loop pass, which is too late: the
        // dropdown becomes visible synchronously right after this function returns)
        content->setGeometry(m.left(),m.top(),natural.width(),natural.height());
        if (content->layout()!=nullptr)
        {
            content->layout()->activate();
        }
        for (auto* w : contentDescendants)
        {
            if (w->layout()!=nullptr)
            {
                w->layout()->activate();
            }
        }
    }

    QSize full(natural.width()+m.left()+m.right(),natural.height()+m.top()+m.bottom());

    auto* host=window();
    auto anchorGlobal=mapToGlobal(QPoint(pimpl->dropdownOffsetX,height()+pimpl->dropdownOffsetY));
    auto p=host->mapFromGlobal(anchorGlobal);
    auto avail=host->rect();

    int x=p.x();
    auto anchor=Qt::TopLeftCorner;
    if (x+full.width()>avail.right())
    {
        auto rightGlobalX=mapToGlobal(QPoint(width(),0)).x();
        x=host->mapFromGlobal(QPoint(rightGlobalX,0)).x()-full.width();
        anchor=Qt::TopRightCorner;
    }
    x=qMax(avail.left(),x);

    auto availableHeight=avail.bottom()-p.y();
    if (availableHeight<1)
    {
        availableHeight=1;
    }
    full.setHeight(qMin(full.height(),availableHeight));

    pimpl->dropFullRect=QRect(x,p.y(),full.width(),full.height());
    pimpl->dropAnchor=anchor;

    pimpl->dropdown->setAnchorCorner(anchor);
    pimpl->dropdown->setFullSize(full);
    // content geometry and synchronous child layout were already applied inside the
    // two-pass measurement loop above
}

//--------------------------------------------------------------------------

void FastSwitchButton::setState(State state)
{
    if (pimpl->state==state)
    {
        return;
    }
    pimpl->state=state;
    onStateChanged(state);
    emit stateChanged(state);
}

//--------------------------------------------------------------------------

void FastSwitchButton::animateExtraWidget(bool forward, bool immediate)
{
    if (!forward && pimpl->extraClip!=nullptr && !pimpl->extraClip->isVisible() && qFuzzyIsNull(pimpl->extraT))
    {
        return;
    }

    if (forward)
    {
        // If extraClip is already visible, this call is reversing a hide animation that a
        // quick hover-out/hover-in cycle interrupted mid-flight. Re-running measureExtra()
        // in that state would refill and re-measure the extra widget while it is still on
        // screen and mid-transition -- the same class of "stuck wrong size" race fixed for
        // the dropdown in openDropdown() -- so skip it and just reverse the animation using
        // the existing, already-settled extraFullSize instead.
        const auto alreadyVisible=pimpl->extraClip!=nullptr && pimpl->extraClip->isVisible();
        if (!alreadyVisible)
        {
            measureExtra();
        }
        // pin the clip to its current (start-of-animation) width before it becomes visible,
        // so the first layout pass after setVisible(true) never sees a stale/default width
        pimpl->applyExtraFrame(pimpl->extraT);
        pimpl->extraClip->setVisible(true);
        setExtraWidgetHovered(pimpl->extraWidget,true);
    }

    const qreal target=forward ? 1.0 : 0.0;

    if (immediate)
    {
        pimpl->stopAnimation(pimpl->extraAnim);
        pimpl->extraT=target;
        pimpl->applyExtraFrame(target);
        finishExtraAnimation(forward);
        return;
    }

    pimpl->stopAnimation(pimpl->extraAnim);
    pimpl->extraAnim->setDuration(pimpl->extraSlideDurationMs);
    pimpl->extraAnim->setEasingCurve(static_cast<QEasingCurve::Type>(pimpl->extraEasingCurveType));
    pimpl->extraAnim->setStartValue(pimpl->extraT);
    pimpl->extraAnim->setEndValue(target);
    pimpl->extraAnimForward=forward;
    pimpl->extraAnim->start();
}

//--------------------------------------------------------------------------

void FastSwitchButton::finishExtraAnimation(bool forward)
{
    if (forward)
    {
        return;
    }

    if (pimpl->extraClip!=nullptr)
    {
        pimpl->extraClip->setVisible(false);
        // force the containing navbar/layout chain to shrink around the now-hidden clip
        // right now, instead of waiting for a deferred QEvent::LayoutRequest -- see
        // activateLayoutsUpward(). Start from `this`, not parentWidget(): this control's
        // own layout cache must be busted first, so the parent's activate() below picks up
        // the fresh (post-hide) sizeHint rather than a stale, still-wide cached one.
        activateLayoutsUpward(this);
    }
    if (!pimpl->extraWidget.isNull())
    {
        setExtraWidgetHovered(pimpl->extraWidget,false);
        clearExtraWidget(pimpl->extraWidget);
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::animateDropdownFrame(bool forward, bool immediate)
{
    if (!forward && (pimpl->dropdown.isNull() || !pimpl->dropdown->isVisible()) && qFuzzyIsNull(pimpl->dropT))
    {
        return;
    }

    if (forward)
    {
        pimpl->dropdown->raise();
        if (!pimpl->dropdown->isVisible())
        {
            // only reset to the tiny starting geometry for a genuinely fresh open. If the
            // dropdown is already visible, this call is reversing a close animation that a
            // quick toggle interrupted mid-flight -- snapping its geometry back down to
            // (1,1) here would throw away that in-flight size and jump-cut it small before
            // growing again, instead of smoothly reversing from wherever it currently is
            // (which setStartValue(pimpl->dropT) below already does correctly)
            pimpl->dropdown->setGeometry(pimpl->dropFullRect.x(),pimpl->dropFullRect.y(),1,1);
            pimpl->dropdown->show();
        }
        pimpl->dropdown->raise();
    }

    const qreal target=forward ? 1.0 : 0.0;

    if (immediate)
    {
        pimpl->stopAnimation(pimpl->dropAnim);
        pimpl->dropT=target;
        pimpl->applyDropFrame(target);
        finishDropdownAnimation(forward);
        return;
    }

    pimpl->stopAnimation(pimpl->dropAnim);
    pimpl->dropAnim->setDuration(pimpl->dropdownAnimationDurationMs);
    pimpl->dropAnim->setEasingCurve(static_cast<QEasingCurve::Type>(pimpl->dropdownEasingCurveType));
    pimpl->dropAnim->setStartValue(pimpl->dropT);
    pimpl->dropAnim->setEndValue(target);
    pimpl->dropAnimForward=forward;
    pimpl->dropAnim->start();
}

//--------------------------------------------------------------------------

void FastSwitchButton::finishDropdownAnimation(bool forward)
{
    if (forward)
    {
        emit dropdownShown();
        return;
    }

    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->hide();
    }
    if (!pimpl->dropdownContent.isNull())
    {
        clearDropdownContent(pimpl->dropdownContent);
    }
    emit dropdownHidden();
}

//--------------------------------------------------------------------------

void FastSwitchButton::openDropdown()
{
    if (pimpl->state==State::Dropdown)
    {
        return;
    }

    pimpl->hoverTimer->clear();

    if (pimpl->state==State::Normal)
    {
        // force the extra widget visible synchronously, since the drop-down keeps it
        // shown for as long as it is open regardless of actual mouse hover
        animateExtraWidget(true,true);
    }

    setState(State::Dropdown);

    // arm the redelivery guard (see suppressNextOwnPressClose) and disarm it once the
    // synchronous propagation of the opening click has fully finished
    pimpl->suppressNextOwnPressClose=true;
    QTimer::singleShot(0,this,[this](){ pimpl->suppressNextOwnPressClose=false; });

    // If the dropdown is still visible here, this open is reversing a close animation that
    // a previous, very quick toggle interrupted mid-flight (see the animStopGuard comment).
    // Re-running measureDropdown() in that state would tear down and rebuild the content
    // (fillDropdownContent() destroys and recreates rows) while it is still on screen and
    // mid-transition, which is exactly what leaves the popup with a wrong, "stuck" size for
    // several activations afterwards -- content and dropFullRect from the previous, already
    // fully-settled measurement are still valid, so just keep them and let the animation
    // reverse back towards open instead of measuring again.
    const auto reopeningWhileStillVisible=!pimpl->dropdown.isNull() && pimpl->dropdown->isVisible();
    if (!reopeningWhileStillVisible)
    {
        measureDropdown();
    }

    emit dropdownAboutToShow();

    if (!pimpl->mainButton->isChecked())
    {
        QSignalBlocker b(pimpl->mainButton);
        pimpl->mainButton->setChecked(true);
    }

    pimpl->escShortcut->setEnabled(true);
    pimpl->focusBefore=window()->focusWidget();

    animateDropdownFrame(true,false);
}

//--------------------------------------------------------------------------

void FastSwitchButton::closeDropdown()
{
    if (pimpl->state!=State::Dropdown)
    {
        return;
    }

    if (pimpl->mainButton->isChecked())
    {
        QSignalBlocker b(pimpl->mainButton);
        pimpl->mainButton->setChecked(false);
    }

    pimpl->escShortcut->setEnabled(false);

    if (pimpl->focusBefore)
    {
        pimpl->focusBefore->setFocus();
    }
    pimpl->focusBefore=nullptr;

    auto stayHovered=underMouse();
    setState(stayHovered ? State::Hovered : State::Normal);

    animateDropdownFrame(false,false);
    if (!stayHovered)
    {
        animateExtraWidget(false,false);
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::returnToNormal(bool immediate)
{
    if (pimpl->inTransition)
    {
        return;
    }
    pimpl->inTransition=true;

    pimpl->hoverTimer->clear();

    if (pimpl->mainButton->isChecked())
    {
        QSignalBlocker b(pimpl->mainButton);
        pimpl->mainButton->setChecked(false);
    }

    pimpl->escShortcut->setEnabled(false);

    if (pimpl->focusBefore)
    {
        pimpl->focusBefore->setFocus();
    }
    pimpl->focusBefore=nullptr;

    auto dropdownWasVisible=!pimpl->dropdown.isNull() && pimpl->dropdown->isVisible();

    setState(State::Normal);

    if (dropdownWasVisible)
    {
        animateDropdownFrame(false,immediate);
    }
    animateExtraWidget(false,immediate);

    pimpl->inTransition=false;
}

//--------------------------------------------------------------------------

void FastSwitchButton::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);

    if (pimpl->state==State::Dropdown)
    {
        return;
    }

    pimpl->hoverTimer->clear();
    if (pimpl->hoverEnterDelayMs<=0)
    {
        showExtraWidget();
    }
    else
    {
        pimpl->hoverTimer->shot(static_cast<size_t>(pimpl->hoverEnterDelayMs),[this](){ showExtraWidget(); },true);
    }
}

//--------------------------------------------------------------------------

void FastSwitchButton::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);

    if (pimpl->state==State::Dropdown)
    {
        // the extra widget keeps showing while the drop-down is open, even if the
        // mouse leaves the control
        return;
    }

    pimpl->hoverTimer->clear();
    if (pimpl->hoverLeaveDelayMs<=0)
    {
        hideExtraWidget();
    }
    else
    {
        pimpl->hoverTimer->shot(static_cast<size_t>(pimpl->hoverLeaveDelayMs),[this](){ hideExtraWidget(); },true);
    }
}

//--------------------------------------------------------------------------

bool FastSwitchButton::eventFilter(QObject* obj, QEvent* event)
{
    if (pimpl->state!=State::Dropdown)
    {
        return QFrame::eventFilter(obj,event);
    }

    switch (event->type())
    {
        case (QEvent::MouseButtonPress):
        {
            if (pimpl->suppressNextOwnPressClose)
            {
                // this is a propagated redelivery of the very click that just opened the
                // dropdown (see suppressNextOwnPressClose) -- an unhandled press is ignored
                // by QWidget::mousePressEvent by default, so Qt keeps redelivering the SAME
                // event to each ancestor up the parent chain until something accepts it or
                // it reaches the top-level widget, re-invoking this filter once per ancestor.
                // Only the deferred reset armed in openDropdown() clears this flag, so every
                // one of those redeliveries is suppressed, not just the first.
                break;
            }

            auto* me=static_cast<QMouseEvent*>(event);
            auto g=me->globalPosition().toPoint();

            if (!pimpl->dropdown.isNull() && pimpl->dropdown->isVisible()
                && FastSwitchButton_p::globalRect(pimpl->dropdown).contains(g))
            {
                // inside the dropdown: let it through, the item itself will call
                // notifyActivated()
                break;
            }

            if (FastSwitchButton_p::globalRect(pimpl->mainButton).contains(g))
            {
                // consume the press so IconTextButton::mousePressEvent never runs and
                // cannot re-toggle the button back open
                returnToNormal();
                return true;
            }

            // genuine outside click: close, but pass the press through so it can still
            // activate whatever else it landed on
            returnToNormal();
            break;
        }

        case (QEvent::WindowDeactivate):
        {
            if (obj==window())
            {
                returnToNormal();
            }
        }
        break;

        case (QEvent::Move): [[fallthrough]];
        case (QEvent::Resize):
        {
            if (obj==window())
            {
                returnToNormal(true);
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
