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

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/chatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
        QFrame* contentFrame;
        QBoxLayout* contentLayout;

        QFrame* selectionFrame;
};

//--------------------------------------------------------------------------

ChatMessage::ChatMessage(QWidget* parent)
    : AbstractChatMessage(parent),
      pimpl(std::make_unique<ChatMessage_p>())
{
    pimpl->layout=Layout::vertical(this);

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

    pimpl->main->setObjectName("main");
    pimpl->mainLayout=Layout::horizontal(pimpl->main);

    pimpl->avatarFrame=new QFrame(pimpl->main);
    pimpl->avatarFrame->setObjectName("avatarFrame");
    pimpl->avatarFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);

    pimpl->contentFrame=new QFrame(pimpl->main);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::horizontal(pimpl->contentFrame);

    pimpl->selectionFrame=new QFrame(pimpl->main);
    pimpl->selectionFrame->setObjectName("selectionFrame");
    pimpl->selectionFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessage::~ChatMessage()
{}

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

void ChatMessage::updateSelectable()
{
}

//--------------------------------------------------------------------------

void ChatMessage::updateSelection()
{
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
        auto alignment=Qt::AlignLeft;
        if (direction()==Direction::Sent && alignSent()==AlignSent::Right)
        {
            alignment=Qt::AlignRight;
        }

        if (alignment==Qt::AlignLeft)
        {
            pimpl->mainLayout->addWidget(pimpl->avatarFrame);
            pimpl->mainLayout->addWidget(pimpl->contentFrame);
            pimpl->contentLayout->addWidget(content());
            pimpl->contentLayout->addStretch(1);
        }
        else
        {
            pimpl->contentLayout->addStretch(1);
            pimpl->contentLayout->addWidget(content());
            pimpl->mainLayout->addWidget(pimpl->contentFrame);
            pimpl->mainLayout->addWidget(pimpl->avatarFrame);
        }

        pimpl->mainLayout->addWidget(pimpl->selectionFrame);
    }
}

//--------------------------------------------------------------------------

void ChatMessage::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessage::updateAvatarVisible()
{

}

//--------------------------------------------------------------------------

void ChatMessage::updateDateTime()
{
    if (content() && content()->bottom())
    {
        QLocale locale;
        auto dt=datetime();
        auto tooltip=locale.toString(dt, QLocale::LongFormat);
        auto time=dt.toString("HH:mm");
        content()->bottom()->setTimeString(time,tooltip);
    }
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
    adjustWidth();
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setStatusIcon(std::shared_ptr<SvgIcon> icon, const QString& tooltip)
{
    pimpl->status->setSvgIcon(std::move(icon));
    pimpl->status->setVisible(static_cast<bool>(icon));
    pimpl->status->setToolTip(tooltip);
    adjustWidth();
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setEdited(const QString& text, const QString& tooltip)
{
    pimpl->edited->setText(text);
    pimpl->edited->setToolTip(tooltip);
    pimpl->edited->setVisible(!text.isEmpty());
    adjustWidth();
}

//--------------------------------------------------------------------------

void ChatMessageBottom::setSeen(const QString& text, const QString& tooltip)
{
    pimpl->seen->setText(text);
    pimpl->seen->setToolTip(tooltip);
    pimpl->seen->setVisible(!text.isEmpty());
    adjustWidth();
}

//--------------------------------------------------------------------------

void ChatMessageBottom::adjustWidth()
{
    auto bodyHW=chatContent()->body()->sizeHint().width();
    auto maxW=chatContent()->maximumWidth();

    auto newW1=bodyHW+sizeHint().width();
    auto newW=newW1;

    int contentPadding = chatContent()->getUisePadding();
    auto maxW1=maxW-2*contentPadding;
    if (maxW1>0 && newW>maxW1)
    {
        newW=maxW1;
    }

#if 0
    qDebug() << "ChatMessageBottom::adjustWidth() "
                       << " bodyHW="<<bodyHW
                       << " maxW="<<maxW
                       << " contentPadding="<<contentPadding
                       << " maxW1="<<maxW1
                       << " newW1="<<newW1
                       << " newW="<<newW;
#endif
    setMinimumWidth(newW);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
