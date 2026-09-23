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

/** @file uise/desktop/src/chatmessagesenderheader.cpp
*
*  Defines ChatMessageSenderHeader.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/chatmessagesenderheader.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Mirrors ChatMessageForwardHeader::maxWidthHint's own default -- see this section's own
//! Q_PROPERTY doc comment.
constexpr int DefaultMaxWidthHint=320;

}

//--------------------------------------------------------------------------

class ChatMessageSenderHeader_p
{
    public:

        QBoxLayout* layout=nullptr;

        //! The one and only label -- see this class's own doc comment for why there is no
        //! prefix/suffix split the way ChatMessageForwardHeader needs.
        ElidedLabel* nameLabel=nullptr;

        bool clickable=true;
        int maxWidthHint=DefaultMaxWidthHint;

        //! Matches ChatMessageForwardHeader's own latch: a press only marks the label down,
        //! letting a press dragged out before release cancel the click.
        bool pressed=false;
};

//--------------------------------------------------------------------------

ChatMessageSenderHeader::ChatMessageSenderHeader(QWidget* parent)
    : AbstractChatMessageSenderHeader(parent),
      pimpl(std::make_unique<ChatMessageSenderHeader_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->nameLabel=new ElidedLabel(this);
    pimpl->nameLabel->setObjectName("senderTitle");
    pimpl->nameLabel->setElideMode(Qt::ElideRight);
    pimpl->nameLabel->setMaxLines(1);
    pimpl->nameLabel->installEventFilter(this);
    pimpl->layout->addWidget(pimpl->nameLabel,1);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    setClickable(true);
    // No sender title yet -- an empty name renders nothing, same convention as
    // ChatMessageForwardHeader::refresh()'s no-author case.
    pimpl->nameLabel->setVisible(false);
}

//--------------------------------------------------------------------------

ChatMessageSenderHeader::~ChatMessageSenderHeader()
{}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::setSenderTitle(QString title)
{
    pimpl->nameLabel->setText(title);
    pimpl->nameLabel->setVisible(!title.isEmpty());
    updateGeometry();
}

//--------------------------------------------------------------------------

QString ChatMessageSenderHeader::senderTitle() const
{
    return pimpl->nameLabel->text();
}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::setClickable(bool enable)
{
    pimpl->clickable=enable;
    pimpl->nameLabel->setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (!enable)
    {
        pimpl->pressed=false;
        pimpl->nameLabel->setProperty("hovered",false);
        Style::updateWidgetStyle(pimpl->nameLabel);
    }
}

//--------------------------------------------------------------------------

bool ChatMessageSenderHeader::isClickable() const
{
    return pimpl->clickable;
}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::setMaxWidthHint(int width) noexcept
{
    pimpl->maxWidthHint=width;
}

//--------------------------------------------------------------------------

int ChatMessageSenderHeader::maxWidthHint() const noexcept
{
    return pimpl->maxWidthHint;
}

//--------------------------------------------------------------------------

int ChatMessageSenderHeader::bubbleWidthHint(int forMaxWidth)
{
    if (pimpl->maxWidthHint<=0)
    {
        return 0;
    }

    // isVisibleTo(this), NOT isVisible() -- see ChatMessageForwardHeader::bubbleWidthHint()'s
    // own doc comment for why: a message widget is negotiated via this very call before it is
    // ever inserted into the visible tree, where QWidget::isVisible() would always read false.
    int textWidth=0;
    if (pimpl->nameLabel->isVisibleTo(this))
    {
        textWidth+=pimpl->nameLabel->widthHint();
    }

    auto avail=forMaxWidth-horizontalTotalMargin(this);
    auto natural=qMin(textWidth,pimpl->maxWidthHint);
    natural=qMin(natural,avail);

    return natural+horizontalTotalMargin(this);
}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::updateMaximumBubbleWidth()
{
    auto width=chatContent()->maximumBubbleWidth()-horizontalTotalMargin(this);
    if (pimpl->maxWidthHint>0 && width>pimpl->maxWidthHint)
    {
        width=pimpl->maxWidthHint;
    }
    setMaximumWidth(qMax(0,width)+horizontalTotalMargin(this));
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageSenderHeader::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
}

//--------------------------------------------------------------------------

bool ChatMessageSenderHeader::eventFilter(QObject* obj, QEvent* event)
{
    if (obj==pimpl->nameLabel && pimpl->clickable)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                auto* me=static_cast<QMouseEvent*>(event);
                if (me->button()==Qt::LeftButton)
                {
                    pimpl->pressed=true;
                }
                break;
            }

            case QEvent::MouseButtonRelease:
            {
                auto* me=static_cast<QMouseEvent*>(event);
                if (me->button()==Qt::LeftButton && pimpl->pressed)
                {
                    pimpl->pressed=false;
                    if (pimpl->nameLabel->rect().contains(me->pos()))
                    {
                        emit clicked();
                    }
                }
                break;
            }

            case QEvent::Enter:
            {
                pimpl->nameLabel->setProperty("hovered",true);
                Style::updateWidgetStyle(pimpl->nameLabel);
                break;
            }

            case QEvent::Leave:
            {
                pimpl->pressed=false;
                pimpl->nameLabel->setProperty("hovered",false);
                Style::updateWidgetStyle(pimpl->nameLabel);
                break;
            }

            default:
                break;
        }
    }
    return AbstractChatMessageSenderHeader::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

}
