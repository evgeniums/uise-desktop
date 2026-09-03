/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chatmessage.cpp
*
*  Defines ChatMessage.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QPointer>
#include <QMouseEvent>
#include <QLabel>
#include <QLocale>
#include <QResizeEvent>
#include <QVariantAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QRectF>
#include <QStyleOption>
#include <QStyle>

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/chatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Slack subtracted from the negotiation budget in AbstractChatMessageContent::
//! updateBubbleWidth() before asking sections for their width hints, so a section's
//! own rounding/frame width never pushes the bubble a pixel or two past forMaxWidthIn.
constexpr int BubbleWidthSlack=10;

//! Gap left between the body and the time/status row when ChatMessageBottom::
//! bubbleWidthHint() widens the bubble for a too-narrow body -- see DefaultNarrowBodyWidth.
constexpr int BottomGap=10;

} // anonymous namespace

/***************************AbstractChatMessage******************************/

void AbstractChatMessage::detectMouseSelection(std::optional<bool> select)
{
    if (select)
    {
        setSelectDetectionBlocked(true);
        setSelected(select.value());
        return;
    }
    else if (topSeparator() && topSeparator()->underMouse())
    {
        return;
    }

    if (!isSelectDetectionBlocked())
    {
        if (!isSelected())
        {
            setSelectDetectionBlocked(true);
            setSelected(true);
            if (!isSelectionMode())
            {
                emit selectionModeRequested();
            }
        }
        else
        {
            setSelectDetectionBlocked(true);
            setSelected(false);
        }
    }
    else
    {
    }
}

//--------------------------------------------------------------------------

void AbstractChatMessage::ensureHighlightAnimation()
{
    if (m_highlightAnim!=nullptr)
    {
        return;
    }

    m_highlightHoldTimer=new SingleShotTimer(this);

    m_highlightAnim=new QVariantAnimation(this);
    connect(
        m_highlightAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            setHighlightFactor(value.toReal());
        }
    );
}

//--------------------------------------------------------------------------

void AbstractChatMessage::setHighlightFactor(qreal value)
{
    m_highlightFactor=value;
    update(highlightRect());
}

//--------------------------------------------------------------------------

void AbstractChatMessage::startHighlight()
{
    ensureHighlightAnimation();

    m_highlightHoldTimer->cancel();

    // QAbstractAnimation::stop() emits finished() as a side effect (see DropdownFrame's/
    // RippleOverlay's own animations for the same house pitfall) -- harmless here, nothing is
    // connected to m_highlightAnim's finished(), only its valueChanged() above.
    m_highlightAnim->stop();
    setHighlightFactor(1.0);

    // restart=true: a re-jump onto an already-highlighted (or fading) message always restarts
    // the hold from a full highlightHoldMs(), rather than firing at whatever point the first
    // jump's timer was already at.
    m_highlightHoldTimer->shot(
        static_cast<size_t>(qMax(0,m_highlightHoldMs)),
        [this]()
        {
            m_highlightAnim->setStartValue(1.0);
            m_highlightAnim->setEndValue(0.0);
            m_highlightAnim->setDuration(qMax(0,m_highlightFadeMs));
            m_highlightAnim->setEasingCurve(static_cast<QEasingCurve::Type>(m_highlightEasingCurveType));
            m_highlightAnim->start();
        },
        true
    );
}

//--------------------------------------------------------------------------

void AbstractChatMessage::clearHighlight()
{
    if (m_highlightHoldTimer!=nullptr)
    {
        m_highlightHoldTimer->cancel();
    }
    if (m_highlightAnim!=nullptr)
    {
        m_highlightAnim->stop();
    }
    setHighlightFactor(0.0);
}

//--------------------------------------------------------------------------

void AbstractChatMessage::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    // Re-invoke the stylesheet's own frame painting explicitly -- required once a widget
    // overrides paintEvent(), same idiom as DropdownFrame::paintEvent()/FloatingDialogFrame::
    // paintEvent() (see either for the longer explanation). Without this, any current/future QSS
    // background/border rule on uise--AbstractChatMessage would silently stop rendering.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&painter,this);

    if (m_highlightFactor>0.0)
    {
        // Same opacity convention as RippleOverlay::paintEvent() (ripple.cpp): the colour's own
        // alpha, highlightOpacity's peak fraction, and the current fade factor all multiply
        // together, rather than one overriding the others.
        QColor c=m_highlightColor;
        c.setAlphaF(qBound(0.0,c.alphaF()*m_highlightOpacity*m_highlightFactor,1.0));
        painter.fillRect(highlightRect(),c);
    }
}

//--------------------------------------------------------------------------

void AbstractChatMessageContent::updateBubbleWidth(int forMaxWidthIn)
{
    m_lastForMaxWidth=forMaxWidthIn;
    m_everNegotiated=true;

    auto forMaxWidth=forMaxWidthIn-horizontalTotalMargin(this)-BubbleWidthSlack;

    // Populate the body-hint memo for THIS pass before querying any section -- bodyWidthHint()
    // (used by ChatMessageBottom::bubbleWidthHint() below, via the section loop) reuses it
    // instead of re-invoking body()->bubbleWidthHint(), which for some bodies (e.g.
    // ChatMessageImages::rebuildGrid()) is a full re-layout, not a cheap query.
    m_bodyWidthHintForMaxWidth=forMaxWidth;
    m_bodyWidthHint=(body()!=nullptr) ? body()->bubbleWidthHint(forMaxWidth) : 0;
    m_bodyWidthHintValid=true;

    int widthHint=0;
    for (auto& section : m_sections)
    {
        // Reuse the memo just populated above instead of calling bubbleWidthHint() on the body
        // a second time in the same pass.
        auto sectionWidthHint=(section==static_cast<ChatMessageContentSection*>(body()))
            ? m_bodyWidthHint
            : section->bubbleWidthHint(forMaxWidth);
        if (sectionWidthHint>widthHint)
        {
            widthHint=sectionWidthHint;
        }
    }

    if (widthHint>forMaxWidth)
    {
        widthHint=forMaxWidth;
    }

    setMaximumBubbleWidth(widthHint);
    emit bubbleWidthUpdated();
}

//--------------------------------------------------------------------------

void AbstractChatMessageContent::rebuildSections()
{
    m_sections.clear();

    auto attach=[this](ChatMessageContentSection* section)
    {
        if (section==nullptr)
        {
            return;
        }
        section->setChatMessage(chatMessage());
        section->setChatContent(this);
        m_sections.push_back(section);
    };

    attach(m_header);
    attach(m_reply);
    attach(m_body);
    attach(m_comment);
    attach(m_bottom);
}

//--------------------------------------------------------------------------

void AbstractChatMessageContent::renegotiateBubbleWidth()
{
    if (!m_everNegotiated)
    {
        // No real forMaxWidthIn to repeat yet -- see this function's own doc comment.
        return;
    }
    updateBubbleWidth(m_lastForMaxWidth);
}

//--------------------------------------------------------------------------

void AbstractChatMessageContent::setMaximumBubbleWidth(int width)
{
    m_maximumBubbleWidth=width;
    for (auto& section : m_sections)
    {
        section->updateMaximumBubbleWidth();
    }
    updateGeometry();
    resize(sizeHint());
}

//--------------------------------------------------------------------------

QSize AbstractChatMessageContent::sizeHint() const
{
    return QSize{m_maximumBubbleWidth+horizontalTotalMargin(this),AbstractChatMessageChild::sizeHint().height()};
}

/***************************ChatSeparatorSection*****************************/

//--------------------------------------------------------------------------

class ChatSeparatorSection_p
{
    public:

        QFrame* content;

        QFrame* leftLine=nullptr;

        AvatarButton* button;
        bool clickable=true;

        QFrame* rightLine=nullptr;

        QFrame* makeHLine(QWidget* parent, const QString& name) const;
};

//--------------------------------------------------------------------------

QFrame* ChatSeparatorSection_p::makeHLine(QWidget* parent, const QString& name) const
{
    auto f=new QFrame(parent);
    f->setVisible(false);
    f->setObjectName(name);
    return f;
}

//--------------------------------------------------------------------------

ChatSeparatorSection::ChatSeparatorSection(QWidget* parent)
    : AbstractChatSeparatorSection(parent),
        pimpl(std::make_unique<ChatSeparatorSection_p>())
{
    auto mainL=Layout::horizontal(this);

    pimpl->content=new QFrame(this);
    pimpl->content->setObjectName("separatorSection");
    mainL->addWidget(pimpl->content);

    auto l=Layout::vertical(pimpl->content);

    pimpl->leftLine=pimpl->makeHLine(pimpl->content,"leftLine");
    l->addWidget(pimpl->leftLine,1);

    pimpl->button=new AvatarButton(pimpl->content);
    l->addWidget(pimpl->button,0,Qt::AlignCenter);

    pimpl->rightLine=pimpl->makeHLine(pimpl->content,"rightLine");
    l->addWidget(pimpl->rightLine,1);

    connect(
        pimpl->button,
        &AvatarButton::clicked,
        this,
        [this]()
        {
            if (pimpl->clickable)
            {
                emit clicked();
            }
        }
    );

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatSeparatorSection::~ChatSeparatorSection()
{}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setClickable(bool enable)
{
    pimpl->clickable=enable;
    if (enable)
    {
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
        setCursor(Qt::ArrowCursor);
    }
}

//--------------------------------------------------------------------------

bool ChatSeparatorSection::isClickable() const
{
    return pimpl->clickable;
}

//--------------------------------------------------------------------------

QWidget* ChatSeparatorSection::clickableWidget()
{
    return pimpl->button;
}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setHLineVisible(bool enable)
{
    pimpl->leftLine->setVisible(enable);
    pimpl->rightLine->setVisible(enable);
}

//--------------------------------------------------------------------------

bool ChatSeparatorSection::isHLineVisible() const
{
    return pimpl->leftLine->isVisible();
}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setText(const QString& text)
{
    pimpl->button->setText(text);
}

//--------------------------------------------------------------------------

QString ChatSeparatorSection::text() const
{
    return pimpl->button->text();
}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setIconPath(WithPath path)
{
    pimpl->button->setAvatarPath(std::move(path));
}

//--------------------------------------------------------------------------

WithPath ChatSeparatorSection::iconPath() const
{
    return pimpl->button->avatarPath();
}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setIconSource(std::shared_ptr<AvatarSource> source)
{
    pimpl->button->setAvatarSource(std::move(source));
}

//--------------------------------------------------------------------------

std::shared_ptr<AvatarSource> ChatSeparatorSection::iconSource() const
{
    return pimpl->button->avatarSource();
}

//--------------------------------------------------------------------------

void ChatSeparatorSection::setTailIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->button->setTailSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> ChatSeparatorSection::tailIcon() const
{
    return pimpl->button->tailSvgIcon();
}

/********************************ChatSeparator*******************************/

//--------------------------------------------------------------------------

ChatSeparator::ChatSeparator(QWidget* parent)
    : AbstractChatSeparator(parent)
{
    m_layout=Layout::vertical(this);
}

//--------------------------------------------------------------------------

void ChatSeparator::doInsertSection(AbstractChatSeparatorSection* section, int index)
{
    if (index>=0 && index<static_cast<int>(sectionCount()))
    {
        m_layout->insertWidget(index,section);
    }
    else
    {
        m_layout->addWidget(section);
    }
}

/*************************ChatMessageContent*******************************/

//--------------------------------------------------------------------------

ChatMessageContent::ChatMessageContent(QWidget* parent)
    : AbstractChatMessageContent(parent)
{
    m_layout=Layout::vertical(this);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateWidgets()
{
    // Must be idempotent -- setReply() can re-run this after setWidgets() already has (the
    // original single-shot version just kept appending and adding another trailing stretch).
    // QWidgetItem does not own its widget, so deleting the layout item only frees the item
    // (and any stretch spacer); the widget itself is untouched and stays our child, ready to be
    // re-added below. The setParent(this) that used to run here is guarded: taking a widget out
    // of a layout does not reparent it, so on every real call the parent already IS `this`, and
    // Qt does not short-circuit a same-parent setParent() -- it would run
    // QWidgetPrivate::inheritStyle() over the section's whole subtree for nothing.
    while (m_layout->count()>0)
    {
        auto item=m_layout->takeAt(0);
        if (item->widget()!=nullptr && item->widget()->parentWidget()!=this)
        {
            item->widget()->setParent(this);
        }
        delete item;
    }

    // show() clears WA_WState_Hidden synchronously, so the very next sizeHint() counts this
    // section -- addWidget() alone leaves it hidden until a queued _q_showIfNotHidden when this
    // frame is already visible (e.g. setReply()/setComment() re-running this on a bubble already
    // on screen), and the bubble would be mis-measured until that queued show ran.
    if (header()!=nullptr)
    {
        m_layout->addWidget(header(),0,Qt::AlignLeft);
        header()->show();
    }
    if (reply()!=nullptr)
    {
        m_layout->addWidget(reply(),0,Qt::AlignLeft);
        reply()->show();
    }
    if (body()!=nullptr)
    {
        m_layout->addWidget(body(),0,Qt::AlignLeft);
        body()->show();
    }
    if (comment()!=nullptr)
    {
        m_layout->addWidget(comment(),0,Qt::AlignLeft);
        comment()->show();
    }
    if (bottom()!=nullptr)
    {
        m_layout->addWidget(bottom(),0,Qt::AlignLeft);
        bottom()->show();
    }
    m_layout->addStretch(1);
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

void ChatMessageContent::clearContentSelection()
{
    if (header()!=nullptr)
    {
        header()->clearContentSelection();
    }
    if (reply()!=nullptr)
    {
        reply()->clearContentSelection();
    }
    if (body()!=nullptr)
    {
        body()->clearContentSelection();
    }
    if (comment()!=nullptr)
    {
        comment()->clearContentSelection();
    }
    if (bottom()!=nullptr)
    {
        bottom()->clearContentSelection();
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::setSelected(bool enable)
{
    rememberSelected(enable);
    Style::setStyleProperty(this,"selected",enable);
    if (bottom())
    {
        bottom()->setSelected(enable);
    }
    if (header())
    {
        header()->setSelected(enable);
    }
    if (reply())
    {
        reply()->setSelected(enable);
    }
    if (comment())
    {
        comment()->setSelected(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::setSent(bool enable)
{
    rememberSent(enable);
    Style::setStyleProperty(this,"sent",enable);
    if (bottom())
    {
        bottom()->setSent(enable);
    }
    if (header())
    {
        header()->setSent(enable);
    }
    if (reply())
    {
        reply()->setSent(enable);
    }
    if (comment())
    {
        comment()->setSent(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::setRight(bool enable)
{
    // Repolish: chat.qss keys the bubble's tail corner radii on [right=...] (split off from
    // [sent=...], which now drives colour only -- see updateAlignment()/setAlignSent()), so a
    // side flip must actually re-match the stylesheet.
    Style::setStyleProperty(this,"right",enable);
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateChatMessage()
{
    connect(
        chatMessage(),
        &AbstractChatMessage::lastInBatchUpdated,
        this,
        &ChatMessageContent::updateLastInBatch
    );
    connect(
        chatMessage(),
        &AbstractChatMessage::firstInBatchUpdated,
        this,
        &ChatMessageContent::updateFirstInBatch
    );

    setRight(chatMessage()->isRight());
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateFirstInBatch()
{
    Style::setStyleProperty(this,"first",chatMessage()->isFirstInBatch());
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateLastInBatch()
{
    Style::setStyleProperty(this,"last",chatMessage()->isLastInBatch());
}

/*************************ChatMessageContentWrapper*************************/

ChatMessageContentWrapper::ChatMessageContentWrapper(QWidget* parent) : QFrame(parent)
{
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::setContent(AbstractChatMessageContent* content)
{
    m_content=content;
    // Guarded: callers that built the content under AbstractChatMessage::contentParentWidget()
    // (i.e. under this wrapper) are already correct, and Qt does not short-circuit a same-parent
    // setParent() -- it would run QWidgetPrivate::inheritStyle() over the whole content subtree,
    // the single largest one in the message, for nothing.
    if (m_content->parentWidget()!=this)
    {
        m_content->setParent(this);
    }
    updatePosition();
    m_content->installEventFilter(this);
    updateGeometry();

    connect(
        m_content,
        &AbstractChatMessageContent::bubbleWidthUpdated,
        this,
        &ChatMessageContentWrapper::updatePosition
    );
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::updatePosition()
{
    if (!m_content)
    {
        return;
    }

    m_content->resize(m_content->sizeHint());
    applyContentPosition();
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::applyContentPosition()
{
    if (!m_content)
    {
        return;
    }

    if (m_right)
    {
        m_content->move(width()-contentsMargins().right()-m_content->width(),contentsMargins().top());
    }
    else
    {
        m_content->move(contentsMargins().left(),contentsMargins().top());
    }
}

//--------------------------------------------------------------------------

bool ChatMessageContentWrapper::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_content && event->type() == QEvent::Resize)
    {
        updatePosition();
        updateGeometry();
    }
    return QFrame::eventFilter(obj, event);
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    // Move-only: this wrapper is layout-managed by its parent, so it can be resized by that
    // parent's layout pass before m_content's own geometry has ever been set from a real
    // (non-default) width -- without this, the bubble is first positioned against Qt's default
    // 100px child geometry (a negative x for any wider bubble, i.e. clipped flush left) and only
    // catches up whenever something else happens to re-run updatePosition() later. Move-only
    // also keeps this handler re-entrancy-safe: it can never resize m_content, so it can never
    // trigger the Resize eventFilter above and loop back into this wrapper's own resizeEvent().
    applyContentPosition();
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);

    // m_content's own body section (e.g. ChatMessageFiles) can report a bogus near-zero
    // sizeHint() while still hidden -- its rows are shown individually
    // (ChatMessageFiles::rebuildList()'s explicit row->show()), but the section widget itself
    // only gets its OWN internal layout activated by Qt on its first real show, which is when its
    // sizeHint() first becomes correct.
    //
    // That alone would self-correct once m_content's own m_layout (the QBoxLayout holding
    // header/reply/body/comment/bottom) is re-queried -- except QBoxLayout wraps each
    // addWidget()'d child in its own QWidgetItemV2, which caches that CHILD's size hint
    // SEPARATELY from the box layout's own aggregate cache. Invalidating the box layout alone
    // only clears the latter; the per-child item cache is only cleared when the CHILD ITSELF
    // calls updateGeometry() (QWidgetPrivate::updateGeometry_helper() explicitly invalidates the
    // QWidgetItem wrapping the widget in its parent's layout). Nothing else calls that for a
    // section whose true size only became known once it was shown, so the box layout keeps
    // reading each section's stale, pre-show item cache regardless of how many times it is
    // itself invalidated. Call updateGeometry() on every section explicitly rather than special-
    // casing body -- any section could in principle hit the same gap -- then invalidate the box
    // layout so its own aggregate cache is rebuilt from the now-fresh per-child values, and
    // finally re-apply the result via updatePosition() (m_content->resize(m_content->sizeHint())).
    // Runs once, on this wrapper's own first show, which is exactly when the whole subtree --
    // including every section -- has just been shown for real.
    if (m_content!=nullptr && m_content->layout()!=nullptr)
    {
        auto* l=m_content->layout();
        for (int i=0;i<l->count();++i)
        {
            auto* item=l->itemAt(i);
            auto* w=item!=nullptr?item->widget():nullptr;
            if (w!=nullptr)
            {
                w->updateGeometry();
            }
        }
        l->invalidate();
    }
    updatePosition();
}

//--------------------------------------------------------------------------

QSize ChatMessageContentWrapper::sizeHint() const
{
    if (m_content)
    {
        return m_content->sizeHint();
    }
    return QFrame::sizeHint();
}

/********************************ChatMessage*********************************/

//--------------------------------------------------------------------------

class ChatMessage_p
{
    public:

        QBoxLayout* layout;

        QFrame* bottomSpace;

        QFrame* separatorFrame;
        QBoxLayout* separatorLayout;

        QFrame* main;
        QBoxLayout* mainLayout;

        ChatMessageAvatar* avatarFrame;
        QFrame* avatarFramePlaceholder;

        //! Null until construct() runs -- contentParentWidget() is public and may be reached
        //! before then, so it must be able to tell "not built yet" from a real frame.
        ChatMessageContentWrapper* contentFrame=nullptr;
        QBoxLayout* contentLayout;

        //! Built lazily by ChatMessage::ensureSelector(), so null until multi-select mode is
        //! first entered on this message. Every read must be guarded.
        AbstractChatMessageSelector* selector=nullptr;

        //! Guards ChatMessage::showEvent()'s one-time geometry repair -- see that method's own
        //! doc comment. The flyweight list destroys and rebuilds scrolled-away messages rather
        //! than pooling them, so "this widget's first show" is exactly the per-message point that
        //! needs repairing; re-showing an already-settled widget (e.g. switching back to a page
        //! that never changed while hidden) must stay a no-op.
        bool firstShowSettled=false;
};

//--------------------------------------------------------------------------

ChatMessage::ChatMessage(QWidget* parent)
    : AbstractChatMessage(parent),
      pimpl(std::make_unique<ChatMessage_p>())
{
    pimpl->layout=Layout::vertical(this);
}

//--------------------------------------------------------------------------

ChatMessage::~ChatMessage()
{}

//--------------------------------------------------------------------------

void ChatMessage::construct()
{
    pimpl->separatorFrame=new QFrame(this);
    pimpl->separatorFrame->setObjectName("separatorFrame");
    pimpl->separatorLayout=Layout::vertical(pimpl->separatorFrame);
    pimpl->separatorFrame->setVisible(false);
    pimpl->layout->addWidget(pimpl->separatorFrame);

    pimpl->main=new QFrame(this);
    pimpl->layout->addWidget(pimpl->main);
    pimpl->main->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    pimpl->main->setObjectName("mainMessageFrame");
    pimpl->mainLayout=Layout::horizontal(pimpl->main);

    // Selector is built on demand, see ensureSelector().

    pimpl->avatarFrame=new ChatMessageAvatar(pimpl->main);
    pimpl->avatarFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    pimpl->avatarFrame->avatar()->setVisible(false);

    // Clickable affordance -- AvatarWidget emits clicked() only while clickable, and (unlike
    // ChatSeparatorSection::setClickable()) does not set the cursor itself. Wired unconditionally:
    // while the avatar is hidden it cannot be clicked or hovered anyway.
    pimpl->avatarFrame->avatar()->setClickable(true);
    pimpl->avatarFrame->avatar()->setCursor(Qt::PointingHandCursor);
    connect(
        pimpl->avatarFrame->avatar(),
        &AvatarWidget::clicked,
        this,
        &AbstractChatMessage::avatarClicked
    );

    pimpl->avatarFramePlaceholder=new QFrame(pimpl->main);
    pimpl->avatarFramePlaceholder->setObjectName("avatarFrame");
    pimpl->avatarFramePlaceholder->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);

    pimpl->contentFrame=new ChatMessageContentWrapper(pimpl->main);

    pimpl->bottomSpace=new QFrame(this);
    pimpl->bottomSpace->setObjectName("bottomSpace");
    pimpl->layout->addWidget(pimpl->bottomSpace);
    pimpl->bottomSpace->setVisible(false);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    // A widget's size hint is only meaningful after QStyle::polish() has applied the QSS
    // geometry rules (min/max-width, padding, ...) -- ensurePolished() is what actually
    // guarantees that, recursing into every descendant built above and marking each polished
    // exactly once (QWidgetPrivate::polished), unlike Style::updateWidgetStyle() which is
    // neither recursive nor idempotent. This is what makeMessage() (chatmessagesview.ipp) relies
    // on before it reads bubbleOuterWidth()/minimumWidth()/maximumWidth() off this widget.
    ensurePolished();
}

//--------------------------------------------------------------------------

QRect ChatMessage::highlightRect() const
{
    // Anchored on #mainMessageFrame's own geometry rather than rect() minus a computed offset,
    // so this stays correct whether or not #separatorFrame is currently visible, and skips this
    // row's own "margin-top: 2px" QSS band (chat.qss) so the highlight never bleeds into the
    // previous message.
    QRect r=pimpl->main->geometry();
    if (pimpl->bottomSpace->isVisible())
    {
        r=r.united(pimpl->bottomSpace->geometry());
    }
    return QRect{0,r.top(),width(),r.height()};
}

//--------------------------------------------------------------------------

void ChatMessage::updateTopSeparator()
{
    bool sepVisible=topSeparator()!=nullptr;
    pimpl->separatorFrame->setVisible(sepVisible);
    if (sepVisible)
    {
        pimpl->separatorLayout->addWidget(topSeparator());

        // QLayout::addWidget() on an already-visible parent (separatorFrame, just shown above,
        // is one for any message that's already on screen) only auto-shows the new child via a
        // QUEUED invocation -- see qt-layout-queued-autoshow-gotcha. Until the next event-loop
        // turn the separator stays isHidden()/isEmpty(), so it contributes zero to this
        // message's sizeHint(). Harmless for a message built while still hidden (the normal
        // parent-show cascade shows it synchronously), but for an ALREADY-DISPLAYED message
        // retroactively gaining a top separator -- exactly what prepending older messages does
        // when it shifts date-group boundaries -- this made resizeList() undersize m_llist by
        // one separator's height for the rest of that turn, and the correction that showed up a
        // turn later (via a posted LayoutRequest reaching relayout()) had no paired
        // compensateSizeChange() call to keep the scroll-anchor's screen position steady:
        // exactly the "view creeps up a few pixels after a history prefetch" symptom.
        topSeparator()->show();
    }
}

//--------------------------------------------------------------------------

void ChatMessage::ensureSelector()
{
    if (pimpl->selector!=nullptr)
    {
        return;
    }

    pimpl->selector=makeWidget<AbstractChatMessageSelector,ChatMessageSelector>(pimpl->main);
    connect(
        pimpl->selector,
        &AbstractChatMessageSelector::toggled,
        this,
        [this](bool checked)
        {
            setSelected(checked);
        }
    );
    // Explicit hide BEFORE it enters the layout: QBoxLayout auto-shows a child it adopts unless
    // the child was explicitly hidden, and the caller decides visibility right after this.
    pimpl->selector->setVisible(false);

    // Adopt the current selection state: setSelected() is public and does not require selection
    // mode, so a message can already be selected by the time its selector first gets built.
    pimpl->selector->blockSignals(true);
    pimpl->selector->setChecked(isSelected());
    pimpl->selector->blockSignals(false);

    // Same slot updateContent() would have put it in -- first when the selector sits on the
    // left, last otherwise.
    if (isSelectorOnLeft())
    {
        pimpl->mainLayout->insertWidget(0,pimpl->selector);
    }
    else
    {
        pimpl->mainLayout->addWidget(pimpl->selector);
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelectionMode()
{
    if (isSelectionMode())
    {
        ensureSelector();
    }
    if (pimpl->selector!=nullptr)
    {
        pimpl->selector->setVisible(isSelectionMode());
        if (!isSelectionMode())
        {
            pimpl->selector->blockSignals(true);
            pimpl->selector->setChecked(false);
            pimpl->selector->blockSignals(false);
        }
    }
    // Selector insertion/removal above narrows/widens contentFrame via mainLayout, and
    // ChatMessageContentWrapper::resizeEvent() now repositions the bubble synchronously as soon
    // as that layout pass reaches it, so no deferred re-run is needed here.
    pimpl->contentFrame->updateGeometry();
    pimpl->main->updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelection()
{
    content()->setSelected(isSelected());
    pimpl->avatarFrame->setSelected(isSelected());

    // Null unless multi-select mode was entered on this message, which is the only way it could
    // have become selected in the first place.
    if (pimpl->selector!=nullptr)
    {
        pimpl->selector->blockSignals(true);
        pimpl->selector->setChecked(isSelected());
        pimpl->selector->blockSignals(false);
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateLastInBatch()
{
    pimpl->avatarFrame->setLastInBatch(isLastInBatch());
    pimpl->bottomSpace->setVisible(isLastInBatch());

    // Only the last message of a batch carries the avatar (it is the one with the tail), and a
    // message stops being last as soon as the same sender's next one arrives -- so this has to be
    // re-derived here too, not just when the alignment changes.
    updateAvatarForced();

    // No repolish of `this` here: chat.qss puts qproperty-selectorPositionLeft on
    // uise--AbstractChatMessage, and a repolish would re-apply that default over any
    // programmatic setSelectorOnLeft() -- and no QSS rule keys on [last=...] on this widget
    // itself anyway (only on the content bubble and the avatar, both updated separately above).
    setProperty("last",isLastInBatch());
}

//--------------------------------------------------------------------------

void ChatMessage::updateAvatarForced()
{
    // alignSent() is checked regardless of THIS message's own direction(): ChatMessagesView::
    // makeMessage()/applyAlignSentToMessages() set it uniformly on every message, Sent or
    // Received, so it reads as "the view currently puts own messages on the left too".
    bool leftAligned=(alignSent()==AlignSent::Left);

    setAvatarVisible(leftAligned && isLastInBatch());

    // Width tracks leftAligned ALONE, not the visibility above -- every bubble in a batch must
    // keep the same left inset, including the ones whose avatar is suppressed.
    auto avatarSize=leftAligned ? ForcedAvatarSize : ChatMessageAvatar::DefaultAvatarSize;
    auto columnWidth=leftAligned ? (ForcedAvatarSize+2*ForcedAvatarMargin)
                                 : ChatMessageAvatar::DefaultAvatarSize;
    pimpl->avatarFrame->setAvatarSize(avatarSize);
    pimpl->avatarFrame->setFixedWidth(columnWidth);
    pimpl->avatarFramePlaceholder->setFixedWidth(columnWidth);
}

//--------------------------------------------------------------------------

void ChatMessage::changeEvent(QEvent* event)
{
    AbstractChatMessage::changeEvent(event);

    // avatarSize is a genuine Q_PROPERTY (chat.qss's qproperty-avatarSize:16) so a full app
    // stylesheet reload (e.g. auto-following an OS colour-theme change, Style::instance().
    // applyStyleSheet(true)) re-polishes avatarFrame and silently resets it to that QSS default,
    // shrinking the actual avatar IMAGE back to 16x16 even though the column around it (a plain
    // setFixedWidth(), not QSS-backed) stays at whatever width updateAvatarForced() last gave it.
    // Unlike updateLastInBatch()'s own qproperty-selectorPositionLeft concern (worked around by
    // never repolishing `this`), that repolish happens on avatarFrame directly and cannot be
    // opted out of. Only re-deriving the forced size afterwards fixes it back up -- otherwise
    // every already-built message's avatar stays stuck at the QSS default (looks "very small")
    // until the chat is closed and reopened, which rebuilds ChatMessageAvatar from scratch
    // instead.
    //
    // Deferred via singleShot(0): a mass repolish walks the WHOLE widget tree, and whether this
    // row's own StyleChange fires before or after avatarFrame's own qproperty writers have run is
    // an unspecified ordering internal to Qt's style-sheet engine -- posting this for the next
    // event-loop turn guarantees every repolish this pass triggers (avatarFrame's included) has
    // already landed before updateAvatarForced() re-asserts the forced value over it. `this` as
    // the context object is the usual Qt guard against the row being destroyed before it fires.
    if (event->type()==QEvent::StyleChange)
    {
        QTimer::singleShot(0,this,[this](){updateAvatarForced();});
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateFirstInBatch()
{
    // Same reasoning as updateLastInBatch() above -- no repolish of `this`.
    setProperty("first",isFirstInBatch());
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessage::updateContentVisible()
{
    pimpl->main->setVisible(isContentVisible());
}

//--------------------------------------------------------------------------

void ChatMessage::updateContent()
{
    if (content()!=nullptr)
    {
        content()->updateGeometry();

        content()->setSent(direction()==Direction::Sent);

        pimpl->contentFrame->setContent(content());

        updateAlignment();
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateAlignment()
{
    if (content()==nullptr)
    {
        return;
    }

    // Geometry only (tail radii, avatar mask, left/right ordering) -- colour is setSent()'s job
    // and is not touched here, so calling this repeatedly (setAlignSent() on a live setting
    // change or an AbstractChatMessagesView Auto-mode resize) never disturbs it.
    content()->setRight(isRight());
    pimpl->contentFrame->setRight(isRight());
    pimpl->avatarFrame->setRight(isRight());
    pimpl->avatarFrame->setSent(direction()==Direction::Sent);

    updateAvatarForced();

    // Only re-slot the selector if it exists; ensureSelector() places it correctly on its own
    // when it is built later.
    if (isSelectorOnLeft() && pimpl->selector!=nullptr)
    {
        pimpl->mainLayout->addWidget(pimpl->selector);
    }

    if (!isRight())
    {
        pimpl->mainLayout->addWidget(pimpl->avatarFrame);
        pimpl->mainLayout->addWidget(pimpl->contentFrame,1);
        pimpl->mainLayout->addWidget(pimpl->avatarFramePlaceholder);
    }
    else
    {
        pimpl->mainLayout->addWidget(pimpl->avatarFramePlaceholder);
        pimpl->mainLayout->addWidget(pimpl->contentFrame,1);
        pimpl->mainLayout->addWidget(pimpl->avatarFrame);
    }

    if (!isSelectorOnLeft() && pimpl->selector!=nullptr)
    {
        pimpl->mainLayout->addWidget(pimpl->selector);
    }

    // Apply the re-ordering above in THIS frame, not on the next posted LayoutRequest.
    // updateAvatarForced() a few lines up shows/hides the avatar through setVisible(), which
    // takes effect immediately, while the addWidget() calls only move the bubble once the layout
    // is re-run -- so on a live flip to left alignment the avatar appeared next to a bubble still
    // drawn on the RIGHT, for the frame or two until that pass came round.
    //
    // Only while already on screen: during a bulk load every row runs this before ever being
    // shown, and forcing a layout pass per row there would be pure waste (the list lays them all
    // out once at the end anyway).
    if (isVisible())
    {
        pimpl->mainLayout->activate();
    }
}

//--------------------------------------------------------------------------

void ChatMessage::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {        
        if (isSelectionMode())
        {
            if (topSeparator() && topSeparator()->underMouse())
            {
                return;
            }

            setSelectDetectionBlocked(true);            
            toggleSelected();
        }
        else
        {
            emit clicked();
        }
    }
    AbstractChatMessage::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessage::showEvent(QShowEvent* event)
{
    AbstractChatMessage::showEvent(event);

    if (pimpl->firstShowSettled)
    {
        return;
    }
    pimpl->firstShowSettled=true;

    // A message built or resized while its page was hidden (e.g. it just arrived in a chat that
    // isn't the current page of a QStackedWidget) can carry a stale ROW HEIGHT into its first
    // real show, even though every widget inside it ends up perfectly sized. Qt's own
    // QLayout::widgetEvent() refuses to activate a hidden widget's layout on a posted
    // QEvent::LayoutRequest ("if (parent()->isVisible()) activate();"), so the whole
    // invalidate-then-activate chain that would normally keep #mainMessageFrame's height in sync
    // with its content sits dormant for as long as the page stays hidden.
    //
    // ChatMessageContentWrapper::showEvent() (this row's own bubble wrapper, shown just before
    // this -- Qt delivers QEvent::Show to children before their parent) already repairs the
    // CONTENT bubble's own size against its now-correct sizeHint(). That in turn leaves
    // #mainMessageFrame's (pimpl->main) freshly-computed sizeHint() correct too -- but nothing
    // yet calls activate() on mainLayout or on this row's own top-level layout to actually RESIZE
    // those widgets to match, so #mainMessageFrame (and hence this whole row, and the tail drawn
    // by ChatMessageAvatar stretching to #mainMessageFrame's height) stays pinned at its old,
    // taller on-screen size for one more frame -- exactly the "bubble detaches from its tail,
    // then snaps into place" glitch. invalidate() before activate() is required: QLayout::
    // activate() returns immediately once already activated. Bottom-up, since QLayout::activate()
    // ends by calling its OWN parent widget's updateGeometry() -- activating mainLayout clears
    // #mainMessageFrame's cached QWidgetItemV2 hint inside pimpl->layout, and activating
    // pimpl->layout in turn clears this row's own cached hint in the enclosing LinkedListView.
    pimpl->mainLayout->invalidate();
    pimpl->mainLayout->activate();
    pimpl->layout->invalidate();
    pimpl->layout->activate();
}

//--------------------------------------------------------------------------

void ChatMessage::updateAvatarVisible()
{
    // Deliberately independent of setAvatarObscured(), which works on opacity and leaves the
    // widget in the layout -- see ChatMessageAvatar::setAvatarObscured()'s own doc comment.
    pimpl->avatarFrame->avatar()->setVisible(isAvatarVisible());
}

//--------------------------------------------------------------------------

void ChatMessage::updateDateTime()
{
    auto c=content();
    if (c && c->bottom())
    {
        QLocale locale;
        auto dt=datetime();
        auto editedDt=editedDatetime();

        // One tooltip shared by the "edited" marker and the time label -- hovering either shows
        // the same thing, which is what makes the marker's own hover useful (it has no text of
        // its own worth explaining). The "Edited" line is present only for a message that
        // actually has an edit datetime; an unedited message keeps a single-line tooltip
        // identical to what this method produced before the marker existed, minus the label.
        auto tooltip=tr("Created: %1").arg(locale.toString(dt,QLocale::LongFormat));
        if (editedDt.isValid())
        {
            tooltip+=QStringLiteral("\n")+tr("Edited: %1").arg(locale.toString(editedDt,QLocale::LongFormat));
        }

        // Empty text hides the marker outright (AbstractChatMessageBottom::setEdited()), so an
        // invalid editedDatetime() is all "never edited" needs to mean here.
        c->bottom()->setEdited(editedDt.isValid() ? tr("edited") : QString(),tooltip);

        auto time=locale.toString(dt.time(), QLocale::ShortFormat);
        c->bottom()->setTimeString(time,tooltip);
    }
}

//--------------------------------------------------------------------------

int ChatMessage::bubbleOuterWidth() const
{
    auto w=horizontalTotalMargin(this);

    w+=pimpl->avatarFrame->minimumWidth();
    w+=pimpl->avatarFramePlaceholder->minimumWidth();
    if (pimpl->selector!=nullptr && pimpl->selector->isVisible())
    {
        w+=pimpl->selector->minimumWidth();
    }

    return w;
}

//--------------------------------------------------------------------------

QWidget* ChatMessage::contentParentWidget()
{
    // Before construct() there is no wrapper yet; fall back to the base behaviour rather than
    // handing out a null parent.
    return pimpl->contentFrame!=nullptr ? static_cast<QWidget*>(pimpl->contentFrame)
                                        : AbstractChatMessage::contentParentWidget();
}

//--------------------------------------------------------------------------

void ChatMessage::setAvatarPath(WithPath path)
{
    pimpl->avatarFrame->avatar()->setAvatarPath(std::move(path));
}

//--------------------------------------------------------------------------

WithPath ChatMessage::avatarPath() const
{
    return pimpl->avatarFrame->avatar()->avatarPath();
}

//--------------------------------------------------------------------------

void ChatMessage::setAvatarSource(std::shared_ptr<AvatarSource> avatarSource)
{
    pimpl->avatarFrame->avatar()->setAvatarSource(std::move(avatarSource));
}

//--------------------------------------------------------------------------

std::shared_ptr<AvatarSource> ChatMessage::avatarSource() const
{
    return pimpl->avatarFrame->avatar()->avatarSource();
}

//--------------------------------------------------------------------------

void ChatMessage::setAvatarName(std::string name)
{
    pimpl->avatarFrame->avatar()->setAvatarName(std::move(name));
}

//--------------------------------------------------------------------------

AvatarWidget* ChatMessage::avatarWidget() const
{
    return pimpl->avatarFrame->avatar();
}

//--------------------------------------------------------------------------

QWidget* ChatMessage::avatarColumnWidget() const
{
    return pimpl->avatarFrame;
}

//--------------------------------------------------------------------------

void ChatMessage::setAvatarObscured(bool obscured)
{
    pimpl->avatarFrame->setAvatarObscured(obscured);
}

//--------------------------------------------------------------------------

QString ChatMessage::selectedText() const
{
    if (!content())
    {
        return QString{};
    }

    // Combine the body's selection with the comment section's own (a forwarded message's
    // sender-comments block, see AbstractChatMessageComment) -- otherwise a selection made only
    // in the comment block would be silently invisible to Copy.
    QString text;
    if (content()->body())
    {
        text=content()->body()->selectedText();
    }
    if (content()->comment())
    {
        auto commentText=content()->comment()->selectedText();
        if (!commentText.isEmpty())
        {
            if (!text.isEmpty())
            {
                text+=QStringLiteral("\n");
            }
            text+=commentText;
        }
    }
    return text;
}

/***************************ChatMessageSelector***************************/

ChatMessageSelector::ChatMessageSelector(QWidget* parent) : AbstractChatMessageSelector(parent)
{
    m_layout=Layout::horizontal(this);

    // No explicit setCursor() here any more -- CheckBox already defaults
    // qproperty-cursorShape to "pointer" (see checkbox.qss), and hardcoding it here would
    // silently defeat that QSS knob for this particular selector.
    m_checkBox=new CheckBox(this);
    m_layout->addWidget(m_checkBox,0,Qt::AlignCenter);

    connect(
        m_checkBox,
        &QAbstractButton::toggled,
        this,
        &ChatMessageSelector::toggled
    );
}

//--------------------------------------------------------------------------

void ChatMessageSelector::setChecked(bool enable)
{
    m_checkBox->setChecked(enable);
}

//--------------------------------------------------------------------------

bool ChatMessageSelector::isChecked() const
{
    return m_checkBox->isChecked();
}

/***************************ChatMessageBottom*****************************/

//--------------------------------------------------------------------------

class ChatMessageBottom_p
{
    public:

        QLabel* time;
        WithRoundedImage* status;
        IconTextButton* seen;
        QLabel* edited;
};

//--------------------------------------------------------------------------

ChatMessageBottom::ChatMessageBottom(QWidget* parent)
    : AbstractChatMessageBottom(parent),
      pimpl(std::make_unique<ChatMessageBottom_p>())
{
    auto l=Layout::horizontal(this);
    l->addStretch(1);

    pimpl->seen=new IconTextButton(Style::instance().svgIconLocator().icon("ChatMessageBottom::seen"),this);
    pimpl->seen->setObjectName("seen");
    l->addWidget(pimpl->seen,0,Qt::AlignRight);
    pimpl->seen->setVisible(false);

    pimpl->edited=new QLabel(this);
    pimpl->edited->setObjectName("edited");
    l->addWidget(pimpl->edited,0,Qt::AlignRight);
    pimpl->edited->setVisible(false);

    pimpl->time=new QLabel(this);
    pimpl->time->setObjectName("time");
    l->addWidget(pimpl->time,0,Qt::AlignRight);

    pimpl->status=new WithRoundedImage(this);
    pimpl->status->setObjectName("status");
    l->addWidget(pimpl->status,0,Qt::AlignRight);
    pimpl->status->setVisible(false);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
}

//--------------------------------------------------------------------------

ChatMessageBottom::~ChatMessageBottom()
{}

//--------------------------------------------------------------------------

void ChatMessageBottom::setTimeString(const QString& time, const QString& tooltip)
{
    pimpl->time->setText(time);
    pimpl->time->setToolTip(tooltip);
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setStatusIcon(std::shared_ptr<SvgIcon> icon, const QString& tooltip)
{
    pimpl->status->setVisible(static_cast<bool>(icon));
    pimpl->status->image()->setSvgIcon(std::move(icon));
    pimpl->status->setToolTip(tooltip);
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setEdited(const QString& text, const QString& tooltip)
{
    pimpl->edited->setText(text);
    pimpl->edited->setToolTip(tooltip);
    pimpl->edited->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setSeen(const QString& text, const QString& tooltip)
{
    pimpl->seen->setText(text);
    pimpl->seen->setToolTip(tooltip);
    pimpl->seen->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------

QSize ChatMessageBottom::sizeHint() const
{
    return QSize{chatContent()->maximumBubbleWidth(),AbstractChatMessageBottom::sizeHint().height()};
}

//--------------------------------------------------------------------------

int ChatMessageBottom::bubbleWidthHint(int forMaxWidth)
{
    // bodyWidthHint() reuses the memo AbstractChatMessageContent::updateBubbleWidth() populated
    // just before this section's own bubbleWidthHint() was called, rather than re-invoking
    // body()->bubbleWidthHint() here -- for a body like ChatMessageImages that call is a full
    // rebuildGrid() re-layout, not a cheap query, so this section used to pay for it twice.
    auto bodyHW=chatContent()->bodyWidthHint(forMaxWidth);
    auto bottomW=AbstractChatMessageBottom::sizeHint().width();

    // A threshold below the bottom's own content width would be self-defeating -- the
    // "wide enough" branch would then hand back a bubble the time/status row itself
    // can't fit into.
    auto narrow=std::max(narrowBodyWidth(),bottomW);

    auto wHint=bodyHW;
    if (bodyHW<narrow)
    {
        // Body too narrow to host the time/status row on its own visual row without
        // looking cramped -- widen the bubble to fit both side by side.
        wHint=bodyHW+bottomW+BottomGap;
    }

    if (wHint>forMaxWidth)
    {
        wHint=forMaxWidth;
    }
    else if (wHint<minimumWidth())
    {
        wHint=minimumWidth();
    }
    return wHint;
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setSelected(bool enable)
{
    // No repolish of `this`: no QSS rule keys on [selected=...] directly on
    // uise--ChatMessageBottom (chat.qss's "uise--ChatMessageBottom QLabel[selected=...]" rule
    // matches the label's OWN property below), and repolishing `this` would re-apply chat.qss's
    // qproperty-narrowBodyWidth default over any programmatic override.
    setProperty("selected",enable);
    Style::setStyleProperty(pimpl->time,"selected",enable);
    Style::setStyleProperty(pimpl->edited,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setSent(bool enable)
{
    // Same reasoning as setSelected() above -- no repolish of `this`.
    setProperty("sent",enable);
    Style::setStyleProperty(pimpl->time,"sent",enable);
    Style::setStyleProperty(pimpl->edited,"sent",enable);
}

/***************************ChatMessageAvatar*****************************/

//--------------------------------------------------------------------------

ChatMessageAvatar::ChatMessageAvatar(QWidget* parent)
    : QFrame(parent)
{
    // One layout, not two: the deleted #mask child used to fill this widget's rect exactly
    // (Layout::horizontal/vertical zero margins and spacing, mask was Expanding), so parenting
    // the avatar straight to `this` leaves its rect unchanged. paintEvent() below draws the tail
    // shape directly instead of relying on #mask's opaque background to fake it.
    auto l=Layout::vertical(this);
    m_avatar=new AvatarWidget(this);
    // Bottom-anchored (stretch above it, tail-clearing inset below it via updateAvatarOffset()):
    // the avatar belongs beside the LAST message of a batch, next to that bubble's tail, not at
    // the top of a tall multi-line row. Centred horizontally so the column's own margins (see
    // ChatMessage::updateAvatarForced(), which makes this column wider than the avatar) fall
    // evenly on both sides.
    l->addStretch(1);
    l->addWidget(m_avatar,0,Qt::AlignHCenter);

    setLastInBatch(true);
    setAvatarSize(DefaultAvatarSize);
    setFixedWidth(DefaultAvatarSize);
    setAvatarBottomOffset(DefaultAvatarBottomOffset);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::updateAvatarOffset()
{
    if (layout()!=nullptr)
    {
        layout()->setContentsMargins(0,0,0,m_avatarBottomOffset);
    }
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::ensureOpacityEffect()
{
    if (m_opacityEffect==nullptr)
    {
        m_opacityEffect=new QGraphicsOpacityEffect(m_avatar);
        m_avatar->setGraphicsEffect(m_opacityEffect);
    }
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setAvatarObscured(bool obscured)
{
    if (m_avatarObscured==obscured)
    {
        return;
    }
    m_avatarObscured=obscured;

    if (!obscured && m_opacityEffect==nullptr)
    {
        // Never obscured, nothing to restore -- do not build the effect just to set it to 1.
        return;
    }

    ensureOpacityEffect();
    m_opacityEffect->setOpacity(obscured ? 0.0 : 1.0);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setRight(bool enable)
{
    // No QSS rule keys on uise--ChatMessageAvatar[right=...] any more -- which side the tail
    // points to is now decided in tailPath() from m_right, so a full repolish is unnecessary
    // here. The dynamic property is still set (deliberately not repolished, same idiom as
    // ChatMessageBottom::setSelected()/setSent() above) so it stays visible to any future QSS.
    if (m_right!=enable)
    {
        m_right=enable;
        setProperty("right",enable);
        update();
    }
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setSelected(bool enable)
{
    setStyleProperty("selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setSent(bool enable)
{
    setStyleProperty("sent",enable);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setLastInBatch(bool enable)
{
    // See setRight() above -- no QSS rule keys on uise--ChatMessageAvatar[last=...] any more,
    // the tail is simply not painted at all when m_last is false (see paintEvent()).
    if (m_last!=enable)
    {
        m_last=enable;
        setProperty("last",enable);
        update();
    }
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setStyleProperty(const char* name, bool enable)
{
    // Only `this` is a repolish target now that #mask is gone -- the tail's colour comes from
    // qproperty-tailColor rules keyed on uise--ChatMessageAvatar[sent=...][selected=...]
    // directly (light/chat.qss, dark/chat.qss). Unlike the old style-engine background fill, a
    // repolish that only changes a qproperty-* value does not by itself schedule a repaint, so
    // ask for one explicitly -- but only once Style::setStyleProperty() confirms it actually
    // repolished, to stay a no-op when the value didn't change.
    if (Style::setStyleProperty(this,name,enable))
    {
        update();
    }
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    // Re-invoke the stylesheet's own frame painting explicitly -- required once a widget
    // overrides paintEvent(), same idiom as AbstractChatMessage::paintEvent() above. Without
    // this, any current/future QSS background/border rule on uise--ChatMessageAvatar would
    // silently stop rendering.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&painter,this);

    if (!m_last || !m_tailColor.isValid())
    {
        return;
    }

    auto path=tailPath();
    if (path.isEmpty())
    {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.fillPath(path,m_tailColor);
}

//--------------------------------------------------------------------------

QPainterPath ChatMessageAvatar::tailPath() const
{
    // Both shapes below are built for a LEFT column (bubble to the right, i.e. beyond this
    // widget's right edge, x>=w) and mirrored horizontally at the end for the right column --
    // reflecting a closed fill path is orientation-agnostic, so one mirror step covers both
    // shapes instead of hand-writing a second set of coordinates for each.
    const qreal w=width();
    const qreal h=height();

    QPainterPath path;

    if (m_tailShape==TailShapeRounded)
    {
        // The original shape this class used to fake with an opaque #mask child painted in the
        // chat-background colour: a concave quarter disc of radius r sitting in the corner of
        // the column, its rounded edge cut out of the corner square exactly the way
        // uise--ChatMessageAvatar[...] #mask's border-radius used to. r is tailHeight (matching
        // uise--AbstractChatMessageContent's own border-radius, so it continues the bubble's
        // corner) clamped to the widget's own rect; tailWidth has no meaning for this shape.
        const qreal r=qMin(static_cast<qreal>(m_tailHeight),qMin(w,h));
        if (r<=0.0)
        {
            return path;
        }

        path.moveTo(w-r,h);
        path.arcTo(QRectF(w-2*r,h-2*r,2*r,2*r),270.0,90.0);
        path.lineTo(w,h);
        path.closeSubpath();
    }
    else
    {
        // Teardrop hook, hooking off the bubble's own square corner:
        //
        //   P0 = (w, h-th)   attachment point, on the bubble's own left edge
        //   C  = (w, h)      the bubble's squared bottom-left corner (chat.qss keeps that
        //                    corner unrounded on the last message in a batch)
        //   T  = (w-tw, h)   the tip
        //
        // P0->C is a straight edge along the bubble. C->T (the underside) is a concave cubic
        // scooped up towards the baseline, leaving a notch of chat background under the tail --
        // that scoop is what reads as a hook rather than a blob. T->P0 (the outer edge) is a
        // convex cubic that bulges back out near the tail's full width before arriving at P0, so
        // the tail flows tangentially into the bubble's edge instead of meeting it at a corner.
        //
        // All four control points are expressed as ratios of tailWidth/tailHeight, not absolute
        // coordinates, so the shape keeps its proportions -- and, in particular, gets uniformly
        // thinner or thicker -- as tailWidth is retuned from QSS; tailWidth is this shape's
        // thickness control (qproperty-tailWidth in chat.qss).
        constexpr static const qreal UnderScoopX1=0.30, UnderScoopY1=0.06;
        constexpr static const qreal UnderScoopX2=0.62, UnderScoopY2=0.16;
        constexpr static const qreal OuterX1=0.95,      OuterY1=0.45;
        constexpr static const qreal OuterX2=0.30,      OuterY2=0.92;

        // The path's straight edge (P0-C) is pushed 1px past the widget's own boundary and
        // relies on the widget's device-pixel-aligned clip to cut it back -- at a fractional
        // device pixel ratio this guarantees the boundary column gets full paint coverage,
        // rather than the partial-coverage antialiasing seam a path landing exactly on the
        // boundary would leave against the bubble's opaque background starting at that same
        // coordinate.
        constexpr static const qreal EdgeOvershoot=1.0;

        const qreal tw=qMin(static_cast<qreal>(m_tailWidth),w);
        const qreal th=qMin(static_cast<qreal>(m_tailHeight),h);
        if (tw<=0.0 || th<=0.0)
        {
            return path;
        }

        path.moveTo(w+EdgeOvershoot,h-th);
        path.lineTo(w+EdgeOvershoot,h);
        path.cubicTo(w-tw*UnderScoopX1, h-th*UnderScoopY1,
                     w-tw*UnderScoopX2, h-th*UnderScoopY2,
                     w-tw,              h);
        path.cubicTo(w-tw*OuterX1, h-th*OuterY1,
                     w-tw*OuterX2, h-th*OuterY2,
                     w,            h-th);
        path.closeSubpath();
    }

    if (m_right)
    {
        // Avatar column on the RIGHT of the bubble: mirror about the column's vertical
        // centerline so the tail attaches to the bubble's right edge (local x=0) instead.
        QTransform t;
        t.translate(w,0);
        t.scale(-1,1);
        path=t.map(path);
    }

    return path;
}

//--------------------------------------------------------------------------

}
