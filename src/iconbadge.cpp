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

/** @file uise/desktop/src/iconbadge.cpp
*
*  Defines IconBadge.
*
*/

/****************************************************************************/

#include <algorithm>
#include <vector>

#include <QEvent>
#include <QPointer>
#include <QRect>

#include <uise/desktop/iconbadge.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** IconBadge_p ***********************************/

class IconBadge_p
{
    public:

        QPointer<QWidget> anchor;
        QPointer<QWidget> host;

        // Widgets this badge currently has an event filter installed on: the anchor, every
        // ancestor up to (not including) the host, and the host itself.
        std::vector<QPointer<QWidget>> watched;

        int anchorSize=IconBadge::DefaultAnchorSize;
        int offsetX=IconBadge::DefaultOffsetX;
        int offsetY=IconBadge::DefaultOffsetY;

        bool repositioning=false;
};

/**************************** IconBadge *************************************/

IconBadge::IconBadge(QWidget* anchor, QWidget* host)
    : CountBadge(host),
      pimpl(std::make_unique<IconBadge_p>())
{
    setObjectName("iconBadge");
    setAttribute(Qt::WA_TransparentForMouseEvents);

    pimpl->anchor=anchor;
    pimpl->host=host;

    setVisible(false);
    rewatch();
}

IconBadge::~IconBadge()=default;

//--------------------------------------------------------------------------

void IconBadge::setHost(QWidget* host)
{
    if (pimpl->host==host)
    {
        return;
    }
    pimpl->host=host;
    // setParent() hides the widget; reposition() below shows it again when due. A null host
    // would turn it into a top-level window, so it stays hidden and reposition() bails out.
    setParent(host);
    setVisible(false);
    rewatch();
    reposition();
}

QWidget* IconBadge::host() const
{
    return pimpl->host;
}

//--------------------------------------------------------------------------

void IconBadge::setAnchor(QWidget* anchor)
{
    if (pimpl->anchor==anchor)
    {
        return;
    }
    pimpl->anchor=anchor;
    rewatch();
    reposition();
}

QWidget* IconBadge::anchor() const
{
    return pimpl->anchor;
}

//--------------------------------------------------------------------------

void IconBadge::setAnchorSize(int val)
{
    if (pimpl->anchorSize==val)
    {
        return;
    }
    pimpl->anchorSize=val;
    reposition();
}

int IconBadge::anchorSize() const noexcept
{
    return pimpl->anchorSize;
}

void IconBadge::setOffsetX(int val)
{
    if (pimpl->offsetX==val)
    {
        return;
    }
    pimpl->offsetX=val;
    reposition();
}

int IconBadge::offsetX() const noexcept
{
    return pimpl->offsetX;
}

void IconBadge::setOffsetY(int val)
{
    if (pimpl->offsetY==val)
    {
        return;
    }
    pimpl->offsetY=val;
    reposition();
}

int IconBadge::offsetY() const noexcept
{
    return pimpl->offsetY;
}

//--------------------------------------------------------------------------

void IconBadge::rewatch()
{
    for (auto&& w : pimpl->watched)
    {
        if (!w.isNull())
        {
            w->removeEventFilter(this);
        }
    }
    pimpl->watched.clear();

    auto host=pimpl->host.data();
    for (QWidget* w=pimpl->anchor.data(); w!=nullptr && w!=host; w=w->parentWidget())
    {
        w->installEventFilter(this);
        pimpl->watched.emplace_back(w);
    }
    if (host!=nullptr)
    {
        host->installEventFilter(this);
        pimpl->watched.emplace_back(host);
    }
}

//--------------------------------------------------------------------------

bool IconBadge::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case (QEvent::ParentChange):
        {
            // the chain between anchor and host may have changed
            rewatch();
            reposition();
        }
        break;

        case (QEvent::Move):
        case (QEvent::Resize):
        case (QEvent::Show):
        case (QEvent::Hide):
        {
            reposition();
        }
        break;

        default:
        break;
    }

    return CountBadge::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void IconBadge::contentChanged()
{
    // A text/font/style change alters sizeHint() (a pill is wider than a circle), which moves
    // the left edge of a badge whose right edge is pinned.
    reposition();
}

//--------------------------------------------------------------------------

void IconBadge::reposition()
{
    if (pimpl->repositioning)
    {
        return;
    }
    pimpl->repositioning=true;

    auto anchor=pimpl->anchor.data();
    auto host=pimpl->host.data();

    if (anchor==nullptr || host==nullptr || text().isEmpty() || !anchor->isVisibleTo(host))
    {
        setVisible(false);
        pimpl->repositioning=false;
        return;
    }

    // sizeHint() reads font/padding off the hidden QSS carrier child, which is only polished along
    // with this widget -- by default at its first show, i.e. after the measurement below. Polishing
    // now makes the very first placement use the real size instead of correcting itself a moment
    // later. Any StyleChange this raises lands in contentChanged() -> reposition(), which the
    // re-entrancy guard turns into a no-op.
    ensurePolished();

    const QRect anchorRect(anchor->mapTo(host,QPoint(0,0)),anchor->size());

    QRect virtualIcon(QPoint(0,0),QSize(pimpl->anchorSize,pimpl->anchorSize));
    virtualIcon.moveCenter(anchorRect.center());

    const auto size=sizeHint();
    int x=virtualIcon.right()+1+pimpl->offsetX-size.width();
    int y=virtualIcon.top()-pimpl->offsetY;

    x=std::max(0,std::min(x,host->width()-size.width()));
    y=std::max(0,std::min(y,host->height()-size.height()));

    setGeometry(QRect(QPoint(x,y),size));
    setVisible(true);
    raise();

    pimpl->repositioning=false;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
