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
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/htreestandardlistitem.hpp>

#include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/ipp/htreeflyweightlistitem.ipp>

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
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doSetSelected(bool enable)
{
    m_icon->setChecked(enable);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT FlyweightListItemTraits<HTreeStandardListItem*,HTreeListItem,std::string,std::string>;
template class UISE_DESKTOP_EXPORT FlyweightListItem<HTreeStandardListItemTraits>;
template class UISE_DESKTOP_EXPORT FlyweightListView<HTreeStansardListIemWrapper>;

template class UISE_DESKTOP_EXPORT HTreeFlyweightListItemView<HTreeStandardListItem>;
template class UISE_DESKTOP_EXPORT HTreeFlyweightListView<HTreeStandardListItem>;

UISE_DESKTOP_NAMESPACE_END
