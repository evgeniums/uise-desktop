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

#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/htreestandardlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeStandardListItem ******************************/

class HTreeStandardListItem_p
{
    public:

        HTreeStandardListItem* self=nullptr;
        QHBoxLayout* layout=nullptr;
        PushButton* icon=nullptr;
        ElidedLabel* text=nullptr;
        bool propagateIconClick=true;
};

//--------------------------------------------------------------------------

HTreeStandardListItem::HTreeStandardListItem(HTreePathElement el, const QString& text, std::shared_ptr<SvgIcon> icon, QWidget* parent)
    : HTreeListItem(std::move(el),parent),
      pimpl(std::make_unique<HTreeStandardListItem_p>())
{
    pimpl->self=this;
    pimpl->layout=Layout::horizontal(this);

    pimpl->icon=new PushButton(this);
    pimpl->icon->setObjectName("hTreeItemPixmap");
    pimpl->icon->setSvgIcon(icon);
    pimpl->icon->setCheckable(true);
    pimpl->layout->addWidget(pimpl->icon);

    pimpl->text=new ElidedLabel(this);
    pimpl->text->setObjectName("hTreeItemText");
    pimpl->layout->addWidget(pimpl->text);
    pimpl->layout->addStretch(1);
    setTextElideMode(Qt::ElideMiddle);

    setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);

    connect(
        pimpl->icon,
        &PushButton::clicked,
        this,
        [this]()
        {
            if (pimpl->propagateIconClick)
            {
                click();
            }
            else
            {
                emit iconClicked();
            }
        }
    );
    pimpl->text->setText(text);
}

//--------------------------------------------------------------------------

HTreeStandardListItem::~HTreeStandardListItem()
{}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setText(const QString& text)
{
    pimpl->text->setText(text);
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setPixmap(const QPixmap& pixmap)
{
    pimpl->icon->setIcon(pixmap);
}

//--------------------------------------------------------------------------

QString HTreeStandardListItem::text() const
{
    return pimpl->text->text();
}

//--------------------------------------------------------------------------

QPixmap HTreeStandardListItem::pixmap() const
{
    return pimpl->icon->icon().pixmap(pimpl->icon->size());
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setTextElideMode(Qt::TextElideMode mode)
{
    pimpl->text->setElideMode(mode);
}

//--------------------------------------------------------------------------

Qt::TextElideMode HTreeStandardListItem::textElideMode() const
{
    return pimpl->text->elideMode();
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> HTreeStandardListItem::icon() const
{
    return pimpl->icon->svgIcon();
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::setPropagateIconClick(bool enable)
{
    pimpl->propagateIconClick=enable;
}

//--------------------------------------------------------------------------

bool HTreeStandardListItem::isPropagateIconClick() const
{
    return pimpl->propagateIconClick;
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doSetHovered(bool enable)
{
    pimpl->icon->setParentHovered(enable);
}

//--------------------------------------------------------------------------

void HTreeStandardListItem::doSetSelected(bool enable)
{
    pimpl->icon->setChecked(enable);
}

/************************* HTreeStandardListItemView ***********************/


HTreeStandardListItemView::HTreeStandardListItemView(QWidget* parent) : FlyweightListView<HTreeStansardListIemWrapper>(parent)
{
    setSingleScrollStep(10);
}

/************************* HTreeStandardListView ***********************/

HTreeStandardListView::HTreeStandardListView(QWidget* parent, int minimumWidth) : HTreeListView<HTreeStansardListIemWrapper>(parent)
{
    auto l=Layout::vertical(this);
    auto listView=new HTreeStandardListItemView(this);
    l->addWidget(listView);

    listView->setFlyweightEnabled(false);
    listView->setStickMode(Direction::HOME);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setWheelHorizontalScrollEnabled(false);
    setListView(listView);

    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    setMinimumWidth(minimumWidth);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT FlyweightListItemTraits<HTreeStandardListItem*,HTreeListItem,std::string,std::string>;
template class UISE_DESKTOP_EXPORT FlyweightListItem<HTreeStandardListItemTraits>;
template class UISE_DESKTOP_EXPORT FlyweightListView<HTreeStansardListIemWrapper>;

UISE_DESKTOP_NAMESPACE_END
