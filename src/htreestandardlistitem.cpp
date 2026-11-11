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

/** @file uise/desktop/htreestandardlistitem.cpp
*
*  Defines HTreeStandardListItem.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QApplication>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/countbadge.hpp>

// #include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/ipp/htreeflyweightlistitem.ipp>
#ifdef _MSC_VER
#include <uise/desktop/ipp/htreelistitemtemplate.ipp>
#endif

#include <uise/desktop/htreestandardlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeStandardListItem ******************************/

//--------------------------------------------------------------------------

HTreeStandardListItem::HTreeStandardListItem(HTreePathElement el, const QString& text, std::shared_ptr<SvgIcon> icon, QWidget* parent)
    : HTreeFlyweightListItem<>(std::move(el),parent)
{
    m_icon=new PushButton(this);
    m_icon->setObjectName("hTreeItemPixmap");
    m_icon->setSvgIcon(icon);
    m_icon->setCheckable(true);
    // By default the icon is part of the row: mouse-transparent, so presses go to the row
    // (immediate click, no ripple). See setIconClickable().
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents,!m_iconClickable);

    m_text=new ElidedLabel(this);
    m_text->setObjectName("hTreeItemText");

    setItemWidgets(m_icon,m_text,0,1);
    setTextElideMode(Qt::ElideMiddle);

    connect(
        m_icon,
        &PushButton::clicked,
        this,
        [this]()
        {
            if (m_propagateIconClick)
            {
                click();
            }
            else
            {
                emit iconClicked();
            }
        }
    );
    m_text->setText(text);
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setText(const QString& text)
{
    m_text->setText(text);
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setPixmap(const QPixmap& pixmap)
{
    m_icon->setIcon(pixmap);
}

//--------------------------------------------------------------------------

QString HTreeStandardListItem::text() const
{
    return m_text->text();
}

//--------------------------------------------------------------------------

QPixmap HTreeStandardListItem::pixmap() const
{
    return m_icon->icon().pixmap(m_icon->size());
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setTextElideMode(Qt::TextElideMode mode)
{
    m_text->setElideMode(mode);
}

//--------------------------------------------------------------------------

Qt::TextElideMode HTreeStandardListItem::textElideMode() const
{
    return m_text->elideMode();
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setIcon(std::shared_ptr<SvgIcon> icon)
{
    m_icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> HTreeStandardListItem::icon() const
{
    return m_icon->svgIcon();
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setIconClickable(bool enable)
{
    m_iconClickable=enable;
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents,!enable);
}

//--------------------------------------------------------------------------

bool HTreeStandardListItem::isIconClickable() const
{
    return m_iconClickable;
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setPropagateIconClick(bool enable)
{
    m_propagateIconClick=enable;
}

//--------------------------------------------------------------------------

bool HTreeStandardListItem::isPropagateIconClick() const
{
    return m_propagateIconClick;
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doSetHovered(bool enable)
{
    m_icon->setParentHovered(enable);
    if (m_expand!=nullptr)
    {
        m_expand->setParentHovered(enable);
    }
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doSetSelected(bool enable)
{
    m_icon->setChecked(enable);
    if (m_expand!=nullptr)
    {
        m_expand->setChecked(enable);
    }
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doClick()
{
    emit clicked();
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setExpandVisible(bool enable)
{
    destroyWidget(m_expand);

    if (!enable)
    {
        return;
    }

    m_expand=new PushButton(this);
    m_expand->setObjectName("hTreeItemExpand");
    m_expand->setSvgIcon(Style::instance().svgIconLocator().icon(iconName("expand"),this));
    m_expand->setCheckable(true);
    itemLayout()->addWidget(m_expand);
    connect(
        m_expand,
        &PushButton::clicked,
        this,
        [this]()
        {
            if (m_propagateIconClick)
            {
                click();
            }
            else
            {
                emit iconClicked();
            }
        }
    );
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setBadge(const QString& text, bool muted, bool mentioned)
{
    if (m_badge==nullptr)
    {
        if (text.isEmpty())
        {
            return;
        }

        m_badge=new CountBadge(this);
        m_badge->setObjectName("hTreeItemBadge");

        // Right after the text, ahead of the stretch added by setItemWidgets() (and so ahead of the
        // expand button, if any)
        itemLayout()->insertWidget(itemLayout()->indexOf(m_text)+1,m_badge);
    }

    m_badge->setText(text);
    m_badge->setMuted(muted);
    m_badge->setMentioned(mentioned);
}

//--------------------------------------------------------------------------

QString HTreeStandardListItem::badgeText() const
{
    return m_badge!=nullptr ? m_badge->text() : QString{};
}

//--------------------------------------------------------------------------

template struct UISE_DESKTOP_EXPORT FlyweightListItemTraits<HTreeStandardListItem*,HTreeListItem,std::string,std::string>;
template class UISE_DESKTOP_EXPORT FlyweightListItem<HTreeStandardListItemTraits>;
template class UISE_DESKTOP_EXPORT FlyweightListView<HTreeStansardListIemWrapper>;

template class UISE_DESKTOP_EXPORT HTreeFlyweightListItemView<HTreeStandardListItem>;
template class UISE_DESKTOP_EXPORT HTreeFlyweightListView<HTreeStandardListItem>;

UISE_DESKTOP_NAMESPACE_END
