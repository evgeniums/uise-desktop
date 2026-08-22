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

/** @file uise/desktop/src/chatmessageforwardheader.cpp
*
*  Defines ChatMessageForwardHeader.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/chatmessageforwardheader.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Mirrors AbstractReplyPreview::maxWidthHint's own default -- see this section's own
//! Q_PROPERTY doc comment.
constexpr int DefaultMaxWidthHint=320;

}

//--------------------------------------------------------------------------

class ChatMessageForwardHeader_p
{
    public:

        QBoxLayout* layout=nullptr;

        ElidedLabel* prefixLabel=nullptr;
        //! The clickable author-title portion -- see ChatMessageForwardHeader::eventFilter().
        ElidedLabel* authorLabel=nullptr;
        ElidedLabel* suffixLabel=nullptr;

        QString authorTitle;
        QString authorId;
        QString titleFormat;
        bool authorClickable=true;
        int maxWidthHint=DefaultMaxWidthHint;

        //! Matches ReplyPreview's own latch: a press only marks the label down, letting a press
        //! dragged out before release cancel the click -- see ReplyPreview::mousePressEvent()/
        //! mouseReleaseEvent().
        bool pressed=false;
};

//--------------------------------------------------------------------------

ChatMessageForwardHeader::ChatMessageForwardHeader(QWidget* parent)
    : AbstractChatMessageHeader(parent),
      pimpl(std::make_unique<ChatMessageForwardHeader_p>())
{
    pimpl->titleFormat=tr("Forwarded from %1");

    pimpl->layout=Layout::horizontal(this);

    pimpl->prefixLabel=new ElidedLabel(this);
    pimpl->prefixLabel->setObjectName("forwardPrefix");
    pimpl->prefixLabel->setElideMode(Qt::ElideRight);
    pimpl->prefixLabel->setMaxLines(1);
    pimpl->layout->addWidget(pimpl->prefixLabel);

    // Gets the stretch factor -- the author's title is the variable-length part most likely to
    // need eliding; prefix/suffix are short, static strings taken from titleFormat().
    pimpl->authorLabel=new ElidedLabel(this);
    pimpl->authorLabel->setObjectName("forwardAuthor");
    pimpl->authorLabel->setElideMode(Qt::ElideRight);
    pimpl->authorLabel->setMaxLines(1);
    pimpl->authorLabel->installEventFilter(this);
    pimpl->layout->addWidget(pimpl->authorLabel,1);

    pimpl->suffixLabel=new ElidedLabel(this);
    pimpl->suffixLabel->setObjectName("forwardSuffix");
    pimpl->suffixLabel->setElideMode(Qt::ElideRight);
    pimpl->suffixLabel->setMaxLines(1);
    pimpl->layout->addWidget(pimpl->suffixLabel);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);

    setAuthorClickable(true);
    refresh();
}

//--------------------------------------------------------------------------

ChatMessageForwardHeader::~ChatMessageForwardHeader()
{}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setAuthorTitle(QString title)
{
    pimpl->authorTitle=std::move(title);
    refresh();
}

//--------------------------------------------------------------------------

QString ChatMessageForwardHeader::authorTitle() const
{
    return pimpl->authorTitle;
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setAuthorId(QString id)
{
    pimpl->authorId=std::move(id);
}

//--------------------------------------------------------------------------

QString ChatMessageForwardHeader::authorId() const
{
    return pimpl->authorId;
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setTitleFormat(const QString& format)
{
    pimpl->titleFormat=format;
    refresh();
}

//--------------------------------------------------------------------------

QString ChatMessageForwardHeader::titleFormat() const
{
    return pimpl->titleFormat;
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setAuthorClickable(bool enable)
{
    pimpl->authorClickable=enable;
    pimpl->authorLabel->setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (!enable)
    {
        pimpl->pressed=false;
        pimpl->authorLabel->setProperty("hovered",false);
        Style::updateWidgetStyle(pimpl->authorLabel);
    }
}

//--------------------------------------------------------------------------

bool ChatMessageForwardHeader::isAuthorClickable() const
{
    return pimpl->authorClickable;
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setMaxWidthHint(int width) noexcept
{
    pimpl->maxWidthHint=width;
}

//--------------------------------------------------------------------------

int ChatMessageForwardHeader::maxWidthHint() const noexcept
{
    return pimpl->maxWidthHint;
}

//--------------------------------------------------------------------------

int ChatMessageForwardHeader::bubbleWidthHint(int forMaxWidth)
{
    if (pimpl->maxWidthHint<=0)
    {
        return 0;
    }

    int textWidth=0;
    if (pimpl->prefixLabel->isVisible())
    {
        textWidth+=pimpl->prefixLabel->widthHint();
    }
    if (pimpl->authorLabel->isVisible())
    {
        textWidth+=pimpl->authorLabel->widthHint();
    }
    if (pimpl->suffixLabel->isVisible())
    {
        textWidth+=pimpl->suffixLabel->widthHint();
    }

    auto avail=forMaxWidth-horizontalTotalMargin(this);
    auto natural=qMin(textWidth,pimpl->maxWidthHint);
    natural=qMin(natural,avail);

    return natural+horizontalTotalMargin(this);
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::updateMaximumBubbleWidth()
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

void ChatMessageForwardHeader::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
}

//--------------------------------------------------------------------------

bool ChatMessageForwardHeader::eventFilter(QObject* obj, QEvent* event)
{
    if (obj==pimpl->authorLabel && pimpl->authorClickable)
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
                    if (pimpl->authorLabel->rect().contains(me->pos()))
                    {
                        emit authorClicked();
                    }
                }
                break;
            }

            case QEvent::Enter:
            {
                pimpl->authorLabel->setProperty("hovered",true);
                Style::updateWidgetStyle(pimpl->authorLabel);
                break;
            }

            case QEvent::Leave:
            {
                pimpl->pressed=false;
                pimpl->authorLabel->setProperty("hovered",false);
                Style::updateWidgetStyle(pimpl->authorLabel);
                break;
            }

            default:
                break;
        }
    }
    return AbstractChatMessageHeader::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

void ChatMessageForwardHeader::refresh()
{
    // Split, not substitute -- unlike ReplyPreview::refresh()'s QString::arg() (which fills the
    // WHOLE title into one non-interactive label), this needs the author portion isolated in its
    // own label so only it is clickable, see setTitleFormat()'s doc comment for the three cases.
    const auto& format=pimpl->titleFormat;
    auto idx=format.indexOf(QLatin1String("%1"));

    QString prefix;
    QString suffix;
    bool showAuthor=false;

    if (idx<0)
    {
        // No %1 at all -- render literally, no clickable region.
        prefix=format;
    }
    else
    {
        prefix=format.left(idx);
        suffix=format.mid(idx+2);
        showAuthor=true;
    }

    pimpl->prefixLabel->setText(prefix);
    pimpl->prefixLabel->setVisible(!prefix.isEmpty());

    pimpl->authorLabel->setText(pimpl->authorTitle);
    pimpl->authorLabel->setVisible(showAuthor);

    pimpl->suffixLabel->setText(suffix);
    pimpl->suffixLabel->setVisible(showAuthor && !suffix.isEmpty());

    updateGeometry();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
