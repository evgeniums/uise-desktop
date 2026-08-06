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

/** @file uise/desktop/abstractcheckbox.cpp
*
*  Defines AbstractCheckBox.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QStyleOption>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/abstractcheckbox.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Strip QAbstractButton's '&' mnemonic marker for display in a plain (non-mnemonic) QLabel:
//! a lone '&' is the marker and is dropped, "&&" is a literal '&' and collapses to one.
QString stripMnemonic(const QString& text)
{
    QString result;
    result.reserve(text.size());
    for (int i=0;i<text.size();++i)
    {
        auto ch=text.at(i);
        if (ch==QLatin1Char('&'))
        {
            if (i+1<text.size() && text.at(i+1)==QLatin1Char('&'))
            {
                result.append(ch);
                ++i;
                continue;
            }
            continue;
        }
        result.append(ch);
    }
    return result;
}

} // anonymous namespace

//==========================================================================

class AbstractCheckBox_p
{
    public:

        bool animationEnabled=AbstractCheckBox::DefaultCheckAnimationEnabled;
        int durationMs=AbstractCheckBox::DefaultCheckAnimationDurationMs;
        QEasingCurve::Type easingType=AbstractCheckBox::DefaultCheckAnimationEasingCurve;

        Qt::CursorShape cursorShape=AbstractCheckBox::DefaultCursorShape;
        Qt::CursorShape disabledCursorShape=AbstractCheckBox::DefaultDisabledCursorShape;

        AbstractCheckBox::IndicatorMode indicatorMode=AbstractCheckBox::DefaultIndicatorMode;
        AbstractCheckBox::IndicatorShape indicatorShape=AbstractCheckBox::DefaultIndicatorShape;
        AbstractCheckBox::TextPosition textPosition=AbstractCheckBox::DefaultTextPosition;

        bool hovered=false;
        bool parentHovered=false;
        bool structureUpdateScheduled=false;
        //! Set by mousePressEvent() just before QAbstractButton::mousePressEvent() -- read
        //! and cleared by the pressed() handler, see there.
        bool suppressNextRipple=false;

        std::shared_ptr<SvgIcon> icon;

        QHBoxLayout* boxLayout=nullptr;

        QWidget* indicator=nullptr;
        QFrame* off=nullptr;
        QFrame* on=nullptr;
        QFrame* mark=nullptr;
        RoundedImage* markIconWidget=nullptr;
        QLabel* text=nullptr;

        RippleOverlay* ripple=nullptr;

        QGraphicsOpacityEffect* onEffect=nullptr;
        QPropertyAnimation* checkAnim=nullptr;
};

//==========================================================================

AbstractCheckBox::AbstractCheckBox(QWidget* parent)
    : QAbstractButton(parent),
      pimpl(std::make_unique<AbstractCheckBox_p>())
{
    setCheckable(true);

    // QAbstractButtonPrivate::init() derives the focus policy from SH_Button_FocusPolicy,
    // which differs per platform style -- pin it so Space activation and ":focus" styling
    // behave identically everywhere.
    setFocusPolicy(Qt::StrongFocus);
    // QStyleSheetStyle::polish() only sets WA_Hover when a matched selector happens to use
    // ":hover" -- setting it here keeps ":hover" on uise--CheckBox working even for a
    // consuming stylesheet that has no :hover rule of its own but does key on [hovered="true"].
    setAttribute(Qt::WA_Hover,true);
    // Matches QAbstractButtonPrivate::init()'s own default (QSizePolicy::Minimum horizontally,
    // Fixed vertically) rather than (Fixed,Fixed) -- QCheckBox/QRadioButton never override
    // this, so it is the real native baseline this widget replaces. Horizontally Fixed (the
    // original choice here) stops the widget from ever growing past its own sizeHint, which
    // in a layout that gives the row more width than the checkbox needs (e.g. an
    // EditablePanelGrid column, sized by the panel's widest row) leaves nothing to anchor the
    // row's own alignment to -- the whole panel shrinks to the checkbox's own narrow width and
    // then gets centred as a block by whatever outer layout places the panel, instead of the
    // checkbox filling its row and staying left-anchored the way a native QCheckBox does.
    // #indicator/#text are packed from pimpl->boxLayout's left edge with no trailing stretch,
    // so any extra width Minimum now lets this widget claim just becomes harmless blank space
    // after the (still left-anchored) text -- exactly how native left-aligns too.
    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);

    pimpl->boxLayout=Layout::horizontal(this);

    pimpl->indicator=new QFrame(this);
    pimpl->indicator->setObjectName("indicator");
    auto indicatorLayout=Layout::grid(pimpl->indicator);

    // Both layers live in the SAME grid cell with Qt::AlignCenter: they overlap exactly, each
    // keeps its own QSS-driven size, and whatever #indicator has left over becomes the halo
    // band around them. A QStackedLayout(StackAll) would force-size every page to the
    // container's full rect (so the box could not be sized independently of the halo), and it
    // raises/lowers pages on insert and on setCurrentIndex -- which would silently move a
    // layer above the RippleOverlay installed below. QGridLayout never touches z-order.
    pimpl->off=new QFrame(pimpl->indicator);
    pimpl->off->setObjectName("indicatorOff");
    indicatorLayout->addWidget(pimpl->off,0,0,Qt::AlignCenter);

    pimpl->on=new QFrame(pimpl->indicator);
    pimpl->on->setObjectName("indicatorOn");
    indicatorLayout->addWidget(pimpl->on,0,0,Qt::AlignCenter);

    auto onLayout=Layout::horizontal(pimpl->on);
    onLayout->setAlignment(Qt::AlignCenter);

    pimpl->markIconWidget=new RoundedImage(pimpl->on);
    pimpl->markIconWidget->setObjectName("markIcon");
    pimpl->markIconWidget->setDisableHover(true); // hover is driven from this widget, see applyPartState()
    onLayout->addWidget(pimpl->markIconWidget,0,Qt::AlignCenter);

    pimpl->mark=new QFrame(pimpl->on);
    pimpl->mark->setObjectName("mark");
    onLayout->addWidget(pimpl->mark,0,Qt::AlignCenter);

    pimpl->text=new QLabel(this);
    pimpl->text->setObjectName("text");
    // A QLabel with the default LinksAccessibleByMouse forwards presses to its
    // QWidgetTextControl, which can accept them; and a text-interactive QLabel installs its
    // own I-beam cursor, punching a hole in the pointing hand set on this widget. Neither is
    // wanted: the label is pure decoration, all input belongs to the button.
    pimpl->text->setTextInteractionFlags(Qt::NoTextInteraction);
    pimpl->text->setVisible(false);

    // Every part is input-transparent so a click anywhere -- box, halo band or text -- is
    // delivered straight to this widget's hitButton()/mousePressEvent(). The trade-off is
    // that the parts never see Enter/Leave, so their hover styling has to key on the
    // [hovered] dynamic property rather than ":hover" -- see applyPartState() and checkbox.qss.
    for (auto* w : {static_cast<QWidget*>(pimpl->indicator),static_cast<QWidget*>(pimpl->off),
                    static_cast<QWidget*>(pimpl->on),static_cast<QWidget*>(pimpl->mark),
                    static_cast<QWidget*>(pimpl->markIconWidget),static_cast<QWidget*>(pimpl->text)})
    {
        w->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    }

    pimpl->onEffect=new QGraphicsOpacityEffect(pimpl->on);
    pimpl->onEffect->setOpacity(0.0);
    pimpl->on->setGraphicsEffect(pimpl->onEffect);

    pimpl->checkAnim=new QPropertyAnimation(pimpl->onEffect,"opacity",this);

    applyTextPosition();
    applyIndicatorMode();

    // #indicatorOn must paint over #indicatorOff (children already paint in creation order,
    // so it already does -- this makes the dependency explicit and survives a future
    // reorder), and the ripple must be installed LAST so it ends up on top of both, see
    // RippleOverlay::install().
    pimpl->on->raise();
    pimpl->ripple=RippleOverlay::install(pimpl->indicator);
    // The overlay's own mouse filtering would never fire anyway (#indicator is
    // WA_TransparentForMouseEvents), and it must not: the ripple has to follow the BUTTON's
    // press/release, which is also what a Space-key activation goes through
    // (QAbstractButton::keyPressEvent emits pressed(), and the matching release runs through
    // QAbstractButtonPrivate::click(), which emits released()).
    pimpl->ripple->setAutoTrigger(false);

    connect(this,&QAbstractButton::pressed,this,
        [this]()
        {
            // suppressNextRipple is set by mousePressEvent() just before this signal fires
            // synchronously from inside QAbstractButton::mousePressEvent() -- a mouse press
            // outside the indicator (i.e. on the text label) leaves it true, so the ripple
            // only ever shows for a press that actually lands on the indicator. A press
            // never routed through mousePressEvent at all -- Space/Return activation, which
            // emits pressed() straight from QAbstractButton::keyPressEvent() -- leaves the
            // flag at its default false, so keyboard activation still ripples, matching a
            // press on the indicator.
            if (pimpl->ripple!=nullptr && !pimpl->suppressNextRipple)
            {
                pimpl->ripple->start(pimpl->ripple->rect().center());
            }
            pimpl->suppressNextRipple=false;
        }
    );
    connect(this,&QAbstractButton::released,this,&AbstractCheckBox::endRipple);
    // toggled(), NOT the checkStateSet() virtual: QAbstractButton::setChecked() only calls
    // checkStateSet() when !blockRefresh, and QAbstractButtonPrivate::click() sets
    // blockRefresh around its nextCheckState() call -- so checkStateSet() is silently skipped
    // for every user-driven toggle. toggled() is emitted on every path, including a
    // QButtonGroup unchecking a sibling.
    connect(this,&QAbstractButton::toggled,this,
        [this](bool)
        {
            updateCheckedState(true);
            applyPartState();
        }
    );

    applyCursor();
    applyPartState();
    updateCheckedState(false);
}

//--------------------------------------------------------------------------

AbstractCheckBox::~AbstractCheckBox()
{
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setText(const QString& text)
{
    QAbstractButton::setText(text);
    pimpl->text->setText(stripMnemonic(text));
    pimpl->text->setVisible(!text.isEmpty());
    // uise--AbstractCheckBox[textEmpty="true"] #text (checkbox.qss) is a DESCENDANT selector
    // -- #text is the widget the rule actually paints, so #text is what needs repolishing,
    // not `this`. Repolishing `this` here would also re-invoke every qproperty-* writer
    // checkbox.qss declares on `this` (cursorShape, indicatorMode, textPosition, ...),
    // silently reverting any value a caller had set in C++ back to the stylesheet default --
    // see applyPartState()'s comment for the same hazard.
    setProperty("textEmpty",text.isEmpty());
    Style::updateWidgetStyle(pimpl->text);
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::text() const
{
    return QAbstractButton::text();
}

//--------------------------------------------------------------------------

QLabel* AbstractCheckBox::textWidget() const noexcept
{
    return pimpl->text;
}

//--------------------------------------------------------------------------

QWidget* AbstractCheckBox::indicatorWidget() const noexcept
{
    return pimpl->indicator;
}

//--------------------------------------------------------------------------

QWidget* AbstractCheckBox::indicatorOffWidget() const noexcept
{
    return pimpl->off;
}

//--------------------------------------------------------------------------

QWidget* AbstractCheckBox::indicatorOnWidget() const noexcept
{
    return pimpl->on;
}

//--------------------------------------------------------------------------

RippleOverlay* AbstractCheckBox::rippleOverlay() const noexcept
{
    return pimpl->ripple;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setMarkIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->icon=std::move(icon);
    pimpl->markIconWidget->setSvgIcon(pimpl->icon);
    if (pimpl->indicatorMode==IndicatorMode::Auto)
    {
        scheduleStructureUpdate();
    }
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> AbstractCheckBox::markIcon() const
{
    return pimpl->icon;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCheckAnimationEnabled(bool enable) noexcept
{
    pimpl->animationEnabled=enable;
}

//--------------------------------------------------------------------------

bool AbstractCheckBox::isCheckAnimationEnabled() const noexcept
{
    return pimpl->animationEnabled;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCheckAnimationDurationMs(int ms) noexcept
{
    pimpl->durationMs=ms;
}

//--------------------------------------------------------------------------

int AbstractCheckBox::checkAnimationDurationMs() const noexcept
{
    return pimpl->durationMs;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCheckAnimationEasingCurveType(int type)
{
    pimpl->easingType=static_cast<QEasingCurve::Type>(type);
}

//--------------------------------------------------------------------------

int AbstractCheckBox::checkAnimationEasingCurveType() const noexcept
{
    return static_cast<int>(pimpl->easingType);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCursorShape(Qt::CursorShape shape)
{
    pimpl->cursorShape=shape;
    applyCursor();
}

//--------------------------------------------------------------------------

Qt::CursorShape AbstractCheckBox::cursorShape() const noexcept
{
    return pimpl->cursorShape;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCursorShapeName(const QString& name)
{
    setCursorShape(isDefaultStyleToken(name) ? DefaultCursorShape : cursorShapeFromString(name));
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::cursorShapeName() const
{
    return cursorShapeToString(pimpl->cursorShape);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setDisabledCursorShape(Qt::CursorShape shape)
{
    pimpl->disabledCursorShape=shape;
    applyCursor();
}

//--------------------------------------------------------------------------

Qt::CursorShape AbstractCheckBox::disabledCursorShape() const noexcept
{
    return pimpl->disabledCursorShape;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setDisabledCursorShapeName(const QString& name)
{
    setDisabledCursorShape(isDefaultStyleToken(name) ? DefaultDisabledCursorShape : cursorShapeFromString(name));
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::disabledCursorShapeName() const
{
    return cursorShapeToString(pimpl->disabledCursorShape);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setIndicatorMode(IndicatorMode mode)
{
    // checkbox.qss sets qproperty-indicatorMode unconditionally on uise--AbstractCheckBox, so
    // EVERY repolish of this widget re-invokes this writer with the current value -- without
    // this guard, scheduleStructureUpdate() -> doUpdateStructure() -> applyPartState() ->
    // Style::updateWidgetStyle(this) triggers another polish, which calls this writer again,
    // forever. Same hazard and fix as Calendar::setPropertyRepolish()'s "early-return if
    // unchanged" -- only an actual change is allowed to cause a repolish.
    if (pimpl->indicatorMode==mode)
    {
        return;
    }
    pimpl->indicatorMode=mode;
    scheduleStructureUpdate();
}

//--------------------------------------------------------------------------

AbstractCheckBox::IndicatorMode AbstractCheckBox::indicatorMode() const noexcept
{
    return pimpl->indicatorMode;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setIndicatorModeName(const QString& name)
{
    if (name.compare(QStringLiteral("svg"),Qt::CaseInsensitive)==0)
    {
        setIndicatorMode(IndicatorMode::Svg);
    }
    else if (name.compare(QStringLiteral("qss"),Qt::CaseInsensitive)==0)
    {
        setIndicatorMode(IndicatorMode::Qss);
    }
    else
    {
        setIndicatorMode(IndicatorMode::Auto);
    }
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::indicatorModeName() const
{
    switch (pimpl->indicatorMode)
    {
        case (IndicatorMode::Svg): return QStringLiteral("svg");
        case (IndicatorMode::Qss): return QStringLiteral("qss");
        case (IndicatorMode::Auto): break;
    }
    return QStringLiteral("auto");
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setIndicatorShape(IndicatorShape shape)
{
    // Same recursion hazard as setIndicatorMode() above -- checkbox.qss sets
    // qproperty-indicatorShape unconditionally (uise--CheckBox/uise--RadioBox), so an
    // unguarded write here would repolish `this` from applyPartState() on every single
    // polish pass, which is exactly what triggers this writer -- infinite recursion.
    if (pimpl->indicatorShape==shape)
    {
        return;
    }
    pimpl->indicatorShape=shape;
    applyPartState();
}

//--------------------------------------------------------------------------

AbstractCheckBox::IndicatorShape AbstractCheckBox::indicatorShape() const noexcept
{
    return pimpl->indicatorShape;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setIndicatorShapeName(const QString& name)
{
    setIndicatorShape(name.compare(QStringLiteral("circle"),Qt::CaseInsensitive)==0 ? IndicatorShape::Circle : IndicatorShape::Box);
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::indicatorShapeName() const
{
    return pimpl->indicatorShape==IndicatorShape::Circle ? QStringLiteral("circle") : QStringLiteral("box");
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setTextPosition(TextPosition position)
{
    // Same recursion hazard as setIndicatorMode() above -- checkbox.qss sets
    // qproperty-textPosition unconditionally, and applyTextPosition() itself repolishes
    // `this` (needed for the [textPosition="before"] #text margin swap).
    if (pimpl->textPosition==position)
    {
        return;
    }
    pimpl->textPosition=position;
    scheduleStructureUpdate();
}

//--------------------------------------------------------------------------

AbstractCheckBox::TextPosition AbstractCheckBox::textPosition() const noexcept
{
    return pimpl->textPosition;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setTextPositionName(const QString& name)
{
    setTextPosition(name.compare(QStringLiteral("before"),Qt::CaseInsensitive)==0 ? TextPosition::Before : TextPosition::After);
}

//--------------------------------------------------------------------------

QString AbstractCheckBox::textPositionName() const
{
    return pimpl->textPosition==TextPosition::Before ? QStringLiteral("before") : QStringLiteral("after");
}

//--------------------------------------------------------------------------

Qt::CheckState AbstractCheckBox::checkState() const noexcept
{
    return isChecked() ? Qt::Checked : Qt::Unchecked;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setCheckState(Qt::CheckState state)
{
    // Two-state only -- there is no tristate mode here, PartiallyChecked maps to Checked.
    // Provided for source compatibility with QCheckBox-based call sites being migrated.
    setChecked(state!=Qt::Unchecked);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setParentHovered(bool enable)
{
    pimpl->parentHovered=enable;
    setHovered(enable);
    // The one case uise--AbstractCheckBox[hovered="true"] (checkbox.qss) needs beyond the
    // native ":hover" pseudo-state already in the same rule: a composite parent forcing this
    // checkbox to render hovered while the mouse isn't literally over it. applyPartState()
    // (called by setHovered() above) deliberately never repolishes `this` -- see the comment
    // there -- so do the one-off repolish here instead, where it only costs a call site that
    // is rare and deliberate rather than every routine mouse hover.
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

bool AbstractCheckBox::isParentHovered() const noexcept
{
    return pimpl->parentHovered;
}

//--------------------------------------------------------------------------

void AbstractCheckBox::setHovered(bool enable)
{
    pimpl->hovered=enable;
    applyPartState();
}

//--------------------------------------------------------------------------

void AbstractCheckBox::paintEvent(QPaintEvent* /*event*/)
{
    // QAbstractButton::paintEvent() is pure virtual, and QStyleSheetStyle::polish() only
    // auto-sets WA_StyledBackground for QFrame/QDialog/QMainWindow/... -- QAbstractButton is
    // NOT in that list, so without this handler a QSS background-color/border on
    // uise--CheckBox itself would never be drawn. The indicator parts need no equivalent:
    // they are QFrames (RoundedImage included, it derives from QLabel which derives from
    // QFrame), which Qt's stylesheet engine already draws via CE_ShapedFrame.
    //
    // State_On/State_Off/State_Sunken are set by hand because QStyleOption::initFrom() only
    // fills the generic bits (enabled/focus/mouse-over) -- adding them here is what makes
    // "uise--CheckBox:checked"/":pressed" usable on the widget's own box, alongside the
    // [checked="true"]/[hovered="true"] dynamic properties the child parts key on.
    QStyleOption opt;
    opt.initFrom(this);
    opt.state.setFlag(QStyle::State_On,isChecked());
    opt.state.setFlag(QStyle::State_Off,!isChecked());
    opt.state.setFlag(QStyle::State_Sunken,isDown());

    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&p,this);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::enterEvent(QEnterEvent* event)
{
    QAbstractButton::enterEvent(event);
    if (!pimpl->parentHovered)
    {
        setHovered(true);
        emit hovered(true);
    }
}

//--------------------------------------------------------------------------

void AbstractCheckBox::leaveEvent(QEvent* event)
{
    QAbstractButton::leaveEvent(event);
    if (!pimpl->parentHovered)
    {
        setHovered(false);
        emit hovered(false);
    }
}

//--------------------------------------------------------------------------

void AbstractCheckBox::mousePressEvent(QMouseEvent* event)
{
    // Only left-button presses drive activation (see QAbstractButton::mousePressEvent()),
    // so only those are allowed to touch the flag -- an incidental right-click on the text,
    // which never emits pressed() at all, must not leave a stale value behind for a later
    // unrelated press (mouse or keyboard) to read.
    if (event->button()==Qt::LeftButton)
    {
        // pimpl->indicator is a direct child of `this`, so its geometry() is already
        // expressed in this widget's own coordinates -- the same space event->pos() is in.
        pimpl->suppressNextRipple=!pimpl->indicator->geometry().contains(event->pos());
    }
    QAbstractButton::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::mouseReleaseEvent(QMouseEvent* event)
{
    QAbstractButton::mouseReleaseEvent(event);
    // QAbstractButton only emits released() when the release lands back inside the button; a
    // press dragged out and released elsewhere just does setDown(false) and ignores the
    // event, which would leave the held ripple stuck at full size forever.
    // RippleOverlay::release() is a no-op when nothing is held, so calling it
    // unconditionally here is safe.
    endRipple();
}

//--------------------------------------------------------------------------

void AbstractCheckBox::changeEvent(QEvent* event)
{
    QAbstractButton::changeEvent(event);
    if (event->type()==QEvent::EnabledChange)
    {
        applyCursor();
        // Qt keeps showing a widget's own cursor while it is disabled, and a ripple in
        // flight when the widget is disabled mid-press would never be released.
        if (pimpl->ripple!=nullptr)
        {
            pimpl->ripple->cancel();
        }
        applyPartState();
    }
}

//--------------------------------------------------------------------------

void AbstractCheckBox::applyPartState()
{
    const auto hov=pimpl->hovered || pimpl->parentHovered;
    const auto chk=isChecked();
    // Mirrored as a dynamic property rather than relying on ":disabled" as an ANCESTOR
    // qualifier in checkbox.qss (uise--AbstractCheckBox:disabled #text) -- unlike every other
    // ancestor->descendant rule here, which is dynamic-property-driven ([hovered="true"]
    // #indicatorOff etc., a simple, reliable QObject::property() read), a native pseudo-class
    // used as an ancestor qualifier for a descendant combinator does not appear to be
    // re-evaluated per instance: it can end up applying to every sibling once it has matched
    // once anywhere, e.g. the "Disabled option" CheckBox in demo/checkbox forcing every OTHER
    // (enabled) CheckBox's #text to render disabled-styled too. Self-referential ":disabled"
    // (styling the SAME widget it is attached to, e.g. #indicatorOff:disabled) is unaffected
    // and stays as-is -- that is the common, proven pattern used throughout this library.
    const auto dis=!isEnabled();
    const auto shapeName=indicatorShapeName();
    const auto svg=(pimpl->indicatorMode==IndicatorMode::Svg)
                    || (pimpl->indicatorMode==IndicatorMode::Auto && pimpl->icon!=nullptr);
    const auto modeName=svg ? QStringLiteral("svg") : QStringLiteral("qss");

    // `this` gets the properties too -- every checkbox.qss rule keyed on [shape=...] etc. is a
    // DESCENDANT selector (uise--AbstractCheckBox[shape="circle"] #indicatorOff), and Qt reads
    // the ancestor's property value live when it recomputes the DESCENDANT's own matched rule
    // set, with no repolish of the ancestor required for that read -- but `this` is
    // deliberately NOT in the repolish loop below. checkbox.qss also sets qproperty-cursorShape/
    // -indicatorMode/-textPosition/-checkAnimation* unconditionally on `this`, so repolishing it
    // on every routine hover/toggle would re-invoke those Q_PROPERTY writers and silently
    // revert any value a caller had set in C++ back to the stylesheet default. The one rule
    // that styles `this` directly, uise--AbstractCheckBox[hovered="true"], is redundant with
    // the native ":hover" pseudo-state already in the same selector (see checkbox.qss) for
    // real mouse hover -- it only adds something for setParentHovered()-forced hover, which
    // repolishes `this` itself, see there.
    this->setProperty("hovered",hov);
    this->setProperty("checked",chk);
    this->setProperty("disabled",dis);
    this->setProperty("shape",shapeName);
    this->setProperty("mode",modeName);

    QWidget* parts[]={pimpl->indicator,pimpl->off,pimpl->on,pimpl->mark,pimpl->markIconWidget,pimpl->text};
    for (auto* w : parts)
    {
        w->setProperty("hovered",hov);
        w->setProperty("checked",chk);
        w->setProperty("disabled",dis);
        w->setProperty("shape",shapeName);
        w->setProperty("mode",modeName);
        // A dynamic property change alone does not invalidate Qt's cached style evaluation
        // for the widget it was set on, so repolishing each part is what makes a rule keyed
        // on e.g. #indicatorOff[hovered="true"] take effect -- the exact trap
        // IconTextButton::setHovered() documents.
        Style::updateWidgetStyle(w);
    }

    pimpl->markIconWidget->setParentHovered(hov);
    pimpl->markIconWidget->setSelected(chk);
    pimpl->text->repaint();
}

//--------------------------------------------------------------------------

void AbstractCheckBox::applyCursor()
{
    setCursor(isEnabled() ? pimpl->cursorShape : pimpl->disabledCursorShape);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::updateCheckedState(bool animate)
{
    const auto target=isChecked() ? 1.0 : 0.0;

    pimpl->checkAnim->stop();

    // No animation before the widget is on screen (the initial state must simply BE the
    // state, not fade into it), and none when the stylesheet has switched it off.
    if (!animate || !pimpl->animationEnabled || pimpl->durationMs<=0 || !isVisible())
    {
        pimpl->onEffect->setOpacity(target);
        return;
    }

    pimpl->checkAnim->setStartValue(pimpl->onEffect->opacity());
    pimpl->checkAnim->setEndValue(target);
    pimpl->checkAnim->setDuration(pimpl->durationMs);
    pimpl->checkAnim->setEasingCurve(pimpl->easingType);
    pimpl->checkAnim->start();
}

//--------------------------------------------------------------------------

void AbstractCheckBox::endRipple()
{
    if (pimpl->ripple!=nullptr)
    {
        pimpl->ripple->release();
    }
}

//--------------------------------------------------------------------------

void AbstractCheckBox::applyTextPosition()
{
    if (pimpl->boxLayout!=nullptr)
    {
        pimpl->boxLayout->removeWidget(pimpl->indicator);
        pimpl->boxLayout->removeWidget(pimpl->text);
    }

    if (pimpl->textPosition==TextPosition::Before)
    {
        pimpl->boxLayout->addWidget(pimpl->text);
        pimpl->boxLayout->addWidget(pimpl->indicator);
    }
    else
    {
        pimpl->boxLayout->addWidget(pimpl->indicator);
        pimpl->boxLayout->addWidget(pimpl->text);
    }

    // Drives checkbox.qss's #text margin swap between the "after" (default) and "before"
    // layout -- see uise--AbstractCheckBox[textPosition="before"] #text there. That's a
    // DESCENDANT selector, so #text is what needs repolishing, not `this` -- repolishing
    // `this` would also re-invoke every qproperty-* writer checkbox.qss declares on `this`
    // (indicatorMode, textPosition itself included), silently reverting any value a caller
    // had just set in C++ back to the stylesheet default. See applyPartState()'s comment for
    // the same hazard -- this is exactly why a demo's "Text position" combo box appeared to
    // have no effect.
    setProperty("textPosition",textPositionName());
    Style::updateWidgetStyle(pimpl->text);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::applyIndicatorMode()
{
    const auto svg=(pimpl->indicatorMode==IndicatorMode::Svg)
                    || (pimpl->indicatorMode==IndicatorMode::Auto && pimpl->icon!=nullptr);
    pimpl->markIconWidget->setVisible(svg);
    pimpl->mark->setVisible(!svg);
}

//--------------------------------------------------------------------------

void AbstractCheckBox::scheduleStructureUpdate()
{
    if (pimpl->structureUpdateScheduled)
    {
        return;
    }
    pimpl->structureUpdateScheduled=true;

    QPointer<AbstractCheckBox> guard(this);
    QMetaObject::invokeMethod(
        this,
        [guard]()
        {
            if (guard.isNull())
            {
                return;
            }
            // cleared BEFORE the update so a legitimate later request is not swallowed;
            // doUpdateStructure() itself must never call back into here synchronously
            guard->pimpl->structureUpdateScheduled=false;
            guard->doUpdateStructure();
        },
        Qt::QueuedConnection
    );
}

//--------------------------------------------------------------------------

void AbstractCheckBox::doUpdateStructure()
{
    applyIndicatorMode();
    applyTextPosition();
    applyPartState();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
