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
#include <QSignalBlocker>
#include <QVariantAnimation>
#include <QBoxLayout>
#include <QPointer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/fastswitchbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/*************************************FastSwitchButton*************************/

//--------------------------------------------------------------------------

class FastSwitchButton_p
{
    public:

        IconTextButton* mainButton=nullptr;
        QWidget* extraClip=nullptr;
        QPointer<QWidget> extraWidget;

        QPointer<FastSwitchButtonDropdown> dropdown;
        QPointer<QWidget> dropdownContent;

        QVariantAnimation* extraAnim=nullptr;
        SingleShotTimer* hoverTimer=nullptr;

        FastSwitchButton::State state=FastSwitchButton::State::Normal;

        qreal extraT=0.0;
        bool extraAnimForward=false;

        QSize extraFullSize;

        bool inTransition=false;

        // QAbstractAnimation::stop() emits finished() when the animation is still running.
        // animateExtraWidget() stops a possibly-running animation before restarting it in a
        // new direction; without this guard that stop() would synchronously run the finished
        // handler with the STALE direction flag.
        bool animStopGuard=false;

        void stopAnimation(QVariantAnimation* a)
        {
            animStopGuard=true;
            a->stop();
            animStopGuard=false;
        }

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
    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->setAnimationDurationMs(val);
    }
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
    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->setEasingCurveType(val);
    }
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
    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->setOffsetX(val);
    }
}

int FastSwitchButton::dropdownOffsetX() const noexcept
{
    return pimpl->dropdownOffsetX;
}

//--------------------------------------------------------------------------

void FastSwitchButton::setDropdownOffsetY(int val) noexcept
{
    pimpl->dropdownOffsetY=val;
    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->setOffsetY(val);
    }
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

void FastSwitchButton::onDropdownSelfClosed()
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

    auto stayHovered=underMouse();
    setState(stayHovered ? State::Hovered : State::Normal);

    // do NOT call dropdown()->closeDropdown() here: DropdownFrame's own eventFilter/
    // escShortcut already initiated (or, for immediate window-move/resize closes, already
    // finished) the close before emitting closeRequested(); re-entering it here would
    // double-fire the close animation
    if (!stayHovered)
    {
        animateExtraWidget(false,false);
    }
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
    if (!pimpl->dropdown.isNull())
    {
        return;
    }

    pimpl->dropdown=new FastSwitchButtonDropdown();
    pimpl->dropdown->setAnimationDurationMs(pimpl->dropdownAnimationDurationMs);
    pimpl->dropdown->setEasingCurveType(pimpl->dropdownEasingCurveType);
    pimpl->dropdown->setOffsetX(pimpl->dropdownOffsetX);
    pimpl->dropdown->setOffsetY(pimpl->dropdownOffsetY);
    pimpl->dropdown->setTriggerWidget(pimpl->mainButton);

    connect(
        pimpl->dropdown,
        &DropdownFrame::aboutToShow,
        this,
        &FastSwitchButton::dropdownAboutToShow
    );
    connect(
        pimpl->dropdown,
        &DropdownFrame::shown,
        this,
        &FastSwitchButton::dropdownShown
    );
    connect(
        pimpl->dropdown,
        &DropdownFrame::hidden,
        this,
        [this]()
        {
            if (!pimpl->dropdownContent.isNull())
            {
                clearDropdownContent(pimpl->dropdownContent);
            }
            emit dropdownHidden();
        }
    );
    connect(
        pimpl->dropdown,
        &DropdownFrame::closeRequested,
        this,
        &FastSwitchButton::onDropdownSelfClosed
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
    // DropdownFrame's own measurement already has to (see its comments) for the same reason:
    // a freshly created extra widget with its own internal layout (e.g. an AvatarButton
    // subclass, whose avatar/text spacing comes entirely from a QSS margin on its #text
    // label) can otherwise measure too narrow on the very first fill and never grow to
    // actually show that margin.
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
        // the dropdown content in FastSwitchButton::openDropdown() -- so skip it and just
        // reverse the animation using the existing, already-settled extraFullSize instead.
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
        // Layout::activateUpward(). Start from `this`, not parentWidget(): this control's
        // own layout cache must be busted first, so the parent's activate() below picks up
        // the fresh (post-hide) sizeHint rather than a stale, still-wide cached one.
        Layout::activateUpward(this);
    }
    if (!pimpl->extraWidget.isNull())
    {
        setExtraWidgetHovered(pimpl->extraWidget,false);
        clearExtraWidget(pimpl->extraWidget);
    }
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

    ensureDropdownContent();

    // If the dropdown is still visible here, this open is reversing a close animation that a
    // previous, very quick toggle interrupted mid-flight (see DropdownFrame's animStopGuard
    // comment). Re-filling content in that state would tear down and rebuild rows
    // (fillDropdownContent() destroys and recreates rows) while it is still on screen and
    // mid-transition, which is exactly what leaves the popup with a wrong, "stuck" size for
    // several activations afterwards -- content from the previous, already fully-settled fill
    // is still valid, so just keep it; DropdownFrame::popupBelow() applies the identical
    // isVisible()-gated skip internally for its own measurement.
    if (!pimpl->dropdown->isVisible())
    {
        fillDropdownContent(pimpl->dropdownContent);
    }

    if (!pimpl->mainButton->isChecked())
    {
        QSignalBlocker b(pimpl->mainButton);
        pimpl->mainButton->setChecked(true);
    }

    pimpl->dropdown->popupBelow(pimpl->mainButton);
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

    auto stayHovered=underMouse();
    setState(stayHovered ? State::Hovered : State::Normal);

    if (!pimpl->dropdown.isNull())
    {
        pimpl->dropdown->closeDropdown();
    }
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

    auto dropdownWasVisible=!pimpl->dropdown.isNull() && pimpl->dropdown->isVisible();

    setState(State::Normal);

    if (dropdownWasVisible)
    {
        pimpl->dropdown->closeDropdown(immediate);
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

UISE_DESKTOP_NAMESPACE_END
