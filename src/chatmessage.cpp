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

#include <QPointer>
#include <QCheckBox>
#include <QMouseEvent>
#include <QLabel>
#include <QLocale>
#include <QResizeEvent>
#include <QTimer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/chatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
#if 0
    qDebug() << "AbstractChatMessageContent::updateBubbleWidth "
                       << " margin="<<horizontalTotalMargin(this)
                       << " padding="<<horizontalTotalPadding(this)
                       << " width()="<<width()
                       << " fullWidth()="<<rect().width()
                       << " contentsWidth()="<<contentsRect().width()
                       << " diff="<<rect().width()-contentsRect().width()
        ;
#endif
    auto forMaxWidth=forMaxWidthIn-horizontalTotalMargin(this)-10;

    int widthHint=0;
    for (auto& section : m_sections)
    {
        auto sectionWidthHint=section->bubbleWidthHint(forMaxWidth);
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

        QFrame* outerRightLine=nullptr;

        QFrame* makeHLine(QWidget* parent, const QString& name) const;
};

//--------------------------------------------------------------------------

QFrame* ChatSeparatorSection_p::makeHLine(QWidget* parent, const QString& name) const
{
    auto f=new QFrame(parent);
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
    pimpl->outerRightLine=pimpl->makeHLine(this,"outerRightLine");
    mainL->addWidget(pimpl->outerRightLine);

    auto l=Layout::horizontal(pimpl->content);

    pimpl->leftLine=pimpl->makeHLine(pimpl->content,"leftLine");
    l->addWidget(pimpl->leftLine,1);

    pimpl->button=new AvatarButton(pimpl->content);
    l->addWidget(pimpl->button,Qt::AlignHCenter);

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
    pimpl->outerRightLine->setVisible(enable);
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
    setProperty("selected",enable);
    Style::updateWidgetStyle(this);
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
    m_content->move(0,0);
    m_content->installEventFilter(this);
    updateGeometry();
}

//--------------------------------------------------------------------------

bool ChatMessageContentWrapper::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_content && event->type() == QEvent::Resize)
    {
        updateGeometry();
#if 0
        m_timer->shot(
            1,
            [this]()
            {
                updateGeometry();
            }
        );
#endif
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

        QFrame* topSpace;

        QFrame* separatorFrame;
        QBoxLayout* separatorLayout;

        QFrame* main;
        QBoxLayout* mainLayout;

        QFrame* avatarFrame;
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
    pimpl->topSpace=new QFrame(this);
    pimpl->topSpace->setObjectName("topSpace");
    pimpl->layout->addWidget(pimpl->topSpace);
    pimpl->topSpace->setVisible(false);

    pimpl->separatorFrame=new QFrame(this);
    pimpl->separatorFrame->setObjectName("separatorFrame");
    pimpl->separatorLayout=Layout::horizontal(pimpl->separatorFrame);
    pimpl->separatorFrame->setVisible(false);
    pimpl->layout->addWidget(pimpl->separatorFrame);

    pimpl->main=new QFrame(this);
    pimpl->layout->addWidget(pimpl->main);

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

    pimpl->avatarFrame=new QFrame(pimpl->main);
    pimpl->avatarFrame->setObjectName("avatarFrame");
    pimpl->avatarFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);

    pimpl->contentFrame=new ChatMessageContentWrapper(pimpl->main);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    Style::updateWidgetStyle(this);
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
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelection()
{
    content()->setSelected(isSelected());
    pimpl->selector->blockSignals(true);
    pimpl->selector->setChecked(isSelected());
    pimpl->selector->blockSignals(false);
}

//--------------------------------------------------------------------------

void ChatMessage::updateTopSpaceVisible()
{
    pimpl->topSpace->setVisible(isTopSpaceVisible());
}

//--------------------------------------------------------------------------

void ChatMessage::updateLastInBatch()
{

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

        auto alignment=Qt::AlignLeft;
        if (direction()==Direction::Sent && alignSent()==AlignSent::Right)
        {
            alignment=Qt::AlignRight;
        }

        if (isSelectorOnLeft())
        {
            pimpl->mainLayout->addWidget(pimpl->selector);
        }

        pimpl->contentFrame->setContent(content());
        if (alignment==Qt::AlignLeft)
        {
            pimpl->mainLayout->addWidget(pimpl->avatarFrame);
            pimpl->mainLayout->addWidget(pimpl->contentFrame,1);
        }
        else
        {
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
        auto time=dt.toString("HH:mm");
        c->bottom()->setTimeString(time,tooltip);
    }
}

//--------------------------------------------------------------------------

int ChatMessage::bubbleOuterWidth() const
{
    auto w=horizontalTotalMargin(this);

    if (pimpl->avatarFrame->isVisible())
    {
        w+=pimpl->avatarFrame->minimumWidth();
    }
    if (pimpl->selector->isVisible())
    {
        w+=pimpl->selector->minimumWidth();
    }

    return w;
}

/***************************ChatMessageSelector***************************/

ChatMessageSelector::ChatMessageSelector(QWidget* parent) : AbstractChatMessageSelector(parent)
{
    m_layout=Layout::horizontal(this);

    m_checkBox=new QCheckBox(this);
    m_checkBox->setCursor(Qt::PointingHandCursor);
    m_layout->addWidget(m_checkBox,0,Qt::AlignCenter);

    connect(
        m_checkBox,
        &QCheckBox::toggled,
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
        IconTextButton* status;
        IconTextButton* seen;
        QLabel* edited;
};

//--------------------------------------------------------------------------

ChatMessageBottom::ChatMessageBottom(QWidget* parent)
    : AbstractChatMessageBottom(parent),
      pimpl(std::make_unique<ChatMessageBottom_p>())
{
    auto l=Layout::horizontal(this);

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

    pimpl->status=new IconTextButton(this);
    pimpl->status->setObjectName("status");
    l->addWidget(pimpl->status,0,Qt::AlignRight);
    pimpl->status->setVisible(false);
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
    pimpl->status->setSvgIcon(std::move(icon));
    pimpl->status->setVisible(static_cast<bool>(icon));
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
    auto bodyHW=chatContent()->body()->bubbleWidthHint(forMaxWidth);
    auto wHint=bodyHW+AbstractChatMessageBottom::sizeHint().width();
    if (wHint>forMaxWidth)
    {
        wHint=forMaxWidth;
    }
    else if (wHint<minimumWidth())
    {
        wHint=minimumWidth();
    }

#if 0
    qDebug() << "ChatMessageBottom::bubbleWidthHint"
                          <<" forMaxWidth="<<forMaxWidth
                          <<" bodyHW="<<bodyHW
                          <<" AbstractChatMessageBottom::sizeHint().width()="<<AbstractChatMessageBottom::sizeHint().width()
                          <<" wHint="<<wHint;
#endif
    return wHint;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
