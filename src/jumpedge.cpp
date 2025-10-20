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

/** @file uise/desktop/src/jumpedge.cpp
*
*  Defines JumpEdge.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QPainter>
#include <QEnterEvent>
#include <QPalette>
#include <QStaticText>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/jumpedge.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** JumpEdge ***********************************/

JumpEdge::JumpEdge(QWidget* parent)
    : QFrame(parent),
      m_orientation(Qt::Vertical),
      m_direction(Direction::HOME),
      m_iconDirection(IconDirection::Up)
{
    m_badgeText=new QLabel(this);
    m_badgeText->setObjectName("badge");
    m_badgeText->setVisible(false);

    m_sample=new QFrame(this);
    m_sample->setObjectName("sample");
    m_sample->setVisible(false);

    m_clickTimer=new SingleShotTimer(this);

    setCursor(Qt::PointingHandCursor);
    updateIcon();
}

//--------------------------------------------------------------------------

void JumpEdge::paintEvent(QPaintEvent *event)
{
    QPainter p;
    p.begin(this);

    auto r=rect();

    // draw cirvle background
    auto w=r.width();
    auto circleWidth=w;
    auto outerWidth=w-circleWidth;
    auto halfOuterWidth=outerWidth/2;
    QRect circleRect{r.left()+halfOuterWidth,r.bottom()-halfOuterWidth-circleWidth,circleWidth,circleWidth};
    auto color=m_sample->palette().color(QPalette::Window);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.setRenderHints(QPainter::Antialiasing);
    p.drawEllipse(circleRect);
    p.setBrush(Qt::NoBrush);

    // draw icon
    if (m_icon)
    {
        // encode configurable parameters in QSS using properties:
        //  *min-width is for base padding of icon
        //  *padding is for padding tuning depending on current icon direction
        auto m=m_sample->contentsMargins();
        auto basePadding=m_sample->minimumWidth()-m.left()-m.right();

        auto iconOuterWidth=2*basePadding;
        auto iconWidth=w-iconOuterWidth;

        auto leftPadding=basePadding;
        auto bottomPadding=basePadding;

        switch (m_iconDirection)
        {
            case (IconDirection::Up):
            {
                bottomPadding=m.top();
            }
            break;
            case (IconDirection::Down):
            {
                bottomPadding=m.bottom();
                // bottomPadding=directedPadding;
            }
            break;
            case (IconDirection::Left):
            {
                leftPadding=m.left();
                // leftPadding=directedPadding;
            }
            break;
            case (IconDirection::Right):
            {
                leftPadding=m.right();
            }
            break;
        }

        QRect iconRect{r.left()+leftPadding,r.bottom()-iconWidth-bottomPadding,iconWidth,iconWidth};

        // qDebug() << "JumpEdge::paintEvent rect=" << r << " circleRect="<<circleRect << " iconRect="<<iconRect
        //          << " margins="<<m << " outerWidth="<<outerWidth
        //          << " iconRect.left()="<<iconRect.left()<< " iconRect.top()="<<iconRect.top()
        //          << " iconRect.size()="<<iconRect.size()
        //                    <<  " basePadding="<<basePadding
        //                    << " mins="<<m_sample->minimumSize();

        auto mode=IconMode::Normal;
        if (m_hovered)
        {
            mode=IconMode::Hovered;
        }
        m_icon->paint(&p,iconRect,mode,QIcon::Off,false);
    }

    // draw badge text
    renderBadgeText(p);

    // done
    p.end();
}

//--------------------------------------------------------------------------

void JumpEdge::updateIcon()
{
    QString name;

    if (m_orientation==Qt::Orientation::Vertical)
    {
        if (m_direction==Direction::HOME)
        {
            name="up";
            m_iconDirection=IconDirection::Up;
        }
        else
        {
            name="down";
            m_iconDirection=IconDirection::Down;
        }
    }
    else
    {
        if (m_direction==Direction::HOME)
        {
            name="left";
            m_iconDirection=IconDirection::Left;
        }
        else
        {
            name="right";
            m_iconDirection=IconDirection::Right;
        }
    }

    m_icon=Style::instance().svgIconLocator().icon(QString("JumpEdge::%1").arg(name),this);
    update();
}

//--------------------------------------------------------------------------

void JumpEdge::enterEvent(QEnterEvent* event)
{
    m_hovered=true;
    QFrame::enterEvent(event);
    update();
}

//--------------------------------------------------------------------------

void JumpEdge::leaveEvent(QEvent* event)
{
    m_hovered=false;
    QFrame::leaveEvent(event);
    update();
}

//--------------------------------------------------------------------------

void JumpEdge::setBadgeText(const QString& text)
{
    m_badgeText->setText(text);
    update();
}

//--------------------------------------------------------------------------

QString JumpEdge::badgeText() const
{
    return m_badgeText->text();
}

//--------------------------------------------------------------------------

void JumpEdge::clearBadgeText()
{
    m_badgeText->clear();
    update();
}

//--------------------------------------------------------------------------

void JumpEdge::mousePressEvent(QMouseEvent* event)
{
    QFrame::mousePressEvent(event);
    if (event->button()==Qt::LeftButton)
    {
        m_clickTimer->shot(50,
           [this]()
           {
              emit clicked();
           }
        );
    }
}

//--------------------------------------------------------------------------

void JumpEdge::renderBadgeText(QPainter& painter)
{
    auto badgeText=m_badgeText->text();
    if (!badgeText.isEmpty())
    {
        QColor fontColor=m_badgeText->palette().color(QPalette::Text);
        QFont font=m_badgeText->font();

        auto r=rect();
        auto w=r.width();

        QFontMetrics metrics(font);
        auto br=metrics.tightBoundingRect(badgeText);
        auto bbr=metrics.boundingRect(badgeText);
        auto fw=std::min(br.width(),bbr.width());
        auto fh=br.height();
        auto dy=metrics.ascent()-br.height();
        auto x= w/2 - qCeil(fw/2);
        auto bearing=metrics.leftBearing(badgeText[0]);
        auto textLeft=x-bearing;

        // draw background circle
        auto m=m_badgeText->contentsMargins();
        auto circleWidth=std::max(fw+m.left()+m.right(),fh+m.top()+m.bottom());
        auto minCircleWidth=m_badgeText->minimumWidth()+m.left()+m.right();
        if (circleWidth<minCircleWidth)
        {
            circleWidth=minCircleWidth;
        }
        if (circleWidth>w)
        {
            circleWidth=w;
        }
        auto outerWidth=w-circleWidth;
        auto halfOuterWidth=outerWidth/2;
        auto circleLeft=r.left()+halfOuterWidth;
        auto circleTop=r.top();
        QRect circleRect{circleLeft,circleTop,circleWidth,circleWidth};
        auto color=m_badgeText->palette().color(QPalette::Base);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.setRenderHints(QPainter::Antialiasing);
        painter.drawEllipse(circleRect);

        // draw text
        painter.setBrush(Qt::NoBrush);
        painter.setPen(fontColor);
        painter.setFont(font);
        painter.setRenderHints(QPainter::TextAntialiasing);
        auto textTop=r.top()+(circleWidth-fh)/2 -dy;
        painter.drawStaticText(textLeft,textTop,QStaticText{badgeText});
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
