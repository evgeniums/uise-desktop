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
#include <QTimer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/chatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
    if (header()!=nullptr)
    {
        m_layout->addWidget(header(),0,Qt::AlignLeft);
    }
    if (body()!=nullptr)
    {
        m_layout->addWidget(body(),0,Qt::AlignLeft);
    }
    if (bottom()!=nullptr)
    {
        m_layout->addWidget(bottom(),0,Qt::AlignLeft);
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
    if (body()!=nullptr)
    {
        body()->clearContentSelection();
    }
    if (bottom()!=nullptr)
    {
        bottom()->clearContentSelection();
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
    if (bottom())
    {
        bottom()->setSelected(enable);
    }
    if (header())
    {
        header()->setSelected(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
    if (bottom())
    {
        bottom()->setSent(enable);
    }
    if (header())
    {
        header()->setSent(enable);
    }
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

    // No repolish here: no QSS rule anywhere keys on a "right" property set on
    // uise--AbstractChatMessageContent, so a repolish would only cost a full stylesheet
    // re-match for nothing.
    setProperty("right",chatMessage()->isRight());
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
    m_timer=new SingleShotTimer(this);
}

//--------------------------------------------------------------------------

void ChatMessageContentWrapper::setContent(AbstractChatMessageContent* content)
{
    m_content=content;
    m_content->setParent(this);
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
    if (m_right)
    {
        m_content->move(width()+contentsMargins().left()-m_content->width(),0);
    }
    else
    {
        m_content->move(contentsMargins().left(),0);
    }
}

//--------------------------------------------------------------------------

bool ChatMessageContentWrapper::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_content && event->type() == QEvent::Resize)
    {
        updatePosition();
        updateGeometry();
        m_timer->shot(
            1,
            [this]()
            {
                updatePosition();
                updateGeometry();
            }
        );
    }
    return QFrame::eventFilter(obj, event);
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

        ChatMessageContentWrapper* contentFrame;
        QBoxLayout* contentLayout;

        AbstractChatMessageSelector* selector;
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
    pimpl->selector->setVisible(false);

    pimpl->avatarFrame=new ChatMessageAvatar(pimpl->main);
    pimpl->avatarFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    pimpl->avatarFrame->avatar()->setVisible(false);

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

void ChatMessage::updateTopSeparator()
{
    bool sepVisible=topSeparator()!=nullptr;
    pimpl->separatorFrame->setVisible(sepVisible);
    if (sepVisible)
    {
        pimpl->separatorLayout->addWidget(topSeparator());
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelectionMode()
{
    pimpl->selector->setVisible(isSelectionMode());
    if (!isSelectionMode())
    {
        pimpl->selector->blockSignals(true);
        pimpl->selector->setChecked(false);
        pimpl->selector->blockSignals(false);
    }
    QTimer::singleShot(10,this,
    [this](){
        pimpl->contentFrame->updateGeometry();
        pimpl->contentFrame->updatePosition();

        pimpl->main->updateGeometry();
    });
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelection()
{
    content()->setSelected(isSelected());
    pimpl->avatarFrame->setSelected(isSelected());

    pimpl->selector->blockSignals(true);
    pimpl->selector->setChecked(isSelected());
    pimpl->selector->blockSignals(false);
}

//--------------------------------------------------------------------------

void ChatMessage::updateLastInBatch()
{
    pimpl->avatarFrame->setLastInBatch(isLastInBatch());
    pimpl->bottomSpace->setVisible(isLastInBatch());

    // No repolish of `this` here: chat.qss puts qproperty-selectorPositionLeft on
    // uise--AbstractChatMessage, and a repolish would re-apply that default over any
    // programmatic setSelectorOnLeft() -- and no QSS rule keys on [last=...] on this widget
    // itself anyway (only on the content bubble and the avatar, both updated separately above).
    setProperty("last",isLastInBatch());
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

        pimpl->contentFrame->setRight(isRight());
        pimpl->avatarFrame->setRight(isRight());
        pimpl->avatarFrame->setSent(isRight());

        if (isSelectorOnLeft())
        {
            pimpl->mainLayout->addWidget(pimpl->selector);
        }

        pimpl->contentFrame->setContent(content());
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

        if (!isSelectorOnLeft())
        {
            pimpl->mainLayout->addWidget(pimpl->selector);
        }
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

void ChatMessage::updateAvatarVisible()
{
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
        auto tooltip=locale.toString(dt, QLocale::LongFormat);
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
    if (pimpl->selector->isVisible())
    {
        w+=pimpl->selector->minimumWidth();
    }

    return w;
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

QString ChatMessage::selectedText() const
{
    if (content() && content()->body())
    {
        return content()->body()->selectedText();
    }
    return QString{};
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
    auto l=Layout::horizontal(this);
    m_mask=new QFrame(this);
    m_mask->setObjectName("mask");
    m_mask->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    l->addWidget(m_mask);

    m_layout=Layout::vertical(m_mask);
    m_avatar=new AvatarWidget(m_mask);
    m_layout->addWidget(m_avatar);

    setStyleProperty("last",true);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setRight(bool enable)
{
    setStyleProperty("right",enable);
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
    setStyleProperty("last",enable);
}

//--------------------------------------------------------------------------

void ChatMessageAvatar::setStyleProperty(const char* name, bool enable)
{
    // Both `this` and #mask are repolish targets: chat.qss has rules keyed on
    // uise--ChatMessageAvatar[sent=...][last=...] directly (styling `this`) as well as on
    // uise--ChatMessageAvatar[...] #mask (styling the child) -- unlike setSelected()/setSent()
    // above, neither repolish here is redundant. setStyleProperty() guards each independently;
    // since both are always set to the same value in lockstep, that is equivalent to a combined
    // guard.
    Style::setStyleProperty(this,name,enable);
    Style::setStyleProperty(m_mask,name,enable);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
