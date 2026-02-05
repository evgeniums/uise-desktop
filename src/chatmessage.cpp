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
#include <QTextBrowser>

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatarbutton.hpp>
#include <uise/desktop/alignedstretchingwidget.hpp>
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
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateHeader()
{
    if (header()!=0)
    {
        m_layout->insertWidget(0,header());
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateBody()
{
    if (body()!=0)
    {
        m_layout->insertWidget(0,body());
    }
}

//--------------------------------------------------------------------------

void ChatMessageContent::updateBottom()
{
    if (bottom()!=0)
    {
        m_layout->insertWidget(0,bottom());
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

        AlignedStretchingWidget* body;
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

#if 0
    pimpl->body=new AlignedStretchingWidget(this);
    pimpl->layout->addWidget(pimpl->body);
    pimpl->main=new QFrame(pimpl->body);
#else
    pimpl->main=new QFrame(this);
    pimpl->layout->addWidget(pimpl->main);
#endif

    pimpl->main->setObjectName("main");
    pimpl->mainLayout=Layout::horizontal(pimpl->main);

    pimpl->avatarFrame=new QFrame(pimpl->main);
    pimpl->avatarFrame->setObjectName("avatarFrame");
    pimpl->avatarFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    pimpl->mainLayout->addWidget(pimpl->avatarFrame);

    pimpl->contentFrame=new QFrame(pimpl->main);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::vertical(pimpl->contentFrame);
    pimpl->mainLayout->addWidget(pimpl->contentFrame);

    pimpl->selectionFrame=new QFrame(pimpl->main);
    pimpl->selectionFrame->setObjectName("selectionFrame");
    pimpl->selectionFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    pimpl->mainLayout->addWidget(pimpl->selectionFrame);
#if 0
    pimpl->body->setWidget(pimpl->main,Qt::Horizontal,Qt::AlignLeft);
#endif

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

void ChatMessage::updateAlignSent()
{
    updateAlignment();
}

//--------------------------------------------------------------------------

void ChatMessage::updateDirection()
{
    updateAlignment();
}

//--------------------------------------------------------------------------

void ChatMessage::updateAlignment()
{
    auto alignment=Qt::AlignLeft;
    if (direction()==Direction::Sent && alignSent()==AlignSent::Right)
    {
        alignment=Qt::AlignRight;
    }
    pimpl->body->setAlignment(alignment);
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
    pimpl->body->setVisible(isContentVisible());
}

//--------------------------------------------------------------------------

void ChatMessage::updateContent()
{
    if (content()!=nullptr)
    {
        pimpl->contentLayout->addWidget(content());
    }
}

//--------------------------------------------------------------------------

void ChatMessage::updateAvatarVisible()
{

}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
